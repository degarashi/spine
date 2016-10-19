#pragma once
#include <memory>

namespace spi {
	template <class T>
	struct ResTag {
		using value_t = T;
		using shared_t = std::shared_ptr<value_t>;
		using weak_t = std::weak_ptr<value_t>;
		const value_t*	ptr;
		weak_t			weak;

		ResTag(const shared_t& sp):
			ptr(sp.get()),
			weak(sp)
		{}
		ResTag(const value_t* p):
			ptr(p)
		{}
		bool operator == (const ResTag& t) const noexcept {
			// 生ポインタだけ比較(weak_ptrも中身は同じなので)
			return ptr == t.ptr;
		}
		bool operator != (const ResTag& t) const noexcept {
			return !(operator ==(t));
		}
	};
}
namespace std {
	template <class T>
	struct hash<::spi::ResTag<T>> {
		std::size_t operator()(const ::spi::ResTag<T>& t) const noexcept {
			return hash<const typename ::spi::ResTag<T>::value_t*>()(t.ptr);
		}
	};
}
