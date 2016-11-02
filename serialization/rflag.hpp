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
	void serialize(Ar& ar, RFlag<C,T...>& rf) {
		using base_t = typename RFlag<C,T...>::base_t;
		ar(cereal::base_class<base_t>(&rf), rf._rflag);
	}
}
namespace cereal {
	template <class Ar, class C, class G, class... T>
	struct specialize<Ar, ::spi::AcWrapper<C,G,T...>, cereal::specialization::non_member_serialize> {};
}
