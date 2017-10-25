#pragma once
#include "singleton.hpp"
#include "restag.hpp"
#include "optional.hpp"
#include <unordered_map>

namespace spi {
	//! 名前付きリソースマネージャ
	/*!
		中身の保持はすべてスマートポインタで行う
		シングルスレッド動作
	*/
	template <
		class T,
		class K = std::string,
		class Allocator = std::allocator<T>
	>
	class ResMgrName {
		public:
			using value_t = T;
			using shared_t = std::shared_ptr<value_t>;
			using key_t = K;
		private:
			using this_t = ResMgrName<T,K>;
			using tag_t = ResTag<value_t>;
			using Map = std::unordered_map<key_t, tag_t>;
			using Val2Key = std::unordered_map<const value_t*, key_t>;
			struct Resource {
				Map			map;
				Val2Key		v2k;

				bool operator == (const Resource& r) const noexcept {
					// v2kは比較しない
					return map == r.map;
				}
				bool operator != (const Resource& r) const noexcept {
					return !(this->operator == (r));
				}
			};
			using Resource_SP = std::shared_ptr<Resource>;

			template <bool B>
			class _iterator : public Map::iterator {
				public:
					using base_t = typename Map::iterator;
					_iterator() = default;
					_iterator(const base_t& b):
						base_t(b)
					{}
					using v_t = std::conditional_t<B, const value_t, value_t>;
					using s_t = std::shared_ptr<v_t>;
					s_t operator *() const {
						return base_t::operator*().second.weak.lock();
					}
					s_t operator ->() const {
						return base_t::operator*().second.weak.lock();
					}
			};
			using iterator = _iterator<false>;
			using const_iterator = _iterator<true>;

			Resource_SP		_resource;

			template <class T2>
			static void _Release(const Resource_SP& r, T2* p) noexcept {
				auto& m = r->map;
				auto& v2k = r->v2k;
				typename Allocator::template rebind<T2>::other alc;
				const auto itr = v2k.find(p);
				try {
					Assert0(itr != v2k.end());
					const auto itr2 = m.find(itr->second);
					Assert0(itr2 != m.end());
					v2k.erase(itr);
					m.erase(itr2);
					alc.destroy(p);
					alc.deallocate(p, 1);
				} catch(...) {}
			}
			const static key_t s_anonymousPrefix;
			// 無名リソースを作成する際の通し番号
			uint64_t _acounter;
			key_t _makeAKey() {
				// 適当にリソース名を生成して、ダブりがなければOK
				for(;;) {
					key_t key(s_anonymousPrefix);
					key += std::to_string(_acounter++);
					if(_resource->map.count(key) == 0)
						return key;
				}
			}

			template <class Ar, class T2, class K2>
			friend void save(Ar&, const ResMgrName<T2,K2>&);
			template <class Ar, class T2, class K2>
			friend void load(Ar&, ResMgrName<T2,K2>&);

			key_t _convertKey(const key_t& s) {
				key_t tk(s);
				if(!_HasAnonymousPrefix(tk))
					_modifyResourceName(tk);
				return tk;
			}
			static bool _HasAnonymousPrefix(const key_t& s) {
				if(s.size() >= s_anonymousPrefix.size()) {
					auto itr0 = s.cbegin(),
						 itr1 = s_anonymousPrefix.cbegin();
					do {
						if(*itr0 != *itr1)
							return false;
						++itr0;
						++itr1;
					} while(itr1 != s_anonymousPrefix.cend());
					return true;
				}
				return false;
			}
			template <class T2>
			auto _getDeleter() {
				return [r=_resource](T2 *const p){ _Release(r, p); };
			}
			//! key->resource の関連付けを解除
			tag_t _eraseKey2Tag(const key_t& k_from) {
				auto& m = _resource->map;
				const auto itr = m.find(k_from);
				D_Assert0(itr != m.end());

				tag_t tag = itr->second;
				m.erase(itr);
				return tag;
			}
			//! resource -> key の関連付けを解除
			void _eraseV2K(const value_t* ptr) {
				auto& v2k = _resource->v2k;
				const auto itr = v2k.find(ptr);
				D_Assert0(itr != v2k.end());
				v2k.erase(itr);
			}
			//! あるキーに関連付けられているリソースを別の名前と関連付けしなおす
			void _renameEntry(const key_t& k_from, const key_t& k_to) {
				tag_t tag = _eraseKey2Tag(k_from);
				_eraseV2K(tag.ptr);

				_resource->map.emplace(k_to, tag);
				_resource->v2k.emplace(tag.ptr, k_to);
			}
			template <class T2, class Make>
			auto _addEntry(const key_t& k, const Make& make) {
				// 新しくリソースを作成
				Constructor<T2> ctor;
				make(k, ctor);

				std::shared_ptr<T2> sp(
					ctor.pointer,
					_getDeleter<T2>()
				);
				auto& res = *_resource;
				res.map.emplace(k, sp);
				res.v2k.emplace(sp.get(), k);
				return sp;
			}
		protected:
			//! 継承先のクラスでキーの改変をする必要があればこれをオーバーライドする
			virtual void _modifyResourceName(key_t& /*key*/) const {}

		public:
			ResMgrName():
				_resource(std::make_shared<Resource>()),
				_acounter(0)
			{}
			iterator begin() noexcept { return _resource->map.begin(); }
			iterator end() noexcept { return _resource->map.end(); }
			const_iterator begin() const noexcept { return _resource->map.begin(); }
			const_iterator cbegin() const noexcept { return _resource->map.cbegin(); }
			const_iterator end() const noexcept { return _resource->map.end(); }
			const_iterator cend() const noexcept { return _resource->map.cend(); }

