#include "../Core/Geometry/coord_transform.hpp"
#import "Components/PDFCanvasView.h"
#import "PlanView_Internal.h"
#include <cmath>
#include <unordered_map>

static const CGFloat kMinZoom = 0.05;
static const CGFloat kMaxZoom = 16.0;
static const CGFloat kDefaultWidthPts = 612.0;
static const CGFloat kDefaultHeightPts = 792.0;
static const CGFloat kOverlayStrokeWidthLocal = 1.5;
static const CGFloat kMinCountSizePts = 4.0;

static const int kOverlayViewTag = 9001;
static const int kPdfViewTag = 9002;

static const CGFloat kHitTolerancePts = 10.0;

static CGFloat pointToSegmentDistSq(CGPoint p, CGPoint a, CGPoint b) {
  CGFloat dx = b.x - a.x, dy = b.y - a.y;
  CGFloat lenSq = dx * dx + dy * dy;
  if (lenSq < 0.0001) {
    CGFloat ex = p.x - a.x, ey = p.y - a.y;
    return ex * ex + ey * ey;
  }
  CGFloat t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
  t = fmax(0, fmin(1, t));
  CGFloat projX = a.x + t * dx, projY = a.y + t * dy;
  CGFloat ex = p.x - projX, ey = p.y - projY;
  return ex * ex + ey * ey;
}

static bool pointInPolygon(CGPoint p, const std::vector<CGPoint> &poly) {
  bool inside = false;
  size_t n = poly.size();
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    if (((poly[i].y > p.y) != (poly[j].y > p.y)) &&
        (p.x <
         (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) +
             poly[i].x)) {
      inside = !inside;
    }
  }
  return inside;
}

@implementation PlanView

@synthesize editDelegate = _editDelegate;
@synthesize editEnabled = _editEnabled;
@synthesize angleSnappingEnabled = _angleSnappingEnabled;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self)
    [self _commonInit];
  return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
  self = [super initWithCoder:coder];
  if (self)
    [self _commonInit];
  return self;
}

- (void)_commonInit {
  self.delegate = self;
  self.backgroundColor = [UIColor darkGrayColor];
  self.showsHorizontalScrollIndicator = YES;
  self.showsVerticalScrollIndicator = YES;
  self.bouncesZoom = YES;
  self.minimumZoomScale = kMinZoom;
  self.maximumZoomScale = kMaxZoom;

  _angleSnappingEnabled = YES;
  _pageWidthPts = kDefaultWidthPts;
  _pageHeightPts = kDefaultHeightPts;
  _pageRotation = 0;
  _scaleFactor1 = 1.0;
  _scaleFactor2 = 1.0;

  _editTap = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(_handleEditTap:)];
  _editTap.enabled = NO;
  _editTap.delegate = self;
  [self addGestureRecognizer:_editTap];

  _editLongPress = [[UILongPressGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(_handleEditLongPress:)];
  _editLongPress.minimumPressDuration = 0.5;
  _editLongPress.enabled = NO;
  _editLongPress.delegate = self;
  [self addGestureRecognizer:_editLongPress];

  _editPan = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(_handleEditPan:)];
  _editPan.maximumNumberOfTouches = 1;
  _editPan.enabled = NO;
  _editPan.delegate = self;
  [self addGestureRecognizer:_editPan];

  _placeTap = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(_handlePlaceTap:)];
  _placeTap.enabled = NO;
  [self addGestureRecognizer:_placeTap];

  _placePan = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(_handlePlacePan:)];
  _placePan.maximumNumberOfTouches = 1;
  _placePan.enabled = NO;
  _placePan.delegate = self;
  [self addGestureRecognizer:_placePan];

  [self.panGestureRecognizer requireGestureRecognizerToFail:_editPan];
  [self.panGestureRecognizer requireGestureRecognizerToFail:_placePan];
}

#pragma mark - Public

