#ifndef SIMPLE_THREAD_POOLS_HPP_
#define SIMPLE_THREAD_POOLS_HPP_

#include <future>
#include <shared_mutex>
#include <functional>
#include <list>
#include <queue>

namespace stp
{
	namespace stpi // Implementation namespace
	{
		template <class ParamType, class = typename std::enable_if<std::is_lvalue_reference<ParamType>::value>::type>
		constexpr auto value_wrapper(ParamType && arg) -> decltype(std::ref(arg))
		{
			return std::ref(arg);
		}
		template <class ParamType, class = typename std::enable_if<!std::is_lvalue_reference<ParamType>::value>::type>
		constexpr auto value_wrapper(ParamType && arg) -> decltype(std::bind(std::move<ParamType &>, arg))
		{
			return std::bind(std::move<ParamType &>, arg);
		}
	}

	enum class task_priority : uint16_t
	{
		maximum = 6u,
		very_high = 5u,
		high = 4u,
		normal = 3u,
		low = 2u,
		very_low = 1u,
		minimum = 0u
	};

	enum class threadpool_state : uint16_t
	{
		running = 0u,
		waiting = 1u,
		terminating = 2u
	};

	template <class RetType>
	class task
	{
	public:
		bool ready()
		{
			return task_ready_ || (task_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready 
								   ? (task_result_ = std::move(task_future_.get())), (task_ready_ = true)
								   : false);
		}
		void wait()
		{
			task_result_ = std::move(task_future_.get());
			task_ready_ = true;
		}
		template <class Rep, class Per>
		std::future_status wait_for(std::chrono::duration<Rep, Per> const & timeout_duration)
		{
			auto retval = task_future_.wait_for(timeout_duration);
			if (retval == std::future_status::ready)
			{
				task_result_ = std::move(task_future_.get());
				task_ready_ = true;
			}
			return retval;
		}
		template <class Clock, class Dur>
		std::future_status wait_until(std::chrono::time_point<Clock, Dur> const & timeout_time)
		{
			auto retval = task_future_.wait_until(timeout_time);
			if (retval == std::future_status::ready)
			{
				task_result_ = std::move(task_future_.get());
				task_ready_ = true;
			}
			return retval;
		}
		RetType const & result()
		{
			if (!task_ready_)
			{
				task_result_ = std::move(task_future_.get());
				task_ready_ = true;
			}
			return task_result_;
		}

		task<RetType>() = delete;
		task<RetType>(std::function<RetType()> const & func) :
			task_package_(func),
			task_future_(task_package_.get_future())
		{
		}
		template <class ClosureType,
			class = typename std::enable_if<std::is_convertible<ClosureType, std::function<RetType()>>::value>::type>
		task<RetType>(ClosureType const & func) :
			task_package_(func),
			task_future_(task_package_.get_future())
		{
		}
		template <class ... ParamType>
		task<RetType>(RetType(* func)(ParamType ...), ParamType && ... args) :
			task_package_(std::bind(func, stpi::value_wrapper(std::forward<ParamType>(args)) ...)),
			task_future_(task_package_.get_future())
		{
		}
		template <class ObjType, class ... ParamType>
		task<RetType>(RetType(ObjType::* func)(ParamType ...), ObjType * obj, ParamType && ... args) :
			task_package_(std::bind(func, obj, stpi::value_wrapper(std::forward<ParamType>(args)) ...)),
			task_future_(task_package_.get_future())
		{
		}
		template <class ... RetParamType, class ... ParamType,
			class = typename std::enable_if<!std::is_same<RetParamType ..., ParamType ...>::value>::type>
		task<RetType>(RetType(* func)(RetParamType ...), ParamType && ... args) :
			task_package_(std::bind(func, stpi::value_wrapper(std::forward<ParamType>(args)) ...)),
			task_future_(task_package_.get_future())
		{
		}
		template <class ObjType, class ... RetParamType, class ... ParamType,
			class = typename std::enable_if<!std::is_same<RetParamType ..., ParamType ...>::value>::type>
		task<RetType>(RetType(ObjType::* func)(RetParamType ...), ObjType * obj, ParamType && ... args) :
			task_package_(std::bind(func, obj, stpi::value_wrapper(std::forward<ParamType>(args)) ...)),
			task_future_(task_package_.get_future())
		{
		}
		task<RetType>(task<RetType> const &) = delete;
		task<RetType> & operator=(task<RetType> const &) = delete;
		task<RetType>(task<RetType> &&) = default;
		task<RetType> & operator=(task<RetType> &&) = default;
		~task<RetType>() = default;

