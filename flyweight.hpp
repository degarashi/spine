#pragma once
#include "lubee/error.hpp"
#include <unordered_set>
#include <memory>

namespace spi {
	template <class T, class F_Hash=std::hash<T>, class F_Cmp=std::equal_to<>>
	class Flyweight {
		private:
			using value_t = T;
		public:
			using SP = std::shared_ptr<const value_t>;
		private:
			struct WP {
				using P = std::weak_ptr<const value_t>;
				P				wp;
				const value_t*	ptr = nullptr;
				std::size_t		hash_value;

				WP(const SP& sp):
					wp(sp),
					hash_value(F_Hash()(*sp))
				{}
				WP(const value_t* ptr):
					ptr(ptr),
					hash_value(F_Hash()(*ptr))
				{}

				bool operator == (const WP& w) const noexcept {
					const SP sp0 = wp.lock(),
							sp1 = w.wp.lock();
					const value_t *p0, *p1;
					p0 = ptr ? ptr : sp0.get();
					p1 = w.ptr ? w.ptr : sp1.get();
					if(!p1) {
						return F_Cmp()(p0, nullptr);
					}
					return F_Cmp()(*p0, *p1);
				}
			};
			struct Hash {
				std::size_t operator()(const WP& w) const noexcept {
					return w.hash_value;
				}
			};
			using Set = std::unordered_set<WP, Hash>;
			Set			_set;

		public:
			void gc() {
				auto itr = _set.begin();
				while(itr != _set.end()) {
					if(itr->wp.expired()) {
						itr = _set.erase(itr);
					} else
						++itr;
				}
			}
			template <
				class V,
				ENABLE_IF((std::is_same_v<std::decay_t<V>, value_t>))
			>
			SP make(V&& v) {
				{
					const auto itr = _set.find(WP(&v));
					if(itr != _set.end()) {
						if(const auto sp = itr->wp.lock())
							return sp;
						_set.erase(itr);
					}
				}
				SP ret = std::make_shared<const value_t>(std::forward<V>(v));
				_set.emplace(WP(ret));
				return ret;
			}
			template <
				class V,
				ENABLE_IF(!(std::is_same_v<std::decay_t<V>, value_t>))
			>
			SP make(V&& v) {
				return make(value_t(std::forward<V>(v)));
			}
	};
}
