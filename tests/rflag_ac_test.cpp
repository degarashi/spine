#include "test.hpp"
#include "util.hpp"
#include "../rflag.hpp"
#include "../enum.hpp"

namespace spi {
	namespace test {
		template <class T>
		using GetRaw_t = std::decay_t<decltype(detail::GetAcWrapperValue(std::declval<T>()))>;

		#define countof(a)	(sizeof((a))/sizeof((a)[0]))
		template <class MT>
		class RFObj {
			private:
				struct Value0;
				struct Value2;
				struct Value01;
				struct Value02;
				struct Value01_02_3;
				struct Getter : RFlag_Getter<double> {
					using RFlag_Getter::operator ();
					counter_t operator()(const typename Value0::value_t& c, Value0*, const RFObj&) const {
						return static_cast<counter_t>(c);
					}
					counter_t operator()(const typename Value2::value_t& c, Value2*, const RFObj&) const {
						return static_cast<counter_t>(c);
					}
					counter_t operator()(const typename Value01::value_t& c, Value01*, const RFObj&) const {
						return static_cast<counter_t>(c);
					}
					counter_t operator()(const typename Value02::value_t& c, Value02*, const RFObj&) const {
						return static_cast<counter_t>(c);
					}
				};
				using BaseT = lubee::Types<
									AcCheck<lubee::Wrapper<float>, Getter>,
									uint32_t,
									AcCheck<lubee::Wrapper<uint64_t>, Getter>,
									int16_t
								>;
				using InterT = lubee::Types<
									AcCheck<lubee::Wrapper<float>, Getter>,
									AcCheck<lubee::Wrapper<double>, Getter>
							>;

				using Value01_t = typename InterT::template At<0>;
				using Value02_t = typename InterT::template At<1>;
				using Value01_02_3_t = AcCheck<lubee::Wrapper<int32_t>, Getter>;
				#define SEQ \
					((Value0)(typename BaseT::template At<0>)) \
					((Value1)(typename BaseT::template At<1>)) \
					((Value2)(typename BaseT::template At<2>)) \
					((Value3)(typename BaseT::template At<3>)) \
					((Value01)(Value01_t)(Value0)(Value1)) \
					((Value02)(Value02_t)(Value0)(Value2)) \
					((Value01_02_3)(Value01_02_3_t)(Value01)(Value02)(Value3))
				RFLAG_DEFINE(RFObj, SEQ)
				using Cache_t = typename RFlag_t::ct_base;

			public:
				using Set_t = lubee::Types<Value0, Value1, Value2, Value3>;
				using Ref_t = lubee::Types<Value0, Value2>;
				using Get_t = Cache_t;
				using Func_t = void (*)(RFObj&);
				static Func_t	s_setfunc[],
								s_getfunc[],
								s_reffunc[];

			private:
				using Values = typename Cache_t::template AsTArgs<ValueHolder>;
				Values			_value;
				MT&				_mt;

				template <class Tag>
				auto _calcValue(Tag*) const {
					return _value.cref((Tag*)nullptr);
				}
				auto _calcValue(Value01*) const {
					return _calcValue((Value0*)nullptr) +
							_calcValue((Value1*)nullptr);
				}
				auto _calcValue(Value02*) const {
					return _calcValue((Value0*)nullptr) -
							_calcValue((Value2*)nullptr);
				}
				auto _calcValue(Value01_02_3*) const {
					return _calcValue((Value01*)nullptr) *
							_calcValue((Value02*)nullptr) *
							_calcValue((Value3*)nullptr);
				}

				// Tのランダム値
				template <class T>
				auto _genValue() {
					return _mt.template getUniform<GetRaw_t<T>>();
				}
				template <class Tag>
				static void SetF(RFObj& obj) {
					// フラグを立てつつ、値を書き換え
					auto val = obj._rflag.template ref<Tag>();
					val = MakeDifferentValue(val);
					obj._rflag.template set<Tag>(val);
					obj._value.ref((Tag*)nullptr) = val;
				}
				template <class Tag>
				static void RefF(RFObj& obj) {
					// フラグを立てずに値を書き換え
					auto& dst = obj._rflag.template ref<Tag>();
					dst = MakeDifferentValue(dst);
					obj._value.ref((Tag*)nullptr) = dst;
				}
				template <class Tag>
				static void GetF(RFObj& obj) {
					// 自前で計算した値と比較
					const auto val0 = obj._calcValue((Tag*)nullptr);
					const auto val1 = obj._rflag.template get<Tag>(&obj);
					ASSERT_EQ(val0, val1);
				}

