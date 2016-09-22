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

		template <class T>
		class TestObj {
			public:
				using value_t = T;
			private:
				value_t _value;
				static int s_counter;
			public:
				TestObj(TestObj&&) = default;
				TestObj(const TestObj&) = delete;
				TestObj() {
					++s_counter;
				}
				TestObj(const value_t& v):
					_value(v)
				{
					++s_counter;
				}
				~TestObj() {
					--s_counter;
				}
				TestObj& operator = (const T& t) {
					_value = t;
					return *this;
				}
				bool operator == (const TestObj& m) const {
					return _value == m._value;
				}
				static void InitializeCounter() {
					s_counter = 0;
				}
				static bool CheckCounter(const int c) {
					return c == s_counter;
				}
		};
		template <class T>
		int TestObj<T>::s_counter;
		template <class T>
		inline bool operator == (const T& t, const TestObj<T>& o) {
			return o == t;
		}
		// コンストラクタ・デストラクタが呼ばれる回数をチェックする機構の初期化
		template <class T, ENABLE_IF(!std::is_trivial<T>{})>
		void InitializeCounter() {
			T::InitializeCounter();
		}
		template <class T, ENABLE_IF(std::is_trivial<T>{})>
		void InitializeCounter() {}

		// コンストラクタ・デストラクタが呼ばれる回数をチェック
		template <class T, class V, ENABLE_IF(!std::is_trivial<T>{})>
		void CheckCounter(const V& v) {
			ASSERT_TRUE(T::CheckCounter(v));
		}
		template <class T, class V, ENABLE_IF(std::is_trivial<T>{})>
		void CheckCounter(const V&) {}
	}
}
