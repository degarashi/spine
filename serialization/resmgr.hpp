#pragma once
#include "../resmgr.hpp"
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>

namespace spi {
	template <class Ar, class T>
	void save(Ar& ar, const ResMgr<T>& mgr) {
		using Mgr = std::decay_t<decltype(mgr)>;
		using SPV = std::vector<typename Mgr::shared_t>;
		// 一旦shared_ptrに変換
		SPV spv;
		for(auto& r : mgr._resource)
			spv.emplace_back(r.weak.lock());
		ar(spv);
	}
	template <class Ar, class T>
	void load(Ar& ar, ResMgr<T>& mgr) {
		using Mgr = std::decay_t<decltype(mgr)>;
		using SPV = std::vector<typename Mgr::shared_t>;
		// shared_ptrとして読み取る
		SPV spv;
		ar(spv);
	
		mgr._resource.clear();
		for(auto& s : spv)
			mgr._resource.emplace(s);
	}
}
