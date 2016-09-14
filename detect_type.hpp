#pragma once
#include <type_traits>

namespace spi {
	template <class T>
	class Optional;
	template <class T>
	struct is_optional : std::false_type {};
	template <class T>
	struct is_optional<Optional<T>> : std::true_type {};
}
