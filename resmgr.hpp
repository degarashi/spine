#pragma once
#include "restag.hpp"
#include "lubee/error.hpp"
#include <unordered_set>
#include <vector>

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

				bool deepCmp(const Resource& r) const noexcept {
					return SetDeepCompare(set, r.set);
				}
			};
			using Resource_SP = std::shared_ptr<Resource>;

			template <bool B>
			class _iterator : public Set::iterator {
				public:
					using base_t = typename Set::iterator;
					_iterator() = default;
					_iterator(const base_t& b):
						base_t(b)
					{}
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
			using Vec = std::vector<shared_t>;
			// シリアライズで復元した際の一時的なバックアップ
			// (内部にweak_ptrしか持っておらず削除されてしまう為)
			mutable Vec		_serializeBackup;

			template <class P>
			static void _Release(const Resource_SP& r, P *const p) noexcept {
				auto& set = r->set;
				typename Allocator::template rebind<P>::other alc;
				const auto itr = set.find(p);
				D_Assert0(itr != set.end());
				try {
					set.erase(itr);
					alc.destroy(p);
					alc.deallocate(p, 1);
				} catch(const std::exception& e) {
					AssertF("exception occurred (%s)", e.what());
				} catch(...) {
					AssertF("unknown exception occurred");
				}
			}
		public:
			ResMgr() {
				clear();
			}
			void clear() {
				cleanBackup();
				_resource = std::make_shared<Resource>();
			}
			void cleanBackup() const {
				_serializeBackup.clear();
			}
			const Vec& getBackup() const {
				return _serializeBackup;
			}
			iterator begin() noexcept { return _resource->set.begin(); }
			iterator end() noexcept { return _resource->set.end(); }
			const_iterator begin() const noexcept { return _resource->set.begin(); }
			const_iterator end() const noexcept { return _resource->set.end(); }
			const_iterator cbegin() const noexcept { return _resource->set.cbegin(); }
			const_iterator cend() const noexcept { return _resource->set.cend(); }

			const Resource_SP& getResourceSet() noexcept {
				return _resource;
			}
			bool deepCmp(const ResMgr& m) const noexcept {
				return _resource->deepCmp(*m._resource);
			}
			template <class T2=value_t, class... Ts>
			auto emplace(Ts&&... ts) -> std::shared_ptr<T2> {
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
