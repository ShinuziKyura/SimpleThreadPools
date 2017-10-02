#ifndef SIMPLE_THREAD_POOLS_HPP_
#define SIMPLE_THREAD_POOLS_HPP_

#include <future>
#include <functional>
#include <any>
#include <shared_mutex>
#include <list>
#include <queue>

namespace stp // C++17 version
{
	enum class task_priority : uint16_t
	{
		maximum				= 5,
		high				= 4,
		normal				= 3,
		low					= 2,
		minimum				= 1
	};

	enum class threadpool_state : uint16_t
	{
		running				= 0b0111,
		stopping			= 0b0011,
		terminating			= 0b0001
	};

	template <class RetType, class ... ParamType>
	class task
	{
	public:
		using result_type = RetType;

		template <class ResType = std::enable_if_t<!std::is_same_v<RetType, void>, RetType &>>
		ResType result()
		{
			if (!_task_ready)
			{
				if constexpr (!std::is_same_v<RetType, void>)
				{
					_task_result = std::move(_task_future.get());
				}
				_task_ready = true;
			}
			return std::any_cast<ResType>(_task_result);
		}
		bool ready()
		{
			if (!_task_ready)
			{
				if (_task_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					if constexpr (!std::is_same_v<RetType, void>)
					{
						_task_result = std::move(_task_future.get());
					}
					return _task_ready = true;
				}
				return false;
			}
			return true;
		}
		void wait()
		{
			if (!_task_ready)
			{
				if constexpr (!std::is_same_v<RetType, void>)
				{
					_task_result = std::move(_task_future.get());
				}
				_task_ready = true;
			}
		}
		template <class Rep, class Period>
		std::future_status wait_for(std::chrono::duration<Rep, Period> const & timeout_duration)
		{
			auto retval = _task_future.wait_for(timeout_duration);
			if (!_task_ready && retval == std::future_status::ready)
			{
				if constexpr (!std::is_same_v<RetType, void>)
				{
					_task_result = std::move(_task_future.get());
				}
				_task_ready = true;
			}
			return retval;
		}
		template <class Clock, class Duration>
		std::future_status wait_until(std::chrono::time_point<Clock, Duration> const & timeout_time)
		{
			auto retval = _task_future.wait_until(timeout_time);
			if (!_task_ready && retval == std::future_status::ready)
			{
				if constexpr (!std::is_same_v<RetType, void>)
				{
					_task_result = std::move(_task_future.get());
				}
				_task_ready = true;
			}
			return retval;
		}

		task<RetType, ParamType ...>() = delete;
		template <class FuncType,
				  class = std::enable_if_t<std::is_convertible_v<FuncType, std::function<RetType()>> 
						  && sizeof...(ParamType) == 0>>
		task<RetType, ParamType ...>(FuncType & func) :
			_task_package(static_cast<std::function<RetType()>>(func))
		{
		}
		template <class ... ArgType,
				  class = std::enable_if_t<sizeof...(ParamType) != 0>>
		task<RetType, ParamType ...>(RetType(* func)(ParamType ...), ArgType && ... arg) :
			_task_package(std::bind(func, _arg_wrapper(std::forward<ArgType>(arg)) ...))
		{
		}
		template <class ObjType, class ... ArgType,
				  class = std::enable_if_t<sizeof...(ParamType) != 0>>
		task<RetType, ParamType ...>(RetType(ObjType::* func)(ParamType ...), ObjType * obj, ArgType && ... arg) :
			_task_package(std::bind(func, obj, _arg_wrapper(std::forward<ArgType>(arg)) ...))
		{
		}
		template <class ... AutoParamType, class ... ArgType,
				  class = std::enable_if_t<sizeof...(ParamType) == 0>>
		task<RetType, ParamType ...>(RetType(* func)(AutoParamType ...), ArgType && ... arg) :
			_task_package(std::bind(func, _arg_wrapper(std::forward<ArgType>(arg)) ...))
		{
		}
		template <class ObjType, class ... AutoParamType, class ... ArgType,
				  class = std::enable_if_t<sizeof...(ParamType) == 0>>
		task<RetType, ParamType ...>(RetType(ObjType::* func)(AutoParamType ...), ObjType * obj, ArgType && ... arg) :
			_task_package(std::bind(func, obj, _arg_wrapper(std::forward<ArgType>(arg)) ...))
		{
		}
		task<RetType, ParamType ...>(task<RetType, ParamType ...> const &) = delete;
		task<RetType, ParamType ...> & operator=(task<RetType, ParamType ...> const &) = delete;
		task<RetType, ParamType ...>(task<RetType, ParamType ...> &&) = default;
		task<RetType, ParamType ...> & operator=(task<RetType, ParamType ...> &&) = default;
		~task<RetType, ParamType ...>() = default;

