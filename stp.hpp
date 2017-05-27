#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <future>
#include <shared_mutex>
#include <list>
#include <queue>

namespace stp
{
	enum class task_priority : uint32_t // Maybe expand this in the future
	{
		maximum = 6,
		very_high = 5,
		high = 4,
		normal = 3,
		low = 2,
		very_low = 1,
		minimum = 0
	};

	enum class threadpool_state : uint32_t
	{
		running = 0,
		waiting = 1,
		terminating = 2
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

				task_result_ = std::make_unique<ReturnType>(task_future_.get());
			}

			return *task_result_;
		}
		ReturnType result_wait()
		{
			if (!task_result_)
			{
				task_future_.wait();

				task_result_ = std::make_unique<ReturnType>(task_future_.get());
			}

			return *task_result_;
		}

		task<ReturnType>() = delete;
		template <class FuncType, class ... ArgsType>
		task<ReturnType>(FuncType && func, ArgsType && ... args) :
			task_package_(std::bind(std::forward<FuncType>(func), std::forward<ArgsType>(args) ...)), // Eventually i'll have to do this properly
			task_function_([this] { task_package_(); }),
			task_future_(task_package_.get_future()),
			task_result_(nullptr)
		{
		}
		task<ReturnType>(task<ReturnType> const &) = delete;
		task<ReturnType> & operator=(task<ReturnType> const &) = delete;
		task<ReturnType>(task<ReturnType> &&) = default;
		task<ReturnType> & operator=(task<ReturnType> &&) = default;
		~task<ReturnType>() = default;

		ReturnType operator()()
		{
			if (!task_result_)
			{
				task_function_();

				task_result_ = std::make_unique<ReturnType>(task_future_.get());
			}

			return *task_result_;
		}
	private:
		std::packaged_task<ReturnType()> task_package_;
		std::function<void()> task_function_;
		std::future<ReturnType> task_future_;
		std::unique_ptr<ReturnType> task_result_;

		friend class threadpool;
	};

	class threadpool
	{
	public:
		void new_threads(size_t const amount)
		{
			if (threadpool_state_ != threadpool_state::terminating)
			{
				for (size_t n = 0U; n < amount; ++n)
				{
					thread_array_.emplace_back(&threadpool::threadpool__, this);
				}
			}
		}
		void delete_threads(size_t const amount)
		{
			if (threadpool_state_ != threadpool_state::terminating)
			{
				if (threadpool_size_ < amount)
				{
					std::invalid_argument("Threadpool bad argument");
				}

				auto it = thread_array_.begin(), it_b = thread_array_.begin(), it_e = thread_array_.end();
				for (size_t n = 0U; n < amount; ++it == it_e ? it = it_b : it)
				{
					if (it->thread_state_ != threadpool_state::terminating)
					{
						std::lock_guard<rw_mutex_t> lock(thread_mutex_);

						if (!it->task_.function_)
						{
							it->thread_state_ = threadpool_state::terminating;
							++n;
						}
					}
				}

				thread_alert_.notify_all();
			}
		}
		template <class ReturnType>
		void new_task(task<ReturnType> & task, task_priority const priority = task_priority::normal)
		{
			if (threadpool_state_ != threadpool_state::terminating)
			{
				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				task_queue_.emplace(&task.task_function_, false, false, static_cast<uint32_t>(priority));
				task_priority_ = static_cast<task_priority>(task_queue_.top().priority_);

				if (threadpool_notify_new_tasks_)
				{
					++threadpool_new_tasks_;
					thread_alert_.notify_one();
				}
			}
		}
		template <class FuncType, class ... ArgsType>
		void new_task(FuncType && func, ArgsType && ... args)
		{
			new_task(std::forward<FuncType>(func), task_priority::normal, std::forward<ArgsType>(args) ...);
		}
		template <class FuncType, class ... ArgsType>
		void new_task(FuncType && func, task_priority const priority, ArgsType && ... args)
		{
			if (threadpool_state_ != threadpool_state::terminating)
			{
				auto task = std::bind(std::forward<FuncType>(func), std::forward<ArgsType>(args) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				task_queue_.emplace(task_function, false, true, static_cast<uint32_t>(priority));
				task_priority_ = static_cast<task_priority>(task_queue_.top().priority_);

				if (threadpool_notify_new_tasks_)
				{
					++threadpool_new_tasks_;
					thread_alert_.notify_one();
				}
			}
		}
		template <class ReturnType>
		void new_sync_task(task<ReturnType> & task, task_priority const priority = task_priority::normal)
		{
			if (threadpool_state_ != threadpool_state::terminating)
			{
				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				task_queue_.emplace(&task.task_function_, true, false, static_cast<uint32_t>(priority));
				task_priority_ = static_cast<task_priority>(task_queue_.top().priority_);

				if (threadpool_notify_new_tasks_)
				{
					++threadpool_new_tasks_;
					thread_alert_.notify_one();
				}
			}
		}
		template <class FuncType, class ... ArgsType>
		void new_sync_task(FuncType && func, ArgsType && ... args)
		{
			new_sync_task(std::forward<FuncType>(func), task_priority::normal, std::forward<ArgsType>(args) ...);
		}
		template <class FuncType, class ... ArgsType>
		void new_sync_task(FuncType && func, task_priority const priority, ArgsType && ... args)
		{
			if (threadpool_state_ != threadpool_state::terminating)
			{
				auto task = std::bind(std::forward<FuncType>(func), std::forward<ArgsType>(args) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				task_queue_.emplace(task_function, true, true, static_cast<uint32_t>(priority));
				task_priority_ = static_cast<task_priority>(task_queue_.top().priority_);

				if (threadpool_notify_new_tasks_)
				{
					++threadpool_new_tasks_;
					thread_alert_.notify_one();
				}
			}
		}
		void delete_tasks()
		{
			if (threadpool_state_ != threadpool_state::terminating)
			{
				std::lock(thread_mutex_, thread_sync_mutex_);
				std::lock_guard<rw_mutex_t> lock0(thread_mutex_, std::adopt_lock);
				std::lock_guard<rw_mutex_t> lock1(thread_sync_mutex_, std::adopt_lock);

				while (!task_queue_.empty())
				{
					if (task_queue_.top().dynamic_function_)
					{
						delete task_queue_.top().function_;
					}
					task_queue_.pop();
				}

				for (auto & thread_ : thread_array_)
				{
					std::lock_guard<std::mutex> lock(thread_.task_mutex_);

					if (thread_.task_.dynamic_function_)
					{
						delete thread_.task_.function_;
					}
					thread_.task_.function_ = nullptr;
					thread_.task_.priority_ = 0U;
				}

				threadpool_new_tasks_ = 0U;
				task_priority_ = static_cast<task_priority>(0U);
			}
		}
		void notify_new_tasks(bool const threadpool_notify_new_tasks)
		{
			if ((threadpool_notify_new_tasks_ = threadpool_notify_new_tasks))
			{
				notify_new_tasks();
			}
		}
		void notify_new_tasks()
		{
			if (threadpool_state_ != threadpool_state::terminating)
			{
				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				threadpool_new_tasks_ = task_queue_.size();
				thread_alert_.notify_all();
			}
		}
		void run_sync_tasks()
		{
			if (threadpool_state_ == threadpool_state::running)
			{
				if (threadpool_run_sync_tasks_)
				{
					throw std::runtime_error("Threadpool synchronizing");
				}

				std::lock_guard<rw_mutex_t> lock(thread_sync_mutex_);

				threadpool_run_sync_tasks_ += threadpool_ready_sync_tasks_;
				threadpool_ready_sync_tasks_ -= threadpool_run_sync_tasks_;
				thread_sync_alert_.notify_all();
			}
		}
		void run()
		{
			if (threadpool_state_ == threadpool_state::waiting)
			{
				std::lock(thread_mutex_, thread_sync_mutex_);
				std::lock_guard<rw_mutex_t> lock0(thread_mutex_, std::adopt_lock);
				std::lock_guard<rw_mutex_t> lock1(thread_sync_mutex_, std::adopt_lock);

				threadpool_state_ = threadpool_state::running;
				thread_alert_.notify_all();
			}
		}
		void wait()
		{
			if (threadpool_state_ == threadpool_state::running)
			{
				std::lock(thread_mutex_, thread_sync_mutex_);
				std::lock_guard<rw_mutex_t> lock0(thread_mutex_, std::adopt_lock);
				std::lock_guard<rw_mutex_t> lock1(thread_sync_mutex_, std::adopt_lock);

				threadpool_state_ = threadpool_state::waiting;
			}
		}
		void terminate()
		{
			if (threadpool_state_ != threadpool_state::terminating)
			{
				std::lock(thread_mutex_, thread_sync_mutex_);
				std::lock_guard<rw_mutex_t> lock0(thread_mutex_, std::adopt_lock);
				std::lock_guard<rw_mutex_t> lock1(thread_sync_mutex_, std::adopt_lock);

				threadpool_state_ = threadpool_state::terminating;
				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}
		}

		size_t waiting() const
		{
			return thread_waiting_;
		}
		size_t running() const
		{
			return thread_running_;
		}
		size_t sync_waiting() const
		{
			return thread_sync_waiting_;
		}
		size_t sync_running() const
		{
			return thread_sync_running_;
		}
		size_t size() const
		{
			return threadpool_size_;
		}
		threadpool_state state() const
		{
			return threadpool_state_;
		}
		bool notify() const
		{
			return threadpool_notify_new_tasks_;
		}

		threadpool(size_t const size = std::thread::hardware_concurrency(), threadpool_state const state = threadpool_state::waiting, bool const notify = true) :
			threadpool_size_(0U),
			task_priority_(static_cast<task_priority>(0U)),
			threadpool_state_(state),
			threadpool_notify_new_tasks_(notify),
			threadpool_new_tasks_(0U),
			threadpool_ready_sync_tasks_(0U),
			threadpool_run_sync_tasks_(0U),
			thread_waiting_(0U),
			thread_running_(0U),
			thread_sync_waiting_(0U),
			thread_sync_running_(0U)
		{
			if (threadpool_state_ == threadpool_state::terminating) // Should this be here? (It would be better if it could be tested at compile time)
			{
				throw std::invalid_argument("Threadpool bad argument");
			}

			for (size_t n = 0U; n < size; ++n)
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
			if (threadpool_state_ != threadpool_state::terminating)
			{
				std::lock(thread_mutex_, thread_sync_mutex_);
				std::lock_guard<rw_mutex_t> lock0(thread_mutex_, std::adopt_lock);
				std::lock_guard<rw_mutex_t> lock1(thread_sync_mutex_, std::adopt_lock);

				threadpool_state_ = threadpool_state::terminating;
			}

			while (threadpool_size_ > 0)
			{
				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}

			for (auto & thread_ : thread_array_)
			{
				thread_.thread_.join();
			}
		}
	private:
#if defined(__clang__)
		// To do
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

		class task_t
		{
		public:
			std::function<void()> * function_;
			bool sync_function_;
			bool dynamic_function_;

			uint32_t priority_;
			std::chrono::high_resolution_clock::time_point origin_;

			task_t(std::function<void()> * function = nullptr,
				   bool const sync_function = false,
				   bool const dynamic_function = false,
				   uint32_t const priority = 3U) :
				function_(function),
				sync_function_(sync_function),
				dynamic_function_(dynamic_function),
				priority_(priority),
				origin_(std::chrono::high_resolution_clock::now())
			{
			}
			~task_t()
			{
				if (dynamic_function_)
				{
					delete function_;
				}
			}

			void operator()()
			{
				(*function_)();
				if (dynamic_function_)
				{
					delete function_;
				}
				function_ = nullptr;
				priority_ = 0;
			}
		};

		class task_comparator_t
		{
		public:
			bool operator()(task_t const & task_0, task_t const & task_1)
			{
				return (task_0.priority_ != task_1.priority_ ?
						task_0.priority_ < task_1.priority_ :
						task_0.origin_ > task_1.origin_);
			}
		};

		class thread_t
		{
		public:
			std::mutex task_mutex_;
			task_t task_;

			threadpool_state thread_state_;
			std::thread thread_;

			template <class FuncType, class ObjType>
			thread_t(FuncType func_ptr, ObjType obj_ptr) : thread_state_(threadpool_state::waiting), thread_(func_ptr, obj_ptr, this) // thread_ must be initialized last
			{
			}
		};

		std::atomic<size_t> threadpool_size_;
		std::list<thread_t> thread_array_;
		std::priority_queue<task_t, std::deque<task_t>, task_comparator_t> task_queue_;
		task_priority task_priority_;
		threadpool_state threadpool_state_;

		bool threadpool_notify_new_tasks_;
		std::atomic<size_t> threadpool_new_tasks_;
		std::atomic<size_t> threadpool_ready_sync_tasks_;
		std::atomic<size_t> threadpool_run_sync_tasks_;

		std::atomic<size_t> thread_waiting_;
		std::atomic<size_t> thread_running_;
		std::atomic<size_t> thread_sync_waiting_;
		std::atomic<size_t> thread_sync_running_;

		rw_mutex_t thread_mutex_;
		rw_mutex_t thread_sync_mutex_;
		std::condition_variable_any thread_alert_;
		std::condition_variable_any thread_sync_alert_;

		void threadpool__(thread_t * const this_thread)
		{
			std::shared_lock<rw_mutex_t> lock(thread_mutex_, std::defer_lock);
			std::shared_lock<rw_mutex_t> sync_lock(thread_sync_mutex_, std::defer_lock);

			this_thread->thread_state_ = threadpool_state_;

			++threadpool_size_;

			while (this_thread->thread_state_ != threadpool_state::terminating)
			{
				lock.lock();

				// ===== Wait for task =====
				while (threadpool_state_ != threadpool_state::terminating
					   && this_thread->thread_state_ != threadpool_state::terminating
					   && ((!threadpool_new_tasks_ && !this_thread->task_.function_)
					   || (!threadpool_new_tasks_ && threadpool_state_ == threadpool_state::waiting)
					   || (static_cast<uint32_t>(task_priority_) <= this_thread->task_.priority_
					   && this_thread->task_.function_ && threadpool_state_ == threadpool_state::waiting)))
				{
					++thread_waiting_;

					thread_alert_.wait(lock);

					--thread_waiting_;
				}

				if (this_thread->thread_state_ != threadpool_state::terminating)
				{
					this_thread->thread_state_ = threadpool_state_;
				}

				lock.unlock();

				// ===== Get task =====
				switch (this_thread->thread_state_)
				{
					case threadpool_state::running:
					case threadpool_state::waiting:
						if (!this_thread->task_.function_)
						{
							if (threadpool_new_tasks_ > 0)
							{
								std::lock_guard<rw_mutex_t> lock(thread_mutex_);

								if (threadpool_new_tasks_ > 0)
								{
									--threadpool_new_tasks_;

									this_thread->task_ = task_queue_.top();
									task_queue_.pop();
									task_priority_ = static_cast<task_priority>(task_queue_.empty() ? 0U : task_queue_.top().priority_);

									break;
								}
							}
							continue;
						}
						else
						{
							if (threadpool_new_tasks_ > 0)
							{
								std::lock_guard<rw_mutex_t> lock(thread_mutex_);

								if (threadpool_new_tasks_ > 0
									&& static_cast<uint32_t>(task_priority_) > this_thread->task_.priority_)
								{
									task_queue_.push(this_thread->task_);
									this_thread->task_ = task_queue_.top();
									task_queue_.pop();
									task_priority_ = static_cast<task_priority>(task_queue_.empty() ? 0U : task_queue_.top().priority_);
								}
							}
						}
						break;
					case threadpool_state::terminating:
						continue;
				}

				// ===== Run task =====
				switch (this_thread->thread_state_)
				{
					case threadpool_state::running:
						if (this_thread->task_.function_ && !this_thread->task_.sync_function_)
						{
							threadpool_run_task__(this_thread);
							break;
						}
					case threadpool_state::waiting:
						if (this_thread->task_.function_ && this_thread->task_.sync_function_)
						{
							threadpool_sync_task__(this_thread, sync_lock);
						}
					case threadpool_state::terminating:
						break;
				}
			}

			--threadpool_size_;
		}
		void threadpool_run_task__(thread_t * const this_thread)
		{
			std::lock_guard<std::mutex> lock(this_thread->task_mutex_);

			if (this_thread->task_.function_)
			{
				++(this_thread->task_.sync_function_ ? thread_sync_running_ : thread_running_);

				this_thread->task_();

				--(this_thread->task_.sync_function_ ? thread_sync_running_ : thread_running_);
			}
		}
		void threadpool_sync_task__(thread_t * const this_thread, std::shared_lock<rw_mutex_t> & sync_lock)
		{
			++threadpool_ready_sync_tasks_;

			while (this_thread->thread_state_ != threadpool_state::terminating)
			{
				sync_lock.lock();

				// ===== Wait for signal =====
				while (threadpool_state_ != threadpool_state::terminating
					   && this_thread->thread_state_ != threadpool_state::terminating
					   && !threadpool_run_sync_tasks_)
				{
					++thread_sync_waiting_;

					thread_sync_alert_.wait(sync_lock);

					--thread_sync_waiting_;
				}

				if (this_thread->thread_state_ != threadpool_state::terminating)
				{
					this_thread->thread_state_ = threadpool_state_;
				}

				sync_lock.unlock();

				// ===== Check signal =====
				switch (this_thread->thread_state_)
				{
					case threadpool_state::running:
						if (threadpool_run_sync_tasks_ > 0)
						{
							--threadpool_run_sync_tasks_;
							break;
						}
					case threadpool_state::waiting:
						continue;
					case threadpool_state::terminating:
						return;
				}

				// ===== Run task =====
				switch (this_thread->thread_state_)
				{
					case threadpool_state::running:
						if (this_thread->task_.function_)
						{
							threadpool_run_task__(this_thread);
						}
						return;
					case threadpool_state::waiting:
						++threadpool_ready_sync_tasks_;
						continue;
					case threadpool_state::terminating:
						return;
				}
			}
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
