#ifndef SIMPLE_THREAD_POOLS_HPP_
#define SIMPLE_THREAD_POOLS_HPP_

#include <future>
#include <shared_mutex>
#include <functional>
#include <list>
#include <queue>

namespace stp // C++17 version
{
	enum class task_priority : uint16_t
	{
		maximum		= 5,
		high		= 4,
		normal		= 3,
		low			= 2,
		minimum		= 1
	};

	enum class threadpool_state : uint16_t
	{
		running		= 0b0111,
		stopping	= 0b0011,
		terminating = 0b0001
	};

	template <class RetType>
	class task
	{
	public:
		RetType const & result()
		{
			if (!task_ready_)
			{
				task_result_ = task_future_.get();
				task_ready_ = true;
			}
			return task_result_;
		}
		bool ready()
		{
			return task_ready_ || (task_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready
								   ? (task_result_ = task_future_.get(), task_ready_ = true)
								   : false);
		}
		void wait()
		{
			task_result_ = task_future_.get();
			task_ready_ = true;
		}
		template <class Rep, class Period>
		std::future_status wait_for(std::chrono::duration<Rep, Period> const & timeout_duration)
		{
			auto retval = task_future_.wait_for(timeout_duration);
			if (retval == std::future_status::ready)
			{
				task_result_ = task_future_.get();
				task_ready_ = true;
			}
			return retval;
		}
		template <class Clock, class Duration>
		std::future_status wait_until(std::chrono::time_point<Clock, Duration> const & timeout_time)
		{
			auto retval = task_future_.wait_until(timeout_time);
			if (retval == std::future_status::ready)
			{
				task_result_ = task_future_.get();
				task_ready_ = true;
			}
			return retval;
		}

		task<RetType>() = delete;
		template <class FuncType, class = std::enable_if_t<std::is_convertible_v<FuncType, std::function<RetType()>>>>
		task<RetType>(FuncType & func) :
			task_package_(static_cast<std::function<RetType()>>(func))
		{
		}
		template <class ... ParamType>
		task<RetType>(RetType(* func)(ParamType ...), ParamType && ... arg) :
			task_package_(std::bind(func, arg_wrapper__(std::forward<ParamType>(arg)) ...))
		{
		}
		template <class ObjType, class ... ParamType>
		task<RetType>(RetType(ObjType::* func)(ParamType ...), ObjType * obj, ParamType && ... arg) :
			task_package_(std::bind(func, obj, arg_wrapper__(std::forward<ParamType>(arg)) ...))
		{
		}
		template <class ... ProtoParamType, class ... ParamType,
			class = std::enable_if_t<!std::is_same_v<ProtoParamType ..., ParamType ...>>>
		task<RetType>(RetType(* func)(ProtoParamType ...), ParamType && ... arg) :
			task_package_(std::bind(func, arg_wrapper__(std::forward<ParamType>(arg)) ...))
		{
		}
		template <class ObjType, class ... ProtoParamType, class ... ParamType,
			class = std::enable_if_t<!std::is_same_v<ProtoParamType ..., ParamType ...>>>
		task<RetType>(RetType(ObjType::* func)(ProtoParamType ...), ObjType * obj, ParamType && ... arg) :
			task_package_(std::bind(func, obj, arg_wrapper__(std::forward<ParamType>(arg)) ...))
		{
		}
		task<RetType>(task<RetType> const &) = delete;
		task<RetType> & operator=(task<RetType> const &) = delete;
		task<RetType>(task<RetType> &&) = default;
		task<RetType> & operator=(task<RetType> &&) = delete;
		~task<RetType>() = default;

		RetType const & operator()()
		{
			task_package_();
			task_result_ = task_future_.get();
			task_ready_ = true;
			return task_result_;
		}
	private:
		std::packaged_task<RetType()> task_package_;
		std::unique_ptr<std::function<void()>> task_function_
		{
			std::make_unique<std::function<void()>>([this] { task_package_(); }) 
		};
		std::future<RetType> task_future_
		{
			task_package_.get_future() 
		};
		RetType task_result_
		{
		};
		bool task_ready_ = false;

