#include "../../Core/Domain/DisplayColor.hpp"
#import "../PlanView_Internal.h"
#include "../../Core/Geometry/coord_transform.hpp"
#include "../../Core/Geometry/placement_snap.hpp"
#include "../../Core/Geometry/takeoff_footprint.hpp"
#include <optional>
#include <stdexcept>

// Grid snapping has no setting on iOS; Windows has it on by default
// (Config.snap_to_grid_enabled).
static const bool kGridSnapEnabled = true;
// placement_mode._add_place_handle(x, y, 2.5): the edge middles of the rectangle that is being dragged out
static const CGFloat kRectDragMidHalf = 2.5;

static std::vector<ost_types::Vec2>
pairsToVec2(const std::vector<std::pair<double, double>> &pts) {
  std::vector<ost_types::Vec2> v;
  v.reserve(pts.size());
  for (const auto &p : pts)
    v.push_back({p.first, p.second});
  return v;
}

@implementation PlanView (Placement)

// The exact inverse of the page transform (rotation and flips), like Windows
// selection_manager._scene_pos_to_ost; no snapping here. Empty for an invalid
// page contract (non-positive scale, rotation that is not a quarter turn, ...).
- (std::optional<ost_types::Vec2>)_canvasToOst:(CGPoint)pt {
  try {
    return ost_coord::canvas_to_ost_point(
        pt.x, pt.y, _pageWidthPts, _pageHeightPts, _scaleFactor1, _scaleFactor2,
        _pageRotation, _pageFlipX, _pageFlipY);
  } catch (const std::invalid_argument &) {
    return std::nullopt;
  }
}

- (CGPoint)_ostToCanvas:(double)ox oy:(double)oy {
  try {
    ost_types::Vec2 canvas = ost_coord::ost_to_canvas_point(
        ox, oy, _pageWidthPts, _pageHeightPts, _scaleFactor1, _scaleFactor2,
        _pageRotation, _pageFlipX, _pageFlipY);
    return {(CGFloat)canvas[0], (CGFloat)canvas[1]};
  } catch (const std::invalid_argument &) {
    return CGPointZero;
  }
}

// Windows _placement_snap_from_scene for a touch location: the grid point when
// it lies within kGridSnapThresholdPx screen points of the touch, otherwise
// the raw point (Windows' takeoff and PDF-line snaps do not exist on iOS).
// Empty for an invalid page contract.
- (std::optional<ost_placement::PlacementPoint>)_placementPointAtCanvas:
    (CGPoint)pt {
  std::optional<ost_types::Vec2> raw = [self _canvasToOst:pt];
  if (!raw)
    return std::nullopt;
  // screen points per OST unit: points = ost * 72 * sf1 / sf2, then the zoom
  double screenPtsPerOst =
      kPointsPerInch * _scaleFactor1 / _scaleFactor2 * self.zoomScale;
  return ost_placement::grid_snap((*raw)[0], (*raw)[1], _placeSnapIncrement,
                                  screenPtsPerOst,
                                  ost_placement::kGridSnapThresholdPx,
                                  kGridSnapEnabled);
}

// The angle step: Windows' unpressed default; the iOS toggle stands for the
// Shift key (off = the pressed default, no angle snap).
- (double)_placeAngleDegrees {
  return _angleSnappingEnabled ? ost_placement::kDefaultSnapAngleDegrees : 0.0;
}

- (void)enterPlacementModeWithCondition:(const ost_domain::Condition &)condition
                          snapIncrement:(double)snap {
  _placementActive = YES;
  _placeConditionUID = condition.uid;
  _placeConditionType = condition.condition_type;
  condition.get_rgb(_placeColorR, _placeColorG, _placeColorB);
  _placePattern = condition.pattern;
  _placeThickness = condition.thickness;
  _placeSnapIncrement = snap;
  _placePoints.clear();
  _placeLinearDragging = NO;
  _placeAreaRectDragging = NO;

  _placeTap.enabled = YES;
  _placePan.enabled = YES;
  _editTap.enabled = NO;
  _editLongPress.enabled = NO;
  _editPan.enabled = NO;
  [self _clearPlacePreview];
}

- (void)exitPlacementMode {
  if (!_placementActive)
    return;
  _placementActive = NO;
  _placePoints.clear();
  _placeLinearDragging = NO;
  _placeAreaRectDragging = NO;
  _placeTap.enabled = NO;
  _placePan.enabled = NO;
  [self _clearPlacePreview];
  [self _notifyAreaPlacementInProgress:NO];
}

- (void)undoLastAreaPoint {
  if (!_placementActive ||
      _placeConditionType != ost_domain::ConditionType::Area)
    return;
  if (_placePoints.empty())
    return;
  _placePoints.pop_back();
  if (_placePoints.empty()) {
    [self _clearPlacePreview];
    [self _notifyAreaPlacementInProgress:NO];
  } else {
    [self _updatePlacePreview];
  }
}

