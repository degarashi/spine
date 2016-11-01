#pragma once
#include <vector>
#include <memory>
#include <cassert>
#include "lubee/meta/enable_if.hpp"
#include "lubee/error.hpp"
#include <cstring>

#ifdef OBJECT_POOL_CHECKBLOCK
	#define CallCheckBlock() checkBlock()
#else
	#define CallCheckBlock()
#endif

namespace spi {
	template <std::size_t I0, std::size_t I1>
	struct Min {
		constexpr operator std::size_t() const {
			return (I0<I1) ? I0 : I1;
		}
	};
	template <std::size_t I0, std::size_t I1>
	struct Max {
		constexpr operator std::size_t() const {
			return (I0>I1) ? I0 : I1;
		}
	};
	constexpr std::size_t CalcBlockSize(const std::size_t s, const std::size_t bs) {
		return (s + (bs-1)) / bs * bs;
	}
	inline std::size_t CalcOffsetSize(const std::size_t align, const std::size_t header, const void* ptr) {
		const auto iptr = reinterpret_cast<uintptr_t>(ptr);
		return static_cast<std::size_t>((iptr + header + (align-1)) / align * align) - iptr;
	}
	//! 入力値の一番左のビットだけを残す
	template <class T, ENABLE_IF(std::is_unsigned<T>{})>
	inline  T LowClear(T x) {
		x = x | (x >> 1);
		x = x | (x >> 2);
		x = x | (x >> 4);
		x = x | (x >> 8);
		x = x | (x >>16);
		return x & ~(x>>1);
	}
	//! most significant bit を取得
	/*! もし入力値が0の場合は0を返す */
	inline uint32_t MSB(uint32_t x) {
		const char NLRZ_TABLE[33] = "\x1f\x00\x01\x0e\x1c\x02\x16\x0f\x1d\x1a\x03\x05\x0b\x17"
									"\x07\x10\x1e\x0d\x1b\x15\x19\x04\x0a\x06\x0c\x14\x18\x09"
									"\x13\x08\x12\x11";
		x |= 0x01;
		return NLRZ_TABLE[0x0aec7cd2U * LowClear(x) >> 27];
	}
	//! 固定サイズブロックメモリアロケータ
	template <class T>
	class ObjectPool {
		private:
			class Slot {
				private:
					using Vector = std::vector<uint8_t>;
					Vector			_vec;
					std::size_t		_size,
									_used,
									_alignMod;
					using Id = uint32_t;
					using IVector = std::vector<Id>;
					IVector			_freeId;
					static uint32_t _GetIndexOut(uint32_t m) {
						D_Assert0(m > 0);
						if(m==1)
							return 0;
						return MSB(m-1)+1;
					}
					static uint32_t _GetIndexIn(uint32_t m) {
						D_Assert0(m > 0);
						if(m==1)
							return 0;
						return MSB(m);
					}

					using Canary = uint32_t;
					using Size_t = uint32_t;
					constexpr static Id InvalidId = ~0;
					#ifdef DEBUG
						static Canary s_canary;
					#endif
					#pragma pack(push, 1)
					struct EmptyBlock {
						Id		prevId,
								nextId;
					};
					struct Footer {
						#ifdef DEBUG
							Canary	canary;
						#endif
						Size_t	size;
					};
					struct Header {
						bool	bUse;
						Size_t	size;
						#ifdef DEBUG
							Canary	canary;
							void checkCanary() const {
								auto* ft = getFooter();
								D_Assert0(canary == ft->canary);
							}
							void setCanary(const Canary c) {
								canary = getFooter()->canary = c;
							}
							void checkAndSetCanary(const Canary c) {
								checkCanary();
								setCanary(c);
							}
						#endif
						void setSize(const Size_t& s) noexcept {
							size = s;
							getFooter()->size = s;
						}
						void setNeighbor(const Id prev, const Id next) NOEXCEPT_IF_RELEASE {
							D_Assert0(!bUse);
							auto* eb = getEmptyBlock();
							eb->prevId = prev;
							eb->nextId = next;
						}
						T* getDataArea() const noexcept {
							return reinterpret_cast<T*>(uintptr_t(this) + sizeof(*this));
						}
						EmptyBlock* getEmptyBlock() const noexcept {
							return reinterpret_cast<EmptyBlock*>(uintptr_t(this) + sizeof(*this));
						}
						Footer* getFooter() const noexcept {
							return reinterpret_cast<Footer*>(uintptr_t(this) + BlockSize * size - sizeof(Footer));
						}
						Header* getNextHeader() const noexcept {
							return reinterpret_cast<Header*>(uintptr_t(this) + this->size*BlockSize);
						}
						Footer* getPrevFooter() const noexcept {
							return reinterpret_cast<Footer*>(uintptr_t(this) - sizeof(Footer));
						}
						Header* getPrevHeader() const noexcept {
							const auto* pf = getPrevFooter();
							return reinterpret_cast<Header*>(uintptr_t(this) - pf->size*BlockSize);
						}
						Header* getNextBlock(const intptr_t n) const noexcept {
							return reinterpret_cast<Header*>(intptr_t(this) + intptr_t(BlockSize)*n);
						}
					};
					#pragma pack(pop)

