#ifndef DRAGCONTROLLER_H
#define DRAGCONTROLLER_H

#import <UIKit/UIKit.h>
#include <string>
#include <vector>

@class DragController;

@protocol DragDelegate <NSObject>

/// One drag step produced the candidate position of the dragged takeoff.
/// Return NO to reject it: the position stays as it was before the step (the
/// last valid geometry, as in the Windows plan view). `lastValidPosition` is
/// the position the takeoff has now.
- (BOOL)dragShouldAcceptPosition:(const std::vector<double> &)candidate
                      forTakeoff:(const std::string &)uid
                     handleIndex:(int)handleIndex
               lastValidPosition:(const std::vector<double> &)lastValidPosition;
- (void)dragDidEndForTakeoff:(const std::string &)uid
                 oldPosition:(const std::vector<double> &)oldPos
                 newPosition:(const std::vector<double> &)newPos;

@end

@interface DragController : NSObject

@property(nonatomic, weak) id<DragDelegate> delegate;

/// Handles one drag frame. Mutates position in place (the rules are the
/// Windows ones, Core/Geometry/drag_rules.hpp). Captures drag start on first
/// call for a new takeoff UID. Returns YES when the position was updated, NO
/// when the step was rejected or changed nothing it could apply.
/// `cornerCount` is the number of vertices of an area takeoff and 0 for every
/// other shape. `canvasDelta` is the total displacement since the drag began,
/// in canvas points of the displayed page. Angle snapping off means Windows'
/// Shift held: no angle snap and the free edge drag.
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
     angleSnapping:(BOOL)angleSnap;

/// Called when drag gesture ends. Fires delegate with old/new positions.
- (void)endDrag:(const std::string &)takeoffUID
       position:(const std::vector<double> &)position;

@end

#endif
