#include "test.hpp"
#include "moveonly.hpp"
#include "lubee/src/random/string.hpp"
#include "../serialization/noseq_vec.hpp"

namespace spi {
	namespace test {
		struct NoseqVec : Random {};

		TEST_F(NoseqVec, General) {
			auto& mt = this->mt();
			const auto rdi = mt.getUniformF<int>();
			const auto makeVal = [&rdi](){ return lubee::random::GenAlphabetString(rdi, rdi({0,64})); };

			using value_t = MoveOnly<std::string>;
			using noseq_t = noseq_vec<value_t>;
			noseq_t ns;
			std::multiset<value_t> set;
			int NOp = rdi({1, 64});
			int count = 0;
			while(NOp -- != 0) {
				enum Op {
					Add,
					Remove,
					NumOp
				};
				switch(rdi({0, NumOp-1})) {
					case Op::Add: {
						value_t val0 = makeVal(),
								val1(val0.get());
						set.emplace(std::move(val0));
						ns.emplace_back(std::move(val1));
						++count;
						break; }
					case Op::Remove:
						if(count > 0) {
							const int idx = rdi({0, count-1});
							auto itr = set.find(ns[idx].get());
							set.erase(itr);
							ns.erase(ns.begin()+idx);
							--count;
						}
						break;
				}
			}
			ASSERT_EQ(set.size(), ns.size());
			auto itr = set.begin();
			while(itr != set.end()) {
				auto itr2 = std::upper_bound(itr, set.end(), *itr);
				const int c = std::count(ns.begin(), ns.end(), *itr);
				ASSERT_EQ(std::distance(itr, itr2), c);
				itr = itr2;
			}
		}

		TEST_F(NoseqVec, Serialization) {
			auto& mt = this->mt();
			const auto rdi = mt.getUniformF<int>();
			const auto makeVal = [&rdi](){ return lubee::random::GenAlphabetString(rdi, rdi({0,64})); };

			using noseq_t = noseq_vec<std::string>;
			noseq_t ns;
			int NElem = rdi({0, 64});
			while(NElem-- != 0) {
				ns.emplace_back(makeVal());
			}
			lubee::CheckSerialization(ns);
		}
	}
}
