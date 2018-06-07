#ifndef SIMPLE_THREAD_POOLS_HPP
#define SIMPLE_THREAD_POOLS_HPP

#include <optional>
#include <list>
#include <queue>
#include <functional>
#include <future>
#include <shared_mutex>

namespace stp // SimpleThreadPools - version B.5.0.0
{
	template <int N>
	struct _placeholder
	{
	};

	namespace placeholders
	{
		inline constexpr _placeholder<1> _1;
		inline constexpr _placeholder<2> _2;
		inline constexpr _placeholder<3> _3;
		inline constexpr _placeholder<4> _4;
		inline constexpr _placeholder<5> _5;
		inline constexpr _placeholder<6> _6;
		inline constexpr _placeholder<7> _7;
		inline constexpr _placeholder<8> _8;
		inline constexpr _placeholder<9> _9;
		inline constexpr _placeholder<10> _10;
		inline constexpr _placeholder<11> _11;
		inline constexpr _placeholder<12> _12;
		inline constexpr _placeholder<13> _13;
		inline constexpr _placeholder<14> _14;
		inline constexpr _placeholder<15> _15;
		inline constexpr _placeholder<16> _16;
		inline constexpr _placeholder<17> _17;
		inline constexpr _placeholder<18> _18;
		inline constexpr _placeholder<19> _19;
		inline constexpr _placeholder<20> _20;
		inline constexpr _placeholder<21> _21;
		inline constexpr _placeholder<22> _22;
		inline constexpr _placeholder<23> _23;
		inline constexpr _placeholder<24> _24;
		inline constexpr _placeholder<25> _25;
		inline constexpr _placeholder<26> _26;
		inline constexpr _placeholder<27> _27;
		inline constexpr _placeholder<28> _28;
		inline constexpr _placeholder<29> _29;
		inline constexpr _placeholder<30> _30;
		inline constexpr _placeholder<31> _31;
		inline constexpr _placeholder<32> _32;
		inline constexpr _placeholder<33> _33;
		inline constexpr _placeholder<34> _34;
		inline constexpr _placeholder<35> _35;
		inline constexpr _placeholder<36> _36;
		inline constexpr _placeholder<37> _37;
		inline constexpr _placeholder<38> _38;
		inline constexpr _placeholder<39> _39;
		inline constexpr _placeholder<40> _40;
		inline constexpr _placeholder<41> _41;
		inline constexpr _placeholder<42> _42;
		inline constexpr _placeholder<43> _43;
		inline constexpr _placeholder<44> _44;
		inline constexpr _placeholder<45> _45;
		inline constexpr _placeholder<46> _46;
		inline constexpr _placeholder<47> _47;
		inline constexpr _placeholder<48> _48;
		inline constexpr _placeholder<49> _49;
		inline constexpr _placeholder<50> _50;
		inline constexpr _placeholder<51> _51;
		inline constexpr _placeholder<52> _52;
		inline constexpr _placeholder<53> _53;
		inline constexpr _placeholder<54> _54;
		inline constexpr _placeholder<55> _55;
		inline constexpr _placeholder<56> _56;
		inline constexpr _placeholder<57> _57;
		inline constexpr _placeholder<58> _58;
		inline constexpr _placeholder<59> _59;
		inline constexpr _placeholder<60> _60;
		inline constexpr _placeholder<61> _61;
		inline constexpr _placeholder<62> _62;
		inline constexpr _placeholder<63> _63;
		inline constexpr _placeholder<64> _64;
		inline constexpr _placeholder<65> _65;
		inline constexpr _placeholder<66> _66;
		inline constexpr _placeholder<67> _67;
		inline constexpr _placeholder<68> _68;
		inline constexpr _placeholder<69> _69;
		inline constexpr _placeholder<70> _70;
		inline constexpr _placeholder<71> _71;
		inline constexpr _placeholder<72> _72;
		inline constexpr _placeholder<73> _73;
		inline constexpr _placeholder<74> _74;
		inline constexpr _placeholder<75> _75;
		inline constexpr _placeholder<76> _76;
		inline constexpr _placeholder<77> _77;
		inline constexpr _placeholder<78> _78;
		inline constexpr _placeholder<79> _79;
		inline constexpr _placeholder<80> _80;
		inline constexpr _placeholder<81> _81;
		inline constexpr _placeholder<82> _82;
		inline constexpr _placeholder<83> _83;
		inline constexpr _placeholder<84> _84;
		inline constexpr _placeholder<85> _85;
		inline constexpr _placeholder<86> _86;
		inline constexpr _placeholder<87> _87;
		inline constexpr _placeholder<88> _88;
		inline constexpr _placeholder<89> _89;
		inline constexpr _placeholder<90> _90;
		inline constexpr _placeholder<91> _91;
		inline constexpr _placeholder<92> _92;
		inline constexpr _placeholder<93> _93;
		inline constexpr _placeholder<94> _94;
		inline constexpr _placeholder<95> _95;
		inline constexpr _placeholder<96> _96;
		inline constexpr _placeholder<97> _97;
		inline constexpr _placeholder<98> _98;
		inline constexpr _placeholder<99> _99;
		inline constexpr _placeholder<100> _100;
	}
}

