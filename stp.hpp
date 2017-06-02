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
		maximum = 6u,
		very_high = 5u,
		high = 4u,
		normal = 3u,
		low = 2u,
		very_low = 1u,
		minimum = 0u
	};

	enum class threadpool_state : uint32_t
	{
		running = 0u,
		waiting = 1u,
		terminating = 2u
	};

	template <class ReturnType>
	class task
	{
	public:
		bool ready() const
		{
			return task_result_ || task_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready; // Calling wait_for after task_result_ != nullptr would be UB
		}
		void await()
		{
			if (!task_result_)
			{
				task_result_ = std::make_unique<ReturnType>(task_future_.get());
			}
		}
		ReturnType result()
		{
			if (!task_result_)
			{
				if (task_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				{
					throw std::runtime_error("Task not ready");
				}

				task_result_ = std::make_unique<ReturnType>(task_future_.get());
			}

			return *task_result_;
		}

		task<ReturnType>() = delete;
		template <class FuncType, class ... ArgsType>
		task<ReturnType>(FuncType && func, ArgsType && ... args) :
			task_package_(std::bind(std::forward<FuncType>(func), std::forward<ArgsType>(args) ...)), // Eventually I'll have to do this properly
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

	template <>
	class task<void>
	{
	public:
		bool ready() const
		{
			return task_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		}
		void await()
		{
			task_future_.wait();
		}

		task() = delete;
		template <class FuncType, class ... ArgsType>
		task<void>(FuncType && func, ArgsType && ... args) :
			task_package_(std::bind(std::forward<FuncType>(func), std::forward<ArgsType>(args) ...)),
			task_function_([this] { task_package_(); }),
			task_future_(task_package_.get_future())
		{
		}
		task(task const &) = delete;
		task & operator=(task const &) = delete;
		task(task &&) = default;
		task & operator=(task &&) = default;
		~task() = default;

		void operator()()
		{
			if (task_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			{
				task_function_();
			}
		}
	private:
		std::packaged_task<void()> task_package_;
		std::function<void()> task_function_;
		std::future<void> task_future_;

		friend class threadpool;
	};

	class threadpool
	{
	public:
		void new_threads(size_t const amount)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				for (size_t n = 0u; n < amount; ++n)
				{
					thread_array_.emplace_back(&threadpool::threadpool__, this);
				}
			}
		}
		void delete_threads(size_t const amount)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				if (threadpool_size_ < amount)
				{
					std::invalid_argument("Threadpool bad argument");
				}

				auto it = thread_array_.begin(), it_b = thread_array_.begin(), it_e = thread_array_.end();
				for (size_t n = 0u; n < amount; ++it == it_e ? it = it_b : it)
				{
					if (it->thread_state_ != thread_state_t::terminating && !it->task_.function_)
					{
						std::unique_lock<rw_mutex_t> lock(thread_mutex_, std::try_to_lock);

						if (lock.owns_lock() && !it->task_.function_)
						{
							it->thread_state_ = thread_state_t::terminating;
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
			if (thread_state_ != thread_state_t::terminating)
			{
				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				task_queue_.emplace(&task.task_function_, false, false, static_cast<task_priority_t>(priority));
				task_priority_ = task_queue_.top().priority_;

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
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(std::forward<FuncType>(func), std::forward<ArgsType>(args) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				task_queue_.emplace(task_function, false, true, 3u);
				task_priority_ = task_queue_.top().priority_;

				if (threadpool_notify_new_tasks_)
				{
					++threadpool_new_tasks_;
					thread_alert_.notify_one();
				}
			}
		}
		template <class FuncType, class ... ArgsType>
		void new_task(FuncType && func, task_priority const priority, ArgsType && ... args)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(std::forward<FuncType>(func), std::forward<ArgsType>(args) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				task_queue_.emplace(task_function, false, true, static_cast<task_priority_t>(priority));
				task_priority_ = task_queue_.top().priority_;

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
			if (thread_state_ != thread_state_t::terminating)
			{
				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				task_queue_.emplace(&task.task_function_, true, false, static_cast<task_priority_t>(priority));
				task_priority_ = task_queue_.top().priority_;

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
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(std::forward<FuncType>(func), std::forward<ArgsType>(args) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				task_queue_.emplace(task_function, true, true, 3u);
				task_priority_ = task_queue_.top().priority_;

				if (threadpool_notify_new_tasks_)
				{
					++threadpool_new_tasks_;
					thread_alert_.notify_one();
				}
			}
		}
		template <class FuncType, class ... ArgsType>
		void new_sync_task(FuncType && func, task_priority const priority, ArgsType && ... args)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(std::forward<FuncType>(func), std::forward<ArgsType>(args) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				task_queue_.emplace(task_function, true, true, static_cast<task_priority_t>(priority));
				task_priority_ = task_queue_.top().priority_;

				if (threadpool_notify_new_tasks_)
				{
					++threadpool_new_tasks_;
					thread_alert_.notify_one();
				}
			}
		}
		void delete_tasks()
		{
			if (thread_state_ != thread_state_t::terminating)
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
					std::unique_lock<std::mutex> lock(thread_.task_mutex_, std::try_to_lock);

					if (lock.owns_lock())
					{
						if (thread_.task_.dynamic_function_)
						{
							delete thread_.task_.function_;
						}
						thread_.task_.function_ = nullptr;
						thread_.task_.priority_ = 0u;
					}
				}

				threadpool_new_tasks_ = 0u;
				task_priority_ = 0u;
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
			if (thread_state_ != thread_state_t::terminating)
			{
				std::lock_guard<rw_mutex_t> lock(thread_mutex_);

				threadpool_new_tasks_ = task_queue_.size();
				thread_alert_.notify_all();
			}
		}
		void run_sync_tasks()
		{
			if (thread_state_ == thread_state_t::running)
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
			if (thread_state_ == thread_state_t::waiting)
			{
				std::lock(thread_mutex_, thread_sync_mutex_);
				std::lock_guard<rw_mutex_t> lock0(thread_mutex_, std::adopt_lock);
				std::lock_guard<rw_mutex_t> lock1(thread_sync_mutex_, std::adopt_lock);

				thread_state_ = thread_state_t::running;
				thread_alert_.notify_all();
			}
		}
		void wait()
		{
			if (thread_state_ == thread_state_t::running)
			{
				std::lock(thread_mutex_, thread_sync_mutex_);
				std::lock_guard<rw_mutex_t> lock0(thread_mutex_, std::adopt_lock);
				std::lock_guard<rw_mutex_t> lock1(thread_sync_mutex_, std::adopt_lock);

				thread_state_ = thread_state_t::waiting;
			}
		}
		void terminate()
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				std::lock(thread_mutex_, thread_sync_mutex_);
				std::lock_guard<rw_mutex_t> lock0(thread_mutex_, std::adopt_lock);
				std::lock_guard<rw_mutex_t> lock1(thread_sync_mutex_, std::adopt_lock);

				thread_state_ = thread_state_t::terminating;
				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}
		}
		size_t running() const
		{
			return thread_running_;
		}
		size_t waiting() const
		{
			return thread_waiting_;
		}
		size_t sync_running() const
		{
			return thread_sync_running_;
		}
		size_t sync_waiting() const
		{
			return thread_sync_waiting_;
		}
		size_t size() const
		{
			return threadpool_size_;
		}
		threadpool_state state() const
		{
			return thread_state_;
		}
		bool notify() const
		{
			return threadpool_notify_new_tasks_;
		}

		threadpool(size_t const size = std::thread::hardware_concurrency(), threadpool_state const state = threadpool_state::running, bool const notify = true) :
			threadpool_size_(0u),
			thread_state_(state),
			task_priority_(0u),
			threadpool_notify_new_tasks_(notify),
			threadpool_new_tasks_(0u),
			threadpool_ready_sync_tasks_(0u),
			threadpool_run_sync_tasks_(0u),
			thread_running_(0u),
			thread_waiting_(0u),
			thread_sync_running_(0u),
			thread_sync_waiting_(0u)
		{
			for (size_t n = 0u; n < size; ++n)
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
			if (thread_state_ != thread_state_t::terminating)
			{
				std::lock(thread_mutex_, thread_sync_mutex_);
				std::lock_guard<rw_mutex_t> lock0(thread_mutex_, std::adopt_lock);
				std::lock_guard<rw_mutex_t> lock1(thread_sync_mutex_, std::adopt_lock);

				thread_state_ = thread_state_t::terminating;
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
#if defined(__clang__) && __clang_major__ >= 3
#if ((__clang_major__ == 3 && __clang_minor__ >= 7) || __clang_major__ > 3) && _HAS_SHARED_MUTEX == 1
		typedef std::shared_mutex rw_mutex_t;
#elif __clang_minor__ >= 4
		typedef std::shared_timed_mutex rw_mutex_t;
#endif
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

		typedef uint32_t task_priority_t;

		typedef threadpool_state thread_state_t;

		struct task_t
		{
			std::function<void()> * function_;
			bool sync_function_;
			bool dynamic_function_;

			task_priority_t priority_;
			std::chrono::high_resolution_clock::time_point origin_;

			task_t(std::function<void()> * function = nullptr,
				   bool const sync_function = false,
				   bool const dynamic_function = false,
				   task_priority_t const priority = 3u) :
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
				priority_ = 0u;
			}
		};

		struct thread_t
		{
			std::mutex task_mutex_;
			task_t task_;

			thread_state_t thread_state_;
			std::thread thread_;

			thread_t(void(threadpool::* func_ptr)(thread_t *), threadpool * obj_ptr) : thread_state_(obj_ptr->thread_state_), thread_(func_ptr, obj_ptr, this)
			{
			}
		};

		std::atomic<size_t> threadpool_size_;
		std::list<thread_t> thread_array_;
		std::priority_queue<task_t, std::deque<task_t>, bool(*)(task_t const &, task_t const &)> task_queue_
		{
			[](task_t const & task_1, task_t const & task_2)
			{
				return (task_1.priority_ != task_2.priority_ ?
						task_1.priority_ < task_2.priority_ :
						task_1.origin_ > task_2.origin_);
			}
		};
		thread_state_t thread_state_;
		task_priority_t task_priority_;

		bool threadpool_notify_new_tasks_;
		std::atomic<size_t> threadpool_new_tasks_;
		std::atomic<size_t> threadpool_ready_sync_tasks_;
		std::atomic<size_t> threadpool_run_sync_tasks_;

		std::atomic<size_t> thread_running_;
		std::atomic<size_t> thread_waiting_;
		std::atomic<size_t> thread_sync_running_;
		std::atomic<size_t> thread_sync_waiting_;

		rw_mutex_t thread_mutex_;
		rw_mutex_t thread_sync_mutex_;
		std::condition_variable_any thread_alert_;
		std::condition_variable_any thread_sync_alert_;

		void threadpool__(thread_t * const this_thread)
		{
			std::shared_lock<rw_mutex_t> lock(thread_mutex_, std::defer_lock);
			std::shared_lock<rw_mutex_t> sync_lock(thread_sync_mutex_, std::defer_lock);

			++threadpool_size_;

			while (this_thread->thread_state_ != thread_state_t::terminating)
			{
				lock.lock();

				// ===== Wait for task =====
				while (thread_state_ != thread_state_t::terminating
					   && this_thread->thread_state_ != thread_state_t::terminating
					   && ((!threadpool_new_tasks_ && !this_thread->task_.function_)
					   || (!threadpool_new_tasks_ && thread_state_ == thread_state_t::waiting)
					   || (task_priority_ <= this_thread->task_.priority_
					   && this_thread->task_.function_ && thread_state_ == thread_state_t::waiting)))
				{
					++thread_waiting_;

					thread_alert_.wait(lock);

					--thread_waiting_;
				}

				if (this_thread->thread_state_ != thread_state_t::terminating)
				{
					this_thread->thread_state_ = thread_state_;
				}

				lock.unlock();

				// ===== Get task =====
				switch (this_thread->thread_state_)
				{
					case thread_state_t::running:
					case thread_state_t::waiting:
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
									task_priority_ = (task_queue_.empty() ? 0u : task_queue_.top().priority_);

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
									&& task_priority_ > this_thread->task_.priority_)
								{
									task_queue_.push(this_thread->task_);
									this_thread->task_ = task_queue_.top();
									task_queue_.pop();
									task_priority_ = (task_queue_.empty() ? 0u : task_queue_.top().priority_);
								}
							}
						}
						break;
					case thread_state_t::terminating:
						continue;
				}

				// ===== Run task =====
				switch (this_thread->thread_state_)
				{
					case thread_state_t::running:
						if (this_thread->task_.function_ && !this_thread->task_.sync_function_)
						{
							threadpool_run_task__(this_thread);
							break;
						}
					case thread_state_t::waiting:
						if (this_thread->task_.function_ && this_thread->task_.sync_function_)
						{
							threadpool_sync_task__(this_thread, sync_lock);
						}
					case thread_state_t::terminating:
						break;
				}
			}

			--threadpool_size_;
		}
		void threadpool_sync_task__(thread_t * const this_thread, std::shared_lock<rw_mutex_t> & sync_lock)
		{
			++threadpool_ready_sync_tasks_;

			while (this_thread->thread_state_ != thread_state_t::terminating)
			{
				sync_lock.lock();

				// ===== Wait for signal =====
				while (thread_state_ != thread_state_t::terminating
					   && this_thread->thread_state_ != thread_state_t::terminating
					   && !threadpool_run_sync_tasks_)
				{
					++thread_sync_waiting_;

					thread_sync_alert_.wait(sync_lock);

					--thread_sync_waiting_;
				}

				if (this_thread->thread_state_ != thread_state_t::terminating)
				{
					this_thread->thread_state_ = thread_state_;
				}

				sync_lock.unlock();

				// ===== Check signal =====
				switch (this_thread->thread_state_)
				{
					case thread_state_t::running:
						if (threadpool_run_sync_tasks_ > 0)
						{
							--threadpool_run_sync_tasks_;
							break;
						}
					case thread_state_t::waiting:
						continue;
					case thread_state_t::terminating:
						return;
				}

				// ===== Run task =====
				switch (this_thread->thread_state_)
				{
					case thread_state_t::running:
						if (this_thread->task_.function_)
						{
							threadpool_run_task__(this_thread);
						}
						return;
					case thread_state_t::waiting:
						++threadpool_ready_sync_tasks_;
						continue;
					case thread_state_t::terminating:
						return;
				}
			}
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
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
