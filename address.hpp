#pragma once
#include <cstdint>

namespace spi {
	template <class ST, class T>
	intptr_t CalcSingletonAddressDiff() {
		return reinterpret_cast<intptr_t>(reinterpret_cast<T*>(1)) -
				reinterpret_cast<intptr_t>(static_cast<ST*>(reinterpret_cast<T*>(1)));
	}
}
