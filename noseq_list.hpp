#pragma once
#include <vector>
#include <cstdint>
#include <limits>
#include "optional.hpp"
#include "lubee/wrapper.hpp"
#include "enum.hpp"

namespace spi {
	namespace {
		namespace _noseq_list {
			template <class T>
			struct RemoveRef {
				using T2 = typename std::remove_reference<T>::type;
				using reference = T2&;
				using pointer = T2*;
			};
			template <class T>
			struct RemoveRef <T*> {
				using reference = T*&;
				using pointer = T**;
			};
			template <class Data, class Base>
			class IteratorBase : public Base {
				private:
					using base_t = Base;
				public:
					using this_t = IteratorBase<Data, Base>;
					using base_t::base_t;
					using value_type = Data;
					using pointer = typename RemoveRef<Data>::pointer;
					using reference = typename RemoveRef<Data>::reference;

					IteratorBase() = default;
					IteratorBase(const base_t& base):
						base_t(base)
					{}
					pointer operator ->() const noexcept {
						return &this->operator *();
					}
					reference operator *() const noexcept {
						return (reference)base_t::operator*();
					}
					this_t& operator -- () noexcept {
						base_t::operator --();
						return *this;
					}
					this_t operator -- (const int) const noexcept {
						const this_t ret = *this;
						this->operator --();
						return ret;
					}
					this_t& operator ++ () noexcept {
						base_t::operator ++();
						return *this;
					}
					this_t operator ++ (const int) const noexcept {
						const this_t ret = *this;
						this->operator ++();
						return ret;
					}
					this_t operator + (const int n) const noexcept {
						this_t ret = *this;
						ret += n;
						return ret;
					}
					this_t operator - (const int n) const noexcept {
						this_t ret = *this;
						ret -= n;
						return ret;
					}
					using base_t::operator -;

					this_t& operator += (const int n) noexcept {
						base_t::operator += (n);
						return *this;
					}
					this_t& operator -= (const int n) noexcept {
						base_t::operator -= (n);
						return *this;
					}
			};
		}
	}

	//! ユーザーの要素を格納
	template <class Value>
	struct UData {
		spi::Optional<Value>	value;
		//! ユニークID。要素の配列内移動をする際に使用
		id_t					uid;

		UData() = default;
		template <class T2>
		UData(T2&& t, const id_t id):
			value(std::forward<T2>(t)),
			uid(id)
		{}

		bool operator == (const UData& ud) const noexcept {
			return value == ud.value &&
					uid == ud.uid;
		}
		template <class Ar, class V>
		friend void serialize(Ar& ar, UData<V>&);
	};
	template <class Id>
	struct IDS {
		using id_t = Id;
		DefineEnum(Type, (None)(Free)(Obj));
		Type	type = Type::None;
		id_t	value;

		bool operator == (const IDS& ids) const noexcept {
			return type == ids.type &&
					value == ids.value;
		}
		static IDS AsFreeId(const id_t id) {
			return IDS{Type::Free, id};
		}
		static IDS AsObjId(const id_t id) {
			return IDS{Type::Obj, id};
		}

		template <class Ar, class Id2>
		friend void serialize(Ar& ar, IDS<Id2>&);
	};
	template <class Value, class Id>
	struct Entry {
		using id_t = Id;
		UData<Value>		udata;
		IDS<Id>				ids;

		Entry() = default;
		template <class T2>
		Entry(T2&& t, const id_t idx):
			udata(std::forward<T2>(t),idx),
			ids(IDS<Id>::AsObjId(idx))
		{}
		//! deserialize用
		Entry(spi::Optional<Value>&& value, const id_t uid, const IDS<Id> ids):
			udata(std::move(value), uid), ids(ids)
		{}
		bool operator == (const Entry& e) const {
			return udata == e.udata &&
					ids == e.ids;
		}
		template <class Ar, class V, class Id2>
		friend void serialize(Ar& ar, Entry<V,Id2>&);
	};

	//! 順序なしのID付きリスト
	/*! 全走査を速く、要素の追加削除を速く(走査中はNG)、要素の順序はどうでもいい、あまり余計なメモリは食わないように・・というクラス */
	template <
		class T,
		class Allocator = std::allocator<T>,
		class IDType = uint_fast32_t
	>
	class noseq_list {
		public:
			using this_t = noseq_list<T,Allocator,IDType>;
			using id_t = IDType;
			using value_t = T;
			using entry_t = Entry<value_t, id_t>;
			using ids_t = IDS<id_t>;

			using Array = std::vector<entry_t, typename Allocator::template rebind<entry_t>::other>;
			Array		_array;
			id_t		_nFree = 0,		//!< 空きブロック数
						_firstFree;		//!< 最初の空きブロックインデックス
			//! 現在削除処理中かのフラグ
			bool		_bRemoving = false;
			using RemList = std::vector<id_t>;
			//! 削除中フラグが立っている時に削除予定のオブジェクトを記録しておく配列
			RemList		_remList;

			template <class Ar, class T2, class Alc, class Id>
			friend void serialize(Ar&, noseq_list<T2,Alc,Id>&);

