#pragma once
#include <tuple>
#include <boost/preprocessor.hpp>
#include "lubee/meta/typelist.hpp"
#include "lubee/meta/constant_t.hpp"

namespace spi {
	template <class... Ts>
	struct ValueHolder {
		template <class T2>
		void ref(T2*) noexcept {}
		template <class T2>
		void cref(T2*) const noexcept {}
	};
	template <class T, class... Ts>
	struct ValueHolder<T, Ts...> : ValueHolder<Ts...> {
		using base_t = ValueHolder<Ts...>;
		using value_t = typename T::value_t;
		value_t		value;

		value_t& ref(T*) noexcept {
			return value;
		}
		const value_t& cref(T*) const noexcept {
			return value;
		}
		template <class T2>
		decltype(auto) ref(T2*) noexcept {
			return base_t::ref((T2*)nullptr);
		}
		template <class T2>
		decltype(auto) cref(T2*) const noexcept {
			return base_t::cref((T2*)nullptr);
		}
	};
	//! 更新フラグ群を格納する数値型
	using RFlagValue_t = uint_fast32_t;
	struct RFlagRet {
		bool			bRefreshed;		//!< 変数値の更新がされた時はtrue
		RFlagValue_t	flagOr;			//!< FlagValueのOr差分 (一緒に更新された変数を伝達する時に使用)
	};
	//! 各々の値が変更される毎にインクリメントされる値
	using AcCounter_t = uint_fast32_t;
	//! 変数が更新された時の累積カウンタの値を後で比較するためのラッパークラス
	template <class T, class Getter, class... Ts>
	class AcWrapper : public T {
		public:
			using Flag_t = std::tuple<Ts...>;
			using Getter_t = Getter;
			using Counter_t = typename Getter::counter_t;
		private:
			mutable AcCounter_t ac_counter[sizeof...(Ts)];
			mutable Counter_t user_counter[sizeof...(Ts)];
		public:
			template <class... Args>
			explicit AcWrapper(Args&&... args):
				T(std::forward<Args>(args)...)
			{
				for(auto& a : ac_counter)
					a = ~0;
				for(auto& a : user_counter)
					a = ~0;
			}
			AcWrapper& operator = (const T& t) {
				static_cast<T&>(*this) = t;
				return *this;
			}
			AcWrapper& operator = (T&& t) {
				static_cast<T&>(*this) = std::move(t);
				return *this;
			}
			template <class Type>
			AcCounter_t& refAc() const {
				return ac_counter[Flag_t::template Find<Type>];
			}
			template <class Type>
			Counter_t& refUserAc() const {
				return user_counter[Flag_t::template Find<Type>];
			}
	};
	//! TがAcWrapperテンプレートクラスかをチェック
	template <class T>
	struct IsAcWrapper : std::false_type {};
	template <class... Ts>
	struct IsAcWrapper<AcWrapper<Ts...>> : std::true_type {};

	//! キャッシュ変数の自動管理クラス
	template <class Class, class... Ts>
	class RFlag : public ValueHolder<Ts...> {
		private:
			//! 更新処理がかかる度にインクリメントされるカウンタ
			mutable AcCounter_t _accum[sizeof...(Ts)];

			using base_t = ValueHolder<Ts...>;
			using ct_base = ::lubee::Types<Ts...>;

			//! キャッシュフラグを格納する変数
			mutable RFlagValue_t _rflag;

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
				constexpr auto TFlag = Get<T>();
				// 親クラスのRefresh関数を呼ぶ
				RFlagRet ret = self->_refresh(ptr->ref(_NullPtr<T>()), _NullPtr<T>());
				// 一緒に更新された変数フラグを立てる
				_rflag |= ret.flagOr;
				_rflag &= ~TFlag;
				D_Assert(!(_rflag & (OrHL0<T>() & ~TFlag & ~ACFlag)), "refresh flag was not cleared correctly");
				// Accumulationクラスを継承している変数は常に更新フラグを立てておく
				_rflag |= ACFlag;
				// 累積カウンタをインクリメント
				++_accum[GetFlagIndex<T>()];
				// 変数が更新された場合にはsecondに当該変数のフラグを返す
				return CRef_p<T>(
							ptrC->cref(_NullPtr<T>()),
							ret.bRefreshed ? TFlag : 0
						);
			}
			//! 上位の変数が使用する下位の変数のフラグを計算 (直下のノードのみ)
			template <class T, int N>
			static constexpr RFlagValue_t __IterateHL0(IConst<N>) noexcept {
				using T0 = typename T::template At<N>;
				return Get<T0>() | __IterateHL0<T>(IConst<N-1>());
			}
			template <class T>
			static constexpr RFlagValue_t __IterateHL0(IConst<-1>) noexcept { return 0; }
			template <class T>
			static constexpr RFlagValue_t _IterateHL0() noexcept { return __IterateHL0<T>(IConst<T::size-1>()); }

			//! 上位の変数が使用する下位の変数のフラグを計算
			template <class T, int N>
			static constexpr RFlagValue_t __IterateHL(IConst<N>) noexcept {
				using T0 = typename T::template At<N>;
				return OrHL<T0>() | __IterateHL<T>(IConst<N-1>());
			}
			template <class T>
			static constexpr RFlagValue_t __IterateHL(IConst<-1>) noexcept { return 0; }
			template <class T>
			static constexpr RFlagValue_t _IterateHL() noexcept { return __IterateHL<T>(IConst<T::size-1>()); }

			//! integral_constantの値がtrueなら引数テンプレートのOrLH()を返す
			template <class T0>
			static constexpr RFlagValue_t _Add_If(std::false_type) noexcept { return 0; }
			template <class T0>
			static constexpr RFlagValue_t _Add_If(std::true_type) noexcept { return OrLH<T0>(); }