- (void)loadPageWithWidth:(double)width
                   height:(double)height
                 rotation:(int)rotation
             scaleFactor1:(double)sf1
             scaleFactor2:(double)sf2
                imagePath:(const std::string &)imagePath
                resetZoom:(BOOL)resetZoom {
  _scaleFactor1 = sf1;
  _scaleFactor2 = sf2;
  _pageRotation = rotation;

  _pageWidthPts = (width > 0) ? width * kPointsPerInch : kDefaultWidthPts;
  _pageHeightPts = (height > 0) ? height * kPointsPerInch : kDefaultHeightPts;

  CGFloat canvasW = _pageWidthPts;
  CGFloat canvasH = _pageHeightPts;

  CGFloat savedZoom = self.zoomScale;
  CGPoint savedOffset = self.contentOffset;
  CGSize savedContent = self.contentSize;

  [_canvasView removeFromSuperview];

  _canvasView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, canvasW, canvasH)];
  _canvasView.backgroundColor = [UIColor whiteColor];

  NSString *pdfPath = nil;
  if (!imagePath.empty()) {
    pdfPath = [NSString stringWithUTF8String:imagePath.c_str()];
    if ([[NSFileManager defaultManager] fileExistsAtPath:pdfPath]) {
      PDFCanvasView *pdfView = [[PDFCanvasView alloc]
          initWithFrame:CGRectMake(0, 0, canvasW, canvasH)
                pdfPath:pdfPath];
      pdfView.tag = kPdfViewTag;
      pdfView.autoresizingMask =
          UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
      [_canvasView addSubview:pdfView];
    }
  }

  [self addSubview:_canvasView];
  self.contentSize = CGSizeMake(canvasW, canvasH);

  self.minimumZoomScale = kMinZoom;
  self.maximumZoomScale = kMaxZoom;

  if (resetZoom) {
    self.zoomScale = 1.0;
    [self fitToPage];
  } else {
    self.zoomScale = savedZoom;
    if (savedContent.width > 0 && savedContent.height > 0) {
      CGSize newContent = self.contentSize;
      CGFloat ratioX = savedOffset.x / savedContent.width;
      CGFloat ratioY = savedOffset.y / savedContent.height;
      self.contentOffset =
          CGPointMake(ratioX * newContent.width, ratioY * newContent.height);
    }

    [self _centerCanvas];
  }
}

