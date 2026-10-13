#include "PitchCurveView.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/events.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace VSTGUI;
namespace pitchator {
namespace {
constexpr CColor kBackground(16,24,31,255);
constexpr CColor kPanel(22,33,41,255);
constexpr CColor kGrid(54,75,85,255);
constexpr CColor kBar(88,114,123,255);
constexpr CColor kLine(48,201,178,255);
constexpr CColor kNode(93,171,237,255);
constexpr CColor kNodeSelected(195,223,251,255);
constexpr CColor kText(211,225,226,255);
constexpr CColor kWarning(239,138,111,255);
}
PitchCurveView::PitchCurveView(const CRect& bounds,Pattern& pattern,std::function<bool()> publish,std::function<void()> selected)
:CView(bounds),pattern_(pattern),publish_(std::move(publish)),selectionChanged_(std::move(selected)) {setMouseEnabled(true);}
double PitchCurveView::left() const {return getViewSize().left+55;}
double PitchCurveView::top() const {return getViewSize().top+28;}
double PitchCurveView::width() const {return std::max(1.0,getWidth()-76.0);}
double PitchCurveView::height() const {return std::max(1.0,getHeight()-66.0);}
double PitchCurveView::tickToX(double t) const {
 return left()+(t-scrollBar_*kTicksPerBar)*width()/(viewBars_*kTicksPerBar);
}
double PitchCurveView::semitoneToY(double pitch) const {
 return top()+(pitchRange_-std::clamp(pitch,-static_cast<double>(pitchRange_),static_cast<double>(pitchRange_)))*height()/(2.0*pitchRange_);
}
int32_t PitchCurveView::xToTick(double x) const {
 double t=scrollBar_*kTicksPerBar+(x-left())*(viewBars_*kTicksPerBar)/width();
 t=std::clamp(t,0.0,static_cast<double>(pattern_.lengthTicks()-1));
 if(snap_) t=std::round(t/120.0)*120.0;
 return std::min(pattern_.lengthTicks()-1,static_cast<int32_t>(t));
}
double PitchCurveView::yToSemitone(double y) const {
 double p=pitchRange_-(y-top())*(2.0*pitchRange_)/height();
 // Vertical grid intentionally has integer semitone precision in V1.
 return std::round(std::clamp(p,-static_cast<double>(pitchRange_),static_cast<double>(pitchRange_)));
}
// VSTGUI CViewContainer has ALREADY converted a mouse event's frame
// coordinates into this view's PARENT coordinate system before dispatch.
// getViewSize(), left() and top() are in the SAME parent coordinate system.
// Calling frameToLocal() here then adding the view origin again double-
// transforms the pointer (and rejects every click in Studio One).
double PitchCurveView::pointerX(const CPoint& pos) const { return pos.x; }
double PitchCurveView::pointerY(const CPoint& pos) const { return pos.y; }
void PitchCurveView::draw(CDrawContext* dc){
 // Full redraws must initialize the font explicitly. In VSTGUI partial
 // invalidation the same draw context can inherit a different control's
 // font or no valid font, causing grid/status text to disappear.
 dc->setFont(kNormalFontSmall);
 const auto rect=getViewSize();
 dc->setFillColor(kBackground);dc->drawRect(rect,kDrawFilled);
 dc->setFillColor(kPanel);
 dc->drawRect(CRect(left(),top(),left()+width(),top()+height()),kDrawFilled);
 dc->setLineWidth(1);
 const int pitchGridStep=pitchRange_<=12?1:pitchRange_<=24?2:4;
 for(int st=-pitchRange_;st<=pitchRange_;st+=pitchGridStep){
  const double y=semitoneToY(st);
  dc->setFrameColor(st==0?kBar:kGrid);
  dc->drawLine(CPoint(left(),y),CPoint(left()+width(),y));
  if(st==0||st%std::max(3,pitchRange_/4)==0){
   char label[32];std::snprintf(label,sizeof(label),"%+d",st);
   dc->setFontColor(kText);
   dc->drawString(label,CRect(rect.left+6,y-10,rect.left+48,y+10),kRightText);
  }
 }
 const int bars=pattern_.bars();
 const int last=std::min(bars,scrollBar_+viewBars_);
 for(int b=scrollBar_;b<=last;++b){
  double x=tickToX(b*kTicksPerBar);
  dc->setFrameColor(kBar);dc->drawLine(CPoint(x,top()),CPoint(x,top()+height()));
  if(b<bars){
   char n[24];std::snprintf(n,sizeof(n),"BAR %d",b+1);
   dc->setFontColor(kText);dc->drawString(n,CRect(x+4,rect.top+2,x+90,top()-2),kLeftText);
  }
  if(b<last)for(int beat=1;beat<4;++beat){
   x=tickToX(b*kTicksPerBar+beat*kTicksPerQuarter);
   dc->setFrameColor(kGrid);dc->drawLine(CPoint(x,top()),CPoint(x,top()+height()));
  }
 }
 dc->setFrameColor(kLine);dc->setLineWidth(2.2);
 // Draw individual knot segments, never interpolate through a JUMP.
 // Two same-tick knots generate an exact VERTICAL line and two nodes.
 const double first=scrollBar_*kTicksPerBar;
 const double lastTick=(scrollBar_+viewBars_)*kTicksPerBar;
 for(size_t idx=1;idx<pattern_.size();++idx){
  const auto& a=pattern_.at(idx-1);
  const auto& b=pattern_.at(idx);
  if(b.tick==a.tick){
   if(a.tick>=first&&a.tick<=lastTick)
    dc->drawLine(CPoint(tickToX(a.tick),semitoneToY(a.semitones)),
                 CPoint(tickToX(b.tick),semitoneToY(b.semitones)));
   continue;
  }
  const double lo=std::max<double>(a.tick,first);
  const double hi=std::min<double>(b.tick,lastTick);
  if(hi<=lo)continue;
  auto sample=[&](double t){
   return interpolate(a.semitones,b.semitones,
     (t-a.tick)/(b.tick-a.tick),a.toNext);
  };
  if(a.toNext==Curve::Step){
   const double y=semitoneToY(a.semitones);
   dc->drawLine(CPoint(tickToX(lo),y),CPoint(tickToX(hi),y));
   if(hi==b.tick&&b.semitones!=a.semitones)
    dc->drawLine(CPoint(tickToX(hi),y),
                 CPoint(tickToX(hi),semitoneToY(b.semitones)));
   continue;
  }
  const int segments=a.toNext==Curve::Smooth?
    std::clamp(static_cast<int>(std::ceil((hi-lo)/60.0)),2,64):1;
  double prev=lo;
  for(int j=1;j<=segments;++j){
   const double at=lo+(hi-lo)*j/segments;
   dc->drawLine(CPoint(tickToX(prev),semitoneToY(sample(prev))),
                CPoint(tickToX(at),semitoneToY(sample(at))));
   prev=at;
  }
 }
 const auto& tail=pattern_.at(pattern_.size()-1);
 if(tail.tick<lastTick){
  const double hi=std::min<double>(pattern_.lengthTicks(),lastTick);
  const double lo=std::max<double>(tail.tick,first);
  if(hi>lo)
   dc->drawLine(CPoint(tickToX(lo),semitoneToY(tail.semitones)),
                CPoint(tickToX(hi),semitoneToY(tail.semitones)));
 }
 for(size_t i=0;i<pattern_.size();++i){
  const auto& p=pattern_.at(i);
  if(p.tick<scrollBar_*kTicksPerBar||p.tick>(scrollBar_+viewBars_)*kTicksPerBar)continue;
  const double x=tickToX(p.tick),y=semitoneToY(p.semitones);
  dc->setFillColor(i==selectedIndex_?kNodeSelected:kNode);
  dc->setFrameColor(kBackground);
  dc->drawEllipse(CRect(x-6,y-6,x+6,y+6),kDrawFilledAndStroked);
 }
 const Point* selected=selectedPoint();
 if(!selected)selected=&pattern_.at(0);
 char status[128];
 std::snprintf(status,sizeof(status),"POSITION %d.%d.%03d  |  PITCH %+0.1f ST  |  %s  |  %s",
  selected->tick/kTicksPerBar+1,
  (selected->tick%kTicksPerBar)/kTicksPerQuarter+1,
  selected->tick%kTicksPerQuarter,
  selected->semitones,
  selected->toNext==Curve::Step?"STEP":selected->toNext==Curve::Smooth?"SMOOTH":"LINEAR",
  snap_?"SNAP ON":"SNAP OFF");
 dc->setFontColor(syncError_?kWarning:kText);
 dc->drawString(syncError_?"CURVE NOT SYNCED - CHECK PLUGIN CONNECTION":status,
                CRect(rect.left+10,rect.bottom-28,rect.right-10,rect.bottom-4),kLeftText);
}
void PitchCurveView::commit(){
 if(dirty_){syncError_=!publish_();dirty_=false;}
 invalid();
}
bool PitchCurveView::eraseNear(double x,double y){
 if(x<left()-18||x>left()+width()+18||
    y<top()-18||y>top()+height()+18)return false;
 size_t nearest=pattern_.size();double distance2=18.0*18.0;
 for(size_t i=1;i<pattern_.size();++i){
  const auto& p=pattern_.at(i);
  const double dx=x-tickToX(p.tick),dy=y-semitoneToY(p.semitones);
  const double d=dx*dx+dy*dy;
  if(d<distance2){distance2=d;nearest=i;}
 }
 if(nearest==pattern_.size()||!pattern_.eraseAt(nearest))return false;
 dirty_=true;selectedIndex_=0;commit();
 if(selectionChanged_)selectionChanged_();
 return true;
}
bool PitchCurveView::addJumpSelected(){
 if(selectedIndex_==0||selectedIndex_>=pattern_.size())return false;
 const auto& selected=pattern_.at(selectedIndex_);
 const int32_t tick=selected.tick;
 // Each timestamp supports precisely one incoming and one outgoing point.
 for(size_t i=1;i<pattern_.size();++i)
  if(i!=selectedIndex_&&pattern_.at(i).tick==tick)return false;
 if(!pattern_.addJump({tick,0.0,Curve::Linear}))return false;
 ++selectedIndex_;dirty_=true;commit();
 if(selectionChanged_)selectionChanged_();
 return true;
}
void PitchCurveView::onMouseDownEvent(MouseDownEvent& e){
 const double x=pointerX(e.mousePosition),y=pointerY(e.mousePosition);
 if(x<left()||x>left()+width()||y<top()-10||y>top()+height()+10)return;
 const bool controlDelete=e.buttonState.isLeft()&&
                           e.modifiers.has(ModifierKey::Control);
 if(e.buttonState.isRight()||controlDelete){
  eraseNear(x,y);e.consumed=true;return;
 }
 if(!e.buttonState.isLeft())return;
 size_t nearest=pattern_.size();double minDistance2=130.0;
 for(size_t i=0;i<pattern_.size();++i){
  const auto& p=pattern_.at(i);
  const double dx=x-tickToX(p.tick),dy=y-semitoneToY(p.semitones);
  const double d=dx*dx+dy*dy;
  if(d<minDistance2){minDistance2=d;nearest=i;}
 }
 if(e.modifiers.has(ModifierKey::Shift)){
  // Shift+LeftClick creates the right-hand jump dot at the existing X,
  // independent of the click's Y. A second point is never shifted in time.
  size_t nearTick=pattern_.size();double dxBest=12.0;
  for(size_t i=1;i<pattern_.size();++i){
   const double dx=std::abs(x-tickToX(pattern_.at(i).tick));
   if(dx<dxBest){dxBest=dx;nearTick=i;}
  }
  if(nearTick<pattern_.size()){
   selectedIndex_=nearTick;
   if(pattern_.addJump({pattern_.at(nearTick).tick,yToSemitone(y),
                        selectedShape_})){
    ++selectedIndex_;dirty_=true;commit();
   }
   if(selectionChanged_)selectionChanged_();
   invalid();e.consumed=true;return;
  }
 }
 if(nearest<pattern_.size())selectedIndex_=nearest;
 else {
  const int32_t tick=xToTick(x);
  double pitch=yToSemitone(y);
  if(tick==0)pitch=0;
  if(pattern_.upsert({tick,pitch,selectedShape_})){
   for(size_t i=0;i<pattern_.size();++i)
    if(pattern_.at(i).tick==tick)selectedIndex_=i;
   dirty_=true;
  }
 }
 dragging_=true;if(selectionChanged_)selectionChanged_();
 invalid();e.consumed=true;
}
void PitchCurveView::onMouseMoveEvent(MouseMoveEvent& e){
 if(!dragging_)return;
 if(selectedIndex_==0||selectedIndex_>=pattern_.size()){
  e.consumed=true;return;
 }
 const double x=pointerX(e.mousePosition),y=pointerY(e.mousePosition);
 const int32_t tick=xToTick(std::clamp(x,left(),left()+width()));
 const double value=yToSemitone(std::clamp(y,top(),top()+height()));
 if(tick==0){e.consumed=true;return;}
 const auto selected=pattern_.at(selectedIndex_);
 size_t newIndex=selectedIndex_;
 if(pattern_.moveAt(selectedIndex_,{tick,value,selected.toNext},newIndex)){
  selectedIndex_=newIndex;dirty_=true;invalid();
 }
 e.consumed=true;
}
void PitchCurveView::onMouseUpEvent(MouseUpEvent& e){
 if(!dragging_)return;
 dragging_=false;commit();if(selectionChanged_)selectionChanged_();e.consumed=true;
}
void PitchCurveView::onMouseCancelEvent(MouseCancelEvent& e){
 if(dragging_){dragging_=false;commit();e.consumed=true;}
}
void PitchCurveView::scrollBars(int delta){
 scrollBar_=std::clamp(scrollBar_+delta,0,std::max(0,pattern_.bars()-viewBars_));
 invalid();
}
void PitchCurveView::cycleVisibleBars(){
 viewBars_=viewBars_==1?2:viewBars_==2?4:viewBars_==4?8:viewBars_==8?16:1;
 scrollBar_=std::clamp(scrollBar_,0,std::max(0,pattern_.bars()-viewBars_));
 invalid();
}
void PitchCurveView::setCurve(Curve shape){
 selectedShape_=shape;
 if(selectedIndex_<pattern_.size()){
  Point p=pattern_.at(selectedIndex_);p.toNext=shape;
  if(pattern_.setAt(selectedIndex_,p)){dirty_=true;commit();}
 }
 invalid();
}
bool PitchCurveView::eraseSelected(){
 if(!pattern_.eraseAt(selectedIndex_))return false;
 selectedIndex_=0;dirty_=true;commit();
 if(selectionChanged_)selectionChanged_();return true;
}
void PitchCurveView::clearCurve(){
 pattern_.clear();selectedIndex_=0;dirty_=true;commit();
 if(selectionChanged_)selectionChanged_();
}
const Point* PitchCurveView::selectedPoint() const noexcept {
 return selectedIndex_<pattern_.size()?&pattern_.at(selectedIndex_):nullptr;
}
bool PitchCurveView::editSelectedTick(int32_t tick){
 if(selectedIndex_==0||selectedIndex_>=pattern_.size()||
    tick<=0||tick>=pattern_.lengthTicks())return false;
 const auto old=pattern_.at(selectedIndex_);
 size_t moved=selectedIndex_;
 if(!pattern_.moveAt(selectedIndex_,{tick,old.semitones,old.toNext},moved))
  return false;
 selectedIndex_=moved;dirty_=true;commit();
 if(selectionChanged_)selectionChanged_();
 return true;
}
bool PitchCurveView::editSelectedPitch(double semitones){
 if(selectedIndex_>=pattern_.size()||!std::isfinite(semitones)||
    semitones< -48||semitones>48||(selectedIndex_==0&&semitones!=0))return false;
 Point item=pattern_.at(selectedIndex_);item.semitones=semitones;
 if(!pattern_.setAt(selectedIndex_,item))return false;
 dirty_=true;commit();if(selectionChanged_)selectionChanged_();
 return true;
}
} // namespace pitchator
