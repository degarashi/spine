#pragma once
#include "detect_type.hpp"
#include "lubee/meta/enable_if.hpp"
#include "lubee/none.hpp"
#include <utility>

namespace spi {
	using none_t = lubee::none_t;
	const static none_t none;

	template <class P>
	using IsRP = std::integral_constant<bool, std::is_reference<P>{} || std::is_pointer<P>{}>;
	namespace opt_tmp {
		template <class T>
		struct alignas(alignof(T)) Buffer {
			uint8_t		_data[sizeof(T)];

			Buffer() = default;
			template <class T2>
			Buffer(T2&& t) {
				new(ptr()) T(std::forward<T2>(t));
			}

			T* ptr() noexcept {
				// Debug時は中身が有効かチェック
				#ifdef DEBUG
				#endif
				return reinterpret_cast<T*>(_data);
			}
			const T* ptr() const noexcept {
				// Debug時は中身が有効かチェック
				#ifdef DEBUG
				#endif
				return reinterpret_cast<const T*>(_data);
			}
			T& get() noexcept {
				return *ptr();
			}
			const T& get() const noexcept {
				return *ptr();
			}
			template <class... Ts>
			void ctor(Ts&&... ts) {
				new(ptr()) T(std::forward<Ts>(ts)...);
			}
			void dtor() noexcept {
				ptr()->~T();
			}
			#define NOEXCEPT_WHEN_RAW(t) noexcept(noexcept(IsRP<t>{}))
			template <class T2>
			Buffer& operator = (T2&& t) NOEXCEPT_WHEN_RAW(T2) {
				get() = std::forward<T2>(t);
				return *this;
			}
			#undef NOEXCEPT_WHEN_RAW
			template <class Ar, class T2>
			friend void serialize(Ar&, Buffer<T2>&);
		};
		template <class T>
		struct Buffer<T&> {
			T*	_data;

			Buffer() = default;
			Buffer(T& t) noexcept: _data(&t) {}
			T* ptr() noexcept {
				return _data;
			}
			const T* ptr() const noexcept {
				return _data;
			}
			T& get() noexcept {
				return *ptr();
			}
			const T& get() const noexcept {
				return *ptr();
			}
			void ctor() noexcept {}
			void ctor(T& t) noexcept{
				_data = &t;
			}
			void dtor() noexcept {}
			Buffer& operator = (T& t) noexcept {
				_data = &t;
				return *this;
			}
			template <class Ar, class T2>
			friend void serialize(Ar&, opt_tmp::Buffer<T2&>&);
		};
		template <class T>
		struct Buffer<T*> {
			T*	_data;

			Buffer() = default;
			Buffer(T* t) noexcept: _data(t) {}
			T* ptr() noexcept {
				return _data;
			}
			const T* ptr() const noexcept {
				return _data;
			}
			T*& get() noexcept {
				return _data;
			}
			T* const& get() const noexcept {
				return _data;
			}
			void ctor() noexcept {}
			void ctor(T* t) noexcept {
				_data = t;
			}
			void dtor() noexcept {}
			Buffer& operator = (T* t) noexcept {
				_data = t;
				return *this;
			}
			template <class Ar, class T2>
			friend void serialize(Ar&, opt_tmp::Buffer<T2*>&);
		};
	}

	template <class T>
	class Optional {
		private:
			using value_t = T;
			constexpr static bool Is_RP = IsRP<T>{};
			using Buffer = opt_tmp::Buffer<T>;
			Buffer	_buffer;
			bool	_bInit;

			void _force_release() noexcept {
				_buffer.dtor();
				_bInit = false;
			}
			void _release() noexcept {
				if(_bInit)
					_force_release();
			}
			value_t&& _takeOut() && noexcept {
				return std::move(get());
			}
			const value_t& _takeOut() const& noexcept {
				return get();
			}

		public:
			const static struct _AsInitialized{} AsInitialized;
			//! コンストラクタは呼ばないが初期化された物として扱う
			/*! ユーザーが自分でコンストラクタを呼ばないとエラーになる */
			Optional(_AsInitialized) noexcept: _bInit(true) {}

			template <class T2, ENABLE_IF(!(is_optional<std::decay_t<T2>>{}))>
			Optional(T2&& v) noexcept(noexcept(!IsRP<T2>{})):
				_buffer(std::forward<T2>(v)),
				_bInit(true)
			{}
			template <class T2, ENABLE_IF((is_optional<std::decay_t<T2>>{}))>
			Optional(T2&& v) noexcept(noexcept(!IsRP<T2>{})) :
				_bInit(v._bInit)
			{
				if(_bInit)
					_buffer.ctor(std::forward<T2>(v)._takeOut());
			}
			//! デフォルト初期化: 中身は無効　
			Optional() noexcept: _bInit(false) {}
			//! none_tを渡して初期化するとデフォルトと同じ挙動
			Optional(none_t) noexcept: Optional() {}
			~Optional() {
				_release();
			}
			decltype(auto) get() & noexcept {
				return _buffer.get();
			}
			decltype(auto) get() const& noexcept {
				return _buffer.get();
			}
			decltype(auto) get() && noexcept {
				return std::move(_buffer.get());
			}
			decltype(auto) operator * () & noexcept { return get(); }
			decltype(auto) operator * () const& noexcept { return get(); }
			decltype(auto) operator * () && noexcept { return std::move(get()); }

			explicit operator bool () const noexcept {
				return _bInit;
			}
			decltype(auto) operator -> () noexcept {
				return _buffer.ptr();
			}
			decltype(auto) operator -> () const noexcept {
				return _buffer.ptr();
			}
			bool operator == (const Optional& t) const noexcept {
				const bool b = _bInit;
				if(b == t._bInit) {
					if(b)
						return get() == t.get();
					return true;
				}
				return false;
			}
			bool operator != (const Optional& t) const noexcept {
				return !(this->operator == (t));
			}
			Optional& operator = (none_t) noexcept {
				_release();
				return *this;
			}
			template <class T2>
			bool construct(std::true_type, T2&& t) {
				_buffer.ctor(std::forward<T2>(t));
				return true;
			}
			template <class T2>
			bool construct(std::false_type, T2&&) {
				_buffer.ctor();
				return false;
			}
			template <class T2, ENABLE_IF(!(is_optional<std::decay_t<T2>>{}))>
			Optional& operator = (T2&& t) noexcept(Is_RP) {
				if(!_bInit) {
					_bInit = true;
					using CanConstruct = std::is_constructible<value_t, decltype(std::forward<T2>(t))>;
					if(construct(CanConstruct{}, std::forward<T2>(t)))
						return *this;
				}
				_buffer = std::forward<T2>(t);
				return *this;
			}
			template <class T2, ENABLE_IF((is_optional<std::decay_t<T2>>{}))>
			Optional& operator = (T2&& t) noexcept(Is_RP) {
				if(!t)
					_release();
				else {
					_bInit = true;
					_buffer = std::forward<T2>(t).get();
				}
				return *this;
			}
			template <class Ar, class T2>
			friend void serialize(Ar&, Optional<T2>&);
	};
	template <class T>
	const typename Optional<T>::_AsInitialized Optional<T>::AsInitialized{};
}
