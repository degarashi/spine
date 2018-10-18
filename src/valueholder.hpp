#pragma once

namespace spi {
	template <class... Ts>
	struct ValueHolder {
		template <class T2>
		void ref(T2*) noexcept {}
		template <class T2>
		void cref(T2*) const noexcept {}

		template <class Ar, class... Ts0>
		friend void serialize(Ar&, ValueHolder<Ts0...>&);
		bool operator == (const ValueHolder&) const noexcept { return true; }
		bool operator != (const ValueHolder&) const noexcept { return false; }
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
		auto ref(T2*) noexcept -> decltype(base_t::ref((T2*)nullptr)) {
			return base_t::ref((T2*)nullptr);
		}
		template <class T2>
		auto cref(T2*) const noexcept -> decltype(base_t::cref((T2*)nullptr)) {
			return base_t::cref((T2*)nullptr);
		}

		bool operator == (const ValueHolder& v) const noexcept {
			return value == v.value &&
				static_cast<const base_t&>(*this) == static_cast<const base_t&>(v);
		}
		bool operator != (const ValueHolder& v) const noexcept {
			return !(this->operator == (v));
		}
		template <class Ar, class T0, class... Ts0>
		friend void serialize(Ar&, ValueHolder<T0,Ts0...>&);
	};
}
