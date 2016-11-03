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
		class Allocator = std::allocator<std::pair<const T, K>>
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
			};
			using Resource_SP = std::shared_ptr<Resource>;

			template <bool B>
			class _iterator : public Map::iterator {
				public:
					using base_t = typename Map::iterator;
					using base_t::base_t;
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
			// 無名リソースを作成する際の通し番号
			uint64_t _acounter;
			key_t _makeAKey() {
				key_t key("__anonymous__");
				key += std::to_string(_acounter++);
				return key;
			}

			template <class Ar, class T2, class K2>
			friend void save(Ar&, const ResMgrName<T2,K2>&);
			template <class Ar, class T2, class K2>
			friend void load(Ar&, ResMgrName<T2,K2>&);

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
				auto	&m0 = _resource->map,
						&m1 = m._resource->map;
				if(m0.size() == m1.size()) {
					auto itr0 = m0.begin(),
						 itr1 = m1.begin();
					while(itr0 != m0.end()) {
						if(*itr0 != *itr1)
							return false;
						++itr0;
						++itr1;
					}
					return _acounter == m._acounter;
				}
				return false;
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
				key_t tk(k);
				_modifyResourceName(tk);
				// 既に同じ名前でリソースを確保済みならばそれを返す
				if(auto ret = get(tk))
					return std::make_pair(std::static_pointer_cast<T2>(ret), false);
				// 新しくリソースを作成
				Constructor<T2> ctor;
				make(tk, ctor);

				auto& res = *_resource;
				std::shared_ptr<T2> sp(
					ctor.pointer,
					[r=_resource](T2 *const p){
						_Release(r, p);
					}
				);
				res.map.emplace(tk, sp);
				res.v2k.emplace(sp.get(), tk);
				return std::make_pair(sp, true);
			}
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

			// ---- 無名リソース作成 ----
			template <class P, class... Ts>
			auto acquireA(Ts&&... ts) {
				for(;;) {
					// 適当にリソース名を生成して、ダブりがなければOK
					const auto key = _makeAKey();
					auto ret = acquireWithMake<P>(key,
								[&ts...](auto& /*key*/, auto&& mk){
									mk(std::forward<Ts>(ts)...);
								});
					if(ret.second)
						return ret.first;
				}
			}
			template <class T2, class... Ts>
			auto emplaceA_WithType(Ts&&... ts) {
				return acquireA<T2>(std::forward<Ts>(ts)...);
			}
			template <class... Ts>
			auto emplaceA(Ts&&... ts) {
				return emplaceA_WithType<value_t>(std::forward<Ts>(ts)...);
			}

			Optional<const key_t&> getKey(const shared_t& p) const {
				return getKey(p.get());
			}
			Optional<const key_t&> getKey(const value_t* p) const {
				auto& v2k = _resource->v2k;
				const auto itr = v2k.find(p);
				if(itr != v2k.end())
					return itr->second;
				return none;
			}
			shared_t get(const key_t& k) {
				key_t tk(k);
				auto& map = _resource->map;
				const auto itr = map.find(tk);
				if(itr != map.end()) {
					shared_t ret(itr->second.weak.lock());
					D_Assert0(ret);
					return ret;
				}
				return nullptr;
			}
			std::size_t size() const noexcept {
				return _resource->map.size();
			}
	};
}
