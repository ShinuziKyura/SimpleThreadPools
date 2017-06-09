#ifndef SIMPLE_THREAD_POOLS_HPP_
#define SIMPLE_THREAD_POOLS_HPP_

#include <future>
#include <shared_mutex>
#include <functional>
#include <list>
#include <queue>

namespace stp
{
	enum class task_priority : uint32_t
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
		stopping = 1u,
		terminating = 2u
	};

	class stpi___ // Implementation class
	{
		template <class ParamType, typename = std::enable_if<std::is_lvalue_reference<ParamType>::value>::type>
		static constexpr auto value_wrapper(ParamType && arg) -> decltype(std::ref(arg))
		{
			return std::ref(arg);
		}
		template <class ParamType, typename = std::enable_if<std::is_rvalue_reference<ParamType>::value>::type>
		static constexpr auto value_wrapper(ParamType && arg) -> decltype(arg)
		{
			return arg;
		}
		template <class ParamType, typename = std::enable_if<!std::is_reference<ParamType>::value>::type>
		static constexpr auto value_wrapper(ParamType && arg) -> decltype(std::bind(std::move<ParamType &>, arg))
		{
			return std::bind(std::move<ParamType &>, arg);
		}

		// C++14 equivalents
		typedef std::shared_timed_mutex shared_mutex;

		template <class ... Mutex>
		class scoped_lock;

		template <class Mutex0>
		class scoped_lock<Mutex0>
		{
		public:
			scoped_lock<Mutex0>(Mutex0 & mutex_0) : mutex_0_(mutex_0)
			{
				mutex_0_.lock();
			}
			~scoped_lock<Mutex0>()
			{
				mutex_0_.unlock();
			}
		private:
			Mutex0 & mutex_0_;
		};

		template <class Mutex0, class Mutex1>
		class scoped_lock<Mutex0, Mutex1>
		{
		public:
			scoped_lock<Mutex0, Mutex1>(Mutex0 & mutex_0, Mutex1 & mutex_1) : mutex_0_(mutex_0), mutex_1_(mutex_1)
			{
				std::lock(mutex_0_, mutex_1_);
			}
			~scoped_lock<Mutex0, Mutex1>()
			{
				mutex_0_.unlock();
				mutex_1_.unlock();
			}
		private:
			Mutex0 & mutex_0_;
			Mutex1 & mutex_1_;
		};

		template <class RetType> friend class task;
		friend class threadpool;
	};

	template <class RetType>
	class task
	{
	public:
		bool ready()
		{
			if (!task_result_)
			{
				if (task_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				{
					return false;
				}
				task_result_ = std::make_unique<RetType>(task_future_.get());
			}
			return true;
		}
		void wait()
		{
			if (!task_result_)
			{
				task_future_.wait();
				task_result_ = std::make_unique<RetType>(task_future_.get());
			}
		}
		template <class Rep, class Period>
		std::future_status wait_for(std::chrono::duration<Rep, Period> const & timeout_duration)
		{
			if (!task_result_)
			{
				auto retval = task_future_.wait_for(timeout_duration);
				if (retval == std::future_status::ready)
				{
					task_result_ = std::make_unique<RetType>(task_future_.get());
				}
				return retval;
			}
			return std::future_status::ready;
		}
		template <class Clock, class Duration>
		std::future_status wait_until(std::chrono::time_point<Clock, Duration> const & timeout_time)
		{
			if (!task_result_)
			{
				auto retval = task_future_.wait_until(timeout_time);
				if (retval == std::future_status::ready)
				{
					task_result_ = std::make_unique<RetType>(task_future_.get());
				}
				return retval;
			}
			return std::future_status::ready;
		}
		RetType result()
		{
			if (!task_result_)
			{
				if (task_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				{
					throw std::runtime_error("Task not ready");
				}
				task_result_ = std::make_unique<RetType>(task_future_.get());
			}
			return *task_result_;
		}

		task<RetType>() = delete;
		task<RetType>(std::function<RetType()> & func) :
			task_package_(func),
			task_function_([this] { task_package_(); }),
			task_future_(task_package_.get_future())
		{
		}
		template <class ... RetParamType, class ... ParamType>
		task<RetType>(RetType(* func)(RetParamType ...), ParamType && ... args) :
			task_package_(std::bind(func, stpi___::value_wrapper(std::forward<ParamType>(args)) ...)),
			task_function_([this] { task_package_(); }),
			task_future_(task_package_.get_future())
		{
		}
		template <class ObjType, class ... RetParamType, class ... ParamType>
		task<RetType>(RetType(ObjType::* func)(RetParamType ...), ObjType * obj, ParamType && ... args) :
			task_package_(std::bind(func, obj, stpi___::value_wrapper(std::forward<ParamType>(args)) ...)),
			task_function_([this] { task_package_(); }),
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
			if (!task_result_)
			{
				task_function_();
				task_result_ = std::make_unique<RetType>(task_future_.get());
			}
			return *task_result_;
		}
	private:
		std::packaged_task<RetType()> task_package_;
		std::function<void()> task_function_;
		std::future<RetType> task_future_;
		std::unique_ptr<RetType> task_result_ = nullptr;

		friend class threadpool;
	};

	template <>
	class task<void>
	{
	public:
		bool ready()
		{
			if (!task_result_)
			{
				if (task_future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				{
					return false;
				}
				task_result_ = true;
			}
			return true;
		}
		void wait()
		{
			if (!task_result_)
			{
				task_future_.wait();
				task_result_ = true;
			}
		}
		template <class Rep, class Period>
		std::future_status wait_for(std::chrono::duration<Rep, Period> const & timeout_duration)
		{
			if (!task_result_)
			{
				auto retval = task_future_.wait_for(timeout_duration);
				if (retval == std::future_status::ready)
				{
					task_result_ = true;
				}
				return retval;
			}
			return std::future_status::ready;
		}
		template <class Clock, class Duration>
		std::future_status wait_until(std::chrono::time_point<Clock, Duration> const & timeout_time)
		{
			if (!task_result_)
			{
				auto retval = task_future_.wait_until(timeout_time);
				if (retval == std::future_status::ready)
				{
					task_result_ = true;
				}
				return retval;
			}
			return std::future_status::ready;
		}

		task() = delete;
		template <class RetType>
		task(std::function<RetType()> & func) :
			task_package_(func),
			task_function_([this] { task_package_(); }),
			task_future_(task_package_.get_future())
		{
		}
		template <class RetType, class ... RetParamType, class ... ParamType>
		task(RetType(* func)(RetParamType ...), ParamType && ... args) :
			task_package_(std::bind(func, stpi___::value_wrapper(std::forward<ParamType>(args)) ...)),
			task_function_([this] { task_package_(); }),
			task_future_(task_package_.get_future())
		{
		}
		template <class RetType, class ObjType, class ... RetParamType, class ... ParamType>
		task(RetType(ObjType::* func)(RetParamType ...), ObjType * obj, ParamType && ... args) :
			task_package_(std::bind(func, obj, stpi___::value_wrapper(std::forward<ParamType>(args)) ...)),
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
			if (!task_result_)
			{
				task_function_();
				task_result_ = true;
			}
		}
	private:
		std::packaged_task<void()> task_package_;
		std::function<void()> task_function_;
		std::future<void> task_future_;
		bool task_result_ = false;

		friend class threadpool;
	};

	class threadpool
	{
	public:
		void new_threads(size_t const amount)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto it = thread_array_.begin(), it_e = thread_array_.end();
				while (it != it_e)
				{
					if (it->thread_state_ == thread_state_t::terminating && !it->task_.function_)
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

				threadpool_size_ += amount;
			}
		}
		void delete_threads(size_t const amount)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				stpi___::scoped_lock<stpi___::shared_mutex> lock(thread_mutex_);

				if (threadpool_size_ < amount)
				{
					throw std::invalid_argument("Amount can't be greater than threadpool size");
				}

				auto it = thread_array_.begin(), it_b = thread_array_.begin(), it_e = thread_array_.end();
				for (size_t n = 0u; n < amount; ++it == it_e ? it = it_b : it)
				{
					if (it->thread_state_ != thread_state_t::terminating && !it->task_.function_)
					{
						it->thread_state_ = thread_state_t::terminating;
						++n;
					}
				}

				threadpool_size_ -= amount;
				thread_alert_.notify_all();
			}
		}
		template <class RetType>
		void new_task(task<RetType> & task, task_priority const priority = task_priority::normal)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				new_task__(&task.task_function_, false, false, priority);
			}
		}
		template <class RetType>
		void new_task(std::function<RetType()> & task, task_priority const priority = task_priority::normal)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task_function = new std::function<void()>([=] { task(); });

				new_task__(task_function_, false, true, priority);
			}
		}
		template <class RetType, class ... RetParamType, class ... ParamType>
		void new_task(RetType(* func)(RetParamType ...), ParamType && ... args)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(func, stpi___::value_wrapper(std::forward<ParamType>(args)) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				new_task__(task_function, false, true, task_priority::normal);
			}
		}
		template <class RetType, class ObjType, class ... RetParamType, class ... ParamType>
		void new_task(RetType(ObjType::* func)(RetParamType ...), ObjType * obj, ParamType && ... args)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(func, obj, stpi___::value_wrapper(std::forward<ParamType>(args)) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				new_task__(task_function, false, true, task_priority::normal);
			}
		}
		template <class RetType, class ... RetParamType, class ... ParamType>
		void new_task(RetType(* func)(RetParamType ...), task_priority const priority, ParamType && ... args)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(func, stpi___::value_wrapper(std::forward<ParamType>(args)) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				new_task__(task_function, false, true, priority);
			}
		}
		template <class RetType, class ObjType, class ... RetParamType, class ... ParamType>
		void new_task(RetType(ObjType::* func)(RetParamType ...), ObjType * obj, task_priority const priority, 
					  ParamType && ... args)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(func, obj, stpi___::value_wrapper(std::forward<ParamType>(args)) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				new_task__(task_function, false, true, priority);
			}
		}
		template <class RetType>
		void new_sync_task(task<RetType> & task, task_priority const priority = task_priority::normal)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				new_task__(&task.task_function_, true, false, priority);
			}
		}
		template <class RetType>
		void new_sync_task(std::function<RetType()> & task, task_priority const priority = task_priority::normal)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task_function = new std::function<void()>([=] { task(); });

				new_task__(task_function_, true, true, priority);
			}
		}
		template <class RetType, class ... RetParamType, class ... ParamType>
		void new_sync_task(RetType(* func)(RetParamType ...), ParamType && ... args)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(func, stpi___::value_wrapper(std::forward<ParamType>(args)) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				new_task__(task_function, true, true, task_priority::normal);
			}
		}
		template <class RetType, class ObjType, class ... RetParamType, class ... ParamType>
		void new_sync_task(RetType(ObjType::* func)(RetParamType ...), ObjType * obj, ParamType && ... args)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(func, obj, stpi___::value_wrapper(std::forward<ParamType>(args)) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				new_task__(task_function, true, true, task_priority::normal);
			}
		}
		template <class RetType, class ... RetParamType, class ... ParamType>
		void new_sync_task(RetType(* func)(RetParamType ...), task_priority const priority, ParamType && ... args)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(func, stpi___::value_wrapper(std::forward<ParamType>(args)) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				new_task__(task_function, true, true, priority);
			}
		}
		template <class RetType, class ObjType, class ... RetParamType, class ... ParamType>
		void new_sync_task(RetType(ObjType::* func)(RetParamType ...), ObjType * obj, task_priority const priority,
						   ParamType && ... args)
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				auto task = std::bind(func, obj, stpi___::value_wrapper(std::forward<ParamType>(args)) ...);
				auto task_function = new std::function<void()>([=] { task(); });

				new_task__(task_function, true, true, priority);
			}
		}
	private:
		void new_task__(std::function<void()> * function, bool const sync_function, bool const dynamic_function,
						task_priority const priority)
		{
			stpi___::scoped_lock<stpi___::shared_mutex> lock(thread_mutex_);

			task_queue_.emplace(function, sync_function, dynamic_function, static_cast<task_priority_t>(priority));
			task_priority_ = task_queue_.top().priority_;

			if (threadpool_notify_new_tasks_)
			{
				++threadpool_new_tasks_;
				thread_alert_.notify_one();
			}
		}
	public:
		void delete_tasks()
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				stpi___::scoped_lock<stpi___::shared_mutex, stpi___::shared_mutex> lock(thread_mutex_,
																						thread_sync_mutex_);
				while (!task_queue_.empty())
				{
					if (task_queue_.top().dynamic_function_)
					{
						delete task_queue_.top().function_;
					}
					task_queue_.pop();
				}

				for (auto & thread : thread_array_)
				{
					std::unique_lock<std::mutex> lock(thread.task_mutex_, std::try_to_lock);

					if (lock.owns_lock())
					{
						if (thread.task_.dynamic_function_)
						{
							delete thread.task_.function_;
						}
						thread.task_.function_ = nullptr;
						thread.task_.priority_ = 0u;
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
				stpi___::scoped_lock<stpi___::shared_mutex> lock(thread_mutex_);

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

				stpi___::scoped_lock<stpi___::shared_mutex> lock(thread_sync_mutex_);

				threadpool_run_sync_tasks_ += threadpool_ready_sync_tasks_;
				threadpool_ready_sync_tasks_ -= threadpool_run_sync_tasks_;
				thread_sync_alert_.notify_all();
			}
		}
		void run()
		{
			if (thread_state_ == thread_state_t::stopping)
			{
				stpi___::scoped_lock<stpi___::shared_mutex, stpi___::shared_mutex> lock(thread_mutex_,
																						thread_sync_mutex_);
				thread_state_ = thread_state_t::running;
				thread_alert_.notify_all();
			}
		}
		void stop()
		{
			if (thread_state_ == thread_state_t::running)
			{
				stpi___::scoped_lock<stpi___::shared_mutex, stpi___::shared_mutex> lock(thread_mutex_,
																						thread_sync_mutex_);
				thread_state_ = thread_state_t::stopping;
			}
		}
		void terminate()
		{
			if (thread_state_ != thread_state_t::terminating)
			{
				stpi___::scoped_lock<stpi___::shared_mutex, stpi___::shared_mutex> lock(thread_mutex_,
																						thread_sync_mutex_);
				thread_state_ = thread_state_t::terminating;
				threadpool_size_ = 0u;
				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}
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

		threadpool(size_t const size = std::thread::hardware_concurrency(),
				   threadpool_state const state = threadpool_state::running,
				   bool const notify = true) :
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
				stpi___::scoped_lock<stpi___::shared_mutex, stpi___::shared_mutex> lock(thread_mutex_,
																						thread_sync_mutex_);
				thread_state_ = thread_state_t::terminating;
			}

			while (thread_active_ > 0)
			{
				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();

				std::this_thread::yield();
			}

			for (auto & thread : thread_array_)
			{
				thread.thread_.join();
			}
		}
	private:
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

			thread_t(void(threadpool::* func)(thread_t *), threadpool * obj) :
				thread_state_(obj->thread_state_),
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

		std::atomic<size_t> thread_active_;
		std::atomic<size_t> thread_running_;
		std::atomic<size_t> thread_waiting_;
		std::atomic<size_t> thread_sync_running_;
		std::atomic<size_t> thread_sync_waiting_;

		stpi___::shared_mutex thread_mutex_;
		stpi___::shared_mutex thread_sync_mutex_;
		std::condition_variable_any thread_alert_;
		std::condition_variable_any thread_sync_alert_;

		void threadpool_run_task__(thread_t * const this_thread)
		{
			stpi___::scoped_lock<std::mutex> lock(this_thread->task_mutex_);

			if (this_thread->task_.function_)
			{
				++(this_thread->task_.sync_function_ ? thread_sync_running_ : thread_running_);

				this_thread->task_();

				--(this_thread->task_.sync_function_ ? thread_sync_running_ : thread_running_);
			}
		}
		void threadpool_sync_task__(thread_t * const this_thread, std::shared_lock<stpi___::shared_mutex> & sync_lock)
		{
			++threadpool_ready_sync_tasks_;

			while (this_thread->thread_state_ != thread_state_t::terminating)
			{
				sync_lock.lock();

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

				switch (this_thread->thread_state_)
				{
					case thread_state_t::running:
						if (threadpool_run_sync_tasks_ > 0)
						{
							--threadpool_run_sync_tasks_;
							break;
						}
					case thread_state_t::stopping:
						continue;
					case thread_state_t::terminating:
						return;
				}

				switch (this_thread->thread_state_)
				{
					case thread_state_t::running:
						if (this_thread->task_.function_)
						{
							threadpool_run_task__(this_thread);
						}
						return;
					case thread_state_t::stopping:
						++threadpool_ready_sync_tasks_;
						continue;
					case thread_state_t::terminating:
						return;
				}
			}
		}
		void threadpool__(thread_t * const this_thread)
		{
			std::shared_lock<stpi___::shared_mutex> lock(thread_mutex_, std::defer_lock);
			std::shared_lock<stpi___::shared_mutex> sync_lock(thread_sync_mutex_, std::defer_lock);

			++thread_active_;

			while (this_thread->thread_state_ != thread_state_t::terminating)
			{
				lock.lock();

				while (thread_state_ != thread_state_t::terminating
					   && this_thread->thread_state_ != thread_state_t::terminating
					   && ((!threadpool_new_tasks_ && !this_thread->task_.function_)
					   || (!threadpool_new_tasks_ && thread_state_ == thread_state_t::stopping)
					   || (task_priority_ <= this_thread->task_.priority_
					   && this_thread->task_.function_ && thread_state_ == thread_state_t::stopping)))
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

				switch (this_thread->thread_state_)
				{
					case thread_state_t::running:
					case thread_state_t::stopping:
						if (!this_thread->task_.function_)
						{
							if (threadpool_new_tasks_ > 0)
							{
								stpi___::scoped_lock<stpi___::shared_mutex> lock(thread_mutex_);

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
								stpi___::scoped_lock<stpi___::shared_mutex> lock(thread_mutex_);

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

				switch (this_thread->thread_state_)
				{
					case thread_state_t::running:
						if (this_thread->task_.function_ && !this_thread->task_.sync_function_)
						{
							threadpool_run_task__(this_thread);
							break;
						}
					case thread_state_t::stopping:
						if (this_thread->task_.function_ && this_thread->task_.sync_function_)
						{
							threadpool_sync_task__(this_thread, sync_lock);
						}
					case thread_state_t::terminating:
						break;
				}
			}

			--thread_active_;
		}
	};
}

#endif
