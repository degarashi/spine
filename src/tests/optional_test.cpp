#include "test.hpp"
#include "../optional.hpp"
#include "moveonly.hpp"
#include "../serialization/optional.hpp"

namespace spi {
	namespace test {
		template <class T>
		struct Optional : Random {
			using value_t = T;

			template <class T2=T, ENABLE_IF(lubee::is_number<T2>{})>
			auto makeRV() {
				return this->mt().template getUniform<T2>();
			}
			template <class T2=T, ENABLE_IF(!lubee::is_number<T2>{})>
			auto makeRV() {
				return this->mt().template getUniform<typename T2::value_t>();
			}
			auto makeRVF() {
				return [this](){ return this->makeRV(); };
			}
		};
		using Types = ::testing::Types<uint8_t, uint64_t, double, MoveOnly<uint64_t>>;
		TYPED_TEST_CASE(Optional, Types);

		template <class V, class MkValue, ENABLE_IF(!(std::is_copy_assignable<V>{}))>
		void CheckCopyAssign(MkValue&&) {}
		template <class V, class MkValue, ENABLE_IF((std::is_copy_assignable<V>{}))>
		void CheckCopyAssign(MkValue&& mkv) {
			::spi::Optional<V> op0, op1, op2;
			const auto val = mkv();
			// 値を代入するとtrue
			op0 = val;
			EXPECT_TRUE(op0);
			EXPECT_NE(op0, op1);
			// コピーすれば同値
			op1 = op0;
			EXPECT_EQ(op0, op1);
			op2 = MakeDifferentValue(val);
			EXPECT_NE(op0, op2);
			// Valueのcopy-assign
			op0 = op2.get();
			EXPECT_EQ(op2, op0);
			// Optionalのcopy-assign
			op2 = MakeDifferentValue(*op0);
			op0 = op2;
			EXPECT_EQ(op0, op2);
		}
		template <class V, class MkValue, ENABLE_IF(!(std::is_move_assignable<V>{}))>
		void CheckMoveAssign(MkValue&&) {}
		template <class V, class MkValue, ENABLE_IF((std::is_move_assignable<V>{}))>
		void CheckMoveAssign(MkValue&& mkv) {
			::spi::Optional<V> op0, op1, op2;
			const auto val = mkv();
			// 値を代入するとtrue
			op0 = V(val);
			EXPECT_TRUE(op0);
			EXPECT_NE(op0, op1);
			// Valueのcopy-assign
			// 同じ値をmove-assignすると==はtrue
			op1 = V(val);
			EXPECT_EQ(op0, op1);
			// Optionalのcopy-assign
			// 別のoptionalへmove-assignしても==はtrue
			op2 = std::move(op1);
			EXPECT_EQ(op0, op2);
		}
		template <class V, class MkValue, ENABLE_IF(!(std::is_copy_constructible<V>{}))>
		void CheckCopyConstruct(MkValue&&) {}
		template <class V, class MkValue, ENABLE_IF((std::is_copy_constructible<V>{}))>
		void CheckCopyConstruct(MkValue&& mkv) {
			const auto src0 = mkv(),
						src1 = MakeDifferentValue(src0);
			const V val0(src0),
					val1(src1);
			// ValueからのCopyConstruct
			::spi::Optional<V> op0(val0),
								op1(val1);
			EXPECT_EQ(val0, *op0);
			EXPECT_NE(op0, op1);
			// OptionalからのCopyConstruct
			::spi::Optional<V> op2(op0);
			EXPECT_EQ(op0, op2);
		}
		template <class V, class MkValue, ENABLE_IF(!(std::is_move_constructible<V>{}))>
		void CheckMoveConstruct(MkValue&&) {}
		template <class V, class MkValue, ENABLE_IF((std::is_move_constructible<V>{}))>
		void CheckMoveConstruct(MkValue&& mkv) {
			const auto src0 = mkv(),
						src1 = MakeDifferentValue(src0);
			// ValueからのMoveConstruct
			::spi::Optional<V> op0(V{src0}), op1(V{src1});
			EXPECT_EQ(V(src0), *op0);
			EXPECT_NE(op0, op1);
			// OptionalからのMoveConstruct
			::spi::Optional<V> op2(V{src0});
			op1 = std::move(op0);
			EXPECT_EQ(op2, op1);
		}
		template <class V, class MkValue>
		void CheckConstruct(MkValue&& mkv) {
			const auto src = mkv();
			::spi::Optional<V> op0, op1(src);
			op0 = ::spi::construct(src);
			EXPECT_EQ(op0, op1);
		}