- (void)renderOverlaysForPage:(const std::string &)pageUID
                      project:(const ost_data::ProjectData &)project
                 renderConfig:(const ost_domain::RenderConfig &)config
              selectedTakeoff:(const std::string &)selectedUID
            selectedCondition:(const std::string &)selectedCondUID
                 activeHandle:(int)activeHandle {
  if (!_canvasView)
    return;
  _renderConfig = config;

  UIView *oldOverlay = [_canvasView viewWithTag:kOverlayViewTag];
  [oldOverlay removeFromSuperview];

  auto pageIt = project.pages.find(pageUID);
  if (pageIt == project.pages.end())
    return;

  const auto &page = pageIt->second;
  double scaleRatio =
      (_scaleFactor1 != 0.0) ? _scaleFactor2 / _scaleFactor1 : 1.0;

  std::unordered_map<std::string, std::vector<const ost_domain::Takeoff *>>
      holeMap;
  for (const auto &tk : project.takeoffs) {
    if (tk.page_uid != pageUID)
      continue;
    if (tk.is_hole()) {
      holeMap[tk.parent_uid].push_back(&tk);
    }
  }

  std::vector<OverlayPath> paths;
  std::vector<NegativeIndicator> negativeIndicators;
  _hitAreas.clear();

  for (const auto &takeoff : project.takeoffs) {
    if (takeoff.page_uid != pageUID)
      continue;

    if (takeoff.is_hole())
      continue;
    if (takeoff.position.size() < 2)
      continue;

    auto cdnIt = project.conditions.find(takeoff.condition_uid);
    if (cdnIt == project.conditions.end())
      continue;
    const auto &cdn = cdnIt->second;

    if (!project.is_condition_visible(cdn))
      continue;

    float r, g, b;
    cdn.get_rgb(r, g, b);

    if (config.grayscale_enabled) {
      float gray = 0.299f * r + 0.587f * g + 0.114f * b;
      r = g = b = gray;
    }

    const std::vector<double> &rawPos = takeoff.position;
    std::vector<double> linearPos;
    const std::vector<double> *posPtr = &rawPos;
    if (cdn.is_linear() && rawPos.size() > 4) {
      linearPos.assign(rawPos.begin(), rawPos.begin() + 4);
      posPtr = &linearPos;
    }

    auto pdfVerts = ost_coord::ost_to_pdf_coordinates(
        *posPtr, _pageWidthPts, _pageHeightPts, _scaleFactor1, _scaleFactor2,
        _pageRotation, page.flip_x, page.flip_y, 1.0, 1.0, false, false, 0.0,
        0.0);

    if (pdfVerts.empty())
      continue;

    for (auto &v : pdfVerts) {
      v[1] = _pageHeightPts - v[1];
    }

    float alpha;
    if (config.is_solid()) {
      alpha = 1.0f;
    } else if (config.is_transparent()) {
      alpha = 0.5f;
    } else {
      alpha = (cdn.pattern == ost_domain::Pattern::Solid) ? 1.0f : 0.5f;
    }

    bool isSingleSelected =
        (!selectedUID.empty() && takeoff.uid == selectedUID);
    bool isMultiSelected = (selectedUID.empty() && !selectedCondUID.empty() &&
                            takeoff.condition_uid == selectedCondUID);
    bool isSelected = isSingleSelected || isMultiSelected;

    if (cdn.is_linear()) {
      [self _buildLinearPaths:pdfVerts
                    thickness:cdn.thickness
                   scaleRatio:scaleRatio
                            r:r
                            g:g
                            b:b
                            a:alpha
                        paths:paths];
    } else if (cdn.is_area()) {
      [self _buildAreaPath:pdfVerts r:r g:g b:b a:alpha paths:paths];

      auto holeIt = holeMap.find(takeoff.uid);
      if (holeIt != holeMap.end()) {
        for (const auto *hole : holeIt->second) {
          auto holeVerts = ost_coord::ost_to_pdf_coordinates(
              hole->position, _pageWidthPts, _pageHeightPts, _scaleFactor1,
              _scaleFactor2, _pageRotation, page.flip_x, page.flip_y, 1.0, 1.0,
              false, false, 0.0, 0.0);

          for (auto &hv : holeVerts) {
            hv[1] = _pageHeightPts - hv[1];
          }
          if (!holeVerts.empty()) {
            [self _buildAreaPath:holeVerts
                               r:1.0f
                               g:1.0f
                               b:1.0f
                               a:1.0f
                           paths:paths];
          }
        }
      }
    } else if (cdn.is_count()) {
      [self _buildCountPath:pdfVerts
                      shape:cdn.shape
                      width:cdn.width
                      depth:cdn.depth
                displaySize:cdn.display_size
                   rotation:takeoff.rotation
                 scaleRatio:scaleRatio
                          r:r
                          g:g
                          b:b
                          a:alpha
                      paths:paths];
    }

    if (isSingleSelected) {
      [self _buildSelectionHandles:pdfVerts
                         condition:cdn
                      activeHandle:activeHandle
                             paths:paths];
    } else if (isMultiSelected) {
      [self _buildMultiSelectionBorder:pdfVerts
                             condition:cdn
                            scaleRatio:scaleRatio
                              rotation:takeoff.rotation
                                 paths:paths];
    }

    if (takeoff.is_negative && !pdfVerts.empty()) {
      CGFloat cx = 0, cy = 0;
      for (const auto &v : pdfVerts) {
        cx += v[0];
        cy += v[1];
      }
      cx /= pdfVerts.size();
      cy /= pdfVerts.size();
      negativeIndicators.push_back({CGPointMake(cx, cy)});
    }

    TakeoffHitArea hitArea;
    hitArea.uid = takeoff.uid;
    hitArea.conditionUID = takeoff.condition_uid;
    if (cdn.is_count() && !pdfVerts.empty()) {
      hitArea.isPolygon = false;
      hitArea.center = {(CGFloat)pdfVerts[0][0], (CGFloat)pdfVerts[0][1]};
      CGFloat scaleMul =
          (cdn.display_size > 0 ? cdn.display_size : 100.0) / 100.0;
      CGFloat wPts = (cdn.width > 0 ? cdn.width : 1.0) / scaleRatio *
                     kPointsPerInch * scaleMul;
      hitArea.hitRadius = wPts / 2.0;
      hitArea.handleCenters.push_back(hitArea.center);
      hitArea.cornerCount = 1;
    } else if (cdn.is_linear() && pdfVerts.size() >= 2) {
      hitArea.isPolygon = true;
      hitArea.center = {0, 0};
      hitArea.hitRadius = 0;
      CGFloat thickPts = (cdn.thickness > 0 ? cdn.thickness : 1.0) /
                         scaleRatio * kPointsPerInch;
      if (thickPts < kMinThicknessPts)
        thickPts = kMinThicknessPts;
      thickPts = fmax(thickPts, kHitTolerancePts);
      CGFloat halfT = thickPts / 2.0;
      CGFloat x1 = pdfVerts[0][0], y1 = pdfVerts[0][1];
      CGFloat x2 = pdfVerts[1][0], y2 = pdfVerts[1][1];
      CGFloat dx = x2 - x1, dy = y2 - y1;
      CGFloat len = hypot(dx, dy);
      if (len > 0.001) {
        CGFloat px = -dy / len * halfT, py = dx / len * halfT;
        hitArea.polygon = {
            {x1 + px, y1 + py},
            {x1 - px, y1 - py},
            {x2 - px, y2 - py},
            {x2 + px, y2 + py},
        };
      }
      hitArea.handleCenters.push_back({x1, y1});
      hitArea.handleCenters.push_back({x2, y2});
      hitArea.cornerCount = 2;
    } else {
      hitArea.isPolygon = true;
      hitArea.center = {0, 0};
      hitArea.hitRadius = 0;
      size_t n = pdfVerts.size();
      for (const auto &v : pdfVerts) {
        CGPoint pt = {(CGFloat)v[0], (CGFloat)v[1]};
        hitArea.polygon.push_back(pt);
        hitArea.handleCenters.push_back(pt);
      }
      hitArea.cornerCount = (int)n;
      if (cdn.is_area() && n >= 3) {
        for (size_t i = 0; i < n; i++) {
          size_t j = (i + 1) % n;
          CGFloat mx = ((CGFloat)pdfVerts[i][0] + (CGFloat)pdfVerts[j][0]) / 2;
          CGFloat my = ((CGFloat)pdfVerts[i][1] + (CGFloat)pdfVerts[j][1]) / 2;
          hitArea.handleCenters.push_back({mx, my});
        }
      }
    }
    _hitAreas.push_back(std::move(hitArea));
  }

  if (paths.empty() && negativeIndicators.empty())
    return;

  CGRect canvasBounds = _canvasView.bounds;
  TakeoffOverlayView *overlay =
      [[TakeoffOverlayView alloc] initWithFrame:canvasBounds
                                          paths:std::move(paths)
                             negativeIndicators:std::move(negativeIndicators)
                                      zoomScale:self.zoomScale];
  overlay.tag = kOverlayViewTag;
  overlay.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [_canvasView addSubview:overlay];
}

