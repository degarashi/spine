#include "test.hpp"
#include "../enum.hpp"

namespace spi::test {
	#define SEQ0 (AAAA)(BBBB)(CCCC)(DDDD)(FFFF)(GGGG)
	#define SEQ1
	#define SEQ2 (A)(B)(C)(D)(E)(F)(G)(H)(I)(J)(K)(L)(M)(N)(O)(P)(Q)(R)(S)(T)(U)
	#define DEF_ENUM(name, seq) \
		DefineEnum( \
			name, \
			seq \
		);

	DEF_ENUM(Enum0, SEQ0)
	DEF_ENUM(Enum1, SEQ1)
	DEF_ENUM(Enum2, SEQ2)

	#undef DEF_ENUM

	// data=(Enum)(SEQ)
	// 各値が連番で並んでいるかのチェック
	#define StaticCheckFunc(z, n, data) \
		{ using enum_t = BOOST_PP_SEQ_ELEM(0, data); \
		constexpr auto num = enum_t::BOOST_PP_SEQ_ELEM(n, BOOST_PP_SEQ_ELEM(1, data)); \
		static_assert(num == n); \
		constexpr enum_t e = num; \
		static_assert(e == num); }

	// Enum値の個別チェックと
	// _Numが、Enum値の総数を指しているかのチェック
	#define DEF_STATICCHECK(e, s) \
		void BOOST_PP_CAT(StaticCheck_, e)() { \
			BOOST_PP_REPEAT(BOOST_PP_SEQ_SIZE(s), StaticCheckFunc, (e)(s)) \
			static_assert(e::_Num == BOOST_PP_SEQ_SIZE(s)); \
		}

	namespace {
		DEF_STATICCHECK(Enum0, SEQ0)
		DEF_STATICCHECK(Enum1, SEQ1)
		DEF_STATICCHECK(Enum2, SEQ2)
	}

	#undef DEF_STATICCHECK
	#undef StaticCheckFunc

	template <class E>
	struct Enum : Random {
		using enum_t = E;
		using value_t = typename enum_t::value_t;

		// value_tで表現できる範囲でランダムなEnum値を生成
		enum_t makeRandom() {
			auto& mt = this->mt();
			return {mt.template getUniform<value_t>()};
		}
	};

	namespace {
		using Types = ::testing::Types<Enum0, Enum1, Enum2>;
		TYPED_TEST_CASE(Enum, Types);
	}

	TYPED_TEST(Enum, General) {
		const auto e0 = this->makeRandom(),
				 e1 = this->makeRandom();
		// operator e()
		USING(enum_t);
		ASSERT_EQ(e0.value, static_cast<enum_t>(e0));

		const auto v0 = e0.value,
					 v1 = e1.value;
		{
			// operator |=
			auto et0 = e0;
			et0 |= e1;
			ASSERT_EQ(v0 | v1, et0);
		}
		{
			// operator &=
			auto et0 = e0;
			et0 &= e1;
			ASSERT_EQ(v0 & v1, et0);
		}
		{
			// operator ^=
			auto et0 = e0;
			et0 ^= e1;
			ASSERT_EQ(v0 ^ v1, et0);
		}
	}
	TYPED_TEST(Enum, Serialization) {
		// ランダムな値をセットしたenum値でチェック
		// (value_tで表現できる範囲ならなんでもOK)
		lubee::CheckSerialization(this->makeRandom());
	}
}
