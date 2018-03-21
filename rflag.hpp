#pragma once
#include "valueholder.hpp"
#include <tuple>
#include <boost/preprocessor.hpp>
#include "lubee/meta/typelist.hpp"
#include "lubee/meta/constant_t.hpp"
#include "lubee/wrapper.hpp"
#include "lubee/error.hpp"

namespace spi {
	//! 更新フラグ群を格納する数値型
	using RFlagValue_t = uint_fast32_t;
	//! 各々の値が変更される毎にインクリメントされる値
	using AcCounter_t = uint_fast32_t;

	//! 毎フレームの更新チェックが必要な際に使うキャッシュ変数ラッパー
	/*!
		\tparam	CacheVal	内部キャッシュ値
		\tparam	Getter		累積カウンタを取得するための関数クラス
	*/
	template <class CacheVal, class Getter>
	struct AcCheck {};

	namespace detail {
		//! 累積カウンタの比較用
		template <class T>
		bool CompareAndSet(T& num, const T& target) {
			if(num != target) {
				num = target;
				return true;
			}
			return false;
		}
		//! 変数が更新された時の累積カウンタの値を後で比較するためのラッパークラス
		/*!
			\tparam	CacheVal	内部キャッシュ値
			\tparam	Getter		累積カウンタを取得するための関数クラス
			\tparam	Ts			依存キャッシュ値
		*/
		template <class CacheVal, class Getter, class... Ts>
		class AcWrapper : public CacheVal {
			public:
				using CacheVal_t = CacheVal;
				using Depend_t = lubee::Types<Ts...>;
				using Getter_t = Getter;
				using Counter_t = typename Getter::counter_t;
				using value_t = typename CacheVal_t::value_t;
			private:
				mutable AcCounter_t ac_counter[sizeof...(Ts)];
				mutable Counter_t user_counter[sizeof...(Ts)];

				template <class Ar, class C, class G, class... T>
				friend void serialize(Ar&, AcWrapper<C,G,T...>&);
			public:
				template <class... Args>
				explicit AcWrapper(Args&&... args):
					CacheVal_t(std::forward<Args>(args)...)
				{
					for(auto& a : ac_counter)
						a = ~0;
					for(auto& a : user_counter)
						a = ~0;
				}
				template <
					class T,
					ENABLE_IF((std::is_assignable<CacheVal_t, T>{}))
				>
				AcWrapper& operator = (T&& t) {
					static_cast<CacheVal_t&>(*this) = std::forward<T>(t);
					return *this;
				}
				bool operator == (const AcWrapper& w) const noexcept {
					return static_cast<const CacheVal_t&>(*this) ==
							static_cast<const CacheVal_t&>(w);
				}
				bool operator != (const AcWrapper& w) const noexcept {
					return !(this->operator == (w));
				}
				template <class Type>
				AcCounter_t& refAc() const {
					return ac_counter[Depend_t::template Find<Type>];
				}
				template <class Type>
				Counter_t& refUserAc() const {
					return user_counter[Depend_t::template Find<Type>];
				}
		};
		//! TがAcWrapperテンプレートクラスかをチェック
		template <class T>
		struct IsAcWrapper : std::false_type {};
		template <class... Ts>
		struct IsAcWrapper<AcWrapper<Ts...>> : std::true_type {};
		// TがAcCheckを継承していればAcWrapperを、そうでなければTを返す
		template <class... Depend, class CacheVal, class Getter>
		AcWrapper<CacheVal, Getter, Depend...> Convert_AcCheck(AcCheck<CacheVal, Getter>*);
		template <class... , class T>
		T Convert_AcCheck(T*);
	}
	//! TがAcWrapperならばその内部値を返す
	template <class T>
	inline decltype(auto) UnwrapAcValue(const T& v) noexcept {
		if constexpr (detail::IsAcWrapper<T>{}) {
			return lubee::UnwrapValue(static_cast<const typename T::CacheVal_t&>(v));
		} else {
			return v;
		}
	}

