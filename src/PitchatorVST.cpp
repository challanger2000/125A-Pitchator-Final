#include "PitchatorVST.h"
#include "PitchCurveView.h"
#include "BrandLogoView.h"
#include "vstgui/uidescription/uiattributes.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/controls/ctextedit.h"
#include "vstgui/lib/cframe.h"
#include "PitchPointEdit.h"
#include "ScaleNoteNames.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include <cstring>
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "public.sdk/source/main/pluginfactory.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace Steinberg::Vst {
namespace {
template<class T> bool wr(IBStream* s,const T& v){
 int32 n=0;return s&&s->write(const_cast<T*>(&v),static_cast<int32>(sizeof(T)),&n)==kResultOk&&n==static_cast<int32>(sizeof(T));
}
template<class T> bool rd(IBStream* s,T& v){
 int32 n=0;return s&&s->read(&v,static_cast<int32>(sizeof(T)),&n)==kResultOk&&n==static_cast<int32>(sizeof(T));
}
int discrete(double v,int max){
 return std::clamp(static_cast<int>(std::lround(std::clamp(v,0.0,1.0)*max)),0,max);
}
struct Header {int32 bars=0,key=0,scale=0,range=0,lock=0,trigger=0,loop=0,bypass=0,count=0;uint32 version=kPitchStateVersion;};
bool readHeader(IBStream* s,Header& h){
 uint32 magic=0,version=0;
 if(!rd(s,magic)||!rd(s,version)||magic!=kPitchMagic||(version!=1&&version!=2&&version!=kPitchStateVersion)||
    !rd(s,h.bars)||!rd(s,h.key)||!rd(s,h.scale)||!rd(s,h.range)||
    !rd(s,h.lock)||!rd(s,h.trigger)||!rd(s,h.loop)||!rd(s,h.bypass)||!rd(s,h.count))return false;
 const bool valid=h.bars>=1&&h.bars<=16&&h.key>=0&&h.key<=11&&h.scale>=0&&h.scale<=10&&
 h.range>=1&&h.range<=48&&(h.lock==0||h.lock==1)&&
 (h.trigger>=0&&h.trigger<=(version==1?1:2))&&(h.loop==0||h.loop==1)&&
 (h.bypass==0||h.bypass==1)&&h.count>=1&&h.count<=512;
 // V1 stored mode 1 as NOTE RETRIGGER, i.e. normalized 1.0.
 // In V2 this remains the final dropdown choice (2), preserving
 // legacy automation lanes AND V1 project/component-state data.
 if(valid&&version==1&&h.trigger==1)h.trigger=2;
 h.version=version;
 return valid;
}
bool readPattern(IBStream* s,int bars,int count,uint32 version,pitchator::Pattern& p){
 if(!p.setBars(bars))return false;
 for(int i=0;i<count;++i){
  int32 tick=0,curve=0;double st=0;
  if(!rd(s,tick)||!rd(s,st)||!rd(s,curve)||curve<0||curve>2)return false;
  pitchator::Point point{tick,st,static_cast<pitchator::Curve>(curve)};
  if(i==0){
   if(point.tick!=0||point.semitones!=0||!p.setAt(0,point))return false;
  }else if(version>=3){
   if(!p.appendOrdered(point))return false;
  }else if(!p.upsert(point))return false;
 }
 return p.size()==static_cast<size_t>(count);
}
}
PitchatorProcessor::PitchatorProcessor(){
 setControllerClass(kPitchControllerUID);
 // Explicitly request tempo/PPQ from conforming VST3 hosts.
 processContextRequirements.needProjectTimeMusic().needTempo().needTransportState();
 pattern_.setBars(2);
 pattern_.upsert({960,3,pitchator::Curve::Step});
 pattern_.upsert({1920,1,pitchator::Curve::Linear});
 pattern_.upsert({2880,0,pitchator::Curve::Linear});
 publishedCurve_.publish(pattern_);
}
tresult PLUGIN_API PitchatorProcessor::initialize(FUnknown* ctx){
 auto r=AudioEffect::initialize(ctx);if(r!=kResultOk)return r;
 addEventInput(STR16("MIDI In"),16);addEventOutput(STR16("MIDI Out"),16);
 return kResultOk;
}
tresult PLUGIN_API PitchatorProcessor::setActive(TBool on){
 if(!on){scheduler_.reset();previouslyPlaying_=false;}
 return AudioEffect::setActive(on);
}
tresult PLUGIN_API PitchatorProcessor::setProcessing(TBool on){
 if(!on){scheduler_.reset();previouslyPlaying_=false;}
 return kResultOk; // AudioEffect base returns kNotImplemented.
}
void PitchatorProcessor::changeParam(ParamID id,ParamValue x) noexcept {
 switch(id){
 case kPitchKey:settings_.key=discrete(x,11);break;
 case kPitchScale:settings_.scale=static_cast<pitchator::Scale>(discrete(x,10));break;
 case kPitchBars: {
  const int bars=discrete(x,15)+1;
  currentBars_.store(bars,std::memory_order_relaxed);
  // Shrinking the timeline crops out-of-range points instead of leaving the
  // processor at a different length from its automatable parameter.
  while(!pattern_.setBars(bars)&&pattern_.size()>1){
   pattern_.erase(pattern_.at(pattern_.size()-1).tick);
  }
  pattern_.setBars(bars);
  break;
 }
 case kPitchRange:settings_.range=discrete(x,47)+1;break;
 case kPitchLock:settings_.lock=x>=0.5;break;
 case kPitchTrigger:settings_.trigger=static_cast<pitchator::TriggerMode>(discrete(x,2));break;
 case kPitchLoop:settings_.loop=x>=0.5;break;
 case kPitchBypass:settings_.bypass=x>=0.5;break;
 default:break;
 }
}
tresult PLUGIN_API PitchatorProcessor::process(ProcessData& d){
 // Atomic publish from GUI; the audio callback NEVER locks or allocates.
 const uint64_t latest=publishedCurve_.revision();
 if(latest!=usedCurveRevision_&&!(latest&1u)){
  pitchator::Pattern fresh;
  uint64_t observed=0;
  if(publishedCurve_.snapshot(fresh,observed)){
   pattern_=fresh;
   usedCurveRevision_=observed;
   const int bars=currentBars_.load(std::memory_order_relaxed);
   while(!pattern_.setBars(bars)&&pattern_.size()>1)
    pattern_.erase(pattern_.at(pattern_.size()-1).tick);
   pattern_.setBars(bars);
  }
 }
 // VST3 permits parameter-only flushes with zero samples, no audio buffers
 // and no event output list. The host uses these flushes for state/preset
 // persistence (including Steinberg's bypass persistence validator).
 // Apply the *last* point from each queue before returning.
 if(!d.outputEvents){
  if(d.inputParameterChanges){
   for(int32 i=0;i<d.inputParameterChanges->getParameterCount();++i){
    auto* q=d.inputParameterChanges->getParameterData(i);
    if(!q)continue;
    for(int32 j=0;j<q->getPointCount();++j){
     int32 at=0;ParamValue x=0;
     if(q->getPoint(j,at,x)==kResultOk)
      changeParam(q->getParameterId(),x);
    }
   }
  }
  scheduler_.endBlock(d.numSamples);
  return kResultOk;
 }
 size_t pc=0,ec=0;
 if(d.inputParameterChanges)
  for(int32 i=0;i<d.inputParameterChanges->getParameterCount();++i){
   auto* q=d.inputParameterChanges->getParameterData(i);if(!q)continue;
   for(int32 j=0;j<q->getPointCount();++j){
    int32 at=0;ParamValue x=0;if(q->getPoint(j,at,x)!=kResultOk)continue;
    if(pc>=changes_.size())return kResultFalse;
    Change c{std::clamp(at,0,std::max(0,d.numSamples-1)),q->getParameterId(),x};
    size_t pos=pc;
    while(pos>0&&changes_[pos-1].offset>c.offset){changes_[pos]=changes_[pos-1];--pos;}
    changes_[pos]=c;++pc;
   }
  }
 if(d.inputEvents)
  for(int32 i=0;i<d.inputEvents->getEventCount();++i){
   Event e{};if(d.inputEvents->getEvent(i,e)!=kResultOk)continue;
   if(ec>=inputs_.size()){
    // Overflow: preserve all incoming notes and explicitly center any
    // prior channel bends BEFORE resetting internal scheduler state.
    if(!scheduler_.center(0,[&](pitchator::Bend bend){
     Event reset{};reset.type=Event::kLegacyMIDICCOutEvent;
     reset.busIndex=0;reset.sampleOffset=bend.offset;
     reset.midiCCOut.channel=static_cast<int8>(bend.channel);
     reset.midiCCOut.controlNumber=static_cast<uint8>(kPitchBend);
     reset.midiCCOut.value=static_cast<int8>(bend.value&127u);
     reset.midiCCOut.value2=static_cast<int8>((bend.value>>7)&127u);
     return d.outputEvents->addEvent(reset)==kResultOk;
    }))return kResultFalse;
    scheduler_.reset();
    for(int32 k=0;k<d.inputEvents->getEventCount();++k){
     Event pass{};if(d.inputEvents->getEvent(k,pass)==kResultOk)
      if(d.outputEvents->addEvent(pass)!=kResultOk)return kResultFalse;
    }
    return kResultOk;
   }
   e.sampleOffset=std::clamp(e.sampleOffset,0,std::max(0,d.numSamples-1));
   size_t pos=ec;while(pos>0&&inputs_[pos-1].sampleOffset>e.sampleOffset){
    inputs_[pos]=inputs_[pos-1];--pos;
   }
   inputs_[pos]=e;++ec;
  }
 pitchator::Clock clock{};clock.rate=processSetup.sampleRate;
 if(d.processContext){
  const auto& c=*d.processContext;
  clock.rate=c.sampleRate;
  clock.playing=(c.state&ProcessContext::kPlaying)!=0;
  clock.valid=(c.state&ProcessContext::kProjectTimeMusicValid)!=0&&
              (c.state&ProcessContext::kTempoValid)!=0;
  clock.ppq=c.projectTimeMusic;clock.tempo=c.tempo;
 }
 auto sendBend=[&](pitchator::Bend b){
  Event e{};e.type=Event::kLegacyMIDICCOutEvent;e.busIndex=0;e.sampleOffset=b.offset;
  e.midiCCOut.channel=static_cast<int8>(b.channel);
  e.midiCCOut.controlNumber=static_cast<uint8>(kPitchBend);
  e.midiCCOut.value=static_cast<int8>(b.value&127u);
  e.midiCCOut.value2=static_cast<int8>((b.value>>7)&127u);
  return d.outputEvents->addEvent(e)==kResultOk;
 };
 if(previouslyPlaying_&&!clock.playing&&!scheduler_.center(0,sendBend))return kResultFalse;
 previouslyPlaying_=clock.playing;
 size_t pi=0,ei=0;
 for(int32 pos=0;pos<d.numSamples;++pos){
  while(pi<pc&&changes_[pi].offset==pos){
   changeParam(changes_[pi].id,changes_[pi].value);++pi;
  }
  while(ei<ec&&inputs_[ei].sampleOffset==pos){
   Event e=inputs_[ei++];
   bool isOn=e.type==Event::kNoteOnEvent&&e.noteOn.velocity>0;
   bool isOff=e.type==Event::kNoteOffEvent||
              (e.type==Event::kNoteOnEvent&&e.noteOn.velocity<=0);
   if(isOn){
    scheduler_.noteOn(e.noteOn.channel,e.noteOn.pitch,settings_,pos,clock,pattern_.lengthTicks());
    if(!scheduler_.render(pos,clock,pattern_,settings_,true,sendBend))return kResultFalse;
   }
   if(d.outputEvents->addEvent(e)!=kResultOk)return kResultFalse;
   if(isOff){
    int ch=e.type==Event::kNoteOffEvent?e.noteOff.channel:e.noteOn.channel;
    int pitch=e.type==Event::kNoteOffEvent?e.noteOff.pitch:e.noteOn.pitch;
    scheduler_.noteOff(ch,pitch);
    if(!scheduler_.render(pos,clock,pattern_,settings_,true,sendBend))return kResultFalse;
   }
  }
  const bool exactJump=pattern_.hasJumps()&&scheduler_.jumpDue(pos);
  if((pos%64==0||exactJump)&&
     !scheduler_.render(pos,clock,pattern_,settings_,exactJump,sendBend))
   return kResultFalse;
 }
 if(d.numSamples==0)for(size_t i=0;i<pc;++i)changeParam(changes_[i].id,changes_[i].value);
 scheduler_.endBlock(d.numSamples);
 return kResultOk;
}
tresult PLUGIN_API PitchatorProcessor::getState(IBStream* s){
 if(!s)return kInvalidArgument;
 pitchator::Pattern saved;
 uint64_t ignored=0;
 if(!publishedCurve_.snapshot(saved,ignored))return kResultFalse;
 const int bars=currentBars_.load(std::memory_order_relaxed);
 while(!saved.setBars(bars)&&saved.size()>1)
  saved.erase(saved.at(saved.size()-1).tick);
 if(!saved.setBars(bars))return kResultFalse;
 Header h{saved.bars(),settings_.key,static_cast<int32>(settings_.scale),
  settings_.range,settings_.lock?1:0,static_cast<int32>(settings_.trigger),
  settings_.loop?1:0,settings_.bypass?1:0,static_cast<int32>(saved.size())};
 if(!wr(s,kPitchMagic)||!wr(s,kPitchStateVersion)||!wr(s,h.bars)||!wr(s,h.key)||
    !wr(s,h.scale)||!wr(s,h.range)||!wr(s,h.lock)||!wr(s,h.trigger)||
    !wr(s,h.loop)||!wr(s,h.bypass)||!wr(s,h.count))return kResultFalse;
 for(int32 i=0;i<h.count;++i){
  const auto& p=saved.at(static_cast<size_t>(i));
  int32 kind=static_cast<int32>(p.toNext);
  if(!wr(s,p.tick)||!wr(s,p.semitones)||!wr(s,kind))return kResultFalse;
 }
 return kResultOk;
}
tresult PLUGIN_API PitchatorProcessor::setState(IBStream* s){
 Header h{};if(!readHeader(s,h))return kResultFalse;
 pitchator::Pattern restored{};
 if(!readPattern(s,h.bars,h.count,h.version,restored))return kResultFalse;
 pattern_=restored;
 publishedCurve_.publish(restored);
 usedCurveRevision_=0;
 currentBars_.store(h.bars,std::memory_order_relaxed);
 settings_={h.key,static_cast<pitchator::Scale>(h.scale),h.range,h.lock!=0,
  static_cast<pitchator::TriggerMode>(h.trigger),h.loop!=0,h.bypass!=0};
 scheduler_.reset();previouslyPlaying_=false;
 return kResultOk;
}
// VST3 IConnectionPoint messages are delivered on non-realtime threads.
// Invalid or incomplete snapshots cannot overwrite the last valid curve.
tresult PLUGIN_API PitchatorProcessor::notify(IMessage* message){
 if(!message)return kInvalidArgument;
 if(std::strcmp(message->getMessageID(),"125A.Pitchator.Curve")!=0)
  return AudioEffect::notify(message);
 const void* bytes=nullptr;
 uint32 size=0;
 auto* attrs=message->getAttributes();
 if(!attrs||attrs->getBinary("Curve",bytes,size)!=kResultOk||
    size!=sizeof(pitchator::CurvePacket)||!bytes)return kResultFalse;
 pitchator::CurvePacket packet{};
 std::memcpy(&packet,bytes,sizeof(packet));
 pitchator::Pattern candidate;
 if(!pitchator::unpackCurve(packet,candidate))return kResultFalse;
 publishedCurve_.publish(candidate);
 currentBars_.store(candidate.bars(),std::memory_order_relaxed);
 return kResultOk;
}
PitchatorController::PitchatorController(){
 guiPattern_.setBars(2);
 guiPattern_.upsert({960,3,pitchator::Curve::Step});
 guiPattern_.upsert({1920,1,pitchator::Curve::Linear});
 guiPattern_.upsert({2880,0,pitchator::Curve::Linear});
}
tresult PLUGIN_API PitchatorController::initialize(FUnknown* ctx){
 auto r=EditControllerEx1::initialize(ctx);if(r!=kResultOk)return r;
 auto* keys=new StringListParameter(STR16("Key"),kPitchKey);
 const char16_t* noteNames[]={STR16("C"),STR16("C#"),STR16("D"),STR16("D#"),
 STR16("E"),STR16("F"),STR16("F#"),STR16("G"),STR16("G#"),
 STR16("A"),STR16("A#"),STR16("B")};
 for(auto* n:noteNames)keys->appendString(n);parameters.addParameter(keys);
 auto* scales=new StringListParameter(STR16("Scale"),kPitchScale);
 const char16_t* scaleNames[]={STR16("Chromatic"),STR16("Major"),STR16("Natural Minor"),
 STR16("Harmonic Minor"),STR16("Major Pentatonic"),STR16("Minor Pentatonic"),
 STR16("Dorian"),STR16("Phrygian"),STR16("Lydian"),STR16("Mixolydian"),STR16("Locrian")};
 for(auto* n:scaleNames)scales->appendString(n);parameters.addParameter(scales);
 parameters.addParameter(new RangeParameter(STR16("Length"),kPitchBars,STR16("bars"),
  1,16,2,15,ParameterInfo::kCanAutomate));
 parameters.addParameter(new RangeParameter(STR16("Bend Range"),kPitchRange,STR16("st"),
  1,48,12,47,ParameterInfo::kCanAutomate));
 parameters.addParameter(new RangeParameter(STR16("Scale Lock"),kPitchLock,nullptr,
  0,1,0,1,ParameterInfo::kCanAutomate));
 auto* trig=new StringListParameter(STR16("Trigger"),kPitchTrigger,nullptr,0.5);
 trig->appendString(STR16("HOST SYNC"));
 trig->appendString(STR16("NOTE CONTINUE"));
 trig->appendString(STR16("NOTE RETRIGGER"));
 trig->setNormalized(0.5); // Apply default after StringListParameter options are complete.
 parameters.addParameter(trig);
 parameters.addParameter(new RangeParameter(STR16("Loop"),kPitchLoop,nullptr,
  0,1,1,1,ParameterInfo::kCanAutomate));
 parameters.addParameter(new RangeParameter(STR16("Bypass"),kPitchBypass,nullptr,
  0,1,0,1,ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass));
 return kResultOk;
}
tresult PLUGIN_API PitchatorController::setComponentState(IBStream* s){
 Header h{};if(!readHeader(s,h))return kResultFalse;
 pitchator::Pattern check{};if(!readPattern(s,h.bars,h.count,h.version,check))return kResultFalse;
 restoring_=true;
 guiPattern_=check;
 if(curveView_){curveView_->refresh();curveView_->setRange(h.range);}
 syncPointEditors();
 EditControllerEx1::setParamNormalized(kPitchKey,h.key/11.0);EditControllerEx1::setParamNormalized(kPitchScale,h.scale/10.0);
 EditControllerEx1::setParamNormalized(kPitchBars,(h.bars-1)/15.0);
 EditControllerEx1::setParamNormalized(kPitchRange,(h.range-1)/47.0);
 EditControllerEx1::setParamNormalized(kPitchLock,static_cast<double>(h.lock));
 EditControllerEx1::setParamNormalized(kPitchTrigger,h.trigger/2.0);
 EditControllerEx1::setParamNormalized(kPitchLoop,static_cast<double>(h.loop));
 EditControllerEx1::setParamNormalized(kPitchBypass,static_cast<double>(h.bypass));
 restoring_=false;
 syncToggleButtons();
 syncScaleNotes();
 return kResultOk;
}

