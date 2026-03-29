#import "../PlanView_Internal.h"

static const double kPlaceSnapAngleRad = M_PI / 8.0;
static const CGFloat kCloseDistPts = 12.0;

static double placeSnapOst(double val, double inc) {
  if (inc <= 0.0)
    return val;
  return round(val / inc) * inc;
}

static void placeSnapAngle(double ox, double oy, double &tx, double &ty) {
  double dx = tx - ox, dy = ty - oy;
  double len = hypot(dx, dy);
  if (len < 1e-9)
    return;
  double angle = atan2(dy, dx);
  double snapped = round(angle / kPlaceSnapAngleRad) * kPlaceSnapAngleRad;
  tx = ox + len * cos(snapped);
  ty = oy + len * sin(snapped);
}

static std::vector<ost_types::Vec2>
pairsToVec2(const std::vector<std::pair<double, double>> &pts) {
  std::vector<ost_types::Vec2> v;
  v.reserve(pts.size());
  for (const auto &p : pts)
    v.push_back({p.first, p.second});
  return v;
}

@implementation PlanView (Placement)

- (std::pair<double, double>)_canvasToOst:(CGPoint)pt {
  double units_per_inch = (_scaleFactor2 / _scaleFactor1);
  double units_per_point = units_per_inch / kPointsPerInch;
  int rot = ((_pageRotation % 360) + 360) % 360;
  double ox, oy;
  switch (rot) {
  case 90:
    ox = (_pageHeightPts - pt.y) * units_per_point;
    oy = (_pageWidthPts - pt.x) * units_per_point;
    break;
  case 180:
    ox = (_pageWidthPts - pt.x) * units_per_point;
    oy = pt.y * units_per_point;
    break;
  case 270:
    ox = pt.y * units_per_point;
    oy = pt.x * units_per_point;
    break;
  default:
    ox = pt.x * units_per_point;
    oy = pt.y * units_per_point;
    break;
  }
  ox = placeSnapOst(ox, _placeSnapIncrement);
  oy = placeSnapOst(oy, _placeSnapIncrement);
  return {ox, oy};
}