	//! キャッシュ変数の自動管理クラス
	template <class Class, class... Ts>
	class RFlag : public ValueHolder<Ts...> {
		private:
			//! 更新処理がかかる度にインクリメントされるカウンタ
			mutable AcCounter_t _accum[sizeof...(Ts)];

			using base_t = ValueHolder<Ts...>;
			//! キャッシュフラグを格納する変数
			mutable RFlagValue_t _rflag;

			template <class Ar, class C, class... T>
			friend void save(Ar&, const RFlag<C,T...>&);
			template <class Ar, class C, class... T>
			friend void load(Ar&, RFlag<C,T...>&);
		public:
			using ct_base = ::lubee::Types<Ts...>;
			// デバッグ用
			bool operator == (const RFlag& rf) const noexcept {
				return _rflag == rf._rflag &&
					static_cast<const base_t&>(*this) == static_cast<const base_t&>(rf);
			}
			bool operator != (const RFlag& rf) const noexcept {
				return !(this->operator == (rf));
			}
		private:
			//! 整数const型
			template <int N>
			using IConst = lubee::IConst<N>;
			template <class T>
			static T* _NullPtr() noexcept { return reinterpret_cast<T*>(0); }

			// タグを引数として実際の変数型を取り出す
			template <class T>
			using cref_type = decltype(std::declval<base_t>().cref(_NullPtr<T>()));
			template <class T>
			using ref_type = typename std::decay<cref_type<T>>::type&;
			template <class T>
			using cptr_type = const typename std::decay<cref_type<T>>::type*;
			template <class T>
			using CRef_p = std::pair<cref_type<T>, RFlagValue_t>;

			//! キャッシュ変数Tを更新
			template <class T>
			CRef_p<T> _refreshSingle(const Class* self) const {
				const base_t* ptrC = this;
				base_t* ptr = const_cast<base_t*>(ptrC);
				constexpr auto TFlag = FlagMask<T>();
				// 親クラスのRefresh関数を呼ぶ
				const bool ret = self->_refresh(ptr->ref(_NullPtr<T>()), _NullPtr<T>());
				// 一緒に更新された変数フラグを立てる
				_rflag &= ~TFlag;
				D_Assert(!(_rflag & (LowerMask0AndMe<T>() & ~TFlag & ~ACFlag)), "refresh flag was not cleared correctly");
				// Accumulationクラスを継承している変数は常に更新フラグを立てておく
				_rflag |= ACFlag;
				// 累積カウンタをインクリメント
				if(ret)
					++_accum[_GetFlagIndex<T>()];
				// 変数が更新された場合にはsecondに当該変数のフラグを返す
				return CRef_p<T>(
							ptrC->cref(_NullPtr<T>()),
							ret ? TFlag : 0
						);
			}
			//! 上位の変数が使用する下位の変数のフラグを計算 (直下のノードのみ)
			static constexpr RFlagValue_t _LowerMask0(lubee::Types<>) noexcept { return 0; }
			template <class T0, class... TsA>
			static constexpr RFlagValue_t _LowerMask0(lubee::Types<T0,TsA...>) noexcept {
				return FlagMask<T0>() | _LowerMask0(lubee::Types<TsA...>());
			}

			//! 上位の変数が使用する下位の変数のフラグを計算
			static constexpr RFlagValue_t _LowerMask(lubee::Types<>) noexcept { return 0; }
			template <class T0, class... TsA>
			static constexpr RFlagValue_t _LowerMask(lubee::Types<T0,TsA...>) noexcept {
				return LowerMaskAndMe<T0>() | _LowerMask(lubee::Types<TsA...>());
			}

			//! integral_constantの値がtrueなら引数テンプレートのUpperMaskAndMe()を返す
			template <class T0>
			static constexpr RFlagValue_t _UpperMaskAndMe_If(std::false_type) noexcept { return 0; }
			template <class T0>
			static constexpr RFlagValue_t _UpperMaskAndMe_If(std::true_type) noexcept { return UpperMaskAndMe<T0>(); }