- (void)cancelAreaPlacement {
  if (!_placementActive ||
      _placeConditionType != ost_domain::ConditionType::Area)
    return;
  _placePoints.clear();
  _placeAreaRectDragging = NO;
  [self _clearPlacePreview];
  [self _notifyAreaPlacementInProgress:NO];
}

- (void)_notifyAreaPlacementInProgress:(BOOL)inProgress {
  if (_areaPlacementInProgress == inProgress)
    return;
  _areaPlacementInProgress = inProgress;
  if ([_editDelegate
          respondsToSelector:@selector(planView:areaPlacementInProgress:)])
    [_editDelegate planView:self areaPlacementInProgress:inProgress];
}

- (void)_clearPlacePreview {
  [_placePreviewView removeFromSuperview];
  _placePreviewView = nil;
}

- (void)_updatePlacePreview {
  [self _updatePlacePreviewInvalid:NO];
}

- (void)_updatePlacePreviewInvalid:(BOOL)invalid {
  [self _clearPlacePreview];
  if (!_canvasView || _placePoints.empty())
    return;

  float fillR = invalid ? 0.78f : _placeColorR;
  float fillG = invalid ? 0.0f : _placeColorG;
  float fillB = invalid ? 0.0f : _placeColorB;

  // COL-02: the gray byte of Windows, like the overlays and the 3D view
  if (!invalid && _renderConfig.grayscale_enabled)
    ost_domain::grayscale_channels(fillR, fillG, fillB);

  // COL-04: the 2D fill opacity of Windows (pattern 0: no fill, the outline stays)
  float previewAlpha = (float)ost_domain::display_opacity_2d(
      _renderConfig.color_mode, (int)_placePattern);

  CGRect frame = _canvasView.bounds;
  std::vector<OverlayPath> paths;

  if (_placeConditionType == ost_domain::ConditionType::Linear &&
      _placePoints.size() >= 2) {
    CGPoint p1 = [self _ostToCanvas:_placePoints[0].first
                                 oy:_placePoints[0].second];
    CGPoint p2 = [self _ostToCanvas:_placePoints[1].first
                                 oy:_placePoints[1].second];
    // RPV-26: the Windows band thickness (scene px / view scale = canvas points)
    CGFloat halfT = ost_footprint::rendered_linear_thickness(
                        _placeThickness, _scaleFactor2 / _scaleFactor1,
                        ost_footprint::kInteractivePdfRenderScale) /
                    ost_footprint::kInteractivePdfRenderScale / 2.0;
    CGFloat dx = p2.x - p1.x, dy = p2.y - p1.y;
    CGFloat len = hypot(dx, dy);
    if (len > 0.001) {
      CGFloat px = -dy / len * halfT, py = dx / len * halfT;
      OverlayPath op;
      op.filled = true;
      op.r = fillR;
      op.g = fillG;
      op.b = fillB;
      op.a = previewAlpha;
      op.lineWidth = 1.5;
      op.points = {{p1.x + px, p1.y + py},
                   {p1.x - px, p1.y - py},
                   {p2.x - px, p2.y - py},
                   {p2.x + px, p2.y + py}};
      paths.push_back(std::move(op));
    }
    addSquareHandle(p1, kCornerHalf, false, paths);
    addSquareHandle(p2, kCornerHalf, false, paths);
  } else if (_placeConditionType == ost_domain::ConditionType::Area &&
             _placePoints.size() >= 2) {
    std::vector<CGPoint> canvasPts;
    canvasPts.reserve(_placePoints.size());
    for (const auto &pt : _placePoints)
      canvasPts.push_back([self _ostToCanvas:pt.first oy:pt.second]);

    OverlayPath op;
    op.filled = true;
    op.r = fillR;
    op.g = fillG;
    op.b = fillB;
    op.a = previewAlpha;
    op.lineWidth = 0;
    op.points = canvasPts;
    paths.push_back(std::move(op));

    OverlayPath border;
    border.filled = false;
    border.dashed = true;
    border.r = 0;
    border.g = 0;
    border.b = 0;
    border.a = 1.0f;
    border.strokeR = 0;
    border.strokeG = 0;
    border.strokeB = 0;
    border.lineWidth = 1.0;
    border.points = canvasPts;
    paths.push_back(std::move(border));

    for (const auto &cp : canvasPts)
      addSquareHandle(cp, kCornerHalf, false, paths);

    // Windows draws the middle of each edge of the rectangle being dragged with a smaller handle (half 2.5)
    if (_placeAreaRectDragging && canvasPts.size() == 4) {
      for (size_t i = 0; i < 4; i++) {
        const CGPoint &a = canvasPts[i], &b = canvasPts[(i + 1) % 4];
        addSquareHandle(CGPointMake((a.x + b.x) / 2, (a.y + b.y) / 2),
                        kRectDragMidHalf, false, paths);
      }
    }
  }

  if (paths.empty())
    return;

  TakeoffOverlayView *preview =
      [[TakeoffOverlayView alloc] initWithFrame:frame
                                          paths:std::move(paths)
                             negativeIndicators:{}
                                      zoomScale:self.zoomScale];
  preview.userInteractionEnabled = NO;
  [_canvasView addSubview:preview];
  _placePreviewView = preview;
}

