#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <future>
#include <shared_mutex>
#include <queue>

namespace stp
{
	enum class task_priority : unsigned int
	{
		maximum = 6,
		very_high = 5,
		high = 4,
		normal = 3,
		low = 2,
		very_low = 1,
		minimum = 0
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
		ReturnType wait()
		{
			task_result_.wait();

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
			if (threadpool_state_ != threadpool_state_t::finalizing)
			{
				threadpool_lock_.lock();
				task_queue_.emplace(&task.task_function_, false, static_cast<unsigned int>(priority));
				++threadpool_task_emplaced_;
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
			if (threadpool_state_ != threadpool_state_t::finalizing)
			{
				threadpool_lock_.lock();
				task_queue_.emplace(&task.task_function_, true, static_cast<unsigned int>(priority));
				++threadpool_task_emplaced_;
				threadpool_lock_.unlock();

				if (threadpool_state_ == threadpool_state_t::running)
				{					
					threadpool_alert_.notify_one();
				}
			}			
		}
		void sync_run()
		{
			if (threadpool_sync_executed_)
			{
				throw std::runtime_error("Threadpool synchronizing");
			}

			if (threadpool_state_ == threadpool_state_t::running)
			{
				threadpool_sync_lock_.lock();
				threadpool_sync_executed_ = static_cast<size_t>(threadpool_sync_ready_);
				threadpool_sync_lock_.unlock();

				threadpool_sync_alert_.notify_all();
			}
		}
		void run()
		{
			if (threadpool_state_ == threadpool_state_t::waiting)
			{
				threadpool_lock_.lock();
				threadpool_state_ = threadpool_state_t::running;
				threadpool_state_changed_ = threadpool_ready_ + threadpool_sync_ready_;
				threadpool_lock_.unlock();

				threadpool_alert_.notify_all();
				threadpool_sync_alert_.notify_all();
			}
		}
		void stop()
		{
			if (threadpool_state_ == threadpool_state_t::running)
			{
				threadpool_lock_.lock();
				threadpool_state_ = threadpool_state_t::waiting;
				threadpool_state_changed_ = threadpool_ready_ + threadpool_sync_ready_;
				threadpool_lock_.unlock();				

				threadpool_alert_.notify_all();
				threadpool_sync_alert_.notify_all();
			}			
		}
		void finalize()
		{			
			if (threadpool_state_ != threadpool_state_t::finalizing)
			{
				threadpool_lock_.lock();
				threadpool_state_ = threadpool_state_t::finalizing;
				threadpool_state_changed_ = threadpool_number_;
				threadpool_lock_.unlock();

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
			return threadpool_number_;
		}

		threadpool() = delete;
		threadpool(size_t threadpool_number, threadpool_state threadpool_state = threadpool_state::waiting) :
			threadpool_number_(threadpool_number > 0 ? threadpool_number : static_cast<size_t>(std::thread::hardware_concurrency())),
			threadpool_state_(static_cast<threadpool_state_t>(threadpool_state)),
			threadpool_ready_(0),
			threadpool_running_(0),
			threadpool_sync_ready_(0),
			threadpool_sync_running_(0),
			threadpool_task_emplaced_(0),
			threadpool_sync_executed_(0),
			threadpool_state_changed_(0)
		{
			for (size_t n = 0; n < threadpool_number_; ++n)
			{
				thread_array_.emplace_back(&threadpool::threadpool__, this);
			}
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			threadpool_lock_.lock();
			threadpool_state_ = threadpool_state_t::terminating;
			threadpool_state_changed_ = threadpool_number_;
			threadpool_lock_.unlock();

			threadpool_alert_.notify_all();
			threadpool_sync_alert_.notify_all();

			for (size_t n = 0; n < threadpool_number_; ++n)
			{
				thread_array_[n].join();
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

		typedef std::deque<std::thread> thread_array_t;
		typedef std::priority_queue <task_t, std::deque<task_t>, task_comparator_t> task_queue_t;
#if defined(__clang__)
	// Not yet defined
#elif defined(__GNUG__)
	#if __cplusplus > 201402L
		typedef std::shared_mutex shared_mutex;
	#elif __cplusplus == 201402L
		typedef std::shared_timed_mutex shared_mutex;
	#endif
#elif defined(_MSC_VER)
	#if _MSC_VER >= 1900
		typedef std::shared_mutex shared_mutex_t;
	#endif
#elif defined (TODO__)
	// Not yet defined
#endif
		typedef std::condition_variable_any condition_variable_t;

		size_t const threadpool_number_;
		threadpool_state_t threadpool_state_;

		thread_array_t thread_array_;
		task_queue_t task_queue_;

		std::atomic_size_t threadpool_ready_;
		std::atomic_size_t threadpool_running_;
		std::atomic_size_t threadpool_sync_ready_;
		std::atomic_size_t threadpool_sync_running_;

		std::atomic_size_t threadpool_task_emplaced_;
		std::atomic_size_t threadpool_sync_executed_;
		std::atomic_size_t threadpool_state_changed_;

		shared_mutex_t threadpool_lock_;
		shared_mutex_t threadpool_sync_lock_;
		condition_variable_t threadpool_alert_;
		condition_variable_t threadpool_sync_alert_;

		void threadpool__()
		{
			task_t task;
			threadpool_state_t state = threadpool_state_;

			std::unique_lock<shared_mutex_t> unique_lock(threadpool_lock_, std::defer_lock);
			std::shared_lock<shared_mutex_t> shared_lock(threadpool_lock_);			

			++threadpool_ready_;

			while (threadpool_state_ != threadpool_state_t::terminating)
			{
				while (!threadpool_task_emplaced_ && !threadpool_state_changed_)
				{
					threadpool_alert_.wait(shared_lock);
				}

				if (threadpool_state_changed_)
				{
					if (state != threadpool_state_)
					{
						--threadpool_state_changed_;

						state = threadpool_state_;						
					}
				}

				while (true)
				{
					switch (threadpool_state_)
					{
						case threadpool_state_t::running:
							shared_lock.unlock();
							unique_lock.lock();

							if (threadpool_task_emplaced_)
							{
								if (!task.function_)
								{
									--threadpool_task_emplaced_;

									task = task_queue_.top();
									task_queue_.pop();
								}
							}
							else
							{
								unique_lock.unlock();
								shared_lock.lock();

								break;
							}

							unique_lock.unlock();

							if (task.function_ && !task.sync_)
							{
								threadpool_run__(task);
							}
							else if (task.function_ && task.sync_)
							{
								threadpool_sync__(task, state);
							}
							else
							{
								shared_lock.lock();

								break;
							}

							shared_lock.lock();
							continue;
						case threadpool_state_t::waiting:
							shared_lock.unlock();
							unique_lock.lock();

							if (threadpool_task_emplaced_)
							{
								if (!task.function_)
								{
									--threadpool_task_emplaced_;

									task = task_queue_.top();
									task_queue_.pop();
								}
							}
							else
							{
								unique_lock.unlock();
								shared_lock.lock();

								break;
							}

							unique_lock.unlock();

							if (task.function_ && task.sync_)
							{
								threadpool_sync__(task, state);
							}
							else
							{
								shared_lock.lock();

								break;
							}

							shared_lock.lock();
							continue;
						case threadpool_state_t::finalizing:
							shared_lock.unlock();
							unique_lock.lock();

							if (threadpool_task_emplaced_)
							{
								if (!task.function_)
								{
									--threadpool_task_emplaced_;

									task = task_queue_.top();
									task_queue_.pop();
								}
							}
							else
							{
								unique_lock.unlock();
								shared_lock.lock();

								break;
							}

							unique_lock.unlock();

							if (task.function_)
							{
								threadpool_run__(task);
							}
							else
							{
								shared_lock.lock();

								break;
							}

							shared_lock.lock();
							continue;
						case threadpool_state_t::terminating:
							break;
					}
					break;
				}
			}

			--threadpool_ready_;
		}
		void threadpool_run__(task_t & task, bool const sync = false)
		{
			--(sync ? threadpool_sync_ready_ : threadpool_ready_);
			++(sync ? threadpool_sync_running_ : threadpool_running_);

			task();

			--(sync ? threadpool_sync_running_ : threadpool_running_);
			++(sync ? threadpool_sync_ready_ : threadpool_ready_);
		}
		void threadpool_sync__(task_t & task, threadpool_state_t & state)
		{
			--threadpool_ready_;
			++threadpool_sync_ready_;

			std::shared_lock<shared_mutex_t> sync_lock(threadpool_sync_lock_);

			while (true)
			{
				while (!threadpool_sync_executed_ && !threadpool_state_changed_)
				{
					threadpool_sync_alert_.wait(sync_lock);
				}
				
				if (threadpool_state_changed_)
				{
					if (state != threadpool_state_)
					{
						--threadpool_state_changed_;

						state = threadpool_state_;
					}
				}

				switch (threadpool_state_)
				{
					case threadpool_state_t::running:
						if (threadpool_sync_executed_)
						{
							--threadpool_sync_executed_;

							threadpool_run__(task, true);

							break;
						}

					case threadpool_state_t::waiting:
						continue;
					case threadpool_state_t::finalizing:
						threadpool_run__(task, false);

					case threadpool_state_t::terminating:
						break;
				}
				break;
			}

			--threadpool_sync_ready_;
			++threadpool_ready_;
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
