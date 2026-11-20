#import "UndoRedoController.h"
#include "../../../Core/Domain/NegativeChange.hpp"
#include "../../../Core/Domain/TakeoffDeletion.hpp"
#include "../../../Core/Domain/TakeoffSelection.hpp"
#include "../../../Core/Domain/UndoHistory.hpp"

enum class UndoAction { Move, MoveGroup, Place, Delete, SetNegative };

struct UndoEntry {
  UndoAction action;
  std::string takeoff_uid;
  std::vector<double> old_position;
  std::vector<double> new_position;
  std::vector<ost_drag::PositionChange> moves; // MoveGroup: every takeoff moved
  ost_domain::Takeoff placed_takeoff;
  // Delete: every takeoff the delete removed, selected one first, a parent
  // before its children, each with the extras of its stored row (UND-06)
  std::vector<ost_domain::Takeoff> deleted_takeoffs;
  std::vector<NegativeChange> negatives; // SetNegative: the takeoff changed
};

// The (uid, position) pairs a grouped move applies: the old positions on undo,
// the new ones on redo.
static std::vector<std::pair<std::string, std::vector<double>>>
groupPositions(const std::vector<ost_drag::PositionChange> &moves,
               bool useOld) {
  std::vector<std::pair<std::string, std::vector<double>>> positions;
  positions.reserve(moves.size());
  for (const auto &move : moves)
    positions.push_back(
        {move.uid, useOld ? move.old_position : move.new_position});
  return positions;
}

static std::vector<std::string>
uidsOf(const std::vector<ost_domain::Takeoff> &takeoffs) {
  std::vector<std::string> uids;
  uids.reserve(takeoffs.size());
  for (const auto &takeoff : takeoffs)
    uids.push_back(takeoff.uid);
  return uids;
}

// Undoes one entry. YES only when the change was applied and stored; then the
// entry moves to the redo stack, otherwise it stays on the undo stack and the
// next undo tries it again (UND-02).
static BOOL applyUndo(id<UndoRedoDelegate> delegate, UndoEntry &entry) {
  switch (entry.action) {
  case UndoAction::Place: {
    const std::vector<std::string> placed = {entry.takeoff_uid};
    if (![delegate undoRedoRemoveTakeoffs:placed])
      return NO;
    [delegate undoRedoClearSelection];
    return YES;
  }
  case UndoAction::Delete:
    if (![delegate undoRedoAddTakeoffs:entry.deleted_takeoffs])
      return NO;
    // Windows _undo_delete selects the restored takeoffs (UND-10)
    [delegate undoRedoSelectTakeoff:entry.takeoff_uid];
    return YES;
  case UndoAction::MoveGroup:
    return [delegate undoRedoApplyPositions:groupPositions(entry.moves, true)];
  case UndoAction::SetNegative:
    return [delegate
        undoRedoApplyNegatives:ost_domain::negative_values(entry.negatives,
                                                           true)];
  case UndoAction::Move:
    if (![delegate undoRedoApplyPosition:entry.old_position
                               toTakeoff:entry.takeoff_uid])
      return NO;
    [delegate undoRedoSelectTakeoff:entry.takeoff_uid];
    return YES;
  }
  return NO;
}

// Redoes one entry; the same rule as applyUndo for the redo stack.
static BOOL applyRedo(id<UndoRedoDelegate> delegate, UndoEntry &entry) {
  switch (entry.action) {
  case UndoAction::Place: {
    std::vector<ost_domain::Takeoff> placed = {entry.placed_takeoff};
    if (![delegate undoRedoAddTakeoffs:placed])
      return NO;
    [delegate undoRedoSelectTakeoff:entry.takeoff_uid];
    return YES;
  }
  case UndoAction::Delete:
    if (![delegate undoRedoRemoveTakeoffs:uidsOf(entry.deleted_takeoffs)])
      return NO;
    // Windows _redo_delete clears the selection
    [delegate undoRedoClearSelection];
    return YES;
  case UndoAction::MoveGroup:
    return [delegate undoRedoApplyPositions:groupPositions(entry.moves, false)];
  case UndoAction::SetNegative:
    return [delegate
        undoRedoApplyNegatives:ost_domain::negative_values(entry.negatives,
                                                           false)];
  case UndoAction::Move:
    if (![delegate undoRedoApplyPosition:entry.new_position
                               toTakeoff:entry.takeoff_uid])
      return NO;
    [delegate undoRedoSelectTakeoff:entry.takeoff_uid];
    return YES;
  }
  return NO;
}

