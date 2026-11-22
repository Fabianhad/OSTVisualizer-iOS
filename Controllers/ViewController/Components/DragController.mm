#import "DragController.h"
#include "../../../Core/Geometry/coord_transform.hpp"
#include "../../../Core/Geometry/drag_rules.hpp"
#include "../../../Core/Geometry/placement_snap.hpp"
#include <cmath>

@implementation DragController {
  std::vector<double> _dragStartPosition;
  std::string _dragStartUID;
}

- (BOOL)handleDrag:(const std::string &)takeoffUID
          position:(std::vector<double> &)position
       handleIndex:(int)handleIndex
       cornerCount:(int)cornerCount
       canvasDelta:(CGPoint)delta
      scaleFactor1:(double)sf1
      scaleFactor2:(double)sf2
      pageRotation:(int)rotation
             flipX:(BOOL)flipX
             flipY:(BOOL)flipY
     snapIncrement:(double)snap
     angleSnapping:(BOOL)angleSnap {

  if (_dragStartUID != takeoffUID) {
    _dragStartUID = takeoffUID;
    _dragStartPosition = position;
  }

  if (_dragStartPosition.size() != position.size())
    return NO;

  auto ostDelta = ost_coord::canvas_delta_to_ost(delta.x, delta.y, sf1, sf2,
                                                 rotation, flipX, flipY);
  if (!std::isfinite(ostDelta[0]) || !std::isfinite(ostDelta[1]))
    return NO;

  // The Windows rules (drag_handler.compute_new_position) from the position the
  // drag started at. Angle snapping off is Windows' Shift held: no angle snap
  // and the free edge drag.
  std::vector<double> candidate = ost_drag::compute_new_position(
      _dragStartPosition, ostDelta[0], ostDelta[1], handleIndex, cornerCount,
      snap, angleSnap ? ost_placement::kDefaultSnapAngleDegrees : 0.0, false,
      !angleSnap);

  // A rejected step keeps the last valid geometry.
  if (_delegate && ![_delegate dragShouldAcceptPosition:candidate
                                             forTakeoff:takeoffUID
                                            handleIndex:handleIndex
                                      lastValidPosition:position])
    return NO;

  position = std::move(candidate);
  return YES;
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
