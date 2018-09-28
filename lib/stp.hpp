//======================================================================================================================================================================================================//
//																																																		//
//		SimpleThreadPools, a library that provides a threadpool class and related utilities																												//	
//		Copyright(C) 2017 Ricardo Santos																																								//
//																																																		//
//		This program is free software; you can redistribute it and/or modify																															//
//		it under the terms of the GNU General Public License as published by																															//
//		the Free Software Foundation; either version 2 of the License, or																																//
//		(at your option) any later version.																																								//
//																																																		//
//		This program is distributed in the hope that it will be useful,																																	//
//		but WITHOUT ANY WARRANTY; without even the implied warranty of																																	//
//		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the																																		//
//		GNU General Public License for more details.																																					//
//																																																		//
//		You should have received a copy of the GNU General Public License along																															//
//		with this program; if not, write to the Free Software Foundation, Inc.,																															//
//		51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301 USA.																																	//
//																																																		//
//======================================================================================================================================================================================================//

#ifndef SIMPLE_THREAD_POOLS_HPP
#define SIMPLE_THREAD_POOLS_HPP

#include <list>
#include <queue>
#include <optional>
#include <functional>
#include <future>
#include <shared_mutex>

namespace stp // SimpleThreadPools - version B.5.1.0
{
	namespace stpi // Implementation namespace (review names)
	{
		namespace stdx::meta // Modified version
		{
			// Container types

				// Packs

			template <class ...>
			struct pack
			{
				static constexpr size_t size = 0;
				template <class ... Types>
				using push = pack<Types ...>;
				template <size_t N>
				using pop = pack<>;
			};

			template <class Type>
			struct pack<Type>
			{
				static constexpr size_t size = 1;
				using first = Type;
				using last = Type;
				template <class ... Types>
				using push = pack<Type, Types ...>;
				template <size_t N>
				using pop = std::conditional_t<bool(N), pack<>, pack<Type>>;
			};

			template <class Type, class ... Types>
			struct pack<Type, Types ...>
			{
				static constexpr size_t size = 1 + sizeof...(Types);
				using first = Type;
				using last = typename pack<Types ...>::last;
				template <class ... Types1>
				using push = pack<Type, Types ..., Types1 ...>;
				template <size_t N>
				using pop = std::conditional_t<bool(N), typename pack<Types ...>::template pop<N - 1>, pack<Type, Types ...>>;
			};

				// Valpacks

			template <auto ...>
			struct valpack
			{
				static constexpr size_t size = 0;
				template <auto ... Values>
				using push = valpack<Values ...>;
				template <size_t N>
				using pop = pack<>;
			};

			template <auto Value>
			struct valpack<Value>
			{
				static constexpr size_t size = 1;
				static constexpr auto first = Value;
				static constexpr auto last = Value;
				template <auto ... Values>
				using push = valpack<Value, Values ...>;
				template <size_t N>
				using pop = std::conditional_t<bool(N), valpack<>, valpack<Value>>;
			};

			template <auto Value, auto ... Values>
			struct valpack<Value, Values ...>
			{
				static constexpr size_t size = 1 + sizeof...(Values);
				static constexpr auto first = Value;
				static constexpr auto last = valpack<Values ...>::last;
				template <auto ... Values1>
				using push = valpack<Value, Values ..., Values1 ...>;
				template <size_t N>
				using pop = std::conditional_t<bool(N), typename valpack<Values ...>::template pop<N - 1>, valpack<Value, Values ...>>;
			};

				// Pack modifiers (Specialized for stp)

			template <class InPack1, class InPack2, class OutPack>
			struct _placeholder_types : _placeholder_types<typename InPack1::template pop<1>, typename InPack2::template pop<1>, std::conditional_t<bool(std::is_placeholder_v<std::remove_cv_t<std::remove_reference_t<typename InPack1::first>>>), typename OutPack::template push<typename InPack2::first>, OutPack>>
			{
			};

			template <class OutPack>
			struct _placeholder_types<pack<>, pack<>, OutPack>
			{
				using _type = OutPack;
			};

			template <class InPack1, class InPack2>
			using placeholder_types = typename _placeholder_types<InPack1, InPack2, pack<>>::_type;

