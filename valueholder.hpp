#pragma once

namespace spi {
	template <class... Ts>
	struct ValueHolder {
		template <class T2>
		void ref(T2*) noexcept {}
		template <class T2>
		void cref(T2*) const noexcept {}
	};
	template <class T, class... Ts>
	struct ValueHolder<T, Ts...> : ValueHolder<Ts...> {
		using base_t = ValueHolder<Ts...>;
		using value_t = typename T::value_t;
		value_t		value;

		value_t& ref(T*) noexcept {
			return value;
		}
		const value_t& cref(T*) const noexcept {
			return value;
		}
		template <class T2>
		decltype(auto) ref(T2*) noexcept {
			return base_t::ref((T2*)nullptr);
		}
		template <class T2>
		decltype(auto) cref(T2*) const noexcept {
			return base_t::cref((T2*)nullptr);
		}
	};
}