					constexpr static std::size_t
						Size = Max<sizeof(EmptyBlock), sizeof(T)>{},
						BlockSize = CalcBlockSize(Size+sizeof(Header)+sizeof(Footer), alignof(T));
					uintptr_t _blockBegin() const noexcept {
						return uintptr_t(_vec.data()) + _alignMod - sizeof(Header);
					}
					Header* _headerAt(const Id s) noexcept {
						return reinterpret_cast<Header*>(_blockBegin() + s*BlockSize);
					}
					const Header* _headerAt(const Id s) const noexcept {
						return reinterpret_cast<const Header*>(_blockBegin() + s*BlockSize);
					}
					Header* _headerEnd() noexcept {
						return _headerAt(_size);
					}
					const Header* _headerEnd() const noexcept {
						return _headerAt(_size);
					}
					static Header* _ToHeader(T* p) noexcept {
						return reinterpret_cast<Header*>(uintptr_t(p) - sizeof(Header));
					}
					Id _idFromHeader(const Header* hdr) const noexcept {
						const auto diff = uintptr_t(hdr) - _blockBegin();
						D_Assert0(diff % BlockSize == 0);
						return diff / BlockSize;
					}
					void _registerBlock(Header* h) NOEXCEPT_IF_RELEASE {
						D_Assert0(_used >= h->size);
						_used -= h->size;
						D_Assert0(h->bUse);
						h->bUse = false;
						#ifdef DEBUG
							h->checkAndSetCanary(++s_canary);
						#endif
						const Id id = _idFromHeader(h);
						auto& freeId = _freeId[_GetIndexIn(h->size)];
						h->setNeighbor(InvalidId, freeId);
						if(freeId != InvalidId) {
							auto* fh = _headerAt(freeId);
							D_Assert0(!fh->bUse);
							auto* feb = fh->getEmptyBlock();
							D_Assert0(feb->prevId == InvalidId);
							feb->prevId = id;
						}
						freeId = id;
						CallCheckBlock();
					}
					void _unregisterBlock(Header* h) NOEXCEPT_IF_RELEASE {
						CallCheckBlock();
						_used += h->size;
						D_Assert0(!h->bUse);
						const auto* eb = h->getEmptyBlock();
						if(eb->nextId != InvalidId) {
							auto* nhdr = _headerAt(eb->nextId);
							D_Assert0(!nhdr->bUse);
							auto* neb = nhdr->getEmptyBlock();
							D_Assert0(neb->prevId == _idFromHeader(h));
							neb->prevId = eb->prevId;
						}
						if(eb->prevId != InvalidId) {
							auto* phdr = _headerAt(eb->prevId);
							D_Assert0(!phdr->bUse);
							auto* peb = phdr->getEmptyBlock();
							D_Assert0(peb->nextId == _idFromHeader(h));
							peb->nextId = eb->nextId;
						} else {
							auto& fid = _freeId[_GetIndexIn(h->size)];
							D_Assert0(fid == _idFromHeader(h));
							fid = eb->nextId;
						}
						h->bUse = true;
						#ifdef DEBUG
							h->checkAndSetCanary(++s_canary);
						#endif
						CallCheckBlock();
					}
					T* _acquireFrom(Header* h, const std::size_t s) NOEXCEPT_IF_RELEASE {
						_unregisterBlock(h);
						D_Assert0(h->size >= s);
						const auto remain = h->size - s;
						if(remain > 0) {
							h->setSize(s);
							// 残りを再登録
							auto* nhdr = h->getNextBlock(s);
							nhdr->bUse = true;
							nhdr->setSize(remain);
							#ifdef DEBUG
								nhdr->setCanary(++s_canary);
							#endif
							_registerBlock(nhdr);
						}
						CallCheckBlock();
						return h->getDataArea();
					}

