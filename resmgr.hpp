#pragma once
#include "restag.hpp"
#include "singleton.hpp"
#include <cereal/types/unordered_set.hpp>

namespace spi {
	//! 名前無しリソースマネージャ
	/*!
		中身の保持はすべてスマートポインタで行う
		シングルスレッド動作
	*/
	template <class T>
	class ResMgr : public Singleton<ResMgr<T>> {
		public:
			template <class M>
			friend struct ResDeleter;
			using value_t = T;
			using shared_t = std::shared_ptr<value_t>;
		private:
			using this_t = ResMgr<T>;
			using tag_t = ResTag<value_t>;
			using Resource = std::unordered_set<tag_t>;
			Resource	_resource;

			void _release(value_t* p) noexcept {
				const auto itr = _resource.find(p);
				try {
					Assert0(itr != _resource.end());
					_resource.erase(itr);
					p->~value_t();
				} catch(...) {}
			}

		public:
			template <class Ar>
			void serialize(Ar& ar) {
				ar(_resource);
			}
			template <class... Ts>
			auto acquire(Ts&&... ts) {
				shared_t p(new value_t(std::forward<Ts>(ts)...), ResDeleter<this_t>());
				// リソース管理のためのリスト登録
				_resource.emplace(p.get());
				return p;
			}
			std::size_t size() const noexcept {
				return _resource.size();
			}
	};
}
