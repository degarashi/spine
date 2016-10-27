#pragma once
#include "optional.hpp"
#include <deque>
#include <algorithm>

namespace spi {
	namespace test {
		class PQueueTest;
	}
	struct InsertAfter {
		template <class T, class Pred>
		bool operator()(const T& t0, const T& t1, Pred p) const {
			return p(t0, t1);
		}
	};
	struct InsertBefore {
		template <class T, class Pred>
		bool operator()(const T& t0, const T& t1, Pred p) const {
			return !p(t1, t0);
		}
	};
	//! 優先度付きキュー
	template <class T, template<class,class> class Container=std::deque, class Pred=std::less<T>, class Insert=InsertAfter>
	class pqueue : private Container<T, std::allocator<T>> {
		using base_type = Container<T, std::allocator<T>>;
		friend class spi::test::PQueueTest;
		template <class TA>
		void _push(TA&& t, std::bidirectional_iterator_tag) {
			Pred pred;
			Insert ins;
			// 線形探索
			auto itr = base_type::begin();
			while(itr != base_type::end()) {
				if(ins(t, *itr, pred))
					break;
				++itr;
			}
			base_type::emplace(itr, std::forward<TA>(t));
		}
		template <class TA>
		void _push(TA&& t, std::random_access_iterator_tag) {
			Pred pred;
			Insert ins;
			// 2分探索
			auto itrA = base_type::begin(),
				itrC = base_type::end();
			while(itrA < itrC) {
				auto itrB = itrA + (itrC - itrA)/2;
				if(ins(t, *itrB, pred))
					itrC = itrB;
				else
					itrA = itrB+1;
			}
			base_type::emplace(itrA, std::forward<TA>(t));
		}
		public:
			using base_type::base_type;
			template <class TA>
			void push(TA&& t) {
				_push(std::forward<TA>(t), typename std::iterator_traits<typename base_type::iterator>::iterator_category());
			}
			// 優先順位が変わってしまうかもしれないので参照はconstのみとする
			const T& back() const {
				return base_type::back();
			}
			const T& front() const {
				return base_type::front();
			}
			void pop_front() {
				base_type::erase(base_type::begin());
			}
			void pop_front(T& dst) {
				dst = std::move(const_cast<T&>(base_type::front()));
				pop_front();
			}
			void pop_back() {
				auto itr = base_type::end();
				erase(--itr);
			}
			void pop_back(T& dst) {
				dst = std::move(const_cast<T&>(base_type::back()));
				pop_back();
			}
			typename base_type::const_iterator erase(const T& obj) {
				const auto itr = std::find(base_type::begin(), base_type::end(), obj);
				if(itr != base_type::end())
					return base_type::erase(itr);
				return base_type::end();
			}
			bool empty() const noexcept {
				return base_type::empty();
			}
			std::size_t size() const noexcept {
				return base_type::size();
			}
			using base_type::cbegin;
			using base_type::cend;
			using base_type::clear;
	};
}