#pragma mark - Path Builders

- (void)_buildLinearPaths:(const std::vector<ost_types::Vec2> &)verts
                thickness:(double)thickness
               scaleRatio:(double)scaleRatio
                        r:(float)r
                        g:(float)g
                        b:(float)b
                        a:(float)a
                    paths:(std::vector<OverlayPath> &)paths {
  if (verts.size() < 2)
    return;

  CGFloat thickPts =
      (thickness > 0 ? thickness : 1.0) / scaleRatio * kPointsPerInch;
  if (thickPts < kMinThicknessPts)
    thickPts = kMinThicknessPts;
  CGFloat halfThick = thickPts / 2.0;

  for (size_t i = 0; i + 1 < verts.size(); i++) {
    CGFloat x1 = verts[i][0], y1 = verts[i][1];
    CGFloat x2 = verts[i + 1][0], y2 = verts[i + 1][1];

    CGFloat dx = x2 - x1;
    CGFloat dy = y2 - y1;
    CGFloat len = hypot(dx, dy);
    if (len < 0.001)
      continue;

    CGFloat px = -dy / len * halfThick;
    CGFloat py = dx / len * halfThick;

    OverlayPath op;
    op.points = {
        {x1 + px, y1 + py},
        {x1 - px, y1 - py},
        {x2 - px, y2 - py},
        {x2 + px, y2 + py},
    };
    op.r = r;
    op.g = g;
    op.b = b;
    op.a = a;
    op.filled = true;
    op.isEllipse = false;
    op.lineWidth = 0;
    paths.push_back(std::move(op));
  }
}

- (void)_buildAreaPath:(const std::vector<ost_types::Vec2> &)verts
                     r:(float)r
                     g:(float)g
                     b:(float)b
                     a:(float)a
                 paths:(std::vector<OverlayPath> &)paths {
  if (verts.size() < 3)
    return;

  OverlayPath op;
  op.points.reserve(verts.size());
  for (const auto &v : verts) {
    op.points.push_back({(CGFloat)v[0], (CGFloat)v[1]});
  }
  op.r = r;
  op.g = g;
  op.b = b;
  op.a = a;
  op.filled = true;
  op.isEllipse = false;
  op.lineWidth = kOverlayStrokeWidthLocal;
  paths.push_back(std::move(op));
}