namespace std
{
	template <int N>
	struct is_placeholder<stp::_placeholder<N>> : std::integral_constant<int, N>
	{
	};
}

namespace stp
{
	namespace _stp // Implementation namespace
	{
		namespace stdx::meta // Fundamental version
		{
			template <auto>
			struct val;

			template <class ...>
			struct pack;

			template <auto ...>
			struct valpack;

			// Type traits

			// Identity trait, provides type equal to the type which it was instantiated

			template <class Type>
			struct identity
			{
				using type = Type;
			};

			// Apply a trait that takes a type parameter to type aliases from Val and Pack

			template <template <class> class Trait>
			struct type_trait
			{
				template <class Val>
				using type = Trait<typename Val::type>;
				template <class Pack>
				using first = Trait<typename Pack::first>;
				template <class Pack>
				using last = Trait<typename Pack::last>;
			};

			// Apply a trait that takes a non-type parameter to static constexpr variables from Val and Valpack

			template <template <auto> class Trait>
			struct value_trait
			{
				template <class Val>
				using value = Trait<Val::value>;
				template <class Valpack>
				using first = Trait<Valpack::first>;
				template <class Valpack>
				using last = Trait<Valpack::last>;
			};

			// Class-template equality

			template <class, template <class ...> class>
			struct is_same_as : std::false_type
			{
			};

			template <template <class ...> class Template, class ... Parameters>
			struct is_same_as<Template<Parameters ...>, Template> : std::true_type
			{
			};

			template <class Class, template <class ...> class Template>
			inline constexpr bool is_same_as_v = is_same_as<Class, Template>::value;

			// Tag types for function_signature and make_function_signature

