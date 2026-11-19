#ifndef TAKEOFFOVERLAYVIEW_H
#define TAKEOFFOVERLAYVIEW_H

#import <UIKit/UIKit.h>
#include <vector>

// One polygon of the overlay, in canvas points (the displayed page).
struct OverlayPath
{
    std::vector<CGPoint> points;
    // Contours cut out of the fill (the backouts of an area: the PDF shows through, like the Windows subtracted path); the
    // outline is stroked around them too.
    std::vector<std::vector<CGPoint>> holes;
    float r, g, b, a;
    float strokeR = -1, strokeG = -1, strokeB = -1;
    bool filled;
    bool dashed = false;
    // Stroke width in SCREEN points: a Windows cosmetic pen keeps its width at every zoom (the overlay divides it by the
    // zoom scale). 0 is the takeoff outline pen, 2.0.
    CGFloat lineWidth;
    // > 0: a selection handle, a square of 2 * handleHalf screen points centred on points[0] (ItemIgnoresTransformations
    // items of Windows keep their size at every zoom).
    CGFloat handleHalf = 0;
};

struct NegativeIndicator
{
    CGPoint center;
};

/// Appends a square selection-handle overlay (half-size in screen points) to the paths vector. Windows handle style
/// (handle_style.py): white fill with alpha 224 and a black 1 px outline; the handle that is being dragged stays orange (an
/// iOS-only touch affordance).
void addSquareHandle(CGPoint c, CGFloat half, bool active,
                     std::vector<OverlayPath> &paths);

static const CGFloat kCornerHalf = 4.0;
static const CGFloat kMidHalf = 2.0;

@interface TakeoffOverlayView : UIView

- (instancetype)initWithFrame:(CGRect)frame
                        paths:(std::vector<OverlayPath>)paths
           negativeIndicators:(std::vector<NegativeIndicator>)indicators
                    zoomScale:(CGFloat)zoomScale;
- (void)updateZoomScale:(CGFloat)zoomScale;

@end

#endif
