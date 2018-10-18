#pragma once
#include "../flyweight_item.hpp"
#include <cereal/access.hpp>
#include <cereal/types/memory.hpp>

namespace spi {
	template <class Ar, class T, class FH, class FC>
	void save(Ar& ar, const FlyweightItem<T,FH,FC>& f) {
		ar(f._sp);
	}
	template <class Ar, class T, class FH, class FC>
	void load(Ar& ar, FlyweightItem<T,FH,FC>& f) {
		using FW = FlyweightItem<T,FH,FC>;
		using V = typename FW::value_t;
		std::shared_ptr<V> sp;
		ar(sp);
		f = FW(std::move(*sp));
	}
}
namespace cereal {
	template <class Ar, class T, class FH, class FC>
	struct specialize<Ar, spi::FlyweightItem<T,FH,FC>, cereal::specialization::non_member_load_save> {};
}
