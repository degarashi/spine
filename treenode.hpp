#pragma once
#include "lubee/meta/check_macro.hpp"
#include "lubee/meta/enable_if.hpp"
#include "lubee/error.hpp"
#include "enum.hpp"
#include <vector>
#include <algorithm>
#include <memory>

namespace spi {
	DEF_HASMETHOD(clone)

	// cloneメソッドを持っていればそれを呼ぶ
	template <class T, ENABLE_IF(HasMethod_clone_t<T>{})>
	auto CopyAsSP(T& t) {
		return t.clone();
	}
	// cloneメソッドを持っていない時はコピーコンストラクタで対処
	template <class T, ENABLE_IF(!HasMethod_clone_t<T>{})>
	auto CopyAsSP(T& t) {
		using Tc = typename std::remove_const<T>::type;
		return std::shared_ptr<Tc>(new Tc(t));
	}

	//! iterateDepthFirstの戻り値
	DefineEnum(
		Iterate,
		(ReturnFromChild)	//!< 内部用
		(StepIn)			//!< 子を巡回
		(Next)				//!< 子を巡回せず兄弟ノードへ進む
		(Quit)				//!< 直ちに巡回を終える
	);
	//! 汎用ツリー構造
	/*! Tは必ずTreeNode<T>を継承する前提 */
	template <class T>
	class TreeNode : public std::enable_shared_from_this<T> {
		// 巡回参照を避けるために親ノードはweak_ptrで格納
		public:
			using Iterate = ::spi::Iterate;
			using this_t = TreeNode<T>;
			using pointer = this_t*;
			using element_type = T;
			using SP = std::shared_ptr<T>;
			using SPC = std::shared_ptr<const T>;
			using WP = std::weak_ptr<T>;
			using SPVector = std::vector<SP>;
			using SPCVector = std::vector<SPC>;
			using PVector = std::vector<T*>;
			using PCVector = std::vector<const T*>;

		private:
			template <class Ar, class T2>
			friend void serialize(Ar&, TreeNode<T2>&);

			SP			_spChild,
						_spSibling;
			WP			_wpParent,
						_wpPrevSibling;

			// --- ノードつなぎ変え時に呼ばれる関数. 継承先クラスが適時オーバーライドする ---
			static void OnParentChange(const TreeNode* /*self*/, const SP& /*node*/) {}
			static void OnChildRemove(const TreeNode* /*self*/, const SP& /*node*/) {}
			static void OnChildAdded(const TreeNode* /*self*/, const SP& /*node*/) {}

		private:
			//! 決められた回数分、タブを出力
			static void _PrintIndent(std::ostream& os, int n) {
				while(--n >= 0)
					os << '\t';
			}
			template <class CB>
			void _iterateAll(CB&& cb) const {
				auto* self = const_cast<this_t*>(this);
				self->template iterateDepthFirst<true>([&cb](auto& nd, int){
					cb(nd);
					return Iterate::StepIn;
				});
			}
			template <class T_SP>
			std::vector<T_SP> _plain() const {
				std::vector<T_SP> spv;
				_iterateAll([&spv](auto& nd){
					spv.emplace_back(nd.shared_from_this());
				});
				return spv;
			}
			template <class T_PTR>
			std::vector<T_PTR> _plainPtr() const {
				std::vector<T_PTR> pv;
				_iterateAll([&pv](auto& nd){
					pv.emplace_back(&nd);
				});
				return pv;
			}
			void _isolate() {
				if(const auto p = getPrevSibling())
					p->_spSibling = getSibling();
				if(const auto s = getSibling()) {
					s->_wpPrevSibling = _wpPrevSibling;
					_spSibling.reset();
				}
				_wpPrevSibling.reset();
				if(auto s = getParent()) {
					T::OnParentChange(this, nullptr);
					_wpParent.reset();
				}
			}
			void _setParent(const SP& s) NOEXCEPT_IF_RELEASE {
				D_Assert(s.get() != this, "self-reference detected");
				D_Assert(isolating(), "target node must isolated");
				T::OnParentChange(this, s);
				_wpParent = s;
			}
			template <class S, class CB>
			static void _IterateChild(S& node, CB&& cb) noexcept(noexcept(cb(node.getChild()))) {
				if(auto sp = node.getChild()) {
					do {
						if(!cb(sp))
							break;
						sp = sp->getSibling();
					} while(sp);
				}
			}
		public:
			TreeNode() = default;
			explicit TreeNode(TreeNode&& t) = default;
			//! copy-ctorに置いてはリンク情報をコピーしない
			explicit TreeNode(const TreeNode&) {}
			// コピー禁止
			TreeNode& operator = (const TreeNode&) = delete;
			// ムーブは可
			TreeNode& operator = (TreeNode&& t) = default;

