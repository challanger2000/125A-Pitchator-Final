#pragma once
#include "PitchEngine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
namespace pitchator::edit {
// Exact position: BAR.BEAT.TICK, tick 0..959 within the quarter-note beat.
// This permits every 1/32 subdivision (120 ticks) plus arbitrary PPQ ticks.
inline bool parsePosition(const char* input,int bars,int32_t& tick) noexcept {
 if(!input||bars<1||bars>16)return false;
 int b=0,beat=0,sub=0,used=0;
 if(std::sscanf(input,"%d.%d.%d%n",&b,&beat,&sub,&used)!=3 ||
    used<=0||input[used]!=0||b<1||b>bars||beat<1||beat>4||
    sub<0||sub>=kTicksPerQuarter)return false;
 tick=(b-1)*kTicksPerBar+(beat-1)*kTicksPerQuarter+sub;
 return tick>=0&&tick<bars*kTicksPerBar;
}
inline bool parseSemitones(const char* input,double& semitones) noexcept {
 if(!input)return false;
 const size_t len=std::strlen(input);
 if(len==0||len>=64)return false;
 char buffer[64];
 for(size_t i=0;i<len;++i) buffer[i]=input[i]==','?'.':input[i];
 buffer[len]=0;
 char* end=nullptr;
 const double val=std::strtod(buffer,&end);
 if(end==buffer||*end!=0||!std::isfinite(val)||val < -48.0||val>48.0)
  return false;
 semitones=val;
 return true;
}
inline void formatPosition(int32_t tick,char (&dest)[32]) noexcept {
 std::snprintf(dest,sizeof(dest),"%d.%d.%03d",tick/kTicksPerBar+1,
   tick%kTicksPerBar/kTicksPerQuarter+1,tick%kTicksPerQuarter);
}
inline void formatSemitones(double val,char (&dest)[32]) noexcept {
 std::snprintf(dest,sizeof(dest),"%+.2f",val);
}
} // namespace pitchator::edit
