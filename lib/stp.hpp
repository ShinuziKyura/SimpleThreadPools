//======================================================================================================================================================================================================//
//																																																		//
//		SimpleThreadPools, a library that implements a threadpool class and related utilities using the C++ Standard Library																			//	
//		Copyright(C) 2018 Ricardo Santos																																								//
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

#include <any>
#include <optional>
#include <queue>
#include <functional>
#include <future>
#include <shared_mutex>

namespace stp // SimpleThreadPools - Library namespace - Version B.5.3.0
{
	namespace _stp // SimpleThreadPools - Implementation namespace
	{
		// Meta

		namespace meta
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
				using pop = valpack<>;
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

			// Common type

			template <class>
			struct common_type
			{
				using type = std::any;
			};

			template <class Type, class ... Types>
			struct common_type<pack<Type, Types ...>>
			{
				using type = std::common_type_t<Type, Types ...>;
			};

			template <class Pack>
			using common_type_t = typename common_type<Pack>::type;

			// Convertible types

			template <class ... FromTypes, class ... ToTypes>
			constexpr bool _convertible_types(pack<FromTypes ...>, pack<ToTypes ...>)
			{
				return (... && (std::is_placeholder_v<std::remove_cv_t<std::remove_reference_t<FromTypes>>> || std::is_convertible_v<FromTypes, ToTypes>));
			}

			template <class, class>
			struct convertible_types;

			template <class ... FromTypes, class ... ToTypes>
			struct convertible_types<pack<FromTypes ...>, pack<ToTypes ...>>
			{
				static constexpr bool value = _convertible_types(pack<FromTypes ...>(), pack<ToTypes ...>());
			};

			template <class FromPack, class ToPack>
			constexpr bool convertible_types_v = convertible_types<FromPack, ToPack>::value;

			// Container types, specialized for stp

			// Placeholder types

			template <class InPack1, class InPack2, class OutPack>
			struct _placeholder_types :
				_placeholder_types<
					typename InPack1::template pop<1>,
					typename InPack2::template pop<1>,
					std::conditional_t<bool(std::is_placeholder_v<std::remove_cv_t<std::remove_reference_t<typename InPack1::first>>>), typename OutPack::template push<typename InPack2::first>, OutPack>
				>
			{
			};

			template <class OutPack>
			struct _placeholder_types<pack<>, pack<>, OutPack>
			{
				using _type = OutPack;
			};

			template <class InPack1, class InPack2>
			using placeholder_types = typename _placeholder_types<InPack1, InPack2, pack<>>::_type;

			// Placeholder values

			template <class InPack, class OutPack>
			struct _placeholder_values :
				_placeholder_values<
					typename InPack::template pop<1>,
					std::conditional_t<bool(std::is_placeholder_v<std::remove_cv_t<std::remove_reference_t<typename InPack::first>>>), typename OutPack::template push<std::is_placeholder_v<std::remove_cv_t<std::remove_reference_t<typename InPack::first>>>>, OutPack>
				>
			{
			};

			template <class OutPack>
			struct _placeholder_values<pack<>, OutPack>
			{
				using _type = OutPack;
			};

			template <class InPack>
			using placeholder_values = typename _placeholder_values<InPack, valpack<>>::_type;

			// Make placeholder parameters

			template <class InPack1, class InPack2, class OutPack1, class OutPack2, size_t Index>
			struct _make_placeholder_parameters :
				_make_placeholder_parameters<
					std::conditional_t<bool(InPack2::first != Index), typename InPack1::template pop<1>::template push<typename InPack1::first>, typename InPack1::template pop<1>>,
					std::conditional_t<bool(InPack2::first != Index), typename InPack2::template pop<1>::template push<InPack2::first>, typename InPack2::template pop<1>>,
					OutPack1,
					std::conditional_t<bool(InPack2::first != Index), OutPack2, typename OutPack2::template push<typename InPack1::first>>,
					Index
				>
			{
			};

			template <class ... InTypes, auto ... InValues, class OutPack1, class OutPack2, size_t Index>
			struct _make_placeholder_parameters<pack<void, InTypes ...>, valpack<0, InValues ...>, OutPack1, OutPack2, Index> :
				_make_placeholder_parameters<
					pack<InTypes ..., void>,
					valpack<InValues ..., 0>,
					typename OutPack1::template push<common_type_t<OutPack2>>,
					pack<>,
					Index + 1
				>
			{
			};

