#include "../Core/Domain/DisplayColor.hpp"
#include "../Core/Domain/Layer.hpp"
#include "../Core/Geometry/coord_transform.hpp"
#include "../Core/Geometry/page_transform.hpp"
#include "../Core/Geometry/page_view.hpp"
#include "../Core/Geometry/takeoff_footprint.hpp"
#include "../Core/Meshing/TakeoffMeshBuilder.hpp"
#import "Components/PDFCanvasView.h"
#import "PlanView_Internal.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

static const CGFloat kMinZoom = 0.05;
static const CGFloat kMaxZoom = 16.0;

static const int kOverlayViewTag = 9001;
static const int kPdfViewTag = 9002;

static const CGFloat kHitTolerancePts = 10.0;

// RPV-17: Windows draws an attachment like a count item in 2D
// (takeoff_renderer._create_path: condition.is_count or condition.is_attachment).
static bool isCountLike(const ost_domain::Condition &cdn) {
  return cdn.is_count() || cdn.is_attachment();
}

static std::vector<CGPoint> toCGPoints(const std::vector<ost_types::Vec2> &pts) {
  std::vector<CGPoint> out;
  out.reserve(pts.size());
  for (const auto &p : pts)
    out.push_back(CGPointMake((CGFloat)p[0], (CGFloat)p[1]));
  return out;
}

// The canvas points of the first `maxPairs` (x, y) pairs of a stored position: where the handles of a takeoff sit. Throws
// std::invalid_argument for an invalid page contract.
static std::vector<CGPoint>
canvasPositionPoints(const std::vector<double> &position, size_t maxPairs,
                     const ost_footprint::PageFrame &frame) {
  std::vector<CGPoint> pts;
  size_t pairs = std::min(position.size() / 2, maxPairs);
  pts.reserve(pairs);
  for (size_t i = 0; i < pairs; i++) {
    ost_types::Vec2 p = ost_coord::ost_to_canvas_point(
        position[2 * i], position[2 * i + 1], frame.width_pts, frame.height_pts,
        frame.scale_factor1, frame.scale_factor2, frame.rotation, frame.flip_x,
        frame.flip_y);
    pts.push_back(CGPointMake((CGFloat)p[0], (CGFloat)p[1]));
  }
  return pts;
}

// The midpoints of the edges of an area (the edge handles), in edge order.
static void appendAreaMidpoints(const std::vector<CGPoint> &vertices,
                                std::vector<CGPoint> &out) {
  size_t n = vertices.size();
  if (n < 3)
    return;
  for (size_t i = 0; i < n; i++) {
    size_t j = (i + 1) % n;
    out.push_back(CGPointMake((vertices[i].x + vertices[j].x) / 2,
                              (vertices[i].y + vertices[j].y) / 2));
  }
}

// A filled takeoff outline (Windows: path fill with the colour at the 2D opacity and the 2.0 cosmetic pen in the colour).
static void appendFootprintPath(const std::vector<CGPoint> &outline,
                                const std::vector<std::vector<CGPoint>> &holes,
                                float r, float g, float b, float a,
                                std::vector<OverlayPath> &paths) {
  if (outline.size() < 3)
    return;
  OverlayPath op;
  op.points = outline;
  op.holes = holes;
  op.r = r;
  op.g = g;
  op.b = b;
  op.a = a;
  op.filled = true;
  op.lineWidth = 0; // the takeoff outline pen
  paths.push_back(std::move(op));
}

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
  if (n == 0)
    return false;
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
  _pageWidthPts = ost_page_view::kDefaultPageWidthPts;
  _pageHeightPts = ost_page_view::kDefaultPageHeightPts;
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

