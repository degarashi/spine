#include "test.hpp"
#include "../treenode.hpp"
#include "lubee/compiler_macro.hpp"
#include "../serialization/treenode.hpp"

namespace spi {
	namespace test {
		struct TestNode;
		using SPNode = std::shared_ptr<TestNode>;
		using WPNode = std::weak_ptr<TestNode>;
		using SPNodeV = std::vector<SPNode>;
		struct TestNode : std::enable_shared_from_this<TestNode> {
			SPNodeV		child;
			WPNode		parent;
			int			value;

			DefineEnum(
				Iterate,
				(ReturnFromChild)
				(StepIn)
				(Next)
				(Quit)
			);
			TestNode(int val): value(val) {}
			void addChild(const SPNode& nd) {
				ASSERT_TRUE(std::find(child.begin(), child.end(), nd) == child.end());
				child.push_back(nd);
				ASSERT_FALSE(nd->getParent());
				nd->parent = shared_from_this();
			}
			void removeChild(const SPNode& nd) {
				auto itr = std::find(child.begin(), child.end(), nd);
				ASSERT_TRUE(itr != child.end());
				child.erase(itr);
				ASSERT_EQ(nd->getParent().get(), this);
				nd->parent = WPNode();
			}
			template <bool Sib, class Callback>
			void iterateDepthFirst(Callback&& cb, int depth=0) {
				cb(*this, depth);
				for(auto& c : child)
					c->iterateDepthFirst<Sib>(std::forward<Callback>(cb), depth+1);
			}
			SPNode getChild() const {
				if(child.empty())
					return SPNode();
				return child[0];
			}
			SPNode getParent() {
				return parent.lock();
			}
		};

		class TreeNode_t : public TreeNode<TreeNode_t> {
			using base_t = TreeNode<TreeNode_t>;
			private:
				friend class cereal::access;
				template <class Ar>
				void serialize(Ar& ar) {
					ar(value, cereal::base_class<base_t>(this));
				}
			public:
				int		value;
				TreeNode_t() = default;
				TreeNode_t(int v): value(v) {}
				bool operator == (const TreeNode_t& t) const {
					return value == t.value;
				}
		};
		using SPTreeNode = std::shared_ptr<TreeNode_t>;

		void CheckParent(const SPTreeNode& nd) {
			auto c = nd->getChild();
			while(c) {
				// 子ノードの親をチェック
				auto p = c->getParent();
				ASSERT_EQ(p.get(), nd.get());
				// 下層のノードを再帰的にチェック
				ASSERT_NO_FATAL_FAILURE(CheckParent(c));
				// 兄弟ノードをチェック
				c = c->getSibling();
			}
		}
		template <class A, class B>
		void CheckEqual(const A& a, const B& b) {
			ASSERT_EQ(a.size(), b.size());
			int nN = a.size();
			for(int i=0 ; i<nN ; i++)
				ASSERT_EQ(a[i]->value, b[i]->value);
		}
		template <class T>
		class TestTree {
			using SP = std::shared_ptr<T>;
			using VEC = std::vector<SP>;
			SP		_spRoot;
			VEC		_spArray;

			using CB = std::function<void (const SP&, const SP&)>;
			CB		_cbAdd,
					_cbRem,
					_cbParent;
			bool hasCB() const {
				return static_cast<bool>(_cbAdd);
			}
			static std::false_type _IsSP(...);
			static std::true_type _IsSP(const SP&);
			template <class A>
			using IsSP = decltype(_IsSP(std::decay_t<A>()));

			void _reflArray() {
				_spArray = plain();
			}
			public:
				using Type = T;
				template <
					class A,
					class... Ts,
					ENABLE_IF(!IsSP<A>{})
				>
				TestTree(A&& a, Ts&&... params):
					_spRoot(
						std::make_shared<T>(
							std::forward<A>(a),
							std::forward<Ts>(params)...
						)
					)
				{
					_reflArray();
				}
				template <
					class A,
					ENABLE_IF(IsSP<A>{})
				>
				TestTree(A&& sp):
					_spRoot(std::forward<A>(sp))
				{
					_reflArray();
				}
				TestTree() = default;
				void setCallback(const CB& add, const CB& rem, const CB& p) {
					if(add) {
						Assert0(add && rem && p);
						_cbAdd = add;
						_cbRem = rem;
						_cbParent = p;
					} else
						_cbAdd = _cbRem = _cbParent = nullptr;
				}
				//! 要素の追加
				void add(const int n, const SP& node) {
					if(hasCB()) {
						_cbParent(node, _spArray.at(n));
						_cbAdd(_spArray.at(n), node);
					}
					ASSERT_NO_FATAL_FAILURE(_spArray.at(n)->addChild(node));
					_reflArray();
				}
				//! 要素の削除
				SP rem(const int n) {
					SP sp = _spArray.at(n);
					if(auto pr = sp->getParent()) {
						if(hasCB()) {
							_cbRem(pr, sp);
							_cbParent(sp, nullptr);
						}
						pr->removeChild(sp);
						if(::testing::Test::HasFatalFailure())
							return SP();
					} else {
						_spRoot = sp->getChild();
						EXPECT_NE(static_cast<T*>(nullptr), _spRoot.get());
					}
					_reflArray();
					return sp;
				}
				//! 要素の入れ替え
				SP swapAt(const int from, const int to) {
					const auto node = rem(from);
					add(to % size(), node);
					return node;
				}