			template <class OutPack1, class OutPack2, size_t Index>
			struct _make_placeholder_parameters<pack<void>, valpack<0>, OutPack1, OutPack2, Index>
			{
				using _type = std::conditional_t<bool(OutPack2::size), typename OutPack1::template push<common_type_t<OutPack2>>, OutPack1>;
			};

			template <class InPack1, class InPack2>
			using make_placeholder_parameters = typename _make_placeholder_parameters<typename InPack1::template push<void>, typename InPack2::template push<0>, pack<>, pack<>, 1>::_type;

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

			template <class, class>
			struct _make_function_signature;

			template <class RetType, class ... ParamTypes>
			struct _make_function_signature<RetType, pack<ParamTypes ...>>
			{
				using _type = RetType(ParamTypes ...);
			};

			template <class RetType, class ParamTypes>
			using make_function_signature = typename _make_function_signature<RetType, ParamTypes>::_type;
		}

		template <class FuncType>
		using task_return_type = typename meta::function_signature<FuncType>::return_type;

		template <class FuncType, class ... ArgTypes>
		using task_parameter_types = meta::make_placeholder_parameters<meta::placeholder_types<meta::pack<ArgTypes ...>, typename meta::function_signature<FuncType>::parameter_types>, meta::placeholder_values<meta::pack<ArgTypes ...>>>;

		template <class FuncType, class ... ArgTypes>
		using task_function_type = meta::make_function_signature<_stp::task_return_type<FuncType>, _stp::task_parameter_types<FuncType, ArgTypes ...>>;

		// Functional

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