- (void)_buildCountPath:(const std::vector<ost_types::Vec2> &)verts
                  shape:(ost_domain::ShapeId)shape
                  width:(double)width
                  depth:(double)depth
            displaySize:(double)displaySize
               rotation:(double)rotation
             scaleRatio:(double)scaleRatio
                      r:(float)r
                      g:(float)g
                      b:(float)b
                      a:(float)a
                  paths:(std::vector<OverlayPath> &)paths {
  if (verts.empty())
    return;

  CGFloat cx = verts[0][0];
  CGFloat cy = verts[0][1];

  CGFloat scaleMul = (displaySize > 0 ? displaySize : 100.0) / 100.0;
  CGFloat wPts =
      (width > 0 ? width : 1.0) / scaleRatio * kPointsPerInch * scaleMul;
  CGFloat hPts = (depth > 0   ? depth
                  : width > 0 ? width
                              : 1.0) /
                 scaleRatio * kPointsPerInch * scaleMul;

  if (shape == ost_domain::ShapeId::Square ||
      shape == ost_domain::ShapeId::Circle) {
    hPts = wPts;
  }

  if (wPts < kMinCountSizePts)
    wPts = kMinCountSizePts;
  if (hPts < kMinCountSizePts)
    hPts = kMinCountSizePts;

  CGFloat rx = wPts / 2.0;
  CGFloat ry = hPts / 2.0;

  OverlayPath op;
  op.r = r;
  op.g = g;
  op.b = b;
  op.a = a;
  op.filled = true;
  op.lineWidth = kOverlayStrokeWidthLocal;

  using SI = ost_domain::ShapeId;

  switch (shape) {
  case SI::Circle:
  case SI::Ellipse: {
    op.isEllipse = true;
    op.points = {
        {cx - rx, cy - ry},
        {cx + rx, cy + ry},
    };
    break;
  }
  case SI::Triangle: {
    op.isEllipse = false;
    op.points = {
        {cx, cy - ry},
        {cx - rx, cy + ry},
        {cx + rx, cy + ry},
    };
    break;
  }
  case SI::Rhombus: {
    op.isEllipse = false;
    CGFloat rhombRx = rx * 0.575;
    op.points = {
        {cx, cy - ry},
        {cx + rhombRx, cy},
        {cx, cy + ry},
        {cx - rhombRx, cy},
    };
    break;
  }
  case SI::Hexagon: {
    op.isEllipse = false;
    op.points = regularPolygonPoints(cx, cy, rx, ry, 6);
    break;
  }
  case SI::Octagon: {
    op.isEllipse = false;
    op.points = regularPolygonPoints(cx, cy, rx, ry, 8);
    break;
  }
  case SI::Square:
  case SI::Rectangle:
  default: {
    op.isEllipse = false;
    op.points = {
        {cx - rx, cy - ry},
        {cx + rx, cy - ry},
        {cx + rx, cy + ry},
        {cx - rx, cy + ry},
    };
    break;
  }
  }

  if (rotation != 0.0 && !op.isEllipse) {
    CGFloat cosR = cos(rotation);
    CGFloat sinR = sin(rotation);
    for (auto &pt : op.points) {
      CGFloat dx = pt.x - cx;
      CGFloat dy = pt.y - cy;
      pt.x = cx + dx * cosR - dy * sinR;
      pt.y = cy + dx * sinR + dy * cosR;
    }
  }

  paths.push_back(std::move(op));
}

- (void)_buildSelectionHandles:(const std::vector<ost_types::Vec2> &)verts
                     condition:(const ost_domain::Condition &)cdn
                  activeHandle:(int)activeHandle
                         paths:(std::vector<OverlayPath> &)paths {
  int idx = 0;
  if (cdn.is_linear() && verts.size() >= 2) {
    addSquareHandle({(CGFloat)verts[0][0], (CGFloat)verts[0][1]}, kCornerHalf,
                    activeHandle == idx++, paths);
    addSquareHandle({(CGFloat)verts[1][0], (CGFloat)verts[1][1]}, kCornerHalf,
                    activeHandle == idx++, paths);
  } else if (cdn.is_area()) {
    size_t n = verts.size();
    for (size_t i = 0; i < n; i++) {
      addSquareHandle({(CGFloat)verts[i][0], (CGFloat)verts[i][1]}, kCornerHalf,
                      activeHandle == idx++, paths);
    }
    for (size_t i = 0; i < n; i++) {
      size_t j = (i + 1) % n;
      CGFloat mx = ((CGFloat)verts[i][0] + (CGFloat)verts[j][0]) / 2;
      CGFloat my = ((CGFloat)verts[i][1] + (CGFloat)verts[j][1]) / 2;
      addSquareHandle({mx, my}, kMidHalf, activeHandle == idx++, paths);
    }
  } else if (cdn.is_count() && !verts.empty()) {
    addSquareHandle({(CGFloat)verts[0][0], (CGFloat)verts[0][1]}, kCornerHalf,
                    activeHandle == idx++, paths);
  }
}

