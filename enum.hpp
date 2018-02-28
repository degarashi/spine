#pragma once
#include <boost/preprocessor.hpp>
#include <type_traits>

namespace spi {
	namespace inner {
		enum EnumTmp {};
	}
	using Enum_t = std::underlying_type_t<inner::EnumTmp>;
}
// 内部使用
#define DefineEnumStr_func2(r, data, elem) BOOST_PP_STRINGIZE(elem),
#define DefineEnumStr_func(seq) \
		static const char* ToStr(const ::spi::Enum_t v) noexcept { \
			static const char* str[_Num] = { \
				BOOST_PP_SEQ_FOR_EACH(DefineEnumStr_func2, 0, seq) \
			}; \
			return str[v]; \
		}
#define DefineEnum_Postfix(name, seq, func) \
			_Num = BOOST_PP_SEQ_SIZE(seq) \
		} value; \
		func(seq) \
		name() = default; \
		name(const name&) = default; \
		name(const e& n): value(n) {} \
		operator e () const noexcept { return value; } \
		using serialize_t = std::underlying_type_t<e>; \
		template <class Ar> \
		serialize_t save_minimal(const Ar&) const noexcept { return value; } \
		template <class Ar> \
		void load_minimal(const Ar&, const serialize_t& v) noexcept { value=static_cast<e>(v); } \
		const char* toStr() const noexcept { return ToStr(value); } \
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
		static const char* ToStr(const Enum_t v);
	};
	のような形で定義される
*/
#define DefineEnum(name, seq) \
	struct name { \
		enum e { \
			BOOST_PP_SEQ_ENUM(seq), \
			DefineEnum_Postfix(name,seq, DefineEnumStr_func)

// 内部使用
#define DefineEnumPair_func(r, data, elem) BOOST_PP_SEQ_ELEM(0,elem)=BOOST_PP_SEQ_ELEM(1,elem),
#define DefineEnumStrPair_func2(r, data, elem) {BOOST_PP_SEQ_ELEM(0,elem), BOOST_PP_STRINGIZE(BOOST_PP_SEQ_ELEM(1,elem))},
#define DefineEnumStrPair_func(seq) \
		static const char* ToStr(const ::spi::Enum_t v) { \
			static std::pair<::spi::Enum_t, const char*> str[_Num] = { \
				BOOST_PP_SEQ_FOR_EACH(DefineEnumStrPair_func2, 0, seq) \
			}; \
			for(::spi::Enum_t i=0 ; i<_Num ; i++) { \
				if(str[i].first == v) \
					return str[i].second; \
			} \
			return nullptr; \
		}

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
		static const char* ToStr(const Enum_t v);
	};
	のような形で定義される
*/
#define DefineEnumPair(name, seq) \
	struct name { \
		enum e { \
			BOOST_PP_SEQ_FOR_EACH(DefineEnumPair_func, 0, seq) \
			DefineEnum_Postfix(name, seq, DefineEnumStrPair_func)
