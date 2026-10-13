#pragma once
#include "PitchEngine.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace pitchator {
constexpr uint32_t kCurvePacketMagic = 0x50435632u; // PCV2, ordered jump pairs
constexpr uint32_t kLegacyCurvePacketMagic = 0x50435631u; // PCV1
struct CurvePacketPoint {int32_t tick=0;int32_t curve=0;double semitones=0;};
struct CurvePacket {
 uint32_t magic=kCurvePacketMagic;
 int32_t bars=2, count=0;
 std::array<CurvePacketPoint,kMaxPoints> points{};
};
inline bool unpackCurve(const CurvePacket& packet,Pattern& result) noexcept {
 if((packet.magic!=kCurvePacketMagic&&packet.magic!=kLegacyCurvePacketMagic)||
    packet.bars<1||packet.bars>16||
    packet.count<1||packet.count>static_cast<int>(kMaxPoints))return false;
 Pattern candidate;
 if(!candidate.setBars(packet.bars))return false;
 for(int i=0;i<packet.count;++i) {
  const auto& p=packet.points[static_cast<size_t>(i)];
  if(p.curve<0||p.curve>2)return false;
  Point point{p.tick,p.semitones,static_cast<Curve>(p.curve)};
  if(i==0){
   if(point.tick!=0||point.semitones!=0||!candidate.setAt(0,point))
    return false;
  }else if(packet.magic==kCurvePacketMagic){
   if(!candidate.appendOrdered(point))return false;
  }else if(!candidate.upsert(point))return false; // historical PCV1
 }
 if(candidate.size()!=static_cast<size_t>(packet.count)||
    packet.points[0].tick!=0||packet.points[0].semitones!=0)return false;
 result=candidate;return true;
}
inline CurvePacket packCurve(const Pattern& pattern) noexcept {
 CurvePacket packet;
 packet.bars=pattern.bars();
 packet.count=static_cast<int32_t>(pattern.size());
 for(int i=0;i<packet.count;++i){
  const auto& p=pattern.at(static_cast<size_t>(i));
  packet.points[static_cast<size_t>(i)]={p.tick,static_cast<int32_t>(p.toNext),p.semitones};
 }
 return packet;
}
// A writer lock is only used from NON-realtime threads. Every field read by
// the realtime thread is atomic. Readers use a bounded retry and otherwise
// retain the previous valid pattern. No locks, allocs or waits in process().
class CurveMailbox {
public:
 explicit CurveMailbox(const Pattern& initial) noexcept {publish(initial);}
 void publish(const Pattern& pattern) noexcept {
  const std::lock_guard<std::mutex> lock(writer_);
  revision_.fetch_add(1,std::memory_order_acq_rel); // odd: writing
  bars_.store(pattern.bars(),std::memory_order_relaxed);
  count_.store(static_cast<int>(pattern.size()),std::memory_order_relaxed);
  for(size_t i=0;i<pattern.size();++i){
   const auto& p=pattern.at(i);
   points_[i].tick.store(p.tick,std::memory_order_relaxed);
   points_[i].semitones.store(p.semitones,std::memory_order_relaxed);
   points_[i].curve.store(static_cast<int>(p.toNext),std::memory_order_relaxed);
  }
  revision_.fetch_add(1,std::memory_order_release); // even: committed
 }
 uint64_t revision() const noexcept {return revision_.load(std::memory_order_acquire);}
 bool snapshot(Pattern& out,uint64_t& stamp) const noexcept {
  for(int attempts=0;attempts<3;++attempts){
   const auto begin=revision_.load(std::memory_order_acquire);
   if(begin&1u)continue;
   const int bars=bars_.load(std::memory_order_relaxed);
   const int count=count_.load(std::memory_order_relaxed);
   if(bars<1||bars>16||count<1||count>static_cast<int>(kMaxPoints))continue;
   Pattern next;
   if(!next.setBars(bars))continue;
   bool good=true;
   for(int i=0;i<count;++i){
    const auto& slot=points_[static_cast<size_t>(i)];
    Point p{slot.tick.load(std::memory_order_relaxed),
            slot.semitones.load(std::memory_order_relaxed),
            static_cast<Curve>(slot.curve.load(std::memory_order_relaxed))};
    if(!(i==0?next.setAt(0,p):next.appendOrdered(p))){good=false;break;}
   }
   const auto end=revision_.load(std::memory_order_acquire);
   if(good&&begin==end&&!(end&1u)&&next.size()==static_cast<size_t>(count)){
    out=next;stamp=end;return true;
   }
  }
  return false;
 }
private:
 struct AtomicPoint {
  std::atomic<int32_t> tick{0},curve{0};
  std::atomic<double> semitones{0.0};
 };
 mutable std::mutex writer_;
 std::atomic<uint64_t> revision_{0};
 std::atomic<int> bars_{1},count_{1};
 std::array<AtomicPoint,kMaxPoints> points_{};
};
} // namespace pitchator
