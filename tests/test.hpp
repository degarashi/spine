#pragma once
#include "../random.hpp"
#include <gtest/gtest.h>

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
	}
}
