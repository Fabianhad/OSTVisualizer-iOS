#ifndef DRAGCONTROLLER_H
#define DRAGCONTROLLER_H

#import <UIKit/UIKit.h>
#include <string>
#include <vector>

@class DragController;

@protocol DragDelegate <NSObject>

- (void)dragDidUpdatePosition;
- (void)dragDidEndForTakeoff:(const std::string &)uid
                 oldPosition:(const std::vector<double> &)oldPos
                 newPosition:(const std::vector<double> &)newPos;

@end

@interface DragController : NSObject

@property(nonatomic, weak) id<DragDelegate> delegate;

/// Handles one drag frame. Mutates position in place.
/// Captures drag start on first call for a new takeoff UID.
- (void)handleDrag:(const std::string &)takeoffUID
          position:(std::vector<double> &)position
       handleIndex:(int)handleIndex
       canvasDelta:(CGPoint)delta
      scaleFactor1:(double)sf1
      scaleFactor2:(double)sf2
      pageRotation:(int)rotation
     snapIncrement:(double)snap
     angleSnapping:(BOOL)angleSnap;

/// Called when drag gesture ends. Fires delegate with old/new positions.
- (void)endDrag:(const std::string &)takeoffUID
       position:(const std::vector<double> &)position;

@end

#endif