- (CGPoint)_ostToCanvas:(double)ox oy:(double)oy {
  double ppu = kPointsPerInch / (_scaleFactor2 / _scaleFactor1);
  int rot = ((_pageRotation % 360) + 360) % 360;
  switch (rot) {
  case 90:
    return {(CGFloat)(_pageWidthPts - oy * ppu),
            (CGFloat)(_pageHeightPts - ox * ppu)};
  case 180:
    return {(CGFloat)(_pageWidthPts - ox * ppu), (CGFloat)(oy * ppu)};
  case 270:
    return {(CGFloat)(oy * ppu), (CGFloat)(ox * ppu)};
  default:
    return {(CGFloat)(ox * ppu), (CGFloat)(oy * ppu)};
  }
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

  if (!invalid && _renderConfig.grayscale_enabled) {
    float gray = 0.299f * fillR + 0.587f * fillG + 0.114f * fillB;
    fillR = fillG = fillB = gray;
  }

  float previewAlpha;
  if (_renderConfig.is_solid()) {
    previewAlpha = 1.0f;
  } else if (_renderConfig.is_transparent()) {
    previewAlpha = 0.5f;
  } else {
    previewAlpha = (_placePattern == ost_domain::Pattern::Solid) ? 1.0f : 0.5f;
  }

  CGRect frame = _canvasView.bounds;
  std::vector<OverlayPath> paths;

  if (_placeConditionType == ost_domain::ConditionType::Linear &&
      _placePoints.size() >= 2) {
    CGPoint p1 = [self _ostToCanvas:_placePoints[0].first
                                 oy:_placePoints[0].second];
    CGPoint p2 = [self _ostToCanvas:_placePoints[1].first
                                 oy:_placePoints[1].second];
    double scaleRatio =
        (_scaleFactor1 != 0.0) ? _scaleFactor2 / _scaleFactor1 : 1.0;
    CGFloat thickPts = (_placeThickness > 0 ? _placeThickness : 1.0) /
                       scaleRatio * kPointsPerInch;
    if (thickPts < kMinThicknessPts)
      thickPts = kMinThicknessPts;
    CGFloat halfT = thickPts / 2.0;
    CGFloat dx = p2.x - p1.x, dy = p2.y - p1.y;
    CGFloat len = hypot(dx, dy);
    if (len > 0.001) {
      CGFloat px = -dy / len * halfT, py = dx / len * halfT;
      OverlayPath op;
      op.isEllipse = false;
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
    op.isEllipse = false;
    op.filled = true;
    op.r = fillR;
    op.g = fillG;
    op.b = fillB;
    op.a = previewAlpha;
    op.lineWidth = 0;
    op.points = canvasPts;
    paths.push_back(std::move(op));

    OverlayPath border;
    border.isEllipse = false;
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

- (void)_handlePlaceTap:(UITapGestureRecognizer *)tap {
  if (tap.state != UIGestureRecognizerStateEnded)
    return;
  if (!_canvasView || !_editDelegate || !_placementActive)
    return;

  CGPoint canvasPt = [tap locationInView:_canvasView];
  auto [ox, oy] = [self _canvasToOst:canvasPt];

  if (_placeConditionType == ost_domain::ConditionType::Count) {
    std::vector<double> pos = {ox, oy};
    [_editDelegate planView:self
        didPlaceTakeoffWithCondition:_placeConditionUID
                            position:pos];
    return;
  }

  if (_placeConditionType == ost_domain::ConditionType::Area) {
    if (_placePoints.empty()) {
      ox = placeSnapOst(ox, _placeSnapIncrement);
      oy = placeSnapOst(oy, _placeSnapIncrement);
      _placePoints.push_back({ox, oy});
      [self _updatePlacePreview];
      [self _notifyAreaPlacementInProgress:YES];
      return;
    }

    auto &last = _placePoints.back();
    if (_angleSnappingEnabled)
      placeSnapAngle(last.first, last.second, ox, oy);
    ox = placeSnapOst(ox, _placeSnapIncrement);
    oy = placeSnapOst(oy, _placeSnapIncrement);

    if (_placePoints.size() >= 3) {
      CGPoint startCanvas = [self _ostToCanvas:_placePoints[0].first
                                            oy:_placePoints[0].second];
      CGPoint tapCanvas = [self _ostToCanvas:ox oy:oy];
      CGFloat closeDist = kCloseDistPts / self.zoomScale;
      CGFloat cdx = tapCanvas.x - startCanvas.x;
      CGFloat cdy = tapCanvas.y - startCanvas.y;
      if (cdx * cdx + cdy * cdy < closeDist * closeDist) {
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

  CGPoint canvasPt = [pan locationInView:_canvasView];

  if (_placeConditionType == ost_domain::ConditionType::Linear) {
    if (pan.state == UIGestureRecognizerStateBegan) {
      auto [ox, oy] = [self _canvasToOst:canvasPt];
      _placePoints.clear();
      _placePoints.push_back({ox, oy});
      _placeLinearDragging = YES;
    } else if (pan.state == UIGestureRecognizerStateChanged &&
               _placeLinearDragging) {
      auto [ox, oy] = [self _canvasToOst:canvasPt];
      if (_angleSnappingEnabled)
        placeSnapAngle(_placePoints[0].first, _placePoints[0].second, ox, oy);
      ox = placeSnapOst(ox, _placeSnapIncrement);
      oy = placeSnapOst(oy, _placeSnapIncrement);
      if (_placePoints.size() < 2)
        _placePoints.push_back({ox, oy});
      else
        _placePoints[1] = {ox, oy};
      [self _updatePlacePreview];
    } else if (_placeLinearDragging) {
      _placeLinearDragging = NO;
      if (_placePoints.size() >= 2) {
        double dx = _placePoints[1].first - _placePoints[0].first;
        double dy = _placePoints[1].second - _placePoints[0].second;
        double minLen = _placeSnapIncrement > 0 ? _placeSnapIncrement : 1e-6;
        if (hypot(dx, dy) >= minLen) {
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
      auto [ox, oy] = [self _canvasToOst:canvasPt];
      ox = placeSnapOst(ox, _placeSnapIncrement);
      oy = placeSnapOst(oy, _placeSnapIncrement);
      _placePoints.push_back({ox, oy});
      _placeAreaRectDragging = YES;
      [self _notifyAreaPlacementInProgress:YES];
    } else if (pan.state == UIGestureRecognizerStateChanged &&
               _placeAreaRectDragging) {
      auto [ox, oy] = [self _canvasToOst:canvasPt];
      ox = placeSnapOst(ox, _placeSnapIncrement);
      oy = placeSnapOst(oy, _placeSnapIncrement);
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
      double minLen = _placeSnapIncrement > 0 ? _placeSnapIncrement : 1e-6;
      if (fabs(x2 - x1) >= minLen && fabs(y2 - y1) >= minLen) {
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
