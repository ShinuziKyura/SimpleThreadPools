#ifndef SIMPLE_THREAD_POOLS_HPP
#define SIMPLE_THREAD_POOLS_HPP

#include <list>
#include <queue>
#include <functional>
#include <future>
#include <shared_mutex>

// SimpleThreadPools - version B.4.1.0
namespace stp
{
	enum class task_error_code : uint_least8_t
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

	enum class task_state : uint_least8_t
	{
		ready,
		running,
		waiting,
		suspended,
		null
	};

	template <class RetType>
	class _task
	{
	protected:
		_task<RetType>() = default;
		template <class ... ParamTypes, class ... ArgTypes>
		_task<RetType>(RetType(* func)(ParamTypes ...), ArgTypes && ... args) :
			_task_package(std::bind(func, _bind_forward<ArgTypes>(args) ...)),
			_task_state(task_state::suspended)
		{
		}
		template <class ObjType, class ... ParamTypes, class ... ArgTypes>
		_task<RetType>(RetType(ObjType::* func)(ParamTypes ...), ObjType * obj, ArgTypes && ... args) :
			_task_package(std::bind(func, obj, _bind_forward<ArgTypes>(args) ...)),
			_task_state(task_state::suspended)
		{
		}
		~_task<RetType>()
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
	public:
		void operator()()
		{
			function()();
		}

