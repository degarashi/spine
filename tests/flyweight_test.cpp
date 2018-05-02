#include "test.hpp"
#include "moveonly.hpp"
#include "../flyweight.hpp"

namespace spi {
	namespace test {
		template <class T>
		struct Flyweight : Random {
			using sp_t = std::shared_ptr<const T>;
			using wp_t = std::weak_ptr<const T>;
			using set_t = ::spi::Flyweight<T>;
			set_t	_set;

			template <class T2=T, ENABLE_IF(lubee::is_number<T2>{})>
			auto makeRV() {
				return this->mt().template getUniform<T2>();
			}
			template <class T2=T, ENABLE_IF(!lubee::is_number<T2>{})>
			auto makeRV() {
				return this->mt().template getUniform<uint32_t>({0, 10});
			}
		};
		using FwT = ::testing::Types<uint32_t, MoveOnly<uint32_t>>;
		TYPED_TEST_CASE(Flyweight, FwT);

		TYPED_TEST(Flyweight, Make) {
			const auto value = this->template makeRV<TypeParam>();
			const auto fw_value = this->_set.make(TypeParam(value));
			ASSERT_EQ(value, Deref_MoveOnly(*fw_value));

			if constexpr (std::is_copy_constructible_v<TypeParam>) {
				auto cvalue = *fw_value;
				ASSERT_EQ(value, Deref_MoveOnly(cvalue));
				cvalue = MakeDifferentValue(cvalue);
				ASSERT_NE(value, Deref_MoveOnly(cvalue));
			}
			{
				// 同じ値でmakeしたら同じポインタが返る
				const auto fw_value2 = this->_set.make(TypeParam(value));
				ASSERT_EQ(fw_value, fw_value2);
				ASSERT_EQ(*fw_value, *fw_value2);
			}
			{
				// 違う値でmakeすれば違うポインタが返る
				const auto value2 = MakeDifferentValue(value);
				const auto fw_value3 = this->_set.make(TypeParam(value2));
				ASSERT_NE(fw_value, fw_value3);
				ASSERT_NE(*fw_value, *fw_value3);
			}
		}
		TYPED_TEST(Flyweight, Variety) {
			USING(sp_t);
			USING(wp_t);
			using set_0 = std::unordered_map<TypeParam, wp_t>;
			using set_1 = std::unordered_set<sp_t>;

			set_0 set0;
			set_1 set1;

			auto& mt = this->mt();
			std::size_t N = 1024;
			while(N-- != 0) {
				const auto value = this->template makeRV<TypeParam>();
				const auto [itr1, added1] = set1.emplace(this->_set.make(TypeParam(value)));
				const auto [itr0, added0] = set0.emplace(TypeParam(value), *itr1);
				ASSERT_EQ(added0, added1);

				if(!set0.empty() && mt.template getUniform<int>({0, 10}) == 0) {
					const auto itr0 = set0.begin();
					const auto itr1 = set1.find(itr0->second.lock());
					ASSERT_NE(itr1, set1.end());
					ASSERT_EQ(Deref_MoveOnly(itr0->first), Deref_MoveOnly(**itr1));

					set0.erase(itr0);
					set1.erase(itr1);
					ASSERT_EQ(set0.size(), set1.size());
				}
				if(mt.template getUniform<int>({0, 100}) == 0) {
					this->_set.gc();
				}
				ASSERT_EQ(set0.size(), set1.size());
			}
		}
	}
}
