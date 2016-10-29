#pragma once
#include "../resmgr_named.hpp"
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>

namespace spi {
	template <class Ar, class T, class K>
	void save(Ar& ar, const ResMgrName<T,K>& mgr) {
		using Mgr = std::decay_t<decltype(mgr)>;
		using NVPair = std::vector<std::pair<typename Mgr::key_t, typename Mgr::shared_t>>;
		// 一旦shared_ptrに変換
		NVPair nv;
		for(auto& r : mgr._resource)
			nv.emplace_back(r.first, r.second.weak.lock());
		ar(nv);
		ar(mgr._acounter);
	}
	template <class Ar, class T, class K>
	void load(Ar& ar, ResMgrName<T,K>& mgr) {
		using Mgr = std::decay_t<decltype(mgr)>;
		using NVPair = std::vector<std::pair<typename Mgr::key_t, typename Mgr::shared_t>>;
		// shared_ptrとして読み取る
		NVPair nv;
		ar(nv);

		mgr._resource.clear();
		mgr._v2k.clear();
		for(auto& n : nv) {
			auto ret = mgr._resource.emplace(n.first, n.second);
			mgr._v2k[n.second.get()] = n.first;
		}
		ar(mgr._acounter);
	}
}
