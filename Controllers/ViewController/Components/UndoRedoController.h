#ifndef UNDOREDOCONTROLLER_H
#define UNDOREDOCONTROLLER_H

#import <Foundation/Foundation.h>
#include "../../../Core/Domain/Takeoff.hpp"
#include <string>
#include <vector>

@class UndoRedoController;

@protocol UndoRedoDelegate <NSObject>

- (void)undoRedoApplyPosition:(const std::vector<double> &)position
                    toTakeoff:(const std::string &)uid;
- (void)undoRedoRemoveTakeoff:(const std::string &)uid;
- (void)undoRedoAddTakeoff:(ost_domain::Takeoff &)takeoff;
- (void)undoRedoSelectTakeoff:(const std::string &)uid;
- (void)undoRedoClearSelection;
- (void)undoRedoDidModify;

@end

@interface UndoRedoController : NSObject

@property(nonatomic, weak) id<UndoRedoDelegate> delegate;
@property(nonatomic, readonly) BOOL canUndo;
@property(nonatomic, readonly) BOOL canRedo;

- (void)pushMoveForTakeoff:(const std::string &)uid
               oldPosition:(const std::vector<double> &)oldPos
               newPosition:(const std::vector<double> &)newPos;

- (void)pushPlaceForTakeoff:(const ost_domain::Takeoff &)takeoff;

- (void)performUndo;
- (void)performRedo;

- (void)deleteSelection:(const std::string &)selectedUID
           fromTakeoffs:(const std::vector<ost_domain::Takeoff> &)takeoffs;

- (void)clear;

@end

#endif