- (void)loadPage:(const ost_domain::Page &)page resetZoom:(BOOL)resetZoom {
  // Windows build_page_info: `scale_factor or 1.0`
  _scaleFactor1 = page.scale_factor1 != 0.0 ? page.scale_factor1 : 1.0;
  _scaleFactor2 = page.scale_factor2 != 0.0 ? page.scale_factor2 : 1.0;
  _pageRotation = page.rotation;
  // the flips of the displayed page: the background, the overlays and the placement maps share them
  _pageFlipX = page.flip_x;
  _pageFlipY = page.flip_y;

  // LP-16: the PDF page BidPages.Index1 shows, and the size that page really has
  PDFCanvasView *pdfView = nil;
  if (!page.image_path.empty()) {
    NSString *pdfPath = [NSString stringWithUTF8String:page.image_path.c_str()];
    if (pdfPath && [[NSFileManager defaultManager] fileExistsAtPath:pdfPath]) {
      PDFCanvasView *candidate =
          [[PDFCanvasView alloc] initWithPdfPath:pdfPath
                                       pageIndex:page.page_index];
      if (candidate.pageSize.width > 0 && candidate.pageSize.height > 0)
        pdfView = candidate;
    }
  }
  CGSize pdfSize = pdfView ? pdfView.pageSize : CGSizeZero;

  // the size of the page: the PDF's own, else BidPages Width / Height, else 612 x 792
  const auto pageSize = ost_page_view::resolve_pdf_page_size(
      page.width, page.height, pdfSize.width, pdfSize.height);
  _pageWidthPts = pageSize[0];
  _pageHeightPts = pageSize[1];

  // The canvas is the DISPLAYED page: the page mirrored by the flips and turned counter-clockwise by the rotation, width
  // and height swapped for 90 / 270 (Windows scene page transform).
  CGFloat canvasW = _pageWidthPts;
  CGFloat canvasH = _pageHeightPts;
  CGAffineTransform pageTransform = CGAffineTransformIdentity;
  try {
    const auto canvasSize = ost_page_transform::output_dimensions(
        _pageWidthPts, _pageHeightPts, _pageRotation);
    const auto affine = ost_page_view::page_affine(
        _pageWidthPts, _pageHeightPts, _pageRotation, _pageFlipX, _pageFlipY);
    canvasW = canvasSize[0];
    canvasH = canvasSize[1];
    pageTransform = CGAffineTransformMake(affine.a, affine.b, affine.c,
                                          affine.d, affine.tx, affine.ty);
  } catch (const std::invalid_argument &) {
    // a rotation that is not a multiple of 90 degrees: the unrotated page; no takeoff is drawn on it (renderOverlays)
  }

  CGFloat savedZoom = self.zoomScale;
  CGPoint savedOffset = self.contentOffset;
  CGSize savedContent = self.contentSize;

  [_canvasView removeFromSuperview];

  _canvasView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, canvasW, canvasH)];
  _canvasView.backgroundColor = [UIColor whiteColor];

  if (pdfView) {
    pdfView.frame = CGRectMake(0, 0, canvasW, canvasH);
    pdfView.pageTransform = pageTransform;
    pdfView.tag = kPdfViewTag;
    pdfView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [_canvasView addSubview:pdfView];
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

- (ost_footprint::PageFrame)_pageFrame {
  ost_footprint::PageFrame frame;
  frame.width_pts = _pageWidthPts;
  frame.height_pts = _pageHeightPts;
  frame.scale_factor1 = _scaleFactor1;
  frame.scale_factor2 = _scaleFactor2;
  frame.rotation = _pageRotation;
  frame.flip_x = _pageFlipX;
  frame.flip_y = _pageFlipY;
  return frame;
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

  // LP-15: the PDF is drawn while the Image layer is shown (Windows Page.layer_visible); the white canvas and the takeoffs
  // stay either way
  [_canvasView viewWithTag:kPdfViewTag].hidden =
      !ost_domain::is_image_layer_visible(project.layers);

  UIView *oldOverlay = [_canvasView viewWithTag:kOverlayViewTag];
  [oldOverlay removeFromSuperview];

  if (project.pages.find(pageUID) == project.pages.end())
    return;

  const ost_footprint::PageFrame frame = [self _pageFrame];

  // RPV-17: only an area takeoff whose parent is an area takeoff of the page is
  // a hole (Windows group_area_takeoffs_with_holes); a child of any other kind
  // (an attachment, a linear item, ...) is drawn like every other takeoff.
  std::vector<const ost_domain::Takeoff *> pageTakeoffs;
  for (const auto &tk : project.takeoffs) {
    if (tk.page_uid == pageUID)
      pageTakeoffs.push_back(&tk);
  }
  const ost_meshing::AreaHoleGrouping holeGrouping =
      ost_meshing::group_area_takeoffs_with_holes(pageTakeoffs,
                                                  project.conditions);
  const auto &holeMap = holeGrouping.holes;
  std::unordered_set<std::string> holeUIDs;
  for (const auto &entry : holeMap) {
    for (const auto *hole : entry.second)
      holeUIDs.insert(hole->uid);
  }

  std::vector<OverlayPath> paths;
  std::vector<NegativeIndicator> negativeIndicators;
  _hitAreas.clear();

  // The selection of a takeoff: handles for the selected takeoff, a border for every takeoff of the selected condition.
  auto appendSelection = [&](const ost_domain::Takeoff &takeoff,
                             const ost_domain::Condition &cdn,
                             const std::vector<CGPoint> &outline,
                             const std::vector<CGPoint> &positionPoints) {
    bool isSingleSelected =
        (!selectedUID.empty() && takeoff.uid == selectedUID);
    bool isMultiSelected = (selectedUID.empty() && !selectedCondUID.empty() &&
                            takeoff.condition_uid == selectedCondUID);
    if (isSingleSelected) {
      [self _buildSelectionHandles:positionPoints
                         condition:cdn
                      activeHandle:activeHandle
                             paths:paths];
    } else if (isMultiSelected) {
      [self _buildMultiSelectionBorder:outline paths:paths];
    }
  };

  for (const auto &takeoff : project.takeoffs) {
    if (takeoff.page_uid != pageUID)
      continue;

    if (holeUIDs.count(takeoff.uid))
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

    // COL-02: the gray byte of Windows, the same as in the 3D view and exports
    if (config.grayscale_enabled)
      ost_domain::grayscale_channels(r, g, b);

    // COL-04: the 2D fill opacity of Windows (pattern 0: no fill, only the outline)
    float alpha =
        (float)ost_domain::display_opacity_2d(config.color_mode, (int)cdn.pattern);

    // The footprint on the canvas, built in the unrotated page like the Windows scene and then turned and mirrored with the
    // page (RPV-25, RPV-26, RPV-M04, GEO-18); an invalid page contract draws nothing (GEO-06).
    std::vector<CGPoint> outline, hitOutline, positionPoints;
    std::vector<std::vector<CGPoint>> holePolygons;
    std::vector<std::vector<double>> holePositions;
    std::optional<ost_types::Vec2> markerAnchor;
    try {
      outline = toCGPoints(ost_footprint::canvas_footprint(takeoff, cdn, frame));
      if (outline.empty())
        continue;

      // the backouts of an area: cut out of its fill (RPV-31)
      auto holeIt = holeMap.find(takeoff.uid);
      if (cdn.is_area() && holeIt != holeMap.end()) {
        for (const auto *hole : holeIt->second) {
          auto holeCdnIt = project.conditions.find(hole->condition_uid);
          if (holeCdnIt == project.conditions.end())
            continue;
          std::vector<CGPoint> polygon = toCGPoints(
              ost_footprint::canvas_footprint(*hole, holeCdnIt->second, frame));
          if (polygon.empty())
            continue;
          holePolygons.push_back(std::move(polygon));
          holePositions.push_back(hole->position);
        }
      }

      // a touch hits a linear band that is at least kHitTolerancePts wide
      hitOutline = outline;
      if (cdn.is_linear()) {
        std::vector<CGPoint> wide = toCGPoints(ost_footprint::canvas_footprint(
            takeoff, cdn, frame, kHitTolerancePts * frame.view_scale));
        if (!wide.empty())
          hitOutline = std::move(wide);
      }

      // the handles sit on the stored points: both ends of a linear, the position of a count item, every corner of an area
      positionPoints = canvasPositionPoints(
          takeoff.position, cdn.is_area() ? takeoff.position.size() / 2 : cdn.is_linear() ? 2 : 1,
          frame);

      // RPV-27: the marker of a negative takeoff
      if (takeoff.is_negative)
        markerAnchor =
            ost_footprint::canvas_marker_anchor(takeoff, cdn, holePositions, frame);
    } catch (const std::invalid_argument &) {
      continue;
    }

    appendFootprintPath(outline, holePolygons, r, g, b, alpha, paths);
    appendSelection(takeoff, cdn, outline, positionPoints);

    if (markerAnchor) {
      negativeIndicators.push_back(
          {CGPointMake((CGFloat)(*markerAnchor)[0], (CGFloat)(*markerAnchor)[1])});
    }

    TakeoffHitArea hitArea;
    hitArea.uid = takeoff.uid;
    hitArea.conditionUID = takeoff.condition_uid;
    hitArea.polygon = std::move(hitOutline);
    hitArea.holes = std::move(holePolygons);
    hitArea.handleCenters = positionPoints;
    hitArea.cornerCount = (int)positionPoints.size();
    if (cdn.is_area())
      appendAreaMidpoints(positionPoints, hitArea.handleCenters);
    _hitAreas.push_back(std::move(hitArea));
  }

  // RPV-31: a hole is a takeoff of its own, invisible apart from the cut-out it makes in its parent, and a tap inside it
  // selects it (not the parent). Windows adds these items after all others, so they are on top.
  for (const auto &takeoff : project.takeoffs) {
    if (takeoff.page_uid != pageUID || !holeUIDs.count(takeoff.uid))
      continue;
    auto cdnIt = project.conditions.find(takeoff.condition_uid);
    if (cdnIt == project.conditions.end())
      continue;
    const auto &cdn = cdnIt->second;
    if (!project.is_condition_visible(cdn))
      continue;

    std::vector<CGPoint> outline, positionPoints;
    try {
      outline = toCGPoints(ost_footprint::canvas_footprint(takeoff, cdn, frame));
      positionPoints = canvasPositionPoints(
          takeoff.position, takeoff.position.size() / 2, frame);
    } catch (const std::invalid_argument &) {
      continue;
    }
    if (outline.empty())
      continue;

    appendSelection(takeoff, cdn, outline, positionPoints);

    TakeoffHitArea hitArea;
    hitArea.uid = takeoff.uid;
    hitArea.conditionUID = takeoff.condition_uid;
    hitArea.polygon = outline;
    hitArea.handleCenters = positionPoints;
    hitArea.cornerCount = (int)positionPoints.size();
    appendAreaMidpoints(positionPoints, hitArea.handleCenters);
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

- (void)_buildSelectionHandles:(const std::vector<CGPoint> &)points
                     condition:(const ost_domain::Condition &)cdn
                  activeHandle:(int)activeHandle
                         paths:(std::vector<OverlayPath> &)paths {
  int idx = 0;
  if (cdn.is_linear() && points.size() >= 2) {
    addSquareHandle(points[0], kCornerHalf, activeHandle == idx++, paths);
    addSquareHandle(points[1], kCornerHalf, activeHandle == idx++, paths);
  } else if (cdn.is_area()) {
    for (const CGPoint &p : points)
      addSquareHandle(p, kCornerHalf, activeHandle == idx++, paths);
    std::vector<CGPoint> mids;
    appendAreaMidpoints(points, mids);
    for (const CGPoint &m : mids)
      addSquareHandle(m, kMidHalf, activeHandle == idx++, paths);
  } else if (isCountLike(cdn) && !points.empty()) {
    addSquareHandle(points[0], kCornerHalf, activeHandle == idx++, paths);
  }
}

- (void)_buildMultiSelectionBorder:(const std::vector<CGPoint> &)outline
                             paths:(std::vector<OverlayPath> &)paths {
  if (outline.size() < 2)
    return;
  OverlayPath border;
  border.filled = false;
  border.r = 1.0f;
  border.g = 0.86f;
  border.b = 0.0f;
  border.a = 1.0f;
  border.strokeR = 1.0f;
  border.strokeG = 0.86f;
  border.strokeB = 0.0f;
  border.lineWidth = 2.0;
  border.points = outline;
  paths.push_back(std::move(border));
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
  if (pointInPolygon(pt, area.polygon)) {
    // the backouts of an area are not on it: a tap inside one belongs to the hole takeoff (RPV-31)
    bool inHole = false;
    for (const auto &hole : area.holes) {
      if (pointInPolygon(pt, hole)) {
        inHole = true;
        break;
      }
    }
    if (!inHole)
      return true;
  }
  for (size_t i = 0; i < area.polygon.size(); i++) {
    size_t j = (i + 1) % area.polygon.size();
    if (pointToSegmentDistSq(pt, area.polygon[i], area.polygon[j]) < tolSq)
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

- (void)clearTapSelection {
  _lastTappedUID.clear();
  _draggingUID.clear();
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
    // where the touch went down, not where the pan was recognised (RPV-M07)
    _dragStartCanvasPoint = PlanViewPanStartPoint(pan, _canvasView);
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
