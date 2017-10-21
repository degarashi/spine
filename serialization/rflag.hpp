#pragma once
#include "../rflag.hpp"
#include "valueholder.hpp"

namespace spi {
	template <class Ar, class C, class G, class... T>
	void serialize(Ar& ar, AcWrapper<C,G,T...>& ac) {
		using base_t = typename AcWrapper<C,G,T...>::CacheVal_t;
		ar(cereal::base_class<base_t>(&ac));
	}

	template <class Ar, class C, class... T>
	void save(Ar& ar, const RFlag<C,T...>& rf) {
		const_cast<RFlag<C,T...>&>(rf)._iterateLowestLayer([&ar](const auto& val, const auto& acc){
			ar(val, acc);
		});
	}
	template <class Ar, class C, class... T>
	void load(Ar& ar, RFlag<C,T...>& rf) {
		rf.resetAll();
		rf._iterateLowestLayer([&ar](auto& val, auto& acc){
			ar(val, acc);
		});
	}
}
namespace cereal {
	template <class Ar, class C, class G, class... T>
	struct specialize<Ar, ::spi::AcWrapper<C,G,T...>, cereal::specialization::non_member_serialize> {};
}
