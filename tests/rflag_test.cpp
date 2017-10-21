#include "test.hpp"
#include "../rflag.hpp"
#include "lubee/bit.hpp"
#include "lubee/meta/index_sequence.hpp"
#include "../enum.hpp"
#include "../serialization/rflag.hpp"

namespace spi {
	namespace test {
		template <class T>
		using GetRaw_t = std::decay_t<decltype(AcWrapperValue(std::declval<T>()))>;
		// 1つのキャッシュ変数の更新が別の変数を同時に計算できる場合のテスト用
		template <class MT, class R>
		struct RFRefr {
			using Types0 = lubee::Types<typename R::Value2, typename R::Value3, typename R::Value02>;
			using Types1 = lubee::Types<typename R::Value3>;
			using TypesC = typename Types0::template Append<Types1>;

			MT&				_mt;
			// [Types0(Value01が他に及ぼす変数)][Types1(Value02が他に及ぼす変数)]
			// ビットが立っている場合に更新
			const uint32_t	_mask;
			RFRefr(MT& mt):
				_mt(mt),
				_mask(mt.template getUniform<uint32_t>({0, (1<<TypesC::size)-1}))
			{}

			uint32_t getTestPattern() const {
				return _mask;
			}
			template <int N>
			using IConst = lubee::IConst<N>;

			// フラグに従って該当するキャッシュ変数にランダム値をセットする
			template <class Types, class Obj>
			void _rewrite(Obj&, IConst<Types::size>, uint32_t) const {}
			template <class Types, class Obj, int N, ENABLE_IF((N != Types::size))>
			void _rewrite(Obj& obj, IConst<N>, uint32_t mask) const {
				using Tag = typename Types::template At<N>;
				if(mask & 1) {
					const auto val = _mt.template getUniform<GetRaw_t<typename Tag::value_t>>();
					obj._rflag.template set<Tag>(val);
					obj.template _updateSetflag<Tag>();
				}
				_rewrite<Types>(obj, IConst<N+1>(), mask>>1);
			}
			template <class Dst, class T>
			void value01(Dst& dst, T& obj) const {
				using lubee::wrapper_value;
				dst = AcWrapperValue(obj.getValue0()) + AcWrapperValue(obj.getValue1());
				// Value02に関わるキャッシュ変数をmaskに従って書き換え
				_rewrite<Types0>(obj, IConst<0>(), _mask&((1<<Types0::size)-1));
			}
			template <class Dst, class T>
			void value02(Dst& dst, T& obj) const {
				dst = AcWrapperValue(obj.getValue0()) - AcWrapperValue(obj.getValue2());
				// Value01に関わるキャッシュ変数をmaskに従って書き換え
				_rewrite<Types1>(obj, IConst<0>(), _mask>>Types0::size);
			}
		};

		template <std::size_t N>
		using SZConst = lubee::SZConst<N>;
		template <bool A>
		using BConst = lubee::BConst<A>;

		template <std::size_t Bit, std::size_t N, class A>
		struct MakeBitArray;
		template <std::size_t N>
		struct MakeBitArray<0, N, std::false_type> {
			using type = std::index_sequence<>;
		};
		template <std::size_t Bit, std::size_t N>
		struct MakeBitArray<Bit, N, std::true_type> {
			constexpr static auto B0 = Bit>>1,
								N0 = N+1;
			using type0 = typename MakeBitArray<B0, N0, BConst<static_cast<bool>(B0&1)>>::type;
			using type = lubee::seq::Concat_t<std::index_sequence<N>, type0>;
		};
		template <std::size_t Bit, std::size_t N>
		struct MakeBitArray<Bit, N, std::false_type> {
			constexpr static auto B0 = Bit>>1,
								N0 = N+1;
			using type = typename MakeBitArray<B0, N0, BConst<static_cast<bool>(B0&1)>>::type;
		};
		template <std::size_t Bit>
		using MakeBitArray_t = typename MakeBitArray<Bit, 0, BConst<Bit&1>>::type;

