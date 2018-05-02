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
				return _sp->get();
			}
			Temp ref() {
				return Temp(this);
			}
	};
	template <class T, class F_Hash, class F_Cmp>
	typename FlyweightItem<T, F_Hash, F_Cmp>::Set FlyweightItem<T, F_Hash, F_Cmp>::s_set;
}
