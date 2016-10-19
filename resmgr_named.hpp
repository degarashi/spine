#pragma once
#include "singleton.hpp"
#include "restag.hpp"
#include "optional.hpp"
#include <cereal/types/unordered_map.hpp>

namespace spi {
	//! 名前付きリソースマネージャ
	/*!
		中身の保持はすべてスマートポインタで行う
		シングルスレッド動作
	*/
	template <class T, class K=std::string>
	class ResMgrName {
		public:
			using value_t = T;
			using shared_t = std::shared_ptr<value_t>;
			using key_t = K;
		private:
			using this_t = ResMgrName<T,K>;
			using tag_t = ResTag<value_t>;
			using Resource = std::unordered_map<key_t, tag_t>;
			using Val2Key = std::unordered_map<const value_t*, key_t>;
			Resource	_resource;
			Val2Key		_v2k;

			void _release(value_t* p) noexcept {
				const auto itr = _v2k.find(p);
				try {
					Assert0(itr != _v2k.end());
					const auto itr2 = _resource.find(itr->second);
					Assert0(itr2 != _resource.end());
					_v2k.erase(itr);
					_resource.erase(itr2);
					p->~value_t();
				} catch(...) {}
			}

			using DeleteF = std::function<void (value_t*)>;
			const DeleteF	_deleter;

		public:
			ResMgrName():
				_deleter([this](value_t* p){
					this->_release(p);
				})
			{}
			template <class Ar>
			void serialize(Ar& ar) {
				ar(_resource);
				//FIXME: _v2kの復元
			}
			template <class... Ts>
			auto acquire(const key_t& k, Ts&&... ts) {
				// 既に同じ名前でリソースを確保済みならばそれを返す
				if(auto ret = get(k))
					return ret;
				// 新しくリソースを作成
				shared_t p(new value_t(std::forward<Ts>(ts)...), _deleter);
				_resource.emplace(k, p);
				_v2k.emplace(p.get(), k);
				return p;
			}
			Optional<const key_t&> getKey(const shared_t& p) const {
				return getKey(p.get());
			}
			Optional<const key_t&> getKey(const value_t* p) const {
				const auto itr = _v2k.find(p);
				if(itr != _v2k.end())
					return itr->second;
				return none;
			}
			shared_t get(const key_t& k) {
				const auto itr = _resource.find(k);
				if(itr != _resource.end()) {
					shared_t ret(itr->second.weak.lock());
					D_Assert0(ret);
					return ret;
				}
				return nullptr;
			}
			std::size_t size() const noexcept {
				return _resource.size();
			}
	};
}
