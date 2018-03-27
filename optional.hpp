#pragma once
#include "detect_type.hpp"
#include "lubee/meta/enable_if.hpp"
#include "lubee/none.hpp"
#include "argholder.hpp"
#include <utility>
#include "none.hpp"

namespace spi {
	template <class... Ts>
	auto construct(Ts&&... ts) {
		return ArgHolder<decltype(ts)...>(std::forward<Ts>(ts)...);
	}
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
			void ctor(Ts&&... ts) noexcept(noexcept(new T(std::forward<Ts>(ts)...))) {
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
			template <class T2>
			friend class Optional;
			using value_t = T;
			constexpr static bool Is_RP = IsRP<T>{};
			using Buffer = opt_tmp::Buffer<T>;
			Buffer	_buffer;
			bool	_bInit;

			void _force_release() noexcept {
				D_Assert0(_bInit);
				_buffer.dtor();
				_bInit = false;
			}
			void _release() noexcept {
				if(_bInit)
					_force_release();
			}
			template <class T2>
			bool _construct(std::true_type, T2&& t) {
				_buffer.ctor(std::forward<T2>(t));
				return true;
			}
			template <class T2>
			bool _construct(std::false_type, T2&&) {
				_buffer.ctor();
				return false;
			}
			void _invalidate_if_rvalue() const& noexcept {}
			void _invalidate_if_rvalue() && noexcept {
				_bInit = false;
			}

		public:
			const static struct _AsInitialized{} AsInitialized;
			//! コンストラクタは呼ばないが初期化された物として扱う
			/*! ユーザーが自分でコンストラクタを呼ばないとエラーになる */
			Optional(_AsInitialized) noexcept:
				_bInit(true)
			{}

			Optional(const Optional& opt):
				_bInit(opt._bInit)
			{
				if(_bInit) {
					_buffer.ctor(opt._buffer.get());
				}
			}
			Optional(Optional&& opt) noexcept:
				_bInit(opt._bInit)
			{
				if(_bInit) {
					_buffer.ctor(std::move(opt).get());
					opt._bInit = false;
				}
			}
			template <class T2, ENABLE_IF(!(is_optional<std::decay_t<T2>>{}))>
			Optional(T2&& v) noexcept(noexcept(Buffer(std::forward<T2>(v)))):
				_buffer(std::forward<T2>(v)),
				_bInit(true)
			{}
			template <class T2, ENABLE_IF((is_optional<std::decay_t<T2>>{}))>
			Optional(T2&& v) noexcept(noexcept(std::declval<Buffer>().ctor(std::forward<T2>(v).get()))):
				_bInit(v._bInit)
			{
				if(_bInit) {
					_buffer.ctor(std::forward<T2>(v).get());
					v._invalidate_if_rvalue();
				}
			}
			//! デフォルト初期化: 中身は無効　
			Optional() noexcept:
				_bInit(false)
			{}
			//! none_tを渡して初期化するとデフォルトと同じ挙動
			Optional(none_t) noexcept:
				Optional()
			{}
			~Optional() {
				_release();
			}
			auto get() & noexcept -> decltype(_buffer.get()) {
				return _buffer.get();
			}
			auto get() const& noexcept -> decltype(_buffer.get()) {
				return _buffer.get();
			}
			auto get() && noexcept -> decltype(std::move(_buffer.get())) {
				return std::move(_buffer.get());
			}
			auto operator * () & noexcept -> decltype(get()) {
				return get();
			}
			auto operator * () const& noexcept -> decltype(get()) {
				return get();
			}
			auto operator * () && noexcept -> decltype(std::move(get())) {
				return std::move(get());
			}
			explicit operator bool () const noexcept {
				return _bInit;
			}
			auto operator -> () noexcept -> decltype(_buffer.ptr()) {
				return _buffer.ptr();
			}
			auto operator -> () const noexcept -> decltype(_buffer.ptr()) {
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
			template <
				class T2,
				ENABLE_IF(!(is_optional<std::decay_t<T2>>{}))
			>
			Optional& operator = (T2&& t) {
				if(!_bInit) {
					_bInit = true;
					using CanConstruct = std::is_constructible<value_t, decltype(std::forward<T2>(t))>;
					if(_construct(CanConstruct{}, std::forward<T2>(t)))
						return *this;
				}
				_buffer = std::forward<T2>(t);
				return *this;
			}
			template <class... Ts>
			Optional& operator = (ArgHolder<Ts...>&& ah) {
				if(_bInit)
					_release();
				const auto fn = [&buff = _buffer](auto&&... ts) {
					buff.ctor(std::forward<decltype(ts)>(ts)...);
				};
				ah.inorder(fn);
				_bInit = true;
				return *this;
			}
			template <class T2, ENABLE_IF((is_optional<std::decay_t<T2>>{}))>
			Optional& _assign_operator(T2&& t) noexcept(Is_RP) {
				if(!t)
					_release();
				else {
					if(!_bInit) {
						_bInit = true;
						_buffer.ctor();
					}
					_buffer = std::forward<T2>(t).get();
				}
				return *this;
			}
			template <class T2, ENABLE_IF((is_optional<std::decay_t<T2>>{}))>
			Optional& operator = (T2&& t) noexcept(Is_RP) {
				return _assign_operator(std::forward<T2>(t));
			}
			Optional& operator = (const Optional& opt) {
				return _assign_operator(opt);
			}
			Optional& operator = (Optional&& opt) noexcept {
				return _assign_operator(std::move(opt));
			}
			template <class Ar, class T2>
			friend void load(Ar&, Optional<T2>&);
			template <class Ar, class T2>
			friend void save(Ar&, const Optional<T2>&);
	};
	template <class T>
	std::ostream& operator << (std::ostream& os, const Optional<T>& opt) {
		os << "optional{";
		if(opt)
			os << *opt;
		else
			os << "none";
		return os << "}";
	}
	template <class T>
	const typename Optional<T>::_AsInitialized Optional<T>::AsInitialized{};
}
