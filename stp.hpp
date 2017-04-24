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
		bool result_ready() const
		{
			return task_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		}
		ReturnType result()
		{
			if (!task_result_)
			{
				if (task_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				{
					throw std::runtime_error("Future not ready");
				}

				task_result_ = new ReturnType(task_future_.get());
			}

			return *task_result_;
		}
		ReturnType result_wait()
		{
			if (!task_result_)
			{
				task_future_.wait();

				task_result_ = new ReturnType(task_future_.get());
			}

			return *task_result_;
		}

		task<ReturnType>() = delete;
		template <class FuncType, class ... ArgType>
		task<ReturnType>(FuncType && func, ArgType && ... arg) :
			task_package_(std::bind(std::forward<FuncType>(func), std::forward<ArgType>(arg) ...)),
			task_function_([this] { task_package_(); }),
			task_future_(task_package_.get_future()),
			task_result_(nullptr)
		{
		}
		task<ReturnType>(task<ReturnType> const &) = delete;
		task<ReturnType> & operator=(task<ReturnType> const &) = delete;
		task<ReturnType>(task<ReturnType> &&) = default;
		task<ReturnType> & operator=(task<ReturnType> &&) = default;
		~task<ReturnType>()
		{
			if (task_result_)
			{
				delete task_result_;
			}
		}

		void operator()()
		{
			task_function_();
		}
	private:
		std::packaged_task<ReturnType()> task_package_;
		std::function<void()> task_function_;
		std::future<ReturnType> task_future_;
		ReturnType * task_result_;

		friend class threadpool;
	};

	class threadpool
	{
	public:
		template <class ReturnType>
		void new_task(task<ReturnType> & task, task_priority const priority = task_priority::normal)
		{
			if (threadpool_state_ != state_t::finalizing)
			{
				threadpool_lock_.lock();

				task_queue_.emplace(&task.task_function_, false, false, static_cast<unsigned int>(priority));
				if (task_queue_notify_)
				{
					++threadpool_task_inserted_;
				}

				threadpool_lock_.unlock();

				if (task_queue_notify_)
				{
					threadpool_alert_.notify_one();
				}
			}
		}
		template <class FuncType, class ... ArgType>
		void new_task(FuncType && func, task_priority const priority, ArgType && ... arg)
		{
			if (threadpool_state_ != state_t::finalizing)
			{
				auto task = std::bind(std::forward<FuncType>(func), std::forward<ArgType>(arg) ...);
				auto task_wrapper = new std::function<void()>([=] { task(); });

				threadpool_lock_.lock();

				task_queue_.emplace(task_wrapper, false, true, static_cast<unsigned int>(priority));
				if (task_queue_notify_)
				{
					++threadpool_task_inserted_;
				}

				threadpool_lock_.unlock();

				if (task_queue_notify_)
				{
					threadpool_alert_.notify_one();
				}
			}
		}
		template <class ReturnType>
		void new_sync_task(task<ReturnType> & task, task_priority const priority = task_priority::normal)
		{
			if (threadpool_state_ != state_t::finalizing)
			{
				threadpool_lock_.lock();

				task_queue_.emplace(&task.task_function_, true, false, static_cast<unsigned int>(priority));
				if (task_queue_notify_)
				{
					++threadpool_task_inserted_;
				}

				threadpool_lock_.unlock();

				if (task_queue_notify_)
				{
					threadpool_alert_.notify_one();
				}
			}
		}
		template <class FuncType, class ... ArgType>
		void new_sync_task(FuncType && func, task_priority const priority, ArgType && ... arg)
		{
			if (threadpool_state_ != state_t::finalizing)
			{
				auto task = std::bind(std::forward<FuncType>(func), std::forward<ArgType>(arg) ...);
				auto task_wrapper = new std::function<void()>([=] { task(); });

				threadpool_lock_.lock();

				task_queue_.emplace(task_wrapper, true, true, static_cast<unsigned int>(priority));
				if (task_queue_notify_)
				{
					++threadpool_task_inserted_;
				}

				threadpool_lock_.unlock();

				if (task_queue_notify_)
				{
					threadpool_alert_.notify_one();
				}
			}
		}
		void delete_tasks()
		{
			threadpool_lock_.lock();
			threadpool_sync_lock_.lock();

			threadpool_task_inserted_ = 0U;
			while (!task_queue_.empty())
			{
				task_queue_.pop();
			}
			threadpool_task_disposed_ = threadpool_waiting_ + threadpool_sync_waiting_;

			threadpool_lock_.unlock();
			threadpool_sync_lock_.unlock();

			threadpool_alert_.notify_all();
			threadpool_sync_alert_.notify_all();
		}
		void set_notify_tasks(bool const notify)
		{
			if (task_queue_notify_ = notify)
			{
				notify_tasks();
			}
		}
		void notify_tasks()
		{
			threadpool_lock_.lock();

			threadpool_task_inserted_ = task_queue_.size();

			threadpool_lock_.unlock();

			threadpool_alert_.notify_all();
		}
		void run_sync_tasks()
		{
			if (threadpool_sync_executed_)
			{
				throw std::runtime_error("Threadpool synchronizing");
			}

			if (threadpool_state_ == state_t::running)
			{
				threadpool_sync_lock_.lock();

				threadpool_sync_executed_ = static_cast<size_t>(threadpool_sync_waiting_);

				threadpool_sync_lock_.unlock();

				threadpool_sync_alert_.notify_all();
			}
		}
		void run()
		{
			if (threadpool_state_ == state_t::waiting)
			{
				threadpool_lock_.lock();
				threadpool_sync_lock_.lock();

				threadpool_state_ = state_t::running;
				threadpool_state_age_ = std::chrono::high_resolution_clock::now();
				threadpool_state_changed_ = threadpool_waiting_ + threadpool_sync_waiting_;

				threadpool_lock_.unlock();
				threadpool_sync_lock_.unlock();

				threadpool_alert_.notify_all();
				threadpool_sync_alert_.notify_all();
			}
		}
		void stop()
		{
			if (threadpool_state_ == state_t::running)
			{
				threadpool_lock_.lock();
				threadpool_sync_lock_.lock();

				threadpool_state_ = state_t::waiting;
				threadpool_state_age_ = std::chrono::high_resolution_clock::now();
				threadpool_state_changed_ = threadpool_waiting_ + threadpool_sync_waiting_;

				threadpool_lock_.unlock();
				threadpool_sync_lock_.unlock();

				threadpool_alert_.notify_all();
				threadpool_sync_alert_.notify_all();
			}
		}
		void finalize()
		{
			if (threadpool_state_ != state_t::finalizing)
			{
				threadpool_lock_.lock();
				threadpool_sync_lock_.lock();
				threadpool_sleep_lock_.lock();

				threadpool_state_ = state_t::finalizing;
				threadpool_state_age_ = std::chrono::high_resolution_clock::now();
				threadpool_state_changed_ = threadpool_sleeping_ + threadpool_waiting_ + threadpool_sync_waiting_;

				threadpool_thread_signal_awake_ = thread_array_asleep_;

				threadpool_lock_.unlock();
				threadpool_sync_lock_.unlock();
				threadpool_sleep_lock_.unlock();

				threadpool_alert_.notify_all();
				threadpool_sync_alert_.notify_all();
				threadpool_sleep_alert_.notify_all();
			}
		}
		size_t sleeping() const
		{
			return threadpool_sleeping_;
		}
		size_t waiting() const
		{
			return threadpool_waiting_;
		}
		size_t running() const
		{
			return threadpool_running_;
		}
		size_t sync_waiting() const
		{
			return threadpool_sync_waiting_;
		}
		size_t sync_running() const
		{
			return threadpool_sync_running_;
		}
		size_t size() const
		{
			return thread_array_awake_ + thread_array_asleep_;
		}
		void resize(size_t const threadpool_size)
		{
			if (thread_array_awake_ < threadpool_size)
			{
				threadpool_sleep_lock_.lock();

				size_t threadpool_size_diff = threadpool_size - thread_array_awake_;
				if (threadpool_size_diff > thread_array_asleep_)
				{
					threadpool_size_diff = thread_array_asleep_;
				}
				threadpool_thread_signal_awake_ = threadpool_size_diff;

				thread_array_awake_ += threadpool_size_diff;
				thread_array_asleep_ -= threadpool_size_diff;

				threadpool_sleep_lock_.unlock();

				threadpool_sleep_alert_.notify_all();
			}
			else if (thread_array_awake_ > threadpool_size)
			{
				threadpool_lock_.lock();

				size_t threadpool_size_diff = thread_array_awake_ - threadpool_size;
				threadpool_thread_signal_sleep_ = threadpool_size_diff;

				thread_array_awake_ -= threadpool_size_diff;
				thread_array_asleep_ += threadpool_size_diff;

				threadpool_lock_.unlock();

				threadpool_alert_.notify_all();
			}
		}

		threadpool() = delete;
		threadpool(size_t min_threadpool_number = std::thread::hardware_concurrency(), size_t max_threadpool_number = 0U, threadpool_state threadpool_state = threadpool_state::waiting) :
			thread_array_awake_(min_threadpool_number),
			thread_array_asleep_(max_threadpool_number != 0U ? max_threadpool_number - min_threadpool_number : (max_threadpool_number = min_threadpool_number, 0U)),
			task_queue_notify_(true),
			threadpool_state_(static_cast<state_t>(threadpool_state)),
			threadpool_state_age_(std::chrono::high_resolution_clock::now()),
			threadpool_sleeping_(0U),
			threadpool_waiting_(0U),
			threadpool_running_(0U),
			threadpool_sync_waiting_(0U),
			threadpool_sync_running_(0U),
			threadpool_thread_signal_awake_(0U),
			threadpool_thread_signal_sleep_(max_threadpool_number - min_threadpool_number),			
			threadpool_task_inserted_(0U),
			threadpool_task_disposed_(0U),
			threadpool_sync_executed_(0U),
			threadpool_state_changed_(0U)
		{
			if (max_threadpool_number < min_threadpool_number)
			{
				throw std::runtime_error("Maximum size shouldn't be smaller than minimum size");
			}

			threadpool_lock_.lock();

			for (size_t n = 0U; n < max_threadpool_number; ++n)
			{
				thread_array_.emplace_back(&threadpool::threadpool__, this);
			}

			threadpool_lock_.unlock();
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			threadpool_lock_.lock();
			threadpool_sync_lock_.lock();
			threadpool_sleep_lock_.lock();

			threadpool_state_ = state_t::terminating;
			threadpool_state_age_ = std::chrono::high_resolution_clock::now();
			threadpool_state_changed_ = thread_array_awake_ + thread_array_asleep_;

			threadpool_thread_signal_awake_ = thread_array_asleep_;

			threadpool_lock_.unlock();
			threadpool_sync_lock_.unlock();
			threadpool_sleep_lock_.unlock();

			threadpool_alert_.notify_all();
			threadpool_sync_alert_.notify_all();
			threadpool_sleep_alert_.notify_all();

			for (auto & thread : thread_array_)
			{
				thread.join();
			}
		}
	private:
		enum class state_t : int
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
			bool cleanup_;
			unsigned int priority_;
			std::chrono::high_resolution_clock::time_point age_;

			task_t(std::function<void()> * function = nullptr, bool const sync = false, bool const cleanup = false, unsigned int const priority = 3U) :
				function_(function),
				sync_(sync),
				cleanup_(cleanup),
				priority_(priority),
				age_(std::chrono::high_resolution_clock::now())
			{
			}
			task_t & operator=(task_t const & task)
			{
				function_ = task.function_;
				sync_ = task.sync_;
				cleanup_ = task.cleanup_;
				priority_ = task.priority_;
				age_ = task.age_;
				return *this;
			}

			void operator()()
			{
				(*function_)();
				if (cleanup_)
				{
					delete function_;
				}
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

#if defined(__clang__)
		// Not yet defined
#elif defined(__GNUG__)
#if __cplusplus >= 201402L && _HAS_SHARED_MUTEX == 1
		typedef std::shared_mutex rw_mutex_t;
#elif __cplusplus == 201402L
		typedef std::shared_timed_mutex rw_mutex_t;
#endif
#elif defined(_MSC_VER)
#if _MSVC_LANG >= 201402L && _HAS_SHARED_MUTEX == 1
		typedef std::shared_mutex rw_mutex_t;
#elif _MSVC_LANG == 201402L
		typedef std::shared_timed_mutex rw_mutex_t;
#endif
#endif
		typedef std::condition_variable_any condition_variable_t;
		
		std::vector<std::thread> thread_array_;
		size_t thread_array_awake_;
		size_t thread_array_asleep_;
		std::priority_queue <task_t, std::deque<task_t>, task_comparator_t> task_queue_;
		bool task_queue_notify_;
		state_t threadpool_state_;
		std::chrono::high_resolution_clock::time_point threadpool_state_age_;

		std::atomic_size_t threadpool_sleeping_;
		std::atomic_size_t threadpool_waiting_;
		std::atomic_size_t threadpool_running_;
		std::atomic_size_t threadpool_sync_waiting_;
		std::atomic_size_t threadpool_sync_running_;

		std::atomic_size_t threadpool_thread_signal_awake_;
		std::atomic_size_t threadpool_thread_signal_sleep_;
		std::atomic_size_t threadpool_task_inserted_;
		std::atomic_size_t threadpool_task_disposed_;
		std::atomic_size_t threadpool_sync_executed_;
		std::atomic_size_t threadpool_state_changed_;

		rw_mutex_t threadpool_lock_;
		rw_mutex_t threadpool_sync_lock_;
		rw_mutex_t threadpool_sleep_lock_;
		condition_variable_t threadpool_alert_;
		condition_variable_t threadpool_sync_alert_;
		condition_variable_t threadpool_sleep_alert_;

		void threadpool__()
		{
			task_t task;
			std::chrono::high_resolution_clock::time_point state_age;

			std::unique_lock<rw_mutex_t> unique_lock(threadpool_lock_, std::defer_lock);
			std::shared_lock<rw_mutex_t> shared_lock(threadpool_lock_, std::defer_lock);
			std::shared_lock<rw_mutex_t> sync_lock(threadpool_sync_lock_, std::defer_lock);
			std::unique_lock<rw_mutex_t> sleep_lock(threadpool_sleep_lock_, std::defer_lock);

			shared_lock.lock();

			while (threadpool_state_ != state_t::terminating)
			{
				++threadpool_waiting_;

				while (!threadpool_thread_signal_sleep_ && !threadpool_task_inserted_ && !threadpool_task_disposed_ && !threadpool_state_changed_)
				{
					threadpool_alert_.wait(shared_lock);
				}

				--threadpool_waiting_;

				if (threadpool_state_changed_)
				{
					if (state_age != threadpool_state_age_)
					{
						--threadpool_state_changed_;

						state_age = threadpool_state_age_;
					}
				}

				if (threadpool_thread_signal_sleep_)
				{
					shared_lock.unlock();

					if (task.function_)
					{
						unique_lock.lock();

						++threadpool_task_inserted_;

						task_queue_.push(task);
						task.function_ = nullptr;

						threadpool_alert_.notify_one();

						unique_lock.unlock();
					}

					threadpool_sleep__(task, sleep_lock);

					shared_lock.lock();
				}

				while (true)
				{
					switch (threadpool_state_)
					{
						case state_t::running:
							shared_lock.unlock();
							unique_lock.lock();

							if (!task.function_)
							{
								if (threadpool_task_inserted_)
								{
									--threadpool_task_inserted_;

									task = task_queue_.top();
									task_queue_.pop();
								}
							}
							else
							{
								if (threadpool_task_disposed_)
								{
									--threadpool_task_disposed_;

									task.function_ = nullptr;
								}
							}

							unique_lock.unlock();

							if (task.function_ && !task.sync_)
							{
								threadpool_run__(task);
							}
							else if (task.function_ && task.sync_)
							{
								threadpool_sync__(task, state_age, sync_lock);
							}
							else
							{
								shared_lock.lock();

								break;
							}

							shared_lock.lock();

							continue;
						case state_t::waiting:
							shared_lock.unlock();
							unique_lock.lock();

							if (!task.function_)
							{
								if (threadpool_task_inserted_)
								{
									--threadpool_task_inserted_;

									task = task_queue_.top();
									task_queue_.pop();
								}
							}
							else
							{
								if (threadpool_task_disposed_)
								{
									--threadpool_task_disposed_;

									task.function_ = nullptr;
								}
							}

							unique_lock.unlock();

							if (task.function_ && task.sync_)
							{
								threadpool_sync__(task, state_age, sync_lock);
							}
							else
							{
								shared_lock.lock();

								break;
							}

							shared_lock.lock();

							continue;
						case state_t::finalizing:
							shared_lock.unlock();
							unique_lock.lock();

							if (!task.function_)
							{
								if (threadpool_task_inserted_)
								{
									--threadpool_task_inserted_;

									task = task_queue_.top();
									task_queue_.pop();
								}
							}
							else
							{
								if (threadpool_task_disposed_)
								{
									--threadpool_task_disposed_;

									task.function_ = nullptr;
								}
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
						case state_t::terminating:
							break;
					}
					break;
				}
			}

			shared_lock.unlock();
		}
		void threadpool_sleep__(task_t & task, std::unique_lock<rw_mutex_t> & sleep_lock)
		{
			++threadpool_sleeping_;

			sleep_lock.lock();

			if (threadpool_thread_signal_sleep_)
			{
				--threadpool_thread_signal_sleep_;

				while (!threadpool_thread_signal_awake_)
				{
					threadpool_sleep_alert_.wait(sleep_lock);
				}

				--threadpool_thread_signal_awake_;
			}

			sleep_lock.unlock();

			--threadpool_sleeping_;
		}
		void threadpool_run__(task_t & task, bool const sync = false)
		{
			++(sync ? threadpool_sync_running_ : threadpool_running_);

			task();

			--(sync ? threadpool_sync_running_ : threadpool_running_);
		}
		void threadpool_sync__(task_t & task, std::chrono::high_resolution_clock::time_point & state_age, std::shared_lock<rw_mutex_t> & sync_lock)
		{
			sync_lock.lock();

			while (true)
			{
				++threadpool_sync_waiting_;

				while (!threadpool_task_disposed_ && !threadpool_sync_executed_ && !threadpool_state_changed_)
				{
					threadpool_sync_alert_.wait(sync_lock);
				}

				--threadpool_sync_waiting_;

				if (threadpool_state_changed_)
				{
					if (state_age != threadpool_state_age_)
					{
						--threadpool_state_changed_;

						state_age = threadpool_state_age_;
					}
				}

				if (threadpool_task_disposed_)
				{
					break;
				}

				switch (threadpool_state_)
				{
					case state_t::running:
						if (threadpool_sync_executed_)
						{
							--threadpool_sync_executed_;

							sync_lock.unlock();

							threadpool_run__(task, true);

							sync_lock.lock();

							break;
						}

					case state_t::waiting:
						continue;
					case state_t::finalizing:
						sync_lock.unlock();

						threadpool_run__(task, false);

						sync_lock.lock();

					case state_t::terminating:
						break;
				}
				break;
			}

			sync_lock.unlock();
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