				template <int N>
				using IConst = lubee::IConst<N>;
				template <class F>
				static void _InitFunc(Func_t*, const F&, IConst<-1>) {}
				template <class F, int Cur>
				static void _InitFunc(Func_t* dst, const F& f, IConst<Cur>) {
					*(dst+Cur) = f(IConst<Cur>());
					_InitFunc(dst, f, IConst<Cur-1>());
				}
				static void _InitializeFunc() {
					if(!s_setfunc[0]) {
						static_assert(Set_t::size == countof(s_setfunc), "");
						_InitFunc(
							s_setfunc,
							[](auto f) -> Func_t {
								return &SetF<typename Set_t::template At<decltype(f)::value>>;
							},
							IConst<countof(s_setfunc)-1>()
						);
						static_assert(Get_t::size == countof(s_getfunc), "");
						_InitFunc(
							s_getfunc,
							[](auto f) -> Func_t {
								return &GetF<typename Get_t::template At<decltype(f)::value>>;
							},
							IConst<countof(s_getfunc)-1>()
						);
						static_assert(Ref_t::size == countof(s_reffunc), "");
						_InitFunc(
							s_reffunc,
							[](auto f) -> Func_t {
								return &RefF<typename Ref_t::template At<decltype(f)::value>>;
							},
							IConst<countof(s_reffunc)-1>()
						);
					}
				}
				// キャッシュ値を全てゼロで初期化
				void _init(IConst<Cache_t::size>) {}
				template <int N>
				void _init(IConst<N>) {
					using Tag = typename Cache_t::template At<N>;
					_rflag.template ref<Tag>() = 0;
					_value.ref((Tag*)nullptr) = 0;
					_init(IConst<N+1>());
				}

			public:
				RFLAG_GETMETHOD_DEFINE(SEQ)
				RFObj(MT& mt):
					_mt(mt)
				{
					_InitializeFunc();
					_init(IConst<0>());
				}
		};
		template <class MT>
		typename RFObj<MT>::Func_t RFObj<MT>::s_setfunc[Set_t::size] = {};
		template <class MT>
		typename RFObj<MT>::Func_t RFObj<MT>::s_reffunc[Ref_t::size] = {};
		template <class MT>
		typename RFObj<MT>::Func_t RFObj<MT>::s_getfunc[Get_t::size] = {};

		template <class MT>
		bool RFObj<MT>::_refresh(typename RFObj::Value01::value_t& dst, RFObj::Value01*) const {
			if(auto ret = _rflag.getWithCheck(this, dst)) {
				const auto& v0 = *std::get<0>(ret);
				const auto& v1 = *std::get<1>(ret);
				dst = v0 + v1;
				return true;
			}
			return false;
		}
		template <class MT>
		bool RFObj<MT>::_refresh(typename RFObj::Value02::value_t& dst, RFObj::Value02*) const {
			if(auto ret = _rflag.getWithCheck(this, dst)) {
				const auto& v0 = *std::get<0>(ret);
				const auto& v2 = *std::get<1>(ret);
				dst = v0 - v2;
				return true;
			}
			return false;
		}
		template <class MT>
		bool RFObj<MT>::_refresh(typename RFObj::Value01_02_3::value_t& dst, RFObj::Value01_02_3*) const {
			if(auto ret = _rflag.getWithCheck(this, dst)) {
				dst = *std::get<0>(ret) * *std::get<1>(ret) * *std::get<2>(ret);
				return true;
			}
			return false;
		}

		struct RFlagAc : Random {
			using MT = std::decay_t<decltype(std::declval<Random>().mt())>;
			using RF = RFObj<MT>;
		};
		DefineEnum(
			Act,
			(Set)
			(Get)
			(Ref)
		);
		TEST_F(RFlagAc, General) {
			using RF = RFlagAc::RF;
			// 基本的な動作は RFlag/*.General で確認済みなのでgetWithCheck()のテストのみ行う
			auto& mt = this->mt();
			RF rf(mt);

			const auto mtf = mt.getUniformF<int>();
			// ランダムに値をセットする
			int range;
			RF::Func_t* func;
			const int N = mtf({1, 128});
			for(int i=0 ; i<N ; i++) {
				const int idAct = mtf({0, Act::_Num-1});
				switch(idAct) {
					case Act::Set:
						func = RF::s_setfunc;
						range = RF::Set_t::size;
						break;
					case Act::Get:
						func = RF::s_getfunc;
						range = RF::Get_t::size;
						break;
					case Act::Ref:
						func = RF::s_reffunc;
						range = RF::Ref_t::size;
						break;
				}
				const int idVal = mtf({0, range-1});
				ASSERT_NO_FATAL_FAILURE(func[idVal](rf));
			}
		}
	}
}
