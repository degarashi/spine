#include "test.hpp"
#include "../resmgr.hpp"
#include "../resmgr_named.hpp"
#include "../lubee/random/string.hpp"

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

		template <class T>
		struct ResourceMgrName : Random {
			using rmgr_t = ResMgrName<T>;
			rmgr_t	_mgr;
			using value_t = typename T::value_t;
			using res_t = typename rmgr_t::shared_t;
			using key_t = typename rmgr_t::key_t;

			auto& getMgr() {
				return _mgr;
			}
		};
		using NTypes = ::testing::Types<TestObj<int>>;
		TYPED_TEST_CASE(ResourceMgrName, NTypes);

		struct ActionN {
			enum e {
				Acquire,
				Release,
				AcquireExist,
				FindByName,
				FindByHandle,
				_Num
			};
		};
		TYPED_TEST(ResourceMgrName, General) {
			USING(key_t);
			USING(res_t);
			USING(value_t);
			InitializeCounter<TypeParam>();

			struct Res {
				key_t		key;
				res_t		res;
				value_t		val;
			};
			using RMap = std::unordered_map<key_t, Res>;
			const auto mtf = this->mt().template getUniformF<int>();
			const auto rv = this->mt().template getUniformF<value_t>();
			auto& mgr = this->getMgr();

			RMap res;
			const auto pickRandom = [&res, &mtf](){
				const int idx = mtf({0, int(res.size()-1)});
				auto itr = res.begin();
				std::advance(itr, idx);
				return itr;
			};
			switch(mtf({0, ActionN::_Num})) {
				case ActionN::Acquire:
				{
					// 新たにリソースを確保
					const auto val = rv();
					const auto key = lubee::random::GenAlphabetString(mtf, 16);
					res.emplace(key, Res{key, mgr.acquire(key, val), val});
					break;
				}
				case ActionN::Release:
					// リソースを解放
					if(!res.empty()) {
						res.erase(pickRandom());
					}
					break;
				case ActionN::AcquireExist:
					if(!res.empty()) {
						// 既に存在するリソースを取得
						// 同じポインタが返ってくればOK
						const auto itr = pickRandom();
						const auto r = mgr.acquire(itr->first, rv());
						ASSERT_EQ(itr->second.res.get(), r.get());
					}
					break;
				case ActionN::FindByName:
					if(!res.empty()) {
						// 既に存在するリソースを名前から検索
						const auto itr = pickRandom();
						ASSERT_TRUE(mgr.get(itr->first));
					}
					break;
				case ActionN::FindByHandle:
					if(!res.empty()) {
						const auto itr = pickRandom();
						// 既に存在するリソースのポインタから名前を検索
						const auto key0 = mgr.getKey(itr->second.res);
						const auto key1 = mgr.getKey(itr->second.res.get());
						ASSERT_TRUE(key0);
						ASSERT_TRUE(key1);
						ASSERT_EQ(*key0, *key1);
						const auto res0 = mgr.get(*key0);
						ASSERT_EQ(itr->second.res, res0);
					}
					break;
			}
			// 各リソースの内部値が合致しているか
			for(auto& r : res) {
				ASSERT_EQ(r.second.val, r.second.res->getValue());
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