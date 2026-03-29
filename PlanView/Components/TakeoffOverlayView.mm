#import "TakeoffOverlayView.h"

static const CGFloat kOverlayStrokeWidth = 1.5;

std::vector<CGPoint> regularPolygonPoints(CGFloat cx, CGFloat cy, CGFloat rx,
                                          CGFloat ry, int sides) {
  std::vector<CGPoint> pts;
  pts.reserve(sides);
  for (int i = 0; i < sides; i++) {
    CGFloat angle = (CGFloat)i / sides * 2.0 * M_PI - M_PI / 2.0;
    pts.push_back({cx + rx * cosf(angle), cy + ry * sinf(angle)});
  }
  return pts;
}

void addSquareHandle(CGPoint c, CGFloat half, bool active,
                     std::vector<OverlayPath> &paths) {
  OverlayPath handle;
  handle.isEllipse = false;
  handle.filled = true;
  if (active) {
    handle.r = 1.0f;
    handle.g = 0.6f;
    handle.b = 0.0f;
    handle.a = 1.0f;
  } else {
    handle.r = 30.0f / 255;
    handle.g = 120.0f / 255;
    handle.b = 1.0f;
    handle.a = 1.0f;
  }
  handle.strokeR = 1.0f;
  handle.strokeG = 1.0f;
  handle.strokeB = 1.0f;
  handle.lineWidth = 1.0;
  handle.points = {
      {c.x - half, c.y - half},
      {c.x + half, c.y - half},
      {c.x + half, c.y + half},
      {c.x - half, c.y + half},
  };
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
  if (!_negativeIndicators.empty())
    [self setNeedsDisplay];
}

- (void)drawRect:(CGRect)rect {
  CGContextRef ctx = UIGraphicsGetCurrentContext();
  if (!ctx)
    return;

  for (const auto &path : _paths) {
    CGFloat fillR = path.r, fillG = path.g, fillB = path.b;
    CGFloat fillA = path.a;
    CGFloat sR = path.strokeR >= 0 ? path.strokeR : fillR;
    CGFloat sG = path.strokeG >= 0 ? path.strokeG : fillG;
    CGFloat sB = path.strokeB >= 0 ? path.strokeB : fillB;

    if (path.isEllipse && path.points.size() >= 2) {
      CGRect ellipseRect = CGRectMake(path.points[0].x, path.points[0].y,
                                      path.points[1].x - path.points[0].x,
                                      path.points[1].y - path.points[0].y);

      if (path.filled) {
        CGContextSetRGBFillColor(ctx, fillR, fillG, fillB, fillA);
        CGContextFillEllipseInRect(ctx, ellipseRect);
      }
      CGContextSetRGBStrokeColor(ctx, sR, sG, sB, 1.0);
      CGContextSetLineWidth(ctx, path.lineWidth > 0 ? path.lineWidth
                                                    : kOverlayStrokeWidth);
      CGContextStrokeEllipseInRect(ctx, ellipseRect);
      continue;
    }

    if (path.points.size() < 2)
      continue;

    CGMutablePathRef cgPath = CGPathCreateMutable();
    CGPathMoveToPoint(cgPath, NULL, path.points[0].x, path.points[0].y);
    for (size_t i = 1; i < path.points.size(); i++) {
      CGPathAddLineToPoint(cgPath, NULL, path.points[i].x, path.points[i].y);
    }
    CGPathCloseSubpath(cgPath);

    if (path.filled) {
      CGContextSetRGBFillColor(ctx, fillR, fillG, fillB, fillA);
      CGContextAddPath(ctx, cgPath);
      CGContextFillPath(ctx);
    }

    CGContextSetRGBStrokeColor(ctx, sR, sG, sB, 1.0);
    CGContextSetLineWidth(ctx, path.lineWidth > 0 ? path.lineWidth
                                                  : kOverlayStrokeWidth);
    if (path.dashed) {
      CGFloat dashLengths[] = {6.0, 4.0};
      CGContextSetLineDash(ctx, 0, dashLengths, 2);
    }
    CGContextAddPath(ctx, cgPath);
    CGContextStrokePath(ctx);
    if (path.dashed) {
      CGContextSetLineDash(ctx, 0, NULL, 0);
    }

    CGPathRelease(cgPath);
  }

  CGFloat invZoom = (_zoomScale > 0) ? (1.0 / _zoomScale) : 1.0;
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
