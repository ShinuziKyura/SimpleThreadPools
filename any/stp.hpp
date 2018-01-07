#ifndef SIMPLE_THREAD_POOLS_HPP
#define SIMPLE_THREAD_POOLS_HPP

#include <any>
#include <list>
#include <queue>
#include <functional>
#include <future>
#include <shared_mutex>

// SimpleThreadPools - version B.3.7.0 - Only allocates big objects inside stp::tasks dynamically
namespace stp
{
	enum class task_error_code
	{
		no_state = 1,
		invalid_state,
		state_loss_would_occur,
		thread_deadlock_would_occur
	};

	class task_error : public std::logic_error
	{
	public:
		task_error(task_error_code code) : logic_error(_task_error_code_to_string(code))
		{
		}
	private:
		static std::string _task_error_code_to_string(task_error_code code)
		{
			switch (code)
			{
				case task_error_code::no_state:
					return std::string("no state");
				case task_error_code::invalid_state:
					return std::string("invalid state");
				case task_error_code::state_loss_would_occur:
					return std::string("state loss would occur");
				case task_error_code::thread_deadlock_would_occur:
					return std::string("thread deadlock would occur");
			}
			return std::string();
		}
	};

	enum class task_priority : uint_fast8_t
	{
		maximum				= 5,
		high				= 4,
		normal				= 3,
		low					= 2,
		minimum				= 1
	};

	inline task_priority default_priority{ task_priority::normal };

	enum class task_state
	{
		ready,
		running,
		waiting,
		suspended,
		null
	};

	template <class RetType, class ... ParamTypes>
	class task
	{
	public:
		using type = task<RetType>;
		using value_type = RetType;

		std::add_lvalue_reference_t<RetType> get()
		{
			wait();

			if (_task_result.type() == typeid(_exception_ptr))
			{
				_task_state.store(task_state::null, std::memory_order_relaxed);

				std::rethrow_exception(std::any_cast<_exception_ptr>(_task_result).exception_ptr);
			}

			if constexpr (std::negation_v<std::is_same<RetType, void>>)
			{
				return std::any_cast<std::add_lvalue_reference_t<RetType>>(_task_result);
			}
		}
		void wait()
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
					throw task_error(task_error_code::thread_deadlock_would_occur);
				default:
					break;
			}

