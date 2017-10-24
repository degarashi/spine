#pragma once
#include "../resmgr.hpp"
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>

namespace spi {
	template <class Ar, class T>
	void save(Ar& ar, const ResMgr<T>& mgr) {
		D_Assert0(mgr._serializeBackup.empty());
		using Mgr = std::decay_t<decltype(mgr)>;
		using SPV = std::vector<typename Mgr::shared_t>;
		// 一旦shared_ptrに変換
		SPV spv;
		for(auto& r : mgr._resource->set)
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

		auto& set = mgr._resource->set;
		set.clear();
		for(auto& s : spv)
			set.emplace(s);
		mgr._serializeBackup = std::move(spv);
	}
}
