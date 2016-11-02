#pragma once
#include "../rflag.hpp"
#include "valueholder.hpp"

namespace spi {
	template <class Ar, class C, class... T>
	void serialize(Ar& ar, RFlag<C,T...>& rf) {
		using base_t = typename RFlag<C,T...>::base_t;
		ar(cereal::base_class<base_t>(&rf), rf._rflag);
	}
}