			// (主にデバッグ用)
			bool operator == (const ResMgrName& m) const noexcept {
				if(*_resource != *m._resource)
					return false;
				return _acounter == m._acounter;
			}
			bool operator != (const ResMgrName& m) const noexcept {
				return !(this->operator == (m));
			}
			template <class T2>
			struct Constructor {
				using Alc = typename Allocator::template rebind<T2>::other;
				Alc			alc;
				T2*			pointer;
				Constructor():
					pointer(alc.allocate(1))
				{}
				template <class... Ts>
				void operator()(Ts&&... ts) {
					alc.construct(pointer, std::forward<Ts>(ts)...);
				}
			};

			// ---- 名前付きリソース作成 ----
			template <class T2, class Make>
			auto acquireWithMake(const key_t& k, Make&& make) {
				const key_t tk = _convertKey(k);
				// 既に同じ名前でリソースを確保済みならばそれを返す
				if(auto ret = get(tk))
					return std::make_pair(std::static_pointer_cast<T2>(ret), false);
				return std::make_pair(_addEntry<T2>(tk, make), true);
			}
			//! 型を指定してのリソース確保
			template <class T2, class... Ts>
			auto emplaceWithType(const key_t& k, Ts&&... ts) {
				return acquireWithMake<T2>(k, [&ts...](auto& /*key*/, auto&& mk){
					mk(std::forward<Ts>(ts)...);
				});
			}
			template <class... Ts>
			auto emplace(const key_t& k, Ts&&... ts) {
				return emplaceWithType<value_t>(k, std::forward<Ts>(ts)...);
			}
			bool setAnonymous(const key_t& k, key_t* oldKey=nullptr) {
				auto& m = _resource->map;
				const auto itr = m.find(k);
				if(itr != m.end()) {
					const auto nk = _makeAKey();
					if(oldKey)
						*oldKey = nk;
					_renameEntry(k, nk);
					return true;
				}
				return false;
			}
			/*!
				\param[in] oldKey	有効なポインタを指定すれば上書きされたリソースの新しいキー名がセットされる
				\return std::pair(first=置き換えられた後のリソースハンドル, second=古いリソースが置き換えられたかのフラグ)
			*/
			template <class T2, class Make>
			auto replaceWithMake(const key_t& k, const Make& make, key_t* oldKey=nullptr) {
				const key_t tk = _convertKey(k);
				// 既に同じ名前でリソースを確保済みならばキーを無名リソースキーに書き換え
				const bool hasOld = setAnonymous(tk, oldKey);
				return std::make_pair(_addEntry<T2>(tk, make), hasOld);
			}
			template <class T2, class... Ts>
			auto replaceEmplaceWithType(const key_t& k, key_t* oldKey, Ts&&... ts) {
				return replaceWithMake<T2>(k, [&ts...](auto& /*key*/, auto&& mk){
					mk(std::forward<Ts>(ts)...);
				}, oldKey);
			}
			template <class... Ts>
			auto replaceEmplace(const key_t& k, key_t* oldKey, Ts&&... ts) {
				return replaceEmplaceWithType<value_t>(k, oldKey, std::forward<Ts>(ts)...);
			}

			// ---- 無名リソース作成 ----
			template <class P, class... Ts>
			auto acquireA(Ts&&... ts) {
				const auto key = _makeAKey();
				auto ret = acquireWithMake<P>(key,
							[&ts...](auto& /*key*/, auto&& mk){
								mk(std::forward<Ts>(ts)...);
							});
				D_Assert0(ret.second);
				return ret.first;
			}
			//! データ型を指定しての無名リソース確保
			template <class T2, class... Ts>
			auto emplaceA_WithType(Ts&&... ts) {
				return acquireA<T2>(std::forward<Ts>(ts)...);
			}
			//! 無名リソース確保(データ型 = value_t>
			template <class... Ts>
			auto emplaceA(Ts&&... ts) {
				return emplaceA_WithType<value_t>(std::forward<Ts>(ts)...);
			}

			//! リソースに対応するキーを取得(from shared_ptr)
			Optional<const key_t&> getKey(const shared_t& p) const {
				return getKey(p.get());
			}
			//! リソースに対応するキーを取得(from pointer)
			/*! 管轄外のリソースや不正なポインタを入力した場合、noneが返る */
			Optional<const key_t&> getKey(const value_t* p) const {
				auto& v2k = _resource->v2k;
				const auto itr = v2k.find(p);
				if(itr != v2k.end())
					return itr->second;
				return none;
			}
			//! キーに対応するリソースを取り出す(ない場合はnullptrを返す)
			shared_t get(const key_t& k) {
				const key_t tk = _convertKey(k);
				auto& map = _resource->map;
				const auto itr = map.find(tk);
				if(itr != map.end()) {
					shared_t ret(itr->second.weak.lock());
					D_Assert0(ret);
					return ret;
				}
				return nullptr;
			}
			//! 確保されたリソース数
			std::size_t size() const noexcept {
				return _resource->map.size();
			}
	};
	template <class T>
	const char* GetAnonymousPrefix(T*) { return u8"__anonymous__"; }
	inline const char16_t* GetAnonymousPrefix(std::u16string*) { return  u"__anonymous__"; }
	inline const char32_t* GetAnonymousPrefix(std::u32string*) { return  U"__anonymous__"; }

	template <class T, class K, class A>
	const typename ResMgrName<T,K,A>::key_t ResMgrName<T,K,A>::s_anonymousPrefix(GetAnonymousPrefix((K*)nullptr));
}
