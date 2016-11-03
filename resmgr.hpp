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
	template <class T, class Allocator=std::allocator<T>>
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

			using Set = std::unordered_set<tag_t>;
			struct Resource {
				Set			set;
			};
			using Resource_SP = std::shared_ptr<Resource>;

			template <bool B>
			class _iterator : public Set::iterator {
				public:
					using base_t = typename Set::iterator;
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

			Resource_SP		_resource;

			template <class P>
			static void _Release(const Resource_SP& r, P *const p) noexcept {
				auto& set = r->set;
				typename Allocator::template rebind<P>::other alc;
				const auto itr = set.find(p);
				try {
					Assert0(itr != set.end());
					set.erase(itr);
					alc.destroy(p);
					alc.deallocate(p, 1);
				} catch(...) {}
			}
		public:
			ResMgr():
				_resource(std::make_shared<Resource>())
			{}
			iterator begin() noexcept { return _resource->set.begin(); }
			iterator end() noexcept { return _resource->set.end(); }
			const_iterator begin() const noexcept { return _resource->set.begin(); }
			const_iterator end() const noexcept { return _resource->set.end(); }
			const_iterator cbegin() const noexcept { return _resource->set.cbegin(); }
			const_iterator cend() const noexcept { return _resource->set.cend(); }

			// (主にデバッグ用)
			bool operator == (const ResMgr& m) const noexcept {
				auto	&s0 = _resource->set,
						&s1 = m._resource->set;
				if(s0.size() == s1.size()) {
					auto itr0 = s0.begin(),
						 itr1 = s1.begin();
					while(itr0 != s0.end()) {
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
			template <class T2=value_t, class... Ts>
			auto emplace(Ts&&... ts) {
				typename Allocator::template rebind<T2>::other alc;
				T2 *const ptr = alc.allocate(1);
				alc.construct(ptr, std::forward<Ts>(ts)...);
				std::shared_ptr<T2> sp(
					ptr,
					[r=_resource](T2 *const p){
						_Release(r, p);
					}
				);
				D_Assert0(_resource->set.count(tag_t(sp.get())) == 0);

				// リソース管理のためのリスト登録
				_resource->set.emplace(sp);
				return sp;
			}
			std::size_t size() const noexcept {
				return _resource->set.size();
			}
	};
}