			//! 下位の変数に影響される上位の変数のフラグを計算
			template <class T, class T0, class... TsA>
			static constexpr RFlagValue_t __UpperMask(lubee::Types<T0, TsA...>) noexcept {
				using Has = typename T0::template Has<T>;
				return _UpperMaskAndMe_If<T0>(Has()) | __UpperMask<T>(lubee::Types<TsA...>());
			}
			template <class T>
			static constexpr RFlagValue_t __UpperMask(lubee::Types<>) noexcept { return 0; }
			template <class T>
			static constexpr RFlagValue_t _UpperMask() noexcept {
				return __UpperMask<T>(ct_base());
			}

			//! 引数の変数フラグをORで合成
			static constexpr RFlagValue_t _FlagMaskOr(lubee::Types<>) noexcept { return 0; }
			template <class TA, class... TsA>
			static constexpr RFlagValue_t _FlagMaskOr(lubee::Types<TA, TsA...>) noexcept {
				return _FlagMaskSingle<TA>() | _FlagMaskOr(lubee::Types<TsA...>());
			}

			constexpr static RFlagValue_t _CalcAcFlag(lubee::Types<>) noexcept { return 0; }
			template <class T, class... TsA>
			constexpr static RFlagValue_t _CalcAcFlag(lubee::Types<T,TsA...>) noexcept {
				return _UpperMaskAndMe_If<T>(detail::IsAcWrapper<typename T::value_t>()) | _CalcAcFlag(lubee::Types<TsA...>());
			}

			// シリアライズ用。最下層のレイヤーのみを巡回
			template <class CB>
			void __iterateLowestLayer(CB&&, lubee::Types<>) {}
			template <class CB, class T0, class... TsA>
			void __iterateLowestLayer(CB&& cb, lubee::Types<T0, TsA...>) {
				if(T0::size == 0) {
					constexpr int pos = ct_base::template Find<T0>;
					cb(ref<T0>(), _accum[pos]);
				}
				__iterateLowestLayer(cb, lubee::Types<TsA...>());
			}
			template <class CB>
			void _iterateLowestLayer(CB&& cb) {
				__iterateLowestLayer(cb, ct_base());
			}

			template <class... TsA>
			void __setFlag(IConst<0>) noexcept {}
			template <class T, class... TsA, int N>
			void __setFlag(IConst<N>) noexcept {
				// 自分の階層より上の変数は全てフラグを立てる
				_rflag |= UpperMaskAndMe<T>();
				_rflag &= ~FlagMask<T>();
				// Accumulationクラスを継承している変数は常に更新フラグを立てておく
				_rflag |= ACFlag;
				// 累積カウンタをインクリメント
				++_accum[_GetFlagIndex<T>()];
				__setFlag<TsA...>(IConst<N-1>());
			}
			//! 変数に影響するフラグを立てる
			template <class... TsA>
			void _setFlag() noexcept {
				__setFlag<TsA...>(IConst<sizeof...(TsA)>());
			}
			//! キャッシュ変数ポインタをstd::tupleにして返す
			template <int N, class Tup>
			RFlagValue_t _getAsTuple(const Class* /*self*/, const Tup& /*dst*/) const { return 0; }
			template <int N, class Tup, class T, class... TsA>
			RFlagValue_t _getAsTuple(const Class* self, Tup& dst, T*, TsA*... remain) const {
				auto ret = refresh<T>(self);
				std::get<N>(dst) = &ret.first;
				return ret.second | _getAsTuple<N+1>(self, dst, remain...);
			}

