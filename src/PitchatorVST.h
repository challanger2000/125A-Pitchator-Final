#pragma once
#include "PitchScheduler.h"
#include "CurveMailbox.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/lib/controls/icontrollistener.h"
#include <atomic>
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include <array>
#include "DemoTimer.h"
namespace VSTGUI {class CControl; class CTextButton; class CTextEdit; class CTextLabel;}
namespace pitchator {class PitchCurveView;}
namespace Steinberg::Vst {
enum PitchParam:ParamID {
 kPitchKey=100,kPitchScale=101,kPitchBars=102,kPitchRange=103,
 kPitchLock=104,kPitchTrigger=105,kPitchLoop=106,kPitchBypass=107
};
static const FUID kPitchProcessorUID(0x125A4501,0x50495443,0x4841544F,0x52010001);
static const FUID kPitchControllerUID(0x125A4502,0x50495443,0x4841544F,0x52010001);
constexpr uint32_t kPitchMagic=0x31544350u;
constexpr uint32_t kPitchStateVersion=3u;
class PitchatorProcessor final:public AudioEffect {
public:
 PitchatorProcessor();
 static FUnknown* createInstance(void*){return static_cast<IAudioProcessor*>(new PitchatorProcessor);}
 tresult PLUGIN_API initialize(FUnknown*) override;
 tresult PLUGIN_API setActive(TBool) override;
 tresult PLUGIN_API setProcessing(TBool) override;
 tresult PLUGIN_API process(ProcessData&) override;
 tresult PLUGIN_API getState(IBStream*) override;
 tresult PLUGIN_API setState(IBStream*) override;
 tresult PLUGIN_API notify(IMessage*) override;
private:
 struct Change {int32 offset=0;ParamID id=0;ParamValue value=0;};
 std::array<Change,1024> changes_{};
 std::array<Event,2048> inputs_{};
 pitchator::Pattern pattern_{};
 pitchator::CurveMailbox publishedCurve_{pattern_};
 uint64_t usedCurveRevision_=0;
 std::atomic<int> currentBars_{2};
 pitchator::Settings settings_{};
 pitchator::Scheduler scheduler_{};
 bool previouslyPlaying_=false;
 pitchator::DemoTimer demoTimer_{};
 std::array<uint8_t,16*128> demoHeld_{};
 std::array<uint8_t,16> demoChannelUsed_{};
 bool demoCleanupSent_=false;
 void changeParam(ParamID,ParamValue) noexcept;
 void demoTrack(const Event&) noexcept;
 bool demoCleanup(IEventList*,int32) noexcept;
};
class PitchatorController final: public EditControllerEx1,
                                public VSTGUI::VST3EditorDelegate,
                                public VSTGUI::IControlListener {
public:
 PitchatorController();
 static FUnknown* createInstance(void*){return static_cast<IEditController*>(new PitchatorController);}
 tresult PLUGIN_API initialize(FUnknown*) override;
 tresult PLUGIN_API setComponentState(IBStream*) override;
 tresult PLUGIN_API setParamNormalized(ParamID,ParamValue) override;
 IPlugView* PLUGIN_API createView(FIDString) override;
 VSTGUI::CView* verifyView(VSTGUI::CView*,const VSTGUI::UIAttributes&,
                           const VSTGUI::IUIDescription*,VSTGUI::VST3Editor*) override;
 void willClose(VSTGUI::VST3Editor*) override;
 void valueChanged(VSTGUI::CControl*) override;
 void controlBeginEdit(VSTGUI::CControl*) override;
 void controlEndEdit(VSTGUI::CControl*) override;
 bool publishPattern();
 void syncPointEditors();
 void syncToggleButtons();
 void syncScaleNotes();
private:
 pitchator::Pattern guiPattern_{};
 pitchator::PitchCurveView* curveView_=nullptr;
 VSTGUI::VST3Editor* activeEditor_=nullptr;
 VSTGUI::CTextButton* zoomButton_=nullptr;
 VSTGUI::CTextButton* shapeButton_=nullptr;
 VSTGUI::CTextButton* snapButton_=nullptr;
 VSTGUI::CTextButton* lockButton_=nullptr;
 VSTGUI::CTextButton* loopButton_=nullptr;
 VSTGUI::CTextButton* bypassButton_=nullptr;
 VSTGUI::CTextEdit* positionEdit_=nullptr;
 VSTGUI::CTextEdit* pitchEdit_=nullptr;
 VSTGUI::CTextLabel* scaleNotesLabel_=nullptr;
 bool restoring_=false;
 double zoom_=1.0;
};
}
