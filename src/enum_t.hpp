#pragma once
#include <type_traits>

namespace spi {
	namespace inner {
		enum EnumTmp {};
	}
	using Enum_t = std::underlying_type_t<inner::EnumTmp>;
}
