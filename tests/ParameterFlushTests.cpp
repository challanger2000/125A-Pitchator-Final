#include "../src/PitchatorVST.h"
#include "../src/ScaleNoteNames.h"
#include "../src/PitchCurveView.h"
#include "vstgui/lib/controls/ctextedit.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include <cstdio>
#include <memory>
#include "vstgui/lib/vstguiinit.h"
#include "vstgui/lib/events.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/cview.h"
#include "vstgui/uidescription/uiattributes.h"
#ifdef _WIN32
#include <windows.h>
#endif
using namespace Steinberg;
using namespace Steinberg::Vst;

// Standalone EXE test harness only: the plugin DLL gets the real HINSTANCE
// from the SDK's Windows module entry. Never use this placeholder in the DLL.
void* moduleHandle=nullptr;

#define CHECK(v) do { if(!(v)){std::fprintf(stderr,"ParameterFlushTests line %d: %s\n",__LINE__,#v);return 1;}}while(0)
static bool flushBypass(PitchatorProcessor& processor,ParamValue value){
 ParameterChanges changes(1);
 int32 index=0;
 auto* queue=changes.addParameterData(kPitchBypass,index);
 if(!queue)return false;
 int32 point=0;
 if(queue->addPoint(0,value,point)!=kResultOk)return false;
 ProcessData data{};
 data.numSamples=0;
 data.numInputs=0;
 data.numOutputs=0;
 data.inputParameterChanges=&changes;
 data.outputEvents=nullptr;  // crucial: validator flushes with no event list
 return processor.process(data)==kResultOk;
}
static int runTests(){

 auto processor=std::make_unique<PitchatorProcessor>();
 PitchatorController controller;
 CHECK(controller.initialize(nullptr)==kResultOk);
 bool foundThreeTriggers=false;
 for(int32 i=0;i<controller.getParameterCount();++i){
  ParameterInfo info{};
  if(controller.getParameterInfo(i,info)==kResultOk&&info.id==kPitchTrigger){
   CHECK(info.stepCount==2); // host sees three choices (0, 1, 2)
   foundThreeTriggers=true;
  }
 }
 CHECK(foundThreeTriggers);
 CHECK(controller.getParamNormalized(kPitchTrigger)==0.5); // fresh instances start NOTE CONTINUE
 CHECK(flushBypass(*processor,1));
 MemoryStream snapshot;
 CHECK(processor->getState(&snapshot)==kResultOk);
 CHECK(flushBypass(*processor,0));
 MemoryStream zeroState;
 CHECK(processor->getState(&zeroState)==kResultOk);
 CHECK(zeroState.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 CHECK(controller.setComponentState(&zeroState)==kResultOk);
 CHECK(controller.getParamNormalized(kPitchBypass)==0.0);
 CHECK(snapshot.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 CHECK(controller.setComponentState(&snapshot)==kResultOk);
 CHECK(controller.getParamNormalized(kPitchBypass)==1.0);
 CHECK(snapshot.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 auto restored=std::make_unique<PitchatorProcessor>();
 CHECK(restored->setState(&snapshot)==kResultOk);
 MemoryStream roundtrip;
 CHECK(restored->getState(&roundtrip)==kResultOk);
 CHECK(roundtrip.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 CHECK(controller.setComponentState(&roundtrip)==kResultOk);
 CHECK(controller.getParamNormalized(kPitchBypass)==1.0);
 // Integration probe: GUI-shaped CurvePacket -> IMessage -> processor
 // mailbox -> real ProcessData MIDI output list, *not* just a curve-unit test.
 auto routed=std::make_unique<PitchatorProcessor>();
 // This integration fixture validates absolute HOST SYNC PPQ mapping.
 // Select it explicitly; fresh plugin instances default to NOTE CONTINUE.
 {
  ParameterChanges modeChanges(1);
  int32 index=0,point=0;
  auto* queue=modeChanges.addParameterData(kPitchTrigger,index);
  CHECK(queue!=nullptr);
  CHECK(queue->addPoint(0,0.0,point)==kResultOk);
  ProcessData flush{};
  flush.numSamples=0;
  flush.inputParameterChanges=&modeChanges;
  CHECK(routed->process(flush)==kResultOk);
 }
 pitchator::Pattern modified;
 CHECK(modified.setBars(2));
 CHECK(modified.upsert({960,-3.0,pitchator::Curve::Step}));
 HostMessage message;
 message.setMessageID("125A.Pitchator.Curve");
 auto packed=pitchator::packCurve(modified);
 CHECK(message.getAttributes()->setBinary("Curve",&packed,
       static_cast<uint32>(sizeof(packed)))==kResultOk);
 CHECK(routed->notify(&message)==kResultOk);
 // Invalid GUI messages must not overwrite the last accepted snapshot.
 packed.magic=0;
 HostMessage invalid;
 invalid.setMessageID("125A.Pitchator.Curve");
 CHECK(invalid.getAttributes()->setBinary("Curve",&packed,
       static_cast<uint32>(sizeof(packed)))==kResultOk);
 CHECK(routed->notify(&invalid)==kResultFalse);
 EventList midiIn(32),midiOut(128);
 Event on{};
 on.busIndex=0;on.type=Event::kNoteOnEvent;on.sampleOffset=12;
 on.noteOn.channel=3;on.noteOn.pitch=60;on.noteOn.velocity=0.75f;
 on.noteOn.noteId=42;on.noteOn.length=0;
 CHECK(midiIn.addEvent(on)==kResultOk);
 Event off{};
 off.busIndex=0;off.type=Event::kNoteOffEvent;off.sampleOffset=128;
 off.noteOff.channel=3;off.noteOff.pitch=60;off.noteOff.velocity=0.25f;
 off.noteOff.noteId=42;
 CHECK(midiIn.addEvent(off)==kResultOk);
 ProcessContext clock{};
 clock.state=ProcessContext::kPlaying |
             ProcessContext::kProjectTimeMusicValid |
             ProcessContext::kTempoValid;
 clock.projectTimeMusic=1.0; // beat 2: curve target -3 st
 clock.tempo=120.0;clock.sampleRate=48000.0;
 ProcessData block{};
 block.numSamples=160;
 block.numInputs=0;block.numOutputs=0;
 block.inputEvents=&midiIn;block.outputEvents=&midiOut;
 block.processContext=&clock;
 CHECK(routed->process(block)==kResultOk);
 // Expect bend first (the NoteOn at 12 must already sound transposed),
 // then original NoteOn, NoteOff and precise channel pitch center.
 CHECK(midiOut.getEventCount()==4);
 Event bendBefore{},noteOut{},noteOffOut{},bendReset{};
 CHECK(midiOut.getEvent(0,bendBefore)==kResultOk);
 CHECK(midiOut.getEvent(1,noteOut)==kResultOk);
 CHECK(midiOut.getEvent(2,noteOffOut)==kResultOk);
 CHECK(midiOut.getEvent(3,bendReset)==kResultOk);
 CHECK(bendBefore.type==Event::kLegacyMIDICCOutEvent);
 CHECK(bendBefore.sampleOffset==12&&bendBefore.midiCCOut.channel==3);
 CHECK(bendBefore.midiCCOut.controlNumber==kPitchBend);
 CHECK((static_cast<int>(bendBefore.midiCCOut.value)&127)+
       ((static_cast<int>(bendBefore.midiCCOut.value2)&127)<<7)==
       pitchator::bend14(-3,12));
 CHECK(noteOut.type==Event::kNoteOnEvent&&noteOut.sampleOffset==12);
 CHECK(noteOut.noteOn.noteId==42&&noteOut.noteOn.pitch==60);
 CHECK(noteOut.noteOn.channel==3&&noteOut.noteOn.velocity==0.75f);
 CHECK(noteOffOut.type==Event::kNoteOffEvent&&noteOffOut.sampleOffset==128);
 CHECK(noteOffOut.noteOff.noteId==42&&noteOffOut.noteOff.pitch==60);
 CHECK(bendReset.type==Event::kLegacyMIDICCOutEvent);
 CHECK(bendReset.sampleOffset==128&&bendReset.midiCCOut.channel==3);
 CHECK((static_cast<int>(bendReset.midiCCOut.value)&127)+
       ((static_cast<int>(bendReset.midiCCOut.value2)&127)<<7)==8192);
 MemoryStream routedSaved;
 CHECK(routed->getState(&routedSaved)==kResultOk);
 CHECK(routedSaved.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 auto reopened=std::make_unique<PitchatorProcessor>();
 CHECK(reopened->setState(&routedSaved)==kResultOk);
 // Backward compatibility: V1 trigger 1 and normalized 1.0 must still
 // represent NOTE RETRIGGER. The new NOTE CONTINUE occupies 0.5.
 auto writeLegacy = [](MemoryStream& stream,uint32 legacyVersion=1u)->bool{
  auto wr32=[&](uint32 value){
   int32 n=0;
   return stream.write(&value,sizeof(value),&n)==kResultOk&&n==sizeof(value);
  };
  auto wrI=[&](int32 value){
   int32 n=0;
   return stream.write(&value,sizeof(value),&n)==kResultOk&&n==sizeof(value);
  };
  const double semitones=0;
  int32 bytes=0;
  return wr32(kPitchMagic)&&wr32(legacyVersion)&&
         wrI(2)&&wrI(0)&&wrI(0)&&wrI(12)&&
         wrI(0)&&wrI(1)&&wrI(1)&&wrI(0)&&
         wrI(1)&&wrI(0)&&
         stream.write(const_cast<double*>(&semitones),sizeof(semitones),&bytes)==kResultOk&&
         bytes==sizeof(semitones)&&wrI(1);
 };
 MemoryStream legacy;
 CHECK(writeLegacy(legacy));
 CHECK(legacy.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 auto legacyProcessor=std::make_unique<PitchatorProcessor>();
 CHECK(legacyProcessor->setState(&legacy)==kResultOk);
 CHECK(legacy.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 CHECK(controller.setComponentState(&legacy)==kResultOk);
 CHECK(controller.getParamNormalized(kPitchTrigger)==1.0);
 // Old V2 state used 3 trigger choices and must retain retrigger as ID=2.
 MemoryStream versionTwo;
 CHECK(writeLegacy(versionTwo,2u));
 CHECK(versionTwo.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 auto v2Processor=std::make_unique<PitchatorProcessor>();
 CHECK(v2Processor->setState(&versionTwo)==kResultOk);
 CHECK(versionTwo.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 CHECK(controller.setComponentState(&versionTwo)==kResultOk);
 CHECK(controller.getParamNormalized(kPitchTrigger)==0.5);
 CHECK(controller.setParamNormalized(kPitchTrigger,0.5)==kResultOk);
 CHECK(controller.getParamNormalized(kPitchTrigger)==0.5);
 // We must test saving/restoring the new mode through the real processor
 // rather than assuming that three GUI strings also imply 3-state persistence.
 ParameterChanges triggerChanges(1);
 int32 triggerIndex=0,pointIndex=0;
 auto* triggerQueue=triggerChanges.addParameterData(kPitchTrigger,triggerIndex);
 CHECK(triggerQueue!=nullptr);
 CHECK(triggerQueue->addPoint(0,0.5,pointIndex)==kResultOk);
 ProcessData flush{};flush.numSamples=0;flush.inputParameterChanges=&triggerChanges;
 CHECK(legacyProcessor->process(flush)==kResultOk);
 MemoryStream continueState;
 CHECK(legacyProcessor->getState(&continueState)==kResultOk);
 CHECK(continueState.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 auto continueRestored=std::make_unique<PitchatorProcessor>();
 CHECK(continueRestored->setState(&continueState)==kResultOk);
 CHECK(continueState.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 CHECK(controller.setComponentState(&continueState)==kResultOk);
 CHECK(controller.getParamNormalized(kPitchTrigger)==0.5);
 // The legacy migration probe changed BYPASS to OFF deliberately.
 // Restore the original known ON snapshot before existing button checks.
 CHECK(snapshot.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 CHECK(controller.setComponentState(&snapshot)==kResultOk);
 CHECK(controller.getParamNormalized(kPitchBypass)==1.0);

 // Live KEY/SCALE note display and state restoration, no audio-DSP change.
 VSTGUI::UIAttributes noteAttrs;
 noteAttrs.setAttribute("pitchator-id","scale-notes");
 auto* noteLabel=new VSTGUI::CTextLabel(VSTGUI::CRect(40,180,1162,200),"NOTES");
 CHECK(controller.verifyView(noteLabel,noteAttrs,nullptr,nullptr)==noteLabel);
 CHECK(!noteLabel->getMouseEnabled());
 CHECK(noteLabel->getText()==pitchator::scaleNoteNames(0,pitchator::Scale::Chromatic).c_str());
 CHECK(controller.setParamNormalized(kPitchKey,0.0)==kResultOk);
 CHECK(controller.setParamNormalized(kPitchScale,2.0/10.0)==kResultOk);
 CHECK(noteLabel->getText()==
       u8"NOTES: C  \u00B7  D  \u00B7  E\u266D  \u00B7  F  \u00B7  G  \u00B7  A\u266D  \u00B7  B\u266D");
 CHECK(controller.setParamNormalized(kPitchKey,5.0/11.0)==kResultOk);
 CHECK(noteLabel->getText()==pitchator::scaleNoteNames(5,pitchator::Scale::NaturalMinor).c_str());
 CHECK(snapshot.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 CHECK(controller.setComponentState(&snapshot)==kResultOk);
 CHECK(noteLabel->getText()==pitchator::scaleNoteNames(0,pitchator::Scale::Chromatic).c_str());
 controller.willClose(nullptr);
 noteLabel->forget();
 // Real VSTGUI CTextButton left-down / left-up path into controller,
 // including explicit VST3 edit sequence for ON and OFF.
 auto checkToggle=[&](ParamID id,bool initial,const char* initialTitle)->bool {
  auto* toggle=new VSTGUI::CTextButton(VSTGUI::CRect(0,0,120,32),
                                       nullptr,static_cast<int>(id),
                                       initialTitle);
  VSTGUI::UIAttributes attrs;
  if(controller.verifyView(toggle,attrs,nullptr,nullptr)!=toggle)return false;
  if(toggle->getListener()!=&controller)return false;
  if(toggle->getStyle()!=VSTGUI::CTextButton::kOnOffStyle)return false;
  if((controller.getParamNormalized(id)>=0.5)!=initial)return false;
  if((toggle->getValueNormalized()>=0.5)!=initial)return false;
  for(int click=0;click<2;++click){
   auto down=VSTGUI::MouseDownEvent(VSTGUI::CPoint(28,16),VSTGUI::MouseButton::Left);
   toggle->onMouseDownEvent(down);
   if(!down.consumed)return false;
   auto up=VSTGUI::MouseUpEvent(VSTGUI::CPoint(28,16),VSTGUI::MouseButton::Left);
   toggle->onMouseUpEvent(up);
   if(!up.consumed)return false;
   const bool expected=(click==0)?!initial:initial;
   if((controller.getParamNormalized(id)>=0.5)!=expected)return false;
   if((toggle->getValueNormalized()>=0.5)!=expected)return false;
  }
  // Automation/restored state must update the toggle without a mouse click.
  controller.setParamNormalized(id,initial?0.0:1.0);
  if((toggle->getValueNormalized()>=0.5)==initial)return false;
  controller.setParamNormalized(id,initial?1.0:0.0);
  if((toggle->getValueNormalized()>=0.5)!=initial)return false;
  controller.willClose(nullptr);
  toggle->forget();
  return true;
 };
 CHECK(checkToggle(kPitchLock,false,"LOCK OFF"));
 CHECK(checkToggle(kPitchLoop,true,"LOOP ON"));
 CHECK(checkToggle(kPitchBypass,true,"BYPASS ON"));
 // Bypass remains ON after previously-verified stored state restore.
 CHECK(controller.getParamNormalized(kPitchBypass)==1.0);

 // All eight toolbar buttons 900..907: real CTextButton mouse gestures
 // through verifyView's explicit listener + valueChanged route.
 VSTGUI::UIAttributes graphAttrs;
 graphAttrs.setAttribute("pitchator-id","curve");
 auto* placeholder=new VSTGUI::CView(VSTGUI::CRect(30,248,1170,515));
 auto* graph=dynamic_cast<pitchator::PitchCurveView*>(
    controller.verifyView(placeholder,graphAttrs,nullptr,nullptr));
 CHECK(graph!=nullptr&&graph!=placeholder);
 placeholder->forget();
 CHECK(graph->pattern().size()==4);
 auto click=[&](VSTGUI::CTextButton* b)->bool{
  auto d=VSTGUI::MouseDownEvent(VSTGUI::CPoint(40,15),VSTGUI::MouseButton::Left);
  b->onMouseDownEvent(d);
  if(!d.consumed)return false;
  auto u=VSTGUI::MouseUpEvent(VSTGUI::CPoint(40,15),VSTGUI::MouseButton::Left);
  b->onMouseUpEvent(u);
  return u.consumed;
 };
 auto custom=[&](int tag,const char* title){
  auto* b=new VSTGUI::CTextButton(VSTGUI::CRect(0,0,170,30),
                                  nullptr,tag,title,
                                  VSTGUI::CTextButton::kKickStyle);
  VSTGUI::UIAttributes attrs;
  controller.verifyView(b,attrs,nullptr,nullptr);
  return b;
 };
 auto* zoom=custom(900,"100 %");
 CHECK(zoom->getListener()==&controller&&click(zoom));
 CHECK(zoom->getTitle()=="150 %");
 auto* shape=custom(903,"CURVE: LINEAR");
 CHECK(click(shape));
 CHECK(graph->curve()==pitchator::Curve::Smooth);
 auto* snap=custom(904,"SNAP ON");
 CHECK(graph->snap()&&click(snap)&&!graph->snap());
 auto* graphZoom=custom(907,"GRAPH ZOOM");
 CHECK(graph->visibleBars()==2&&click(graphZoom)&&graph->visibleBars()==4);
 auto* next=custom(906,"BAR >");
 auto* prev=custom(905,"< BAR");
 CHECK(click(next)&&click(prev));
 // Clear Pattern always reduces to only immutable point zero.
 auto* clear=custom(901,"CLEAR PATTERN");
 CHECK(click(clear)&&graph->pattern().size()==1);
 // Create a point using actual graph mouse dispatch then delete it.
 auto add=VSTGUI::MouseDownEvent(VSTGUI::CPoint(280,330),VSTGUI::MouseButton::Left);
 graph->onMouseDownEvent(add);
 auto release=VSTGUI::MouseUpEvent(VSTGUI::CPoint(280,330),VSTGUI::MouseButton::Left);
 graph->onMouseUpEvent(release);
 CHECK(add.consumed&&release.consumed&&graph->pattern().size()==2);
 auto* remove=custom(902,"DELETE POINT");
 CHECK(click(remove)&&graph->pattern().size()==1);
 // Numeric controls 908 and 909 must link to the same selected GUI point.
 auto second=VSTGUI::MouseDownEvent(VSTGUI::CPoint(360,360),VSTGUI::MouseButton::Left);
 graph->onMouseDownEvent(second);
 auto endSecond=VSTGUI::MouseUpEvent(VSTGUI::CPoint(360,360),VSTGUI::MouseButton::Left);
 graph->onMouseUpEvent(endSecond);
 CHECK(graph->pattern().size()==2);
 auto* posEdit=new VSTGUI::CTextEdit(VSTGUI::CRect(0,0,130,30),nullptr,908);
 auto* pitchEdit=new VSTGUI::CTextEdit(VSTGUI::CRect(0,0,130,30),nullptr,909);
 VSTGUI::UIAttributes plain;
 controller.verifyView(posEdit,plain,nullptr,nullptr);
 controller.verifyView(pitchEdit,plain,nullptr,nullptr);
 CHECK(posEdit->getListener()==&controller&&pitchEdit->getListener()==&controller);
 posEdit->setText(VSTGUI::UTF8String("1.2.120"));
 controller.valueChanged(posEdit);
 CHECK(graph->selectedTick()==1080);
 pitchEdit->setText(VSTGUI::UTF8String("+5,25"));
 controller.valueChanged(pitchEdit);
 CHECK(graph->selectedPoint()!=nullptr&&
       graph->selectedPoint()->semitones==5.25);
 // Reject a second point at tick zero, keeping the existing curve.
 posEdit->setText(VSTGUI::UTF8String("1.1.000"));
 controller.valueChanged(posEdit);
 CHECK(graph->selectedTick()==1080);
 // JUMP button must duplicate exactly the selected tick and preserve
 // ordered pair through controller packet -> processor -> project state.
 auto* jump=custom(910,"JUMP TO 0");
 CHECK(click(jump));
 CHECK(graph->pattern().size()==3&&graph->pattern().hasJumps());
 CHECK(graph->pattern().at(1).tick==graph->pattern().at(2).tick);
 CHECK(graph->pattern().at(1).semitones==5.25);
 CHECK(graph->pattern().at(2).semitones==0.0);
 auto jumped= pitchator::packCurve(graph->pattern());
 HostMessage jumpMessage;jumpMessage.setMessageID("125A.Pitchator.Curve");
 CHECK(jumpMessage.getAttributes()->setBinary("Curve",&jumped,
       static_cast<uint32>(sizeof(jumped)))==kResultOk);
 CHECK(routed->notify(&jumpMessage)==kResultOk);
 MemoryStream jumpSaved;CHECK(routed->getState(&jumpSaved)==kResultOk);
 CHECK(jumpSaved.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 auto jumpReopened=std::make_unique<PitchatorProcessor>();
 CHECK(jumpReopened->setState(&jumpSaved)==kResultOk);
 CHECK(jumpSaved.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk);
 CHECK(controller.setComponentState(&jumpSaved)==kResultOk);
 CHECK(graph->pattern().size()==3&&graph->pattern().hasJumps());
 CHECK(graph->pattern().at(1).tick==1080&&graph->pattern().at(2).tick==1080);
 jump->forget();
 controller.willClose(nullptr);
 zoom->forget();shape->forget();snap->forget();graphZoom->forget();
 next->forget();prev->forget();clear->forget();remove->forget();
 posEdit->forget();pitchEdit->forget();graph->forget();
 CHECK(controller.terminate()==kResultOk);
 // Overflow regression: 2049 incoming events must not leave the receiving
 // synthesizer on a stale channel bend when scheduler falls back to passthrough.
 auto overflowProcessor=std::make_unique<PitchatorProcessor>();
 EventList overflowStartIn(4),overflowStartOut(8);
 Event held{};held.type=Event::kNoteOnEvent;held.busIndex=0;
 held.sampleOffset=0;held.noteOn.channel=3;held.noteOn.pitch=60;
 held.noteOn.velocity=0.8f;held.noteOn.noteId=123;
 CHECK(overflowStartIn.addEvent(held)==kResultOk);
 ProcessContext overflowClock{};
 overflowClock.state=ProcessContext::kPlaying |
    ProcessContext::kProjectTimeMusicValid|ProcessContext::kTempoValid;
 overflowClock.projectTimeMusic=1.0;overflowClock.tempo=120;
 overflowClock.sampleRate=48000;
 ParameterChanges overflowHost(1);
 int32 ovIndex=0,ovPoint=0;
 auto* ovQueue=overflowHost.addParameterData(kPitchTrigger,ovIndex);
 CHECK(ovQueue!=nullptr);
 CHECK(ovQueue->addPoint(0,0.0,ovPoint)==kResultOk); // HOST SYNC for +3 st
 ProcessData first{};
 first.numSamples=64;first.inputEvents=&overflowStartIn;
 first.outputEvents=&overflowStartOut;
 first.inputParameterChanges=&overflowHost;
 first.processContext=&overflowClock;
 CHECK(overflowProcessor->process(first)==kResultOk);
 CHECK(overflowStartOut.getEventCount()>=2);
 Event activeBend{};
 CHECK(overflowStartOut.getEvent(0,activeBend)==kResultOk);
 CHECK(activeBend.type==Event::kLegacyMIDICCOutEvent);
 CHECK((static_cast<int>(activeBend.midiCCOut.value)&127)+
       ((static_cast<int>(activeBend.midiCCOut.value2)&127)<<7)!=8192);
 EventList overflowIn(4096),overflowOut(4096);
 Event extra{};extra.type=Event::kNoteOffEvent;extra.busIndex=0;
 extra.sampleOffset=0;extra.noteOff.channel=3;extra.noteOff.pitch=60;
 extra.noteOff.noteId=123;
 for(int i=0;i<2049;++i)CHECK(overflowIn.addEvent(extra)==kResultOk);
 ProcessData excess{};excess.numSamples=64;
 excess.inputEvents=&overflowIn;excess.outputEvents=&overflowOut;
 excess.processContext=&overflowClock;
 CHECK(overflowProcessor->process(excess)==kResultOk);
 CHECK(overflowOut.getEventCount()==2050); // bend reset + all 2049 source events
 Event centered{};
 CHECK(overflowOut.getEvent(0,centered)==kResultOk);
 CHECK(centered.type==Event::kLegacyMIDICCOutEvent);
 CHECK(centered.midiCCOut.channel==3);
 CHECK((static_cast<int>(centered.midiCCOut.value)&127)+
       ((static_cast<int>(centered.midiCCOut.value2)&127)<<7)==8192);
 std::puts("VST3 parameter flush, mouse toggles, host state, overflow and recall PASS");
 return 0;
}
int main(){
#ifdef _WIN32
 VSTGUI::init(GetModuleHandleW(nullptr));
#else
 VSTGUI::init(nullptr);
#endif
 const int result=runTests();
 VSTGUI::exit();
 return result;
}