		template <class MT, class BaseT, class InterT>
		class RFObj {
			template <class, class>
			friend struct RFRefr;
			public:
				template <int N>
				using IConst = lubee::IConst<N>;

				using Value01_t = typename InterT::template At<0>;
				using Value02_t = typename InterT::template At<1>;
				using Value01_02_3_t = int32_t;
				#define SEQ \
					((Value0)(typename BaseT::template At<0>)) \
					((Value1)(typename BaseT::template At<1>)) \
					((Value2)(typename BaseT::template At<2>)) \
					((Value3)(typename BaseT::template At<3>)) \
					((Value01)(Value01_t)(Value0)(Value1)) \
					((Value02)(Value02_t)(Value0)(Value2)) \
					((Value01_02_3)(Value01_02_3_t)(Value01)(Value02)(Value3))
				RFLAG_DEFINE(RFObj, SEQ)

			public:
				using Cache_t = typename RFlag_t::ct_base;
				DefineEnum(
					Action,
					(Set)
					(Get)
					(Ref)
				);
				constexpr static int ValW = lubee::bit::LowClear(static_cast<unsigned int>(Cache_t::size)) << 1,
									ValB = lubee::bit::MSB(ValW),
									ActW = lubee::bit::LowClear(static_cast<unsigned int>(Action::_Num)) << 1,
									ActB = lubee::bit::MSB(ActW);
				using RF = std::decay_t<decltype(_rflag)>;
				constexpr static RFlagValue_t LowFlag = RF::template FlagMask<Value0, Value1, Value2, Value3>();

			private:
				using Values = typename Cache_t::template AsTArgs<ValueHolder>;
				Values			_value;
				RFlagValue_t	_setflag;
				mutable int		_counter;
				MT&				_mt;

				template <class Tag>
				auto _calcValue(Tag*) const {
					return AcWrapperValue(_value.cref((Tag*)nullptr));
				}
				template <class T>
				using AcV = std::decay_t<decltype(AcWrapperValue(_value.cref((T*)nullptr)))>;
				AcV<Value01> _calcValue(Value01*) const {
					if(_setflag & RF::template FlagMask<Value01>() & ~RF::ACFlag)
						return _value.cref((Value01*)nullptr);
					return _calcValue((Value0*)nullptr)
							+ _calcValue((Value1*)nullptr);
				}
				AcV<Value02> _calcValue(Value02*) const {
					if(_setflag & RF::template FlagMask<Value02>() & ~RF::ACFlag)
						return _value.cref((Value02*)nullptr);
					return _calcValue((Value0*)nullptr)
							- _calcValue((Value2*)nullptr);
				}
				AcV<Value01_02_3> _calcValue(Value01_02_3*) const {
					if(_setflag & RF::template FlagMask<Value01_02_3>() & ~RF::ACFlag)
						return _value.cref((Value01_02_3*)nullptr);
					return _calcValue((Value01*)nullptr)
							* _calcValue((Value02*)nullptr)
							* _calcValue((Value3*)nullptr);
				}

			public:
				// 動作チェック用関数配列: Func[Action | Tag]
				using ChkFunc = std::function<void (RFObj&)>;
				static ChkFunc s_chkfunc[1 << (ActB + ValB)];
				static ChkFunc s_chkgetfunc[1 << Cache_t::size];

