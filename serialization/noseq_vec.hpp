#pragma once
#include "../noseq_vec.hpp"
#include <cereal/types/vector.hpp>

namespace spi {
	template <class Ar, class T, class AL>
	void serialize(Ar& ar, noseq_vec<T,AL>& ns) {
		using base_t = typename noseq_vec<T,AL>::base_t;
		ar(cereal::base_class<base_t>(&ns));
	}
}
namespace cereal {
	template <class Ar, class T, class AL>
	struct specialize<Ar, spi::noseq_vec<T,AL>, cereal::specialization::non_member_serialize> {};
}