			template <int N, class Acc, class Tup>
			RFlagValue_t _getWithCheck(const Class*, const Acc&, const Tup&) const { return 0; }
			template <int N, class Acc, class Tup, class T, class... TsA>
			RFlagValue_t _getWithCheck(const Class* self, Acc& acc, Tup& dst, T*, TsA*... remain) const {
				auto ret = refresh<T>(self);
				// キャッシュ値のポインタはdstへ
				std::get<N>(dst) = &ret.first;
				// refreshがかかるか、累積カウンタが異なっていればキャッシュ値のフラグを返す
				if constexpr (typename Acc::Depend_t::template Has<T>{}) {
					using C = typename Acc::Counter_t;
					using G = typename Acc::Getter_t;
					// ユーザー定義の累積カウンタ、システム定義(RFlag)の累積カウンタの何れかが異なっていたらキャッシュフラグを返す
					if(detail::CompareAndSet<C>(acc.template refUserAc<T>(), G()(ret.first, _NullPtr<T>(), *self)) |
						detail::CompareAndSet(acc.template refAc<T>(), getAcCounter<T>()))
						ret.second |= FlagMask<T>();
				}
				return ret.second | _getWithCheck<N+1>(self, acc, dst, remain...);
			}
			template <class Acc, class... TsA>
			auto _getWithCheck(const Class* self, Acc& acc, lubee::Types<TsA...>) const {
				Cache<TsA...> ret;
				ret.flag = _getWithCheck<0>(self, acc, ret, ((TsA*)nullptr)...);
				return ret;
			}
			//! 変数型の格納順インデックス
			template <class TA>
			static constexpr int _GetFlagIndex() noexcept {
				constexpr int pos = ct_base::template Find<TA>;
				static_assert(pos>=0, "invalid flag tag");
				return pos;
			}
			//! 変数型を示すフラグ(単体)
			template <class TA>
			static constexpr RFlagValue_t _FlagMaskSingle() noexcept {
				return 1 << _GetFlagIndex<TA>();
			}

		public:
			//! 変数全てを示すフラグ値
			static constexpr RFlagValue_t FlagMaskAll() noexcept {
				return (1 << (sizeof...(Ts)+1)) -1;
			}
			//! 引数の変数を示すフラグ
			template <class... TsA>
			static constexpr RFlagValue_t FlagMask() noexcept {
				return _FlagMaskOr(lubee::Types<TsA...>());
			}
			template <class... TsA>
			static constexpr RFlagValue_t FlagMask(lubee::Types<TsA...>) noexcept {
				return FlagMask<TsA...>();
			}
			//! 自分より上の階層のフラグ (Low -> High)
			template <class T>
			static constexpr RFlagValue_t UpperMaskAndMe() noexcept {
				// TypeListを巡回、A::Has<T>ならOr<A>()をたす
				return FlagMask<T>() | _UpperMask<T>();
			}
			//! 自分以下の階層のフラグ (High -> Low)
			template <class T>
			static constexpr RFlagValue_t LowerMaskAndMe() noexcept {
				return FlagMask<T>() | _LowerMask(T());
			}
			//! 自分以下の階層のフラグ (High -> Low) 直下のみ
			template <class T>
			static constexpr RFlagValue_t LowerMask0AndMe() noexcept {
				return FlagMask<T>() | _LowerMask0(T());
			}
			//! 全てのキャッシュを無効化
			void resetAll() noexcept {
				for(auto& a : _accum)
					a = 0;
				_rflag = FlagMaskAll();
			}
			constexpr static RFlagValue_t ACFlag = _CalcAcFlag(ct_base());

			template <class... TsA>
			struct Cache : std::tuple<cptr_type<TsA>...> {
				RFlagValue_t	flag;

				explicit operator bool() const {
					return flag != 0;
				}
			};
			//! 一度に複数のキャッシュ変数をポインタで取得
			template <class... TsA>
			Cache<TsA...> getAsTuple(const Class* self) const {
				Cache<TsA...> ret;
				ret.flag = _getAsTuple<0>(self, ret, ((TsA*)nullptr)...);
				return ret;
			}
			// 独自の累積カウンタ
			//! _refresh()から呼び出す
			template <class Acc>
			auto getWithCheck(const Class* self, Acc& acc) const {
				return _getWithCheck(self, acc, typename Acc::Depend_t());
			}

