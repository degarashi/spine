#pragma once
#include "../lubee/random.hpp"
#include "../lubee/check_serialization.hpp"
#include <gtest/gtest.h>
#include <cereal/archives/json.hpp>
#include <cereal/archives/binary.hpp>

namespace spi {
	namespace test {
		class Random : public ::testing::Test {
			private:
				lubee::RandomMT	_mt;
			public:
				Random(): _mt(lubee::RandomMT::Make<4>()) {}
				auto& mt() {
					return _mt;
				}
		};
		#define USING(t) using t = typename TestFixture::t
	}
}