		RetType operator()()
		{
			task_package_();
			task_result_ = std::move(task_future_.get());
			task_ready_ = true;
			return task_result_;
		}
	private:
		std::packaged_task<RetType()> task_package_;
		std::future<RetType> task_future_;
		RetType task_result_;
		bool task_ready_ = false;

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
		template <class Rep, class Per>
		std::future_status wait_for(std::chrono::duration<Rep, Per> const & timeout_duration)
		{
			auto retval = task_future_.wait_for(timeout_duration);
			if (retval == std::future_status::ready)
			{
				task_ready_ = true;
			}
			return retval;
		}
		template <class Clock, class Dur>
		std::future_status wait_until(std::chrono::time_point<Clock, Dur> const & timeout_time)
		{
			auto retval = task_future_.wait_until(timeout_time);
			if (retval == std::future_status::ready)
			{
				task_ready_ = true;
			}
			return retval;
		}

		task() = delete;
		template <class RetType>
		task(std::function<RetType()> const & func) :
			task_package_(func),
			task_future_(task_package_.get_future())
		{
		}
		template <class RetType, class ClosureType,
			class = typename std::enable_if<std::is_convertible<ClosureType, std::function<RetType()>>::value>::type>
		task(ClosureType const & func) :
			task_package_(func),
			task_future_(task_package_.get_future())
		{
		}
		template <class RetType, class ... ParamType>
		task(RetType(* func)(ParamType ...), ParamType && ... args) :
			task_package_(std::bind(func, stpi::value_wrapper(std::forward<ParamType>(args)) ...)),
			task_future_(task_package_.get_future())
		{
		}
		template <class RetType, class ObjType, class ... ParamType>
		task(RetType(ObjType::* func)(ParamType ...), ObjType * obj, ParamType && ... args) :
			task_package_(std::bind(func, obj, stpi::value_wrapper(std::forward<ParamType>(args)) ...)),
			task_future_(task_package_.get_future())
		{
		}
		template <class RetType, class ... RetParamType, class ... ParamType,
			class = typename std::enable_if<!std::is_same<RetParamType ..., ParamType ...>::value>::type>
		task(RetType(* func)(RetParamType ...), ParamType && ... args) :
			task_package_(std::bind(func, stpi::value_wrapper(std::forward<ParamType>(args)) ...)),
			task_future_(task_package_.get_future())
		{
		}
		template <class RetType, class ObjType, class ... RetParamType, class ... ParamType,
			class = typename std::enable_if<!std::is_same<RetParamType ..., ParamType ...>::value>::type>
		task(RetType(ObjType::* func)(RetParamType ...), ObjType * obj, ParamType && ... args) :
			task_package_(std::bind(func, obj, stpi::value_wrapper(std::forward<ParamType>(args)) ...)),
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
			task_package_();
			task_ready_ = true;
		}
	private:
		std::packaged_task<void()> task_package_;
		std::future<void> task_future_;
		bool task_ready_ = false;

