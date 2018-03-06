#ifndef SIMPLE_THREAD_POOLS_HPP
#define SIMPLE_THREAD_POOLS_HPP

#include <list>
#include <queue>
#include <unordered_set>
#include <functional>
#include <future>
#include <shared_mutex>

// SimpleThreadPools - version B.3.10.1 - Allocates objects dynamically inside stp::task
namespace stp
{
	enum class task_error_code : uint_fast8_t
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
		static char const * _task_error_code_to_string(task_error_code code)
		{
			switch (code)
			{
				case task_error_code::no_state:
					return "no state";
				case task_error_code::invalid_state:
					return "invalid state";
				case task_error_code::state_loss_would_occur:
					return "state loss would occur";
				case task_error_code::thread_deadlock_would_occur:
					return "thread deadlock would occur";
			}

			return "";
		}
	};

	enum class task_state : uint_fast8_t
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
		static_assert(!std::is_rvalue_reference<RetType>::value, "stp::task<T>: T may not be of rvalue-reference type");
		using ResultType = std::conditional_t<!std::is_reference<RetType>::value, RetType, std::remove_reference_t<RetType> *>;
	public:
		task<RetType, ParamTypes ...>() = default;
		template <class ... AutoParamTypes, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(* func)(AutoParamTypes ...),
									  ArgTypes && ... args) :
			_task_package(std::bind(func, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_package.get_future()),
			_task_state(task_state::suspended)
		{
		}
		template <class ObjType, class ... AutoParamTypes, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(AutoParamTypes ...),
									  ObjType * obj,
									  ArgTypes && ... args) :
			_task_package(std::bind(func, obj, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_package.get_future()),
			_task_state(task_state::suspended)
		{
		}
		template <class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(* func)(ParamTypes ...),
									  ArgTypes && ... args) :
			_task_package(std::bind(func, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_package.get_future()),
			_task_state(task_state::suspended)
		{
		}
		template <class ObjType, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(ParamTypes ...),
									  ObjType * obj,
									  ArgTypes && ... args) :
			_task_package(std::bind(func, obj, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_package.get_future()),
			_task_state(task_state::suspended)
		{
		}
		template <class ... MoveParamTypes>
		task<RetType, ParamTypes ...>(task<RetType, MoveParamTypes ...> && other) // Move pseudo-constructor
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::waiting:
				case task_state::running:
					throw task_error(task_error_code::state_loss_would_occur);
				default:
					break;
			}

			_task_package = std::move(other._task_package);
			_task_future = std::move(other._task_future);
			_task_result = std::move(other._task_result);
			_task_exception = std::move(other._task_exception);
			_task_state.store(other._task_state.exchange(task_state::null, std::memory_order_relaxed),
							  std::memory_order_relaxed);
		}
		template <class ... MoveParamTypes>
		task<RetType, ParamTypes ...> & operator=(task<RetType, MoveParamTypes ...> && other) // Move assignment pseudo-operator
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::waiting:
				case task_state::running:
					throw task_error(task_error_code::state_loss_would_occur);
				default:
					break;
			}

			_task_package = std::exchange(other._task_package, std::packaged_task<RetType()>());
			_task_future = std::exchange(other._task_future, std::future<RetType>());
			_task_result = std::exchange(other._task_result, std::shared_ptr<ResultType>());
			_task_exception = std::exchange(other._task_exception, std::exception_ptr());
			_task_state.store(other._task_state.exchange(task_state::null, std::memory_order_relaxed),
							  std::memory_order_relaxed);

			return *this;
		}
		~task<RetType, ParamTypes ...>()
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::waiting:
				case task_state::running:
					_task_future.wait();
				default:
					break;
			}
		}

		std::add_lvalue_reference_t<RetType> get()
		{
			wait();

			if (_task_exception)
			{
				std::rethrow_exception(_task_exception);
			}

			return _if_constexpr_1_get(std::integral_constant<bool, !std::is_same<RetType, void>::value>());
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
					_if_constexpr_1_wait(std::integral_constant<bool, !std::is_same<RetType, void>::value>());
				}
				catch (...)
				{
					_task_exception = std::current_exception();
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
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::waiting:
				case task_state::running:
					_task_future.wait_until(timeout_time);
				default:
					break;
			}

			return _task_state.load(std::memory_order_acquire);
		}
		task_state state() const
		{
			return _task_state.load(std::memory_order_acquire);
		}
		bool ready() const
		{
			return _task_state.load(std::memory_order_acquire) == task_state::ready;
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

			if (_task_package.valid())
			{
				_task_package.reset();
				_task_future = _task_package.get_future();
				_task_result.reset();
				_task_exception = nullptr;
				_task_state.store(task_state::suspended, std::memory_order_relaxed);
			}
		}

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
		void _task_function(task_state state)
		{
			_task_state.store(state, std::memory_order_release);

			if (state == task_state::running)
			{
				_task_package();

				_task_state.store(task_state::ready, std::memory_order_release);
			}
		}
		std::add_lvalue_reference_t<RetType> _if_constexpr_1_get(std::true_type)
		{
			return _if_constexpr_2_get(std::integral_constant<bool, !std::is_reference<RetType>::value>());
		}
		void _if_constexpr_1_get(std::false_type)
		{
			return;
		}
		std::add_lvalue_reference_t<RetType> _if_constexpr_2_get(std::true_type)
		{
			return *_task_result;
		}
		std::add_lvalue_reference_t<RetType> _if_constexpr_2_get(std::false_type)
		{
			return *(*_task_result);
		}
		void _if_constexpr_1_wait(std::true_type)
		{
			_if_constexpr_2_wait(std::integral_constant<bool, !std::is_reference<RetType>::value>());
		}
		void _if_constexpr_1_wait(std::false_type)
		{
			_task_future.get();
		}
		void _if_constexpr_2_wait(std::true_type)
		{
			_task_result = std::make_unique<RetType>(_task_future.get());
		}
		void _if_constexpr_2_wait(std::false_type)
		{
			_task_result = std::make_unique<std::remove_reference_t<RetType> *>(&_task_future.get());
		}

		template <class ArgType>
		static auto _bind_forward(ArgType && arg)
		{
			return _if_constexpr_bind_forward<ArgType>(std::is_lvalue_reference<ArgType>(), std::forward<ArgType>(arg));
		}
		template <class ArgType>
		static auto _if_constexpr_bind_forward(std::true_type, std::remove_reference_t<ArgType> & arg)
		{
			return std::ref(arg);
		}
		template <class ArgType>
		static auto _if_constexpr_bind_forward(std::false_type, std::remove_reference_t<ArgType> & arg)
		{
			return std::bind(std::move<ArgType &>, std::ref(arg));
		}

		std::packaged_task<RetType()> _task_package;
		std::future<RetType> _task_future;
		std::shared_ptr<ResultType> _task_result;
		std::exception_ptr _task_exception;
		std::atomic<task_state> _task_state{ task_state::null };

		template <class MoveRetType, class ... MoveParamTypes>
		friend class task;
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

	enum class threadpool_state : uint_fast8_t
	{
		running,
		stopped,
		terminating
	};

	class threadpool
	{
	public:
		threadpool(size_t size = std::thread::hardware_concurrency(),
				   threadpool_state state = threadpool_state::running) :
			_threadpool_minimum_priority(std::numeric_limits<int_fast8_t>::min()),
			_threadpool_maximum_priority(std::numeric_limits<int_fast8_t>::max()),
			_threadpool_default_priority(0),
			_threadpool_size(size),
			_threadpool_state(state)
		{
			for (size_t n = 0; n < size; ++n)
			{
				_threadpool_thread_list.emplace_back(this);
			}
		}
		threadpool(int_fast8_t minimum_priority,
				   int_fast8_t maximum_priority,
				   int_fast8_t default_priority,
				   size_t size = std::thread::hardware_concurrency(),
				   threadpool_state state = threadpool_state::running) :
			_threadpool_minimum_priority(minimum_priority),
			_threadpool_maximum_priority(maximum_priority),
			_threadpool_default_priority(_clamp(default_priority,
												std::min(minimum_priority, maximum_priority),
												std::max(minimum_priority, maximum_priority))),
			_threadpool_size(size),
			_threadpool_state(state)
		{
			for (size_t n = 0; n < size; ++n)
			{
				_threadpool_thread_list.emplace_back(this);
			}
		}
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			_threadpool_mutex.lock();

			_threadpool_state = threadpool_state::terminating;

			_threadpool_condvar.notify_all();

			_threadpool_mutex.unlock();

			for (auto & thread : _threadpool_thread_list)
			{
				if (thread.active)
				{
					thread.thread.join();
				}
			}
		}

		template <class RetType, class ... ParamTypes>
		void push(task<RetType, ParamTypes ...> & task)
		{
			push(task, _threadpool_default_priority);
		}
		template <class RetType, class ... ParamTypes>
		void push(task<RetType, ParamTypes ...> & task, int_fast8_t priority)
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

			task._task_function(task_state::waiting);

			priority = _clamp(priority,
							  std::min(_threadpool_minimum_priority, _threadpool_maximum_priority),
							  std::max(_threadpool_minimum_priority, _threadpool_maximum_priority));

			std::lock_guard<std::mutex> lock(_threadpool_task_mutex);

			_threadpool_task_queue.emplace(std::bind(&stp::task<RetType, ParamTypes ...>::_task_function,
													 &task,
													 std::placeholders::_1),
										   &task,
										   uint_fast8_t(std::abs(_threadpool_minimum_priority - priority)));

			_threadpool_task_set.insert(&task);

			_threadpool_task.store(true, std::memory_order_release);

			_threadpool_condvar.notify_one();
		}
		template <class RetType, class ... ParamTypes>
		bool pop(task<RetType, ParamTypes ...> & task)
		{
			std::lock_guard<std::mutex> lock(_threadpool_task_mutex);

			return _threadpool_task_set.erase(&task);
		}
		void resize(size_t size)
		{
			if (_threadpool_size != size)
			{
				std::lock_guard<std::shared_timed_mutex> lock(_threadpool_mutex);
				size_t delta_size = std::max(_threadpool_size, size) - std::min(_threadpool_size, size);

				if (_threadpool_size < size)
				{
					auto it = std::begin(_threadpool_thread_list), it_e = std::end(_threadpool_thread_list);
					for (size_t n = 0; n < delta_size; ++it)
					{
						if (it != it_e)
						{
							if (it->inactive)
							{
								it->active = true;
								it->inactive = false;
								it->thread = std::thread(&threadpool::_threadpool_function, this, &*it);

								++n;
							}
						}
						else
						{
							while (n < delta_size)
							{
								_threadpool_thread_list.emplace_back(this);

								++n;
							}
						}
					}
				}
				else
				{
					auto it_b = std::begin(_threadpool_thread_list), it_e = std::end(_threadpool_thread_list), it = it_b;
					for (size_t n = 0; n < delta_size; ++it)
					{
						if ((it != it_e ? it : it = it_b)->active)
						{
							it->active = false;

							++n;
						}
					}

					_threadpool_condvar.notify_all();
				}

				_threadpool_size = size;
			}
		}
		void run()
		{
			if (_threadpool_state == threadpool_state::stopped)
			{
				std::lock_guard<std::shared_timed_mutex> lock(_threadpool_mutex);

				_threadpool_state = threadpool_state::running;

				_threadpool_condvar.notify_all();
			}
		}
		void stop()
		{
			if (_threadpool_state == threadpool_state::running)
			{
				std::lock_guard<std::shared_timed_mutex> lock(_threadpool_mutex);

				_threadpool_state = threadpool_state::stopped;
			}
		}
		void reset_priority(int_fast8_t default_priority)
		{
			_threadpool_default_priority = _clamp(default_priority,
												  std::min(_threadpool_minimum_priority, _threadpool_maximum_priority),
												  std::max(_threadpool_minimum_priority, _threadpool_maximum_priority));
		}
		size_t size() const
		{
			return _threadpool_size;
		}
		threadpool_state state() const
		{
			return _threadpool_state;
		}
		int_fast8_t minimum_priority() const
		{
			return _threadpool_minimum_priority;
		}
		int_fast8_t maximum_priority() const
		{
			return _threadpool_maximum_priority;
		}
		int_fast8_t default_priority() const
		{
			return _threadpool_default_priority;
		}
	private:
		struct _task
		{
			_task(std::function<void(task_state)> func = nullptr, void * iden = nullptr, uint_fast8_t prio = 0) :
				function(func),
				identity(iden),
				priority(prio)
			{
			}

			bool operator<(_task const & other) const
			{
				return priority != other.priority ? priority < other.priority : origin > other.origin;
			}

			std::function<void(task_state)> function;
			void * identity;
			uint_fast8_t priority;
			std::chrono::steady_clock::time_point origin{ std::chrono::steady_clock::now() };
		};
		struct _thread
		{
			_thread(threadpool * threadpool) :
				thread(&threadpool::_threadpool_function, threadpool, this)
			{
			}

			bool operator==(_thread const & other) const
			{
				return thread.get_id() == other.thread.get_id();
			}

			_task task;
			bool active{ true };
			bool inactive{ false };
			std::thread thread; // Must be the last variable to be initialized
		};

		void _threadpool_function(_thread * this_thread)
		{
			std::shared_lock<std::shared_timed_mutex> threadpool_lock(_threadpool_mutex);
			bool valid = true;

			while (_threadpool_state != threadpool_state::terminating && this_thread->active)
			{
				if (_threadpool_state == threadpool_state::running && _threadpool_task.load(std::memory_order_acquire))
				{
					threadpool_lock.unlock();

					{
						std::lock_guard<std::mutex> lock(_threadpool_task_mutex);

						if (_threadpool_task.load(std::memory_order_relaxed))
						{
							this_thread->task = std::move(_threadpool_task_queue.top());

							valid = _threadpool_task_set.erase(this_thread->task.identity);

							_threadpool_task_queue.pop();

							if (_threadpool_task_queue.empty())
							{
								_threadpool_task.store(false, std::memory_order_release);
							}
						}
					}

					if (this_thread->task.function)
					{
						this_thread->task.function(valid ? task_state::running : task_state::suspended);

						this_thread->task.function = nullptr;
					}

					threadpool_lock.lock();
				}
				else
				{
					_threadpool_condvar.wait(threadpool_lock);
				}
			}

			if (bool(this_thread->inactive = !this_thread->active))
			{
				this_thread->thread.detach();
			}
		}

		static int_fast8_t const & _clamp(int_fast8_t const & value, int_fast8_t const & low, int_fast8_t const & high)
		{
			return value < low ? low : value > high ? high : value;
		}

		int_fast8_t const _threadpool_minimum_priority;
		int_fast8_t const _threadpool_maximum_priority;
		int_fast8_t _threadpool_default_priority;
		size_t _threadpool_size;
		threadpool_state _threadpool_state;
		std::atomic<bool> _threadpool_task{ false };
		std::priority_queue<_task, std::deque<_task>> _threadpool_task_queue;
		std::unordered_set<void *> _threadpool_task_set;
		std::list<_thread> _threadpool_thread_list;
		std::mutex _threadpool_task_mutex;
		std::shared_timed_mutex _threadpool_mutex;
		std::condition_variable_any _threadpool_condvar;
	};
}

#endif