@implementation UndoRedoController {
  // The newest 50 entries (Windows UndoRedoService max_size, UND-01)
  ost_domain::UndoHistory<UndoEntry> _history;
}

- (BOOL)canUndo {
  return _history.can_undo();
}
- (BOOL)canRedo {
  return _history.can_redo();
}

- (void)pushMoveForTakeoff:(const std::string &)uid
               oldPosition:(const std::vector<double> &)oldPos
               newPosition:(const std::vector<double> &)newPos {
  UndoEntry entry;
  entry.action = UndoAction::Move;
  entry.takeoff_uid = uid;
  entry.old_position = oldPos;
  entry.new_position = newPos;
  _history.push(std::move(entry));
}

- (void)pushMoveForTakeoffs:(const std::vector<ost_drag::PositionChange> &)moves {
  if (moves.empty())
    return;
  UndoEntry entry;
  entry.action = UndoAction::MoveGroup;
  entry.takeoff_uid = moves.front().uid;
  entry.moves = moves;
  _history.push(std::move(entry));
}

- (void)pushPlaceForTakeoff:(const ost_domain::Takeoff &)takeoff {
  UndoEntry entry;
  entry.action = UndoAction::Place;
  entry.takeoff_uid = takeoff.uid;
  entry.placed_takeoff = takeoff;
  _history.push(std::move(entry));
}

- (void)pushNegativeChanges:(const std::vector<NegativeChange> &)changes {
  if (changes.empty())
    return;
  UndoEntry entry;
  entry.action = UndoAction::SetNegative;
  entry.takeoff_uid = changes.front().uid;
  entry.negatives = changes;
  _history.push(std::move(entry));
}

- (void)performUndo {
  id<UndoRedoDelegate> delegate = _delegate;
  if (!delegate)
    return;
  const bool advanced = _history.undo(
      [delegate](UndoEntry &entry) { return applyUndo(delegate, entry) == YES; });
  if (advanced)
    [delegate undoRedoDidModify];
}

- (void)performRedo {
  id<UndoRedoDelegate> delegate = _delegate;
  if (!delegate)
    return;
  const bool advanced = _history.redo(
      [delegate](UndoEntry &entry) { return applyRedo(delegate, entry) == YES; });
  if (advanced)
    [delegate undoRedoDidModify];
}

- (BOOL)deleteSelection:(const std::string &)selectedUID
           fromTakeoffs:(const std::vector<ost_domain::Takeoff> &)takeoffs {
  id<UndoRedoDelegate> delegate = _delegate;
  if (selectedUID.empty() || !delegate)
    return NO;

  // The selected takeoff and every descendant at any depth, parents first
  // (Windows expand_takeoff_uids_with_descendants, UND-08), and nothing else:
  // the Core decision of the Delete button (TakeoffSelection.hpp).
  std::vector<ost_domain::Takeoff> deletedTakeoffs =
      ost_domain::takeoffs_to_delete(takeoffs, selectedUID);
  if (deletedTakeoffs.empty())
    return NO;

  // The whole stored row of each takeoff, taken before the delete, so that the
  // undo restores it with all its columns (UND-06).
  for (auto &takeoff : deletedTakeoffs)
    [delegate undoRedoSnapshotTakeoff:takeoff];

  // A failed delete adds no history (Windows: a failed forward persistence
  // pushes no entry): nothing was removed and the selection stays.
  if (![delegate undoRedoRemoveTakeoffs:uidsOf(deletedTakeoffs)])
    return NO;

  UndoEntry entry;
  entry.action = UndoAction::Delete;
  entry.takeoff_uid = selectedUID;
  entry.deleted_takeoffs = std::move(deletedTakeoffs);
  _history.push(std::move(entry));

  [delegate undoRedoClearSelection];
  [delegate undoRedoDidModify];
  return YES;
}

- (void)clear {
  _history.clear();
}

@end