			template <class InPack, class OutPack>
			struct _placeholder_values : _placeholder_values<typename InPack::template pop<1>, std::conditional_t<bool(std::is_placeholder_v<std::remove_cv_t<std::remove_reference_t<typename InPack::first>>>), typename OutPack::template push<size_t(std::is_placeholder_v<std::remove_cv_t<std::remove_reference_t<typename InPack::first>>> - 1)>, OutPack>>
			{
			};

			template <class OutPack>
			struct _placeholder_values<pack<>, OutPack>
			{
				using _type = OutPack;
			};

			template <class InPack>
			using placeholder_values = typename _placeholder_values<InPack, valpack<>>::_type;

			template <class InPack, class OutPack, class IndexPack>
			struct _parameter_types : _parameter_types<InPack, typename OutPack::template push<typename InPack::template pop<IndexPack::first>::first>, typename IndexPack::template pop<1>>
			{
			};

			template <class InPack, class OutPack>
			struct _parameter_types<InPack, OutPack, valpack<>>
			{
				using _type = OutPack;
			};

			template <class InPack, class IndexPack>
			using parameter_types = typename _parameter_types<InPack, pack<>, IndexPack>::_type;

			// Function traits

				// Function signature

			template <class>
			struct _function_signature;

			template <class RetType, class ... ParamTypes>
			struct _function_signature<RetType(ParamTypes ...)>
			{
				using return_type = RetType;
				using parameter_types = pack<ParamTypes ...>;
			};

			template <class>
			struct function_signature;

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...)> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) &> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const &> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile &> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile &> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) &&> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const &&> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile &&> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile &&> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) & noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const & noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile & noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile & noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) && noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const && noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile && noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile && noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
			};

			template <class FuncType>
			struct function_signature<FuncType *> : function_signature<FuncType>
			{
			};

			template <class FuncType, class ObjType>
			struct function_signature<FuncType ObjType::*> : function_signature<FuncType>
			{
			};

				// Make function signature

			template <class>
			struct _make_function_signature;

			template <class RetType, class ... ParamTypes>
			struct _make_function_signature<pack<RetType, pack<ParamTypes ...>>>
			{
				using _type = RetType(ParamTypes ...);
			};

			template <class FuncType>
			using make_function_signature = typename _make_function_signature<FuncType>::_type;

			// Atomic traits (Note: this might be helpful when atomic_ptr and concurrent_queue are added)

			// Determines if built-in atomic type is lock-free, assuming that it is properly aligned

