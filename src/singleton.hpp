#pragma once
#include "address.hpp"
#include "lubee/src/error.hpp"

namespace spi {
	//! 汎用シングルトン
	template <class T>
	class Singleton {
		private:
			using value_t = T;
			static value_t*		s_singleton;
		public:
			Singleton() {
				Assert(!s_singleton, "initializing error - already initialized");
				const auto offset = CalcSingletonAddressDiff<Singleton<value_t>, value_t>();
				s_singleton = reinterpret_cast<value_t*>(reinterpret_cast<intptr_t>(this) + offset);
			}
			virtual ~Singleton() {
				Assert(s_singleton, "destructor error");
				s_singleton = 0;
			}
			static value_t& ref() {
				Assert(s_singleton, "reference error");
				return *s_singleton;
			}
			static bool Initialized() {
				return s_singleton;
			}
	};
	template <class T>
	T* Singleton<T>::s_singleton = nullptr;
}
