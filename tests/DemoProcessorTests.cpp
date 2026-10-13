#include "../src/PitchatorVST.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include <cstdio>
#include <memory>

using namespace Steinberg;
using namespace Steinberg::Vst;

// Standalone EXE harness only. The real VST3 DLL uses the SDK module entry.
void* moduleHandle=nullptr;

#define CHECK(v) do{if(!(v)){std::fprintf(stderr,"DemoProcessorTests line %d: %s\n",__LINE__,#v);return 1;}}while(0)

static ProcessContext contextAt(double ppq){
 ProcessContext c{};
 c.state=ProcessContext::kPlaying |
         ProcessContext::kProjectTimeMusicValid |
         ProcessContext::kTempoValid;
 c.projectTimeMusic=ppq;
 c.tempo=120.0;
 c.sampleRate=48000.0;
 return c;
}

static Event noteOn(int offset,int channel,int pitch,int noteId){
 Event e{};
 e.busIndex=0;
 e.type=Event::kNoteOnEvent;
 e.sampleOffset=offset;
 e.noteOn.channel=static_cast<int16>(channel);
 e.noteOn.pitch=static_cast<int16>(pitch);
 e.noteOn.velocity=0.8f;
 e.noteOn.tuning=0.f;
 e.noteOn.length=0;
 e.noteOn.noteId=noteId;
 return e;
}

static int bendValue(const Event& e){
 return (static_cast<int>(e.midiCCOut.value)&127) |
        ((static_cast<int>(e.midiCCOut.value2)&127)<<7);
}

int main(){
 auto processor=std::make_unique<PitchatorProcessor>();

 double ppq=0.0;
 bool sawNote=false;
 for(int second=0;second<29;++second){
  EventList input(8),output(4096);
  if(second==0){
   auto on=noteOn(0,2,60,77);
   CHECK(input.addEvent(on)==kResultOk);
  }
  auto ctx=contextAt(ppq);
  ProcessData d{};
  d.numSamples=48000;
  d.inputEvents=&input;
  d.outputEvents=&output;
  d.processContext=&ctx;
  CHECK(processor->process(d)==kResultOk);
  for(int32 i=0;i<output.getEventCount();++i){
   Event e{};CHECK(output.getEvent(i,e)==kResultOk);
   if(e.type==Event::kNoteOnEvent)sawNote=true;
  }
  ppq+=2.0;
 }
 CHECK(sawNote);

 // Two-second block: the remaining one second expires exactly at sample 48000.
 EventList crossingIn(8),crossingOut(8192);
 auto crossingCtx=contextAt(ppq);
 ProcessData crossing{};
 crossing.numSamples=96000;
 crossing.inputEvents=&crossingIn;
 crossing.outputEvents=&crossingOut;
 crossing.processContext=&crossingCtx;
 CHECK(processor->process(crossing)==kResultOk);

 bool sawCleanupNoteOff=false;
 bool sawCenter=false;
 bool sawAllNotesOff=false;
 for(int32 i=0;i<crossingOut.getEventCount();++i){
  Event e{};CHECK(crossingOut.getEvent(i,e)==kResultOk);
  CHECK(e.sampleOffset<=48000);
  if(e.sampleOffset==48000&&e.type==Event::kNoteOffEvent&&
     e.noteOff.channel==2&&e.noteOff.pitch==60)
   sawCleanupNoteOff=true;
  if(e.sampleOffset==48000&&e.type==Event::kLegacyMIDICCOutEvent&&
     e.midiCCOut.channel==2&&e.midiCCOut.controlNumber==kPitchBend&&
     bendValue(e)==8192)
   sawCenter=true;
  if(e.sampleOffset==48000&&e.type==Event::kLegacyMIDICCOutEvent&&
     e.midiCCOut.channel==2&&e.midiCCOut.controlNumber==123)
   sawAllNotesOff=true;
 }
 CHECK(sawCleanupNoteOff);
 CHECK(sawCenter);
 CHECK(sawAllNotesOff);

 // Expired Demo: even a fresh NoteOn must not reach the output.
 EventList expiredIn(8),expiredOut(256);
 auto fresh=noteOn(0,2,64,88);
 CHECK(expiredIn.addEvent(fresh)==kResultOk);
 ppq+=4.0;
 auto expiredCtx=contextAt(ppq);
 ProcessData expired{};
 expired.numSamples=48000;
 expired.inputEvents=&expiredIn;
 expired.outputEvents=&expiredOut;
 expired.processContext=&expiredCtx;
 CHECK(processor->process(expired)==kResultOk);
 for(int32 i=0;i<expiredOut.getEventCount();++i){
  Event e{};CHECK(expiredOut.getEvent(i,e)==kResultOk);
  CHECK(e.type!=Event::kNoteOnEvent);
  if(e.type==Event::kLegacyMIDICCOutEvent)
   CHECK(e.midiCCOut.controlNumber!=kPitchBend);
 }

 std::puts("Pitchator 30-second processor Demo PASS");
 return 0;
}
