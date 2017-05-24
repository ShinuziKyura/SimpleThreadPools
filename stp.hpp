#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <future>
#include <shared_mutex>
#include <queue>

namespace stp
{
	enum class task_priority : uint32_t
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
		template <class FuncType, class ... ArgType>
		task<ReturnType>(FuncType && func, ArgType && ... arg) :
			task_package_(std::bind(std::forward<FuncType>(func), std::forward<ArgType>(arg) ...)), // Eventually i'll have to do this properly
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
				for (size_t n = 0U, it = 0U; n < amount; it + 1 != amount ? ++it : it = 0)
				{
					if (!thread_array_.at(it).task_.function_)
					{
						thread_array_.at(it).thread_active_ = false;
						++n;
					}
				}

				thread_alert_.notify_all();
			}
		}
		template <class ReturnType>
		void new_task(task<ReturnType> & task, task_priority const priority = task_priority::normal)
		{
			std::lock_guard<rw_mutex_t> lock(thread_mutex_);

			task_queue_.emplace(&task.task_function_, false, false, static_cast<uint32_t>(priority));
			if (threadpool_notify_new_tasks_)
			{
				++threadpool_new_tasks_;
				thread_alert_.notify_one();
			}
		}
		template <class FuncType, class ... ArgType>
		void new_task(FuncType && func, ArgType && ... arg)
		{
			new_task(std::forward<FuncType>(func), task_priority::normal, std::forward<ArgType>(arg) ...);
		}
		template <class FuncType, class ... ArgType>
		void new_task(FuncType && func, task_priority const priority, ArgType && ... arg)
		{
			auto task = std::bind(std::forward<FuncType>(func), std::forward<ArgType>(arg) ...);
			auto task_function = new std::function<void()>([=] { task(); });

			std::lock_guard<rw_mutex_t> lock(thread_mutex_);

			task_queue_.emplace(task_function, false, true, static_cast<uint32_t>(priority));
			if (threadpool_notify_new_tasks_)
			{
				++threadpool_new_tasks_;
				thread_alert_.notify_one();
			}
		}
		template <class ReturnType>
		void new_sync_task(task<ReturnType> & task, task_priority const priority = task_priority::normal)
		{
			std::lock_guard<rw_mutex_t> lock(thread_mutex_);

			task_queue_.emplace(&task.task_function_, true, false, static_cast<uint32_t>(priority));
			if (threadpool_notify_new_tasks_)
			{
				++threadpool_new_tasks_;
				thread_alert_.notify_one();
			}
		}
		template <class FuncType, class ... ArgType>
		void new_sync_task(FuncType && func, ArgType && ... arg)
		{
			new_sync_task(std::forward<FuncType>(func), task_priority::normal, std::forward<ArgType>(arg) ...);
		}
		template <class FuncType, class ... ArgType>
		void new_sync_task(FuncType && func, task_priority const priority, ArgType && ... arg)
		{
			auto task = std::bind(std::forward<FuncType>(func), std::forward<ArgType>(arg) ...);
			auto task_function = new std::function<void()>([=] { task(); });

			std::lock_guard<rw_mutex_t> lock(thread_mutex_);

			task_queue_.emplace(task_function, true, true, static_cast<uint32_t>(priority));
			if (threadpool_notify_new_tasks_)
			{
				++threadpool_new_tasks_;
				thread_alert_.notify_one();
			}
		}
		void delete_tasks()
		{
			std::lock_guard<rw_mutex_t, rw_mutex_t> lock(thread_mutex_, thread_sync_mutex_);

			threadpool_new_tasks_ = 0;

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
				std::lock_guard<std::mutex> (thread_.task_mutex_);

				if (thread_.task_.dynamic_function_)
				{
					delete thread_.task_.function_;
				}
				thread_.task_.function_ = nullptr;
			}
		}
		void notify_new_tasks(bool const threadpool_notify_new_tasks)
		{
			if (threadpool_notify_new_tasks_ = threadpool_notify_new_tasks)
			{
				notify_new_tasks();
			}
		}
		void notify_new_tasks()
		{
			if (threadpool_state_ == threadpool_state::running)
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
				std::lock_guard<rw_mutex_t, rw_mutex_t> lock(thread_mutex_, thread_sync_mutex_);

				threadpool_state_ = threadpool_state::running;
				thread_alert_.notify_all();
			}
		}
		void wait()
		{
			if (threadpool_state_ == threadpool_state::running)
			{
				std::lock_guard<rw_mutex_t, rw_mutex_t> lock(thread_mutex_, thread_sync_mutex_);

				threadpool_state_ = threadpool_state::waiting;
			}
		}
		void terminate()
		{
			std::lock_guard<rw_mutex_t, rw_mutex_t> lock(thread_mutex_, thread_sync_mutex_);

			threadpool_state_ = threadpool_state::terminating;
			thread_alert_.notify_all();
			thread_sync_alert_.notify_all();
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
		bool notify() const
		{
			return threadpool_notify_new_tasks_;
		}
		threadpool_state state() const
		{
			return threadpool_state_;
		}

		threadpool(size_t size = std::thread::hardware_concurrency(), threadpool_state state = threadpool_state::waiting, bool notify = true) :
			threadpool_size_(0U),
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
			{
				std::lock_guard<rw_mutex_t, rw_mutex_t> lock(thread_mutex_, thread_sync_mutex_);

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

			void operator()()
			{
				(*function_)();
				if (dynamic_function_)
				{
					delete function_;
				}
				function_ = nullptr;
			}
		};

		class task_comparator_t
		{
		public:
			bool operator()(task_t const & task_1, task_t const & task_2)
			{
				return (task_1.priority_ != task_2.priority_ ? 
						task_1.priority_ < task_2.priority_ : 
						task_1.origin_ > task_2.origin_);
			}
		};

		class thread_t
		{
		public:
			std::mutex task_mutex_;
			task_t task_;

			bool thread_active_;
			std::thread thread_;

			template <class FuncType, class ObjType>
			thread_t(FuncType func_ptr, ObjType obj_ptr) : thread_active_(true), thread_(func_ptr, obj_ptr, this) // thread_ must be initialized last
			{
			}
		};

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
		typedef std::condition_variable_any cond_variable_t;

		std::atomic_size_t threadpool_size_;
		std::deque<thread_t> thread_array_;
		std::priority_queue<task_t, std::deque<task_t>, task_comparator_t> task_queue_;
		threadpool_state threadpool_state_;

		bool threadpool_notify_new_tasks_;
		std::atomic_int32_t threadpool_new_tasks_;
		std::atomic_int32_t threadpool_ready_sync_tasks_;
		std::atomic_int32_t threadpool_run_sync_tasks_;

		std::atomic_size_t thread_waiting_;
		std::atomic_size_t thread_running_;
		std::atomic_size_t thread_sync_waiting_;
		std::atomic_size_t thread_sync_running_;

		rw_mutex_t thread_mutex_;
		rw_mutex_t thread_sync_mutex_;
		cond_variable_t thread_alert_;
		cond_variable_t thread_sync_alert_;

		void threadpool__(thread_t * const this_thread)
		{
			std::unique_lock<rw_mutex_t> unique_lock(thread_mutex_, std::defer_lock);
			std::shared_lock<rw_mutex_t> shared_lock(thread_mutex_, std::defer_lock);
			std::shared_lock<rw_mutex_t> sync_lock(thread_sync_mutex_, std::defer_lock);

			++threadpool_size_;

			while (threadpool_state_ != threadpool_state::terminating && this_thread->thread_active_)
			{
				shared_lock.lock();

				// ===== Wait for task =====
				while (threadpool_state_ != threadpool_state::terminating && this_thread->thread_active_
					   && ((threadpool_new_tasks_ <= 0 && !this_thread->task_.function_)
					   || (this_thread->task_.function_ && threadpool_state_ == threadpool_state::waiting)))
				{
					++thread_waiting_;

					thread_alert_.wait(shared_lock);

					--thread_waiting_;
				}

				shared_lock.unlock();

				// ===== Get task =====
				switch (threadpool_state_)
				{
					case threadpool_state::running:
					case threadpool_state::waiting:
						if (!this_thread->task_.function_)
						{
							if (threadpool_new_tasks_-- > 0)
							{
								unique_lock.lock();

								this_thread->task_ = task_queue_.top();
								task_queue_.pop();

								unique_lock.unlock();

								break;
							}
							++threadpool_new_tasks_;
							continue;
						}
						break;
					case threadpool_state::terminating:
						continue;
				}

				// ===== Run task =====
				switch (threadpool_state_)
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

			while (threadpool_state_ != threadpool_state::terminating && this_thread->thread_active_)
			{
				sync_lock.lock();

				// ===== Wait for signal =====
				while (threadpool_state_ != threadpool_state::terminating && this_thread->thread_active_
					   && threadpool_run_sync_tasks_ <= 0)
				{
					++thread_sync_waiting_;

					thread_sync_alert_.wait(sync_lock);

					--thread_sync_waiting_;
				}

				sync_lock.unlock();

				// ===== Check signal =====
				switch (threadpool_state_)
				{
					case threadpool_state::running:
						if (threadpool_run_sync_tasks_ > 0)
						{
							--threadpool_run_sync_tasks_;
							break;
						}
					case threadpool_state::waiting:
					case threadpool_state::terminating:
						continue;
				}

				// ===== Run task =====
				switch (threadpool_state_)
				{
					case threadpool_state::running:
						if (this_thread->task_.function_)
						{
							threadpool_run_task__(this_thread);
						}
						return;
					case threadpool_state::waiting:
						++threadpool_ready_sync_tasks_;
					case threadpool_state::terminating:
						break;
				}
			}
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