			if (_task_future.valid())
			{
				try
				{
					if constexpr (std::is_same_v<RetType, void>)
					{
						_task_future.get();
					}
					else
					{
						_task_result = std::make_any<RetType>(std::move(_task_future.get()));
					}
				}
				catch (...)
				{
					_task_result = std::make_any<_exception_ptr>(std::current_exception());
				}
			}
		}
		template <class Rep, class Period>
		task_state wait_for(std::chrono::duration<Rep, Period> const & timeout_duration)
		{
			return wait_until(std::chrono::steady_clock::now() + timeout_duration);
		}
		template <class Clock, class Duration>
		task_state wait_until(std::chrono::time_point<Clock, Duration> const & timeout_time)
		{
			auto state = _task_state.load(std::memory_order_acquire);

			switch (state)
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				default:
					break;
			}

			if (state == task_state::waiting || state == task_state::running)
			{
				_task_future.wait_until(timeout_time);
			}

			return _task_state.load(std::memory_order_acquire);
		}
		task_state state() const
		{
			return _task_state.load(std::memory_order_acquire);
		}
		void reset()
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::waiting:
				case task_state::running:
					throw task_error(task_error_code::state_loss_would_occur);
				default:
					break;
			}

			if (_task_object.valid())
			{
				_task_object.reset();
				_task_future = _task_object.get_future();
				_task_result.reset();
				_task_state.store(task_state::suspended, std::memory_order_relaxed);
			}
		}

		task<RetType, ParamTypes ...>() :
			_task_state(task_state::null)
		{
		}
		template <class ... AutoParamTypes, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(* func)(AutoParamTypes ...), ArgTypes && ... args) :
			_task_object(std::bind(func, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_object.get_future())
		{
		}
		template <class ObjType, class ... AutoParamTypes, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(AutoParamTypes ...), ObjType * obj, ArgTypes && ... args) :
			_task_object(std::bind(func, obj, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_object.get_future())
		{
		}
		template <class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(* func)(ParamTypes ...), ArgTypes && ... args) :
			_task_object(std::bind(func, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_object.get_future())
		{
		}
		template <class ObjType, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(ParamTypes ...), ObjType * obj, ArgTypes && ... args) :
			_task_object(std::bind(func, obj, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_object.get_future())
		{
		}
		task<RetType, ParamTypes ...>(task<RetType, ParamTypes ...> const &) = delete;
		task<RetType, ParamTypes ...> & operator=(task<RetType, ParamTypes ...> const &) = delete;
		template <class ... MoveParamTypes>
		task<RetType, ParamTypes ...>(task<RetType, MoveParamTypes ...> && that) // Move pseudo-constructor
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::waiting:
				case task_state::running:
					throw task_error(task_error_code::state_loss_would_occur);
				default:
					break;
			}

			_task_object = std::move(that._task_object);
			_task_future = std::move(that._task_future);
			_task_result = std::move(that._task_result);
			_task_state.store(that._task_state.exchange(task_state::null,
														std::memory_order_relaxed),
							  std::memory_order_relaxed);
		}
		template <class ... MoveParamTypes>
		task<RetType, ParamTypes ...> & operator=(task<RetType, MoveParamTypes ...> && that) // Move assignment pseudo-operator
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::waiting:
				case task_state::running:
					throw task_error(task_error_code::state_loss_would_occur);
				default:
					break;
			}

			_task_object = std::move(that._task_object);
			_task_future = std::move(that._task_future);
			_task_result = std::move(that._task_result);
			_task_state.store(that._task_state.exchange(task_state::null,
														std::memory_order_relaxed),
							  std::memory_order_relaxed);

			return *this;
		}
		~task<RetType, ParamTypes ...>() = default;

		void operator()()
		{
			switch (_task_state.load(std::memory_order_relaxed))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::waiting:
				case task_state::running:
				case task_state::ready:
					throw task_error(task_error_code::invalid_state);
				default:
					break;
			}

			_task_function(task_state::running);
		}
	private:
		static_assert(std::negation_v<std::is_same<RetType, std::any>>, "RetType may not be std::any");

		struct _exception_ptr
		{
			std::exception_ptr exception_ptr;

			_exception_ptr(std::exception_ptr exception_ptr) :
				exception_ptr(exception_ptr)
			{
			}
		};

		std::packaged_task<RetType()> _task_object;
		std::future<RetType> _task_future;
		std::any _task_result;
		std::atomic<task_state> _task_state{ task_state::suspended };

		void _task_function(task_state state)
		{
			_task_state.store(state, std::memory_order_release);

			if (state == task_state::running)
			{
				_task_object();
				_task_state.store(task_state::ready, std::memory_order_release);
			}
		}

		template <class ArgType>
		static auto _bind_forward(std::remove_reference_t<ArgType> & arg)
		{
			if constexpr (std::is_lvalue_reference_v<ArgType>)
			{
				return std::ref(arg);
			}
			else
			{
				return std::bind(std::move<ArgType &>, std::ref(arg));
			}
		}

		template <class MoveRetType, class ... MoveParamTypes> friend class task; // Required by pseudo-move constructor and pseudo-move assignment operator
		friend class threadpool;
	};

	template <class RetType, class ... ParamTypes, class ... ArgTypes>
	inline task<RetType> make_task(RetType(* func)(ParamTypes ...), ArgTypes && ... args)
	{
		return task<RetType>(func, std::forward<ArgTypes>(args) ...);
	}
	template <class RetType, class ... ParamTypes, class ObjType, class ... ArgTypes>
	inline task<RetType> make_task(RetType(ObjType::* func)(ParamTypes ...), ObjType * obj, ArgTypes && ... args)
	{
		return task<RetType>(func, obj, std::forward<ArgTypes>(args) ...);
	}

	enum class threadpool_state
	{
		running,
		stopped,
		terminating
	};

	class threadpool
	{
	public:
		template <class RetType, class ... ParamTypes>
		void push_task(task<RetType, ParamTypes ...> & task, task_priority priority = default_priority)
		{
			switch (task._task_state.load(std::memory_order_relaxed))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::waiting:
				case task_state::running:
				case task_state::ready:
					throw task_error(task_error_code::invalid_state);
				default:
					break;
			}

			std::scoped_lock<std::mutex> lock(_threadpool_task_mutex);

			_threadpool_task_queue.emplace(std::bind(&stp::task<RetType, ParamTypes ...>::_task_function,
													 &task,
													 std::placeholders::_1),
										   static_cast<uint_fast8_t>(priority));

			_threadpool_task_priority.store(_threadpool_task_queue.top().priority, std::memory_order_release);

			_threadpool_condvar.notify_one();
		}
		void pop_tasks()
		{
			if (_threadpool_task_priority.load(std::memory_order_acquire))
			{
				std::scoped_lock<std::mutex, std::shared_mutex> lock(_threadpool_task_mutex, _threadpool_mutex);

				while (!_threadpool_task_queue.empty())
				{
					_threadpool_task_queue.top().function(task_state::suspended);
					_threadpool_task_queue.pop();
				}

				_threadpool_task_priority.store(0, std::memory_order_release);
			}
		}
		void run()
		{
			if (_threadpool_state == threadpool_state::stopped)
			{
				std::scoped_lock<std::shared_mutex> lock(_threadpool_mutex);

				_threadpool_state = threadpool_state::running;

				_threadpool_condvar.notify_all();
			}
		}
		void stop()
		{
			if (_threadpool_state == threadpool_state::running)
			{
				std::scoped_lock<std::shared_mutex> lock(_threadpool_mutex);

				_threadpool_state = threadpool_state::stopped;
			}
		}
		void resize(size_t new_size)
		{
			if (_threadpool_size != new_size)
			{
				std::scoped_lock<std::shared_mutex> lock(_threadpool_mutex);

				uintmax_t size_diff = std::abs(static_cast<intmax_t>(_threadpool_size) 
											   - static_cast<intmax_t>(new_size));

				if (_threadpool_size < new_size)
				{
					auto it = _threadpool_thread_list.begin(), it_e = _threadpool_thread_list.end();
					for (uintmax_t n = 0; n < size_diff; ++it)
					{
						if (it != it_e)
						{
							if (!it->active)
							{
								it->task = {};
								it->active = true;
								it->thread = std::thread(&threadpool::_threadpool_pool, this, &*it);

								++n;
							}
						}
						else
						{
							while (n++ < size_diff)
							{
								_threadpool_thread_list.emplace_back(this);
							}
						}
					}
				}
				else
				{
					auto it_b = _threadpool_thread_list.begin(), it_e = _threadpool_thread_list.end(), it = it_b;
					for (uintmax_t n = 0; n < size_diff; ++it == it_e ? it = it_b, void() : void())
					{
						if (it->active)
						{
							it->active = false;

							++n;
						}
					}

					_threadpool_condvar.notify_all();
				}

				_threadpool_size = new_size;
			}
		}
		size_t size() const
		{
			return _threadpool_size;
		}
		threadpool_state state() const
		{
			return _threadpool_state;
		}

		threadpool(size_t size = std::thread::hardware_concurrency(), threadpool_state state = threadpool_state::running) :
			_threadpool_size(size),
			_threadpool_state(state)
		{
			for (size_t n = 0; n < size; ++n)
			{
				_threadpool_thread_list.emplace_back(this);
			}
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			{
				std::scoped_lock<std::shared_mutex> lock(_threadpool_mutex);

				_threadpool_state = threadpool_state::terminating;

				_threadpool_condvar.notify_all();
			}

			for (auto & thread : _threadpool_thread_list)
			{
				if (thread.active)
				{
					thread.thread.join();
				}
			}
		}
	private:
		struct _task
		{
			std::function<void(task_state)> function;
			uint_fast8_t priority;
			std::chrono::steady_clock::time_point origin{ std::chrono::steady_clock::now() };

			_task(std::function<void(task_state)> function = nullptr, uint_fast8_t priority = 0) :
				function(function),
				priority(priority)
			{
				if (function)
				{
					function(task_state::waiting);
				}
			}

			bool operator<(_task const & that) const
			{
				return priority != that.priority ? priority < that.priority : origin > that.origin;
			}
		};

		struct _thread
		{
			_task task;
			bool active{ true };
			std::thread thread; // Must be the last variable to be initialized

			_thread(threadpool * threadpool) :
				thread(&threadpool::_threadpool_pool, threadpool, this)
			{
			}

			bool operator==(_thread const & that) const
			{
				return thread.get_id() == that.thread.get_id();
			}
		};

		size_t _threadpool_size;
		threadpool_state _threadpool_state;
		std::list<_thread> _threadpool_thread_list;
		std::priority_queue<_task, std::deque<_task>> _threadpool_task_queue;
		std::atomic<uint_fast8_t> _threadpool_task_priority{ 0 };
		std::mutex _threadpool_task_mutex;
		std::shared_mutex _threadpool_mutex;
		std::condition_variable_any _threadpool_condvar;

		void _threadpool_pool(_thread * this_thread)
		{
			std::shared_lock<std::shared_mutex> threadpool_lock(_threadpool_mutex);

			threadpool_state state = _threadpool_state;
			uint_fast8_t priority = 0;

			while (state != threadpool_state::terminating)
			{
				priority = _threadpool_task_priority.load(std::memory_order_acquire);

				while (state != threadpool_state::terminating && this_thread->active
					   && ((!priority
							? !this_thread->task.function || state == threadpool_state::stopped
							: false)
						   || (priority <= this_thread->task.priority
							   && this_thread->task.function
							   && state == threadpool_state::stopped)))
				{
					_threadpool_condvar.wait(threadpool_lock);

					state = _threadpool_state;
					priority = _threadpool_task_priority.load(std::memory_order_acquire);
				}

				if (!this_thread->active)
				{
					if (this_thread->task.function)
					{
						std::scoped_lock<std::mutex> lock(_threadpool_task_mutex);

						_threadpool_task_queue.push(this_thread->task);

						_threadpool_task_priority.store(_threadpool_task_queue.top().priority,
														std::memory_order_release);
					}

					this_thread->thread.detach();

					return;
				}

				threadpool_lock.unlock();

				switch (state)
				{
					case threadpool_state::running:
					case threadpool_state::stopped:
						if (!this_thread->task.function || priority > this_thread->task.priority)
						{
							std::scoped_lock<std::mutex> lock(_threadpool_task_mutex);

							priority = _threadpool_task_priority.load(std::memory_order_relaxed);

							if (!this_thread->task.function)
							{
								if (priority)
								{
									this_thread->task = _threadpool_task_queue.top();
									_threadpool_task_queue.pop();

									_threadpool_task_priority.store(!_threadpool_task_queue.empty()
																	? _threadpool_task_queue.top().priority
																	: 0,
																	std::memory_order_release);
								}
							}
							else if (priority > this_thread->task.priority)
							{
								_threadpool_task_queue.push(this_thread->task);

								this_thread->task = _threadpool_task_queue.top();
								_threadpool_task_queue.pop();

								_threadpool_task_priority.store(_threadpool_task_queue.top().priority,
																std::memory_order_release);
							}
						}
					case threadpool_state::terminating:
						break;
				}

				switch (state)
				{
					case threadpool_state::running:
						if (this_thread->task.function)
						{
							this_thread->task.function(task_state::running);
							this_thread->task.function = nullptr;
							this_thread->task.priority = 0;
						}
					case threadpool_state::stopped:
					case threadpool_state::terminating:
						break;
				}

				threadpool_lock.lock();

				state = _threadpool_state;
			}
		}
	};
}

#endif