				//! 深度優先で配列展開
				VEC plain() const {
					return Plain(_spRoot);
				}
				static VEC Plain(const SP& sp) {
					VEC ret;
					sp->template iterateDepthFirst<true>([&ret](auto& nd, int){
						ret.emplace_back(nd.shared_from_this());
						return Iterate::StepIn;
					});
					return ret;
				}
				const VEC& getArray() const {
					return _spArray;
				}
				const SP& getRoot() const {
					return _spRoot;
				}
				int size() const {
					return _spArray.size();
				}
		};
		template <class CB>
		int _CallTS(CB&& /*cb*/) { return -1; }
		template <class CB, class T, class... Ts>
		int _CallTS(CB&& cb, T& tree, Ts&&... ts) {
			int res = cb(tree);
			_CallTS(std::forward<CB>(cb), std::forward<Ts>(ts)...);
			return res;
		}
		DefineEnum(
			Manip,
			(Add)
			(Remove)
			(Recompose)
		);
		using ManipV = std::vector<Manip>;
		template <class RD, class VT, class T, class... Ts>
		void RandomManipulate(RD& rd, VT&& vt, const int index, const ManipV& manipList, T&& t, Ts&&... ts) {
			// ノードが1つしか無い時は追加オンリー
			Manip typ;
			if(t.size() <= 1) {
				if(std::count(manipList.begin(), manipList.end(), Manip::Add) == 0)
					return;
				typ = Manip::Add;
			} else {
				typ = manipList.at(rd.template getUniform<int>({0, manipList.size()-1}));
			}
			switch(typ) {
				// 新たなノードを追加
				case Manip::Add: {
					// 挿入箇所を配列から適当に選ぶ
					const int at = rd.template getUniform<int>({0, t.size()-1});
					// Tree-A
					auto value = vt(index);
					value = _CallTS([value, at](auto& t){
										t.add(at, std::make_shared<typename std::decay_t<decltype(t)>::Type>(value));
										return value;
									}, std::forward<T>(t), std::forward<Ts>(ts)...);
					// std::cout << boost::format("added: %1% value=%2%\n") % at % value;
					break; }
				// ノードを削除
				case Manip::Remove: {
					// ルート以外を選ぶ
					const int at = rd.template getUniform<int>({1, t.size()-1});
					_CallTS([at](auto& t){
									return t.rem(at)->value;
								}, std::forward<T>(t), std::forward<Ts>(ts)...);
					// std::cout << boost::format("removed: %1% value=%2%\n") % at % value;
					break; }
				// ノードを別の場所に繋ぎ変え
				case Manip::Recompose: {
					int from = rd.template getUniform<int>({1, t.size()-1}),
						to = rd.template getUniform<int>({0, t.size()-2});
					if(to >= from)
						to = (to+1) % t.size();

					_CallTS([from, to](auto& t){
									return t.swapAt(from, to)->value;
								}, std::forward<T>(t), std::forward<Ts>(ts)...);
					// std::cout << boost::format("moved: %1% to %2% value=%3%\n") % from % to % value;
					break; }
				default:
					__assume(0);
			}
		}
/*
		//! print out テスト
		void PrintOut(const SPTreeNode& root) {
			using T = typename std::decay_t<decltype(*root)>::element_type;
			std::unordered_set<const T*> testset;
			root->iterateDepthFirst<false>([&testset](auto& nd, int d){
				using Ret = typename std::decay_t<decltype(nd)>::Iterate;
				while(d-- > 0)
					std::cout << '\t';
				if(testset.count(&nd) == 0) {
					testset.emplace(&nd);
					std::cout << nd.value << std::endl;
					return Ret::StepIn;
				} else {
					std::cout << '[' << nd.value << ']' << std::endl;
					return Ret::Quit;
				}
			});
		}
*/
		//! 重複ノードに関するテスト
		template <class TR>
		void DuplNodeTest(TR& tree)	{
			using T = typename TR::Type;
			std::unordered_set<T*> testset;
			tree.getRoot()->iterateDepthFirst<false>([&testset](auto& nd, int){
				EXPECT_EQ(testset.count(&nd), 0);
				testset.emplace(&nd);
				return std::decay_t<decltype(nd)>::Iterate::StepIn;
			});
		}
		namespace {
			// ランダムにノード操作
			template <class RD, class V, class... TS>
			void RandomManipulation(RD& rd, const int repeat, V&& fnValue, const ManipV& manipList, TS&&... tree) {
				for(int i=0 ; i<repeat ; i++)
					RandomManipulate(rd, fnValue, i, manipList, std::forward<TS>(tree)...);
			}
			const ManipV cs_manip = {
				Manip::Add, Manip::Add, Manip::Add,
				Manip::Remove,
				Manip::Recompose
			};
			const ManipV cs_manip_noremove = {
				Manip::Add, Manip::Add, Manip::Add,
				Manip::Recompose
			};
		}
		struct TreeNodeTest : Random {};
		TEST_F(TreeNodeTest, General) {
			auto& rd = this->mt();
			// TreeNodeと、子を配列で持つノードでそれぞれツリーを作成
			TestTree<TestNode>		treeA(0);		// 確認用
			TestTree<TreeNode_t>	treeB(0);		// テスト対象
			RandomManipulation(
				rd,
				100,
				[](const int r){
					return r+1;
				},
				cs_manip,
				treeA,
				treeB
			);
			// 親ノードの接続確認
			ASSERT_NO_FATAL_FAILURE(CheckParent(treeB.getRoot()));
			// Cloneしたツリーでも確認
			ASSERT_NO_FATAL_FAILURE(CheckParent(treeB.getRoot()->cloneTree()));
			ASSERT_EQ(treeA.size(), treeB.size());
			// 2種類のツリーで比較
			// (深度優先で巡回した時に同じ順番でノードが取り出せる筈)
			ASSERT_NO_FATAL_FAILURE(CheckEqual(treeA.getArray(), treeB.getArray()));
		}
		namespace {
			template <class T>
			void Check(const T& node) {
				if(node->getChild()) {
					// 順方向で取り出した子ノードとprevSiblingで取り出したそれは、逆転すれば同一になる
					using V = typename std::decay_t<decltype(*node)>::SPVector;
					V vn, vr;
					auto sp = node->getChild();
					while(sp) {
						vn.emplace_back(sp);
						sp = sp->getSibling();
					}
					sp = node->getChild();
					while(auto next = sp->getSibling())
						sp = next;
					while(sp) {
						vr.emplace_back(sp);
						sp = sp->getPrevSibling();
					}
					std::reverse(vr.begin(), vr.end());
					ASSERT_EQ(vn, vr);
				}
				// 子ノードも再帰的に調べる
				ASSERT_NO_FATAL_FAILURE(
					node->iterateChild(
						[](auto& nd){
							Check(nd);
							return true;
						}
					);
				);
			}
		}
		TEST_F(TreeNodeTest, PrevSibling) {
			auto& rd = this->mt();
			// ランダムなツリーを作る
			TestTree<TreeNode_t>	tree(0);
			int value = 0;
			constexpr int N_Manip = 100;
			RandomManipulation(
				rd,
				N_Manip,
				[&value](const int){
					return ++value;
				},
				cs_manip,
				tree
			);
			ASSERT_NO_FATAL_FAILURE(Check(tree.getRoot()));
		}
		namespace {
			template <class T>
			T* GetPointer(T* p) {
				return p;
			}
			template <class T>
			T* GetPointer(const std::shared_ptr<T>& sp) {
				return sp.get();
			}
			template <class T>
			void CheckPlain(const T& ar, int idx) {
				if(idx == 0)
					return;
	
				auto ptr = GetPointer(ar[idx]->getParent());
				ASSERT_TRUE(static_cast<bool>(ptr));
				auto itr = std::find_if(ar.begin(), ar.end(), [ptr](auto& p){
					return GetPointer(p) == ptr;
				});
				int idx1(itr - ar.begin());
				ASSERT_LT(idx1, idx);
				ASSERT_NO_FATAL_FAILURE(CheckPlain(ar, idx1));
			}
		}
		TEST_F(TreeNodeTest, Plain) {
			// ランダムなツリーを生成
			auto& rd = this->mt();
			TestTree<TreeNode_t>	tree(0);
			RandomManipulation(
				rd,
				100,
				[](const int r){
					return r+1;
				},
				cs_manip,
				tree
			);
			auto spRoot = tree.getRoot();
			{
				// 配列化(smart pointer)
				auto ar = spRoot->plain();
				for(size_t i=0 ; i<ar.size() ; i++)
					ASSERT_NO_FATAL_FAILURE(CheckPlain(ar, i));
			}
			{
				// 配列化(pointer)
				auto ar = spRoot->plainPtr();
				for(size_t i=0 ; i<ar.size() ; i++)
					ASSERT_NO_FATAL_FAILURE(CheckPlain(ar, i));
			}
		}
		TEST_F(TreeNodeTest, CompareTree) {
			// ランダムなツリーを生成
			auto& rd = this->mt();
			TestTree<TreeNode_t>	treeA(0);
			RandomManipulation(
				rd,
				100,
				[](const int r){
					return r+1;
				},
				cs_manip,
				treeA
			);
			TestTree<TreeNode_t>	treeB(treeA.getRoot()->cloneTree());
	
			// ツリー構造比較はtrue
			ASSERT_TRUE(CompareTreeStructure(*treeA.getRoot(), *treeB.getRoot()));
			// データ比較もtrue
			ASSERT_TRUE(CompareTree(*treeA.getRoot(), *treeB.getRoot()));
			// 値を適当に変更
			auto ar = treeA.plain();
			ar[rd.template getUniform<int>({0, ar.size()-1})]->value += 100;
			// ツリー構造比較はtrue
			ASSERT_TRUE(CompareTreeStructure(*treeA.getRoot(), *treeB.getRoot()));
			// データ比較はfalse
			ASSERT_FALSE(CompareTree(*treeA.getRoot(), *treeB.getRoot()));
		}
		TEST_F(TreeNodeTest, TreeNode) {
			auto& rd = this->mt();
			// ランダムなツリーを作る
			TestTree<TreeNode_t>	tree(0);
			RandomManipulation(
				rd,
				100,
				[](const int r){
					return r+1;
				},
				cs_manip,
				tree
			);
		
			// シリアライズ
			auto sl = tree.getRoot();
			auto ar0 = TestTree<TreeNode_t>::Plain(sl);
			std::stringstream ss;
			{
				cereal::JSONOutputArchive oa(ss);
				oa(cereal::make_nvp("test", sl));
			}
			// デシリアライズ
			{
				cereal::JSONInputArchive ia(ss);
				ia(cereal::make_nvp("test", sl));
			}
			auto ar1 = TestTree<TreeNode_t>::Plain(sl);
		
			// シリアライズ前後でデータを比べる
			ASSERT_NO_FATAL_FAILURE(CheckEqual(ar0, ar1));
		}
		TEST_F(TreeNodeTest, TreeDepth) {
			auto& rd = this->mt();
			// ランダムなツリーを作る
			TestTree<TreeNode_t>	tree(0);
			RandomManipulation(
				rd,
				100,
				[](const int r){
					return r+1;
				},
				cs_manip,
				tree
			);
	
			// Iterate関数を使ってカウント
			int depth = 0;
			tree.getRoot()->iterateDepthFirst<false>([&depth](auto&, int d){
				depth = std::max(depth, d);
				return Iterate::StepIn;
			});
			// GetDepthでカウント
			int depth2 = tree.getRoot()->getDepth();
			EXPECT_EQ(depth, depth2);
		}
		namespace {
			void _ChkCount(std::vector<int>& count, const SPTreeNode& sp) {
				ASSERT_LT(sp->value, int(count.size()));
				++count[sp->value];
				if(auto c = sp->getChild()) {
					do {
						_ChkCount(count, c);
					} while((c = c->getSibling()));
				}
			}
			// 0からmまでの数値が1回ずつ出現するかのチェック
			void ChkCount(const SPTreeNode& sp, const int m) {
				std::vector<int> count(m);
				std::fill(count.begin(), count.end(), 0);
				_ChkCount(count, sp);
				for(auto& c : count) {
					ASSERT_EQ(1, c);
				}
			}
			// ちゃんとソートされているか
			void ChkSort(const SPTreeNode& sp) {
				if(auto c = sp->getChild()) {
					int value = c->value;
					while((c = c->getSibling())) {
						ASSERT_LE(value, c->value);
						value = c->value;
						ChkSort(c);
					}
				}
			}
		}
		TEST_F(TreeNodeTest, Sort) {
			auto& rd = this->mt();
			// ランダムなツリーを作る
			TestTree<TreeNode_t>	tree(0);
			int value = 0;
			constexpr int N_Entry = 100;
			RandomManipulation(
				rd,
				N_Entry,
				[&value](const int){
					return ++value;
				},
				cs_manip_noremove,
				tree
			);
			SPTreeNode root = tree.getRoot();
			ChkCount(root, value+1);
			root->sortChild(
				[](const auto& a, const auto& b){
					return a->value < b->value;
				},
				true
			);
			ChkSort(root);
			ChkCount(root, value+1);
		}
		namespace {
			DefineEnum(
				Action,
				(ParentChange)
				(AddChild)
				(RemChild)
			);
			class TreeNotify_t;
			using SP = std::shared_ptr<TreeNotify_t>;
			struct ActInfo {
				const TreeNotify_t*		target;
				Action					action;
				const TreeNotify_t*		node;
	