// Windows handle_place_press (placement_mode.py:1648) refuses at the first press for a condition that is gone or not visible
// ("event.accept(); return"): nothing is previewed, no polygon point exists, no placement is in progress. The delegate decides
// (the Image layer toggle leaves the tool armed on a condition that is then hidden, COND-PLACE-01); a delegate without the query
// allows every press.
- (BOOL)_placementPressAllowed {
  id<PlanViewEditDelegate> delegate = _editDelegate;
  if (![delegate respondsToSelector:@selector(planView:canPlaceWithCondition:)])
    return YES;
  return [delegate planView:self canPlaceWithCondition:_placeConditionUID];
}

- (void)_handlePlaceTap:(UITapGestureRecognizer *)tap {
  if (tap.state != UIGestureRecognizerStateEnded)
    return;
  if (!_canvasView || !_editDelegate || !_placementActive)
    return;
  if (![self _placementPressAllowed])
    return;

  CGPoint canvasPt = [tap locationInView:_canvasView];
  std::optional<ost_placement::PlacementPoint> cursor =
      [self _placementPointAtCanvas:canvasPt];
  if (!cursor)
    return;
  double ox = cursor->point[0], oy = cursor->point[1];

  if (_placeConditionType == ost_domain::ConditionType::Count) {
    std::vector<double> pos = {ox, oy};
    [_editDelegate planView:self
        didPlaceTakeoffWithCondition:_placeConditionUID
                            position:pos];
    return;
  }

  if (_placeConditionType == ost_domain::ConditionType::Area) {
    if (_placePoints.empty()) {
      _placePoints.push_back({ox, oy});
      [self _updatePlacePreview];
      [self _notifyAreaPlacementInProgress:YES];
      return;
    }

    // Windows closes the polygon when the RAW tap (not the snapped point) is
    // within 12 screen points of the first point and at least three points are
    // placed; an invalid polygon neither closes nor adds a point.
    if (_placePoints.size() >= 3) {
      CGPoint startCanvas = [self _ostToCanvas:_placePoints[0].first
                                            oy:_placePoints[0].second];
      double closeDistPx = hypot(canvasPt.x - startCanvas.x,
                                 canvasPt.y - startCanvas.y) *
                           self.zoomScale;
      if (ost_placement::polygon_close_hit(_placePoints.size(), closeDistPx)) {
        auto polyVec = pairsToVec2(_placePoints);
        if (!ost_geom::polygon_is_valid(polyVec))
          return;

        std::vector<double> pos;
        for (const auto &p : _placePoints) {
          pos.push_back(p.first);
          pos.push_back(p.second);
        }
        _placePoints.clear();
        [self _clearPlacePreview];
        [self _notifyAreaPlacementInProgress:NO];
        [_editDelegate planView:self
            didPlaceTakeoffWithCondition:_placeConditionUID
                                position:pos];
        return;
      }
    }

    const auto &last = _placePoints.back();
    ost_types::Vec2 end = ost_placement::snap_angle_for_placement(
        last.first, last.second, ox, oy, cursor->kind, _placeSnapIncrement,
        [self _placeAngleDegrees]);
    ox = end[0];
    oy = end[1];

    auto candidate = _placePoints;
    candidate.push_back({ox, oy});
    if (ost_geom::polyline_self_intersects(pairsToVec2(candidate))) {
      if (!_placeFlashing) {
        _placeFlashing = YES;
        [self _updatePlacePreviewInvalid:YES];
        dispatch_after(
            dispatch_time(DISPATCH_TIME_NOW, (int64_t)(250 * NSEC_PER_MSEC)),
            dispatch_get_main_queue(), ^{
              self->_placeFlashing = NO;
              [self _updatePlacePreview];
            });
      }
      return;
    }

    _placePoints.push_back({ox, oy});
    [self _updatePlacePreview];
  }
}