			//! 孤立判定
			bool isolating() const {
				return _wpParent.expired() && !hasSibling();
			}
			//! 前後どちらかでも兄弟ノードを持っているか
			bool hasSibling() const {
				return !_wpPrevSibling.expired() || _spSibling;
			}
			//! 直下の子ノードだけ巡回
			template <class CB>
			void iterateChild(CB&& cb) const { _IterateChild(*this, std::forward<CB>(cb)); }
			template <class CB>
			void iterateChild(CB&& cb) { _IterateChild(*this, std::forward<CB>(cb)); }
			//! 任意の基準で子ノードをソート
			template <class CMP>
			void sortChild(CMP&& cmp, const bool recursive) {
				if(auto c = getChild()) {
					SPVector spv;
					iterateChild([&spv](auto& nd){
						spv.emplace_back(nd);
						return true;
					});
					std::sort(spv.begin(), spv.end(), cmp);
					SP	cur = spv.front(),
						pcur;
					_spChild = cur;
					auto itr = spv.begin();
					do {
						cur = *itr;
						if(pcur) {
							cur->_wpPrevSibling = pcur;
							pcur->_spSibling = cur;
						} else
							cur->_wpPrevSibling.reset();
						pcur = cur;
					} while(++itr != spv.end());
					cur->_spSibling.reset();

					if(recursive) {
						for(auto& s : spv)
							s->sortChild(cmp, recursive);
					}
				}
			}

