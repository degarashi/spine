#include "test.hpp"
#include "../rflag.hpp"

namespace spi {
	namespace test {
		//! 入力値の一番左のビットだけを残す
		template <class T, ENABLE_IF(std::is_unsigned<T>{})>
		inline constexpr T BitLowClear(T x) noexcept {
			T ts = 1;
			while(ts < T(sizeof(T)*8/2)) {
				x = x | (x >> ts);
				ts <<= 1;
			}
			return x & ~(x>>1);
		}
		//! most significant bit を取得
		/*! もし入力値が0の場合は0を返す */
		inline constexpr uint32_t MSB(uint32_t x) noexcept {
			const char NLRZ_TABLE[33] = "\x1f\x00\x01\x0e\x1c\x02\x16\x0f\x1d\x1a\x03\x05\x0b\x17"
										"\x07\x10\x1e\x0d\x1b\x15\x19\x04\x0a\x06\x0c\x14\x18\x09"
										"\x13\x08\x12\x11";
			x |= 0x01;
			return NLRZ_TABLE[0x0aec7cd2U * BitLowClear(x) >> 27];
		}
		//! ビットが幾つ立っているか数える
		template <class T, ENABLE_IF(std::is_unsigned<T>{} && (sizeof(T)<=4))>
		inline constexpr int BitCount(T v) noexcept {
			T tmp = v & 0xaaaaaaaa;
			v &= 0x55555555;
			v = v + (tmp >> 1);
			tmp = v & 0xcccccccc;
			v &= 0x33333333;
			v = v + (tmp >> 2);
			tmp = v & 0xf0f0f0f0;
			v &= 0x0f0f0f0f;
			v = v + (tmp >> 4);
			tmp = v & 0xff00ff00;
			v &= 0x00ff00ff;
			v = v + (tmp >> 8);
			tmp = v & 0xffff0000;
			v &= 0x0000ffff;
			v = v + (tmp >> 16);
			return v;
		}
		template <class T, ENABLE_IF(std::is_unsigned<T>{} && (sizeof(T)==8))>
		inline constexpr int BitCount(T v) noexcept {
			return BitCount(uint32_t(v)) + BitCount(uint32_t(v>>32));
		}

		using CTypes = lubee::Types<float,uint32_t,uint64_t,int16_t>;
		using Value01_t = float;
		using Value02_t = double;
		using Value01_02_3_t = int32_t;
		template <class MT>
		class RFObj {
			private:
				template <int N>
				using IConst = lubee::IConst<N>;

				#define SEQ \
					((Value0)(CTypes::template At<0>)) \
					((Value1)(CTypes::template At<1>)) \
					((Value2)(CTypes::template At<2>)) \
					((Value3)(CTypes::template At<3>)) \
					((Value01)(Value01_t)(Value0)(Value1)) \
					((Value02)(Value02_t)(Value0)(Value2)) \
					((Value01_02_3)(Value01_02_3_t)(Value01)(Value02)(Value3))
				RFLAG_DEFINE(RFObj, SEQ)

			public:
				using Cache_t = lubee::Types<Value0, Value1, Value2, Value3, Value01, Value02, Value01_02_3>;
				struct Action {
					enum e {
						Set,
						Get,
						Ref,
						_Num
					};
				};
				constexpr static int ValW = BitLowClear(static_cast<unsigned int>(Cache_t::size)) << 1,
									ValB = MSB(ValW),
									ActW = BitLowClear(static_cast<unsigned int>(Action::_Num)) << 1,
									ActB = MSB(ActW);
				using RF = std::decay_t<decltype(_rflag)>;
				constexpr static RFlagValue_t LowFlag = RF::template Get<Value0, Value1, Value2, Value3>();

			private:
				using Values = typename Cache_t::template AsTArgs<ValueHolder>;
				Values			_value;
				RFlagValue_t	_setflag;
				mutable int		_counter;
				MT&				_mt;

				template <class Tag>
				auto _calcValue(Tag*) const {
					return _value.cref((Tag*)nullptr);
				}
				Value01_t _calcValue(Value01*) const {
					if(_setflag & RF::template Get<Value01>())
						return _value.cref((Value01*)nullptr);
					return _calcValue((Value0*)nullptr)
							+ _calcValue((Value1*)nullptr);
				}
				Value02_t _calcValue(Value02*) const {
					if(_setflag & RF::template Get<Value02>())
						return _value.cref((Value02*)nullptr);
					return _calcValue((Value0*)nullptr)
							- _calcValue((Value2*)nullptr);
				}
				Value01_02_3_t _calcValue(Value01_02_3*) const {
					if(_setflag & RF::template Get<Value01_02_3>())
						return _value.cref((Value01_02_3*)nullptr);
					return _calcValue((Value01*)nullptr)
							* _calcValue((Value02*)nullptr)
							* _calcValue((Value3*)nullptr);
				}

