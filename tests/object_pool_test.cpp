#define OBJECT_POOL_CHECKBLOCK
#include "test.hpp"
#include "../object_pool.hpp"

namespace spi {
	namespace test {
		struct Action {
			enum {
				Allocate,
				AllocateArray,
				Destroy,
				DestroyArray,
				_Num
			} e;
		};

		template <class T, class MTF, class MkValue>
		void TestPool(MTF&& mtf, MkValue&& mkValue) {
			InitializeCounter<T>();

			// 初期サイズをランダムで決める
			const int initial = mtf({1,10});
			::spi::ObjectPool<T> pool(initial);

			using value_t = decltype(mkValue());
			struct Data {
				value_t		data;
				T*			ptr;
				Data(value_t d, T* p):
					data(d), ptr(p) {}
			};
			struct Array {
				std::vector<value_t>	data;
				T*						ptr;
			};
			std::vector<Data>	objdata;
			std::vector<Array>	arraydata;
			int counter = 0;
			const auto fnAllocObj = [&counter, &mkValue, &pool, &objdata](){
				const auto val = mkValue();
				T* ptr = pool.allocate(val);
				objdata.emplace_back(val, ptr);
				++counter;
			};
			// ランダムな回数、ランダムな操作を繰り返す
			const int NIter = mtf({1, 100});
			for(int i=0 ; i<NIter ; i++) {
				switch(mtf({0, Action::_Num-1})) {
					// 単体オブジェクトを確保
					case Action::Allocate:
						fnAllocObj();
						for(auto& o : objdata) {
							ASSERT_EQ(o.data, *o.ptr);
						}
						break;
					// 配列を確保
					case Action::AllocateArray:
					{
						const int n = mtf({0,10});
						auto* ptr = pool.allocateArray(n);
						arraydata.resize(arraydata.size()+1);
						auto& ar = arraydata.back();
						ar.ptr = ptr;
						for(int i=0 ; i<n ; i++) {
							const auto val = mkValue();
							ptr[i] = val;
							ar.data.emplace_back(val);
						}
						counter += n;
						break;
					}
					// 単体オブジェクトを解放
					case Action::Destroy:
						if(!objdata.empty()) {
							const int idx = mtf({0,int(objdata.size())-1});
							const auto itr = objdata.begin() + idx;
							ASSERT_EQ(itr->data, *itr->ptr);
							pool.destroy(itr->ptr);
							objdata.erase(itr);
							for(auto& o : objdata) {
								ASSERT_EQ(o.data, *o.ptr);
							}
							ASSERT_GE(--counter, 0);
						}
						break;
					// 配列を解放
					case Action::DestroyArray:
						if(!arraydata.empty()) {
							const int idx = mtf({0,int(arraydata.size())-1});
							const auto itr = arraydata.begin() + idx;
							const int n = itr->data.size();
							for(int i=0 ; i<n ; i++)
								ASSERT_EQ(itr->ptr[i], itr->data[i]);
							pool.destroy(itr->ptr);
							arraydata.erase(itr);
							ASSERT_GE(counter-=n, 0);
						}
						break;
				}
				// 割り当てブロック数の確認
				ASSERT_EQ(counter, int(pool.allocatingBlock()));
			}
			if(mtf({0,1})) {
				// 追加メモリ無しで確保できる分は単体で1つずつ確保していけば全て使える
				const int n = pool.remainingBlock();
				int rem = n;
				for(int i=0 ; i<n ; i++) {
					fnAllocObj();
					ASSERT_EQ(--rem, int(pool.remainingBlock()));
				}
			}
			if(mtf({0,1})) {
				// [全てユーザーが個別に開放するパターン]
				for(auto& o : objdata) {
					// 内部値のチェック
					ASSERT_EQ(o.data, *o.ptr);
					pool.destroy(o.ptr);
					--counter;
				}
				for(auto& a : arraydata) {
					// 内部値のチェック
					const int n = a.data.size();
					for(int i=0 ; i<n ; i++)
						ASSERT_EQ(a.data[i], a.ptr[i]);
					pool.destroy(a.ptr);
					counter -= n;
				}
				ASSERT_EQ(0, counter);
				ASSERT_EQ(0, pool.allocatingBlock());
				ASSERT_NO_FATAL_FAILURE(CheckCounter<T>(0));
			} else {
				// [残りのオブジェクトをclearで一括開放するパターン]
				const bool bShrink = mtf({0,1});
				const auto rem = pool.remainingBlock(),
							alc = pool.allocatingBlock();
				if(mtf({0,1})) {
					pool.clear(bShrink, true);
					// コンストラクタとデストラクタの呼び出し回数は同じになる
					ASSERT_NO_FATAL_FAILURE(CheckCounter<T>(0));
				} else {
					// メモリ領域はそのままでデストラクタだけ呼ぶ(割り当てブロックはリセット)
					pool.clear(bShrink, false);
					// コンストラクタの呼び出し回数の方がallocatingBlock分だけ多くなる
					ASSERT_NO_FATAL_FAILURE(CheckCounter<T>(counter));
				}
				if(bShrink) {
					// メモリブロックの残りはmin(初期値, 前のallocatingBlock+remainingBlock)
					ASSERT_EQ(pool.remainingBlock(), std::min(initial, int(rem+alc)));
				} else {
					// ブロック総数(remainingBlock+allocatingBlock) はclearを呼ぶ前と同じ
					ASSERT_EQ(0, pool.allocatingBlock());
					ASSERT_EQ(rem+alc, pool.remainingBlock());
				}
			}
		}

		template <class T>
		struct ObjectPool : Random {};
		using Types = ::testing::Types<TestObj<int>, TestObj<double>>;
		TYPED_TEST_CASE(ObjectPool, Types);

		// Trivialではないオブジェクトの生成、破棄
		TYPED_TEST(ObjectPool, General) {
			ASSERT_NO_FATAL_FAILURE(
				TestPool<TypeParam>(
					this->mt().template getUniformF<int>(),
					[rd=this->mt().template getUniformF<typename TypeParam::value_t>()](){
						return rd();
					}
				)
			);
		}

		template <class T>
		using ObjectPoolT = ObjectPool<T>;
		using TypesT = ::testing::Types<int, double>;
		TYPED_TEST_CASE(ObjectPoolT, TypesT);

		// Trivialなオブジェクトの生成、破棄
		TYPED_TEST(ObjectPoolT, General) {
			ASSERT_NO_FATAL_FAILURE(
				TestPool<TypeParam>(
					this->mt().template getUniformF<int>(),
					[rd=this->mt().template getUniformF<TypeParam>()](){
						return rd();
					}
				)
			);
		}
	}
}
