// frea: 8563fd17a1934c0c6d13942f08442c8b8d19682f からのコピー
#pragma once
#include "meta/enable_if.hpp"
#include "range.hpp"
#include <algorithm>
#include <array>
#include <functional>

namespace frea {
	template <class T>
	using is_number = std::integral_constant<bool, std::is_integral<T>{} || std::is_floating_point<T>{}>;
	template <class MT>
	class Random {
		private:
			using MT_t = MT;
			MT_t	_mt;

			template <class T>
			using Lim = std::numeric_limits<T>;
			template <class T>
			struct Dist_Int {
				using uniform_t = std::uniform_int_distribution<T>;
				constexpr static Range<T> NumericRange{Lim<T>::lowest(), Lim<T>::max()};
			};
			template <class T>
			struct Dist_Float  {
				using uniform_t = std::uniform_real_distribution<T>;
				constexpr static Range<T> NumericRange{Lim<T>::lowest()/2, Lim<T>::max()/2};
			};
			template <class T, ENABLE_IF((std::is_integral<T>{}))>
			static Dist_Int<T> DetectDist();
			template <class T, ENABLE_IF((std::is_floating_point<T>{}))>
			static Dist_Float<T> DetectDist();
			template <class T>
			using Dist_t = decltype(DetectDist<T>());
		public:
			template <class T>
			class RObj {
				private:
					Random&			_rd;
					const Range<T>	_range;
				public:
					RObj(Random& rd, const Range<T>& r):
						_rd(rd),
						_range(r)
					{}
					RObj(const RObj&) = default;
					//! 実行時範囲指定
					auto operator ()(const Range<T>& r) const {
						return _rd.template getUniform<T>(r);
					}
					//! デフォルト範囲指定
					auto operator ()() const {
						return (*this)(_range);
					}
			};
		public:
			Random(MT_t mt) noexcept: _mt(std::move(mt)) {}
			Random(const Random&) = delete;
			Random(Random&& t) noexcept {
				*this = std::move(t);
			}
			Random& operator = (Random&& t) noexcept {
				swap(t);
				return *this;
			}
			void swap(Random& t) noexcept {
				std::swap(_mt, t._mt);
			}
			MT_t& refMt() noexcept {
				return _mt;
			}
			//! 一様分布
			/*! 範囲がnumeric_limits<T>::lowest() -> max()の乱数 */
			template <class T, ENABLE_IF(is_number<T>{})>
			T getUniform() {
				return getUniform<T>(Dist_t<T>::NumericRange);
			}
			//! 指定範囲の一様分布(in range)
			template <class T, ENABLE_IF(is_number<T>{})>
			T getUniform(const Range<T>& range) {
				return typename Dist_t<T>::uniform_t(range.from, range.to)(_mt);
			}
			//! 一様分布を返すファンクタを作成
			template <class T, ENABLE_IF(is_number<T>{})>
			auto getUniformF(const Range<T>& r) noexcept {
				return RObj<T>(*this, r);
			}
			//! 一様分布を返すファンクタを作成
			template <class T, ENABLE_IF(is_number<T>{})>
			auto getUniformF() noexcept {
				return RObj<T>(*this, Dist_t<T>::NumericRange);
			}
			//! 指定範囲の一様分布(vmax)
			template <class T, ENABLE_IF(is_number<T>{})>
			T getUniformMax(const T& vmax) {
				return getUniform<T>({Lim<T>::lowest(), vmax});
			}
			//! 指定範囲の一様分布(vmin)
			template <class T, ENABLE_IF(is_number<T>{})>
			T getUniformMin(const T& vmin) {
				return getUniform<T>({vmin, Lim<T>::max()});
			}

			template <std::size_t NumSeed>
			static Random<MT_t> Make() {
				std::random_device rnd;
				std::array<std::seed_seq::result_type, NumSeed> seed;
				std::generate(seed.begin(), seed.end(), std::ref(rnd));
				std::seed_seq seq(seed.begin(), seed.end());
				return std::move(Random<MT_t>(MT_t{seq}));
			}
	};
	template <class MT> template <class T>
	const Range<T> Random<MT>::Dist_Int<T>::NumericRange;
	template <class MT> template <class T>
	const Range<T> Random<MT>::Dist_Float<T>::NumericRange;

	using RandomMT = Random<std::mt19937>;
	using RandomMT64 = Random<std::mt19937_64>;
}
