#include "test.hpp"
#include "lubee/src/random/string.hpp"
#include "moveonly.hpp"
#include "../serialization/noseq_list.hpp"
#include "lubee/src/check_serialization.hpp"

namespace spi {
	namespace test {
		struct NoseqList : Random {
			using id_t = uint64_t;
			using value_t = MoveOnly<int>;
			using noseq_t = noseq_list<value_t>;
		};

		DefineEnum(Op, (Add)(AddEmplace)(Verify)(Modify)(Delete)(CheckId)(Clear));
		TEST_F(NoseqList, Serialization) {
			auto& mt = this->mt();
			const auto rdi = mt.getUniformF<int>();
			const auto makeVal = [&rdi](){ return lubee::random::GenAlphabetString(rdi, rdi({0,128})); };
			noseq_list<std::string> nl;
			int nElem = rdi({1, 128});
			while(nElem-- != 0) {
				nl.add(makeVal());
			}
			lubee::CheckSerialization(nl);
		}
		TEST_F(NoseqList, General) {
			auto& mt = this->mt();
			const auto rdi = mt.getUniformF<int>();

			// 確認用の連想配列
			using raw_t = std::decay_t<decltype(std::declval<value_t>().get())>;
			using Map = std::unordered_map<id_t, raw_t>;
			Map	map;
			noseq_t nl;

			const auto selectRandomNode = [&map, &rdi]() {
				const int n = map.size();
				if(n > 0) {
					const int idx = rdi({0, n-1});
					auto itr = map.begin();
					std::advance(itr, idx);
					return itr;
				}
				return map.end();
			};
			int nOp = rdi({8, 128});
			while(nOp-- != 0) {
				switch(rdi({0, Op::_Num-1})) {
					// 適当に幾つか要素を加える -> IDを記憶
					// addによる追加
					case Op::Add: {
						const raw_t val = rdi();
						map.emplace(nl.add(value_t(val)), val);
						break; }
					// emplaceによる追加
					case Op::AddEmplace: {
						const raw_t val = rdi();
						map.emplace(nl.emplace(val), val);
						break; }
					case Op::Verify:
						for(auto& ent : map) {
							// 記録したIDが有効か判定
							ASSERT_TRUE(nl.has(ent.first));
							// 値がユーザー操作以外で改竄されてないか確認
							ASSERT_EQ(ent.second, nl.get(ent.first).get());
						}
						break;
					// 値の編集
					case Op::Modify: {
						auto itr = selectRandomNode();
						if(itr != map.end()) {
							ModifyValue(itr->second);
							nl.get(itr->first) = itr->second;
						}
						break; }
					// 適当に要素を一つ削除
					case Op::Delete: {
						auto itr = selectRandomNode();
						if(itr != map.end()) {
							nl.rem(itr->first);
							map.erase(itr);
						}
						break; }
					// 無効なIDを指定したらfalseが返る
					case Op::CheckId: {
						auto itr = selectRandomNode();
						if(itr == map.end())
							break;
						const auto id = MakeDifferentValue(itr->first);
						ASSERT_EQ(nl.has(id), map.count(id)==1);
						break; }
					case Op::Clear:
						nl.clear();
						map.clear();
						break;
				}
				ASSERT_EQ(map.size(), nl.size());
				// サイズがゼロならempty()もtrue
				ASSERT_EQ(nl.empty(), nl.size()==0);
				// 自身との比較は常にtrue
				ASSERT_EQ(nl, nl);
			}
			using RawV = std::vector<raw_t>;
			// イテレータによる巡回
			RawV plain;
			{
				const auto itrE = nl.end();
				for(auto itr=nl.begin() ; itr!=itrE ; ++itr) {
					plain.push_back(itr->get());
				}
			}
			ASSERT_EQ(nl.size(), plain.size());

			// 逆イテレータによる巡回
			RawV rplain;
			{
				const auto itrE = nl.rend();
				for(auto itr=nl.rbegin() ; itr!=itrE ; ++itr) {
					rplain.push_back(itr->get());
				}
			}
			std::reverse(rplain.begin(), rplain.end());
			ASSERT_EQ(plain, rplain);

			// constイテレータによる巡回
			plain.clear();
			{
				const auto itrE = nl.cend();
				for(auto itr=nl.cbegin() ; itr!=itrE ; ++itr) {
					plain.push_back(itr->get());
				}
			}
			ASSERT_EQ(nl.size(), plain.size());

			// const逆イテレータによる巡回
			rplain.clear();
			{
				const auto itrE = nl.crend();
				for(auto itr=nl.crbegin() ; itr!=itrE ; ++itr) {
					rplain.push_back(itr->get());
				}
			}
			std::reverse(rplain.begin(), rplain.end());
			ASSERT_EQ(plain, rplain);
		}
	}
}
