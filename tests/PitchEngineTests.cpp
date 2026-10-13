#include "../src/PitchEngine.h"
#undef NDEBUG
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>
using namespace pitchator;
static bool near(double a,double b,double tol=1e-9){return std::abs(a-b)<tol;}
int main(){
 Pattern p;
 assert(p.size()==1&&p.bars()==1&&near(p.value(960),0));
 assert(p.setBars(16)&&p.lengthTicks()==61440);
 assert(!p.setBars(17)&&!p.setBars(0));
 assert(p.upsert({gridTick(1,2,0),3,Curve::Step}));
 assert(p.upsert({gridTick(1,3,0),1,Curve::Linear}));
 assert(p.upsert({gridTick(1,4,0),0,Curve::Smooth}));
 assert(near(p.value(960),3)&&near(p.value(1919),3));
 assert(near(p.value(1920),1)&&near(p.value(2400),0.5));
 assert(near(p.value(2880),0));
 assert(!p.upsert({0,2,Curve::Step}));
 assert(!p.upsert({400,-49,Curve::Linear}));
 assert(!p.upsert({400,NAN,Curve::Linear}));
 assert(!p.upsert({61440,2,Curve::Step}));
 assert(!p.setBars(0));
 assert(!p.erase(0));
 assert(p.erase(960)&&near(p.value(960),0.5));
 assert(!p.erase(960));
 p.clear();assert(p.size()==1&&near(p.value(960),0));
 assert(p.setBars(1));
 assert(p.upsert({960,3,Curve::Linear}));
 assert(near(p.value(480),1.5));
 assert(p.upsert({960,4,Curve::Smooth})&&near(p.value(480),2.0));
 assert(p.upsert({1920,0,Curve::Step}));
 assert(near(p.value(1440),2.0)); // smooth midpoint
 assert(near(p.value(3840),0)); // 1-bar loop
 // Twelve tone classes of each scale checked against independent contracts.
 assert(isInScale(Scale::Major,0)&&isInScale(Scale::Major,11));
 assert(!isInScale(Scale::Major,1));
 assert(isInScale(Scale::NaturalMinor,3)&&!isInScale(Scale::NaturalMinor,4));
 assert(isInScale(Scale::Dorian,9)&&!isInScale(Scale::Dorian,8));
 assert(isInScale(Scale::Lydian,6)&&!isInScale(Scale::Lydian,5));
 assert(isInScale(Scale::Mixolydian,10)&&!isInScale(Scale::Mixolydian,11));
 assert(isInScale(Scale::Locrian,6)&&!isInScale(Scale::Locrian,7));
 assert(isInScale(Scale::HarmonicMinor,11)&&!isInScale(Scale::HarmonicMinor,10));
 // Exact half-tone +3 and -2 from current destination for a chromatic pattern.
 assert(near(value(p,960,60,0,Scale::Chromatic,true),4));
 assert(near(snapTarget(3,60,0,Scale::NaturalMinor),3));
 assert(near(snapTarget(1,60,0,Scale::NaturalMinor),2)); // C# -> D
 assert(near(snapTarget(-1,60,0,Scale::NaturalMinor),-2)); // B -> Bb
 assert(near(snapTarget(0,61,0,Scale::NaturalMinor),0)); // neutral is neutral
 assert(near(value(p,480,60,0,Scale::Major,false),2)); // unlocked glide
 // Check the FULL 12-tone membership contract, not just one note per mode.
 const std::vector<std::vector<int>> expectedNotes={
  {0,1,2,3,4,5,6,7,8,9,10,11}, // chromatic
  {0,2,4,5,7,9,11},            // major
  {0,2,3,5,7,8,10},            // natural minor
  {0,2,3,5,7,8,11},            // harmonic minor
  {0,2,4,7,9},                 // major pentatonic
  {0,3,5,7,10},                // minor pentatonic
  {0,2,3,5,7,9,10},            // dorian
  {0,1,3,5,7,8,10},            // phrygian
  {0,2,4,6,7,9,11},            // lydian
  {0,2,4,5,7,9,10},            // mixolydian
  {0,1,3,5,6,8,10}             // locrian
 };
 for(size_t s=0;s<expectedNotes.size();++s)
  for(int n=0;n<12;++n){
   const auto& allowed=expectedNotes[s];
   const bool expected=std::find(allowed.begin(),allowed.end(),n)!=allowed.end();
   assert(isInScale(static_cast<Scale>(s),n)==expected);
  }
 // MIDI 14-bit encoding, exact central + both endpoints, symmetry contracts.
 assert(bend14(0,12)==8192);
 assert(bend14(-12,12)==0&&bend14(12,12)==16383);
 assert(bend14(3,12)==10240);
 assert(bend14(-3,12)==6144);
 assert(bend14(99,12)==16383&&bend14(-99,12)==0);
 assert(bend14(NAN,12)==8192);
 assert(near(hostTick(0,120,48000,24000,1),960));
 assert(near(hostTick(4,120,48000,0,1),0));
 assert(near(hostTick(-1,120,48000,0,1),2880));
 assert(gridTick(16,4,3)==(16*3840-240));
 assert(gridTick(0,1,0)==-1&&gridTick(1,5,0)==-1);
 // Same-tick dual knots: end BAR 1 at +6, jump to 0 at BAR 2.
 Pattern saw;assert(saw.setBars(2));
 assert(saw.upsert({3840,6,Curve::Linear}));
 assert(saw.addJump({3840,0,Curve::Linear}));
 assert(saw.size()==3&&saw.hasJumps());
 assert(saw.at(1).tick==saw.at(2).tick);
 assert(near(saw.value(3840),0));
 assert(saw.value(3839)>5.99&&saw.value(3839)<6.0);
 assert(near(saw.value(1920),3));
 assert(!saw.addJump({3840,4,Curve::Linear})); // never three points
 assert(!saw.addJump({0,3,Curve::Linear})); // immutable origin
 assert(near(saw.value(3840.01),0));
 size_t moved=0;
 assert(saw.moveAt(2,{3840,-3,Curve::Linear},moved));
 assert(moved==2&&near(saw.value(3840),-3));
 assert(near(saw.value(3839),saw.value(3839.0)));
 assert(saw.crossesJump(3839,3840,false));
 assert(!saw.crossesJump(3840,3850,false));
 assert(saw.crossesJump(3839,3840,true));
 assert(saw.crossesJump(11519,11520,true)); // bar 2 in next loop
 assert(saw.eraseAt(2)&&!saw.hasJumps()&&saw.size()==2);
 assert(near(saw.value(3840),6));
 assert(saw.addJump({3840,0,Curve::Smooth}));
 Pattern swept=saw; // Move a knot onto another to create an intentional pair.
 assert(swept.upsert({4800,4,Curve::Linear}));
 size_t newIndex=0;
 assert(!swept.moveAt(3,{3840,2,Curve::Linear},newIndex)); // no third
 assert(swept.eraseAt(2));
 assert(swept.moveAt(2,{3840,2,Curve::Linear},newIndex));
 assert(swept.hasJumps()&&newIndex==2&&near(swept.value(3840),2));
 // Chained vertical jumps at arbitrary subdivisions (zigzag).
 Pattern zig;assert(zig.setBars(4));
 for(int tick=960;tick<4*kTicksPerBar;tick+=1920){
  assert(zig.upsert({tick,6,Curve::Linear}));
  assert(zig.addJump({tick,-4,Curve::Linear}));
  assert(near(zig.value(tick),-4));
  assert(zig.value(tick-0.01)>-4);
 }
 std::cout<<"PitchEngine: all deterministic contracts PASS\n";
}
