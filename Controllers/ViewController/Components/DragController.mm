#import "DragController.h"
#include "../../../Core/Geometry/coord_transform.hpp"
#include "../../../Core/Geometry/geom_utils.hpp"

static double snapOst(double val, double increment) {
  if (increment <= 0.0)
    return val;
  return round(val / increment) * increment;
}

static const double kSnapAngleRad = M_PI / 8.0;

static void snapAngle(double originX, double originY, double &targetX,
                      double &targetY) {
  double dx = targetX - originX;
  double dy = targetY - originY;
  double length = hypot(dx, dy);
  if (length < 1e-9)
    return;
  double angle = atan2(dy, dx);
  double snapped = round(angle / kSnapAngleRad) * kSnapAngleRad;
  targetX = originX + length * cos(snapped);
  targetY = originY + length * sin(snapped);
}

@implementation DragController {
  std::vector<double> _dragStartPosition;
  std::string _dragStartUID;
}

- (void)handleDrag:(const std::string &)takeoffUID
          position:(std::vector<double> &)position
       handleIndex:(int)handleIndex
       canvasDelta:(CGPoint)delta
      scaleFactor1:(double)sf1
      scaleFactor2:(double)sf2
      pageRotation:(int)rotation
     snapIncrement:(double)snap
     angleSnapping:(BOOL)angleSnap {

  if (_dragStartUID != takeoffUID) {
    _dragStartUID = takeoffUID;
    _dragStartPosition = position;
  }

  auto ostDelta =
      ost_coord::canvas_delta_to_ost(delta.x, delta.y, sf1, sf2, rotation);

  if (_dragStartPosition.size() != position.size())
    return;
  int cornerCount = (int)(position.size() / 2);

  if (handleIndex >= 0 && handleIndex >= cornerCount) {
    // Edge drag — move both endpoints of the edge
    double sdx = ostDelta[0], sdy = ostDelta[1];
    if (angleSnap)
      snapAngle(0.0, 0.0, sdx, sdy);
    int edgeI = handleIndex - cornerCount;
    int ci0 = edgeI;
    int ci1 = (edgeI + 1) % cornerCount;
    size_t b0 = (size_t)ci0 * 2, b1 = (size_t)ci1 * 2;
    if (b0 + 1 < position.size()) {
      position[b0] = snapOst(_dragStartPosition[b0] + sdx, snap);
      position[b0 + 1] = snapOst(_dragStartPosition[b0 + 1] + sdy, snap);
    }
    if (b1 + 1 < position.size()) {
      position[b1] = snapOst(_dragStartPosition[b1] + sdx, snap);
      position[b1 + 1] = snapOst(_dragStartPosition[b1 + 1] + sdy, snap);
    }
  } else if (handleIndex >= 0) {
    // Corner drag — move single corner with angle snap and polygon validation
    size_t base = (size_t)handleIndex * 2;
    if (base + 1 < position.size()) {
      double newX = snapOst(_dragStartPosition[base] + ostDelta[0], snap);
      double newY = snapOst(_dragStartPosition[base + 1] + ostDelta[1], snap);

      if (angleSnap) {
        if (cornerCount == 2 && handleIndex < 2) {
          int other = 1 - handleIndex;
          double ox = _dragStartPosition[other * 2];
          double oy = _dragStartPosition[other * 2 + 1];
          snapAngle(ox, oy, newX, newY);
        } else if (cornerCount > 2) {
          int prev = (handleIndex - 1 + cornerCount) % cornerCount;
          double ox = _dragStartPosition[prev * 2];
          double oy = _dragStartPosition[prev * 2 + 1];
          snapAngle(ox, oy, newX, newY);
        }
      }

      if (cornerCount > 2) {
        double oldX = position[base], oldY = position[base + 1];
        double origSign = ost_geom::signed_area(position);
        position[base] = newX;
        position[base + 1] = newY;

        std::vector<ost_types::Vec2> poly;
        for (size_t vi = 0; vi + 1 < position.size(); vi += 2)
          poly.push_back({position[vi], position[vi + 1]});

        double newSign = ost_geom::signed_area(position);
        bool windingFlipped =
            (origSign != 0 && newSign != 0 && (origSign > 0) != (newSign > 0));
        if (!ost_geom::polygon_is_valid(poly) || windingFlipped) {
          position[base] = oldX;
          position[base + 1] = oldY;
        }
      } else {
        position[base] = newX;
        position[base + 1] = newY;
      }
    }
  } else {
    // Whole shape drag
    for (size_t i = 0; i + 1 < position.size(); i += 2) {
      position[i] = snapOst(_dragStartPosition[i] + ostDelta[0], snap);
      position[i + 1] = snapOst(_dragStartPosition[i + 1] + ostDelta[1], snap);
    }
  }

  [_delegate dragDidUpdatePosition];
}

- (void)endDrag:(const std::string &)takeoffUID
       position:(const std::vector<double> &)position {
  if (_dragStartUID.empty() || _dragStartUID != takeoffUID)
    return;

  std::vector<double> oldPos = _dragStartPosition;
  _dragStartUID.clear();
  _dragStartPosition.clear();

  [_delegate dragDidEndForTakeoff:takeoffUID
                      oldPosition:oldPos
                      newPosition:position];
}

@end