- (void)_buildMultiSelectionBorder:(const std::vector<ost_types::Vec2> &)verts
                         condition:(const ost_domain::Condition &)cdn
                        scaleRatio:(double)scaleRatio
                          rotation:(double)rotation
                             paths:(std::vector<OverlayPath> &)paths {
  if (verts.empty())
    return;

  auto makeBorder = [](std::vector<CGPoint> pts, bool close) {
    OverlayPath border;
    border.filled = false;
    border.isEllipse = false;
    border.r = 1.0f;
    border.g = 0.86f;
    border.b = 0.0f;
    border.a = 1.0f;
    border.strokeR = 1.0f;
    border.strokeG = 0.86f;
    border.strokeB = 0.0f;
    border.lineWidth = 2.0;
    border.points = std::move(pts);
    if (close && border.points.size() >= 3) {
      border.points.push_back(border.points.front());
    }
    return border;
  };

  if (cdn.is_linear() && verts.size() >= 2) {
    CGFloat thickPts =
        (cdn.thickness > 0 ? cdn.thickness : 1.0) / scaleRatio * kPointsPerInch;
    if (thickPts < kMinThicknessPts)
      thickPts = kMinThicknessPts;
    CGFloat halfThick = thickPts / 2.0;

    for (size_t i = 0; i + 1 < verts.size(); i++) {
      CGFloat x1 = verts[i][0], y1 = verts[i][1];
      CGFloat x2 = verts[i + 1][0], y2 = verts[i + 1][1];
      CGFloat dx = x2 - x1, dy = y2 - y1;
      CGFloat len = hypot(dx, dy);
      if (len < 0.001)
        continue;
      CGFloat px = -dy / len * halfThick;
      CGFloat py = dx / len * halfThick;
      paths.push_back(makeBorder({{x1 + px, y1 + py},
                                  {x2 + px, y2 + py},
                                  {x2 - px, y2 - py},
                                  {x1 - px, y1 - py}},
                                 true));
    }
  } else if (cdn.is_area() && verts.size() >= 3) {
    std::vector<CGPoint> pts;
    for (const auto &v : verts)
      pts.push_back({(CGFloat)v[0], (CGFloat)v[1]});
    paths.push_back(makeBorder(std::move(pts), true));
  } else if (cdn.is_count()) {
    OverlayPath shape;
    shape.r = 1.0f;
    shape.g = 0.86f;
    shape.b = 0.0f;
    shape.a = 1.0f;
    shape.strokeR = 1.0f;
    shape.strokeG = 0.86f;
    shape.strokeB = 0.0f;
    shape.lineWidth = 2.0;
    shape.filled = false;

    CGFloat cx = verts[0][0], cy = verts[0][1];
    CGFloat scaleMul =
        (cdn.display_size > 0 ? cdn.display_size : 100.0) / 100.0;
    CGFloat wPts = (cdn.width > 0 ? cdn.width : 1.0) / scaleRatio *
                   kPointsPerInch * scaleMul;
    CGFloat hPts = (cdn.depth > 0   ? cdn.depth
                    : cdn.width > 0 ? cdn.width
                                    : 1.0) /
                   scaleRatio * kPointsPerInch * scaleMul;
    if (cdn.shape == ost_domain::ShapeId::Square ||
        cdn.shape == ost_domain::ShapeId::Circle)
      hPts = wPts;
    if (wPts < kMinCountSizePts)
      wPts = kMinCountSizePts;
    if (hPts < kMinCountSizePts)
      hPts = kMinCountSizePts;
    CGFloat rx = wPts / 2.0, ry = hPts / 2.0;

    using SI = ost_domain::ShapeId;
    switch (cdn.shape) {
    case SI::Circle:
    case SI::Ellipse:
      shape.isEllipse = true;
      shape.points = {{cx - rx, cy - ry}, {cx + rx, cy + ry}};
      break;
    case SI::Triangle:
      shape.isEllipse = false;
      shape.points = {
          {cx, cy - ry}, {cx - rx, cy + ry}, {cx + rx, cy + ry}, {cx, cy - ry}};
      break;
    case SI::Rhombus: {
      CGFloat rhombRx = rx * 0.575;
      shape.isEllipse = false;
      shape.points = {{cx, cy - ry},
                      {cx + rhombRx, cy},
                      {cx, cy + ry},
                      {cx - rhombRx, cy},
                      {cx, cy - ry}};
      break;
    }
    case SI::Hexagon:
      shape.isEllipse = false;
      shape.points = regularPolygonPoints(cx, cy, rx, ry, 6);
      shape.points.push_back(shape.points.front());
      break;
    case SI::Octagon:
      shape.isEllipse = false;
      shape.points = regularPolygonPoints(cx, cy, rx, ry, 8);
      shape.points.push_back(shape.points.front());
      break;
    default:
      shape.isEllipse = false;
      shape.points = {{cx - rx, cy - ry},
                      {cx + rx, cy - ry},
                      {cx + rx, cy + ry},
                      {cx - rx, cy + ry},
                      {cx - rx, cy - ry}};
      break;
    }

    if (rotation != 0.0 && !shape.isEllipse) {
      CGFloat cosR = cos(rotation), sinR = sin(rotation);
      for (auto &pt : shape.points) {
        CGFloat dx = pt.x - cx, dy = pt.y - cy;
        pt.x = cx + dx * cosR - dy * sinR;
        pt.y = cy + dx * sinR + dy * cosR;
      }
    }
    paths.push_back(std::move(shape));
  }
}