			//! 下位の変数に影響される上位の変数のフラグを計算
			template <class T, int N>
			static constexpr RFlagValue_t __IterateLH(IConst<N>) noexcept {
				using T0 = typename ct_base::template At<N>;
				using T0Has = typename T0::template Has<T>;
				return _Add_If<T0>(T0Has()) | __IterateLH<T>(IConst<N+1>());
			}
			template <class T>
			static constexpr RFlagValue_t __IterateLH(IConst<ct_base::size>) noexcept { return 0; }
			template <class T>
			static constexpr RFlagValue_t _IterateLH() noexcept { return __IterateLH<T>(IConst<0>()); }

			//! 引数の変数フラグをORで合成
			template <class... TsA>
			static constexpr RFlagValue_t _Sum(IConst<0>) noexcept { return 0; }
			template <class TA, class... TsA, int N>
			static constexpr RFlagValue_t _Sum(IConst<N>) noexcept {
				return GetSingle<TA>() | _Sum<TsA...>(IConst<N-1>());
			}

			template <class T>
			constexpr static T ReturnIf(T flag, std::true_type) noexcept { return flag; }
			template <class T>
			constexpr static T ReturnIf(T, std::false_type) noexcept { return 0; }

			template <class... TsA>
			constexpr static RFlagValue_t _CalcAcFlag(lubee::Types<TsA...>) noexcept { return 0; }
			template <class T, class... TsA>
			constexpr static RFlagValue_t _CalcAcFlag(lubee::Types<T,TsA...>) noexcept {
				return ReturnIf(OrLH<T>(), IsAcWrapper<typename T::value_t>()) | _CalcAcFlag(lubee::Types<TsA...>());
			}

			template <class... TsA>
			void __setFlag(IConst<0>) noexcept {}
			template <class T, class... TsA, int N>
			void __setFlag(IConst<N>) noexcept {
				// 自分の階層より上の変数は全てフラグを立てる
				_rflag |= OrLH<T>();
				_rflag &= ~Get<T>();
				// Accumulationクラスを継承している変数は常に更新フラグを立てておく
				_rflag |= ACFlag;
				// 累積カウンタをインクリメント
				++_accum[GetFlagIndex<T>()];
				__setFlag<TsA...>(IConst<N-1>());
			}
			//! 変数に影響するフラグを立てる
			template <class... TsA>
			void _setFlag() noexcept {
				__setFlag<TsA...>(IConst<sizeof...(TsA)>());
			}

		public:
			//! 変数型の格納順インデックス
			template <class TA>
			static constexpr int GetFlagIndex() noexcept {
				constexpr int pos = ct_base::template Find<TA>;
				static_assert(pos>=0, "invalid flag tag");
				return pos;
			}
			//! 変数型を示すフラグ
			template <class TA>
			static constexpr RFlagValue_t GetSingle() noexcept {
				return 1 << GetFlagIndex<TA>();
			}
			//! 変数全てを示すフラグ値
			static constexpr RFlagValue_t All() noexcept {
				return (1 << (sizeof...(Ts)+1)) -1;
			}
			//! 引数の変数を示すフラグ
			template <class... TsA>
			static constexpr RFlagValue_t Get() noexcept {
				return _Sum<TsA...>(IConst<sizeof...(TsA)>());
			}
			template <class... TsA>
			static constexpr RFlagValue_t GetFromTypes(lubee::Types<TsA...>) noexcept {
				return Get<TsA...>();
			}
			//! 自分より上の階層のフラグ (Low -> High)
			template <class T>
			static constexpr RFlagValue_t OrLH() noexcept {
				// TypeListを巡回、A::Has<T>ならOr<A>()をたす
				return Get<T>() | _IterateLH<T>();
			}
			//! 自分以下の階層のフラグ (High -> Low)
			template <class T>
			static constexpr RFlagValue_t OrHL() noexcept {
				return Get<T>() | _IterateHL<T>();
			}
			//! 自分以下の階層のフラグ (High -> Low) 直下のみ
			template <class T>
			static constexpr RFlagValue_t OrHL0() noexcept {
				return Get<T>() | _IterateHL0<T>();
			}
			//! 全てのキャッシュを無効化
			void resetAll() noexcept {
				for(auto& a : _accum)
					a = 0;
				_rflag = All();
			}
			constexpr static RFlagValue_t ACFlag = _CalcAcFlag(ct_base());

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
				return _accum[GetFlagIndex<TA>()];
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
				return getFlag() & Get<TsA...>();
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
				if(!(_rflag & Get<T>()))
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
	template <class Base, class Getter>
	struct AcCheck {};
	// AcCheckを継承していればAcWrapperを、そうでなければTを返す
	template <class... Ts, class Base, class Getter>
	static AcWrapper<Base, Getter, Ts...> AcDetect(AcCheck<Base, Getter>*);
	template <class... Ts, class T>
	static T AcDetect(T*);

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
			using value_t = decltype(::spi::AcDetect<__VA_ARGS__>((valueT*)nullptr)); };
	// 中間refresh関数 -> 宣言のみ
	#define RFLAG_CACHEDVALUE_MIDDLE(name, valueT, ...) \
		::spi::RFlagRet _refresh(typename name::value_t&, name*) const;
	// 最下層refresh関数 -> 何もしない
	#define RFLAG_CACHEDVALUE_LOW(name, valueT) \
		::spi::RFlagRet _refresh(typename name::value_t&, name*) const { return {true, 0}; }
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

			更新関数を RFlagRet _refresh(Type0&, Value0*) const; の形で記述
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
