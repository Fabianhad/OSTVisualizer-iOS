#ifndef UNDOREDOCONTROLLER_H
#define UNDOREDOCONTROLLER_H

#import <Foundation/Foundation.h>
#include "../../../Core/Domain/NegativeChange.hpp"
#include "../../../Core/Domain/PageScaleTransform.hpp"
#include "../../../Core/Domain/Takeoff.hpp"
#include "../../../Core/Geometry/drag_rules.hpp"
#include <optional>
#include <string>
#include <utility>
#include <vector>

@class UndoRedoController;

/// One takeoff whose "Count as Negative Quantity" flag changed, with the value
/// before and after (Windows on_set_negative records both). The type and the
/// choice of the takeoff live in Core (UND-09).
using NegativeChange = ost_domain::NegativeChange;

/// The apply methods answer whether the change was applied AND stored. NO means
/// nothing changed: the history does not advance and the next undo / redo tries
/// the same entry again (Windows _submit_history_transition, UND-02).
@protocol UndoRedoDelegate <NSObject>

- (BOOL)undoRedoApplyPosition:(const std::vector<double> &)position
                    toTakeoff:(const std::string &)uid;
/// Applies the positions of one grouped move (all of them or none, persisted in
/// one transaction).
- (BOOL)undoRedoApplyPositions:
    (const std::vector<std::pair<std::string, std::vector<double>>> &)positions;
/// Sets the negative flag of every listed takeoff (all of them or none,
/// persisted in one transaction).
- (BOOL)undoRedoApplyNegatives:
    (const std::vector<std::pair<std::string, bool>> &)negatives;
/// Removes the takeoffs from the project and the database in one transaction.
- (BOOL)undoRedoRemoveTakeoffs:(const std::vector<std::string> &)uids;
/// Inserts the takeoffs (with their uids and extras, a parent before its
/// children) into the project and the database in one transaction.
- (BOOL)undoRedoAddTakeoffs:(std::vector<ost_domain::Takeoff> &)takeoffs;
/// Fills takeoff.extras with the stored row (the columns the typed fields do
/// not hold) before the takeoff is deleted, so that the undo restores the whole
/// row (UND-06).
- (void)undoRedoSnapshotTakeoff:(ost_domain::Takeoff &)takeoff;
- (void)undoRedoSelectTakeoff:(const std::string &)uid;
- (void)undoRedoClearSelection;
- (void)undoRedoDidModify;

@optional

/// The page scale capture of Set Scale (plan 4.7, 5.4, N-M1-06). An entry that
/// holds positions stores them in the units of the page scale it was created
/// under, and a replay rescales them to the scale that is current then (Windows
/// plan_view_action_handler.py:1667-1828). A delegate without these two methods
/// captures nothing: the replay factor is then 1.
///
/// Called when an entry is created, while the takeoff still exists (for a
/// Delete before the takeoffs are removed, like the snapshot). Fills `pageUID`
/// with the page of the takeoff and answers the (ScaleFactor1, ScaleFactor2)
/// pair of that page; std::nullopt = unknown (the takeoff, or its page, is not in
/// the project): nothing is captured for that takeoff.
- (std::optional<ost_domain::PageScale>)
    undoRedoPageScaleForTakeoff:(const std::string &)uid
                        pageUID:(std::string &)pageUID;
/// Called at replay: the CURRENT pair of the page whose uid an entry stored;
/// std::nullopt = the page is gone, that undo / redo is refused (the apply
/// method answers NO and the history does not advance, UND-02).
- (std::optional<ost_domain::PageScale>)undoRedoPageScaleForPage:
    (const std::string &)pageUID;

@end

@interface UndoRedoController : NSObject

@property(nonatomic, weak) id<UndoRedoDelegate> delegate;
@property(nonatomic, readonly) BOOL canUndo;
@property(nonatomic, readonly) BOOL canRedo;

- (void)pushMoveForTakeoff:(const std::string &)uid
               oldPosition:(const std::vector<double> &)oldPos
               newPosition:(const std::vector<double> &)newPos;

/// One history entry for a move of several takeoffs (a group drag, an area with
/// its children): one undo restores every old position, one redo reapplies
/// every new one (Windows on_positions_flushed pushes one entry).
- (void)pushMoveForTakeoffs:(const std::vector<ost_drag::PositionChange> &)moves;

- (void)pushPlaceForTakeoff:(const ost_domain::Takeoff &)takeoff;

/// One history entry for a change of the negative flag of one or several
/// takeoffs (Windows _push_local_property_history).
- (void)pushNegativeChanges:(const std::vector<NegativeChange> &)changes;

- (void)performUndo;
- (void)performRedo;

/// Deletes the takeoff and all its descendants (Windows
/// expand_takeoff_uids_with_descendants) in one step and records it. NO, with
/// nothing deleted and no history entry, when there is nothing to delete or the
/// delegate could not remove the takeoffs.
- (BOOL)deleteSelection:(const std::string &)selectedUID
           fromTakeoffs:(const std::vector<ost_domain::Takeoff> &)takeoffs;

/// Drops the whole history (Windows UndoRedoService.clear).
- (void)clear;

@end

#endif