#pragma mark - Edit Mode

- (void)setEditEnabled:(BOOL)editEnabled {
  _editEnabled = editEnabled;
  _editTap.enabled = editEnabled;
  _editLongPress.enabled = editEnabled;
  _editPan.enabled = editEnabled;
  if (!editEnabled)
    _draggingUID.clear();
}

- (bool)_hitArea:(const TakeoffHitArea &)area
         atPoint:(CGPoint)pt
     toleranceSq:(CGFloat)tolSq {
  if (area.isPolygon) {
    if (pointInPolygon(pt, area.polygon))
      return true;
    for (size_t i = 0; i < area.polygon.size(); i++) {
      size_t j = (i + 1) % area.polygon.size();
      if (pointToSegmentDistSq(pt, area.polygon[i], area.polygon[j]) < tolSq)
        return true;
    }
  } else {
    CGFloat dx = pt.x - area.center.x;
    CGFloat dy = pt.y - area.center.y;
    CGFloat r = fmax(area.hitRadius, sqrt(tolSq));
    if (dx * dx + dy * dy < r * r)
      return true;
  }
  return false;
}

- (std::vector<std::string>)_allHitsAtCanvasPoint:(CGPoint)pt {
  CGFloat tolerance = kHitTolerancePts / self.zoomScale;
  CGFloat tolSq = tolerance * tolerance;
  std::vector<std::string> hits;
  for (auto it = _hitAreas.rbegin(); it != _hitAreas.rend(); ++it) {
    if ([self _hitArea:*it atPoint:pt toleranceSq:tolSq])
      hits.push_back(it->uid);
  }
  return hits;
}

- (std::string)_hitTestAtCanvasPoint:(CGPoint)pt {
  auto hits = [self _allHitsAtCanvasPoint:pt];
  return hits.empty() ? std::string{} : hits[0];
}

- (NSString *)takeoffUIDAtPoint:(CGPoint)viewPoint {
  if (!_canvasView)
    return nil;
  CGPoint canvasPt = [self convertPoint:viewPoint toView:_canvasView];
  std::string uid = [self _hitTestAtCanvasPoint:canvasPt];
  if (uid.empty())
    return nil;
  return [NSString stringWithUTF8String:uid.c_str()];
}

- (int)_handleHitForTakeoff:(const std::string &)uid atPoint:(CGPoint)pt {
  CGFloat zoom = self.zoomScale;
  CGFloat cornerTol = (kCornerHalf + 18.0) / zoom;
  CGFloat midTol = (kMidHalf + 18.0) / zoom;
  for (const auto &area : _hitAreas) {
    if (area.uid != uid)
      continue;
    for (size_t i = 0; i < area.handleCenters.size(); i++) {
      CGFloat dx = pt.x - area.handleCenters[i].x;
      CGFloat dy = pt.y - area.handleCenters[i].y;
      CGFloat tol = ((int)i < area.cornerCount) ? cornerTol : midTol;
      if (dx * dx + dy * dy < tol * tol)
        return (int)i;
    }
    break;
  }
  return -1;
}

- (void)_handleEditTap:(UITapGestureRecognizer *)tap {
  if (tap.state != UIGestureRecognizerStateEnded)
    return;
  if (!_canvasView || !_editDelegate)
    return;

  CGPoint canvasPt = [tap locationInView:_canvasView];

  if (!_lastTappedUID.empty()) {
    int hi = [self _handleHitForTakeoff:_lastTappedUID atPoint:canvasPt];
    if (hi >= 0) {
      [_editDelegate planView:self didTapHandle:hi ofTakeoff:_lastTappedUID];
      return;
    }
  }

  auto hits = [self _allHitsAtCanvasPoint:canvasPt];

  if (hits.empty()) {
    _lastTappedUID.clear();
    [_editDelegate planView:self didTapEmptySpaceAtCanvasPoint:canvasPt];
    return;
  }

  std::string pick = hits[0];
  for (size_t i = 0; i < hits.size(); i++) {
    if (hits[i] == _lastTappedUID) {
      pick = hits[(i + 1) % hits.size()];
      break;
    }
  }
  _lastTappedUID = pick;
  [_editDelegate planView:self didTapTakeoff:pick];
}

