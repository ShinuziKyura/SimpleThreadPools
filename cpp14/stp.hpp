#ifndef SIMPLE_THREAD_POOLS_HPP_
#define SIMPLE_THREAD_POOLS_HPP_

#include <future>
#include <functional>
#include <shared_mutex>
#include <list>
#include <queue>

// SimpleThreadPools - C++14 version
namespace stp
{
	enum class task_errc
	{
		state_loss_would_occur = 1,
		thread_deadlock_would_occur
	};

	enum class task_priority : uint8_t
	{
		maximum				= 5,
		high				= 4,
		normal				= 3,
		low					= 2,
		minimum				= 1,
		null				= 0
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
				case task_errc::state_loss_would_occur:
					return std::string("state loss would occur");
				case task_errc::thread_deadlock_would_occur:
					return std::string("thread deadlock would occur");
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
			if (value != task_priority::null)
			{
				_value = value;
			}
		}

		operator task_priority()
		{
			return _value;
		}
	private:
		static task_priority _value;
	};

	task_priority task_priority_default::_value{ task_priority::normal };

	template <class RetType, class ... ParamTypes>
	class task
	{
	public:
		using result_type = RetType;

		template <class = std::enable_if_t<!std::is_same<RetType, void>::value>>
		RetType result()
		{
			wait();

			return *_task_result;
		}
		void wait()
		{
			if (_task_state == task_state::suspended)
			{
				throw task_error(task_errc::thread_deadlock_would_occur);
			}

			_task_future_condition.wait();

			while (_task_state != task_state::ready);
		}
		template <class Rep, class Period>
		task_state wait_for(std::chrono::duration<Rep, Period> const & timeout_duration)
		{
			if (_task_state != task_state::suspended)
			{
				if (_task_future_condition.wait_for(timeout_duration) == std::future_status::ready)
				{
					while (_task_state != task_state::ready);
				}
			}
			return _task_state;
		}
		template <class Clock, class Duration>
		task_state wait_until(std::chrono::time_point<Clock, Duration> const & timeout_time)
		{
			if (_task_state != task_state::suspended)
			{
				if (_task_future_condition.wait_until(timeout_time) == std::future_status::ready)
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
			if (priority != task_priority::null)
			{
				_task_priority = priority;
			}
		}
		void reset()
		{
			if (_task_state == task_state::running || _task_state == task_state::waiting)
			{
				throw task_error(task_errc::state_loss_would_occur);
			}

			_task_package.reset();
			_task_future = std::move(_task_package.get_future());
			_task_future_condition = _task_future;
			_task_result.reset();
			_task_state = task_state::suspended;
		}

		task<RetType, ParamTypes ...>() = default;
		template <class ... AutoParamTypes, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(* func)(AutoParamTypes ...), ArgTypes && ... args) :
			_task_package(std::bind(func, _bind_forward(std::forward<ArgTypes>(args)) ...)),
			_task_future(std::move(_task_package.get_future())),
			_task_future_condition(_task_future)
		{
		}
		template <class ObjType, class ... AutoParamTypes, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(AutoParamTypes ...), ObjType * obj, ArgTypes && ... args) :
			_task_package(std::bind(func, obj, _bind_forward(std::forward<ArgTypes>(args)) ...)),
			_task_future(std::move(_task_package.get_future())),
			_task_future_condition(_task_future)
		{
		}
		template <class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(* func)(ParamTypes ...), ArgTypes && ... args) :
			_task_package(std::bind(func, _bind_forward(std::forward<ArgTypes>(args)) ...)),
			_task_future(std::move(_task_package.get_future())),
			_task_future_condition(_task_future)
		{
		}
		template <class ObjType, class ... ArgTypes>
		task<RetType, ParamTypes ...>(RetType(ObjType::* func)(ParamTypes ...), ObjType * obj, ArgTypes && ... args) :
			_task_package(std::bind(func, obj, _bind_forward(std::forward<ArgTypes>(args)) ...)),
			_task_future(std::move(_task_package.get_future())),
			_task_future_condition(_task_future)
		{
		}
		task<RetType, ParamTypes ...>(task<RetType, ParamTypes ...> const &) = delete;
		task<RetType, ParamTypes ...> & operator=(task<RetType, ParamTypes ...> const &) = delete;
		template <class ... MoveParamTypes>
		task<RetType, ParamTypes ...>(task<RetType, MoveParamTypes ...> && task) // Pseudo-move constructor
		{
			if (task._task_state == task_state::running || task._task_state == task_state::waiting)
			{
				throw task_error(task_errc::state_loss_would_occur);
			}

			_task_package = std::move(task._task_package);
			_task_future = std::move(task._task_future);
			_task_future_condition = std::move(task._task_future_condition);
			_task_result = std::move(task._task_result);
			_task_state = static_cast<task_state>(task._task_state);
			_task_priority = task._task_priority;
		}
		template <class ... MoveParamTypes>
		task<RetType, ParamTypes ...> & operator=(task<RetType, MoveParamTypes ...> && task) // Pseudo-move assignment operator
		{
			if (task._task_state == task_state::running || task._task_state == task_state::waiting)
			{
				throw task_error(task_errc::state_loss_would_occur);
			}

			_task_package = std::move(task._task_package);
			_task_future = std::move(task._task_future);
			_task_future_condition = std::move(task._task_future_condition);
			_task_result = std::move(task._task_result);
			_task_state = static_cast<task_state>(task._task_state);
			_task_priority = task._task_priority;
			return *this;
		}
		~task<RetType, ParamTypes ...>() = default;

		RetType operator()()
		{
			if (_task_state == task_state::running || _task_state == task_state::waiting)
			{
				throw task_error(task_errc::state_loss_would_occur);
			}

			_task_function();
			return _result(std::is_same<RetType, void>());
		}
	private:
		std::packaged_task<RetType()> _task_package;
		std::shared_future<RetType> _task_future;
		std::shared_future<RetType> _task_future_condition;
		std::shared_ptr<RetType> _task_result;
		std::atomic<task_state> _task_state{ task_state::suspended };
		task_priority _task_priority{ task_priority_default() };
		std::function<void()> _task_function{ [this] { _function(std::is_same<RetType, void>()); } };
		
		void _function(std::false_type)
		{
			_task_package();
			_task_result = std::move(std::make_shared<RetType>(std::move(_task_future.get())));
			_task_state = task_state::ready;
		}
		void _function(std::true_type) // Alternative to if constexpr in _function()
		{
			_task_package();
			_task_state = task_state::ready;
		}
		RetType _result(std::false_type)
		{
			return *_task_result;
		}
		RetType _result(std::true_type) // Alternative to if constexpr in operator()()
		{
		}
		template <class ArgType>
		static auto _bind_forward(ArgType & arg)
		{
			return std::ref(arg);
		}
		template <class ArgType>
		static auto _bind_forward(ArgType && arg)
		{
			return std::bind(std::move<ArgType &>, std::ref(arg));
		}

		template <class MoveRetType, class ... MoveParamTypes> friend class task; // Required by pseudo-move constructor and assignment operator
		friend class threadpool;
	};

	template <class RetType, class ... ParamTypes, class ... ArgTypes>
	inline task<RetType> make_task(RetType(* func)(ParamTypes ...), ArgTypes && ... args) // Deduces parameters or disambiguates function
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
		void new_task(task<RetType, ParamTypes ...> & task, task_priority priority = task_priority::null)
		{
			std::lock_guard<std::mutex> lock1(_threadpool_task_queue_mutex);

			task._task_state = task_state::waiting;
			_threadpool_task_queue.emplace(&task._task_function,
										   &task._task_state,
										   static_cast<uint8_t>(priority != task_priority::null
																? priority
																: task._task_priority));

			if (_threadpool_signals)
			{
				std::lock_guard<std::shared_timed_mutex> lock2(_threadpool_mutex);

				_threadpool_task_priority = _threadpool_task_queue.top()._priority;

				_threadpool_condition_variable.notify_one();
			}
		}
		void clear_tasks()
		{
			std::lock(_threadpool_task_queue_mutex, _threadpool_mutex);
			std::lock_guard<std::mutex> lock1(_threadpool_task_queue_mutex, std::adopt_lock);
			std::lock_guard<std::shared_timed_mutex> lock2(_threadpool_mutex, std::adopt_lock);

			for (auto & thread : _threadpool_thread_list)
			{
				if (!thread._working)
				{
					*(thread._task._state) = task_state::suspended;
					thread._task._function = nullptr;
					thread._task._state = nullptr;
					thread._task._priority = 0;
				}
			}

			while (!_threadpool_task_queue.empty())
			{
				*(_threadpool_task_queue.top()._state) = task_state::suspended;
				_threadpool_task_queue.pop();
			}

			_threadpool_task_priority = 0;
		}
		size_t number_tasks()
		{
			std::lock_guard<std::mutex> lock(_threadpool_task_queue_mutex);

			return _threadpool_task_queue.size();
		}
		void signal_tasks()
		{
			std::lock_guard<std::mutex> lock1(_threadpool_task_queue_mutex);

			if (!_threadpool_task_queue.empty())
			{
				std::lock_guard<std::shared_timed_mutex> lock2(_threadpool_mutex);

				_threadpool_task_priority = _threadpool_task_queue.top()._priority;

				_threadpool_condition_variable.notify_all();
			}
		}
		void run()
		{
			if (_threadpool_state == threadpool_state::stopped)
			{
				std::lock_guard<std::shared_timed_mutex> lock(_threadpool_mutex);

				_threadpool_state = threadpool_state::running;

				_threadpool_condition_variable.notify_all();
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
		void resize(size_t new_size)
		{
			if (_threadpool_size != new_size)
			{
				std::lock_guard<std::mutex> lock1(_threadpool_thread_list_mutex);

				uintmax_t amount = std::abs(static_cast<intmax_t>(_threadpool_size) - static_cast<intmax_t>(new_size));

				if (_threadpool_size < new_size)
				{
					for (uintmax_t n = 0; n < amount; ++n)
					{
						_threadpool_thread_list.emplace_back(this);
					}
				}
				else
				{
					std::lock_guard<std::shared_timed_mutex> lock2(_threadpool_mutex);

					auto it_b = _threadpool_thread_list.begin(), it_e = _threadpool_thread_list.end(), it = it_b;
					for (uintmax_t n = 0; n < amount; ++it == it_e ? it = it_b, void() : void())
					{
						if (it->_active && !it->_working)
						{
							it->_active = false;

							if (it->_task._function)
							{
								*(it->_task._state) = task_state::suspended;
							}

							++n;
						}
					}

					_threadpool_condition_variable.notify_all();
				}

				_threadpool_size = new_size;
			}
		}
		size_t size() const
		{
			return _threadpool_size;
		}
		bool signals() const
		{
			return _threadpool_signals;
		}
		void signals(bool signal)
		{
			if ((_threadpool_signals = signal) == true)
			{
				signal_tasks();
			}
			else
			{
				std::lock(_threadpool_task_queue_mutex, _threadpool_mutex);
				std::lock_guard<std::mutex> lock1(_threadpool_task_queue_mutex, std::adopt_lock);
				std::lock_guard<std::shared_timed_mutex> lock2(_threadpool_mutex, std::adopt_lock);

				_threadpool_task_priority = 0;
			}
		}
		threadpool_state state() const
		{
			return _threadpool_state;
		}

		threadpool(size_t size = std::thread::hardware_concurrency(),
				   bool signals = true,
				   threadpool_state state = threadpool_state::running) :
			_threadpool_size(size),
			_threadpool_signals(signals),
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

			_threadpool_condition_variable.notify_all();

			_threadpool_mutex.unlock();

			do
			{
				std::this_thread::yield();
			}
			while (_threadpool_active_threads);
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
			std::atomic<bool> _active{ true };
			std::atomic<bool> _working{ true };
			std::thread _thread; // Must be last variable to be initialized

			_thread_t(threadpool * threadpool) :
				_thread(&threadpool::_thread_pool, threadpool, this)
			{
			}
		};

		size_t _threadpool_size;
		bool _threadpool_signals;
		std::atomic<threadpool_state> _threadpool_state;
		std::atomic<uint8_t> _threadpool_task_priority{ 0 };
		std::atomic<size_t> _threadpool_active_threads{ 0 };
		std::list<_thread_t> _threadpool_thread_list;
		std::priority_queue<_task_t, std::deque<_task_t>>_threadpool_task_queue;
		std::mutex _threadpool_thread_list_mutex;
		std::mutex _threadpool_task_queue_mutex;
		std::shared_timed_mutex _threadpool_mutex;
		std::condition_variable_any _threadpool_condition_variable;

		void _thread_pool(_thread_t * this_thread)
		{
			++_threadpool_active_threads;

			std::shared_lock<std::shared_timed_mutex> threadpool_lock(_threadpool_mutex, std::defer_lock);

			while (_threadpool_state != threadpool_state::terminating && this_thread->_active)
			{
				this_thread->_working = false;

				threadpool_lock.lock();

				while (_threadpool_state != threadpool_state::terminating && this_thread->_active
					   && ((!_threadpool_task_priority && !this_thread->_task._function)
						   || (!_threadpool_task_priority && _threadpool_state == threadpool_state::stopped)
						   || (_threadpool_task_priority <= this_thread->_task._priority
							   && this_thread->_task._function
							   && _threadpool_state == threadpool_state::stopped)))
				{
					_threadpool_condition_variable.wait(threadpool_lock);
				}

				this_thread->_working = true;

				threadpool_lock.unlock();

				if (this_thread->_active)
				{
					switch (_threadpool_state)
					{
						case threadpool_state::running:
						case threadpool_state::stopped:
							if (!this_thread->_task._function || _threadpool_task_priority > this_thread->_task._priority)
							{
								std::lock_guard<std::mutex> lock(_threadpool_task_queue_mutex);

								if (!this_thread->_task._function)
								{
									if (_threadpool_task_priority)
									{
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

			std::lock_guard<std::mutex> lock(_threadpool_thread_list_mutex);

			auto it = _threadpool_thread_list.begin();
			while (this_thread->_thread.get_id() != it->_thread.get_id() ? ++it, true : false);
			it->_thread.detach();
			_threadpool_thread_list.erase(it);

			--_threadpool_active_threads;
		}
	};
}

#endif