		template <class ResType = std::conditional_t<!std::is_same_v<RetType, void>, RetType &, void>>
		ResType operator()()
		{
			_task_package();
			if constexpr (!std::is_same_v<RetType, void>)
			{
				_task_result = std::move(_task_future.get());
			}
			_task_ready = true;
			if constexpr (!std::is_same_v<RetType, void>)
			{
				return std::any_cast<ResType>(_task_result);
			}
		}
	private:
		std::packaged_task<RetType()> _task_package;
		std::function<void()> _task_function = [this] { _task_package(); };
		std::future<RetType> _task_future = _task_package.get_future();
		std::any _task_result;
		bool _task_ready = false;

		template <class ArgType, class = std::enable_if_t<std::is_lvalue_reference_v<ArgType &&>>>
		auto _arg_wrapper(ArgType && arg) -> decltype(std::ref(arg))
		{
			return std::ref(arg);
		}
		template <class ArgType, class = std::enable_if_t<std::is_rvalue_reference_v<ArgType &&>>>
		auto _arg_wrapper(ArgType && arg) -> decltype(std::bind(std::move<ArgType &>, std::ref(arg)))
		{
			return std::bind(std::move<ArgType &>, std::ref(arg));
		}

		friend class threadpool;
	};

	class threadpool
	{
	public:
		template <class RetType>
		void new_task(task<RetType> & task, task_priority priority = task_priority::normal)
		{
			std::scoped_lock<std::mutex> lock(_task_queue_mutex);

			_task_queue.emplace(&task._task_function, static_cast<_task_priority_t>(priority));

			if (_threadpool_notify)
			{
				std::scoped_lock<std::shared_mutex> lock(_threadpool_mutex);

				++_threadpool_new_tasks;
				_task_priority = _task_queue.top()._priority;

				_threadpool_condvar.notify_one();
			}
		}
		void delete_tasks()
		{
			std::scoped_lock<std::shared_mutex, std::mutex> lock(_threadpool_mutex, _task_queue_mutex);

			while (!_task_queue.empty())
			{
				_task_queue.pop();
			}

			_threadpool_new_tasks = 0;
			_task_priority = 0;
		}
		void notify_threads(bool threadpool_notify)
		{
			if ((_threadpool_notify = threadpool_notify))
			{
				notify_threads();
			}
			else
			{
				std::scoped_lock<std::shared_mutex, std::mutex> lock(_threadpool_mutex, _task_queue_mutex);

				_threadpool_new_tasks = 0;
				_task_priority = 0;
			}
		}
		void notify_threads()
		{
			std::scoped_lock<std::shared_mutex, std::mutex> lock(_threadpool_mutex, _task_queue_mutex);

			_threadpool_new_tasks = _task_queue.size();
			_task_priority = _task_queue.top()._priority;

			_threadpool_condvar.notify_all();
		}
		void run()
		{
			if (_threadpool_state == _threadpool_state_t::stopping)
			{
				std::scoped_lock<std::shared_mutex> lock(_threadpool_mutex);

				_threadpool_state = _threadpool_state_t::running;

				_threadpool_condvar.notify_all();
			}
		}
		void stop()
		{
			if (_threadpool_state == _threadpool_state_t::running)
			{
				std::scoped_lock<std::shared_mutex> lock(_threadpool_mutex);

				_threadpool_state = _threadpool_state_t::stopping;
			}
		}
		void resize(size_t threadpool_size)
		{
			if (_threadpool_size != threadpool_size)
			{
				uintmax_t threadpool_diff = abs(static_cast<intmax_t>(_threadpool_size) - 
												static_cast<intmax_t>(threadpool_size));

				std::scoped_lock<std::mutex> lock(_thread_list_mutex);

				if (_threadpool_size < threadpool_size)
				{
					for (uintmax_t n = 0; n < threadpool_diff; ++n)
					{
						_thread_list.emplace_back(this);
					}
				}
				else
				{
					std::unique_lock<std::shared_mutex> threadpool_lock(_threadpool_mutex);

					auto it_b = _thread_list.begin(), it_e = _thread_list.end(), it = it_b;
					for (uintmax_t n = 0; n < threadpool_diff; ++it)
					{
						if (it == it_e)
						{
							it = it_b;

							_threadpool_condvar.notify_all();

							threadpool_lock.unlock();

							std::this_thread::yield();

							threadpool_lock.lock();
						}
						if (!it->_task._function)
						{
							it->_active = false;
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
		size_t running() const
		{
			return _thread_running;
		}
		size_t waiting() const
		{
			return _thread_waiting;
		}

		threadpool(size_t size = std::thread::hardware_concurrency(),
				   bool notify = true,
				   threadpool_state state = threadpool_state::running) :
			_threadpool_size(size),
			_threadpool_notify(notify),
			_task_priority(0),
			_threadpool_state(state),
			_threadpool_new_tasks(0),
			_thread_active(0),
			_thread_running(0),
			_thread_waiting(0)
		{
			for (size_t n = 0; n < _threadpool_size; ++n)
			{
				_thread_list.emplace_back(this);
			}
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			_threadpool_mutex.lock();

			_threadpool_state = _threadpool_state_t::terminating;

			_threadpool_condvar.notify_all();

			_threadpool_mutex.unlock();
		
			do
			{
				std::this_thread::yield();
			}
			while (_thread_active);
		}
	private:
		using _task_priority_t = uint16_t;
		using _threadpool_state_t = threadpool_state;

		struct _task_t
		{
			std::function<void()> * _function;

			_task_priority_t _priority;
			std::chrono::high_resolution_clock::time_point _age;

			_task_t(std::function<void()> * function = nullptr,
					_task_priority_t priority = 0) :
				_function(function),
				_priority(priority),
				_age(std::chrono::high_resolution_clock::now())
			{
			}
		};

		struct _thread_t
		{
			_task_t _task;

			std::atomic_bool _active;
			std::thread _thread;

			_thread_t(threadpool * threadpool) : 
				_task(),
				_active(true),
				_thread(&threadpool::_thread_dispatch, threadpool, this)
			{
			}
		};

		size_t _threadpool_size;
		bool _threadpool_notify;
		std::list<_thread_t> _thread_list;
		std::priority_queue<_task_t, std::deque<_task_t>, bool(*)(_task_t const &, _task_t const &)> _task_queue
		{
			[] (_task_t const & task_1, _task_t const & task_2) -> bool
			{
				return (task_1._priority != task_2._priority
						? task_1._priority < task_2._priority
						: task_1._age > task_2._age);
			}
		};
		
		std::atomic<_task_priority_t> _task_priority;
		std::atomic<_threadpool_state_t> _threadpool_state;
		std::atomic_size_t _threadpool_new_tasks;

		std::atomic_size_t _thread_active;
		std::atomic_size_t _thread_running;
		std::atomic_size_t _thread_waiting;

		std::mutex _thread_list_mutex;
		std::mutex _task_queue_mutex;
		std::shared_mutex _threadpool_mutex;
		std::condition_variable_any _threadpool_condvar;

		void _thread_pool(_thread_t * this_thread)
		{
			std::shared_lock<std::shared_mutex> threadpool_lock(_threadpool_mutex, std::defer_lock);

			while (_threadpool_state != _threadpool_state_t::terminating && this_thread->_active)
			{
				threadpool_lock.lock();

				while (_threadpool_state != _threadpool_state_t::terminating && this_thread->_active
					   && ((!_threadpool_new_tasks && !this_thread->_task._function)
					   || (!_threadpool_new_tasks && _threadpool_state == _threadpool_state_t::stopping)
					   || (_task_priority <= this_thread->_task._priority && this_thread->_task._function
					   && _threadpool_state == _threadpool_state_t::stopping)))
				{
					++_thread_waiting;

					_threadpool_condvar.wait(threadpool_lock);

					--_thread_waiting;
				}

				threadpool_lock.unlock();

				if (this_thread->_active)
				{
					switch (_threadpool_state)
					{
						case _threadpool_state_t::running:
						case _threadpool_state_t::stopping:
							if ((!this_thread->_task._function && _threadpool_new_tasks)
								|| _task_priority > this_thread->_task._priority)
							{
								std::scoped_lock<std::mutex> lock(_task_queue_mutex);

								if (!this_thread->_task._function)
								{
									if (_threadpool_new_tasks)
									{
										--_threadpool_new_tasks;

										this_thread->_task = _task_queue.top();
										_task_queue.pop();

										_task_priority = (!_task_queue.empty() ? _task_queue.top()._priority : 0);
									}
								}
								else if (_task_priority > this_thread->_task._priority)
								{
									_task_queue.push(this_thread->_task);

									this_thread->_task = _task_queue.top();
									_task_queue.pop();

									_task_priority = (!_task_queue.empty() ? _task_queue.top()._priority : 0);
								}
							}
						case _threadpool_state_t::terminating:
							break;
					}

					switch (_threadpool_state)
					{
						case _threadpool_state_t::running:
							if (this_thread->_task._function)
							{
								++_thread_running;

								(*this_thread->_task._function)();
								this_thread->_task._function = nullptr;
								this_thread->_task._priority = 0;

								--_thread_running;
							}
						case _threadpool_state_t::stopping:
						case _threadpool_state_t::terminating:
							break;
					}
				}
			}
		}
		void _thread_dispatch(_thread_t * this_thread)
		{
			++_thread_active;

			_thread_pool(this_thread);

			std::scoped_lock<std::mutex> lock(_thread_list_mutex);

			auto it = _thread_list.begin();
			while (this_thread->_thread.get_id() != it->_thread.get_id())
			{
				++it;
			}
			it->_thread.detach();
			_thread_list.erase(it);

			--_thread_active;
		}
	};
}

#endif