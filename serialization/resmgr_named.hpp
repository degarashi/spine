#pragma once
#include "../resmgr_named.hpp"
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>

namespace spi {
	template <class Ar, class T, class K>
	void save(Ar& ar, const ResMgrName<T,K>& mgr) {
		D_Assert0(mgr._serializeBackup.empty());
		using Mgr = std::decay_t<decltype(mgr)>;
		using NVPair = std::vector<std::pair<typename Mgr::key_t, typename Mgr::shared_t>>;
		// 一旦shared_ptrに変換
		NVPair nv;
		for(auto& r : mgr._resource->map)
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

		auto& map = mgr._resource->map;
		auto& v2k = mgr._resource->v2k;
		map.clear();
		v2k.clear();
		for(auto& n : nv) {
			v2k[n.second.get()] = n.first;
		}
		ar(mgr._acounter);

		const int n = nv.size();
		auto& bkup = mgr._serializeBackup;
		bkup.resize(n);
		for(int i=0 ; i<n ; i++) {
			bkup[i] = nv[i].second;
		}
	}
}
