#pragma once
#include <boost/preprocessor.hpp>
#include <type_traits>

// 内部使用
#define DefineEnum_Postfix(name, seq) \
			_Num = BOOST_PP_SEQ_SIZE(seq) \
		} value; \
		name() = default; \
		name(const name&) = default; \
		name(const e& n): value(n) {} \
		operator e () const noexcept { return value; } \
		using serialize_t = std::underlying_type_t<e>; \
		template <class Ar> \
		serialize_t save_minimal(const Ar&) const noexcept { return value; } \
		template <class Ar> \
		void load_minimal(const Ar&, const serialize_t& v) noexcept { value=static_cast<e>(v); } \
	}

//! Enum定義
/*!
	DefineEnum(MyEnum, (AAA)(BBB)(CCC));
	と記述すると
	struct MyEnum {
		enum e {
			AAA,
			BBB,
			CCC,
			_Num = 3
		} value;
	};
	のような形で定義される
*/
#define DefineEnum(name, seq) \
	struct name { \
		enum e { \
			BOOST_PP_SEQ_ENUM(seq), \
			DefineEnum_Postfix(name,seq)

// 内部使用
#define DefineEnumPair_func(r, data, elem) BOOST_PP_SEQ_ELEM(0,elem)=BOOST_PP_SEQ_ELEM(1,elem),
//! 値を明示的に指定するEnum定義
/*!
	DefineEnumPair(
		MyEnum,
		((AAA)(100))
		((BBB)(200))
		((CCC)(300))
	);
	と記述すると
	struct MyEnum {
		enum e {
			AAA = 100,
			BBB = 200,
			CCC = 300,
			_Num = 3
		} value;
	};
	のような形で定義される
*/
#define DefineEnumPair(name, seq) \
	struct name { \
		enum e { \
			BOOST_PP_SEQ_FOR_EACH(DefineEnumPair_func, 0, seq) \
			DefineEnum_Postfix(name, seq)