		template <class ParamType, class = std::enable_if_t<std::is_lvalue_reference_v<ParamType>>>
		constexpr auto arg_wrapper__(ParamType && arg) -> decltype(std::ref(arg))
		{
			return std::ref(arg);
		}
		template <class ParamType, class = std::enable_if_t<!std::is_lvalue_reference_v<ParamType>>>
		constexpr auto arg_wrapper__(ParamType && arg) -> decltype(std::bind(std::move<ParamType &>, arg))
		{
			return std::bind(std::move<ParamType &>, arg);
		}

		friend class threadpool;
	};

	template <>
	class task<void>
	{
	public:
		bool ready()
		{
			return task_ready_ || (task_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready
								   ? (task_ready_ = true)
								   : false);
		}
		void wait()
		{
			task_future_.wait();
			task_ready_ = true;
		}
		template <class Rep, class Period>
		std::future_status wait_for(std::chrono::duration<Rep, Period> const & timeout_duration)
		{
			auto retval = task_future_.wait_for(timeout_duration);
			if (retval == std::future_status::ready)
			{
				task_ready_ = true;
			}
			return retval;
		}
		template <class Clock, class Duration>
		std::future_status wait_until(std::chrono::time_point<Clock, Duration> const & timeout_time)
		{
			auto retval = task_future_.wait_until(timeout_time);
			if (retval == std::future_status::ready)
			{
				task_ready_ = true;
			}
			return retval;
		}

		task() = delete;
		template <class FuncType, class = std::enable_if_t<std::is_convertible_v<FuncType, std::function<void()>>>>
		task(FuncType & func) :
			task_package_(static_cast<std::function<void()>>(func))
		{
		}
		template <class ... ParamType>
		task(void(* func)(ParamType ...), ParamType && ... arg) :
			task_package_(std::bind(func, arg_wrapper__(std::forward<ParamType>(arg)) ...))
		{
		}
		template <class ObjType, class ... ParamType>
		task(void(ObjType::* func)(ParamType ...), ObjType * obj, ParamType && ... arg) :
			task_package_(std::bind(func, obj, arg_wrapper__(std::forward<ParamType>(arg)) ...))
		{
		}
		template <class ... ProtoParamType, class ... ParamType,
			class = std::enable_if_t<!std::is_same_v<ProtoParamType ..., ParamType ...>>>
		task(void(* func)(ProtoParamType ...), ParamType && ... arg) :
			task_package_(std::bind(func, arg_wrapper__(std::forward<ParamType>(arg)) ...))
		{
		}
		template <class ObjType, class ... ProtoParamType, class ... ParamType,
			class = std::enable_if_t<!std::is_same_v<ProtoParamType ..., ParamType ...>>>
		task(void(ObjType::* func)(ProtoParamType ...), ObjType * obj, ParamType && ... arg) :
			task_package_(std::bind(func, obj, arg_wrapper__(std::forward<ParamType>(arg)) ...))
		{
		}
		task(task const &) = delete;
		task & operator=(task const &) = delete;
		task(task &&) = default;
		task & operator=(task &&) = delete;
		~task() = default;

		void operator()()
		{
			task_package_();
			task_ready_ = true;
		}
	private:
		std::packaged_task<void()> task_package_;
		std::unique_ptr<std::function<void()>> task_function_
		{
			std::make_unique<std::function<void()>>([this] { task_package_(); }) 
		};
		std::future<void> task_future_
		{ 
			task_package_.get_future() 
		};
		bool task_ready_ = false;

		template <class ParamType, class = std::enable_if_t<std::is_lvalue_reference_v<ParamType>>>
		constexpr auto arg_wrapper__(ParamType && arg) -> decltype(std::ref(arg))
		{
			return std::ref(arg);
		}
		template <class ParamType, class = std::enable_if_t<!std::is_lvalue_reference_v<ParamType>>>
		constexpr auto arg_wrapper__(ParamType && arg) -> decltype(std::bind(std::move<ParamType &>, arg))
		{
			return std::bind(std::move<ParamType &>, arg);
		}

		friend class threadpool;
	};

