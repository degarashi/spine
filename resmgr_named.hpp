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
	template <class T, class K=std::string>
	class ResMgrName {
		public:
			using value_t = T;
			using shared_t = std::shared_ptr<value_t>;
			using key_t = K;
		private:
			using this_t = ResMgrName<T,K>;
			using tag_t = ResTag<value_t>;
			using Resource = std::unordered_map<key_t, tag_t>;
			using Val2Key = std::unordered_map<const value_t*, key_t>;
			template <bool B>
			class _iterator : public Resource::iterator {
				public:
					using base_t = typename Resource::iterator;
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

			Resource	_resource;
			Val2Key		_v2k;

			void _release(value_t* p) noexcept {
				const auto itr = _v2k.find(p);
				try {
					Assert0(itr != _v2k.end());
					const auto itr2 = _resource.find(itr->second);
					Assert0(itr2 != _resource.end());
					_v2k.erase(itr);
					_resource.erase(itr2);
					p->~value_t();
				} catch(...) {}
			}
			// 無名リソースを作成する際の通し番号
			uint64_t _acounter;
			key_t _makeAKey() {
				key_t key("__anonymous__");
				key += std::to_string(_acounter++);
				return key;
			}

			using DeleteF = std::function<void (value_t*)>;
			const DeleteF	_deleter;

			template <class Ar, class T2, class K2>
			friend void save(Ar&, const ResMgrName<T2,K2>&);
			template <class Ar, class T2, class K2>
			friend void load(Ar&, ResMgrName<T2,K2>&);

		protected:
			//! 継承先のクラスでキーの改変をする必要があればこれをオーバーライドする
			virtual void _modifyResourceName(key_t& /*key*/) const {}

		public:
			ResMgrName():
				_acounter(0),
				_deleter([this](value_t* p){
					this->_release(p);
				})
			{}
			iterator begin() noexcept { return _resource.begin(); }
			iterator end() noexcept { return _resource.end(); }
			const_iterator begin() const noexcept { return _resource.begin(); }
			const_iterator cbegin() const noexcept { return _resource.cbegin(); }
			const_iterator end() const noexcept { return _resource.end(); }
			const_iterator cend() const noexcept { return _resource.cend(); }

			// (主にデバッグ用)
			bool operator == (const ResMgrName& m) const noexcept {
				if(_resource.size() == m._resource.size()) {
					auto itr0 = _resource.begin(),
						 itr1 = m._resource.begin();
					while(itr0 != _resource.end()) {
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
			// ---- 名前付きリソース作成 ----
			template <class Make>
			auto acquireWithMake(const key_t& k, Make&& make) {
				key_t tk(k);
				_modifyResourceName(tk);
				using ret_t = std::remove_pointer_t<decltype(make(tk))>;
				// 既に同じ名前でリソースを確保済みならばそれを返す
				if(auto ret = get(tk))
					return std::make_pair(std::static_pointer_cast<ret_t>(ret), false);
				// 新しくリソースを作成
				std::shared_ptr<ret_t> p(make(tk), _deleter);
				_resource.emplace(tk, p);
				_v2k.emplace(p.get(), tk);
				return std::make_pair(p, true);
			}
			template <class T2, class... Ts>
			auto emplaceWithType(const key_t& k, Ts&&... ts) {
				return acquireWithMake(k, [&ts...](auto&&){
					return new T2(std::forward<Ts>(ts)...);
				});
			}
			template <class... Ts>
			auto emplace(const key_t& k, Ts&&... ts) {
				return emplaceWithType<value_t>(k, std::forward<Ts>(ts)...);
			}

			// ---- 無名リソース作成 ----
			template <class P>
			auto acquireA(P* ptr) {
				for(;;) {
					// 適当にリソース名を生成して、ダブりがなければOK
					const auto key = _makeAKey();
					auto ret = acquireWithMake(key, [=](auto&&){ return ptr; });
					if(ret.second)
						return ret.first;
				}
			}
			template <class T2, class... Ts>
			auto emplaceA_WithType(Ts&&... ts) {
				return acquireA(new T2(std::forward<Ts>(ts)...));
			}
			template <class... Ts>
			auto emplaceA(Ts&&... ts) {
				return emplaceA_WithType<value_t>(std::forward<Ts>(ts)...);
			}

			Optional<const key_t&> getKey(const shared_t& p) const {
				return getKey(p.get());
			}
			Optional<const key_t&> getKey(const value_t* p) const {
				const auto itr = _v2k.find(p);
				if(itr != _v2k.end())
					return itr->second;
				return none;
			}
			shared_t get(const key_t& k) {
				key_t tk(k);
				const auto itr = _resource.find(tk);
				if(itr != _resource.end()) {
					shared_t ret(itr->second.weak.lock());
					D_Assert0(ret);
					return ret;
				}
				return nullptr;
			}
			std::size_t size() const noexcept {
				return _resource.size();
			}
	};
}
