#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <future>
#include <queue>

namespace stp
{
	enum class task_priority : unsigned int
	{
		maximum = std::numeric_limits<unsigned int>::max(),
		high = 2,
		normal = 1,
		low = 0
	};

	enum class threadpool_state : int
	{
		running = 0,
		waiting = 1
	};

	template <class ReturnType>
	class task
	{
	public:
		bool running() const
		{
			return task_running_;
		}
		bool ready() const
		{
			return task_result_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		}
		ReturnType result()
		{
			if (task_result_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			{
				throw std::runtime_error("Future not ready");
			}

			return task_result_.get();
		}

		task<ReturnType>() = delete;
		template <class FuncType, class ... ArgType>
		task<ReturnType>(FuncType && func, ArgType && ... args) :
			task_running_(false),
			task_package_(std::bind(std::forward<FuncType>(func), std::forward<ArgType>(args) ...)),
			task_function_([this] { task_running_ = true; task_package_(); task_running_ = false; }),
			task_result_(task_package_.get_future())
		{
		}
		task<ReturnType>(task<ReturnType> const &) = delete;
		task<ReturnType> & operator=(task<ReturnType> const &) = delete;
		task<ReturnType>(task<ReturnType> &&) = default;
		task<ReturnType> & operator=(task<ReturnType> &&) = default;
		~task<ReturnType>() = default;

		void operator()()
		{
			task_function_();
		}
	private:
		std::atomic<bool> task_running_;
		std::packaged_task<ReturnType()> task_package_;
		std::function<void()> task_function_;
		std::future<ReturnType> task_result_;

		friend class threadpool;
	};

	class threadpool
	{
	public:
		template <class ReturnType>
		void new_task(task<ReturnType> & task, task_priority const priority = task_priority::normal)
		{
			new_task(task, static_cast<unsigned int const>(priority));
		}
		template <class ReturnType>
		void new_task(task<ReturnType> & task, unsigned int const priority)
		{
			if (threadpool_state_ != threadpool_state_t::finalizing)
			{
				threadpool_lock_.lock();

				task_queue_.emplace(&task.task_function_, false, priority);

				threadpool_lock_.unlock();

				if (threadpool_state_ == threadpool_state_t::running)
				{
					threadpool_alert_.notify_one();
				}
			}
		}
		template <class ReturnType>
		void new_sync_task(task<ReturnType> & task, task_priority const priority = task_priority::normal)
		{
			new_sync_task(task, static_cast<unsigned int const>(priority));
		}
		template <class ReturnType>
		void new_sync_task(task<ReturnType> & task, unsigned int const priority)
		{
			if (threadpool_state_ != threadpool_state_t::finalizing)
			{
				threadpool_lock_.lock();

				task_queue_.emplace(&task.task_function_, true, priority);

				threadpool_lock_.unlock();

				if (threadpool_state_ == threadpool_state_t::running)
				{
					threadpool_alert_.notify_one();
				}
			}
		}
		void sync_run()
		{
			if (threadpool_state_ == threadpool_state_t::running)
			{
				if (threadpool_sync_executed_)
				{
					throw std::runtime_error("Threadpool synchronizing");
				}

				if (threadpool_sync_count_ = static_cast<size_t>(threadpool_sync_ready_))
				{
					threadpool_sync_executed_ = true;

					threadpool_sync_alert_.notify_all();
				}
			}
		}
		void run()
		{
			if (threadpool_state_ == threadpool_state_t::waiting)
			{
				threadpool_state_ = threadpool_state_t::running;
				threadpool_state_changed_ = threadpool_number_;

				threadpool_alert_.notify_all();
				threadpool_sync_alert_.notify_all();
			}
		}
		void stop()
		{
			if (threadpool_state_ == threadpool_state_t::running)
			{
				threadpool_state_ = threadpool_state_t::waiting;
				threadpool_state_changed_ = threadpool_number_;

				threadpool_alert_.notify_all();
				threadpool_sync_alert_.notify_all();
			}
		}
		void finalize()
		{
			if (threadpool_state_ != threadpool_state_t::finalizing)
			{
				threadpool_state_ = threadpool_state_t::finalizing;
				threadpool_state_changed_ = threadpool_number_;

				threadpool_alert_.notify_all();
				threadpool_sync_alert_.notify_all();
			}
		}
		size_t ready() const
		{
			return threadpool_ready_;
		}
		size_t running() const
		{
			return threadpool_running_;
		}
		size_t sync_ready() const
		{
			return threadpool_sync_ready_;
		}
		size_t sync_running() const
		{
			return threadpool_sync_running_;
		}
		size_t size() const
		{
			return (threadpool_state_ != threadpool_state_t::finalizing ? threadpool_number_ : size_t());
		}
		bool available() const
		{
			return threadpool_ready_ != 0;
		}

		threadpool() = delete;
		threadpool(size_t threadpool_number, threadpool_state threadpool_state = threadpool_state::waiting) :
			threadpool_number_(threadpool_number > 0 ? threadpool_number : static_cast<size_t>(std::thread::hardware_concurrency())),
			threadpool_ready_(0),
			threadpool_running_(0),
			threadpool_sync_ready_(0),
			threadpool_sync_running_(0),
			threadpool_state_(static_cast<threadpool_state_t>((static_cast<int>(threadpool_state) & 0xFFFFFFFE) == 0 ?
				threadpool_state :
				threadpool_state::waiting)),
			threadpool_state_changed_(0),
			threadpool_sync_executed_(false),
			threadpool_sync_count_(0)
		{
			for (size_t n = 0; n < threadpool_number_; ++n)
			{
				threadpool_array_.emplace_back(&threadpool::threadpool__, this);
			}
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			threadpool_state_ = threadpool_state_t::terminating;
			threadpool_state_changed_ = threadpool_number_;

			threadpool_alert_.notify_all();
			threadpool_sync_alert_.notify_all();

			for (size_t n = 0; n < threadpool_number_; ++n)
			{
				threadpool_array_[n].join();
			}
		}
	private:
		enum class threadpool_state_t : int
		{
			running = 0,
			waiting = 1,
			finalizing = 2,
			terminating = 3
		};

