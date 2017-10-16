#ifndef SIMPLE_THREAD_POOLS_HPP_
#define SIMPLE_THREAD_POOLS_HPP_

#include <future>
#include <functional>
#include <any>
#include <shared_mutex>
#include <list>
#include <queue>

// SimpleThreadPools
// C++17 version
namespace stp
{
	enum class task_errc
	{
		race_condition		= 1,
		thread_deadlock		= 2
	};

	enum class task_priority : uint8_t
	{
		maximum				= 5,
		high				= 4,
		normal				= 3,
		low					= 2,
		minimum				= 1,
		default				= 0
	};

	enum class task_state
	{
		ready,
		running,
		waiting,
		suspended
	};

	enum class threadpool_state
	{
		running,
		stopped,
		terminating
	};

	class task_error : public std::logic_error
	{
	public:
		task_error(task_errc errc) : logic_error(_task_errc_to_string(errc))
		{
		}
	private:
		static std::string _task_errc_to_string(task_errc errc)
		{
			switch (errc)
			{
				case task_errc::race_condition:
					return std::string("race condition");
				case task_errc::thread_deadlock:
					return std::string("thread deadlock");
			}
			return std::string();
		}
	};

	class task_priority_default
	{
	public:
		task_priority_default() = default;
		task_priority_default(task_priority value)
		{
			_value = value != task_priority::default ? value : task_priority::normal;
		}

		operator task_priority()
		{
			return _value;
		}
	private:
		static task_priority _value;
	};

	task_priority task_priority_default::_value = task_priority::normal;

	template <class RetType, class ... ParamTypes>
	class task
	{
	public:
		using result_type = RetType;

		template <class = std::enable_if_t<!std::is_same_v<RetType, void>>>
		RetType result()
		{
			if (_task_state == task_state::suspended)
			{
				throw task_error(task_errc::thread_deadlock);
			}

			if (_task_state == task_state::running || _task_state == task_state::waiting)
			{
				_task_future.wait();

				while (_task_state != task_state::ready);
			}
			return std::any_cast<RetType>(_task_result);
		}
		void wait()
		{
			if (_task_state == task_state::suspended)
			{
				throw task_error(task_errc::thread_deadlock);
			}

			if (_task_state == task_state::running || _task_state == task_state::waiting)
			{
				_task_future.wait();

				while (_task_state != task_state::ready);
			}
		}
		template <class Rep, class Period>
		task_state wait_for(std::chrono::duration<Rep, Period> const & timeout_duration)
		{
			if (_task_state == task_state::running || _task_state == task_state::waiting)
			{
				if (_task_future.wait_for(timeout_duration) == std::future_status::ready)
				{
					while (_task_state != task_state::ready);
				}
			}
			return _task_state;
		}
		template <class Clock, class Duration>
		task_state wait_until(std::chrono::time_point<Clock, Duration> const & timeout_time)
		{
			if (_task_state == task_state::running || _task_state == task_state::waiting)
			{
				if (_task_future.wait_until(timeout_time) == std::future_status::ready)
				{
					while (_task_state != task_state::ready);
				}
			}
			return _task_state;
		}
		bool ready() const
		{
			return _task_state == task_state::ready;
		}
		task_state state() const
		{
			return _task_state;
		}
		task_priority priority() const
		{
			return _task_priority;
		}
		void priority(task_priority priority)
		{
			if ((_task_priority = priority) == task_priority::default)
			{
				_task_priority = task_priority_default();
			}
		}
		void reset()
		{
			if (_task_state == task_state::running || _task_state == task_state::waiting)
			{
				throw task_error(task_errc::race_condition);
			}

			_task_package.reset();
			_task_future = std::move(_task_package.get_future());
			_task_shared_future = _task_future;
			_task_result.reset();
			_task_state = task_state::suspended;
		}