			struct signature_;
			struct signature_c;
			struct signature_v;
			struct signature_cv;
			struct signature_l;
			struct signature_cl;
			struct signature_vl;
			struct signature_cvl;
			struct signature_r;
			struct signature_cr;
			struct signature_vr;
			struct signature_cvr;
			struct signature_n;
			struct signature_cn;
			struct signature_vn;
			struct signature_cvn;
			struct signature_ln;
			struct signature_cln;
			struct signature_vln;
			struct signature_cvln;
			struct signature_rn;
			struct signature_crn;
			struct signature_vrn;
			struct signature_cvrn;

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
				using type = pack<RetType, signature_, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_c, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_v, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_cv, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) &> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_l, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const &> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_cl, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile &> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_vl, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile &> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_cvl, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) &&> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_r, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const &&> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_cr, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile &&> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_vr, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile &&> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_cvr, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_n, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_cn, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_vn, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_cvn, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) & noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_ln, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const & noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_cln, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile & noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_vln, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile & noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_cvln, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) && noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_rn, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const && noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_crn, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) volatile && noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_vrn, pack<ParamTypes ...>>;
			};

			template <class RetType, class ... ParamTypes>
			struct function_signature<RetType(ParamTypes ...) const volatile && noexcept> : _function_signature<RetType(ParamTypes ...)>
			{
				using type = pack<RetType, signature_cvrn, pack<ParamTypes ...>>;
			};

			template <class FuncType>
			struct function_signature<FuncType *> : function_signature<FuncType>
			{
			};

			template <class FuncType, class ObjType>
			struct function_signature<FuncType ObjType::*> : function_signature<FuncType>
			{
				using object_type = ObjType;
			};

			// Numeric traits

			// Subtraction

			template <auto Y>
			struct subtraction
			{
				template <auto X>
				using trait = std::integral_constant<decltype(X), X - Y>;
			};

			// Range

			template <auto Min, auto Max>
			struct between
			{
				using _auto = decltype(Min);
				template <_auto N>
				using trait = std::bool_constant<Min <= N && N <= Max>;
			};

			// Vals

			template <auto N>
			struct val
			{
				using type = decltype(N);
				static constexpr auto value = N;
			};

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
				using first = Type;
				using last = Type;
				static constexpr size_t size = 1;
				template <class ... Types>
				using push = pack<Type, Types ...>;
				template <size_t N>
				using pop = std::conditional_t<bool(N), pack<>, pack<Type>>;
			};

			template <class Type, class ... Types>
			struct pack<Type, Types ...>
			{
				using first = Type;
				using last = typename pack<Types ...>::last;
				static constexpr size_t size = 1 + sizeof...(Types);
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
				static constexpr auto first = Value;
				static constexpr auto last = Value;
				static constexpr size_t size = 1;
				template <auto ... Values>
				using push = valpack<Value, Values ...>;
				template <size_t N>
				using pop = std::conditional_t<bool(N), valpack<>, valpack<Value>>;
			};

			template <auto Value, auto ... Values>
			struct valpack<Value, Values ...>
			{
				static constexpr auto first = Value;
				static constexpr auto last = valpack<Values ...>::last;
				static constexpr size_t size = 1 + sizeof...(Values);
				template <auto ... Values1>
				using push = valpack<Value, Values ..., Values1 ...>;
				template <size_t N>
				using pop = std::conditional_t<bool(N), typename valpack<Values ...>::template pop<N - 1>, valpack<Value, Values ...>>;
			};

			// Create Val from static constexpr value

			template <class Type>
			struct as_val
			{
				using type = val<Type::value>;
			};

			// Convert from Pack of Vals to Valpack

			template <class>
			struct _as_valpack;

			template <auto ... Values>
			struct _as_valpack<valpack<Values ...>>
			{
				using type = valpack<Values ...>;
			};

			template <auto ... Values>
			struct _as_valpack<pack<val<Values> ...>>
			{
				using type = valpack<Values ...>;
			};

			template <>
			struct _as_valpack<pack<>>
			{
				using type = valpack<>;
			};

			template <class Pack>
			using as_valpack = typename _as_valpack<Pack>::type;

			// Convert from Valpack to Pack of Vals

			template <class>
			struct _as_pack_val;

			template <auto ... Values>
			struct _as_pack_val<pack<val<Values> ...>>
			{
				using type = pack<val<Values> ...>;
			};

			template <>
			struct _as_pack_val<pack<>>
			{
				using type = pack<>;
			};

			template <auto ... Values>
			struct _as_pack_val<valpack<Values ...>>
			{
				using type = pack<val<Values> ...>;
			};

			template <class Pack>
			using as_pack_val = typename _as_pack_val<Pack>::type;

			// Constrained pack, applies a trait to each element of a pack, constructing a new pack with the elements of the old one for which bool(trait<element>::value) == true

			template <template <class> class Trait, class InPack, class OutPack>
			struct _constrained_pack : _constrained_pack<Trait, typename InPack::template pop<1>, std::conditional_t<bool(Trait<typename InPack::first>::value), typename OutPack::template push<typename InPack::first>, OutPack>>
			{
			};

			template <template <class> class Trait, class OutPack>
			struct _constrained_pack<Trait, pack<>, OutPack>
			{
				using type = OutPack;
			};

			template <template <class> class Trait, class InPack>
			struct _assert_constrained_pack : _constrained_pack<Trait, InPack, pack<>>
			{
				static_assert(is_same_as_v<InPack, pack>,
							  "'stdx::meta::constrained_pack<Trait, InPack>': "
							  "InPack must be of type stdx::meta::pack<T ...> where T is any type parameter");
			};

			template <template <class> class Trait, class InPack>
			using constrained_pack = typename _assert_constrained_pack<Trait, InPack>::type;

			// Transformed pack, applies a trait to each element of a pack, constructing a new pack of the same size as the old one and with the corresponding elements as trait<element>::type

			template <template <class> class Trait, class InPack, class OutPack>
			struct _transformed_pack : _transformed_pack<Trait, typename InPack::template pop<1>, typename OutPack::template push<typename Trait<typename InPack::first>::type>>
			{
			};

			template <template <class> class Trait, class OutPack>
			struct _transformed_pack<Trait, pack<>, OutPack>
			{
				using type = OutPack;
			};

			template <template <class> class Trait, class InPack>
			struct _assert_transformed_pack : _transformed_pack<Trait, InPack, pack<>>
			{
				static_assert(is_same_as_v<InPack, pack>,
							  "'stdx::meta::transformed_pack<Trait, InPack>': "
							  "InPack must be of type stdx::meta::pack<T ...> where T is any type parameter");
			};

			template <template <class> class Trait, class InPack>
			using transformed_pack = typename _assert_transformed_pack<Trait, InPack>::type;

			// Permutated pack, permutates the elements of a pack based on a valpack with integral values in the interval [0, N) where N is the number of elements in the pack

			template <class InPack, class OutPack, class IndexPack>
			struct _permutated_pack : _permutated_pack<InPack, typename OutPack::template push<typename InPack::template pop<IndexPack::first>::first>, typename IndexPack::template pop<1>>
			{
			};

			template <class InPack, class OutPack>
			struct _permutated_pack<InPack, OutPack, valpack<>>
			{
				using type = OutPack;
			};

			template <class IndexPack>
			struct _assert_index_values_permutated_pack : std::is_same<IndexPack, constrained_pack<value_trait<between<0, IndexPack::size - 1>::trait>::template value, constrained_pack<type_trait<std::is_integral>::template type, IndexPack>>>
			{
			};

			template <class InPack, class IndexPack>
			struct _assert_permutated_pack : _permutated_pack<InPack, pack<>, as_valpack<IndexPack>>
			{
				static_assert(is_same_as_v<InPack, pack>,
							  "'stdx::meta::permutated_pack<InPack, IndexPack>': "
							  "InPack must be of type stdx::meta::pack<T ...> where T is any type parameter");
				static_assert(InPack::size == IndexPack::size,
							  "'stdx::meta::permutated_pack<InPack, IndexPack>': "
							  "InPack::size must be equal to IndexPack::size");
				static_assert(_assert_index_values_permutated_pack<as_pack_val<IndexPack>>::value,
							  "'stdx::meta::permutated_pack<InPack, IndexPack>': "
							  "IndexPack must be of type stdx::template::valpack<V ...> type where V are integral types in the interval of [0, IndexPack::size), all unique");
			};

			template <class InPack, class IndexPack>
			using permutated_pack = typename _assert_permutated_pack<InPack, IndexPack>::type;

			// Merged pack, merges several packs of the same size into one pack of packs where the nth pack contains the nth elements of each original pack

			template <size_t N, class OutPack, class ... InPack>
			struct _merged_pack : _merged_pack<N - 1, typename OutPack::template push<pack<typename InPack::first ...>>, typename InPack::template pop<1> ...>
			{
			};

			template <class OutPack, class ... InPack>
			struct _merged_pack<0, OutPack, InPack ...>
			{
				using type = OutPack;
			};

			template <class SizeType, class ... SizeTypes>
			inline constexpr bool _assert_pack_sizes_merged_pack(SizeType size, SizeTypes ... sizes)
			{
				return (... && (size == sizes));
			};

			template <class ... InPack>
			struct _assert_merged_pack : _merged_pack<pack<InPack ...>::first::size, pack<>, InPack ...>
			{
				static_assert(_assert_pack_sizes_merged_pack(InPack::size ...),
							  "'stdx::meta::merged_pack<InPack ...>': "
							  "InPack::size must be equal for all packs");
			};

			template <class ... InPack>
			using merged_pack = typename _assert_merged_pack<InPack ...>::type;
		}

		using namespace stdx::meta;

		namespace stdx::binder
		{
			template <class FuncType, class ... ArgTypes>
			auto bind(FuncType * func, ArgTypes && ... args)
			{
				static_assert(std::is_function_v<FuncType>,
							  "'stdx::binder::bind(FuncType *, ArgTypes && ...)': "
							  "FuncType must be a function, a pointer to function, or a pointer to member function");
				return std::bind(func, bind_forward<ArgTypes>(args) ...);
			}
			template <class FuncType, class ObjType, class ... ArgTypes>
			auto bind(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args)
			{
				static_assert(std::is_function_v<FuncType>,
							  "'stdx::binder::bind(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
							  "FuncType must be a function, a pointer to function, or a pointer to member function");
				return std::bind(func, obj, bind_forward<ArgTypes>(args) ...);
			}

			template <class ValType>
			auto bind_forward(std::remove_reference_t<ValType> & val)
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
			template <class ValType>
			auto bind_forward(std::remove_reference_t<ValType> && val)
			{
				if constexpr (std::is_placeholder_v<std::remove_cv_t<std::remove_reference_t<ValType>>>)
				{
					return val;
				}
				else
				{
					return std::bind(std::move<ValType &>, std::move(val));
				}
			}
		}

		using namespace stdx::binder;
	}

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

	class task_priority
	{
	public:
		task_priority(int_least16_t default_priority = 0, 
					  int_least8_t minimum_priority = std::numeric_limits<int_least8_t>::min(), 
					  int_least8_t maximum_priority = std::numeric_limits<int_least8_t>::max()) :
			_task_priority_default(std::clamp<int_least16_t>(default_priority,
															 std::min(minimum_priority, maximum_priority),
															 std::max(minimum_priority, maximum_priority))),
			_task_priority_minimum(minimum_priority),
			_task_priority_maximum(maximum_priority)
		{
		}

		uint_least8_t operator()()
		{
			return uint_least8_t(std::abs(_task_priority_default - _task_priority_minimum));
		}
		uint_least8_t operator()(int_least16_t priority)
		{
			priority = std::clamp<int_least16_t>(priority, 
												 std::min(_task_priority_minimum, _task_priority_maximum), 
												 std::max(_task_priority_minimum, _task_priority_maximum));
			return uint_least8_t(std::abs(priority - _task_priority_minimum));
		}
	private:
		int_least16_t const _task_priority_default;
		int_least8_t const _task_priority_minimum;
		int_least8_t const _task_priority_maximum;
	};

	enum class task_state : uint_least8_t
	{
		ready,
		running,
		waiting, 
		cancelling,
		suspended,
		null
	};

	template <class>
	class _task;

	template <class RetType, class ... ParamTypes> // TODO review memory order
	class _task<RetType(ParamTypes ...)>
	{
	protected:
		_task() = default;
		template <class FuncType, class ... ArgTypes>
		_task(FuncType * func, ArgTypes && ... args) :
			_task_package(_stp::bind(func, std::forward<ArgTypes>(args) ...)),
			_task_state(task_state::suspended)
		{
			static_assert(
				std::is_function_v<FuncType>,
				"'stp::task<RetType, ParamTypes ...>::task(FuncType *, ArgTypes && ...)': "
				"FuncType must be a function"
			);
			static_assert(
				std::is_same_v<
					RetType,
					typename _stp::function_signature<FuncType>::return_type
				>,
				"'stp::task<RetType, ParamTypes ...>::task(FuncType *, ArgTypes && ...)': "
				"RetType must be the same type as the FuncType return type"
			);
			static_assert(
				std::is_same_v<
					_stp::pack<ParamTypes ...>,
					_stp::permutated_pack<
						_stp::transformed_pack<
							_stp::type_trait<_stp::identity>::last,
							_stp::constrained_pack<
								_stp::type_trait<std::is_placeholder>::first,
								_stp::merged_pack<
									_stp::pack<std::remove_cv_t<std::remove_reference_t<ArgTypes>> ...>, 
									typename _stp::function_signature<FuncType>::parameter_types
								>
							>
						>,
						_stp::transformed_pack<
							_stp::as_val,
							_stp::transformed_pack<
								_stp::value_trait<_stp::subtraction<1>::trait>::value,
								_stp::transformed_pack<
									std::is_placeholder,
									_stp::constrained_pack<
										std::is_placeholder,
										_stp::pack<std::remove_cv_t<std::remove_reference_t<ArgTypes>> ...>
									>
								>
							>
						>
					>
				>,
				"'stp::task<RetType, ParamTypes ...>::task(FuncType *, ArgTypes && ...)': "
				"ParamTypes must be the same types as the FuncType parameter types corresponding to the placeholder ArgTypes"
			);
		}
		template <class FuncType, class ObjType, class ... ArgTypes>
		_task(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args) :
			_task_package(_stp::bind(func, obj, std::forward<ArgTypes>(args) ...)),
			_task_state(task_state::suspended)
		{
			static_assert(
				std::is_function_v<FuncType>,
				"'stp::task<RetType, ParamTypes ...>::task(FuncType *, ArgTypes && ...)': "
				"FuncType must be a function"
			);
			static_assert(
				std::is_same_v<
					RetType,
					typename _stp::function_signature<FuncType>::return_type
				>,
				"'stp::task<RetType, ParamTypes ...>::task(FuncType *, ArgTypes && ...)': "
				"RetType must be the same type as the FuncType return type"
			);
			static_assert(
				std::is_same_v<
					_stp::pack<ParamTypes ...>,
					_stp::permutated_pack<
						_stp::transformed_pack<
							_stp::type_trait<_stp::identity>::last,
							_stp::constrained_pack<
								_stp::type_trait<std::is_placeholder>::first,
								_stp::merged_pack<
									_stp::pack<std::remove_cv_t<std::remove_reference_t<ArgTypes>> ...>, 
									typename _stp::function_signature<FuncType>::parameter_types
								>
							>
						>,
						_stp::transformed_pack<
							_stp::as_val,
							_stp::transformed_pack<
								_stp::value_trait<_stp::subtraction<1>::trait>::value,
								_stp::transformed_pack<
									std::is_placeholder,
									_stp::constrained_pack<
										std::is_placeholder,
										_stp::pack<std::remove_cv_t<std::remove_reference_t<ArgTypes>> ...>
									>
								>
							>
						>
					>
				>,
				"'stp::task<RetType, ParamTypes ...>::task(FuncType *, ArgTypes && ...)': "
				"ParamTypes must be the same types as the FuncType parameter types corresponding to the placeholder ArgTypes"
			);
		}
		~_task()
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::cancelling:
				case task_state::waiting:
				case task_state::running:
					_task_future.wait();
				default:
					break;
			}
		}
	public:
		void operator()(ParamTypes && ... args)
		{
			function(std::forward<ParamTypes>(args) ...)();
		}

		void cancel()
		{
			auto state = task_state::waiting;

			_task_state.compare_exchange_strong(state, task_state::cancelling, std::memory_order_relaxed);
		}
		void wait() const
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
				case task_state::cancelling:
					throw task_error(task_error_code::thread_deadlock_would_occur);
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
		int_least8_t get_priority()
		{
			return _task_priority;
		}
		void set_priority(int_least8_t priority)
		{
			_task_priority = priority;
		}
		[[nodiscard]] std::packaged_task<void()> function(ParamTypes && ... args) & // WARNING: The destructor for the task will block until operator() has been called for the object returned by this function
		{
			switch (_task_state.load(std::memory_order_acquire))
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

			_task_state.store(task_state::waiting, std::memory_order_release);

			return function;
		}
	protected:
		void _move(_task<RetType(ParamTypes ...)> && other) // TODO Review this
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::cancelling:
				case task_state::waiting:
				case task_state::running:
					throw task_error(task_error_code::state_loss_would_occur); // Idea: instead of throwing, why not waiting?
				default:
					break;
			}

			_task_package = std::move(other._task_package);
			_task_future = std::move(other._task_future);
			_task_state.store(other._task_state.exchange(task_state::null, std::memory_order_release), std::memory_order_release);
		}
		void _exchange(_task<RetType(ParamTypes ...)> && other) // std::exchange protects against self-assignment and enforces safe valid state
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::cancelling:
				case task_state::waiting:
				case task_state::running:
					throw task_error(task_error_code::state_loss_would_occur);
				default:
					break;
			}

			_task_package = std::exchange(other._task_package, std::packaged_task<RetType(ParamTypes ...)>());
			_task_future = std::exchange(other._task_future, std::future<void>());
			_task_state.store(other._task_state.exchange(task_state::null, std::memory_order_release), std::memory_order_release);
		}
		void _execute(ParamTypes && ... args)
		{
			auto state = task_state::waiting;

			if (_task_state.compare_exchange_strong(state, task_state::running, std::memory_order_relaxed))
			{
				_task_package(std::forward<ParamTypes>(args) ...);

				_task_state.store(task_state::ready, std::memory_order_release);
			}
			else
			{
				_task_state.store(task_state::suspended, std::memory_order_release);
			}
		}
		void _reset()
		{
			switch (_task_state.load(std::memory_order_acquire))
			{
				case task_state::cancelling:
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

		std::packaged_task<RetType(ParamTypes ...)> _task_package;
		std::future<void> _task_future;
		std::atomic<task_state> _task_state{ task_state::null };
		int_least8_t _task_priority{ 0 };
	};

	template <class>
	class task;

	template <class RetType, class ... ParamTypes>
	class task<RetType(ParamTypes ...)> : public _task<RetType(ParamTypes ...)>
	{
		static_assert(std::negation_v<std::is_rvalue_reference<RetType>>, "stp::task<T>: T cannot be a rvalue-reference");

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
		task(task<RetType(ParamTypes ...)> && other) // Move constructor (TODO review after checking _move)
		{
			this->_move(static_cast<_task<RetType(ParamTypes ...)> &&>(other));

			_task_result = std::move(other._task_result);
		}
		task & operator=(task<RetType(ParamTypes ...)> && other) // Move assignment operator
		{
			this->_exchange(static_cast<_task<RetType(ParamTypes ...)> &&>(other));

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
				case task_state::cancelling:
					throw task_error(task_error_code::thread_deadlock_would_occur);
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
		task(task<void(ParamTypes ...)> && other) // Move constructor 
		{
			this->_move(static_cast<_task<void(ParamTypes ...)> &&>(other));
		}
		task & operator=(task<void(ParamTypes ...)> && other) // Move assignment operator
		{
			this->_exchange(static_cast<_task<void(ParamTypes ...)> &&>(other));

			return *this;
		}

		void get()
		{
			switch (this->_task_state.load(std::memory_order_acquire))
			{
				case task_state::null:
					throw task_error(task_error_code::no_state);
				case task_state::suspended:
				case task_state::cancelling:
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
	};

	template <class RetType, class ... ParamTypes> // Implementation specialization
	class task<_stp::pack<RetType, _stp::pack<ParamTypes ...>>> : public task<RetType(ParamTypes ...)>
	{
	public:
		task() = default;
		template <class FuncType, class ... ArgTypes>
		task(FuncType * func, ArgTypes && ... args) :
			task<RetType(ParamTypes ...)>(func, std::forward<ArgTypes>(args) ...)
		{
		}
		template <class FuncType, class ObjType, class ... ArgTypes>
		task(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args) :
			task<RetType(ParamTypes ...)>(func, obj, std::forward<ArgTypes>(args) ...)
		{
		}
	};

	template <class FuncType, class ... ArgTypes>
	task(FuncType *, ArgTypes ...) -> 
		task<
			_stp::pack<
				typename _stp::function_signature<FuncType>::return_type,
				_stp::permutated_pack<
					_stp::transformed_pack<
						_stp::type_trait<_stp::identity>::last,
						_stp::constrained_pack<
							_stp::type_trait<std::is_placeholder>::first,
							_stp::merged_pack<
								_stp::pack<std::remove_cv_t<std::remove_reference_t<ArgTypes>> ...>, 
								typename _stp::function_signature<FuncType>::parameter_types
							>
						>
					>,
					_stp::transformed_pack<
						_stp::as_val,
						_stp::transformed_pack<
							_stp::value_trait<_stp::subtraction<1>::trait>::value,
							_stp::transformed_pack<
								std::is_placeholder,
								_stp::constrained_pack<
									std::is_placeholder,
									_stp::pack<std::remove_cv_t<std::remove_reference_t<ArgTypes>> ...>
								>
							>
						>
					>
				>
			>
		>;
	template <class FuncType, class ObjType, class ... ArgTypes>
	task(FuncType ObjType::*, ObjType *, ArgTypes ...) -> 
		task<
			_stp::pack<
				typename _stp::function_signature<FuncType>::return_type,
				_stp::permutated_pack<
					_stp::transformed_pack<
						_stp::type_trait<_stp::identity>::last,
						_stp::constrained_pack<
							_stp::type_trait<std::is_placeholder>::first,
							_stp::merged_pack<
								_stp::pack<std::remove_cv_t<std::remove_reference_t<ArgTypes>> ...>, 
								typename _stp::function_signature<FuncType>::parameter_types
							>
						>
					>,
					_stp::transformed_pack<
						_stp::as_val,
						_stp::transformed_pack<
							_stp::value_trait<_stp::subtraction<1>::trait>::value,
							_stp::transformed_pack<
								std::is_placeholder,
								_stp::constrained_pack<
									std::is_placeholder,
									_stp::pack<std::remove_cv_t<std::remove_reference_t<ArgTypes>> ...>
								>
							>
						>
					>
				>
			>
		>;

	template <class FuncType, int_least8_t Priority = 0, class ... ArgTypes>
	inline auto make_task(FuncType * func, ArgTypes && ... args) // Allows disambiguation of function overload by specifying the parameters' types, and automatically deduces type of task based on placeholder arguments
	{
		static_assert(
			std::is_function_v<FuncType>,
			"'stp::make_task(FuncType *, ArgTypes && ...)': "
			"FuncType must be a function"
		);
		task t(func, std::forward<ArgTypes>(args) ...);
		t.set_priority(Priority);
		return t;
	}
	template <class FuncType, int_least8_t Priority = 0, class ObjType, class ... ArgTypes>
	inline auto make_task(FuncType ObjType::* func, ObjType * obj, ArgTypes && ... args)
	{
		static_assert(
			std::is_function_v<FuncType>,
			"'stp::make_task(FuncType ObjType::*, ObjType *, ArgTypes && ...)': "
			"FuncType must be a function"
		);
		task t(func, obj, std::forward<ArgTypes>(args) ...);
		t.set_priority(Priority);
		return t;
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
		threadpool(size_t size = std::thread::hardware_concurrency(), 
				   threadpool_state state = threadpool_state::running, 
				   task_priority priority = task_priority()) :
			_threadpool_size(size),
			_threadpool_state(state),
			_threadpool_priority(priority)
		{
			for (size_t n = 0; n < size; ++n) // Idea: make thread initialize threadpool for maximum speed
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
		void execute(task<RetType(ParamTypes ...)> & task, ParamTypes && ... args)
		{
			auto priority = _threadpool_priority(task.get_priority());

			auto function = task.function(std::forward<ParamTypes>(args) ...);

			std::scoped_lock<std::mutex> lock(_threadpool_queue_mutex);

			_threadpool_task_queue.emplace(std::move(function), priority);

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
				thread(&threadpool::_pool, threadpool, this)
			{
			}

			_task task;
			bool active{ true };
			bool inactive{ false };
			std::thread thread; // Must be the last variable to be initialized
		};

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

		size_t _threadpool_size;
		threadpool_state _threadpool_state;
		task_priority _threadpool_priority;
		std::atomic_bool _threadpool_task{ false }; // Replace by concurrent_queue when done
		std::priority_queue<_task, std::deque<_task>> _threadpool_task_queue; // Replace by concurrent_queue when done
		std::list<_thread> _threadpool_thread_list;
		std::mutex _threadpool_queue_mutex; // Replace by concurrent_queue when done
		std::shared_mutex _threadpool_mutex;
		std::condition_variable_any _threadpool_condvar;
	};
}

#endif
