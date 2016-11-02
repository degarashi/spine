#pragma once
#include "../valueholder.hpp"
#include <cereal/types/base_class.hpp>

namespace spi {
	template <class Ar, class... Ts>
	void serialize(Ar&, ValueHolder<Ts...>&) {}

	template <class Ar, class T, class... Ts>
	void serialize(Ar& ar, ValueHolder<T,Ts...>& v) {
		using base_t = typename ValueHolder<T,Ts...>::base_t;
		ar(cereal::base_class<base_t>(&v), v.value);
	}
}
