#pragma once
#include "flyweight.hpp"

namespace spi {
	template <class T, class F_Hash=std::hash<T>, class F_Cmp=std::equal_to<>>
	class FlyweightItem {
		private:
			using value_t = T;
			using self_t = FlyweightItem<value_t, F_Hash, F_Cmp>;
			struct Temp {
				self_t*		self;
				value_t		value;

				Temp(self_t* self):
					self(self),
					value(*self->_sp)
				{}
				Temp(const Temp&) = delete;
				Temp(Temp&& t):
					self(t.self),
					value(std::move(t.value))
				{
					t.self = nullptr;
				}
				~Temp() {
					if(self)
						self->_sp = s_set.make(std::move(value));
				}
				value_t* operator -> () noexcept {
					return &value;
				}
				value_t& operator * () noexcept {
					return value;
				}
			};
			using Set = Flyweight<value_t, F_Hash, F_Cmp>;
			using SP = typename Set::SP;
			static Set		s_set;
			SP	_sp;

		public:
			FlyweightItem():
				_sp(s_set.make(value_t{}))
			{}
			template <class V>
			FlyweightItem(V&& v):
				_sp(s_set.make(std::forward<V>(v)))
			{}
			const value_t& cref() const noexcept {
				return static_cast<const value_t&>(*_sp);
			}
			operator const value_t& () const noexcept {
				return cref();
			}
			const value_t& operator * () const noexcept {
				return cref();
			}
			const value_t* operator -> () const noexcept {
				return _sp.get();
			}
			Temp ref() {
				return Temp(this);
			}
			#define DEF_OP(op) bool operator op (const value_t& v) const noexcept { return cref() op v; }
			DEF_OP(==)
			DEF_OP(!=)
			DEF_OP(>)
			DEF_OP(>=)
			DEF_OP(<)
			DEF_OP(<=)
			#undef DEF_OP
	};
	template <class T, class F_Hash, class F_Cmp>
	typename FlyweightItem<T, F_Hash, F_Cmp>::Set FlyweightItem<T, F_Hash, F_Cmp>::s_set;

	namespace detail {
		template <class T>
		struct is_flyweightitem : std::false_type {};
		template <class T, class FH, class FC>
		struct is_flyweightitem<FlyweightItem<T,FH,FC>> : std::true_type {};
	}

	#define DEF_OP(op) \
		template <class Val, class T, class F_Hash, class F_Cmp, ENABLE_IF(!detail::is_flyweightitem<Val>{})> \
		bool operator op (const Val& val, const FlyweightItem<T, F_Hash, F_Cmp>& fw) noexcept { \
			return fw.cref() op val; \
		}
	DEF_OP(==)
	DEF_OP(!=)
	DEF_OP(>)
	DEF_OP(>=)
	DEF_OP(<)
	DEF_OP(<=)
	#undef DEF_OP
}
namespace std {
	template <class T, class F_Hash, class F_Cmp>
	struct hash<spi::FlyweightItem<T, F_Hash, F_Cmp>> {
		std::size_t operator()(const spi::FlyweightItem<T,F_Hash,F_Cmp>& f) const noexcept {
			auto* r = &f.cref();
			return std::hash<decltype(r)>()(r);
		}
	};
}