			private:
				template <class Tag>
				void _updateSetflag() {
					_setflag &= ~RF::template OrLH<Tag>();
					_setflag |= RF::template FlagMask<Tag>();
					ASSERT_EQ(0, _setflag & _rflag.getFlag() & ~LowFlag & ~RF::ACFlag);
				}
				// Set<Tag>の動作をチェック
				template <class Tag>
				static ChkFunc _CheckF(IConst<Action::Set>) {
					return
						[](RFObj& obj){
							// 適当に値を生成
							const auto val = obj.template _genValue<typename Tag::value_t>();
							// セット後に値を比較
							obj._rflag.template set<Tag>(val);
							ASSERT_EQ(val, obj._rflag.template ref<Tag>());
							if(!(RF::ACFlag & RF::template FlagMask<Tag>())) {
								// セットした値を取得しても更新はかからない
								obj._counter = 0;
								ASSERT_EQ(val, obj._rflag.template get<Tag>(&obj));
								ASSERT_EQ(0, obj._counter);
							}

							ASSERT_NO_FATAL_FAILURE(obj._updateSetflag<Tag>());
							obj._sync();
						};
				}
				// FlagMask<Tag>の動作をチェック
				template <class Tag>
				static ChkFunc _CheckF(IConst<Action::Get>) {
					return
						[](RFObj& obj){
							if(obj._rflag.template test<Tag>()) {
								if(obj._refr.getTestPattern() == 0) {
									// 下位の値から算出
									const auto val = obj._calcValue((Tag*)nullptr);
									// 最下位の値を除いた更新フラグの数だけカウンタがインクリメントされる(筈)
									const RFlagValue_t flag = RF::template OrHL<Tag>() & (~LowFlag) & obj._rflag.getFlag();
									const int nbit = lubee::bit::Count(flag);
									obj._counter = 0;
									ASSERT_EQ(val, obj._rflag.template get<Tag>(&obj));
									ASSERT_EQ(nbit, obj._counter);
									obj._sync();
								} else {
									const auto val0 = obj._rflag.template get<Tag>(&obj);
									obj._sync();
									// 下位の値から算出
									const auto val = obj._calcValue((Tag*)nullptr);
									ASSERT_EQ(val, val0);
								}
							} else
								ASSERT_EQ(obj._value.cref((Tag*)nullptr), obj._rflag.template ref<Tag>());
						};
				}
				// Ref<Tag>の動作をチェック
				template <class Tag>
				static ChkFunc _CheckF(IConst<Action::Ref>) {
					return
						[](RFObj& obj){
							// 適当に値を生成
							const auto val = obj.template _genValue<typename Tag::value_t>();
							constexpr auto Flag = RF::template FlagMask<Tag>();
							if(Flag & RF::ACFlag) {
								if(obj._refr.getTestPattern() == 0) {
									// 常に更新される変数に対するテスト
									const auto prev = obj._rflag.template get<Tag>(&obj);
									obj._rflag.template ref<Tag>() = val;
									const auto cur = obj._rflag.template get<Tag>(&obj);
									if(Flag & LowFlag) {
										// 最下層の変数はrefで指定した値に書き換わる
										ASSERT_EQ(val, cur);
									} else {
										// 中間層の変数はrefで何を指定しても無意味
										ASSERT_EQ(prev, cur);
									}
								}
							} else {
								obj._rflag.template refF<Tag>() = val;
								obj._counter = 0;
								// セットした値がそのまま取り出せるか
								ASSERT_EQ(val, obj._rflag.template get<Tag>(&obj));
								// セットした箇所なので更新はかからない
								ASSERT_EQ(0, obj._counter);
							}
							ASSERT_NO_FATAL_FAILURE(obj._updateSetflag<Tag>());
							obj._sync();
						};
				}

				static void _InitFunc(ChkFunc*, IConst<Action::_Num>, IConst<0>) {}
				template <int ActId>
				static void _InitFunc(ChkFunc* func, IConst<ActId>, IConst<Cache_t::size>) {
					_InitFunc(func+(ValW-Cache_t::size), IConst<ActId+1>(), IConst<0>());
				}
				template <int ActId, int ValueId>
				static void _InitFunc(ChkFunc* dst, IConst<ActId>, IConst<ValueId>) {
					*dst = _CheckF<typename Cache_t::template At<ValueId>>(IConst<ActId>());
					_InitFunc(dst+1, IConst<ActId>(), IConst<ValueId+1>());
				}
				// 未初期化の場合のみ、チェック関数を[Action | Tag]すべての組み合わせで生成 -> s_chkfunc
				static void _InitFunc() {
					if(!s_chkfunc[0])
						_InitFunc(s_chkfunc, IConst<0>(), IConst<0>());
				}
				template <std::size_t... Idx>
				static auto MakeTypeArray(std::index_sequence<Idx...>) -> lubee::Types<typename Cache_t::template At<Idx>...>;
				template <class CT>
				using MakeTypeArray_t = decltype(MakeTypeArray(std::declval<CT>()));