		//! Optionalテストケース: 非ポインタ、非リファレンス
		template <class V, class MkValue>
		void Test_General(MkValue&& mkv) {
			{
				::spi::Optional<V> op0, op1(none);
				// デフォルトの状態ではfalse
				EXPECT_FALSE(op0);
				EXPECT_FALSE(op1);
				EXPECT_EQ(op0, op1);
			}
			{
				const auto src0 = mkv(),
							src1 = MakeDifferentValue(src0);
				::spi::Optional<V> op0(src0), op1(src1), op2(src0);
				EXPECT_TRUE(op0);
				EXPECT_NE(op0, op1);
				EXPECT_EQ(op2, op0);

				op0 = none;
				EXPECT_FALSE(op0);
				EXPECT_NE(op0, op2);
			}
			// 値のコピーテスト
			EXPECT_NO_FATAL_FAILURE(CheckCopyAssign<V>(mkv));
			// 値のムーブテスト
			EXPECT_NO_FATAL_FAILURE(CheckMoveAssign<V>(mkv));
			// コピーコンストラクタのチェック
			EXPECT_NO_FATAL_FAILURE(CheckCopyConstruct<V>(mkv));
			// ムーブコンストラクタのチェック
			EXPECT_NO_FATAL_FAILURE(CheckMoveConstruct<V>(mkv));
			// construct関数のチェック
			EXPECT_NO_FATAL_FAILURE(CheckConstruct<V>(mkv));
		}

		template <class V, class MkValue, ENABLE_IF(!(std::is_move_assignable<V>{}))>
		void CheckWriteThroughPointer(MkValue&&) {}
		template <class V, class MkValue, ENABLE_IF((std::is_move_assignable<V>{}))>
		void CheckWriteThroughPointer(MkValue&& mkv) {
			const auto src0 = mkv(),
						src1 = MakeDifferentValue(src0);
			auto p0 = std::make_unique<V>(src0),
				 p1 = std::make_unique<V>(src1),
				 p2 = std::make_unique<V>(src1);
			// optional経由で値を書き換え
			::spi::Optional<V*> pp0(p0.get());
			*(*pp0) = std::move(*p1);
			EXPECT_EQ(*p2, *(*pp0));
		}
		//! Optionalテストケース: ポインタ
		template <class V, class MkValue>
		void Test_Pointer(MkValue&& mkv) {
			const auto src0 = mkv(),
						src1 = MakeDifferentValue(src0);
			auto p0 = std::make_unique<V>(src0),
				 p1 = std::make_unique<V>(src1),
				 p2 = std::make_unique<V>(src0);
			{
				::spi::Optional<V*> pp0, pp2(p2.get());
				// デフォルトの状態ではfalse
				EXPECT_FALSE(pp0);
				// 値を代入するとtrue
				pp0 = p0.get();
				EXPECT_TRUE(pp0);
				// 中身の値が同じでもポインタが違えば==はfalse
				EXPECT_NE(pp0, pp2);
				// デリファレンスした値(=ポインタ)の比較
				EXPECT_EQ(p0.get(), *pp0);
				// 元の値を変更すればoptionalの方でも反映される
				ModifyValue(*p0);
				EXPECT_EQ(*p0, *(*pp0));

				// 参照する値を変更
				pp0 = p1.get();
				EXPECT_EQ(p1.get(), *pp0);
				// デリファレンスに代入してもポインタなので値そのものは書き換わらない
				*pp0 = p2.get();
				EXPECT_NE(*p1, *(*pp0));
			}
			CheckWriteThroughPointer<V>(mkv);
		}

