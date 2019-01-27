//! リアルタイムプロファイラ
#pragma once
#include "treenode.hpp"
#include "rflag.hpp"
#include "prof_clock.hpp"
#include "lubee/src/dataswitch.hpp"
#include <mutex>

#ifdef PROFILER_ENABLED
	#define SpiBeginProfile(name)	::spi::profiler.beginBlock(#name)
	#define	SpiEndProfile(name)		::spi::profiler.endBlock(#name)
	#define SpiProfile(name)		const auto name##__LINE__ = ::spi::profiler(#name)
#else
	#define SpiBeginProfile(name)
	#define	SpiEndProfile(name)
	#define SpiProfile(name)
#endif

namespace spi {
	namespace prof {
		struct Parameter {
			//!< 履歴を更新する間隔
			#define SEQ \
				((Interval)(Duration)) \
				((Accum)(uint32_t)(Interval))
			RFLAG_DEFINE(Parameter, SEQ)
			RFLAG_GETMETHOD_DEFINE(SEQ)
			RFLAG_SETMETHOD_DEFINE(SEQ)
			RFLAG_SETMETHOD(Accum)
			#undef SEQ

			inline const static Duration DefaultInterval = Seconds(1);
			Parameter() {
				setInterval(DefaultInterval);
				setAccum(0);
			}
		};
		inline bool Parameter::_refresh(Accum::value_t& a, Accum*) const {
			getInterval();
			++a;
			return true;
		}
		namespace detail {
			inline Parameter		g_param;
			inline std::mutex		g_mutex;
		}
		inline void SetInterval(const Duration dur) {
			std::lock_guard lk(detail::g_mutex);
			detail::g_param.setInterval(dur);
		}

		using Name = const char*;
		struct History {
			uint_fast32_t		nCalled;	//!< 呼び出し回数
			Duration			tMax,		//!< 最高値
								tMin,		//!< 最低値
								tAccum;		//!< 累積値
			History():
				nCalled(0),
				tMax(std::numeric_limits<Duration::rep>::lowest()),
				tMin(std::numeric_limits<Duration::rep>::max()),
				tAccum(0)
			{}
			void addTime(const Duration t) {
				++nCalled;
				tMax = std::max(tMax, t);
				tMin = std::min(tMin, t);
				tAccum += t;
			}
			Duration getAverageTime() const {
				return tAccum / nCalled;
			}
		};
		//! プロファイラブロック
		struct Block : spi::TreeNode<Block> {
			Name			name;				//!< ブロック名
			History			hist;

			Block(const Name& name):
				name(name)
			{}
			//! 下層にかかった時間
			Duration getLowerTime() const {
				Duration sum(0);
				iterateChild([&sum](const auto& nd){
					sum += nd->hist.tAccum;
					return true;
				});
				return sum;
			}
			Duration getAverageTime(const bool omitLower) const {
				if(omitLower)
					return (hist.tAccum - getLowerTime()) / hist.nCalled;
				return hist.getAverageTime();
			}
		};
		using BlockSP = std::shared_ptr<Block>;
		class Profiler;
	}
	extern inline thread_local prof::Profiler profiler;
	namespace prof {
		class Profiler {
			public:
				//! 1インターバル間に集計される情報
				struct IntervalInfo {
					using ByName = std::unordered_map<Name, History>;

					Timepoint	tmBegin;			//!< インターバル開始時刻
					BlockSP		root;				//!< ツリー構造ルート
					ByName		byName;				//!< 名前ブロック毎の集計(最大レイヤー数を超えた分も含める)
				};
			private:
				using IntervalInfoSW = lubee::DataSwitcher<IntervalInfo>;
				IntervalInfoSW	_intervalInfo;

				using TPStk = std::vector<Timepoint>;
				Name			_rootName;
				uint32_t		_param_accum;
				Block*			_curBlock;			//!< 現在計測中のブロック
				std::size_t		_maxLayer;			//!< 最大ツリー深度
				TPStk			_tmBegin;			//!< レイヤー毎の計測開始した時刻
				//! 閉じられていないブロックを閉じる(ルートノード以外)
				void _closeAllBlocks() {
					auto* cur = _curBlock;
					while(cur) {
						endBlock(cur->name);
						cur = cur->getParent().get();
					}
					_curBlock = nullptr;
				}
				void _prepareRootNode() {
					beginBlock(_rootName);
				}