			RFlag() noexcept {
				resetAll();
			}
			//! 現在のフラグ値
			RFlagValue_t getFlag() const noexcept {
				return _rflag;
			}
			//! 累積カウンタ値取得
			template <class TA>
			AcCounter_t getAcCounter() const noexcept {
				return _accum[_GetFlagIndex<TA>()];
			}

			//! 更新フラグだけを立てる
			/*! 累積カウンタも更新 */
			template <class... TsA>
			void setFlag() noexcept {
				_setFlag<TsA...>();
			}
			//! フラグのテストだけする
			template <class... TsA>
			RFlagValue_t test() const noexcept {
				return getFlag() & FlagMask<TsA...>();
			}
			//! 更新フラグはそのままに参照を返す
			template <class T>
			ref_type<T> ref() const noexcept {
				const auto& ret = base_t::cref(_NullPtr<T>());
				return const_cast<std::decay_t<decltype(ret)>&>(ret);
			}
			//! 更新フラグを立てつつ参照を返す
			template <class T>
			ref_type<T> refF() noexcept {
				// 更新フラグをセット
				setFlag<T>();
				return ref<T>();
			}
			//! キャッシュ機能付きの値を変更する際に使用
			template <class T, class TA>
			void set(TA&& t) {
				refF<T>() = std::forward<TA>(t);
			}
			//! リフレッシュコールだけをする
			template <class T>
			CRef_p<T> refresh(const Class* self) const {
				// 全ての変数が更新済みなら参照を返す
				if(!(_rflag & FlagMask<T>()))
					return CRef_p<T>(base_t::cref(_NullPtr<T>()), 0);
				// 更新をかけて返す
				return _refreshSingle<T>(self);
			}
			//! 必要に応じて更新をかけつつ、参照を返す
			template <class T>
			cref_type<T> get(const Class* self) const {
				return refresh<T>(self).first;
			}
	};
	template <class C>
	struct RFlag_Getter {
		using counter_t = C;
		template <class Value, class Tag, class Self>
		counter_t operator()(const Value&, Tag*, const Self&) const {
			return 0;
		}
	};

	#define PASS_THROUGH(func, ...)		func(__VA_ARGS__)
	#define SEQ_GETFIRST(z, data, elem)	(BOOST_PP_SEQ_ELEM(0,elem))

	#define RFLAG_GETMETHOD(name) decltype(auto) get##name() const { return _rflag.template get<name>(this); }
	#define CALL_RFLAG_GETMETHOD(z, data, elem)	RFLAG_GETMETHOD(elem)

	#define RFLAG_GETMETHOD_DEFINE(seq) \
		BOOST_PP_SEQ_FOR_EACH( \
			CALL_RFLAG_GETMETHOD, \
			BOOST_PP_NIL, \
			BOOST_PP_SEQ_FOR_EACH( \
				SEQ_GETFIRST, \
				BOOST_PP_NIL, \
				seq \
			) \
		)