- (void)_handleEditLongPress:(UILongPressGestureRecognizer *)lp {
  if (lp.state != UIGestureRecognizerStateBegan)
    return;
  if (!_canvasView || !_editDelegate)
    return;

  CGPoint canvasPt = [lp locationInView:_canvasView];
  auto hits = [self _allHitsAtCanvasPoint:canvasPt];

  if (hits.empty()) {
    if ([_editDelegate
            respondsToSelector:@selector(
                                   planView:didLongPressEmptySpaceAtPoint:)]) {
      CGPoint screenPt = [lp locationInView:self];
      [_editDelegate planView:self didLongPressEmptySpaceAtPoint:screenPt];
    }
    return;
  }

  std::string hit = hits[0];
  if ([_editDelegate
          respondsToSelector:@selector(
                                 planView:didLongPressTakeoff:atPoint:)]) {
    CGPoint screenPt = [lp locationInView:self];
    [_editDelegate planView:self didLongPressTakeoff:hit atPoint:screenPt];
  }
}

- (void)_handleEditPan:(UIPanGestureRecognizer *)pan {
  if (!_canvasView || !_editDelegate)
    return;

  CGPoint canvasPt = [pan locationInView:_canvasView];

  if (pan.state == UIGestureRecognizerStateBegan) {

    _draggingUID = _lastTappedUID;
    _dragStartCanvasPoint = canvasPt;
  } else if (pan.state == UIGestureRecognizerStateChanged) {
    if (_draggingUID.empty())
      return;

    CGPoint delta = {canvasPt.x - _dragStartCanvasPoint.x,
                     canvasPt.y - _dragStartCanvasPoint.y};
    [_editDelegate planView:self
             didDragTakeoff:_draggingUID
                handleIndex:-1
                canvasDelta:delta];
  } else {
    if (!_draggingUID.empty()) {
      [_editDelegate planView:self didEndDragTakeoff:_draggingUID];
      _draggingUID.clear();
    }
  }
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gr {
  if (gr == _editPan) {
    if (!_canvasView)
      return NO;
    CGPoint pt = [gr locationInView:_canvasView];
    CGFloat tolerance = kHitTolerancePts / self.zoomScale;
    CGFloat tolSq = tolerance * tolerance;

    if (!_lastTappedUID.empty()) {
      for (const auto &area : _hitAreas) {
        if (area.uid != _lastTappedUID)
          continue;
        return [self _hitArea:area atPoint:pt toleranceSq:tolSq];
      }
    }

    if (_selectedConditionUID) {
      std::string condUID = std::string(_selectedConditionUID.UTF8String);
      for (const auto &area : _hitAreas) {
        if (area.conditionUID != condUID)
          continue;
        if ([self _hitArea:area atPoint:pt toleranceSq:tolSq]) {
          _draggingUID = area.uid;
          return YES;
        }
      }
    }
    return NO;
  }
  if (gr == _placePan) {
    return _placementActive && _canvasView != nil;
  }
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gr
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer *)other {
  return NO;
}

#pragma mark - UIScrollViewDelegate

- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView {
  return _canvasView;
}

- (void)scrollViewDidZoom:(UIScrollView *)scrollView {
  [self _centerCanvas];

  CGFloat zoom = self.zoomScale;
  TakeoffOverlayView *overlay =
      (TakeoffOverlayView *)[_canvasView viewWithTag:kOverlayViewTag];
  if (overlay)
    [overlay updateZoomScale:zoom];
  if (_placePreviewView &&
      [_placePreviewView isKindOfClass:[TakeoffOverlayView class]]) {
    [(TakeoffOverlayView *)_placePreviewView updateZoomScale:zoom];
  }
}

#pragma mark - Layout

- (void)fitToPage {
  if (!_canvasView)
    return;

  CGSize viewSize = self.bounds.size;
  if (viewSize.width <= 0 || viewSize.height <= 0)
    return;

  CGSize canvasSize = _canvasView.frame.size;
  if (canvasSize.width <= 0 || canvasSize.height <= 0)
    return;

  CGFloat scaleX = viewSize.width / canvasSize.width;
  CGFloat scaleY = viewSize.height / canvasSize.height;
  CGFloat fitScale = MIN(scaleX, scaleY);
  fitScale = MAX(kMinZoom, MIN(kMaxZoom, fitScale));

  if (fabs(self.zoomScale - fitScale) < 0.001)
    return;

  self.minimumZoomScale = MIN(kMinZoom, fitScale);
  self.zoomScale = fitScale;

  [self _centerCanvas];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self _centerCanvas];
}

#pragma mark - Private

- (void)_centerCanvas {
  if (!_canvasView)
    return;

  CGSize viewSize = self.bounds.size;
  CGSize contentSize = self.contentSize;

  CGFloat hInset = MAX(0, (viewSize.width - contentSize.width) / 2.0);
  CGFloat vInset = MAX(0, (viewSize.height - contentSize.height) / 2.0);

  self.contentInset = UIEdgeInsetsMake(vInset, hInset, vInset, hInset);
}

@end
