#pragma once
#include "../noseq_list.hpp"
#include "optional.hpp"
#include <cereal/types/vector.hpp>

namespace spi {
	template <class Ar, class Value>
	void serialize(Ar& ar, UData<Value>& ns) {
		ar(
			cereal::make_nvp("value", ns.value),
			cereal::make_nvp("uid", ns.uid)
		);
	}
	template <class Ar, class Id>
	void serialize(Ar& ar, IDS<Id>& id) {
		ar(
			cereal::make_nvp("type", id.type),
			cereal::make_nvp("value", id.value)
		);
	}
	template <class Ar, class Value, class Id>
	void serialize(Ar& ar, Entry<Value,Id>& ent) {
		ar(
			cereal::make_nvp("udata", ent.udata),
			cereal::make_nvp("ids", ent.ids)
		);
	}
	template <class Ar, class T, class Alc, class Id>
	void serialize(Ar& ar, noseq_list<T,Alc,Id>& nl) {
		D_Assert0(!nl._bRemoving && nl._remList.empty());
		ar(
			cereal::make_nvp("array", nl._array),
			cereal::make_nvp("n_free", nl._nFree),
			cereal::make_nvp("first_free", nl._firstFree)
		);
	}
}