				using Ptr = TreeNode<TreeNotify_t>*;
				ActInfo(const Ptr tgt,
						Action type,
						const Ptr node);
				static ActInfo AsParentChange(const Ptr tgt, const Ptr node);
				static ActInfo AsAddChild(const Ptr tgt, const Ptr node);
				static ActInfo AsRemChild(const Ptr tgt, const Ptr node);
				bool operator == (const ActInfo& info) const {
					if(info.target != target ||
							info.action != action)
					{
						return false;
					}
					return info.node == node;
				}
			};
			using ActV = std::vector<ActInfo>;
			ActV	g_act;
			class TreeNotify_t : public TreeNode<TreeNotify_t> {
				friend class TreeNode<TreeNotify_t>;
				private:
					static void OnParentChange(const pointer self, const SP& node) {
						g_act.emplace_back(ActInfo::AsParentChange(self, node.get()));
					}
					static void OnChildAdded(const pointer self, const SP& node) {
						g_act.emplace_back(ActInfo::AsAddChild(self, node.get()));
					}
					static void OnChildRemove(const pointer self, const SP& node) {
						g_act.emplace_back(ActInfo::AsRemChild(self, node.get()));
					}
				public:
					int		value;
					TreeNotify_t(const int v): value(v) {}
			};
			ActInfo::ActInfo(const Ptr tgt,
							Action type,
							const Ptr node):
				target(static_cast<const TreeNotify_t*>(tgt)),
				action(type),
				node(static_cast<const TreeNotify_t*>(node))
			{}
			ActInfo ActInfo::AsParentChange(const Ptr tgt, const Ptr node) {
				return ActInfo(tgt, Action::ParentChange, node);
			}
			ActInfo ActInfo::AsAddChild(const Ptr tgt, const Ptr node) {
				return ActInfo(tgt, Action::AddChild, node);
			}
			ActInfo ActInfo::AsRemChild(const Ptr tgt, const Ptr node) {
				return ActInfo(tgt, Action::RemChild, node);
			}
		}
		TEST_F(TreeNodeTest, Notify) {
			auto& rd = this->mt();
			// ランダムなツリーを作る
			TestTree<TreeNotify_t>	tree(0);
			int value = 0;
			constexpr int N_Manip = 100;
			RandomManipulation(
				rd,
				N_Manip,
				[&value](const int){
					return ++value;
				},
				{Manip::Add},
				tree
			);
			ActV act;
			tree.setCallback(
				// Add
				[&act](const auto& tgt, const auto& node){
					act.emplace_back(ActInfo::AsAddChild(tgt.get(), node.get()));
				},
				// Rem
				[&act](const auto& tgt, const auto& node){
					act.emplace_back(ActInfo::AsRemChild(tgt.get(), node.get()));
				},
				// Parent
				[&act](const auto& tgt, const auto& node){
					act.emplace_back(ActInfo::AsParentChange(tgt.get(), node.get()));
				}
			);
			// g_act = TreeNode側で検知したイベントリスト
			// act = 確認用のイベントリスト
			g_act.clear();
			RandomManipulation(
				rd,
				100,
				[&value](const int){
					return ++value;
				},
				{Manip::Add, Manip::Remove, Manip::Recompose},
				tree
			);
			ASSERT_EQ(act.size(), g_act.size());
			const int n = act.size();
			for(int i=0 ; i<n ; i++) {
				ASSERT_EQ(act[i], g_act[i]);
			}
		}
	}
}
namespace cereal {
	template <class Ar>
	struct specialize<Ar, spi::test::TreeNode_t, cereal::specialization::member_serialize> {};
}
