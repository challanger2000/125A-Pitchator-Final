#pragma once
#include "PitchEngine.h"
#include "vstgui/lib/cview.h"
#include <functional>
#include <algorithm>
#include "PitchPointEdit.h"

namespace pitchator {
class PitchCurveView final : public VSTGUI::CView {
public:
 PitchCurveView(const VSTGUI::CRect& bounds, Pattern& pattern,
                std::function<bool()> publish,std::function<void()> selectionChanged);
 void draw(VSTGUI::CDrawContext* ctx) override;
 void onMouseDownEvent(VSTGUI::MouseDownEvent& event) override;
 void onMouseMoveEvent(VSTGUI::MouseMoveEvent& event) override;
 void onMouseUpEvent(VSTGUI::MouseUpEvent& event) override;
 void onMouseCancelEvent(VSTGUI::MouseCancelEvent& event) override;
 void scrollBars(int delta);
 void cycleVisibleBars();
 void setSnap(bool enabled) {snap_=enabled;invalid();}
 bool snap() const {return snap_;}
 void setCurve(Curve shape);
 Curve curve() const {return selectedShape_;}
 bool eraseSelected();
 void clearCurve();
 bool addJumpSelected();
 void refresh() {selectedIndex_=0;invalid();}
 void setRange(int range){pitchRange_=std::clamp(range,1,48);invalid();}
 const Point* selectedPoint() const noexcept;
 bool editSelectedTick(int32_t tick);
 bool editSelectedPitch(double semitones);
 int32_t selectedTick()const noexcept {const auto* p=selectedPoint();return p?p->tick:0;}
 // Read-only hostless GUI integration-test probes, never mutate state.
 const Pattern& pattern() const noexcept {return pattern_;}
 int visibleBars()const noexcept {return viewBars_;}
 int scrollBar()const noexcept {return scrollBar_;}
private:
 Pattern& pattern_;
 std::function<bool()> publish_;
 std::function<void()> selectionChanged_;
 bool snap_=true;
 bool dragging_=false;
 bool dirty_=false;
 bool syncError_=false;
 int viewBars_=2,scrollBar_=0,pitchRange_=12;
 size_t selectedIndex_=0;
 Curve selectedShape_=Curve::Linear;
 double left() const;
 double top() const;
 double width() const;
 double height() const;
 int32_t xToTick(double x) const;
 double tickToX(double tick) const;
 double yToSemitone(double y) const;
 double semitoneToY(double value) const;
 double pointerX(const VSTGUI::CPoint& p) const;
 double pointerY(const VSTGUI::CPoint& p) const;
 bool eraseNear(double x,double y);
 void commit();
};
} // namespace pitchator