// GUI lives on the controller thread; no heap work is performed in process().
bool PitchatorController::publishPattern(){
 auto* message=allocateMessage();
 if(!message)return false;
 message->setMessageID("125A.Pitchator.Curve");
 const auto packet=pitchator::packCurve(guiPattern_);
 auto* attrs=message->getAttributes();
 const bool ok=attrs&&attrs->setBinary("Curve",&packet,
     static_cast<uint32>(sizeof(packet)))==kResultOk&&sendMessage(message)==kResultOk;
 message->release();
 return ok;
}
tresult PLUGIN_API PitchatorController::setParamNormalized(ParamID id,ParamValue normalized){
 const auto result=EditControllerEx1::setParamNormalized(id,normalized);
 if(id==kPitchBars&&!restoring_){
  const int bars=discrete(normalized,15)+1;
  while(!guiPattern_.setBars(bars)&&guiPattern_.size()>1)
   guiPattern_.erase(guiPattern_.at(guiPattern_.size()-1).tick);
  guiPattern_.setBars(bars);
  if(curveView_){curveView_->refresh();publishPattern();syncPointEditors();}
 }
 if(id==kPitchRange&&curveView_)curveView_->setRange(discrete(normalized,47)+1);
 if(id==kPitchLock||id==kPitchLoop||id==kPitchBypass)syncToggleButtons();
 if(id==kPitchKey||id==kPitchScale)syncScaleNotes();
 return result;
}
IPlugView* PLUGIN_API PitchatorController::createView(FIDString name){
 if(!name||std::strcmp(name,ViewType::kEditor)!=0)return nullptr;
 auto* editor=new VSTGUI::VST3Editor(this,"PitchatorView","pitchator.uidesc");
 editor->setDelegate(this);
 editor->setAllowedZoomFactors({1.0,1.5});
 editor->setZoomFactor(zoom_);
 return editor;
}
VSTGUI::CView* PitchatorController::verifyView(
          VSTGUI::CView* view,const VSTGUI::UIAttributes& attrs,
          const VSTGUI::IUIDescription*,VSTGUI::VST3Editor* editor){
 activeEditor_=editor;
 if(auto* id=attrs.getAttributeValue("pitchator-id")){
  if(*id=="logo")return new pitchator::BrandLogoView(view->getViewSize());
  if(*id=="scale-notes"){
   scaleNotesLabel_=dynamic_cast<VSTGUI::CTextLabel*>(view);
   if(scaleNotesLabel_)scaleNotesLabel_->setMouseEnabled(false);
   syncScaleNotes();
   return view;
  }
  if(*id=="curve"){
   auto* graph=new pitchator::PitchCurveView(view->getViewSize(),guiPattern_,
                       [this]{return publishPattern();},[this]{syncPointEditors();});
   curveView_=graph;
   curveView_->setRange(discrete(getParamNormalized(kPitchRange),47)+1);
   return graph;
  }
 }
 if(auto* control=dynamic_cast<VSTGUI::CControl*>(view)){
  const int tag=control->getTag();
  // Registered VST parameters 100..103 and 105 stay on Steinberg's
  // VST3Editor parameter binding, including menu/text edits/automation.
  // Three toggles have explicit gesture+host edit routing so they cannot
  // silently end up as inert CTextButtons on Studio One.
  const bool toggle=(tag==kPitchLock||tag==kPitchLoop||tag==kPitchBypass);
  if(toggle||(tag>=900&&tag<=910))control->setListener(this);
  if(auto* button=dynamic_cast<VSTGUI::CTextButton*>(control)){
   // VSTGUI CTextButton defaults to KICK style: one mouse click calls
   // valueChanged(ON) then immediately valueChanged(OFF). This was why
   // Scale Lock, Loop and Bypass could never remain engaged in Studio One.
   // They MUST be persistent ON/OFF buttons, whereas action buttons kick.
   button->setStyle(toggle?VSTGUI::CTextButton::kOnOffStyle:
                          VSTGUI::CTextButton::kKickStyle);
   // CTextButton has LIGHT idle gradient by default; the old light text
   // was practically invisible, especially for ON and disabled states.
   button->setTextColor(VSTGUI::CColor(20,28,35,255));
   button->setTextColorHighlighted(VSTGUI::CColor(255,255,255,255));
   if(tag==kPitchLock)lockButton_=button;
   if(tag==kPitchLoop)loopButton_=button;
   if(tag==kPitchBypass)bypassButton_=button;
   if(tag==900)zoomButton_=button;
   if(tag==903)shapeButton_=button;
   if(tag==904)snapButton_=button;
  }
  if(toggle)syncToggleButtons();
  if(tag==908)positionEdit_=dynamic_cast<VSTGUI::CTextEdit*>(control);
  if(tag==909)pitchEdit_=dynamic_cast<VSTGUI::CTextEdit*>(control);
  if(tag==908||tag==909)syncPointEditors();
 }
 return view;
}
void PitchatorController::willClose(VSTGUI::VST3Editor* editor){
 if(editor)zoom_=editor->getZoomFactor();
 activeEditor_=nullptr;curveView_=nullptr;zoomButton_=nullptr;
 shapeButton_=nullptr;snapButton_=nullptr;
 lockButton_=nullptr;loopButton_=nullptr;bypassButton_=nullptr;
 positionEdit_=nullptr;pitchEdit_=nullptr;scaleNotesLabel_=nullptr;
}
void PitchatorController::syncScaleNotes(){
 if(!scaleNotesLabel_)return;
 const int key=discrete(getParamNormalized(kPitchKey),11);
 const auto scale=static_cast<pitchator::Scale>(discrete(getParamNormalized(kPitchScale),10));
 scaleNotesLabel_->setText(VSTGUI::UTF8String(pitchator::scaleNoteNames(key,scale)));
 scaleNotesLabel_->invalid();
}
void PitchatorController::syncToggleButtons(){
 auto sync=[this](VSTGUI::CTextButton* button,ParamID id,
                   const char* on,const char* off){
  if(!button)return;
  const bool enabled=getParamNormalized(id)>=0.5;
  button->setValueNormalized(enabled?1.f:0.f);
  button->setTitle(enabled?on:off);
  button->invalid();
 };
 sync(lockButton_,kPitchLock,"LOCK ON","LOCK OFF");
 sync(loopButton_,kPitchLoop,"LOOP ON","LOOP OFF");
 sync(bypassButton_,kPitchBypass,"BYPASS ON","BYPASS OFF");
}
void PitchatorController::controlBeginEdit(VSTGUI::CControl* control){
 if(!control)return;
 const int tag=control->getTag();
 if(tag==kPitchLock||tag==kPitchLoop||tag==kPitchBypass)
  EditControllerEx1::beginEdit(static_cast<ParamID>(tag));
}
void PitchatorController::controlEndEdit(VSTGUI::CControl* control){
 if(!control)return;
 const int tag=control->getTag();
 if(tag==kPitchLock||tag==kPitchLoop||tag==kPitchBypass)
  EditControllerEx1::endEdit(static_cast<ParamID>(tag));
}
void PitchatorController::syncPointEditors(){
 if(!curveView_)return;
 const auto* p=curveView_->selectedPoint();
 if(!p)return;
 char position[32],pitch[32];
 pitchator::edit::formatPosition(p->tick,position);
 pitchator::edit::formatSemitones(p->semitones,pitch);
 // Avoid replacing text while the user is typing into a native edit control.
 if(positionEdit_&&!positionEdit_->getPlatformTextEdit())
  positionEdit_->setText(VSTGUI::UTF8String(position));
 if(pitchEdit_&&!pitchEdit_->getPlatformTextEdit())
  pitchEdit_->setText(VSTGUI::UTF8String(pitch));
}
void PitchatorController::valueChanged(VSTGUI::CControl* control){
 if(!control)return;
 const int tag=control->getTag();
 if(tag==kPitchLock||tag==kPitchLoop||tag==kPitchBypass){
  // Explicit normalized control -> controller -> host parameter route.
  // Treat both 0 and 1 as real edits (never ignore the OFF transition).
  const ParamValue next=control->getValueNormalized()>=0.5?1.0:0.0;
  EditControllerEx1::setParamNormalized(static_cast<ParamID>(tag),next);
  EditControllerEx1::performEdit(static_cast<ParamID>(tag),next);
  syncToggleButtons();
  return;
 }
 if(tag==908||tag==909){
  auto* field=dynamic_cast<VSTGUI::CTextEdit*>(control);
  if(!field||!curveView_)return;
  bool ok=false;
  if(tag==908){
   int32_t tick=-1;
   ok=pitchator::edit::parsePosition(field->getText().data(),
                                    guiPattern_.bars(),tick)&&
      curveView_->editSelectedTick(tick);
  }else{
   double semitones=0;
   ok=pitchator::edit::parseSemitones(field->getText().data(),semitones)&&
      curveView_->editSelectedPitch(semitones);
  }
  // Commit on focus loss, restore valid text after rejection.
  if(!ok){
   // Restore the accepted value even while the platform text box has focus.
   const auto* selected=curveView_->selectedPoint();
   if(selected){
    char canonical[32];
    if(tag==908)pitchator::edit::formatPosition(selected->tick,canonical);
    else pitchator::edit::formatSemitones(selected->semitones,canonical);
    field->setText(VSTGUI::UTF8String(canonical));
   }
  }
  return;
 }
 if(control->getValueNormalized()<=0.5f)return;
 if(tag==900){
  zoom_=zoom_<1.25?1.5:1.0;
  if(activeEditor_)activeEditor_->setZoomFactor(zoom_);
  if(zoomButton_)zoomButton_->setTitle(zoom_>1.25?"150 %":"100 %");
  return;
 }
 if(!curveView_)return;
 switch(tag){
 case 901:curveView_->clearCurve();break;
 case 902:curveView_->eraseSelected();break;
 case 903:{
  const auto current=curveView_->curve();
  const auto next=current==pitchator::Curve::Step?pitchator::Curve::Linear:
                  current==pitchator::Curve::Linear?pitchator::Curve::Smooth:
                  pitchator::Curve::Step;
  curveView_->setCurve(next);
  if(shapeButton_)shapeButton_->setTitle(next==pitchator::Curve::Step?"CURVE: STEP":
                       next==pitchator::Curve::Smooth?"CURVE: SMOOTH":"CURVE: LINEAR");
  break;
 }
 case 904:
  curveView_->setSnap(!curveView_->snap());
  if(snapButton_)snapButton_->setTitle(curveView_->snap()?"SNAP ON":"SNAP OFF");
  break;
 case 905:curveView_->scrollBars(-1);break;
 case 906:curveView_->scrollBars(1);break;
 case 907:curveView_->cycleVisibleBars();break;
 case 910:curveView_->addJumpSelected();break;
 default:break;
 }
}
} // namespace Steinberg::Vst
using namespace Steinberg;
using namespace Steinberg::Vst;
#ifndef PITCHATOR_NO_FACTORY
BEGIN_FACTORY_DEF("125A Systems","https://github.com/challanger2000","")
 DEF_CLASS2(INLINE_UID_FROM_FUID(kPitchProcessorUID),PClassInfo::kManyInstances,
  kVstAudioEffectClass,"125A Pitchator",Vst::kDistributable,Vst::PlugType::kInstrumentSynth,"1.0.0",kVstVersionString,
  PitchatorProcessor::createInstance)
 DEF_CLASS2(INLINE_UID_FROM_FUID(kPitchControllerUID),PClassInfo::kManyInstances,
  kVstComponentControllerClass,"125A Pitchator Controller",0,"","1.0.0",
  kVstVersionString,PitchatorController::createInstance)
END_FACTORY

#endif // PITCHATOR_NO_FACTORY
