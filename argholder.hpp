#pragma once
#include "lubee/error.hpp"
#include "lubee/meta/enable_if.hpp"

namespace spi {
	template <class... Ts>
	struct ArgHolder {
		ArgHolder() = default;
		ArgHolder(ArgHolder&& /*a*/) {}
		template <class CB, class... TsA>
		decltype(auto) reverse(CB cb, TsA&&... tsa) {
			return cb(std::forward<TsA>(tsa)...);
		}
		template <class CB, class... TsA>
		decltype(auto) inorder(CB cb, TsA&&... tsa) {
			return cb(std::forward<TsA>(tsa)...);
		}
	};
	//! 引数を一旦とっておいて後で関数に渡す
	/*! not reference, not pointerな型はrvalue_referenceで渡されるので2回以上呼べない */
	template <class T0, class... Ts>
	struct ArgHolder<T0,Ts...> {
		T0		_value;
		using Lower = ArgHolder<Ts...>;
		Lower	_other;

		// 2度呼び出しチェック
		#ifdef DEBUG
			bool	_bCall = false;
			void _check() {
				if(_bCall)
					AssertF("Invalid ArgHolder call (called twice)");
				_bCall = true;
			}
		#else
			void _check() {}
		#endif

		template <class T2>
		ArgHolder(ArgHolder<T2,Ts...>&& a):
			_value(std::move(a._value)),
			_other(std::move(a._other))
		#ifdef DEBUG
			, _bCall(a._bCall)
		#endif
		{}

		template <class T0A, class... TsA>
		explicit ArgHolder(T0A&& t0, TsA&&... tsa):
			_value(std::forward<T0A>(t0)),
			_other(std::forward<TsA>(tsa)...)
		{}

		// T0がnot reference, not pointerな時はrvalue referenceで渡す
		template <
			class CB,
			class... TsA,
			class TT0 = T0,
			ENABLE_IF(std::is_object<TT0>{})
		>
		decltype(auto) reverse(CB cb, TsA&&... tsa) {
			_check();
			return _other.reverse(cb, std::move(_value), std::forward<TsA>(tsa)...);
		}
		// それ以外はそのまま渡す
		template <
			class CB,
			class... TsA,
			class TT0 = T0,
			ENABLE_IF(!std::is_object<TT0>{})
		>
		decltype(auto) reverse(CB cb, TsA&&... tsa) {
			return _other.reverse(cb, _value, std::forward<TsA>(tsa)...);
		}

		// T0がnot reference, not pointerな時はrvalue referenceで渡す
		template <
			class CB,
			class... TsA,
			class TT0 = T0,
			ENABLE_IF(std::is_object<TT0>{})
		>
		decltype(auto) inorder(CB cb, TsA&&... tsa) {
			_check();
			return _other.inorder(cb, std::forward<TsA>(tsa)..., std::move(_value));
		}
		// それ以外はそのまま渡す
		template <
			class CB,
			class... TsA,
			class TT0 = T0,
			ENABLE_IF(!std::is_object<TT0>{})
		>
		decltype(auto) inorder(CB cb, TsA&&... tsa) {
			return _other.inorder(cb, std::forward<TsA>(tsa)..., _value);
		}
	};
	//! 引数の順序を逆にしてコール
	template <class CB, class... Ts>
	decltype(auto) ReversedArg(CB cb, Ts&&... ts) {
		return ArgHolder<decltype(std::forward<Ts>(ts))...>(std::forward<Ts>(ts)...).reverse(cb);
	}
	template <class T>
	struct is_argholder : std::false_type {};
	template <class... Ts>
	struct is_argholder<ArgHolder<Ts...>> : std::true_type {};
}
