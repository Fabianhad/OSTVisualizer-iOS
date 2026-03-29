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
static const CGFloat kMinThicknessPts = 2.0;

struct TakeoffHitArea
{
    std::string uid;
    std::string conditionUID;
    std::vector<CGPoint> polygon;
    CGPoint center;
    CGFloat hitRadius;
    bool isPolygon;
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

    UIView *_canvasView;
    CGFloat _pageWidthPts;
    CGFloat _pageHeightPts;
    int _pageRotation;
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
