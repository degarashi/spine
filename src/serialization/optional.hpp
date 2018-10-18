#pragma once
#include "../optional.hpp"
#include <cereal/access.hpp>

namespace spi {
	namespace opt_tmp {
		template <class Ar, class T>
		void serialize(Ar& ar, Buffer<T>& opt) {
			ar(opt.get());
		}
		// 参照のシリアライズには対応しない
		template <class Ar, class T>
		void serialize(Ar&, Buffer<T&>&) {
			static_assert(!sizeof(Ar), "can't serialize reference");
		}
		// ポインタのシリアライズには対応しない
		template <class Ar, class T>
		void serialize(Ar&, Buffer<T*>&) {
			static_assert(!sizeof(Ar), "can't serialize pointer");
		}
	}
	template <class Ar, class T>
	void load(Ar& ar, Optional<T>& opt) {
		if(!opt)
			opt = construct();
		ar(cereal::make_nvp("bInit", opt._bInit));
		if(opt._bInit) {
			opt_tmp::serialize(ar, opt._buffer);
		}
	}
	template <class Ar, class T>
	void save(Ar& ar, const Optional<T>& opt) {
		ar(cereal::make_nvp("bInit", opt._bInit));
		if(opt._bInit) {
			opt_tmp::serialize(ar, (opt_tmp::Buffer<T>&)opt._buffer);
		}
	}
}