				public:
					Slot(const Slot&) = delete;
					Slot(Slot&& s):
						_vec(std::move(s._vec)),
						_size(s._size),
						_used(s._used),
						_alignMod(s._alignMod),
						_freeId(std::move(s._freeId))
					{
						// ムーブ元データブロックを初期化しておく
						s.clear(false);
						CallCheckBlock();
					}
					void clear(const bool dtor) {
						if(dtor) {
							// 全てのブロックを巡回してデストラクタを呼ぶ
							auto *hdr = _headerAt(0),
								*end = _headerEnd();
							while(hdr != end) {
								if(hdr->bUse) {
									auto* ptr = hdr->getDataArea();
									for(int i=0 ; i<int(hdr->size) ; i++)
										(ptr++)->~T();
								}
								hdr = hdr->getNextHeader();
							}
						}
						_used = 0;
						if(!_freeId.empty()) {
							std::fill(_freeId.begin(), _freeId.end(), InvalidId);
							_freeId[_GetIndexIn(_size)] = 0;
							auto* h = _headerAt(0);
							h->bUse = false;
							h->setSize(_size);
							h->setNeighbor(InvalidId, InvalidId);
							#ifdef DEBUG
								h->setCanary(++s_canary);
							#endif
							CallCheckBlock();
						}
					}
					Slot(const std::size_t s):
						_vec(s * BlockSize + alignof(T)-1),
						_size(s),
						_alignMod(CalcOffsetSize(alignof(T), sizeof(Header), _vec.data())),
						_freeId(_GetIndexIn(s)+1)
					{
						#ifdef DEBUG
							memset(_vec.data(), 0xfd, _vec.size());
						#endif
						clear(false);
						CallCheckBlock();
					}
					#ifdef OBJECT_POOL_CHECKBLOCK
						void checkBlock() const {
							auto* ptr = _headerAt(0),
								* end = _headerEnd();
							auto size = _size;
							while(ptr < end) {
								Assert0(size >= ptr->size);
								if(!ptr->bUse) {
									Id thisId = _idFromHeader(ptr);
									Id id = _freeId[_GetIndexIn(ptr->size)],
									   prevId = InvalidId;
									for(;;) {
										if(id == thisId)
											break;
										Assert0(id != InvalidId);
										auto* eb = _headerAt(id)->getEmptyBlock();
										Assert0(eb->prevId == prevId);
										prevId = id;
										id = eb->nextId;
									}
								}
								size -= ptr->size;
								Assert0(ptr->size != 0 && ptr->size <= _size);
								const auto* ft = ptr->getFooter();
								Assert0(ft->size == ptr->size);
								Assert0(ptr->getNextBlock(ptr->size) == ptr->getNextHeader());
								ptr = ptr->getNextBlock(ptr->size);
							}
							Assert0(ptr == end);

							for(int i=0 ; i<int(_freeId.size()) ; i++) {
								const Size_t smin = 1<<i,
											smax = smin<<1;
								Id id = _freeId[i];
								while(id != InvalidId) {
									const auto* ptr = _headerAt(id);
									Assert0(ptr->size >= smin &&
											ptr->size < smax &&
											!ptr->bUse);
									id = ptr->getEmptyBlock()->nextId;
								}
							}
						}
					#endif
					auto getSize() const noexcept {
						return _size;
					}
					bool hasMemory(T* t) const noexcept {
						return uintptr_t(_headerAt(0)) <= uintptr_t(t) &&
								uintptr_t(t) < uintptr_t(_headerEnd());
					}
					std::size_t remainingBlock() const noexcept {
						return _size - _used;
					}
					std::size_t allocatingBlock() const noexcept {
						return _used;
					}
					T* acquireBlock(const std::size_t s) NOEXCEPT_IF_RELEASE {
						CallCheckBlock();
						D_Assert0(s>0);

						auto cur = _GetIndexOut(s);
						if(decltype(cur)(_freeId.size()) <= cur)
							return nullptr;
						if(_freeId[cur] != InvalidId) {
							auto* hdr = _headerAt(_freeId[cur]);
							return _acquireFrom(hdr, s);
						}
						do {
							auto& id = _freeId[cur];
							if(id != InvalidId) {
								auto* hdr = _headerAt(id);
								D_Assert0(hdr->size >= s);
								return _acquireFrom(hdr, s);
							}
							++cur;
						} while(cur < _freeId.size());
						return nullptr;
					}
					void putBlock(T* p) NOEXCEPT_IF_RELEASE {
						D_Assert0(p);
						auto* hdr = _ToHeader(p);
						D_Assert0(hasMemory(p) && hdr->size>0);
						// 二重解放についてはRelease時にもチェックし、失敗したらクラッシュさせる
						Assert0(hdr->bUse);

						{
							auto* ptr = hdr->getDataArea();
							for(int i=0 ; i<int(hdr->size) ; i++)
								(ptr++)->~T();
						}

						std::size_t nfree = hdr->size;
						// 前方にある空きブロックと結合
						auto const
							*const	beg = _headerAt(0),
							*const	end = _headerEnd();
						auto* phdr = hdr;
						while(phdr != beg) {
							// 次のブロックを探索
							phdr = phdr->getPrevHeader();
							if(phdr->bUse)
								break;
							hdr = phdr;
							nfree += phdr->size;
							// 登録を解除
							_unregisterBlock(phdr);
						}
						// 後方にある空きブロックと結合
						auto* nhdr = hdr->getNextHeader();
						while(nhdr < end) {
							if(nhdr->bUse)
								break;
							nfree += nhdr->size;
							// 登録を解除
							_unregisterBlock(nhdr);
							// 次のブロックを探索
							nhdr = nhdr->getNextHeader();
						}
						// 新しく出来た結合済みの空きブロックを登録
						hdr->setSize(nfree);
						#ifdef DEBUG
							hdr->setCanary(++s_canary);
						#endif
						_registerBlock(hdr);
						CallCheckBlock();
					}
			};
			using SlotV = std::vector<Slot>;
			SlotV		_slot;
		public:
			constexpr static std::size_t DefaultSize = 8;
			ObjectPool(const std::size_t s=DefaultSize) {
				_slot.emplace_back(s);
			}
			ObjectPool(const ObjectPool&) = delete;
			ObjectPool(ObjectPool&&) = default;
			template <class... Args>
			T* allocate(Args&&... args) {
				for(auto& s : _slot) {
					if(T* mem = s.acquireBlock(1)) {
						new(mem) T(std::forward<Args>(args)...);
						return mem;
					}
				}
				_slot.emplace_back(_slot.back().getSize() * 2);
				return allocate(std::forward<Args>(args)...);
			}
			template <class T2=T, ENABLE_IF(std::is_default_constructible<T2>{})>
			T* allocateArray(const std::size_t s) {
				if(s == 0)
					return nullptr;
				for(auto& sl : _slot) {
					if(T* mem = sl.acquireBlock(s)) {
						return new(mem) T[s];
					}
				}
				// メモリが断片化していて連続した領域が無い時はシンプルに新しく配列を確保する
				_slot.emplace_back(_slot.back().getSize() * 2);
				return allocateArray(s);
			}
			void clear(const bool shrink, const bool dtor=true) NOEXCEPT_IF_RELEASE {
				// 全てのオブジェクトのデストラクタを呼ぶ
				for(auto& s : _slot)
					s.clear(dtor);
				if(shrink) {
					// 確保した領域を解放
					const auto initSize = _slot.front().getSize();
					_slot.clear();
					_slot.shrink_to_fit();
					_slot.emplace_back(initSize);
				}
			}
			//! メモリの追加なしに確保可能なブロック数
			std::size_t remainingBlock() const noexcept {
				std::size_t sum = 0;
				for(auto& s : _slot)
					sum += s.remainingBlock();
				return sum;
			}
			//! 現在確保されているブロックの総数
			std::size_t allocatingBlock() const noexcept {
				std::size_t sum = 0;
				for(auto& s : _slot)
					sum += s.allocatingBlock();
				return sum;
			}
			//! 個別にブロックを解放
			void destroy(T* p) NOEXCEPT_IF_RELEASE {
				if(!p)
					return;
				// どのスロットのメモリか特定
				for(auto& s : _slot) {
					if(s.hasMemory(p)) {
						s.putBlock(p);
						return;
					}
				}
				D_Assert0(false);
			}
	};
	template <class T>
	constexpr typename ObjectPool<T>::Slot::Id ObjectPool<T>::Slot::InvalidId;
	#ifdef DEBUG
		template <class T>
		typename ObjectPool<T>::Slot::Canary ObjectPool<T>::Slot::s_canary(0);
	#endif
}
