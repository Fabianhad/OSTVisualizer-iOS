#import "TakeoffOverlayView.h"

// The Windows takeoff outline pen is 2.0 wide and cosmetic (takeoff_renderer._build_item line_width 2.0): the default of a path
// without a width, in screen points.
static const CGFloat kOverlayStrokeWidth = 2.0;
// Windows handle_style.py: the fill is white with alpha 224, the outline black.
static const float kHandleFillAlpha = 224.0f / 255.0f;
static const CGFloat kHandleOutlineWidth = 1.0;

void addSquareHandle(CGPoint c, CGFloat half, bool active,
                     std::vector<OverlayPath> &paths) {
  OverlayPath handle;
  handle.filled = true;
  if (active) {
    handle.r = 1.0f;
    handle.g = 0.6f;
    handle.b = 0.0f;
    handle.a = 1.0f;
  } else {
    handle.r = 1.0f;
    handle.g = 1.0f;
    handle.b = 1.0f;
    handle.a = kHandleFillAlpha;
  }
  handle.strokeR = 0.0f;
  handle.strokeG = 0.0f;
  handle.strokeB = 0.0f;
  handle.lineWidth = kHandleOutlineWidth;
  handle.handleHalf = half;
  handle.points = {c};
  paths.push_back(std::move(handle));
}

@implementation TakeoffOverlayView {
  std::vector<OverlayPath> _paths;
  std::vector<NegativeIndicator> _negativeIndicators;
  CGFloat _zoomScale;
}

- (instancetype)initWithFrame:(CGRect)frame
                        paths:(std::vector<OverlayPath>)paths
           negativeIndicators:(std::vector<NegativeIndicator>)indicators
                    zoomScale:(CGFloat)zoomScale {
  self = [super initWithFrame:frame];
  if (self) {
    _paths = std::move(paths);
    _negativeIndicators = std::move(indicators);
    _zoomScale = zoomScale;
    self.backgroundColor = [UIColor clearColor];
    self.opaque = NO;
    self.userInteractionEnabled = NO;
  }
  return self;
}

- (void)updateZoomScale:(CGFloat)zoomScale {
  if (fabs(_zoomScale - zoomScale) < 0.0001)
    return;
  _zoomScale = zoomScale;
  // strokes, handles and markers keep their size on the screen: redraw
  [self setNeedsDisplay];
}

static void addContour(CGMutablePathRef cgPath,
                       const std::vector<CGPoint> &points) {
  CGPathMoveToPoint(cgPath, NULL, points[0].x, points[0].y);
  for (size_t i = 1; i < points.size(); i++) {
    CGPathAddLineToPoint(cgPath, NULL, points[i].x, points[i].y);
  }
  CGPathCloseSubpath(cgPath);
}

- (void)drawRect:(CGRect)rect {
  CGContextRef ctx = UIGraphicsGetCurrentContext();
  if (!ctx)
    return;

  CGFloat invZoom = (_zoomScale > 0) ? (1.0 / _zoomScale) : 1.0;

  for (const auto &path : _paths) {
    CGFloat fillR = path.r, fillG = path.g, fillB = path.b;
    CGFloat fillA = path.a;
    CGFloat sR = path.strokeR >= 0 ? path.strokeR : fillR;
    CGFloat sG = path.strokeG >= 0 ? path.strokeG : fillG;
    CGFloat sB = path.strokeB >= 0 ? path.strokeB : fillB;
    CGFloat strokeWidth =
        (path.lineWidth > 0 ? path.lineWidth : kOverlayStrokeWidth) * invZoom;

    if (path.handleHalf > 0 && !path.points.empty()) {
      CGFloat half = path.handleHalf * invZoom;
      CGRect handleRect = CGRectMake(path.points[0].x - half,
                                     path.points[0].y - half, half * 2, half * 2);
      CGContextSetRGBFillColor(ctx, fillR, fillG, fillB, fillA);
      CGContextFillRect(ctx, handleRect);
      CGContextSetRGBStrokeColor(ctx, sR, sG, sB, 1.0);
      CGContextSetLineWidth(ctx, strokeWidth);
      CGContextStrokeRect(ctx, handleRect);
      continue;
    }

    if (path.points.size() < 2)
      continue;

    if (path.filled) {
      CGContextSaveGState(ctx);
      // The backouts are subtracted from the fill: each one clips to everything outside it (the rectangle around the
      // visible area minus the hole), so the PDF shows through.
      for (const auto &hole : path.holes) {
        if (hole.size() < 3)
          continue;
        CGMutablePathRef holePath = CGPathCreateMutable();
        CGPathAddRect(holePath, NULL, CGContextGetClipBoundingBox(ctx));
        addContour(holePath, hole);
        CGContextAddPath(ctx, holePath);
        CGContextEOClip(ctx);
        CGPathRelease(holePath);
      }
      CGMutablePathRef fillPath = CGPathCreateMutable();
      addContour(fillPath, path.points);
      CGContextSetRGBFillColor(ctx, fillR, fillG, fillB, fillA);
      CGContextAddPath(ctx, fillPath);
      // Qt paths fill odd-even
      CGContextEOFillPath(ctx);
      CGPathRelease(fillPath);
      CGContextRestoreGState(ctx);
    }

    CGMutablePathRef strokePath = CGPathCreateMutable();
    addContour(strokePath, path.points);
    for (const auto &hole : path.holes) {
      if (hole.size() >= 3)
        addContour(strokePath, hole);
    }
    CGContextSetRGBStrokeColor(ctx, sR, sG, sB, 1.0);
    CGContextSetLineWidth(ctx, strokeWidth);
    if (path.dashed) {
      CGFloat dashLengths[] = {6.0 * invZoom, 4.0 * invZoom};
      CGContextSetLineDash(ctx, 0, dashLengths, 2);
    }
    CGContextAddPath(ctx, strokePath);
    CGContextStrokePath(ctx);
    if (path.dashed) {
      CGContextSetLineDash(ctx, 0, NULL, 0);
    }
    CGPathRelease(strokePath);
  }

  CGFloat rectSize = 12.0 * invZoom;
  CGFloat minusW = 6.0 * invZoom;
  CGFloat minusH = 1.5 * invZoom;
  CGFloat borderW = 1.0 * invZoom;

  for (const auto &neg : _negativeIndicators) {
    CGRect bgRect = CGRectMake(neg.center.x - rectSize / 2,
                               neg.center.y - rectSize / 2, rectSize, rectSize);
    CGContextSetRGBFillColor(ctx, 1.0, 0.0, 0.0, 1.0);
    CGContextFillRect(ctx, bgRect);
    CGContextSetRGBStrokeColor(ctx, 0.0, 0.0, 0.0, 1.0);
    CGContextSetLineWidth(ctx, borderW);
    CGContextStrokeRect(ctx, bgRect);

    CGRect minusRect = CGRectMake(neg.center.x - minusW / 2,
                                  neg.center.y - minusH / 2, minusW, minusH);
    CGContextSetRGBFillColor(ctx, 1.0, 1.0, 1.0, 1.0);
    CGContextFillRect(ctx, minusRect);
  }
}

@end
