#ifndef PLANVIEW_INTERNAL_H
#define PLANVIEW_INTERNAL_H

#import "PlanView.h"
#import "Components/TakeoffOverlayView.h"
#include "../Core/Domain/Types.hpp"
#include "../Core/Geometry/geom_utils.hpp"
#include <vector>
#include <utility>
#include <string>

static const CGFloat kPointsPerInch = 72.0;

// Where a pan gesture's touch went down, in `view`: its location at Began minus
// the translation travelled before UIKit recognised the pan (Windows starts a
// drag or a placement at the press position, RPV-M07). Use at Began only.
static inline CGPoint PlanViewPanStartPoint(UIPanGestureRecognizer *pan,
                                            UIView *view) {
    CGPoint location = [pan locationInView:view];
    CGPoint travelled = [pan translationInView:view];
    return CGPointMake(location.x - travelled.x, location.y - travelled.y);
}

// What a tap can hit: the footprint of a takeoff on the canvas (a point shape, a linear band, an area, like the Windows item
// shape) and its handles.
struct TakeoffHitArea
{
    std::string uid;
    std::string conditionUID;
    std::vector<CGPoint> polygon;
    // The backouts cut out of an area: a point inside one is not on this takeoff (it is on the hole takeoff, RPV-31).
    std::vector<std::vector<CGPoint>> holes;
    std::vector<CGPoint> handleCenters;
    int cornerCount;
};

// Class extension — ivars visible to all PlanView .mm files that import this header.
@interface PlanView () <UIScrollViewDelegate, UIGestureRecognizerDelegate>
{
@package
    __weak id<PlanViewEditDelegate> _editDelegate;
    BOOL _editEnabled;
    BOOL _angleSnappingEnabled;

    // The canvas is the DISPLAYED page: the PDF page flipped and turned with the page transform (width and height swapped
    // for 90 / 270). _pageWidthPts / _pageHeightPts are the UNROTATED page (the PDF's own size when it can be read).
    UIView *_canvasView;
    CGFloat _pageWidthPts;
    CGFloat _pageHeightPts;
    int _pageRotation;
    // page.flip_x / page.flip_y of the displayed page (set when the page is loaded, with the rotation)
    BOOL _pageFlipX;
    BOOL _pageFlipY;
    double _scaleFactor1;
    double _scaleFactor2;

    std::vector<TakeoffHitArea> _hitAreas;
    UITapGestureRecognizer *_editTap;
    UILongPressGestureRecognizer *_editLongPress;
    UIPanGestureRecognizer *_editPan;
    std::string _draggingUID;
    std::string _lastTappedUID;
    CGPoint _dragStartCanvasPoint;

    BOOL _placementActive;
    std::string _placeConditionUID;
    ost_domain::ConditionType _placeConditionType;
    float _placeColorR, _placeColorG, _placeColorB;
    ost_domain::Pattern _placePattern;
    double _placeThickness;
    double _placeSnapIncrement;
    std::vector<std::pair<double, double>> _placePoints;
    BOOL _placeLinearDragging;
    BOOL _placeAreaRectDragging;
    BOOL _placeFlashing;
    BOOL _areaPlacementInProgress;
    UITapGestureRecognizer *_placeTap;
    UIPanGestureRecognizer *_placePan;
    UIView *_placePreviewView;

    ost_domain::RenderConfig _renderConfig;
}
@end

#endif