		task<RetType, ParamTypes ...>() = default;
		template <class ... AutoParamTypes, class ... ArgTypes,
				  class = std::enable_if_t<sizeof...(ParamTypes) == 0>>
		task<RetType, ParamTypes ...>(RetType(* func)(AutoParamTypes ...), ArgTypes && ... args) :
			_task_package(std::bind(func, _arg_wrapper(std::forward<ArgTypes>(args)) ...)),
			_task_future(std::move(_task_package.get_future())),
			_task_shared_future(_task_future)
		{
		}
		template <class ObjType, class ... AutoParamTypes, class ... ArgTypes,
				  class = std::enable_if_t<sizeof...(ParamTypes) == 0>>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(AutoParamTypes ...), ObjType * obj, ArgTypes && ... args) :
			_task_package(std::bind(func, obj, _arg_wrapper(std::forward<ArgTypes>(args)) ...)),
			_task_future(std::move(_task_package.get_future())),
			_task_shared_future(_task_future)
		{
		}
		template <class ... ArgTypes,
				  class = std::enable_if_t<sizeof...(ParamTypes) != 0>>
		task<RetType, ParamTypes ...>(RetType(* func)(ParamTypes ...), ArgTypes && ... args) :
			_task_package(std::bind(func, _arg_wrapper(std::forward<ArgTypes>(args)) ...)),
			_task_future(std::move(_task_package.get_future())),
			_task_shared_future(_task_future)
		{
		}
		template <class ObjType, class ... ArgTypes,
				  class = std::enable_if_t<sizeof...(ParamTypes) != 0>>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(ParamTypes ...), ObjType * obj, ArgTypes && ... args) :
			_task_package(std::bind(func, obj, _arg_wrapper(std::forward<ArgTypes>(args)) ...)),
			_task_future(std::move(_task_package.get_future())),
			_task_shared_future(_task_future)
		{
		}
		task<RetType, ParamTypes ...>(task<RetType, ParamTypes ...> const &) = delete;
		task<RetType, ParamTypes ...> & operator=(task<RetType, ParamTypes ...> const &) = delete;
		template <class ... OldParamTypes>
		task<RetType, ParamTypes ...>(task<RetType, OldParamTypes ...> && task) // Pseudo-move constructor
		{
			if (task._task_state == task_state::running || task._task_state == task_state::waiting)
			{
				throw task_error(task_errc::race_condition);
			}

			_task_package = std::move(task._task_package);
			_task_future = std::move(task._task_future);
			_task_shared_future = std::move(task._task_shared_future);
			_task_result = std::move(task._task_result);
			task._task_state == task_state::ready ? _task_state = task_state::ready, void() : void();
			_task_priority = task._task_priority;
		}
		template <class ... OldParamTypes>
		task<RetType, ParamTypes ...> & operator=(task<RetType, OldParamTypes ...> && task) // Pseudo-move assignment operator
		{
			if (task._task_state == task_state::running || task._task_state == task_state::waiting)
			{
				throw task_error(task_errc::race_condition);
			}

			_task_package = std::move(task._task_package);
			_task_future = std::move(task._task_future);
			_task_shared_future = std::move(task._task_shared_future);
			_task_result = std::move(task._task_result);
			task._task_state == task_state::ready ? _task_state = task_state::ready, void() : void();
			_task_priority = task._task_priority;
			return *this;
		}
		~task<RetType, ParamTypes ...>() = default;

		RetType operator()()
		{
			if (_task_state == task_state::running || _task_state == task_state::waiting)
			{
				throw task_error(task_errc::race_condition);
			}

			_task_function();
			if constexpr (!std::is_same_v<RetType, void>)
			{
				return std::any_cast<RetType>(_task_result);
			}
		}
	private:
		std::packaged_task<RetType()> _task_package;
		std::shared_future<RetType> _task_future;
		std::shared_future<RetType> _task_shared_future;
		std::any _task_result;
		std::atomic<task_state> _task_state{ task_state::suspended };
		task_priority _task_priority{ task_priority_default() };
		std::function<void()> _task_shared_function
		{ 
			std::bind(&task<RetType, ParamTypes ...>::_task_function, this)
		};

		void _task_function()
		{
			_task_package();
			if constexpr (!std::is_same_v<RetType, void>)
			{
				_task_result = std::move(_task_shared_future.get());
			}
			_task_state = task_state::ready;
		}

		template <class ArgType>
		static auto _arg_wrapper(ArgType & arg) -> decltype(std::ref(arg))
		{
			return std::ref(arg);
		}
		template <class ArgType>
		static auto _arg_wrapper(ArgType && arg) -> decltype(std::bind(std::move<ArgType &>, std::ref(arg)))
		{
			return std::bind(std::move<ArgType &>, std::ref(arg));
		}

		template <class RetType, class ... OldParamTypes> friend class task; // Required by pseudo-move constructor and assignment operator
		friend class threadpool;
	};

	template <class RetType, class ... ParamTypes, class ... ArgTypes>
	inline task<RetType> make_task(RetType(*func)(ParamTypes ...), ArgTypes && ... args) // Deduces parameters or disambiguates function
	{
		return task<RetType>(func, std::forward<ArgTypes>(args) ...);
	}
	template <class RetType, class ... ParamTypes, class ObjType, class ... ArgTypes>
	inline task<RetType> make_task(RetType(ObjType::* func)(ParamTypes ...), ObjType * obj, ArgTypes && ... args)
	{
		return task<RetType>(func, obj, std::forward<ArgTypes>(args) ...);
	}

	class threadpool
	{
	public:
		template <class RetType, class ... ParamTypes>
		void new_task(task<RetType, ParamTypes ...> & task, task_priority priority = task_priority::default)
		{
			std::scoped_lock<std::mutex> lock(_threadpool_task_queue_mutex);

			task._task_state = task_state::waiting;
			_threadpool_task_queue.emplace(&task._task_shared_function,
										   &task._task_state,
										   static_cast<uint8_t>(priority == task_priority::default
																? task._task_priority
																: priority));

			if (_threadpool_notify)
			{
				std::scoped_lock<std::shared_mutex> lock(_threadpool_mutex);

				++_threadpool_tasks;
				_threadpool_task_priority = _threadpool_task_queue.top()._priority;

				_threadpool_condvar.notify_one();
			}
		}
		void delete_tasks()
		{
			std::scoped_lock<std::shared_mutex, std::mutex> lock(_threadpool_mutex, _threadpool_task_queue_mutex);

			while (!_threadpool_task_queue.empty())
			{
				*(_threadpool_task_queue.top()._state) = task_state::suspended;
				_threadpool_task_queue.pop();
			}

			_threadpool_tasks = 0;
			_threadpool_task_priority = 0;
		}
		void notify_threads(bool threadpool_notify)
		{
			if ((_threadpool_notify = threadpool_notify))
			{
				notify_threads();
			}
			else
			{
				std::scoped_lock<std::shared_mutex, std::mutex> lock(_threadpool_mutex, _threadpool_task_queue_mutex);

				_threadpool_tasks = 0;
				_threadpool_task_priority = 0;
			}
		}
		void notify_threads()
		{
			std::scoped_lock<std::shared_mutex, std::mutex> lock(_threadpool_mutex, _threadpool_task_queue_mutex);

			_threadpool_tasks = _threadpool_task_queue.size();
			_threadpool_task_priority = _threadpool_task_queue.top()._priority;

			_threadpool_condvar.notify_all();
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
		void resize(size_t threadpool_size)
		{
			if (_threadpool_size != threadpool_size)
			{
				std::scoped_lock<std::mutex> lock(_threadpool_thread_list_mutex);

				uintmax_t threadpool_diff = std::abs(static_cast<intmax_t>(_threadpool_size)
													 - static_cast<intmax_t>(threadpool_size));

				if (_threadpool_size < threadpool_size)
				{
					for (uintmax_t n = 0; n < threadpool_diff; ++n)
					{
						_threadpool_thread_list.emplace_back(this);
					}
				}
				else
				{
					std::scoped_lock<std::shared_mutex> threadpool_lock(_threadpool_mutex);

					auto it_b = _threadpool_thread_list.begin(), it_e = _threadpool_thread_list.end(), it = it_b;
					for (uintmax_t n = 0; n < threadpool_diff; ++it == it_e ? it = it_b, void() : void())
					{
						if (it->_running && it->_sleeping)
						{
							it->_running = false;

							if (it->_task._function)
							{
								it->_task._function = nullptr;
								*(it->_task._state) = task_state::suspended;
							}

							++n;
						}
					}

					_threadpool_condvar.notify_all();
				}

				_threadpool_size = threadpool_size;
			}
		}
		size_t size() const
		{
			return _threadpool_size;
		}
		bool notify() const
		{
			return _threadpool_notify;
		}
		threadpool_state state() const
		{
			return _threadpool_state;
		}

		threadpool(size_t size = std::thread::hardware_concurrency(),
				   bool notify = true,
				   threadpool_state state = threadpool_state::running) :
			_threadpool_size(size),
			_threadpool_notify(notify),
			_threadpool_state(state)
		{
			for (size_t n = 0; n < _threadpool_size; ++n)
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
			_threadpool_mutex.lock();

			_threadpool_state = threadpool_state::terminating;

			_threadpool_condvar.notify_all();

			_threadpool_mutex.unlock();

			do
			{
				std::this_thread::yield();
			}
			while (_threadpool_threads);
		}
	private:
		struct _task_t
		{
			std::function<void()> * _function;
			std::atomic<task_state> * _state;
			uint8_t _priority;
			std::chrono::steady_clock::time_point _age{ std::chrono::steady_clock::now() };

			_task_t(std::function<void()> * function = nullptr, 
					std::atomic<task_state> * state = nullptr, 
					uint8_t priority = 0) :
				_function(function),
				_state(state),
				_priority(priority)
			{
			}

			bool operator<(_task_t const & task) const
			{
				return _priority != task._priority ? _priority < task._priority : _age > task._age;
			}
		};

		struct _thread_t
		{
			_task_t _task;
			std::atomic<bool> _running{ true };
			std::atomic<bool> _sleeping{ false };
			std::thread _thread; // Must be last variable to be initialized

			_thread_t(threadpool * threadpool) :
				_thread(&threadpool::_thread_pool, threadpool, this)
			{
			}
		};

		size_t _threadpool_size;
		bool _threadpool_notify;
		std::atomic<threadpool_state> _threadpool_state;
		std::atomic<size_t> _threadpool_threads{ 0 };
		std::atomic<uint8_t> _threadpool_task_priority{ 0 };
		std::atomic<size_t> _threadpool_tasks{ 0 };
		std::list<_thread_t> _threadpool_thread_list;
		std::priority_queue<_task_t, std::deque<_task_t>>_threadpool_task_queue;
		std::mutex _threadpool_thread_list_mutex;
		std::mutex _threadpool_task_queue_mutex;
		std::shared_mutex _threadpool_mutex;
		std::condition_variable_any _threadpool_condvar;

		void _thread_pool(_thread_t * this_thread)
		{
			++_threadpool_threads;

			std::shared_lock<std::shared_mutex> threadpool_lock(_threadpool_mutex, std::defer_lock);

			while (_threadpool_state != threadpool_state::terminating && this_thread->_running)
			{
				this_thread->_sleeping = true;

				threadpool_lock.lock();

				while (_threadpool_state != threadpool_state::terminating && this_thread->_running
					   && ((!_threadpool_tasks && !this_thread->_task._function)
						   || (!_threadpool_tasks && _threadpool_state == threadpool_state::stopped)
						   || (_threadpool_task_priority <= this_thread->_task._priority
							   && this_thread->_task._function 
							   && _threadpool_state == threadpool_state::stopped)))
				{
					_threadpool_condvar.wait(threadpool_lock);
				}

				this_thread->_sleeping = false;

				threadpool_lock.unlock();

				if (this_thread->_running)
				{
					switch (_threadpool_state)
					{
						case threadpool_state::running:
						case threadpool_state::stopped:
							if ((!this_thread->_task._function && _threadpool_tasks)
								|| _threadpool_task_priority > this_thread->_task._priority)
							{
								std::scoped_lock<std::mutex> lock(_threadpool_task_queue_mutex);

								if (!this_thread->_task._function)
								{
									if (_threadpool_tasks)
									{
										--_threadpool_tasks;

										this_thread->_task = _threadpool_task_queue.top();
										_threadpool_task_queue.pop();

										_threadpool_task_priority = (!_threadpool_task_queue.empty()
																	 ? _threadpool_task_queue.top()._priority
																	 : 0);
									}
								}
								else if (_threadpool_task_priority > this_thread->_task._priority)
								{
									_threadpool_task_queue.push(this_thread->_task);

									this_thread->_task = _threadpool_task_queue.top();
									_threadpool_task_queue.pop();

									_threadpool_task_priority = (!_threadpool_task_queue.empty()
																 ? _threadpool_task_queue.top()._priority
																 : 0);
								}
							}
						case threadpool_state::terminating:
							break;
					}

					switch (_threadpool_state)
					{
						case threadpool_state::running:
							if (this_thread->_task._function)
							{
								*(this_thread->_task._state) = task_state::running;
								(*this_thread->_task._function)();
								this_thread->_task._function = nullptr;
								this_thread->_task._state = nullptr;
								this_thread->_task._priority = 0;
							}
						case threadpool_state::stopped:
						case threadpool_state::terminating:
							break;
					}
				}
			}

			std::scoped_lock<std::mutex> lock(_threadpool_thread_list_mutex);

			auto it = _threadpool_thread_list.begin();
			while (this_thread->_thread.get_id() != it->_thread.get_id() ? ++it, true : false);
			it->_thread.detach();
			_threadpool_thread_list.erase(it);

			--_threadpool_threads;
		}
	};
}

#endif