		class task_t
		{
		public:
			std::function<void()> * function_;
			bool sync_;
			unsigned int priority_;
			std::chrono::high_resolution_clock::time_point age_;

			task_t(std::function<void()> * function = nullptr, bool const sync = false, unsigned int const priority = 1) :
				function_(function),
				sync_(sync),
				priority_(priority),
				age_(std::chrono::high_resolution_clock::now())
			{
			}
			task_t & operator=(task_t const & task)
			{
				function_ = task.function_;
				sync_ = task.sync_;
				priority_ = task.priority_;
				age_ = task.age_;
				return *this;
			}

			void operator()()
			{
				(*function_)();
				function_ = nullptr;
			}
		};

		class task_comparator_t
		{
		public:
			task_comparator_t() = default;

			bool operator()(task_t const & task_1, task_t const & task_2)
			{
				return (task_1.priority_ != task_2.priority_ ? task_1.priority_ < task_2.priority_ : task_1.age_ > task_2.age_);
			}
		};			

		size_t const threadpool_number_;
		std::atomic<size_t> threadpool_ready_;
		std::atomic<size_t> threadpool_running_;
		std::atomic<size_t> threadpool_sync_ready_;
		std::atomic<size_t> threadpool_sync_running_;
		std::deque<std::thread> threadpool_array_;
		std::priority_queue <task_t, std::deque<task_t>, task_comparator_t> task_queue_;
		std::mutex threadpool_lock_;
		std::condition_variable threadpool_alert_;
		std::condition_variable threadpool_sync_alert_;
		std::atomic<threadpool_state_t> threadpool_state_;
		std::atomic<size_t> threadpool_state_changed_;
		std::atomic<bool> threadpool_sync_executed_;
		std::atomic<size_t> threadpool_sync_count_;

		void threadpool__()
		{
			task_t task;
			std::mutex sync_mutex; // Redundant mutex
			std::unique_lock<std::mutex> lock(threadpool_lock_);
			std::unique_lock<std::mutex> sync_lock(sync_mutex);

			++threadpool_ready_;

			while (threadpool_state_ != threadpool_state_t::terminating)
			{
				do
				{
					threadpool_alert_.wait_for(lock, std::chrono::milliseconds(1));
				}
				while (!threadpool_state_changed_ && task_queue_.empty());
				if (threadpool_state_changed_)
				{
					--threadpool_state_changed_;
				}

				while (threadpool_state_ != threadpool_state_t::terminating)
				{
					switch (threadpool_state_)
					{
						case threadpool_state_t::running:
							if (!task_queue_.empty())
							{
								if (!task.function_)
								{
									task = task_queue_.top();
									task_queue_.pop();
								}
							}
							else
							{
								break;
							}

							if (task.function_ && !task.sync_)
							{
								threadrun__(task, lock);
							}
							else if (task.function_ && task.sync_)
							{
								threadsync__(task, lock, sync_lock);
							}
							else
							{
								break;
							}

							continue;
						case threadpool_state_t::waiting:
							if (!task_queue_.empty())
							{
								if (!task.function_)
								{
									task = task_queue_.top();
									task_queue_.pop();
								}
							}
							else
							{
								break;
							}

							if (task.function_ && task.sync_)
							{
								threadsync__(task, lock, sync_lock);
							}
							else
							{
								break;
							}

							continue;
						case threadpool_state_t::finalizing:
							if (!task_queue_.empty())
							{
								if (!task.function_)
								{
									task = task_queue_.top();
									task_queue_.pop();
								}
							}
							else
							{
								break;
							}

							if (task.function_)
							{
								threadrun__(task, lock);
							}
							else
							{
								break;
							}

							continue;
						case threadpool_state_t::terminating:
							break;
					}
					break;
				}
			}

			--threadpool_ready_;
		}
		void threadrun__(task_t & task, std::unique_lock<std::mutex> & lock)
		{
			lock.unlock();
			--threadpool_ready_;
			++threadpool_running_;

			task();

			--threadpool_running_;
			++threadpool_ready_;
			lock.lock();
		}
		void threadsync__(task_t & task, std::unique_lock<std::mutex> & lock,
			std::unique_lock<std::mutex> & sync_lock)
		{
			lock.unlock();
			--threadpool_ready_;
			++threadpool_sync_ready_;

			while (threadpool_state_ != threadpool_state_t::terminating)
			{
				do
				{
					threadpool_sync_alert_.wait_for(sync_lock, std::chrono::milliseconds(1));
				}
				while (!threadpool_state_changed_ && !threadpool_sync_executed_);
				if (threadpool_state_changed_)
				{
					--threadpool_state_changed_;
				}

				switch (threadpool_state_)
				{
					case threadpool_state_t::running:
						if (threadpool_sync_executed_)
						{
							if (threadpool_sync_count_-- == 1) // Some synchronization trickery
							{
								threadpool_sync_executed_ = false;
							}
							else
							{
								while (threadpool_sync_executed_);
							}

							--threadpool_sync_ready_;
							++threadpool_sync_running_;

							task();

							--threadpool_sync_running_;

							break;
						}

					case threadpool_state_t::waiting:
						continue;
					case threadpool_state_t::finalizing:
						task();

					case threadpool_state_t::terminating:
						--threadpool_sync_ready_;

						break;
				}
				break;
			}

			++threadpool_ready_;
			lock.lock();
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