		public:
			noseq_list() noexcept {
				static_assert(std::is_integral<id_t>::value, "typename ID should be Integral type");
			}
			noseq_list(const noseq_list&) = default;
			noseq_list(noseq_list&& sl) noexcept:
				_array(std::move(sl._array)),
				_nFree(sl._nFree),
				_firstFree(sl._firstFree)
			{
				sl.clear();
			}
			noseq_list& operator = (noseq_list&& ns) noexcept {
				this->~noseq_list();
				new(this) noseq_list(std::move(ns));
				return *this;
			}
			template <class... Ts>
			id_t emplace(Ts&&... ts) {
				return add(value_t(std::forward<Ts>(ts)...));
			}
			template <class T2>
			id_t add(T2&& t) {
				if(_nFree == 0) {
					// 空きが無いので配列の拡張
					const id_t sz = _array.size();
					_array.emplace_back(std::forward<T2>(t), sz);
					return sz;
				}
				const id_t ret = _firstFree;					// IDPairを書き込む場所
				const id_t objI = _array.size() - _nFree;		// ユーザーデータを書き込む場所
				--_nFree;
				auto& objE = _array[objI].udata;
				objE.value = std::forward<T2>(t);		// ユーザーデータの書き込み
				objE.uid = ret;

				auto& idE = _array[ret].ids;
				D_Assert0(idE.type == ids_t::Type::Free);
				_firstFree = idE.value;		// フリーリストの先頭を書き換え
				idE = ids_t::AsObjId(objI);	// IDPairの初期化
				return ret;
			}
			void rem(const id_t uindex) {
				D_Assert(has(uindex), "invalid resource number %1%", uindex);
				if(!_bRemoving) {
					_bRemoving = true;

					auto& ids = _array[uindex].ids;
					// 削除対象のnoseqインデックスを受け取る
					D_Assert0(ids.type == ids_t::Type::Obj);
					const id_t objI = ids.value;
					const id_t backI = _array.size()-_nFree-1;
					if(objI != backI) {
						// 最後尾と削除予定の要素を交換
						std::swap(_array[backI].udata, _array[objI].udata);
						// UIDとindex対応の書き換え
						_array[_array[objI].udata.uid].ids = ids_t::AsObjId(objI);
					}
					// 要素を解放
					_array[backI].udata.value = none;
					// フリーリストをつなぎ替える
					ids = ids_t::AsFreeId(_firstFree);
					_firstFree = uindex;
					++_nFree;

					_bRemoving = false;
					while(!_remList.empty()) {
						const id_t tmp = _remList[0];
						_remList.erase(_remList.begin());
						rem(tmp);
					}
				} else
					_remList.push_back(uindex);
			}
			value_t& get(const id_t uindex) {
				D_Assert(has(uindex), "invalid resource number %1%", uindex);
				auto& ar = _array[uindex];
				D_Assert0(ar.ids.type == ids_t::Type::Obj);
				const id_t idx = ar.ids.value;
				return *_array[idx].udata.value;
			}
			const value_t& get(const id_t uindex) const {
				return const_cast<noseq_list*>(this)->get(uindex);
			}
			//! IDが有効か判定
			bool has(const id_t uindex) const {
				if(_array.size() <= uindex || _array[uindex].ids.type != ids_t::Type::Obj)
					return false;
				return true;
			}

			template <class Data, class Base>
			using ibase = _noseq_list::IteratorBase<Data,Base>;
			using iterator = ibase<T, typename Array::iterator>;
			using const_iterator = ibase<const T, typename Array::const_iterator>;
			using reverse_iterator = ibase<T, std::reverse_iterator<typename Array::iterator>>;
			using const_reverse_iterator = ibase<const T, std::reverse_iterator<typename Array::const_iterator>>;
			// イテレータの範囲はフリー要素を勘定に入れる
			iterator begin() {
				return _array.begin();
			}
			iterator end() {
				return _array.end()-_nFree;
			}
			reverse_iterator rbegin() {
				return _array.rbegin()+_nFree;
			}
			reverse_iterator rend() {
				return _array.rend();
			}
			const_iterator cbegin() const {
				return _array.cbegin();
			}
			const_iterator cend() const {
				return _array.cend()-_nFree;
			}
			const_iterator begin() const {
				return _array.begin();
			}
			const_iterator end() const {
				return _array.end()-_nFree;
			}
			const_reverse_iterator crbegin() const {
				return const_cast<noseq_list*>(this)->rbegin();
			}
			const_reverse_iterator crend() const {
				return const_cast<noseq_list*>(this)->rend();
			}
			std::size_t size() const {
				return _array.size() - _nFree;
			}
			void clear() {
				D_Assert0(!_bRemoving && _remList.empty());
				_array.clear();
				_nFree = 0;
			}
			bool empty() const {
				return size() == 0;
			}
			//! 主にデバッグ用。内部状態も含めて比較
			bool operator == (const noseq_list& lst) const {
				D_Assert0(!_bRemoving);
				return _nFree == lst._nFree &&
						_firstFree == lst._firstFree &&
						_remList == lst._remList &&
						_array == lst._array;
			}
	};
	namespace _noseq_list {}
}