			SP getParent() const {
				return SP(_wpParent.lock());
			}
			const SP& getChild() const {
				return _spChild;
			}
			const SP& getSibling() const {
				return _spSibling;
			}
			//! 直前の兄弟ノードを取得
			SP getPrevSibling(const bool bLoop=false) const {
				if(SP ret = _wpPrevSibling.lock())
					return ret;
				if(bLoop) {
					auto cur = getParent();
					if(!cur)
						return const_cast<this_t*>(this)->shared_from_this();
					cur = cur->getChild();
					while(auto next = cur->getSibling())
						cur = next;
					return cur;
				}
				return nullptr;
			}
			void removeChild(const SP& target) {
				D_Assert0(_spChild);
				if(_spChild == target) {
					T::OnChildRemove(this, target);
					// 最初の子ノードが削除対象
					_spChild = target->getSibling();
					target->_isolate();
				} else {
					// 兄弟ノードのどれかが削除対象
					_spChild->removeSibling(nullptr, target);
				}
			}
			void removeSibling(pointer prev, const SP& target) {
				if(target.get() == this) {
					if(auto sp = getParent())
						T::OnChildRemove(sp.get(), this->shared_from_this());
					// このノードが削除対象
					D_Assert0(prev);
					_isolate();
					return;
				}
				D_Assert0(_spSibling);
				_spSibling->removeSibling(this, target);
			}
			void addChild(const SP& s) {
				D_Assert(s.get() != this, "self-reference detected");
				D_Assert(s->isolating(), "target node must isolated");
				if(_spChild)
					_spChild->addSibling(s);
				else {
					_spChild = s;
					s->_setParent(this->shared_from_this());
					T::OnChildAdded(this, s);
				}
			}
			void addSibling(const SP& s) {
				D_Assert(s.get() != this, "self-reference detected");
				D_Assert(s->isolating(), "target node must isolated");
				if(_spSibling)
					_spSibling->addSibling(s);
				else {
					_spSibling = s;
					s->_setParent(_wpParent.lock());
					s->_wpPrevSibling = this->shared_from_this();
					if(const auto sp = getParent())
						T::OnChildAdded(sp.get(), s);
				}
			}
			//! 深さ優先で巡回
			/*! \tparam Sib		兄弟ノードを巡回対象に加えるか
				\tparam BConst	trueならconst巡回 */
			template <bool Sib, class Callback, bool BConst=false>
			Iterate iterateDepthFirst(Callback&& cb, int depth=0) {
				using thistc = std::conditional_t<BConst, const T, T>;
				Iterate t = cb(static_cast<thistc&>(*this), depth);
				if(t == Iterate::Quit)
					return Iterate::Quit;
				if(t == Iterate::StepIn) {
					if(_spChild) {
						if(_spChild->template iterateDepthFirst<true>(std::forward<Callback>(cb), depth+1) == Iterate::Quit)
							return Iterate::Quit;
					}
				}
				if(Sib && _spSibling)
					return _spSibling->template iterateDepthFirst<true>(std::forward<Callback>(cb), depth);
				return Iterate::ReturnFromChild;
			}
			template <bool Sib, class Callback>
			Iterate iterateDepthFirst(Callback&& cb, int depth=0) const {
				return const_cast<this_t*>(this)->template iterateDepthFirst<Sib, Callback, true>(std::forward<Callback>(cb), depth);
			}
			template <class Callback>
			SP find(Callback&& cb) const {
				SP ret;
				iterateDepthFirst<false>([&cb, &ret](auto& nd, int){
					if(cb(nd)) {
						ret = nd.shared_from_this();
						return Iterate::Quit;
					}
					return Iterate::StepIn;
				});
				return ret;
			}
			//! このノード以下を全て複製
			SP cloneTree(const WP& parent=WP()) const {
				SP sp = CopyAsSP(static_cast<const T&>(*this));
				if(_spChild)
					sp->_spChild = _spChild->cloneTree(sp);
				if(_spSibling) {
					sp->_spSibling = _spSibling->cloneTree(parent);
					sp->_spSibling->_wpPrevSibling = sp;
				}
				sp->_wpParent = parent;
				return sp;
			}
			//! 主にデバッグ用
			void print(std::ostream& os, int indent) const {
				_PrintIndent(os, indent);
				// 明示的なダウンキャスト
				os << *static_cast<const T*>(this);
			}
			//! ツリー構造を配列化
			SPVector plain() {
				return _plain<SP>();
			}
			//! ツリー構造を配列化 (const)
			SPCVector plain() const {
				return _plain<SPC>();
			}
			//! ツリー構造を配列化 (pointer)
			PVector plainPtr() {
				return _plainPtr<T*>();
			}
			//! ツリー構造を配列化 (const pointer)
			PCVector  plainPtr() const {
				return _plainPtr<const T*>();
			}
			//! 最大ツリー深度を取得
			int getDepth() const {
				int depth = 0;
				if(_spChild)
					depth = _spChild->getDepth() + 1;
				if(_spSibling)
					depth = std::max(depth, _spSibling->getDepth());
				return depth;
			}
	};
	template <class T0, class T1, class CMP>
	bool _CompareTree(const TreeNode<T0>& t0, const TreeNode<T1>& t1, CMP&& cmp) {
		auto fnParentIndex = [](const auto& ar, const auto& p){
			if(!p)
				return ar.end() - ar.begin();
			auto itr = std::find_if(ar.begin(), ar.end(), [&p](auto& r){ return r == p.get(); });
			return itr - ar.begin();
		};
		// 配列化して親ノード番号をチェック
		const auto ar0 = t0.plainPtr();
		const auto ar1 = t1.plainPtr();
		if(ar0.size() != ar1.size())
			return false;
		const auto sz = ar0.size();
		for(size_t i=0 ; i<sz ; i++) {
			// 親ノード番号
			const int idx0 = fnParentIndex(ar0, ar0[i]->getParent()),
						idx1 = fnParentIndex(ar1, ar1[i]->getParent());
			if(idx0!=idx1 || !cmp(ar0[i], ar1[i]))
				return false;
		}
		return true;
	}
	// ツリー構造のみを比較する
	template <class T0, class T1>
	bool CompareTreeStructure(const TreeNode<T0>& t0, const TreeNode<T1>& t1) {
		return _CompareTree(t0, t1, [](auto&,auto&){ return true; });
	}
	//! データも含めて比較
	template <class T0, class T1>
	bool CompareTree(const TreeNode<T0>& t0, const TreeNode<T1>& t1) {
		auto cmp = [](auto& sp0, auto& sp1){
			return *sp0 == *sp1;
		};
		return _CompareTree(t0, t1, cmp);
	}
	template <class T>
	inline std::ostream& operator << (std::ostream& os, const TreeNode<T>& t) {
		auto& self = const_cast<TreeNode<T>&>(t);
		self.iterateDepthFirst<false>([&os](auto& s, int indent){
			s.print(os, indent);
			os << std::endl;
			return TreeNode<T>::Iterate::StepIn;
		});
		return os;
	}
}
