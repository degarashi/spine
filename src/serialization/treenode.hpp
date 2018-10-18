#pragma once
#include "../treenode.hpp"
#include <cereal/types/memory.hpp>

namespace spi {
	template <class Ar, class T>
	void serialize(Ar& ar, TreeNode<T>& node) {
		ar(
			cereal::make_nvp("spChild", node._spChild),
			cereal::make_nvp("spSibling", node._spSibling),
			cereal::make_nvp("wpParent", node._wpParent),
			cereal::make_nvp("wpPrevSibling", node._wpPrevSibling)
		);
	}
}
