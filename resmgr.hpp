#pragma once
#include "restag.hpp"
#include "singleton.hpp"
#include <unordered_set>

namespace spi {
	//! 名前無しリソースマネージャ
	/*!
		中身の保持はすべてスマートポインタで行う
		シングルスレッド動作
	*/
	template <class T>
	class ResMgr {
		public:
			using value_t = T;
			using shared_t = std::shared_ptr<value_t>;
		private:
			template <class Ar, class T2>
			friend void save(Ar&, const ResMgr<T2>&);
			template <class Ar, class T2>
			friend void load(Ar&, ResMgr<T2>&);

			using this_t = ResMgr<T>;
			using tag_t = ResTag<value_t>;

			using Resource = std::unordered_set<tag_t>;
			template <bool B>
			class _iterator : public Resource::iterator {
				public:
					using base_t = typename Resource::iterator;
					using base_t::base_t;
					using v_t = std::conditional_t<B, const value_t, value_t>;
					using s_t = std::shared_ptr<v_t>;
					s_t operator *() const {
						return base_t::operator*().weak.lock();
					}
					s_t operator ->() const {
						return base_t::operator->()->weak.lock();
					}
			};
			using iterator = _iterator<false>;
			using const_iterator = _iterator<true>;

			Resource	_resource;

			void _release(value_t* p) noexcept {
				const auto itr = _resource.find(p);
				try {
					Assert0(itr != _resource.end());
					_resource.erase(itr);
					p->~value_t();
				} catch(...) {}
			}
			using DeleteF = std::function<void (value_t*)>;
			const DeleteF	_deleter;
		public:
			ResMgr():
				_deleter([this](value_t* p){
					this->_release(p);
				})
			{}
			iterator begin() noexcept { return _resource.begin(); }
			iterator end() noexcept { return _resource.end(); }
			const_iterator begin() const noexcept { return _resource.begin(); }
			const_iterator end() const noexcept { return _resource.end(); }
			const_iterator cbegin() const noexcept { return _resource.cbegin(); }
			const_iterator cend() const noexcept { return _resource.cend(); }

			// (主にデバッグ用)
			bool operator == (const ResMgr& m) const noexcept {
				if(_resource.size() == m._resource.size()) {
					auto itr0 = _resource.begin(),
						 itr1 = m._resource.begin();
					while(itr0 != _resource.end()) {
						if(*itr0 != *itr1)
							return false;
						++itr0;
						++itr1;
					}
					return true;
				}
				return false;
			}
			bool operator != (const ResMgr& m) const noexcept {
				return !(this->operator == (m));
			}
			auto _acquire(value_t* ptr) {
				shared_t sp(ptr, _deleter);
				// リソース管理のためのリスト登録
				_resource.emplace(sp);
				return sp;
			}
			template <class T2, class... Ts>
			auto emplaceWithType(Ts&&... ts) {
				return _acquire(new T2(std::forward<Ts>(ts)...));
			}
			template <class... Ts>
			auto emplace(Ts&&... ts) {
				return emplaceWithType<value_t>(std::forward<Ts>(ts)...);
			}
			auto acquire(value_t* ptr) {
				D_Assert0(_resource.count(tag_t(ptr)) == 0);
				return _acquire(ptr);
			}
			std::size_t size() const noexcept {
				return _resource.size();
			}
	};
}