		template <class FuncType, class ... ArgTypes>
		auto bind(FuncType * func, ArgTypes && ... args)
		{
			static_assert(std::is_function_v<FuncType>,
						  "'_stp::bind(FuncType *, ArgTypes && ...)': "
						  "FuncType must be a function, a pointer to function, or a pointer to member function");
			return std::bind(func, _stp::forward<ArgTypes>(args) ...);
		}
		template <class FuncType, class ObjType, class ... ArgTypes>
		auto bind(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args)
		{
			static_assert(std::is_function_v<FuncType>,
						  "'_stp::bind(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
						  "FuncType must be a function, a pointer to function, or a pointer to member function");
			return std::bind(func, obj, _stp::forward<ArgTypes>(args) ...);
		}
	}

	// Task error class

	enum class task_error_code : uint_least8_t
	{
		no_state = 1,
		invalid_state,
		deadlock_state,
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
				case task_error_code::deadlock_state:
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
		cancelled, // Currently unused
		cancelling,
		waiting,
		executing, // Currently unused, will replace 'running'
		executed, // Currently unused, will replace 'ready'
		running,
		ready,
	};

	// Task priority

	inline int_least8_t default_task_priority = 0;

	template <class>
	class _task; // Implementation class

	template <class RetType, class ... ParamTypes>
	class _task<RetType(ParamTypes ...)>
	{
	protected:
		_task() = default;
		template <class FuncType, class ... ArgTypes>
		_task(FuncType * func, ArgTypes && ... args) :
			_task_package(_stp::bind(func, std::forward<ArgTypes>(args) ...)),
			_task_state(task_state::suspended)
		{
			static_assert(std::is_function_v<FuncType>,
						  "'stp::task<RetType(ParamTypes ...)>::task(FuncType *, ArgTypes && ...)': "
						  "FuncType must be a function type");
			static_assert(std::is_same_v<RetType, _stp::task_return_type<FuncType>>,
						  "'stp::task<RetType(ParamTypes ...)>::task(FuncType *, ArgTypes && ...)': "
						  "RetType must be of the same type as FuncType's return type");
			static_assert(_stp::meta::convertible_types_v<_stp::meta::pack<ArgTypes ...>, typename _stp::meta::function_signature<FuncType>::parameter_types>,
						  "'stp::task<RetType(ParamTypes ...)>::task(FuncType *, ArgTypes && ...)': "
						  "ArgTypes must be of the same types as FuncType's parameters types");
			static_assert(std::is_same_v<_stp::meta::pack<ParamTypes ...>, _stp::task_parameter_types<FuncType, ArgTypes ...>>,
						  "'stp::task<RetType(ParamTypes ...)>::task(FuncType *, ArgTypes && ...)': "
						  "ParamTypes must be of the same types as FuncType's parameters corresponding to ArgTypes' placeholder types");
		}
		template <class FuncType, class ObjType, class ... ArgTypes>
		_task(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args) :
			_task_package(_stp::bind(func, obj, std::forward<ArgTypes>(args) ...)),
			_task_state(task_state::suspended)
		{
			static_assert(std::is_function_v<FuncType>,
						  "'stp::task<RetType(ParamTypes ...)>::task(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
						  "FuncType must be a function type");
			static_assert(std::is_same_v<RetType, _stp::task_return_type<FuncType>>,
						  "'stp::task<RetType(ParamTypes ...)>::task(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
						  "RetType must be of the same type as FuncType's return type");
			static_assert(_stp::meta::convertible_types_v<_stp::meta::pack<ArgTypes ...>, typename _stp::meta::function_signature<FuncType>::parameter_types>,
						  "'stp::task<RetType(ParamTypes ...)>::task(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
						  "ArgTypes must be of the same types as FuncType's parameters types");
			static_assert(std::is_same_v<_stp::meta::pack<ParamTypes ...>, _stp::task_parameter_types<FuncType, ArgTypes ...>>,
						  "'stp::task<RetType(ParamTypes ...)>::task(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
						  "ParamTypes must be of the same types as FuncType's parameters corresponding to ArgTypes' placeholder types");
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
		void operator()(ParamTypes && ... args)
		{
			function(std::forward<ParamTypes>(args) ...)();
		}

		task_state cancel()
		{
			auto state = task_state::waiting;

			return _task_state.compare_exchange_strong(state, task_state::cancelling, std::memory_order_relaxed) ? task_state::cancelling : state;
		}
		void wait() const
		{
			switch (_task_state.load(std::memory_order_relaxed))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
					throw task_error(task_error_code::deadlock_state);
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
		int_least8_t get_priority()
		{
			return _task_priority;
		}
		void set_priority(int_least8_t priority)
		{
			_task_priority = priority;
		}
		// WARNING: stp::task<FuncType>::~task() will block until operator() has been called for the object returned by this function
		[[nodiscard]] std::packaged_task<void()> function(ParamTypes && ... args) &
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

			std::packaged_task<void()> function(_stp::bind(&_task<RetType(ParamTypes ...)>::_execute, this, std::forward<ParamTypes>(args) ...));

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
		void _execute(ParamTypes && ... args) // Used in function()
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
		void _wait() // Used in move constructor, move assignment operator, destructor, and reset() function
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
		int_least8_t _task_priority{ default_task_priority };
	};

	template <class>
	class task;

	template <class RetType, class ... ParamTypes>
	class task<RetType(ParamTypes ...)> : public _task<RetType(ParamTypes ...)>
	{
		static_assert(!std::is_rvalue_reference_v<RetType>,
					  "'stp::task<RetType(ParamTypes ...)>': RetType cannot be a rvalue-reference");
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

			_task_result = std::move(other._task_result);

			return *this;
		}

		std::add_lvalue_reference_t<RetType> get()
		{
			switch (this->_task_state.load(std::memory_order_relaxed))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
					throw task_error(task_error_code::deadlock_state);
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

		operator std::add_lvalue_reference_t<RetType>()
		{
			return get();
		}
	private:
		std::optional<
			std::conditional_t<
				std::is_reference_v<RetType>,
				std::reference_wrapper<std::remove_reference_t<RetType>>,
				RetType
			>
		> _task_result;
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
					throw task_error(task_error_code::deadlock_state);
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
	task(FuncType *, ArgTypes ...) -> task<_stp::task_function_type<FuncType, ArgTypes ...>>;
	template <class FuncType, class ObjType, class ... ArgTypes>
	task(FuncType ObjType::*, ObjType *, ArgTypes ...) -> task<_stp::task_function_type<FuncType, ArgTypes ...>>;

	// Allows disambiguation of function overload by specifying the parameters' types
	template <class FuncType, class ... ArgTypes>
	inline auto make_task(FuncType * func, ArgTypes && ... args)
	{
		return task(func, std::forward<ArgTypes>(args) ...);
	}
	// Allows disambiguation of function overload by specifying the parameters' types
	template <class FuncType, class ObjType, class ... ArgTypes>
	inline auto make_task(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args)
	{
		return task(func, obj, std::forward<ArgTypes>(args) ...);
	}

	// Threadpool class

	enum class threadpool_state : uint_least8_t
	{
		terminating,
		stopped,
		running,
	};

	class threadpool
	{
	public:
		threadpool(size_t size = std::thread::hardware_concurrency(),
				   threadpool_state state = threadpool_state::running) :
			_threadpool_size(size),
			_threadpool_state(state)
		{
			for (size_t n = 0; n < size; ++n)
			{
				_threadpool_thread_array.emplace_back().launch(this);
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

			for (auto & thread : _threadpool_thread_array)
			{
				if (thread.active)
				{
					thread.thread.join();
				}
			}
		}

		template <class RetType, class ... ParamTypes>
		void execute(task<RetType(ParamTypes ...)> & task, ParamTypes && ... args)
		{
			std::scoped_lock<std::mutex> lock(_threadpool_queue_mutex);

			_threadpool_task_queue.emplace(task.function(std::forward<ParamTypes>(args) ...), task.get_priority());
			_threadpool_has_task.store(true, std::memory_order_relaxed);

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
					auto it = std::begin(_threadpool_thread_array), it_e = std::end(_threadpool_thread_array);
					for (size_t n = 0; n < delta_size; ++it)
					{
						if (it != it_e)
						{
							if (it->inactive)
							{
								it->launch(this);
								
								++n;
							}
						}
						else
						{
							while (n < delta_size)
							{
								_threadpool_thread_array.emplace_back().launch(this);

								++n;
							}
						}
					}
				}
				else
				{
					auto it_b = std::begin(_threadpool_thread_array), it_e = std::end(_threadpool_thread_array), it = it_b;
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
		size_t size() const
		{
			return _threadpool_size;
		}
		threadpool_state state() const
		{
			return _threadpool_state;
		}
	private:
		struct _task
		{
			_task(std::packaged_task<void()> func = std::packaged_task<void()>(), int_least8_t prty = 0) :
				function(std::move(func)),
				priority(prty),
				origin(std::chrono::steady_clock::now())
			{
			}

			bool operator<(_task const & other) const
			{
				return priority != other.priority ? priority < other.priority : origin > other.origin;
			}

			std::packaged_task<void()> function;
			int_least8_t priority;
			std::chrono::steady_clock::time_point origin;
		};
		struct _thread
		{
			void launch(threadpool * threadpool)
			{
				active = true;
				inactive = false;
				thread = std::thread(&threadpool::_pool, threadpool, this);
			}

			_task task;
			bool active;
			bool inactive;
			std::thread thread;
		};

		void _pool(_thread * this_thread)
		{
			std::shared_lock<std::shared_mutex> threadpool_lock(_threadpool_mutex);

			while (_threadpool_state != threadpool_state::terminating && this_thread->active)
			{
				if (_threadpool_state == threadpool_state::running && _threadpool_has_task.load(std::memory_order_relaxed))
				{
					threadpool_lock.unlock();

					if (std::scoped_lock<std::mutex> lock(_threadpool_queue_mutex); _threadpool_has_task.load(std::memory_order_relaxed))
					{
						this_thread->task = const_cast<_task &&>(_threadpool_task_queue.top());

						_threadpool_task_queue.pop();

						if (_threadpool_task_queue.empty())
						{
							_threadpool_has_task.store(false, std::memory_order_relaxed);
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

		size_t _threadpool_size;
		threadpool_state _threadpool_state;

		std::priority_queue<_task, std::deque<_task>> _threadpool_task_queue;
		std::mutex _threadpool_queue_mutex;
		std::atomic_bool _threadpool_has_task{ false };

		std::deque<_thread> _threadpool_thread_array;
		std::shared_mutex _threadpool_mutex;
		std::condition_variable_any _threadpool_condvar;
	};
}

#endif
