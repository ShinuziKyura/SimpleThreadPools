#ifndef SIMPLE_THREAD_POOLS_HPP
#define SIMPLE_THREAD_POOLS_HPP

#include <any>
#include <list>
#include <queue>
#include <unordered_set>
#include <functional>
#include <future>
#include <shared_mutex>

// SimpleThreadPools - version B.3.9.2 - Only allocates big objects inside stp::task objects dynamically
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
		static_assert(std::negation_v<std::is_same<RetType, std::any>>, "stp::task<T>: T may not be of type std::any");
		static_assert(std::negation_v<std::is_rvalue_reference<RetType>>, "stp::task<T>: T may not be of rvalue-reference type");
	public:
		task<RetType, ParamTypes ...>() = default;
		template <class ... AutoParamTypes, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(* func)(AutoParamTypes ...), ArgTypes && ... args) :
			_task_package(std::bind(func, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_package.get_future()),
			_task_state(task_state::suspended)
		{
		}
		template <class ObjType, class ... AutoParamTypes, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(AutoParamTypes ...), ObjType * obj, ArgTypes && ... args) :
			_task_package(std::bind(func, obj, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_package.get_future()),
			_task_state(task_state::suspended)
		{
		}
		template <class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(* func)(ParamTypes ...), ArgTypes && ... args) :
			_task_package(std::bind(func, _bind_forward<ArgTypes>(args) ...)),
			_task_future(_task_package.get_future()),
			_task_state(task_state::suspended)
		{
		}
		template <class ObjType, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(ParamTypes ...), ObjType * obj, ArgTypes && ... args) :
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
			_task_result = std::exchange(other._task_result, std::any());
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

			if (_task_result.type() == typeid(_exception_ptr))
			{
				std::rethrow_exception(std::any_cast<_exception_ptr>(_task_result).exception);
			}

			if constexpr (std::negation_v<std::is_same<RetType, void>>)
			{
				if constexpr (std::negation_v<std::is_reference<RetType>>)
				{
					return std::any_cast<std::add_lvalue_reference_t<RetType>>(_task_result);
				}
				else
				{
					return *std::any_cast<std::remove_reference_t<RetType> *>(_task_result);
				}
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
					if constexpr (std::negation_v<std::is_same<RetType, void>>)
					{
						if constexpr (std::negation_v<std::is_reference<RetType>>)
						{
							_task_result = std::make_any<RetType>(_task_future.get());
						}
						else
						{
							_task_result = std::make_any<std::remove_reference_t<RetType> *>(&_task_future.get());
						}
					}
					else
					{
						_task_future.get();
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
		struct _exception_ptr
		{
			_exception_ptr(std::exception_ptr excep) :
				exception(excep)
			{
			}

			std::exception_ptr exception;
		};

		void _task_function(task_state state)
		{
			_task_state.store(state, std::memory_order_release);

			if (state == task_state::running)
			{
				_task_package();
				_task_state.store(task_state::ready, std::memory_order_release);
			}
		}

		template <class ValueType>
		static auto _bind_forward(std::remove_reference_t<ValueType> & arg)
		{
			if constexpr (std::is_lvalue_reference_v<ValueType>)
			{
				return std::ref(arg);
			}
			else
			{
				return std::bind(std::move<ValueType &>, std::ref(arg));
			}
		}

		std::packaged_task<RetType()> _task_package;
		std::future<RetType> _task_future;
		std::any _task_result;
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
				   threadpool_state state = threadpool_state::running,
				   int_fast8_t minimum_priority = std::numeric_limits<int_fast8_t>::min() + 1,
				   int_fast8_t maximum_priority = std::numeric_limits<int_fast8_t>::max() - 1) :
			_threadpool_size(size),
			_threadpool_state(state),
			_threadpool_minimum_priority(std::clamp<int_fast8_t>(minimum_priority,
																 std::numeric_limits<int_fast8_t>::min() + 1,
																 std::numeric_limits<int_fast8_t>::max() - 1)),
			_threadpool_maximum_priority(std::clamp<int_fast8_t>(maximum_priority,
																 std::numeric_limits<int_fast8_t>::min() + 1,
																 std::numeric_limits<int_fast8_t>::max() - 1)),
			_threadpool_default_priority(_threadpool_minimum_priority / 2 + _threadpool_maximum_priority / 2)
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

			std::scoped_lock<std::mutex> lock(_threadpool_task_mutex);

			priority = std::clamp(priority,
								  std::min(_threadpool_minimum_priority, _threadpool_maximum_priority),
								  std::max(_threadpool_minimum_priority, _threadpool_maximum_priority));

			_threadpool_task_queue.emplace(std::bind(&stp::task<RetType, ParamTypes ...>::_task_function,
													 &task,
													 std::placeholders::_1),
										   &task,
										   static_cast<uint_fast8_t>(std::abs(_threadpool_minimum_priority - priority) + 1));

			_threadpool_task_set.insert(&task);

			_threadpool_task.store(true, std::memory_order_release);

			_threadpool_condvar.notify_one();
		}
		bool pop()
		{
			std::scoped_lock<std::mutex> lock(_threadpool_task_mutex);

			return _threadpool_task_set.size() ? _threadpool_task_set.clear(), true : false;
		}
		template <class RetType, class ... ParamTypes>
		bool pop(task<RetType, ParamTypes ...> & task)
		{
			std::scoped_lock<std::mutex> lock(_threadpool_task_mutex);

			return _threadpool_task_set.erase(&task);
		}
		void resize(size_t new_size)
		{
			if (_threadpool_size != new_size)
			{
				std::scoped_lock<std::shared_mutex> lock(_threadpool_mutex);

				size_t delta_size = std::max(_threadpool_size, new_size) - std::min(_threadpool_size, new_size);

				if (_threadpool_size < new_size)
				{
					auto it = _threadpool_thread_list.begin(), it_e = _threadpool_thread_list.end();
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
							while (n++ < delta_size)
							{
								_threadpool_thread_list.emplace_back(this);
							}
						}
					}
				}
				else
				{
					auto it_b = _threadpool_thread_list.begin(), it_e = _threadpool_thread_list.end(), it = it_b;
					for (size_t n = 0; n < delta_size; ++it == it_e ? it = it_b, void() : void())
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
		void default_priority(int_fast8_t new_priority)
		{
			_threadpool_default_priority = std::clamp(new_priority,
													  std::min(_threadpool_minimum_priority,
															   _threadpool_maximum_priority),
													  std::max(_threadpool_minimum_priority,
															   _threadpool_maximum_priority));
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
			_task(std::function<void(task_state)> func = nullptr, void * id = nullptr, uint_fast8_t prty = 0) :
				function(func),
				identity(id),
				priority(prty)
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
			std::shared_lock<std::shared_mutex> threadpool_lock(_threadpool_mutex);

			bool function = true;

			for (;;)
			{
				while (_threadpool_state != threadpool_state::terminating
					   && this_thread->active
					   && (!_threadpool_task.load(std::memory_order_acquire)
						   || _threadpool_state == threadpool_state::stopped))
				{
					_threadpool_condvar.wait(threadpool_lock);
				}

				if (_threadpool_state == threadpool_state::terminating || !this_thread->active)
				{
					if ((this_thread->inactive = !this_thread->active) == true)
					{
						this_thread->thread.detach();
					}

					return;
				}

				threadpool_lock.unlock();
					
				if (std::scoped_lock<std::mutex> lock(_threadpool_task_mutex);
					_threadpool_task.load(std::memory_order_relaxed))
				{
					this_thread->task = std::move(_threadpool_task_queue.top());
					function = _threadpool_task_set.erase(this_thread->task.identity);
					_threadpool_task_queue.pop();

					_threadpool_task.store(!_threadpool_task_queue.empty(), std::memory_order_release);
				}
				
				if (this_thread->task.function)
				{
					if (function)
					{
						this_thread->task.function(task_state::running);
					}
					else
					{
						this_thread->task.function(task_state::suspended);
					}

					this_thread->task.function = nullptr;
				}

				threadpool_lock.lock();
			}
		}

		size_t _threadpool_size;
		threadpool_state _threadpool_state;
		int_fast8_t const _threadpool_minimum_priority;
		int_fast8_t const _threadpool_maximum_priority;
		int_fast8_t _threadpool_default_priority;
		std::atomic<bool> _threadpool_task{ false };
		std::priority_queue<_task, std::deque<_task>> _threadpool_task_queue;
		std::unordered_set<void *> _threadpool_task_set;
		std::list<_thread> _threadpool_thread_list;
		std::mutex _threadpool_task_mutex;
		std::shared_mutex _threadpool_mutex;
		std::condition_variable_any _threadpool_condvar;
	};
}

#endif