		template <class V, class MkValue, ENABLE_IF(!(std::is_move_assignable<V>{}))>
		void CheckDereference(MkValue&&) {}
		template <class V, class MkValue, ENABLE_IF((std::is_move_assignable<V>{}))>
		void CheckDereference(MkValue&& mkv) {
			const auto src0 = mkv(),
						src1 = MakeDifferentValue(src0);
			{
				// デリファレンスに値を代入すると値そのものも書き換わる
				V	val0(src0);
				::spi::Optional<V&> rp0(val0);
				*rp0 = V(src1);
				EXPECT_NE(V(src0), *rp0);
				EXPECT_EQ(V(src1), *rp0);
			}
		}
		//! Optionalテストケース: リファレンス
		template <class V, class MkValue>
		void Test_Reference(MkValue&& mkv) {
			const auto src0 = mkv();
			{
				V	val0(src0),
					val1(MakeDifferentValue(src0)),
					val2(src0);
				::spi::Optional<V&> rp0, rp2(val2);
				// デフォルトの状態ではfalse
				EXPECT_FALSE(rp0);
				// 値を代入するとtrue
				rp0 = val0;
				EXPECT_TRUE(rp0);
				// 参照が違っても中身の値が同じなら==はtrue
				EXPECT_EQ(rp0, rp2);
				// デリファレンスした値(=参照)の比較
				EXPECT_EQ(val0, *rp0);
				// 元の値を変更すればoptionalの方でも反映される
				ModifyValue(val0);
				EXPECT_EQ(val0, *rp0);

				// 参照する値を変更
				rp0 = val1;
				EXPECT_EQ(val1, *rp0);
				EXPECT_EQ(&val1, &(*rp0));
			}
			{
				// Optionalを代入しても元の値は書き換わらない
				V	val0(src0),
					val1(MakeDifferentValue(src0));
				// optional経由で値を書き換え
				::spi::Optional<V&> rp0(val0), rp1(val1);
				rp0 = rp1;
				EXPECT_EQ(V(src0), val0);
				EXPECT_EQ(*rp1, *rp0);
			}
			CheckDereference<V>(mkv);
		}

		TYPED_TEST(Optional, Test) {
			USING(value_t);
			ASSERT_NO_FATAL_FAILURE(Test_General<value_t>(this->makeRVF()));
		}
		TYPED_TEST(Optional, Test_Pointer) {
			USING(value_t);
			ASSERT_NO_FATAL_FAILURE(Test_Pointer<value_t>(this->makeRVF()));
		}
		TYPED_TEST(Optional, Test_Reference) {
			USING(value_t);
			ASSERT_NO_FATAL_FAILURE(Test_Reference<value_t>(this->makeRVF()));
		}
		TYPED_TEST(Optional, Serialization) {
			using test_t = ::spi::Optional<TypeParam>;
			test_t opt(this->makeRV());

			std::unique_ptr<test_t>	p0(new test_t(std::move(opt))),
									p1;
			lubee::CheckSerialization(p0, p1);
		}

		struct OptionalC : Random {};
		TEST_F(OptionalC, Array) {
			struct E_Counter : std::runtime_error {
				E_Counter(): std::runtime_error("invalid counter value") {}
			};
			struct E_Canary : std::runtime_error {
				E_Canary(): std::runtime_error("invalid canary id") {}
			};
			struct Item {
				int	id, canary;
				int* counter;
				Item(int id, int* c):
					id(id),
					canary(id+1),
					counter(c)
				{
					++(*counter);
				}
				Item(const Item& i):
					id(i.id),
					canary(i.canary),
					counter(i.counter)
				{
					++(*counter);
				}
				//! カナリア値が破壊されていたら例外を投げる
				void checkCanary() const {
					if(canary != id+1)
						throw E_Canary();
				}
				~Item() noexcept(false) {
					if(!::testing::Test::HasFatalFailure() && !std::uncaught_exception()) {
						checkCanary();
						if(--(*counter) < 0)
							throw E_Counter();
					}
				}
			};
			int counter = 0;
			try {
				// 配列にランダムな要素を追加
				const auto mtf = this->mt().getUniformF<int>();
				const int N = mtf({8, 64});
				std::vector<Item>	items;
				for(int i=0 ; i<N ; i++)
					items.emplace_back(mtf(), &counter);
				// 適当にシャッフル
				std::shuffle(items.begin(), items.end(), this->mt().refMt());
			} catch(const E_Canary& e) {
				ASSERT_TRUE(false);
			} catch(const E_Counter& e) {
				ASSERT_TRUE(false);
			} catch(...) {}
			// コンストラクタとデストラクタが呼ばれる回数は等しい
			ASSERT_EQ(0, counter);
		}
	}
}