			public:
				// 動作チェック用関数配列: Func[Action | Tag]
				using ChkFunc = std::function<void (RFObj&)>;
				static ChkFunc s_chkfunc[1 << (ActB + ValB)];

			private:
				template <class Tag>
				void _updateSetflag() {
					_setflag &= ~RF::template OrLH<Tag>();
					_setflag |= RF::template Get<Tag>();
					ASSERT_EQ(0, _setflag & _rflag.getFlag() & ~LowFlag);
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
							obj._counter = 0;
							ASSERT_EQ(val, obj._rflag.template ref<Tag>());
							// セットした値を取得しても更新はかからない
							ASSERT_EQ(val, obj._rflag.template get<Tag>(&obj));
							ASSERT_EQ(0, obj._counter);

							ASSERT_NO_FATAL_FAILURE(obj._updateSetflag<Tag>());
							obj._sync();
						};
				}
				// Get<Tag>の動作をチェック
				template <class Tag>
				static ChkFunc _CheckF(IConst<Action::Get>) {
					return
						[](RFObj& obj){
							// 下位の値から算出
							const auto val = obj._calcValue((Tag*)nullptr);
							// 最下位の値を除いた更新フラグの数だけカウンタがインクリメントされる(筈)
							const RFlagValue_t flag = RF::template OrHL<Tag>() & (~LowFlag) & obj._rflag.getFlag();
							if(obj._rflag.template test<Tag>()) {
								const int nbit = BitCount(flag);
								obj._counter = 0;
								ASSERT_EQ(val, obj._rflag.template get<Tag>(&obj));
								ASSERT_EQ(nbit, obj._counter);

								obj._sync();
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
							obj._rflag.template refF<Tag>() = val;
							obj._counter = 0;
							// セットした値がそのまま取り出せるか
							ASSERT_EQ(val, obj._rflag.template get<Tag>(&obj));
							// セットした箇所なので更新はかからない
							ASSERT_EQ(0, obj._counter);

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

				// Tのランダム値
				template <class T>
				T _genValue() {
					return _mt.template getUniform<T>();
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

			public:
				RFObj(MT& mt):
					_mt(mt)
				{
					_InitFunc();
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
		template <class MT>
		RFlagRet RFObj<MT>::_refresh(typename RFObj::Value01::value_t& dst, RFObj::Value01*) const {
			++_counter;
			dst = getValue0() + getValue1();
			return {true, 0};
		}
		template <class MT>
		RFlagRet RFObj<MT>::_refresh(typename RFObj::Value02::value_t& dst, RFObj::Value02*) const {
			++_counter;
			dst = getValue0() - getValue2();
			return {true, 0};
		}
		template <class MT>
		RFlagRet RFObj<MT>::_refresh(typename RFObj::Value01_02_3::value_t& dst, RFObj::Value01_02_3*) const {
			++_counter;
			dst = getValue01() * getValue02() * getValue3();
			return {true, 0};
		}
		template <class MT>
		typename RFObj<MT>::ChkFunc RFObj<MT>::s_chkfunc[1 << (ActB + ValB)] = {};

		struct RFlag : Random {};
		TEST_F(RFlag, General) {
			auto& mt = this->mt();
			using RF = RFObj<std::decay_t<decltype(mt)>>;
			RF obj(mt);
			using Cache_t = typename RF::Cache_t;
			using Action = typename RF::Action;

			const auto mtf = mt.getUniformF<int>();
			const int NReset = mtf({1, 8});
			for(int k=0 ; k<NReset ; k++) {
				const int N = mtf({0, 128});
				for(int i=0 ; i<N ; i++) {
					// ランダムに(Set|Get|Ref) + (ValueType各種) の組み合わせで動作チェック
					const int idVal = mtf({0, Cache_t::size-1}),
								idAct = mtf({0, Action::_Num-1});
					const int act = (idAct << RF::ValB) | idVal;
					ASSERT_NO_FATAL_FAILURE(RF::s_chkfunc[act](obj));
				}
				obj.resetAll();
			}
		}

		//TODO: 他の更新フラグを同時に立てられるケースのチェック
		//TODO: ユーザーの意図で更新フラグを立てない場合のチェック
		//TODO: 他クラスとの連携で毎回チェックする変数を含む場合のチェック
	}
}
