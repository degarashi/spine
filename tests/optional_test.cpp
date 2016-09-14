#include "test.hpp"
#include "../optional.hpp"

namespace spi {
	namespace test {
		template <class T>
		struct Optional : Random {
			using value_t = T;
			using opt_t = ::spi::Optional<value_t>;
			using popt_t = ::spi::Optional<value_t*>;
			using ropt_t = ::spi::Optional<value_t&>;

			template <class T2=T, ENABLE_IF(frea::is_number<T2>{})>
			auto makeRV() {
				return this->mt().template getUniform<T2>();
			}
			template <class T2=T, ENABLE_IF(!frea::is_number<T2>{})>
			value_t makeRV() {
				return value_t(makeRV<typename value_t::value_t>());
			}
		};
		using Types = ::testing::Types<uint8_t, uint64_t, double>;
		TYPED_TEST_CASE(Optional, Types);

		template <class T>
		T MakeDifferentValue(const T& t) {
			T tmp = 1,
			  res = t;
			do {
				res += tmp;
				tmp *= 2;
			} while(res == t);
			return res;
		}

		TYPED_TEST(Optional, Test) {
			USING(value_t);
			USING(opt_t);
			const value_t v0 = this->makeRV(),
							v1 = MakeDifferentValue(v0);
			{
				opt_t op0, op1;
				// デフォルトの状態ではfalse
				EXPECT_FALSE(op0);
				// 値を代入するとtrue
				op0 = v0;
				EXPECT_TRUE(op0);
				// 参照した値の比較
				EXPECT_EQ(v0, *op0);
				EXPECT_NE(v1, *op0);
				op1 = v1;
				EXPECT_NE(op0, op1);
				// 値のコピーテスト
				op1 = op0;
				EXPECT_EQ(op0, op1);
				EXPECT_EQ(*op0, *op1);
				// 値のムーブテスト
				op1 = v1;
				op0 = std::move(op1);
				EXPECT_EQ(v1, *op0);
			}
			{
				// コピーコンストラクタのチェック
				opt_t op0(v0), op1;
				EXPECT_EQ(v0, *op0);
				op1 = v0;
				EXPECT_EQ(op0, op1);
			}
			{
				// ムーブコンストラクタのチェック
				opt_t op0(std::move(v0)), op1;
				EXPECT_EQ(v0, *op0);
				op1 = v0;
				EXPECT_EQ(op0, op1);
				// noneの代入 -> 無効化
				op0 = none;
				EXPECT_FALSE(op0);
				EXPECT_NE(op0, op1);
			}
		}
		TYPED_TEST(Optional, Test_Reference) {
			USING(value_t);
			USING(ropt_t);
			value_t v0 = this->makeRV();
			{
				ropt_t rp0;
				// デフォルトの状態ではfalse
				EXPECT_FALSE(rp0);
				// 値を代入するとtrue
				rp0 = v0;
				EXPECT_TRUE(rp0);
				// デリファレンスした値(=参照)の比較
				EXPECT_EQ(v0, *rp0);
				// 元の値を変更すればoptionalの方でも反映される
				v0 = MakeDifferentValue(v0);
				EXPECT_EQ(v0, *rp0);
		
				// 参照する値を変更
				value_t v1 = MakeDifferentValue(v0);
				rp0 = v1;
				EXPECT_EQ(v1, rp0.get());
				EXPECT_NE(v0, rp0.get());
				// デリファレンスに代入すると参照先の値も書き換わる
				*rp0 = v0;
				EXPECT_EQ(v0, rp0.get());
			}
			{
				// optional経由で値を書き換え
				ropt_t rp0(v0);
				const value_t v1 = MakeDifferentValue(v0);
				*rp0 = v1;
				EXPECT_EQ(v1, *rp0);
			}
		}
		TYPED_TEST(Optional, Test_Pointer) {
			USING(value_t);
			USING(popt_t);
			value_t v0 = this->makeRV();
			{
				popt_t pp0;
				// デフォルトの状態ではfalse
				EXPECT_FALSE(pp0);
				// 値を代入するとtrue
				pp0 = &v0;
				EXPECT_TRUE(pp0);
				// デリファレンスした値(=ポインタ)の比較
				EXPECT_EQ(&v0, *pp0);
				// 元の値を変更すればoptionalの方でも反映される
				value_t tmp;
				do {
					tmp = this->makeRV();
				} while(tmp == 0);
				v0 += tmp;
				EXPECT_EQ(v0, *(*pp0));
		
				// 参照する値を変更
				value_t v1 = MakeDifferentValue(v0);
				pp0 = &v1;
				EXPECT_EQ(v1, *pp0.get());
				// デリファレンスに代入してもポインタなので値そのものは書き換わらない
				pp0.get() = &v0;
				EXPECT_NE(v1, *pp0.get());
			}
			{
				// optional経由で値を書き換え
				popt_t pp1(&v0);
				const value_t v1 = MakeDifferentValue(v0);
				*(*pp1) = v1;
				EXPECT_EQ(v1, *(*pp1));
			}
		}
	}
}
