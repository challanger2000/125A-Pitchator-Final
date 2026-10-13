#pragma once
#include "vstgui/lib/cview.h"
namespace pitchator {
// Exact 125A-Branding vector master geometry (generated directly from SVG).
// No font recreation, bitmap downscaling or alteration of red A stroke.
class BrandLogoView final : public VSTGUI::CView {
public:
 explicit BrandLogoView(const VSTGUI::CRect& bounds):CView(bounds){setMouseEnabled(false);}
 void draw(VSTGUI::CDrawContext* context) override;
};
}