		friend class threadpool;
	};

	class threadpool
	{
	public:
		template <class RetType>
		void new_task(task<RetType> & task, task_priority priority = task_priority::normal)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				new_task__([&task] { task.task_package_(); }, false, priority);
			}
		}
		template <class RetType>
		void new_task(std::packaged_task<RetType()> & task, task_priority priority = task_priority::normal)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				new_task__([&task] { task(); }, false, priority);
			}
		}
		template <class RetType>
		void new_sync_task(task<RetType> & task, task_priority priority = task_priority::normal)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				new_task__([&task] { task.task_package_(); }, true, priority);
			}
		}
		template <class RetType>
		void new_sync_task(std::packaged_task<RetType()> & task, task_priority priority = task_priority::normal)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				new_task__([&task] { task(); }, true, priority);
			}
		}
		void delete_tasks()
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				std::scoped_lock<std::mutex, std::shared_mutex> lock(thread_task_mutex_, thread_state_mutex_);

				while (!task_queue_.empty())
				{
					task_queue_.pop();
				}

				for (auto & thread : thread_array_)
				{
					std::unique_lock<std::mutex> lock(thread.task_mutex_, std::try_to_lock);

					if (lock.owns_lock())
					{
						thread.task_.function_.reset();
						thread.task_.priority_ = 0u;
					}
				}

				threadpool_new_tasks_ = 0u;
				task_priority_ = 0u;
			}
		}
		void notify_new_tasks(bool threadpool_notify_new_tasks)
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
				std::scoped_lock<std::mutex, std::shared_mutex> lock(thread_task_mutex_, thread_state_mutex_);

				threadpool_new_tasks_ = task_queue_.size();
				thread_state_condvar_.notify_all();
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

				std::scoped_lock<std::shared_mutex> lock(thread_sync_mutex_);

				threadpool_run_sync_tasks_ += threadpool_ready_sync_tasks_;
				threadpool_ready_sync_tasks_ -= threadpool_run_sync_tasks_;
				thread_sync_condvar_.notify_all();
			}
		}
		void run()
		{
			if (thread_state_ == thread_state_t::waiting)
			{
				std::scoped_lock<std::shared_mutex, std::shared_mutex> lock(thread_state_mutex_, thread_sync_mutex_);

				thread_state_ = thread_state_t::running;
				thread_state_condvar_.notify_all();
			}
		}
		void wait()
		{
			if (thread_state_ == thread_state_t::running)
			{
				std::scoped_lock<std::shared_mutex, std::shared_mutex> lock(thread_state_mutex_, thread_sync_mutex_);

				thread_state_ = thread_state_t::waiting;
			}
		}
		void terminate()
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				std::scoped_lock<std::shared_mutex, std::shared_mutex> lock(thread_state_mutex_, thread_sync_mutex_);

				thread_state_ = thread_state_t::terminating;
				threadpool_size_ = 0u;
				thread_state_condvar_.notify_all();
				thread_sync_condvar_.notify_all();
			}
		}
		void resize(size_t threadpool_size)
		{
			uintmax_t amount = std::abs(static_cast<intmax_t>(threadpool_size_) - static_cast<intmax_t>(threadpool_size));

			if (threadpool_size_ < threadpool_size)
			{
				if (thread_state_ != thread_state_t::terminating)
				{
					auto it = thread_array_.begin(), it_e = thread_array_.end();
					while (it != it_e)
					{
						if (it->thread_state_ == thread_state_t::terminating)
						{
							it->thread_.join();
							it = thread_array_.erase(it);
							continue;
						}
						++it;
					}

					for (size_t n = 0u; n < amount; ++n)
					{
						thread_array_.emplace_back(&threadpool::threadpool__, this);
					}
				}
			}
			else
			{
				if (thread_state_ != thread_state_t::terminating)
				{
					std::scoped_lock<std::shared_mutex> lock(thread_state_mutex_);

					auto it = thread_array_.begin(), it_b = thread_array_.begin(), it_e = thread_array_.end();
					for (size_t n = 0u; n < amount; ++it == it_e ? it = it_b : it)
					{
						if (it->thread_state_ == thread_state_t::running && !it->task_.function_)
						{
							it->thread_state_ = thread_state_t::waiting;
							++n;
						}
					}

					thread_state_condvar_.notify_all();
				}
			}

			threadpool_size_ = threadpool_size;
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
		size_t active() const
		{
			return thread_active_;
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

		threadpool(size_t size = std::thread::hardware_concurrency(),
				   threadpool_state state = threadpool_state::running,
				   bool notify = true) :
			threadpool_size_(size),
			thread_state_(state),
			task_priority_(0u),
			threadpool_notify_new_tasks_(notify),
			threadpool_new_tasks_(0u),
			threadpool_ready_sync_tasks_(0u),
			threadpool_run_sync_tasks_(0u),
			thread_active_(0u),
			thread_running_(0u),
			thread_waiting_(0u),
			thread_sync_running_(0u),
			thread_sync_waiting_(0u)
		{
			for (size_t n = 0u; n < threadpool_size_; ++n)
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
				std::scoped_lock<std::shared_mutex, std::shared_mutex> lock(thread_state_mutex_, thread_sync_mutex_);

				thread_state_ = thread_state_t::terminating;
			}

			while (thread_active_ > 0)
			{
				thread_state_condvar_.notify_all();
				thread_sync_condvar_.notify_all();

				std::this_thread::yield();
			}

			for (auto & thread : thread_array_)
			{
				thread.thread_.join();
			}
		}
	private:
		typedef uint16_t task_priority_t;
		typedef threadpool_state thread_state_t;

		struct task_t
		{
			std::shared_ptr<std::function<void()>> function_;
			bool sync_function_;

			task_priority_t priority_;
			std::chrono::high_resolution_clock::time_point creation_;

			task_t(std::shared_ptr<std::function<void()>> const & function = nullptr,
				   bool sync_function = false,
				   task_priority_t priority = 0u) :
				function_(function),
				sync_function_(sync_function),
				priority_(priority),
				creation_(std::chrono::high_resolution_clock::now())
			{
			}

			void operator()()
			{
				(*function_)();
				function_.reset();
				priority_ = 0u;
			}
		};

		struct thread_t
		{
			std::mutex task_mutex_;
			task_t task_;

			thread_state_t thread_state_;
			std::thread thread_;

			thread_t(void(threadpool::* func)(thread_t *),
					 threadpool * obj) :
				thread_state_(thread_state_t::running),
				thread_(func, obj, this)
			{
			}
		};

		size_t threadpool_size_;
		std::list<thread_t> thread_array_;
		std::priority_queue<task_t, std::deque<task_t>, bool(*)(task_t const &, task_t const &)> task_queue_
		{
			[] (task_t const & task_1, task_t const & task_2)
			{
				return (task_1.priority_ != task_2.priority_
						? task_1.priority_ < task_2.priority_
						: task_1.creation_ > task_2.creation_);
			}
		};
		thread_state_t thread_state_;
		std::atomic<task_priority_t> task_priority_;
		bool threadpool_notify_new_tasks_;

		std::atomic<size_t> threadpool_new_tasks_;
		std::atomic<size_t> threadpool_ready_sync_tasks_;
		std::atomic<size_t> threadpool_run_sync_tasks_;

		std::atomic<size_t> thread_active_;
		std::atomic<size_t> thread_running_;
		std::atomic<size_t> thread_waiting_;
		std::atomic<size_t> thread_sync_running_;
		std::atomic<size_t> thread_sync_waiting_;

		std::mutex thread_task_mutex_;
		std::shared_mutex thread_state_mutex_;
		std::shared_mutex thread_sync_mutex_;
		std::condition_variable_any thread_state_condvar_;
		std::condition_variable_any thread_sync_condvar_;

		template <class ClosureType>
		void new_task__(ClosureType function, bool sync_function, task_priority priority)
		{
			std::scoped_lock<std::mutex, std::shared_mutex> lock(thread_task_mutex_, thread_state_mutex_);

			task_queue_.emplace(
				std::move(std::make_shared<std::function<void()>>(function)),
				sync_function,
				static_cast<task_priority_t>(priority)
			);
			task_priority_ = task_queue_.top().priority_;

			if (threadpool_notify_new_tasks_)
			{
				++threadpool_new_tasks_;
				thread_state_condvar_.notify_one();
			}
		}

		void threadpool_run_task__(thread_t * this_thread)
		{
			std::scoped_lock<std::mutex> lock(this_thread->task_mutex_);

			if (this_thread->task_.function_)
			{
				++(this_thread->task_.sync_function_ ? thread_sync_running_ : thread_running_);

				this_thread->task_();

				--(this_thread->task_.sync_function_ ? thread_sync_running_ : thread_running_);
			}
		}
		void threadpool_sync_task__(thread_t * this_thread, std::shared_lock<std::shared_mutex> & sync_lock)
		{
			++threadpool_ready_sync_tasks_;

			while (thread_state_ != thread_state_t::terminating)
			{
				sync_lock.lock();

				while (thread_state_ != thread_state_t::terminating
					   && !threadpool_run_sync_tasks_)
				{
					++thread_sync_waiting_;

					thread_sync_condvar_.wait(sync_lock);

					--thread_sync_waiting_;
				}

				sync_lock.unlock();

				switch (thread_state_)
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

				switch (thread_state_)
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
		void threadpool__(thread_t * this_thread)
		{
			std::shared_lock<std::shared_mutex> state_lock(thread_state_mutex_, std::defer_lock);
			std::shared_lock<std::shared_mutex> sync_lock(thread_sync_mutex_, std::defer_lock);

			++thread_active_;

			while (thread_state_ != thread_state_t::terminating
				   && this_thread->thread_state_ != thread_state_t::waiting)
			{
				state_lock.lock();

				while (thread_state_ != thread_state_t::terminating
					   && this_thread->thread_state_ != thread_state_t::waiting
					   && ((!threadpool_new_tasks_ && !this_thread->task_.function_)
					   || (!threadpool_new_tasks_ && thread_state_ == thread_state_t::waiting)
					   || (task_priority_ <= this_thread->task_.priority_
					   && this_thread->task_.function_ && thread_state_ == thread_state_t::waiting)))
				{
					++thread_waiting_;

					thread_state_condvar_.wait(state_lock);

					--thread_waiting_;
				}

				state_lock.unlock();

				if (this_thread->thread_state_ == thread_state_t::running)
				{
					switch (thread_state_)
					{
						case thread_state_t::running:
						case thread_state_t::waiting:
							if (!this_thread->task_.function_)
							{
								if (threadpool_new_tasks_ > 0)
								{
									std::scoped_lock<std::mutex> task_lock(thread_task_mutex_);

									if (threadpool_new_tasks_ > 0)
									{
										--threadpool_new_tasks_;

										this_thread->task_ = task_queue_.top();
										task_queue_.pop();
										task_priority_ = (!task_queue_.empty() ? task_queue_.top().priority_ : 0u);

										break;
									}
								}
								continue;
							}
							else
							{
								if (threadpool_new_tasks_ > 0)
								{
									std::scoped_lock<std::mutex> task_lock(thread_task_mutex_);

									if (threadpool_new_tasks_ > 0
										&& task_priority_ > this_thread->task_.priority_)
									{
										task_queue_.push(this_thread->task_);
										this_thread->task_ = task_queue_.top();
										task_queue_.pop();
										task_priority_ = (!task_queue_.empty() ? task_queue_.top().priority_ : 0u);
									}
								}
							}
							break;
						case thread_state_t::terminating:
							continue;
					}

					switch (thread_state_)
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
			}

			--thread_active_;

			this_thread->thread_state_ = thread_state_t::terminating;
		}
	};
}

#endif
