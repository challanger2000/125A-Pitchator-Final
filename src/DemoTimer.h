#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pitchator {

class DemoTimer final {
public:
 void enable(double sampleRate) noexcept {
  enabled_=true;
  setSampleRate(sampleRate);
  elapsed_=0;
 }
 void setSampleRate(double sampleRate) noexcept {
  if(!std::isfinite(sampleRate)||sampleRate<=1000.0)return;
  const double elapsedSeconds=rate_>0.0?static_cast<double>(elapsed_)/rate_:0.0;
  rate_=sampleRate;
  limit_=static_cast<std::uint64_t>(std::llround(rate_*30.0));
  if(enabled_)elapsed_=std::min(limit_,static_cast<std::uint64_t>(std::llround(elapsedSeconds*rate_)));
 }
 int allowedSamples(bool playing,int requested) noexcept {
  if(!enabled_||!playing||requested<=0)return requested;
  if(elapsed_>=limit_)return 0;
  const auto remain=limit_-elapsed_;
  const auto allowed=std::min<std::uint64_t>(remain,static_cast<std::uint64_t>(requested));
  elapsed_+=allowed;
  return static_cast<int>(allowed);
 }
 bool enabled() const noexcept{return enabled_;}
 bool expired() const noexcept{return enabled_&&elapsed_>=limit_;}
private:
 bool enabled_=false;
 double rate_=44100.0;
 std::uint64_t limit_=1323000u;
 std::uint64_t elapsed_=0u;
};

} // namespace pitchator
