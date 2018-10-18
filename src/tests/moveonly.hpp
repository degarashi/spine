#pragma once
#include "util.hpp"
#include <utility>
#include <algorithm>
#include <cereal/access.hpp>

namespace spi {
	namespace test {
		//! non-copyableな値
		template <class T>
		class MoveOnly {
			public:
				using value_t = T;
			private:
				value_t		_value;

				// MoveOnlyがネストされていた場合にget()で一括除去
				template <class T2>
				static const T2& _getValue(const T2& t, ...) {
					return t;
				}
				template <class T3>
				static decltype(auto) _getValue(const MoveOnly<T3>& t, ...) {
					return t.getValue();
				}

			public:
				MoveOnly() = default;
				MoveOnly(const value_t& v): _value(v) {}
				MoveOnly(value_t&& v) noexcept: _value(std::move(v)) {}
				MoveOnly(const MoveOnly&) = delete;
				MoveOnly(MoveOnly&&) = default;
				void operator = (const MoveOnly&) = delete;
				MoveOnly& operator = (MoveOnly&&) = default;
				decltype(auto) getValue() const {
					return _getValue(_value, nullptr);
				}
				bool operator == (const MoveOnly& m) const noexcept {
					return getValue() == m.getValue();
				}
				bool operator != (const MoveOnly& m) const noexcept {
					return !(this->operator == (m));
				}
				bool operator < (const MoveOnly& m) const noexcept {
					return getValue() < m.getValue();
				}
				bool operator > (const MoveOnly& m) const noexcept {
					return getValue() > m.getValue();
				}
				bool operator <= (const MoveOnly& m) const noexcept {
					return getValue() <= m.getValue();
				}
				bool operator >= (const MoveOnly& m) const noexcept {
					return getValue() >= m.getValue();
				}
				value_t& get() noexcept {
					return _value;
				}
				const value_t& get() const noexcept {
					return _value;
				}
				template <class Ar>
				void serialize(Ar& ar) {
					ar(_value);
				}
				template <class Ar>
				static void load_and_construct(Ar& ar, cereal::construct<MoveOnly>& cs) {
					value_t val;
					ar(val);
					cs(val);
				}
		};
		template <class T>
		std::ostream& operator << (std::ostream& os, const MoveOnly<T>& m) {
			return os << m.get();
		}
		template <class T>
		decltype(auto) Deref_MoveOnly(const MoveOnly<T>& m) {
			return m.get();
		}
		template <class T>
		decltype(auto) Deref_MoveOnly(const T& t) {
			return t;
		}

		template <class T>
		void ModifyValue(MoveOnly<T>& t) {
			ModifyValue(t.get());
		}
	}
}
namespace std {
	template <class T>
	struct hash<spi::test::MoveOnly<T>> {
		std::size_t operator()(const spi::test::MoveOnly<T>& m) const noexcept {
			return std::hash<T>()(m.get());
		}
	};
}