	// キャッシュ変数タグ定義
	#define RFLAG_CACHEDVALUE_BASE(name, valueT, ...) \
		struct name : ::lubee::Types<__VA_ARGS__> { \
			using value_t = decltype(::spi::detail::Convert_AcCheck<__VA_ARGS__>((valueT*)nullptr)); };
	// 中間refresh関数 -> 宣言のみ
	#define RFLAG_CACHEDVALUE_MIDDLE(name, valueT, ...) \
		bool _refresh(typename name::value_t&, name*) const;
	// 最下層refresh関数 -> 何もしない
	#define RFLAG_CACHEDVALUE_LOW(name, valueT) \
		bool _refresh(typename name::value_t&, name*) const { return false; }
	// ...の部分が単体ならLOWを、それ以外はMIDDLEを呼ぶ
	#define RFLAG_CACHEDVALUE(name, ...) \
			RFLAG_CACHEDVALUE_BASE(name, __VA_ARGS__) \
			BOOST_PP_IF( \
				BOOST_PP_LESS_EQUAL( \
					BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), \
					1 \
				), \
				RFLAG_CACHEDVALUE_LOW, \
				RFLAG_CACHEDVALUE_MIDDLE \
			)(name, __VA_ARGS__)
	#define CALL_RFLAG_CACHEDVALUE(z, data, elem)	PASS_THROUGH(RFLAG_CACHEDVALUE, BOOST_PP_SEQ_ENUM(elem))
	// キャッシュフラグ変数定義
	#define RFLAG_FLAG(clazz, seq) \
		using RFlag_t = ::spi::RFlag<clazz, BOOST_PP_SEQ_ENUM(seq)>; \
		RFlag_t _rflag; \
		template <class T, class... Ts> \
		friend class ::spi::RFlag;

	//! シーケンスによるキャッシュ変数定義
	#define RFLAG_DEFINE(clazz, seq)	\
		BOOST_PP_SEQ_FOR_EACH( \
			CALL_RFLAG_CACHEDVALUE, \
			BOOST_PP_NIL, \
			seq \
		) \
		RFLAG_FLAG( \
			clazz, \
			BOOST_PP_SEQ_FOR_EACH( \
				SEQ_GETFIRST, \
				BOOST_PP_NIL, \
				seq \
			) \
		)

	#define RFLAG_TEST_DEFINE() \
		template <class... TsA> \
		RFlagValue_t test() const noexcept { \
			return _rflag.template test<TsA...>(); \
		}

	//! 何も展開しない
	#define NOTHING
	//! 引数を任意個受け取り、何も展開しない
	#define NOTHING_ARG(...)

	#define RFLAG_FUNC3(z, func, seq)	\
		BOOST_PP_IF( \
			BOOST_PP_LESS_EQUAL( \
				BOOST_PP_SEQ_SIZE(seq), \
				2 \
			), \
			func, \
			NOTHING_ARG \
		)(BOOST_PP_SEQ_ELEM(0, seq))

	//! キャッシュ変数にset()メソッドを定義
	#define RFLAG_SETMETHOD(name) \
		template <class TArg> \
		void BOOST_PP_CAT(set, name(TArg&& t)) { _rflag.template set<name>(std::forward<TArg>(t)); }
	//! 最下層のキャッシュ変数に一括でset()メソッドを定義
	#define RFLAG_SETMETHOD_DEFINE(seq) \
		BOOST_PP_SEQ_FOR_EACH( \
			RFLAG_FUNC3, \
			RFLAG_SETMETHOD, \
			seq \
		)

	//! キャッシュ変数にref()メソッドを定義
	#define RFLAG_REFMETHOD(name) \
		auto& BOOST_PP_CAT(ref, name()) { return _rflag.template refF<name>(); }
	//! 最下層のキャッシュ変数に一括でref()メソッドを定義
	#define RFLAG_REFMETHOD_DEFINE(seq) \
		BOOST_PP_SEQ_FOR_EACH( \
			RFLAG_FUNC3, \
			RFLAG_REFMETHOD, \
			seq \
		)

	/*
		class MyClass {
			#define SEQ \
				((Value0)(Type0)) \
				((Value1)(Type1)(Depends)...)
			RFLAG_DEFINE(MyClass, SEQ)

			更新関数を bool _refresh(Type0&, Value0*) const; の形で記述
			public:
				// 取得用の関数
				RFLAG_GETMETHOD(Value0)
				RFLAG_GETMETHOD(Value1)
				// または
				RFLAG_GETMETHOD_DEFINE(SEQ)

				// セット用の関数
				RFLAG_SETMETHOD(Value0)
				// 参照用の関数
				RFLAG_REFMETHOD(Value0)

			参照する時は _rflag.template get<Value0>(this) とやるか、
			getValue0()とすると依存変数の自動更新
			_rfvalue.template ref<Value0>(this)とすれば依存関係を無視して取り出せる
	*/
}
