#include "test.hpp"
#include "moveonly.hpp"
#include "../pqueue.hpp"

namespace spi {
	namespace test {
		namespace {
			struct MyPair {
				int a,b;
				bool operator < (const MyPair& my) const {
					return a < my.a;
				}
			};
		}

		class PQueueTest : public Random {
			protected:
				template <class T>
				static auto getBegin(T& t) {
					return t.begin();
				}
				template <class T>
				static auto getEnd(T& t) {
					return t.end();
				}
		};
		TEST_F(PQueueTest, InsertBefore) {
			spi::pqueue<MyPair, std::deque, std::less<MyPair>, InsertBefore> myB;
			for(int i=0 ; i<20 ; i++)
				myB.push(MyPair{0, i*100});
		
			int check = myB.front().b;
			for(auto itr=getBegin(myB) ; itr!=getEnd(myB) ; ++itr) {
				EXPECT_GE(check, (*itr).b);
				check = (*itr).b;
			}
		}
		TEST_F(PQueueTest, InsertAfter) {
			spi::pqueue<MyPair, std::deque, std::less<MyPair>, InsertAfter> myA;
			for(int i=0 ; i<20 ; i++)
				myA.push(MyPair{0, i*100});
			int check = myA.front().b;
			for(auto itr=getBegin(myA) ; itr!=getEnd(myA) ; ++itr) {
				EXPECT_LE(check, (*itr).b);
				check = (*itr).b;
			}
		}
		TEST_F(PQueueTest, Sequence) {
			using Data = MoveOnly<MoveOnly<int>>;
			auto& rd = mt();
			pqueue<Data, std::deque, std::less<Data>, InsertBefore> q;
			// 並び順のチェック
			const auto fnCheck = [&q]() {
				int check = 0;
				for(auto itr=getBegin(q) ; itr!=getEnd(q) ; ++itr) {
					EXPECT_LE(check, (*itr).getValue());
					check = (*itr).getValue();
				}
			};
			// ランダムな値を追加
			const auto fnAddrand = [&](const int n) {
				for(int i=0 ; i<n ; i++)
					q.push(Data(rd.getUniformMin(0)));
			};
		
			fnAddrand(512);
			fnCheck();
		
			// 先頭の半分を削除して、再度チェック
			for(int i=0 ; i<256 ; i++)
				q.pop_front();
			fnAddrand(256);
			fnCheck();
			EXPECT_EQ(512, q.size());
		}
	}
}
