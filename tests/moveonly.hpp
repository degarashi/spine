#pragma once
#include <utility>
#include "util.hpp"

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
		};

		template <class T>
		void ModifyValue(MoveOnly<T>& t) {
			ModifyValue(t.get());
		}
	}
}