	class threadpool
	{
	public:
		template <class RetType>
		void new_task(task<RetType> & task, task_priority priority = task_priority::normal)
		{
			std::scoped_lock<std::mutex> lock(task_queue_mutex_);

			task_queue_.emplace(task.task_function_.get(), static_cast<task_priority_t>(priority));

			if (threadpool_notify_)
			{
				std::scoped_lock<std::shared_mutex> lock(threadpool_mutex_);

				++threadpool_new_tasks_;
				task_priority_ = task_queue_.top().priority_;

				threadpool_condvar_.notify_one();
			}
		}
		void delete_tasks()
		{
			std::scoped_lock<std::shared_mutex, std::mutex> lock(threadpool_mutex_, task_queue_mutex_);

			while (!task_queue_.empty())
			{
				task_queue_.pop();
			}

			threadpool_new_tasks_ = 0;
			task_priority_ = 0;
		}
		void notify_threads(bool threadpool_notify)
		{
			if ((threadpool_notify_ = threadpool_notify))
			{
				notify_threads();
			}
			else
			{
				std::scoped_lock<std::shared_mutex, std::mutex> lock(threadpool_mutex_, task_queue_mutex_);

				threadpool_new_tasks_ = 0;
				task_priority_ = 0;
			}
		}
		void notify_threads()
		{
			std::scoped_lock<std::shared_mutex, std::mutex> lock(threadpool_mutex_, task_queue_mutex_);

			threadpool_new_tasks_ = task_queue_.size();
			task_priority_ = task_queue_.top().priority_;

			threadpool_condvar_.notify_all();
		}
		void run()
		{
			if (threadpool_state_ == threadpool_state_t::stopping)
			{
				std::scoped_lock<std::shared_mutex> lock(threadpool_mutex_);

				threadpool_state_ = threadpool_state_t::running;

				threadpool_condvar_.notify_all();
			}
		}
		void stop()
		{
			if (threadpool_state_ == threadpool_state_t::running)
			{
				std::scoped_lock<std::shared_mutex> lock(threadpool_mutex_);

				threadpool_state_ = threadpool_state_t::stopping;
			}
		}
		void resize(size_t threadpool_size)
		{
			if (threadpool_size_ != threadpool_size)
			{
				uintmax_t threadpool_diff = abs(static_cast<intmax_t>(threadpool_size_) - 
												static_cast<intmax_t>(threadpool_size));

				std::scoped_lock<std::mutex> lock(thread_list_mutex_);

				if (threadpool_size_ < threadpool_size)
				{
					for (uintmax_t n = 0; n < threadpool_diff; ++n)
					{
						thread_list_.emplace_back(this);
					}
				}
				else
				{
					std::unique_lock<std::shared_mutex> threadpool_lock(threadpool_mutex_);

					auto it_b = thread_list_.begin(), it_e = thread_list_.end(), it = it_b;
					for (uintmax_t n = 0; n < threadpool_diff; ++it)
					{
						if (it == it_e)
						{
							it = it_b;

							threadpool_condvar_.notify_all();
							threadpool_lock.unlock();

							std::this_thread::yield();
							threadpool_lock.lock();
						}
						if (!it->task_.function_)
						{
							it->active_ = false;
							++n;
						}
					}

					threadpool_condvar_.notify_all();
				}

				threadpool_size_ = threadpool_size;
			}
		}
		size_t size() const
		{
			return threadpool_size_;
		}
		bool notify() const
		{
			return threadpool_notify_;
		}
		threadpool_state state() const
		{
			return threadpool_state_;
		}
		size_t running() const
		{
			return thread_running_;
		}
		size_t waiting() const
		{
			return thread_waiting_;
		}

		threadpool(size_t size = std::thread::hardware_concurrency(),
				   bool notify = true,
				   threadpool_state state = threadpool_state::running) :
			threadpool_size_(size),
			threadpool_notify_(notify),
			task_priority_(0),
			threadpool_state_(state),
			threadpool_new_tasks_(0),
			thread_active_(0),
			thread_running_(0),
			thread_waiting_(0)
		{
			for (size_t n = 0; n < threadpool_size_; ++n)
			{
				thread_list_.emplace_back(this);
			}
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			{
				std::scoped_lock<std::shared_mutex> lock(threadpool_mutex_);

				threadpool_state_ = threadpool_state_t::terminating;

				threadpool_condvar_.notify_all();
			}

			while (thread_active_);
		}
	private:
		using task_priority_t = uint16_t;
		using threadpool_state_t = threadpool_state;

		struct task_t
		{
			std::function<void()> * function_;

			task_priority_t priority_;
			std::chrono::high_resolution_clock::time_point age_;