- (void)_handlePlacePan:(UIPanGestureRecognizer *)pan {
  if (!_canvasView || !_editDelegate || !_placementActive)
    return;
  // The press that starts a drag (Linear drag, Area rectangle drag) is refused at Began, before the first point exists. Every
  // later state of a refused pan finds no drag in progress (_placeLinearDragging / _placeAreaRectDragging are only set at
  // Began), so it does nothing; the recogniser still takes the touch, so the page does not scroll under a refused press.
  if (pan.state == UIGestureRecognizerStateBegan &&
      ![self _placementPressAllowed])
    return;

  // The press position (Windows) is where the touch went down: UIKit begins a
  // pan only after the touch travelled a recognition threshold, so the start
  // is the location at Began minus that travel (RPV-M07).
  CGPoint canvasPt = pan.state == UIGestureRecognizerStateBegan
                         ? PlanViewPanStartPoint(pan, _canvasView)
                         : [pan locationInView:_canvasView];

  if (_placeConditionType == ost_domain::ConditionType::Linear) {
    if (pan.state == UIGestureRecognizerStateBegan) {
      auto cursor = [self _placementPointAtCanvas:canvasPt];
      if (!cursor)
        return;
      _placePoints.clear();
      _placePoints.push_back({cursor->point[0], cursor->point[1]});
      _placeLinearDragging = YES;
    } else if (pan.state == UIGestureRecognizerStateChanged &&
               _placeLinearDragging) {
      auto cursor = [self _placementPointAtCanvas:canvasPt];
      if (!cursor)
        return;
      // angle snap against the first point, then (grid points only) the length
      // rounded to the increment along that direction
      ost_types::Vec2 end = ost_placement::snap_angle_for_placement(
          _placePoints[0].first, _placePoints[0].second, cursor->point[0],
          cursor->point[1], cursor->kind, _placeSnapIncrement,
          [self _placeAngleDegrees]);
      if (_placePoints.size() < 2)
        _placePoints.push_back({end[0], end[1]});
      else
        _placePoints[1] = {end[0], end[1]};
      [self _updatePlacePreview];
    } else if (_placeLinearDragging) {
      _placeLinearDragging = NO;
      if (_placePoints.size() >= 2) {
        double dx = _placePoints[1].first - _placePoints[0].first;
        double dy = _placePoints[1].second - _placePoints[0].second;
        if (ost_placement::meets_placement_minimum(
                hypot(dx, dy),
                ost_placement::placement_minimum_length(_placeSnapIncrement))) {
          std::vector<double> pos = {
              _placePoints[0].first, _placePoints[0].second,
              _placePoints[1].first, _placePoints[1].second};
          _placePoints.clear();
          [self _clearPlacePreview];
          [_editDelegate planView:self
              didPlaceTakeoffWithCondition:_placeConditionUID
                                  position:pos];
          return;
        }
      }
      _placePoints.clear();
      [self _clearPlacePreview];
    }
  } else if (_placeConditionType == ost_domain::ConditionType::Area &&
             (_placePoints.empty() || _placeAreaRectDragging)) {
    if (pan.state == UIGestureRecognizerStateBegan && _placePoints.empty()) {
      auto cursor = [self _placementPointAtCanvas:canvasPt];
      if (!cursor)
        return;
      _placePoints.push_back({cursor->point[0], cursor->point[1]});
      _placeAreaRectDragging = YES;
      [self _notifyAreaPlacementInProgress:YES];
    } else if (pan.state == UIGestureRecognizerStateChanged &&
               _placeAreaRectDragging) {
      auto cursor = [self _placementPointAtCanvas:canvasPt];
      if (!cursor)
        return;
      double ox = cursor->point[0], oy = cursor->point[1];
      double x1 = _placePoints[0].first, y1 = _placePoints[0].second;
      _placePoints.resize(4);
      _placePoints[1] = {ox, y1};
      _placePoints[2] = {ox, oy};
      _placePoints[3] = {x1, oy};
      [self _updatePlacePreview];
    } else if (_placeAreaRectDragging && _placePoints.size() >= 4) {
      _placeAreaRectDragging = NO;
      double x1 = _placePoints[0].first, y1 = _placePoints[0].second;
      double x2 = _placePoints[2].first, y2 = _placePoints[2].second;
      double minLen =
          ost_placement::placement_minimum_length(_placeSnapIncrement);
      if (ost_placement::meets_placement_minimum(fabs(x2 - x1), minLen) &&
          ost_placement::meets_placement_minimum(fabs(y2 - y1), minLen)) {
        std::vector<double> pos = {x1, y1, x2, y1, x2, y2, x1, y2};
        _placePoints.clear();
        [self _clearPlacePreview];
        [self _notifyAreaPlacementInProgress:NO];
        [_editDelegate planView:self
            didPlaceTakeoffWithCondition:_placeConditionUID
                                position:pos];
        return;
      }
      _placePoints.resize(1);
      [self _updatePlacePreview];
    }
  }
}

@end
