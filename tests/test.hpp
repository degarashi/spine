#pragma once
#include "../random.hpp"
#include <gtest/gtest.h>
#include <cereal/archives/json.hpp>
#include <cereal/archives/binary.hpp>

namespace spi {
	namespace test {
		class Random : public ::testing::Test {
			private:
				frea::RandomMT	_mt;
			public:
				Random(): _mt(frea::RandomMT::Make<4>()) {}
				auto& mt() {
					return _mt;
				}
		};
		#define USING(t) using t = typename TestFixture::t

		// from frea:(tests/test.hpp) 84623829a7e9e814d0632e96bc560dc62c3a4cfd
		// ---- for serialization check ----
		template <class OA, class IA, class T>
		void _CheckSerialization(const T& src) {
			std::stringstream buffer;
			{
				OA oa(buffer);
				oa(CEREAL_NVP(src));
			}
			T loaded;
			{
				IA ia(buffer);
				ia(cereal::make_nvp("src", loaded));
			}
			ASSERT_EQ(src, loaded);
		}
		template <class T>
		void CheckSerializationBin(const T& src) {
			_CheckSerialization<cereal::BinaryOutputArchive,
								cereal::BinaryInputArchive>(src);
		}
		template <class T>
		void CheckSerializationJSON(const T& src) {
			_CheckSerialization<cereal::JSONOutputArchive,
								cereal::JSONInputArchive>(src);
		}
		template <class T>
		void CheckSerialization(const T& src) {
			CheckSerializationBin(src);
			CheckSerializationJSON(src);
		}
	}
}
