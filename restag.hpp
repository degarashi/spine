#pragma once
#include <memory>
#include <algorithm>

namespace spi {
	template <class T>
	struct ResTag {
		using value_t = T;
		using shared_t = std::shared_ptr<value_t>;
		using weak_t = std::weak_ptr<value_t>;
		const value_t*	ptr;
		weak_t			weak;

		template <class P>
		ResTag(const std::shared_ptr<P>& sp):
			ptr(sp.get()),
			weak(sp)
		{}
		ResTag(const value_t* p):
			ptr(p)
		{}
		bool deepCmp(const ResTag& t) const noexcept {
			return *ptr == *t.ptr;
		}
		// ResTagをunordered_setに登録するだけなのでポインタ値を比較
		bool operator == (const ResTag& t) const noexcept {
			return ptr == t.ptr;
		}
		bool operator != (const ResTag& t) const noexcept {
			return !(operator ==(t));
		}
	};
	template <class S>
	bool SetDeepCompare(const S& a, const S& b) noexcept {
		if(a.size() != b.size())
			return false;
		const auto itrB0 = b.begin(),
					 itrB1 = b.end();
		for(auto& ap : a) {
			if(std::find_if(itrB0, itrB1, [&ap](const auto& b){
					return ap.deepCmp(b);
				}) == b.end())
			{
				return false;
			}
		}
		return true;
	}
	template <class M>
	bool MapDeepCompare(const M& a, const M& b) noexcept {
		if(a.size() != b.size())
			return false;
		const auto itrB0 = b.begin(),
					 itrB1 = b.end();
		for(auto& ap : a) {
			const auto itr = b.find(ap.first);
			if(itr == b.end())
				return false;
			if(ap.second != itr->second)
				return false;
		}
		return true;
	}
}
namespace std {
	template <class T>
	struct hash<::spi::ResTag<T>> {
		std::size_t operator()(const ::spi::ResTag<T>& t) const noexcept {
			return hash<const typename ::spi::ResTag<T>::value_t*>()(t.ptr);
		}
	};
}