		void wait()
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
					throw task_error(task_error_code::thread_deadlock_would_occur);
				case task_state::waiting:
				case task_state::running:
					_task_future.wait();
				default:
					break;
			}
		}
		template <class Rep, class Period>
		task_state wait_for(std::chrono::duration<Rep, Period> const & duration)
		{
			return wait_until(std::chrono::steady_clock::now() + duration);
		}
		template <class Clock, class Duration>
		task_state wait_until(std::chrono::time_point<Clock, Duration> const & time_point)
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::waiting:
				case task_state::running:
					_task_future.wait_until(time_point);
				default:
					break;
			}

			return _task_state.load(std::memory_order_relaxed);
		}
		task_state state() const
		{
			return _task_state.load(std::memory_order_relaxed);
		}
		bool ready() const
		{
			return _task_state.load(std::memory_order_relaxed) == task_state::ready;
		}
		[[nodiscard]] std::packaged_task<void()> function() & // WARNING: The destructor for the task will block until operator() has been called for the object returned by this function
		{
			switch (_task_state.load(std::memory_order_acquire))
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

			std::packaged_task<void()> function(std::bind(&_task<RetType>::_function, this));
			
			_task_future = function.get_future();

			_task_state.store(task_state::waiting, std::memory_order_release);

			return function;
		}
	protected:
		void _move(_task<RetType> && other)
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
			_task_state.store(other._task_state.exchange(task_state::null, std::memory_order_release), std::memory_order_release);
		}
		void _exchange(_task<RetType> && other) // std::exchange protects against self-assignment and enforces safe valid state
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
			_task_future = std::exchange(other._task_future, std::future<void>());
			_task_state.store(other._task_state.exchange(task_state::null, std::memory_order_release), std::memory_order_release);
		}
		void _reset()
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
				_task_future = std::future<void>();
				_task_state.store(task_state::suspended, std::memory_order_release);
			}
		}
		void _function()
		{
			_task_state.store(task_state::running, std::memory_order_release);

			_task_package();

			_task_state.store(task_state::ready, std::memory_order_release);
		}

		template <class ValueType>
		static auto _bind_forward(std::remove_reference_t<ValueType> & val) // Correctly forwards values to function in bind expression
		{
			if constexpr (std::is_lvalue_reference_v<ValueType>)
			{
				return std::ref(val);
			}
			else
			{
				return std::bind(std::move<ValueType &>, std::move(val));
			}
		}

		std::packaged_task<RetType()> _task_package;
		std::future<void> _task_future;
		std::atomic<task_state> _task_state{ task_state::null };
	};

	template <class RetType, class ... ParamTypes>
	class task : public _task<RetType>
	{
		static_assert(std::is_default_constructible_v<RetType>, "stp::task<T>: T must satisfy the requirements of DefaultConstructible");
		static_assert(std::negation_v<std::is_rvalue_reference<RetType>>, "stp::task<T>: T may not be a rvalue-reference");

		using ResultType = std::conditional_t<std::negation_v<std::is_reference<RetType>>, RetType, std::remove_reference_t<RetType> *>;
	public:
		task<RetType, ParamTypes ...>() = default;
		template <class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(* func)(ParamTypes ...), ArgTypes && ... args) : 
			_task<RetType>(func, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class ObjType, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(ParamTypes ...), ObjType * obj, ArgTypes && ... args) : 
			_task<RetType>(func, obj, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class ... AutoParamTypes, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(* func)(AutoParamTypes ...), ArgTypes && ... args) : 
			_task<RetType>(func, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class ObjType, class ... AutoParamTypes, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(AutoParamTypes ...), ObjType * obj, ArgTypes && ... args) : 
			_task<RetType>(func, obj, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class ... AnyParamTypes, std::enable_if_t<std::is_move_assignable_v<ResultType>, int> = 0>
		task<RetType, ParamTypes ...>(task<RetType, AnyParamTypes ...> && other) // Move pseudo-constructor
		{
			this->_move(static_cast<_task<RetType> &&>(other));

			_task_result = std::move(other._task_result);
		}
		template <class ... AnyParamTypes, std::enable_if_t<std::is_move_assignable_v<ResultType>, int> = 0>
		task<RetType, ParamTypes ...> & operator=(task<RetType, AnyParamTypes ...> && other) // Move assignment operator
		{
			this->_exchange(static_cast<_task<RetType> &&>(other));

			_task_result = std::exchange(other._task_result, ResultType());

			return *this;
		}

		std::add_lvalue_reference_t<RetType> get()
		{
			switch (this->_task_state.load(std::memory_order_acquire))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
					throw task_error(task_error_code::thread_deadlock_would_occur);
				default:
					break;
			}

			if (this->_task_future.valid())
			{
				this->_task_future.get();

				try
				{
					if constexpr (std::negation_v<std::is_reference<RetType>>)
					{
						_task_result = std::move(this->_task_package.get_future().get());
					}
					else
					{
						_task_result = &this->_task_package.get_future().get();
					}
				}
				catch (...)
				{
					reset();

					throw;
				}
			}

			if constexpr (std::negation_v<std::is_reference<RetType>>)
			{
				return _task_result;
			}
			else
			{
				return *_task_result;
			}
		}
		void reset()
		{
			this->_reset();

			_task_result = std::move(ResultType());
		}
	private:
		ResultType _task_result;

		template <class AnyRetType, class ... AnyParamTypes>
		friend class task;
	};

	template <class ... ParamTypes>
	class task<void, ParamTypes ...> : public _task<void>
	{
	public:
		task<void, ParamTypes ...>() = default;
		template <class ... ArgTypes>
		task<void, ParamTypes ...>(void(* func)(ParamTypes ...), ArgTypes && ... args) : 
			_task<void>(func, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class ObjType, class ... ArgTypes>
		task<void, ParamTypes ...>(void(ObjType::* func)(ParamTypes ...), ObjType * obj, ArgTypes && ... args) : 
			_task<void>(func, obj, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class ... AutoParamTypes, class ... ArgTypes>
		task<void, ParamTypes ...>(void(* func)(AutoParamTypes ...), ArgTypes && ... args) : 
			_task<void>(func, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class ObjType, class ... AutoParamTypes, class ... ArgTypes>
		task<void, ParamTypes ...>(void(ObjType::* func)(AutoParamTypes ...), ObjType * obj, ArgTypes && ... args) : 
			_task<void>(func, obj, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class ... AnyParamTypes>
		task<void, ParamTypes ...>(task<void, AnyParamTypes ...> && other) // Move pseudo-constructor 
		{
			this->_move(static_cast<_task<void> &&>(other));
		}
		template <class ... AnyParamTypes>
		task<void, ParamTypes ...> & operator=(task<void, AnyParamTypes ...> && other) // Move assignment operator
		{
			this->_exchange(static_cast<_task<void> &&>(other));

			return *this;
		}

		void get()
		{
			switch (this->_task_state.load(std::memory_order_acquire))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
					throw task_error(task_error_code::thread_deadlock_would_occur);
				default:
					break;
			}

			if (this->_task_future.valid())
			{
				this->_task_future().get();

				try
				{
					this->_task_package.get_future().get();
				}
				catch (...)
				{
					reset();

					throw;
				}
			}
		}
		void reset()
		{
			this->_reset();
		}

		template <class AnyRetType, class ... AnyParamTypes>
		friend class task;
	};

	template <class RetType, class ... ParamTypes, class ... ArgTypes>
	inline task<RetType> make_task(RetType(* func)(ParamTypes ...), ArgTypes && ... args) // Allows disambiguation of function overload by specifying the parameters' types
	{
		return task<RetType>(func, std::forward<ArgTypes>(args) ...);
	}
	template <class RetType, class ... ParamTypes, class ObjType, class ... ArgTypes>
	inline task<RetType> make_task(RetType(ObjType::* func)(ParamTypes ...), ObjType * obj, ArgTypes && ... args)
	{
		return task<RetType>(func, obj, std::forward<ArgTypes>(args) ...);
	}

	enum class threadpool_state : uint_least8_t
	{
		running,
		stopped,
		terminating
	};

	class threadpool
	{
	public:
		threadpool(size_t size = std::thread::hardware_concurrency(), threadpool_state state = threadpool_state::running) :
			_threadpool_minimum_priority(std::numeric_limits<int_least8_t>::min()),
			_threadpool_maximum_priority(std::numeric_limits<int_least8_t>::max()),
			_threadpool_default_priority(0),
			_threadpool_size(size),
			_threadpool_state(state)
		{
			for (size_t n = 0; n < size; ++n)
			{
				_threadpool_thread_list.emplace_back(this);
			}
		}
		threadpool(int_least8_t minimum_priority, int_least8_t maximum_priority, int_least8_t default_priority,
				   size_t size = std::thread::hardware_concurrency(), threadpool_state state = threadpool_state::running) :
			_threadpool_minimum_priority(minimum_priority),
			_threadpool_maximum_priority(maximum_priority),
			_threadpool_default_priority(std::clamp(default_priority,
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
		void execute(task<RetType, ParamTypes ...> & task)
		{
			execute(task, _threadpool_default_priority);
		}
		template <class RetType, class ... ParamTypes>
		void execute(task<RetType, ParamTypes ...> & task, int_least8_t priority)
		{
			auto function = task.function();

			priority = std::clamp(priority,
								  std::min(_threadpool_minimum_priority, _threadpool_maximum_priority),
								  std::max(_threadpool_minimum_priority, _threadpool_maximum_priority));

			std::scoped_lock<std::mutex> lock(_threadpool_task_mutex);

			_threadpool_task_queue.emplace(std::move(function), uint_least8_t(std::abs(_threadpool_minimum_priority - priority)));

			_threadpool_task.store(true, std::memory_order_relaxed);

			_threadpool_condvar.notify_one();
		}
		void resize(size_t size)
		{
			if (_threadpool_size != size)
			{
				std::scoped_lock<std::shared_mutex> lock(_threadpool_mutex);
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
		void set_default_priority(int_least8_t default_priority)
		{
			_threadpool_default_priority = std::clamp(default_priority,
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
		int_least8_t minimum_priority() const
		{
			return _threadpool_minimum_priority;
		}
		int_least8_t maximum_priority() const
		{
			return _threadpool_maximum_priority;
		}
		int_least8_t get_default_priority() const
		{
			return _threadpool_default_priority;
		}
	private:
		struct _task
		{
			_task(std::packaged_task<void()> func = std::packaged_task<void()>(), uint_least8_t prty = 0) :
				function(std::move(func)),
				priority(prty)
			{
			}

			bool operator<(_task const & other) const
			{
				return priority != other.priority ? priority < other.priority : origin > other.origin;
			}

			std::packaged_task<void()> function;
			uint_least8_t priority;
			std::chrono::steady_clock::time_point origin{ std::chrono::steady_clock::now() };
		};
		struct _thread
		{
			_thread(threadpool * threadpool) :
				thread(&threadpool::_threadpool_function, threadpool, this)
			{
			}

			_task task;
			bool active{ true };
			bool inactive{ false };
			std::thread thread; // Must be the last variable to be initialized
		};

		void _threadpool_function(_thread * this_thread)
		{
			std::shared_lock<std::shared_mutex> threadpool_lock(_threadpool_mutex);

			while (_threadpool_state != threadpool_state::terminating && this_thread->active)
			{
				if (_threadpool_state == threadpool_state::running && _threadpool_task.load(std::memory_order_relaxed))
				{
					threadpool_lock.unlock();

					if (std::scoped_lock<std::mutex> lock(_threadpool_task_mutex); _threadpool_task.load(std::memory_order_relaxed))
					{
						this_thread->task = const_cast<_task &&>(_threadpool_task_queue.top());

						_threadpool_task_queue.pop();

						if (_threadpool_task_queue.empty())
						{
							_threadpool_task.store(false, std::memory_order_relaxed);
						}
					}

					if (this_thread->task.function.valid())
					{
						this_thread->task.function();
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

		int_least8_t const _threadpool_minimum_priority;
		int_least8_t const _threadpool_maximum_priority;
		int_least8_t _threadpool_default_priority;
		size_t _threadpool_size;
		threadpool_state _threadpool_state;
		std::atomic_bool _threadpool_task{ false };
		std::priority_queue<_task, std::deque<_task>> _threadpool_task_queue;
		std::list<_thread> _threadpool_thread_list;
		std::mutex _threadpool_task_mutex;
		std::shared_mutex _threadpool_mutex;
		std::condition_variable_any _threadpool_condvar;
	};
}

#endif
