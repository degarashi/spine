#include "test.hpp"
#include "../resmgr.hpp"

namespace spi {
	namespace test {
		template <class T>
		struct ResourceMgr : Random {
			ResMgr<T>	_mgr;
			using value_t = typename T::value_t;
			using res_t = typename decltype(_mgr)::shared_t;

			auto& getMgr() {
				return _mgr;
			}
		};
		using Types = ::testing::Types<TestObj<int>, TestObj<double>>;
		TYPED_TEST_CASE(ResourceMgr, Types);

		struct Action {
			enum e {
				Acquire,
				Release,
				_Num
			};
		};
		TYPED_TEST(ResourceMgr, General) {
			USING(res_t);
			USING(value_t);
			InitializeCounter<TypeParam>();

			auto& mgr = this->getMgr();
			std::vector<std::pair<res_t, value_t>>	res;
			const auto mtf = this->mt().template getUniformF<int>();
			const auto rv = this->mt().template getUniformF<value_t>();
			const int n = mtf({0,100});
			for(int i=0 ; i<n ; i++) {
				switch(mtf({0,Action::_Num-1})) {
					// ランダムにリソースを確保
					case Action::Acquire:
					{
						const auto val = rv();
						res.emplace_back(mgr.acquire(val), val);
						break;
					}
					// ランダムにどれかリソースを解放
					case Action::Release:
						if(!res.empty()) {
							const int idx = mtf({0, int(res.size()-1)});
							res.erase(res.begin()+idx);
						}
						break;
				}
			}
			// 各リソースの内部値が合致しているか
			for(auto& r : res) {
				ASSERT_EQ(r.second, r.first->getValue());
			}
			// コンストラクタ/デストラクタが呼ばれた回数を比較
			// (ctor - dtor == リソースの残り数)
			ASSERT_NO_FATAL_FAILURE(CheckCounter<TypeParam>(res.size()));
			// こちらで管理しているリソースハンドルとリソースマネージャのそれは一致する
			ASSERT_EQ(res.size(), mgr.size());
			res.clear();
			// (ctor - dtor == 0)
			ASSERT_NO_FATAL_FAILURE(CheckCounter<TypeParam>(0));
			// 全てリソースを開放したのでマネージャのリソース数もゼロになる
			ASSERT_EQ(0, mgr.size());
		}
	}
}
