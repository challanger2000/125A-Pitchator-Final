#pragma once
#include "PitchEngine.h"
#include <array>
#include <cmath>
#include <cstdint>
namespace pitchator {
enum class TriggerMode : int {Host=0,Continue=1,Note=2};
struct Settings {
 int key=0; Scale scale=Scale::Chromatic; int range=12;
 bool lock=false; TriggerMode trigger=TriggerMode::Continue;
 bool loop=true,bypass=false;
};
struct Clock {
 bool valid=false,playing=false;
 double ppq=0,tempo=120,rate=48000;
};
struct Bend {int offset=0,channel=0;uint16_t value=8192;};
class Scheduler {
public:
 void reset() noexcept {channels_={};absoluteSample_=0;pendingJumps_.fill(-1);}
 int64_t sample()const noexcept{return absoluteSample_;}
 void endBlock(int count) noexcept {if(count>0)absoluteSample_+=count;}
 bool jumpDue(int offset) const noexcept {
  for(auto pending:pendingJumps_)if(pending>0&&pending==offset)return true;
  return false;
 }
 void noteOn(int ch,int pitch,const Settings& cfg,int offset,
             const Clock& clock=Clock{},int phraseTicks=2*kTicksPerBar) noexcept {
  if(ch<0||ch>=16||pitch<0||pitch>=128)return;
  auto& c=channels_[ch];
  if(c.held[pitch]<255){++c.held[pitch];++c.total;}
  c.pitch=pitch;
  const int64_t now=absoluteSample_+offset;
  if(cfg.trigger==TriggerMode::Note)c.anchor=now;
  else if(cfg.trigger==TriggerMode::Continue){
   // A new note continues the current phrase even across silent note gaps.
   // Once the configured phrase has elapsed, the NEXT NoteOn restarts it.
   // Channel-wide pitch bend: all simultaneous notes on this channel share phase.
   const double elapsed=(now-c.anchor)*clock.tempo*kTicksPerQuarter/(60.0*clock.rate);
   if(c.anchor<0||!std::isfinite(elapsed)||elapsed>=phraseTicks)c.anchor=now;
  }
 }
 void noteOff(int ch,int pitch) noexcept {
  if(ch<0||ch>=16||pitch<0||pitch>=128)return;
  auto& c=channels_[ch];
  if(c.held[pitch]){--c.held[pitch];--c.total;}
  if(c.total==0){c.pitch=-1;return;}
  if(c.pitch==pitch&&c.held[pitch]==0){
   for(int i=127;i>=0;--i)if(c.held[i]){c.pitch=i;break;}
  }
 }
 template<class Sink> bool center(int offset,Sink sink) noexcept {
  for(int ch=0;ch<16;++ch){
   auto& c=channels_[ch];
   if(c.last!=8192){
    if(!sink(Bend{offset,ch,8192}))return false;
    c.last=8192;
   }
  }
  return true;
 }
 template<class Sink> bool render(int offset,const Clock& clock,const Pattern& pattern,
                                  const Settings& cfg,bool force,Sink sink) noexcept {
  for(int ch=0;ch<16;++ch){
   auto& c=channels_[ch];uint16_t next=8192;
   double ticks=0;bool running=false;
   if(c.total&&c.pitch>=0&&!cfg.bypass&&std::isfinite(clock.tempo)&&
      clock.tempo>0&&std::isfinite(clock.rate)&&clock.rate>0){

    if(cfg.trigger==TriggerMode::Host){
     if(clock.playing&&clock.valid&&std::isfinite(clock.ppq)){
      ticks=(clock.ppq+offset*clock.tempo/(60.0*clock.rate))*kTicksPerQuarter;
      running=true;
     }
    }else if(c.anchor>=0){
     ticks=(absoluteSample_+offset-c.anchor)*clock.tempo*kTicksPerQuarter/(60.0*clock.rate);
     running=ticks>=0;
    }
    if(running&&std::isfinite(ticks)&&(cfg.loop||ticks<pattern.lengthTicks())){
     next=bend14(value(pattern,ticks,c.pitch,cfg.key,cfg.scale,cfg.lock),cfg.range);
    }
   }
   // Predict the next same-tick jump within the next 64 samples.
   // A constant-time check per audio sample then emits the discontinuity
   // at the FIRST sample on/after the boundary (never quantizes to 64).
   pendingJumps_[ch]=-1;
   if(c.total&&pattern.hasJumps()&&running&&std::isfinite(ticks)&&
      !cfg.bypass&&std::isfinite(clock.rate)&&clock.rate>0&&
      std::isfinite(clock.tempo)&&clock.tempo>0){
    const double dTick=clock.tempo*kTicksPerQuarter/(60.0*clock.rate);
    double closest=std::numeric_limits<double>::infinity();
    for(size_t i=1;i<pattern.size();++i){
     if(pattern.at(i).tick!=pattern.at(i-1).tick)continue;
     const double t=pattern.at(i).tick;
     double when=t;
     if(cfg.loop){
      const double cycles=std::floor((ticks-t)/pattern.lengthTicks())+1.0;
      when=t+std::max(0.0,cycles)*pattern.lengthTicks();
     }
     if(when>ticks&&when<closest)closest=when;
    }
    if(std::isfinite(closest)){
     const double samples=(closest-ticks)/dTick;
     if(samples>0&&samples<=64.00001){
      const auto delay=static_cast<int>(std::ceil(samples-1e-9));
      if(delay>=1&&delay<=64)pendingJumps_[ch]=offset+delay;
     }
    }
   }
   if(next!=c.last&&(force||c.total==0||offset%64==0)){
    if(!sink(Bend{offset,ch,next}))return false;
    c.last=next;
   }
  }
  return true;
 }
private:
 struct Channel {
  std::array<uint8_t,128> held{};
  uint32_t total=0;
  int pitch=-1;
  int64_t anchor=-1;
  uint16_t last=8192;
 };
 std::array<Channel,16> channels_{};
 std::array<int32_t,16> pendingJumps_{};
 int64_t absoluteSample_=0;
};
}