				template <std::size_t N>
				static void _CheckGetF(RFObj& obj) {
					obj.getTest<MakeTypeArray_t<MakeBitArray_t<N>>>();
				}
				static void _InitGetFunc(ChkFunc*, SZConst<Cache_t::size>) {}
				template <std::size_t N>
				static void _InitGetFunc(ChkFunc* dst, SZConst<N>) {
					*dst = &_CheckGetF<N>;
					_InitGetFunc(dst+1, SZConst<N+1>());
				}
				static void _InitGetFunc() {
					if(!s_chkgetfunc[0])
						_InitGetFunc(s_chkgetfunc, SZConst<0>());
				}

				template <class C, std::size_t N>
				RFlagValue_t getTestSingle(const C&, SZConst<N>) const { return 0; }
				template <class C, std::size_t N, class T0, class... Ts>
				RFlagValue_t getTestSingle(const C& c, SZConst<N>, T0*, Ts*...) const {
					auto ret = _rflag.template refresh<T0>(this);
					[&]{
						ASSERT_EQ(*std::get<N>(c), ret.first);
					}();
					return ret.second | getTestSingle(c, SZConst<N+1>(), ((Ts*)nullptr)...);
				}
				template <class... Ts>
				void getTestDual(lubee::Types<Ts...>) const {
					auto self = *this;
					auto ret = self._rflag.template getAsTuple<Ts...>(&self);
					auto flag = getTestSingle(ret, SZConst<0>(), ((Ts*)nullptr)...);
					ASSERT_FALSE(::testing::Test::HasFatalFailure());
					ASSERT_EQ(ret.flag, flag);
				}
				template <class CT>
				void getTest() const {
					getTestDual(CT());
				}

				// Tのランダム値
				template <class T>
				auto _genValue() {
					return _mt.template getUniform<GetRaw_t<T>>();
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

				// RFlagの内部値をthis->_valueへコピー
				void _sync(IConst<Cache_t::size>) {}
				template <int N>
				void _sync(IConst<N>) {
					using Tag = typename Cache_t::template At<N>;
					_value.ref((Tag*)nullptr) = _rflag.template ref<Tag>();
					_sync(IConst<N+1>());
				}
				void _sync() {
					_sync(IConst<0>());
				}
				RFRefr<MT, RFObj>	_refr;

			public:
				RFObj(MT& mt):
					_mt(mt),
					_refr(mt)
				{
					_InitFunc();
					_InitGetFunc();
					resetAll();
				}

				RFLAG_TEST_DEFINE()
				RFLAG_GETMETHOD_DEFINE(SEQ)

				RFLAG_SETMETHOD(Value01)
				RFLAG_SETMETHOD(Value02)
				RFLAG_SETMETHOD(Value01_02_3)
				RFLAG_SETMETHOD_DEFINE(SEQ)

				RFLAG_REFMETHOD(Value01)
				RFLAG_REFMETHOD(Value02)
				RFLAG_REFMETHOD(Value01_02_3)
				RFLAG_REFMETHOD_DEFINE(SEQ)
				#undef SEQ

				void resetAll() noexcept {
					_setflag = 0;
					_rflag.resetAll();
					_init(IConst<0>());
				}
		};
		template <class MT, class BaseT, class InterT>
		bool RFObj<MT, BaseT, InterT>::_refresh(typename RFObj::Value01::value_t& dst, RFObj::Value01*) const {
			++_counter;
			_refr.value01(dst, const_cast<RFObj&>(*this));
			return true;
		}
		template <class MT, class BaseT, class InterT>
		bool RFObj<MT, BaseT, InterT>::_refresh(typename RFObj::Value02::value_t& dst, RFObj::Value02*) const {
			++_counter;
			_refr.value02(dst, const_cast<RFObj&>(*this));
			return true;
		}
		template <class MT, class BaseT, class InterT>
		bool RFObj<MT, BaseT, InterT>::_refresh(typename RFObj::Value01_02_3::value_t& dst, RFObj::Value01_02_3*) const {
			++_counter;
			dst = getValue01() * getValue02() * getValue3();
			return true;
		}
		template <class MT, class BaseT, class InterT>
		typename RFObj<MT, BaseT, InterT>::ChkFunc RFObj<MT, BaseT, InterT>::s_chkfunc[1 << (ActB + ValB)] = {};
		template <class MT, class BaseT, class InterT>
		typename RFObj<MT, BaseT, InterT>::ChkFunc RFObj<MT, BaseT, InterT>::s_chkgetfunc[1 << Cache_t::size] = {};