				//! スコープを抜けると同時にブロックを閉じる
				class Scope {
					private:
						bool	_bValid;
						Name	_name;
						friend class Profiler;
						Scope(const Name& name):
							_bValid(true),
							_name(name)
						{}
					public:
						Scope(Scope&& b):
							_bValid(b._bValid),
							_name(std::move(b._name))
						{
							b._bValid = false;
						}
						~Scope() {
							if(_bValid)
								profiler.endBlock(_name);
						}
				};
			public:
				inline const static Name DefaultRootName = "Root";
				inline const static std::size_t DefaultMaxLayer = 8;

				Profiler() {
					initialize();
				}
				//! 任意のルートノード名とレイヤー数で再初期化
				void initialize(
					const Name& rootName = DefaultRootName,
					const std::size_t ml=DefaultMaxLayer
				) {
					{
						std::lock_guard lk(detail::g_mutex);
						_param_accum = detail::g_param.getAccum();
					}
					_rootName = rootName;
					// 計測中だった場合は警告を出す
					Expect(!accumulating(), "reinitializing when accumulating");
					Assert(ml > 0, "invalid maxLayer");
					_maxLayer = ml;
					clear();
				}
				//! 何かしらのブロックを開いているか = 計測途中か
				bool accumulating() const {
					// ルートノードの分を加味
					return _tmBegin.size() > 1;
				}
				//! フレーム間の区切りに呼ぶ
				/*! 必要に応じてインターバル切り替え */
				bool checkIntervalSwitch() {
					// ルート以外のブロックを全て閉じてない場合は警告を出す
					Expect(!accumulating(), "profiler block leaked");
					bool b;
					{
						std::lock_guard lk(detail::g_mutex);
						// インターバル時間が変更されていたら履歴をクリア
						const auto ac = detail::g_param.getAccum();
						if(ac != _param_accum) {
							_param_accum = ac;
							clear();
							return false;
						}
						b = (Clock::now() - getCurrent().tmBegin) >= detail::g_param.getInterval();
					}
					_closeAllBlocks();
					if(b) {
						_intervalInfo.advance_clear();
						_intervalInfo.current().tmBegin = Clock::now();
						_curBlock = nullptr;
					}
					_prepareRootNode();
					return b;
				}
				void clear() {
					_closeAllBlocks();
					_intervalInfo.clear();
					_intervalInfo.current().tmBegin = Clock::now();
					_curBlock = nullptr;
					_prepareRootNode();
				}
				void beginBlock(const Name& name) {
					// 最大レイヤー深度を超えた分のブロック累積処理はしない
					const auto n = _tmBegin.size();
					auto& ci = _intervalInfo.current();
					if(n < _maxLayer) {
						// 子ノードに同じ名前のノードが無いか確認
						auto& cur = _curBlock;
						Block::SP node;
						if(cur) {
							cur->iterateChild([&node, &name](auto& nd){
								if(nd->name == name) {
									node = nd->shared_from_this();
									return false;
								}
								return true;
							});
						}
						if(!node) {
							Assert0(ci.root || n==0);
							if(!cur && ci.root && ci.root->name == name)
								cur = ci.root.get();
							else {
								// 新たにノードを作成
								const auto blk = std::make_shared<Block>(name);
								if(cur) {
									// 現在ノードの子に追加
									cur->addChild(blk);
								} else {
									// 新たにルートノードとして設定
									ci.root = blk;
								}
								cur = blk.get();
							}
						} else {
							cur = node.get();
						}
					}
					// エントリを確保
					ci.byName[name];
					// 現在のレイヤーの開始時刻を記録
					_tmBegin.emplace_back(Clock::now());
				}
				void endBlock(const Name& name) {
					// endBlockの呼び過ぎはエラー
					Assert0(!_tmBegin.empty());
					auto& cur = _curBlock;

					// かかった時間を加算
					const Duration dur = Clock::now() - _tmBegin.back();
					{
						const auto n = _tmBegin.size();
						if(n <= _maxLayer) {
							cur->hist.addTime(dur);
							// ネストコールが崩れた時にエラーを出す
							Assert0(name==cur->name);
							// ポインタを親に移す
							cur = cur->getParent().get();
						}
					}
					auto& ci = _intervalInfo.current();
					Assert0(ci.byName.count(name)==1);
					ci.byName.at(name).addTime(dur);
					_tmBegin.pop_back();
				}
				Scope beginScope(const Name& name) {
					beginBlock(name);
					return Scope(name);
				}
				Scope operator()(const Name& name) {
					return beginScope(name);
				}
				//! 同じ名前のブロックを合算したものを取得(前のインターバル)
				const IntervalInfo& getPrev() const {
					return _intervalInfo.prev();
				}
				const IntervalInfo& getCurrent() const {
					return _intervalInfo.current();
				}
		};
	}
	// スレッド毎に集計(=結果をスレッド毎に取り出す)
	inline thread_local prof::Profiler profiler;
}
