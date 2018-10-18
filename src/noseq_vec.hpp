#pragma once
#include <vector>
#include "lubee/src/error.hpp"

namespace spi {
	//! 順序なし配列
	/*! 基本的にはstd::vectorそのまま
		要素を削除した時に順序を保証しない点だけが異なる */
	template <class T, class AL=std::allocator<T>>
	class noseq_vec : public std::vector<T, AL> {
		private:
			template <class Ar, class T2, class AL2>
			friend void serialize(Ar&, noseq_vec<T,AL>&);
		public:
			using base_t = std::vector<T,AL>;
			using base_t::base_t;
			using base_t::erase;
			void erase(typename base_t::iterator itr) {
				if(base_t::size() > 1 && itr != --base_t::end()) {
					D_Assert0(itr < base_t::end());
					// 削除対象が最後尾でなければ最後尾の要素と取り替える
					std::swap(*itr, base_t::back());
				}
				D_Assert0(itr != base_t::end());
				base_t::pop_back();
			}
	};
}