		template <class T>
		struct RFlag : Random {
			using BaseT = std::tuple_element_t<0,T>;
			using InterT = std::tuple_element_t<1,T>;
			using MT = std::decay_t<decltype(std::declval<Random>().mt())>;
			using RF = RFObj<MT, BaseT, InterT>;
		};

		struct Getter : RFlag_Getter<uint32_t> {
			using RFlag_Getter::operator ();
			template <class C, class Self>
			counter_t operator()(const C& c, typename Self::Value0*, const Self&) const {
				return static_cast<counter_t>(c);
			}
		};
		using CTypes = lubee::Types<float,uint32_t,uint64_t,int16_t>;
		using CTypesAc = lubee::Types<
							AcCheck<lubee::Wrapper<float>, Getter>,
							uint32_t,
							AcCheck<lubee::Wrapper<uint64_t>, Getter>,
							int16_t
						>;
		using C2Types = lubee::Types<
							float,
							double
						>;
		using C2TypesAc = lubee::Types<
							AcCheck<lubee::Wrapper<float>, Getter>,
							double
						>;
		using NTypes = ::testing::Types<
							std::tuple<CTypes, C2Types>,
							std::tuple<CTypesAc, C2Types>,
							std::tuple<CTypes, C2TypesAc>,
							std::tuple<CTypesAc, C2TypesAc>
						>;
		TYPED_TEST_CASE(RFlag, NTypes);

		TYPED_TEST(RFlag, General) {
			USING(RF);
			auto& mt = this->mt();
			RF obj(mt);
			using Cache_t = typename RF::Cache_t;
			using Action = typename RF::Action;
			const auto mtf = mt.template getUniformF<int>();
			const int NReset = mtf({1, 8});
			for(int k=0 ; k<NReset ; k++) {
				const int N = mtf({0, 128});
				for(int i=0 ; i<N ; i++) {
					// ランダムに(Set|Get|Ref) + (ValueType各種) の組み合わせで動作チェック
					const int idVal = mtf({0, Cache_t::size-1}),
								idAct = mtf({0, Action::_Num-1});
					const int act = (idAct << RF::ValB) | idVal;
					ASSERT_NO_FATAL_FAILURE(RF::s_chkfunc[act](obj));
					// 時々まとめてget関数のチェックを入れる
					if(mtf({0, 31}) == 0) {
						const int idget = mtf({0, Cache_t::size-1});
						ASSERT_NO_FATAL_FAILURE(RF::s_chkgetfunc[idget](obj));
					}
				}
				obj.resetAll();
			}

			lubee::CheckSerialization(obj._rflag);
		}
	}
}
