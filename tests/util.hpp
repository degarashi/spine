#pragma once

namespace spi {
	namespace test {
		//! 引数の値と異なる値を生成
		/*! 大きいfloat値で+1しても結果が変わらないケースに対応 */
		template <class T>
		T MakeDifferentValue(const T& t) {
			T tmp(1),
			  res(t);
			do {
				res += tmp;
				tmp *= 2;
			} while(res == t);
			return res;
		}
		template <class T>
		void ModifyValue(T& t) {
			t = MakeDifferentValue(t);
		}
	}
}