/*			template <class>
			struct _is_lock_free;

			template <class Type>
			struct _is_lock_free<std::atomic<Type>> : std::false_type
			{
			};

#ifdef ATOMIC_BOOL_LOCK_FREE
			template <>
			struct _is_lock_free<std::atomic<bool>> : std::bool_constant<bool(ATOMIC_BOOL_LOCK_FREE)>
			{
			};
#endif

#ifdef ATOMIC_CHAR_LOCK_FREE
			template <>
			struct _is_lock_free<std::atomic<char>> : std::bool_constant<bool(ATOMIC_CHAR_LOCK_FREE)>
			{
			};

			template <>
			struct _is_lock_free<std::atomic<unsigned char>> : std::bool_constant<bool(ATOMIC_CHAR_LOCK_FREE)>
			{
			};
#endif

#ifdef ATOMIC_CHAR16_T_LOCK_FREE
			template <>
			struct _is_lock_free<std::atomic<char16_t>> : std::bool_constant<bool(ATOMIC_CHAR16_T_LOCK_FREE)>
			{
			};
#endif

#ifdef ATOMIC_CHAR32_T_LOCK_FREE
			template <>
			struct _is_lock_free<std::atomic<char32_t>> : std::bool_constant<bool(ATOMIC_CHAR32_T_LOCK_FREE)>
			{
			};
#endif

#ifdef ATOMIC_WCHAR_T_LOCK_FREE
			template <>
			struct _is_lock_free<std::atomic<wchar_t>> : std::bool_constant<bool(ATOMIC_WCHAR_T_LOCK_FREE)>
			{
			};
#endif

#ifdef ATOMIC_SHORT_LOCK_FREE
			template <>
			struct _is_lock_free<std::atomic<short>> : std::bool_constant<bool(ATOMIC_SHORT_LOCK_FREE)>
			{
			};

			template <>
			struct _is_lock_free<std::atomic<unsigned short>> : std::bool_constant<bool(ATOMIC_SHORT_LOCK_FREE)>
			{
			};
#endif

#ifdef ATOMIC_INT_LOCK_FREE
			template <>
			struct _is_lock_free<std::atomic<int>> : std::bool_constant<bool(ATOMIC_INT_LOCK_FREE)>
			{
			};

			template <>
			struct _is_lock_free<std::atomic<unsigned int>> : std::bool_constant<bool(ATOMIC_INT_LOCK_FREE)>
			{
			};
#endif

#ifdef ATOMIC_LONG_LOCK_FREE
			template <>
			struct _is_lock_free<std::atomic<long>> : std::bool_constant<bool(ATOMIC_LONG_LOCK_FREE)>
			{
			};

			template <>
			struct _is_lock_free<std::atomic<unsigned long>> : std::bool_constant<bool(ATOMIC_LONG_LOCK_FREE)>
			{
			};
#endif

#ifdef ATOMIC_LLONG_LOCK_FREE
			template <>
			struct _is_lock_free<std::atomic<long long>> : std::bool_constant<bool(ATOMIC_LLONG_LOCK_FREE)>
			{
			};

			template <>
			struct _is_lock_free<std::atomic<unsigned long long>> : std::bool_constant<bool(ATOMIC_LLONG_LOCK_FREE)>
			{
			};
#endif

#ifdef ATOMIC_POINTER_LOCK_FREE
			template <class Type>
			struct _is_lock_free<std::atomic<Type *>> : std::bool_constant<bool(ATOMIC_POINTER_LOCK_FREE)>
			{
			};
#endif	*/
		}

		using namespace stdx::meta;

		namespace stdx::functional // Modified version
		{
			template <class ValType>
			auto forward(std::remove_reference_t<ValType> & val)
			{
				if constexpr (std::is_placeholder_v<std::remove_cv_t<std::remove_reference_t<ValType>>>)
				{
					return val;
				}
				else if constexpr (std::is_lvalue_reference_v<ValType>)
				{
					return std::ref(val);
				}
				else
				{
					return std::bind(std::move<ValType &>, std::move(val));
				}
			}

			template <class FuncType, class ... ArgTypes> // Version of bind that respects argument passing semantics 
			auto bind(FuncType * func, ArgTypes && ... args)
			{
				static_assert(std::is_function_v<FuncType>,
							  "'stdx::functional::bind(FuncType *, ArgTypes && ...)': "
							  "FuncType must be a function, a pointer to function, or a pointer to member function");
				return std::bind(func, forward<ArgTypes>(args) ...);
			}
			template <class FuncType, class ObjType, class ... ArgTypes>
			auto bind(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args)
			{
				static_assert(std::is_function_v<FuncType>,
							  "'stdx::functional::bind(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
							  "FuncType must be a function, a pointer to function, or a pointer to member function");
				return std::bind(func, obj, forward<ArgTypes>(args) ...);
			}
		}

		using namespace stdx::functional;

		template <class FuncType>
		using task_return_type = typename function_signature<FuncType>::return_type;

		template <class FuncType, class ... ArgTypes>
		using task_parameter_types = parameter_types<placeholder_types<pack<ArgTypes ...>, typename function_signature<FuncType>::parameter_types>, placeholder_values<pack<ArgTypes ...>>>;
	}

	// Task error class

	enum class task_error_code : uint_least8_t
	{
		no_state = 1,
		invalid_state,
		blocked_state,
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
				case task_error_code::blocked_state:
					return "deadlock state";
			}

			return "";
		}
	};

	// Task class

	enum class task_state : uint_least8_t
	{
		null,
		suspended,
		cancelling,
		waiting, 
		running,
		ready,
	};

	class task_priority
	{
	public:
		task_priority(int_least16_t default_priority = 0) :
			_task_priority_default(std::clamp<int_least16_t>(default_priority, _task_priority_minimum, _task_priority_maximum))
		{
		}
		task_priority(int_least16_t default_priority, int_least8_t minimum_priority, int_least8_t maximum_priority) :
			_task_priority_default(std::clamp<int_least16_t>(default_priority,
															 std::min(minimum_priority, maximum_priority),
															 std::max(minimum_priority, maximum_priority))),
			_task_priority_minimum(minimum_priority),
			_task_priority_maximum(maximum_priority)
		{
		}

		uint_least8_t operator()(int_least16_t priority) const
		{
			priority = (priority == std::numeric_limits<int_least16_t>::min() ?
						_task_priority_default :
						std::clamp<int_least16_t>(priority,
												  std::min(_task_priority_minimum, _task_priority_maximum),
												  std::max(_task_priority_minimum, _task_priority_maximum)));

			return uint_least8_t(std::abs(priority - _task_priority_minimum));
		}
	private:
		int_least16_t const _task_priority_default;
		int_least8_t const _task_priority_minimum{ std::numeric_limits<int_least8_t>::min() };
		int_least8_t const _task_priority_maximum{ std::numeric_limits<int_least8_t>::max() };
	};

	template <class>
	class _task; // Implementation class

	template <class>
	class task;

	template <class RetType, class ... ParamTypes> // TODO review memory order (should need no more than relaxed) / review behaviour for task_state::cancelling in wait() and get()
	class _task<RetType(ParamTypes ...)>
	{
		static_assert(sizeof(int_least8_t) != sizeof(int_least16_t), "Incompatible architecture"); // Consider a better way to check if priority is set
	protected:
		_task() = default;
		template <class FuncType, class ... ArgTypes>
		_task(FuncType * func, ArgTypes && ... args) :
			_task_package(stpi::bind(func, std::forward<ArgTypes>(args) ...)),
			_task_state(task_state::suspended)
		{
			static_assert(std::is_function_v<FuncType>,
						  "'stp::task<RetType, ParamTypes ...>::task(FuncType *, ArgTypes && ...)': "
						  "FuncType must be a function");
			static_assert(std::is_same_v<RetType, stpi::task_return_type<FuncType>>,
						  "'stp::task<RetType, ParamTypes ...>::task(FuncType *, ArgTypes && ...)': "
						  "RetType must be the same type as FuncType's return type");
			static_assert(std::is_same_v<stpi::pack<ParamTypes ...>, stpi::task_parameter_types<FuncType, ArgTypes ...>>,
						  "'stp::task<RetType, ParamTypes ...>::task(FuncType *, ArgTypes && ...)': "
						  "ParamTypes must be the same types as FuncType's parameter types corresponding to the placeholder ArgTypes");
		}
		template <class FuncType, class ObjType, class ... ArgTypes>
		_task(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args) :
			_task_package(stpi::bind(func, obj, std::forward<ArgTypes>(args) ...)),
			_task_state(task_state::suspended)
		{
			static_assert(std::is_function_v<FuncType>,
						  "'stp::task<RetType, ParamTypes ...>::task(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
						  "FuncType must be a function");
			static_assert(std::is_same_v<RetType, stpi::task_return_type<FuncType>>,
						  "'stp::task<RetType, ParamTypes ...>::task(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
						  "RetType must be the same type as the FuncType return type");
			static_assert(std::is_same_v<stpi::pack<ParamTypes ...>, stpi::task_parameter_types<FuncType, ArgTypes ...>>,
						  "'stp::task<RetType, ParamTypes ...>::task(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
						  "ParamTypes must be the same types as the FuncType parameter types corresponding to the placeholder ArgTypes");
		}
		_task(_task && other) :
			_task_package(std::move(other._task_package)),
			_task_future(std::move(other._task_future)),
			_task_state(other._task_state.exchange(task_state::null, std::memory_order_relaxed)),
			_task_priority(other._task_priority)
		{
		}
		_task & operator=(_task && other)
		{
			_wait();

			_task_package = std::exchange(other._task_package, std::packaged_task<RetType(ParamTypes ...)>());
			_task_future = std::exchange(other._task_future, std::future<void>());
			_task_state.store(other._task_state.exchange(task_state::null, std::memory_order_relaxed));
			_task_priority = other._task_priority;

			return *this;
		}
		~_task()
		{
			_wait();
		}
	public:
		RetType operator()(ParamTypes && ... args)
		{
			function(std::forward<ParamTypes>(args) ...)();

			return static_cast<task<RetType(ParamTypes ...)> *>(this)->get();
		}

		task_state cancel()
		{
			auto state = task_state::waiting;

			_task_state.compare_exchange_strong(state, task_state::cancelling, std::memory_order_relaxed);

			return state;
		}
		void wait() const
		{
			switch (_task_state.load(std::memory_order_relaxed))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
					throw task_error(task_error_code::blocked_state);
				case task_state::cancelling:
					throw task_error(task_error_code::invalid_state);
				case task_state::waiting:
				case task_state::running:
					_task_future.wait();
				default:
					break;
			}
		}
		template <class Rep, class Period>
		task_state wait_for(std::chrono::duration<Rep, Period> const & duration) const
		{
			return wait_until(std::chrono::steady_clock::now() + duration);
		}
		template <class Clock, class Duration>
		task_state wait_until(std::chrono::time_point<Clock, Duration> const & time_point) const
		{
			switch (_task_state.load(std::memory_order_relaxed))
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
		int_least16_t get_priority()
		{
			return _task_priority;
		}
		void set_priority(int_least8_t priority)
		{
			_task_priority = priority;
		}
		[[nodiscard]] std::packaged_task<void()> function(ParamTypes && ... args) & // WARNING: The destructor for the task will block until operator() has been called for the object returned by this function
		{
			switch (_task_state.load(std::memory_order_relaxed))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::cancelling:
				case task_state::waiting:
				case task_state::running:
				case task_state::ready:
					throw task_error(task_error_code::invalid_state);
				default:
					break;
			}

			std::packaged_task<void()> function(stpi::bind(&_task<RetType(ParamTypes ...)>::_execute, this, std::forward<ParamTypes>(args) ...));

			_task_future = function.get_future();

			_task_state.store(task_state::waiting, std::memory_order_relaxed);

			return function;
		}
	protected:
		_task && _move()
		{
			_wait();

			return std::move(*this);
		}
		void _reset()
		{
			_wait();

			if (_task_package.valid())
			{
				_task_package.reset();
				_task_future = std::future<void>();
				_task_state.store(task_state::suspended, std::memory_order_relaxed);
			}
		}
	private:
		void _execute(ParamTypes && ... args) // Used in function
		{
			auto state = task_state::waiting;

			if (_task_state.compare_exchange_strong(state, task_state::running, std::memory_order_relaxed))
			{
				_task_package(std::forward<ParamTypes>(args) ...);

				_task_state.store(task_state::ready, std::memory_order_relaxed);
			}
			else
			{
				_task_state.store(task_state::suspended, std::memory_order_relaxed);
			}
		}
		void _wait() // Used in move constructor / assignment operator, destructor, and reset function
		{
			switch (_task_state.load(std::memory_order_relaxed))
			{
				case task_state::cancelling:
				case task_state::waiting:
				case task_state::running:
					_task_future.wait();
				default:
					break;
			}
		}
	protected:
		std::packaged_task<RetType(ParamTypes ...)> _task_package;
		std::future<void> _task_future;
		std::atomic<task_state> _task_state{ task_state::null };
		int_least16_t _task_priority{ std::numeric_limits<int_least16_t>::min() }; // TODO Define this better (to get rid of the assertion)
	};

	template <class RetType, class ... ParamTypes>
	class task<RetType(ParamTypes ...)> : public _task<RetType(ParamTypes ...)>
	{
		static_assert(std::negation_v<std::is_rvalue_reference<RetType>>, "stp::task<RetType(ParamTypes ...)>: RetType cannot be a rvalue-reference");

		using ResultType = std::optional<std::conditional_t<std::negation_v<std::is_reference<RetType>>, RetType, std::reference_wrapper<std::remove_reference_t<RetType>>>>;
	public:
		task() = default;
		template <class FuncType, class ... ArgTypes>
		task(FuncType * func, ArgTypes && ... args) :
			_task<RetType(ParamTypes ...)>(func, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class FuncType, class ObjType, class ... ArgTypes>
		task(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args) : 
			_task<RetType(ParamTypes ...)>(func, obj, std::forward<ArgTypes>(args) ...)
		{
		}
		task(task && other) : 
			_task<RetType(ParamTypes ...)>(other._move()),
			_task_result(std::move(other._task_result))
		{
		}
		task & operator=(task && other)
		{
			_task<RetType(ParamTypes ...)>::operator=(other._move());

			_task_result = std::move(other._task_result); // std::optional move assignment operator should protect against self-assignment

			return *this;
		}

		std::add_lvalue_reference_t<RetType> get()
		{
			switch (this->_task_state.load(std::memory_order_relaxed))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
					throw task_error(task_error_code::blocked_state);
				case task_state::cancelling:
					throw task_error(task_error_code::invalid_state);
				default:
					break;
			}

			if (this->_task_future.valid())
			{
				this->_task_future.get();

				try
				{
					_task_result.emplace(std::forward<RetType>(this->_task_package.get_future().get()));
				}
				catch (...)
				{
					reset();

					throw;
				}
			}

			return _task_result.value();
		}
		void reset()
		{
			this->_reset();

			_task_result.reset();
		}
	private:
		ResultType _task_result;
	};

	template <class ... ParamTypes>
	class task<void(ParamTypes ...)> : public _task<void(ParamTypes ...)>
	{
	public:
		task() = default;
		template <class FuncType, class ... ArgTypes>
		task(FuncType * func, ArgTypes && ... args) :
			_task<void(ParamTypes ...)>(func, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class FuncType, class ObjType, class ... ArgTypes>
		task(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args) :
			_task<void(ParamTypes ...)>(func, obj, std::forward<ArgTypes>(args) ...)
		{
		}
		task(task && other) : 
			_task<void(ParamTypes ...)>(other._move())
		{
		}
		task & operator=(task && other)
		{
			_task<void(ParamTypes ...)>::operator=(other._move());

			return *this;
		}

		void get()
		{
			switch (this->_task_state.load(std::memory_order_relaxed))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
					throw task_error(task_error_code::blocked_state);
				case task_state::cancelling:
					throw task_error(task_error_code::invalid_state);
				default:
					break;
			}

			if (this->_task_future.valid())
			{
				this->_task_future.get();

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
	};

	template <class FuncType, class ... ArgTypes>
	task(FuncType *, ArgTypes ...) -> task<stpi::make_function_signature<stpi::pack<stpi::task_return_type<FuncType>, stpi::task_parameter_types<FuncType, ArgTypes ...>>>>;
	template <class FuncType, class ObjType, class ... ArgTypes>
	task(FuncType ObjType::*, ObjType *, ArgTypes ...) -> task<stpi::make_function_signature<stpi::pack<stpi::task_return_type<FuncType>, stpi::task_parameter_types<FuncType, ArgTypes ...>>>>;

	template <class FuncType, class ... ArgTypes>
	inline auto make_task(FuncType * func, ArgTypes && ... args) // Allows disambiguation of function overload by specifying the parameters' types, automatically deduces type of task based on placeholder arguments, and allows construction of task with pre-set priority
	{
		return task(func, std::forward<ArgTypes>(args) ...);
	}
	template <class FuncType, class ObjType, class ... ArgTypes>
	inline auto make_task(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args)
	{
		return task(func, obj, std::forward<ArgTypes>(args) ...);
	}
	template <class FuncType, class ... ArgTypes>
	inline auto make_task(int_least8_t prty, FuncType * func, ArgTypes && ... args)
	{
		task t(func, std::forward<ArgTypes>(args) ...);
		t.set_priority(prty);
		return t;
	}
	template <class FuncType, class ObjType, class ... ArgTypes>
	inline auto make_task(int_least8_t prty, FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args)
	{
		task t(func, obj, std::forward<ArgTypes>(args) ...);
		t.set_priority(prty);
		return t;
	}

	// Threadpool class

	enum class threadpool_state : uint_least8_t
	{
		terminating,
		stopped,
		running,
	};

	class threadpool // WIP monitor paradigm
	{
	public:
		threadpool(task_priority priority,
				   size_t size = std::thread::hardware_concurrency(),
				   threadpool_state state = threadpool_state::running) :
			_threadpool_priority(priority),
			_threadpool_size(size),
			_threadpool_state(state)/*/,
			_threadpool_monitor(&threadpool::_monitor, this)
		{
		/*/
		{
			for (size_t n = 0; n < _threadpool_size; ++n)
			{
				_threadpool_thread_list.emplace_back(this);
			}
		//*/
		}
		threadpool(size_t size = std::thread::hardware_concurrency(),
				   threadpool_state state = threadpool_state::running) :
			threadpool(task_priority(), size, state)
		{
		}
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			_threadpool_mutex.lock();

			_threadpool_state = threadpool_state::terminating;

			_threadpool_condvar.notify_all();

			_threadpool_mutex.unlock();
			/**/
			for (auto & thread : _threadpool_thread_list)
			{
				if (thread.active)
				{
					thread.thread.join();
				}
			}
			/*/
			_threadpool_monitor.join();
			//*/
		}

		template <class RetType, class ... ParamTypes>
		void execute(task<RetType(ParamTypes ...)> & task, ParamTypes && ... args)
		{
			auto priority = _threadpool_priority(task.get_priority());

			auto function = task.function(std::forward<ParamTypes>(args) ...);

			std::scoped_lock<std::mutex> lock(_threadpool_queue_mutex);

			_threadpool_task_queue.emplace(std::move(function), priority);

			_threadpool_task.store(true, std::memory_order_relaxed);

			_threadpool_condvar.notify_one();
		}
		void resize([[maybe_unused]] size_t size) // TODO move logic to monitor (probably to function called _resize), send message to monitor through queue
		{
	/*		if (_threadpool_size != size)
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
								it->thread = std::thread(&threadpool::_pool, this, &*it);
								
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
			} */
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
		size_t size() const
		{
			return _threadpool_size;
		}
		threadpool_state state() const
		{
			return _threadpool_state;
		}
	private:
		struct _message // TODO messages for monitor thread
		{
		};
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
		struct _thread // TODO change member variables according to needs (inactive might be unnecessary), add time_point to indicate availability (heartbeats)
		{
			_thread(threadpool * threadpool) :
				thread(&threadpool::_pool, threadpool, this)
			{
			}

			_task task;
			bool active{ true };
			bool inactive{ false };
			std::thread thread; // Must be the last variable to be initialized
		};

		void _monitor()
		{
			std::deque<_thread> threads;

			for (size_t n = 0; n < _threadpool_size; ++n)
			{
				threads.emplace_back(this);
			}
		}

		void _pool(_thread * this_thread)
		{
			std::shared_lock<std::shared_mutex> threadpool_lock(_threadpool_mutex);

			while (_threadpool_state != threadpool_state::terminating && this_thread->active)
			{
				if (_threadpool_state == threadpool_state::running && _threadpool_task.load(std::memory_order_relaxed))
				{
					threadpool_lock.unlock();

					if (std::scoped_lock<std::mutex> lock(_threadpool_queue_mutex); _threadpool_task.load(std::memory_order_relaxed))
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

		task_priority const _threadpool_priority;
		size_t _threadpool_size;
		threadpool_state _threadpool_state;
		std::atomic_bool _threadpool_task{ false }; // Replace by concurrent_queue when done
		std::priority_queue<_task, std::deque<_task>> _threadpool_task_queue; // Replace by concurrent_queue when done
		std::queue<_message> _threadpool_message_queue; // Replace by concurrent_queue when done
		std::list<_thread> _threadpool_thread_list; // To be removed
		std::thread _threadpool_monitor;
		std::mutex _threadpool_queue_mutex; // Replace by concurrent_queue when done
		std::shared_mutex _threadpool_mutex;
		std::condition_variable_any _threadpool_condvar;
	};
}

#endif
