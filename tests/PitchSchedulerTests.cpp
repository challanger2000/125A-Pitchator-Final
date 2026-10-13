#include "../src/PitchScheduler.h"
#include <cstdio>
#include <vector>
using namespace pitchator;
#define CHECK(v) do{if(!(v)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#v);return 1;}}while(0)
int main(){
 Pattern p;CHECK(p.setBars(2));
 CHECK(p.upsert({960,3,Curve::Step}));
 CHECK(p.upsert({1920,1,Curve::Linear}));
 CHECK(p.upsert({2880,0,Curve::Linear}));
 Settings cfg;CHECK(cfg.trigger==TriggerMode::Continue); // new instance default
 cfg.trigger=TriggerMode::Host; // legacy HOST SYNC test fixture remains explicit
 Clock h{true,true,0,120,48000};Scheduler s;
 std::vector<Bend> results;
 auto sink=[&](Bend b){results.push_back(b);return true;};
 s.noteOn(0,60,cfg,0);
 CHECK(s.render(0,h,p,cfg,true,sink)&&results.empty());
 CHECK(s.render(12000,h,p,cfg,true,sink));
 CHECK(results.back().value==bend14(1.5,12));
 CHECK(s.render(24000,h,p,cfg,true,sink));
 CHECK(results.back().value==bend14(3,12));
 CHECK(s.render(48000,h,p,cfg,true,sink));
 CHECK(results.back().value==bend14(1,12));
 cfg.bypass=true;CHECK(s.render(48000,h,p,cfg,true,sink));
 CHECK(results.back().value==8192);
 cfg.bypass=false;s.noteOff(0,60);CHECK(s.render(48000,h,p,cfg,true,sink));
 CHECK(results.back().value==8192);
 s.reset();cfg.trigger=TriggerMode::Note;results.clear();
 s.noteOn(2,61,cfg,0);
 CHECK(s.render(24000,h,p,cfg,true,sink));
 CHECK(results.back().channel==2&&results.back().value==bend14(3,12));
 cfg.loop=false;CHECK(s.render(200000,h,p,cfg,true,sink));
 CHECK(results.back().value==8192);
 s.reset();cfg.loop=true;cfg.trigger=TriggerMode::Host;
 s.noteOn(1,60,cfg,0);s.noteOn(1,64,cfg,0);
 results.clear();CHECK(s.render(24000,h,p,cfg,true,sink));
 CHECK(results.size()==1&&results.back().channel==1);
 s.noteOff(1,64);s.noteOff(1,60);
 CHECK(s.render(24000,h,p,cfg,true,sink));CHECK(results.back().value==8192);
 s.reset();s.noteOn(0,60,cfg,0);results.clear();
 CHECK(s.render(24000,h,p,cfg,true,sink));h.playing=false;
 CHECK(s.render(24000,h,p,cfg,true,sink));CHECK(results.back().value==8192);
 // All sixteen channels are independent: no implicit MPE conversion.
 s.reset();h.playing=true;results.clear();
 for(int ch=0;ch<16;++ch)s.noteOn(ch,60,cfg,0);
 CHECK(s.render(24000,h,p,cfg,true,sink));
 CHECK(results.size()==16);
 for(int ch=0;ch<16;++ch)CHECK(results[ch].channel==ch);
 results.clear();
 for(int ch=0;ch<16;++ch)s.noteOff(ch,60);
 CHECK(s.render(24000,h,p,cfg,true,sink));
 CHECK(results.size()==16);
 for(const auto& b:results)CHECK(b.value==8192);
 // Receiver rejection must not claim successful bend transmission.
 s.reset();s.noteOn(0,60,cfg,0);
 auto refuse=[&](Bend){return false;};
 CHECK(!s.render(24000,h,p,cfg,true,refuse));
 results.clear();CHECK(s.render(24000,h,p,cfg,true,sink));
 CHECK(results.size()==1&&results.front().value==bend14(3,12));
 // Three trigger contracts. All timestamps use 120 BPM, 48kHz.
 Pattern phrase;CHECK(phrase.setBars(2));
 CHECK(phrase.upsert({3840,5.0,Curve::Step})); // BAR 2
 cfg.trigger=TriggerMode::Continue;cfg.loop=false;
 s.reset();results.clear();
 s.noteOn(0,60,cfg,0,h,phrase.lengthTicks());
 CHECK(s.render(0,h,phrase,cfg,true,sink));
 s.endBlock(96000); // exactly one 4/4 bar
 s.noteOff(0,60);
 CHECK(s.render(0,h,phrase,cfg,true,sink));
 s.noteOn(0,62,cfg,0,h,phrase.lengthTicks());
 CHECK(s.render(0,h,phrase,cfg,true,sink));
 CHECK(results.back().value==bend14(5,12)); // not BAR 1
 // Note gaps shorter than the phrase must preserve position.
 s.noteOff(0,62);s.endBlock(24000);
 s.noteOn(0,65,cfg,0,h,phrase.lengthTicks());
 CHECK(s.render(0,h,phrase,cfg,true,sink));
 CHECK(results.back().value==bend14(5,12));
 // The first note *after* a finished phrase must restart at BAR 1.
 s.noteOff(0,65);s.endBlock(96000);
 s.noteOn(0,60,cfg,0,h,phrase.lengthTicks());
 CHECK(s.render(0,h,phrase,cfg,true,sink));
 CHECK(results.back().value==8192);
 // Legacy MIDI NOTE always retriggers even with an existing note.
 s.reset();results.clear();cfg.trigger=TriggerMode::Note;
 s.noteOn(0,60,cfg,0,h,phrase.lengthTicks());s.endBlock(96000);
 s.noteOn(0,62,cfg,0,h,phrase.lengthTicks());
 CHECK(s.render(0,h,phrase,cfg,true,sink));
 CHECK(results.empty()); // still at 0 st
 // HOST SYNC must not retrigger on NoteOn and follows absolute song PPQ.
 s.reset();results.clear();cfg.trigger=TriggerMode::Host;
 h.ppq=4; // bar 2
 s.noteOn(0,60,cfg,0,h,phrase.lengthTicks());
 CHECK(s.render(0,h,phrase,cfg,true,sink));
 CHECK(results.back().value==bend14(5,12));
 // Accurate discontinuity scheduling: no 64-sample delayed transition.
 Pattern saw;CHECK(saw.setBars(2));
 CHECK(saw.upsert({3840,6,Curve::Linear}));
 CHECK(saw.addJump({3840,0,Curve::Linear}));
 s.reset();results.clear();cfg.trigger=TriggerMode::Continue;cfg.loop=false;
 h.ppq=0;h.playing=true;
 s.noteOn(0,60,cfg,0,h,saw.lengthTicks());
 CHECK(s.render(0,h,saw,cfg,true,sink));
 s.endBlock(95937); // 63 samples BEFORE BAR 2 at 120BPM/48kHz
 CHECK(s.render(0,h,saw,cfg,true,sink));
 CHECK(results.back().value>8192);
 CHECK(!s.jumpDue(62)&&s.jumpDue(63));
 CHECK(s.render(63,h,saw,cfg,true,sink)); // exact sample 96000
 CHECK(results.back().offset==63&&results.back().value==8192);
 CHECK(!s.jumpDue(64));
 // Another jump, this time with a nonzero pitch target.
 Pattern nextJump;CHECK(nextJump.setBars(2));
 CHECK(nextJump.upsert({960,6,Curve::Linear}));
 CHECK(nextJump.addJump({960,-4,Curve::Linear}));
 s.reset();results.clear();s.noteOn(0,60,cfg,0,h,nextJump.lengthTicks());
 CHECK(s.render(0,h,nextJump,cfg,true,sink));
 s.endBlock(23990);
 CHECK(s.render(0,h,nextJump,cfg,true,sink));
 CHECK(s.jumpDue(10));
 CHECK(s.render(10,h,nextJump,cfg,true,sink));
 CHECK(results.back().value==bend14(-4,12));
 std::puts("PitchScheduler test PASS");
}