			task_t(std::function<void()> * function = nullptr, task_priority_t priority = 0) :
				function_(function),
				priority_(priority),
				age_(std::chrono::high_resolution_clock::now())
			{
			}
		};

		struct thread_t
		{
			task_t task_;

			std::atomic_bool active_;
			std::thread thread_;

			thread_t(threadpool * threadpool) : 
				task_(),
				active_(true),
				thread_(&threadpool::thread__, threadpool, this)
			{
			}
		};

		size_t threadpool_size_;
		bool threadpool_notify_;
		std::list<thread_t> thread_list_;
		std::priority_queue<task_t, std::deque<task_t>, bool(*)(task_t const &, task_t const &)> task_queue_
		{
			[] (task_t const & task_1, task_t const & task_2) -> bool
			{
				return (task_1.priority_ != task_2.priority_
						? task_1.priority_ < task_2.priority_
						: task_1.age_ > task_2.age_);
			}
		};
		
		std::atomic<task_priority_t> task_priority_;
		std::atomic<threadpool_state_t> threadpool_state_;
		std::atomic_size_t threadpool_new_tasks_;

		std::atomic_size_t thread_active_;
		std::atomic_size_t thread_running_;
		std::atomic_size_t thread_waiting_;

		std::mutex thread_list_mutex_;
		std::mutex task_queue_mutex_;
		std::shared_mutex threadpool_mutex_;
		std::condition_variable_any threadpool_condvar_;

		void threadpool__(thread_t * this_thread)
		{
			std::shared_lock<std::shared_mutex> threadpool_lock(threadpool_mutex_, std::defer_lock);

			while (threadpool_state_ != threadpool_state_t::terminating && this_thread->active_)
			{
				threadpool_lock.lock();

				while (threadpool_state_ != threadpool_state_t::terminating && this_thread->active_
					   && ((!threadpool_new_tasks_ && !this_thread->task_.function_)
					   || (!threadpool_new_tasks_ && threadpool_state_ == threadpool_state_t::stopping)
					   || (task_priority_ <= this_thread->task_.priority_ && this_thread->task_.function_
					   && threadpool_state_ == threadpool_state_t::stopping)))
				{
					++thread_waiting_;

					threadpool_condvar_.wait(threadpool_lock);

					--thread_waiting_;
				}

				threadpool_lock.unlock();

				if (this_thread->active_)
				{
					switch (threadpool_state_)
					{
						case threadpool_state_t::running:
						case threadpool_state_t::stopping:
							if ((!this_thread->task_.function_ && threadpool_new_tasks_)
								|| task_priority_ > this_thread->task_.priority_)
							{
								std::scoped_lock<std::mutex> lock(task_queue_mutex_);

								if (!this_thread->task_.function_)
								{
									if (threadpool_new_tasks_)
									{
										--threadpool_new_tasks_;

										this_thread->task_ = task_queue_.top();
										task_queue_.pop();

										task_priority_ = (!task_queue_.empty() ? task_queue_.top().priority_ : 0);
									}
								}
								else if (task_priority_ > this_thread->task_.priority_)
								{
									task_queue_.push(this_thread->task_);

									this_thread->task_ = task_queue_.top();
									task_queue_.pop();

									task_priority_ = (!task_queue_.empty() ? task_queue_.top().priority_ : 0);
								}
							}
						case threadpool_state_t::terminating:
							break;
					}

					switch (threadpool_state_)
					{
						case threadpool_state_t::running:
							if (this_thread->task_.function_)
							{
								++thread_running_;

								(*this_thread->task_.function_)();
								this_thread->task_.function_ = nullptr;
								this_thread->task_.priority_ = 0;

								--thread_running_;
							}
						case threadpool_state_t::stopping:
						case threadpool_state_t::terminating:
							break;
					}
				}
			}
		}
		void thread__(thread_t * this_thread)
		{
			++thread_active_;

			threadpool__(this_thread);

			std::scoped_lock<std::mutex> lock(thread_list_mutex_);

			auto it = thread_list_.begin();
			while (this_thread->thread_.get_id() != it->thread_.get_id())
			{
				++it;
			}
			it->thread_.detach();
			thread_list_.erase(it);

			--thread_active_;
		}
	};
}

#endif