#ifndef TAKEOFFOVERLAYVIEW_H
#define TAKEOFFOVERLAYVIEW_H

#import <UIKit/UIKit.h>
#include <vector>

struct OverlayPath
{
    std::vector<CGPoint> points;
    float r, g, b, a;
    float strokeR = -1, strokeG = -1, strokeB = -1;
    bool filled;
    bool isEllipse;
    bool dashed = false;
    CGFloat lineWidth;
};

struct NegativeIndicator
{
    CGPoint center;
};

/// Generates points for a regular polygon inscribed in an axis-aligned ellipse.
std::vector<CGPoint> regularPolygonPoints(CGFloat cx, CGFloat cy,
                                          CGFloat rx, CGFloat ry,
                                          int sides);

/// Appends a square selection-handle overlay to the paths vector.
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
