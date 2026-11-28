#import "UndoRedoController.h"
#include "../../../Core/Domain/NegativeChange.hpp"
#include "../../../Core/Domain/TakeoffDeletion.hpp"
#include "../../../Core/Domain/TakeoffSelection.hpp"
#include "../../../Core/Domain/PageScaleCapture.hpp"
#include "../../../Core/Domain/UndoHistory.hpp"
#include <unordered_map>

enum class UndoAction { Move, MoveGroup, Place, Delete, SetNegative };

// The page and the page scale an entry captured for one takeoff when it was
// created (plan 4.7, 5.4, N-M1-06): the positions of the entry are in the units
// of this scale.
struct CapturedPageScale {
  std::string page_uid;
  ost_domain::PageScale scale;
};

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
  // Set Scale: by takeoff uid, the page and the scale pair the positions of
  // this entry are in. A takeoff without an item has nothing captured (the
  // delegate does not answer, or the page was unknown): factor 1 at replay.
  std::unordered_map<std::string, CapturedPageScale> page_scales;
};

// Captures the page and the page scale of one takeoff into the entry, asking the
// delegate while the takeoff still exists. A delegate that does not implement
// the method, or an answer of "unknown", captures nothing for that takeoff.
static void capturePageScale(id<UndoRedoDelegate> delegate, UndoEntry &entry,
                             const std::string &takeoffUID) {
  SEL capture = @selector(undoRedoPageScaleForTakeoff:pageUID:);
  if (!delegate || ![delegate respondsToSelector:capture])
    return;
  std::string pageUID;
  const std::optional<ost_domain::PageScale> scale =
      [delegate undoRedoPageScaleForTakeoff:takeoffUID pageUID:pageUID];
  if (!scale || pageUID.empty())
    return;
  entry.page_scales[takeoffUID] = CapturedPageScale{pageUID, *scale};
}

// The positions of one takeoff of an entry at the page scale that is current
// now: the Core rule replay_entry_positions decides. NO = the undo / redo is
// refused (the captured page is gone, or the scales cannot be used): the caller
// answers NO and the history does not advance (UND-02).
static BOOL positionAtCurrentScale(id<UndoRedoDelegate> delegate,
                                   const UndoEntry &entry,
                                   const std::string &takeoffUID,
                                   const std::vector<double> &stored,
                                   std::vector<double> &out) {
  std::optional<ost_domain::PageScale> captured;
  std::optional<ost_domain::PageScale> current;
  const auto item = entry.page_scales.find(takeoffUID);
  if (item != entry.page_scales.end()) {
    captured = item->second.scale;
    // a delegate that cannot say what the page is now leaves `current` empty:
    // the Core rule refuses, nothing is written with a made-up scale
    if ([delegate respondsToSelector:@selector(undoRedoPageScaleForPage:)])
      current = [delegate undoRedoPageScaleForPage:item->second.page_uid];
  }
  const std::optional<std::vector<double>> replayed =
      ost_domain::replay_entry_positions(stored, captured, current);
  if (!replayed)
    return NO;
  out = *replayed;
  return YES;
}

// The (uid, position) pairs a grouped move applies: the old positions on undo,
// the new ones on redo, each at the page scale that is current now. NO = refused
// (nothing is applied).
static BOOL groupPositions(
    id<UndoRedoDelegate> delegate, const UndoEntry &entry, bool useOld,
    std::vector<std::pair<std::string, std::vector<double>>> &positions) {
  positions.clear();
  positions.reserve(entry.moves.size());
  for (const auto &move : entry.moves) {
    std::vector<double> atCurrentScale;
    if (!positionAtCurrentScale(delegate, entry, move.uid,
                                useOld ? move.old_position : move.new_position,
                                atCurrentScale))
      return NO;
    positions.push_back({move.uid, std::move(atCurrentScale)});
  }
  return YES;
}

// The takeoffs of a Delete entry (or the placed one of a Place entry) with their
// positions at the page scale that is current now. The entry keeps the positions
// it captured. NO = refused.
static BOOL takeoffsAtCurrentScale(id<UndoRedoDelegate> delegate,
                                   const UndoEntry &entry,
                                   const std::vector<ost_domain::Takeoff> &stored,
                                   std::vector<ost_domain::Takeoff> &out) {
  out = stored;
  for (auto &takeoff : out) {
    std::vector<double> atCurrentScale;
    if (!positionAtCurrentScale(delegate, entry, takeoff.uid, takeoff.position,
                                atCurrentScale))
      return NO;
    takeoff.position = std::move(atCurrentScale);
  }
  return YES;
}

static std::vector<std::string>
uidsOf(const std::vector<ost_domain::Takeoff> &takeoffs) {
  std::vector<std::string> uids;
  uids.reserve(takeoffs.size());
  for (const auto &takeoff : takeoffs)
    uids.push_back(takeoff.uid);
  return uids;
}

// The references an entry replays, in the words of the Core rule
// (ost_domain::history_replay_targets): which takeoffs it changes in place and
// which pages it re-creates takeoffs on, for the direction of the call.
static ost_domain::HistoryReplayTargets
replayTargets(const UndoEntry &entry, ost_domain::HistoryDirection direction) {
  ost_domain::HistoryEntryKind kind = ost_domain::HistoryEntryKind::Move;
  std::vector<std::string> takeoffUIDs;
  std::vector<std::string> pageUIDs;
  switch (entry.action) {
  case UndoAction::Move:
    kind = ost_domain::HistoryEntryKind::Move;
    takeoffUIDs.push_back(entry.takeoff_uid);
    break;
  case UndoAction::MoveGroup:
    kind = ost_domain::HistoryEntryKind::MoveGroup;
    for (const auto &move : entry.moves)
      takeoffUIDs.push_back(move.uid);
    break;
  case UndoAction::SetNegative:
    kind = ost_domain::HistoryEntryKind::SetNegative;
    for (const auto &change : entry.negatives)
      takeoffUIDs.push_back(change.uid);
    break;
  case UndoAction::Place:
    kind = ost_domain::HistoryEntryKind::Place;
    takeoffUIDs.push_back(entry.placed_takeoff.uid);
    pageUIDs.push_back(entry.placed_takeoff.page_uid);
    break;
  case UndoAction::Delete:
    kind = ost_domain::HistoryEntryKind::Delete;
    for (const auto &takeoff : entry.deleted_takeoffs) {
      takeoffUIDs.push_back(takeoff.uid);
      pageUIDs.push_back(takeoff.page_uid);
    }
    break;
  }
  return ost_domain::history_replay_targets(kind, direction, takeoffUIDs,
                                            pageUIDs);
}

// YES = the page or a takeoff of the entry is gone (Windows
// HistoryTargetUnavailableError): the undo / redo is refused before anything is
// written. A delegate that cannot answer treats the entry as available.
static BOOL targetsUnavailable(id<UndoRedoDelegate> delegate,
                               const UndoEntry &entry,
                               ost_domain::HistoryDirection direction) {
  if (![delegate respondsToSelector:@selector(undoRedoReplayTargetsUnavailable:)])
    return NO;
  return [delegate
      undoRedoReplayTargetsUnavailable:replayTargets(entry, direction)];
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
  case UndoAction::Delete: {
    // the takeoffs come back at the page scale that is current now, not at
    // the one they were deleted under (Set Scale in between, plan 4.7)
    std::vector<ost_domain::Takeoff> restored;
    if (!takeoffsAtCurrentScale(delegate, entry, entry.deleted_takeoffs,
                                restored))
      return NO;
    if (![delegate undoRedoAddTakeoffs:restored])
      return NO;
    // Windows _undo_delete selects the restored takeoffs (UND-10)
    [delegate undoRedoSelectTakeoff:entry.takeoff_uid];
    return YES;
  }
  case UndoAction::MoveGroup: {
    std::vector<std::pair<std::string, std::vector<double>>> positions;
    if (!groupPositions(delegate, entry, true, positions))
      return NO;
    return [delegate undoRedoApplyPositions:positions];
  }
  case UndoAction::SetNegative:
    return [delegate
        undoRedoApplyNegatives:ost_domain::negative_values(entry.negatives,
                                                           true)];
  case UndoAction::Move: {
    std::vector<double> position;
    if (!positionAtCurrentScale(delegate, entry, entry.takeoff_uid,
                                entry.old_position, position))
      return NO;
    if (![delegate undoRedoApplyPosition:position
                               toTakeoff:entry.takeoff_uid])
      return NO;
    [delegate undoRedoSelectTakeoff:entry.takeoff_uid];
    return YES;
  }
  }
  return NO;
}

// Redoes one entry; the same rule as applyUndo for the redo stack.
static BOOL applyRedo(id<UndoRedoDelegate> delegate, UndoEntry &entry) {
  switch (entry.action) {
  case UndoAction::Place: {
    // the placed takeoff comes back at the page scale that is current now
    std::vector<ost_domain::Takeoff> placed;
    if (!takeoffsAtCurrentScale(delegate, entry, {entry.placed_takeoff},
                                placed))
      return NO;
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
  case UndoAction::MoveGroup: {
    std::vector<std::pair<std::string, std::vector<double>>> positions;
    if (!groupPositions(delegate, entry, false, positions))
      return NO;
    return [delegate undoRedoApplyPositions:positions];
  }
  case UndoAction::SetNegative:
    return [delegate
        undoRedoApplyNegatives:ost_domain::negative_values(entry.negatives,
                                                           false)];
  case UndoAction::Move: {
    std::vector<double> position;
    if (!positionAtCurrentScale(delegate, entry, entry.takeoff_uid,
                                entry.new_position, position))
      return NO;
    if (![delegate undoRedoApplyPosition:position
                               toTakeoff:entry.takeoff_uid])
      return NO;
    [delegate undoRedoSelectTakeoff:entry.takeoff_uid];
    return YES;
  }
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
  capturePageScale(_delegate, entry, uid);
  _history.push(std::move(entry));
}

- (void)pushMoveForTakeoffs:(const std::vector<ost_drag::PositionChange> &)moves {
  if (moves.empty())
    return;
  UndoEntry entry;
  entry.action = UndoAction::MoveGroup;
  entry.takeoff_uid = moves.front().uid;
  entry.moves = moves;
  id<UndoRedoDelegate> delegate = _delegate;
  for (const auto &move : moves)
    capturePageScale(delegate, entry, move.uid);
  _history.push(std::move(entry));
}

- (void)pushPlaceForTakeoff:(const ost_domain::Takeoff &)takeoff {
  UndoEntry entry;
  entry.action = UndoAction::Place;
  entry.takeoff_uid = takeoff.uid;
  entry.placed_takeoff = takeoff;
  capturePageScale(_delegate, entry, takeoff.uid);
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
  (void)[self performUndoUserInitiated:YES];
}

- (void)performRedo {
  (void)[self performRedoUserInitiated:YES];
}

- (BOOL)performUndoUserInitiated:(BOOL)userInitiated {
  return [self stepInDirection:ost_domain::HistoryDirection::Undo
                 userInitiated:userInitiated];
}

- (BOOL)performRedoUserInitiated:(BOOL)userInitiated {
  return [self stepInDirection:ost_domain::HistoryDirection::Redo
                 userInitiated:userInitiated];
}

// One undo or redo (Windows undo() / redo(user_initiated)). The decision, the
// four texts and "the entry stays" are Core (ost_domain::run_history_step):
// a stale entry (its page or takeoff is gone) is refused before anything is
// written, a failed write leaves the stacks as they are, an empty history is
// no refusal. A refusal of a USER call is shown; a programmatic call only logs.
- (BOOL)stepInDirection:(ost_domain::HistoryDirection)direction
          userInitiated:(BOOL)userInitiated {
  id<UndoRedoDelegate> delegate = _delegate;
  if (!delegate)
    return NO;
  const bool undoing = direction == ost_domain::HistoryDirection::Undo;
  const ost_domain::HistoryStepResult result = ost_domain::run_history_step(
      _history, direction, userInitiated == YES,
      [delegate, direction](const UndoEntry &entry) {
        return targetsUnavailable(delegate, entry, direction) == YES;
      },
      [delegate, undoing](UndoEntry &entry) {
        return (undoing ? applyUndo(delegate, entry)
                        : applyRedo(delegate, entry)) == YES;
      });
  if (result.advanced) {
    [delegate undoRedoDidModify];
    return YES;
  }
  if (result.refusal) {
    NSLog(@"%s refused (%s); the entry stays in the history", undoing ? "Undo" : "Redo",
          *result.refusal == ost_domain::HistoryRefusal::TargetUnavailable
              ? "the page or item no longer exists"
              : "the change could not be applied or stored");
    SEL present = @selector(undoRedoPresentRefusalWithTitle:message:);
    if (result.alert && [delegate respondsToSelector:present]) {
      NSString *title = [NSString stringWithUTF8String:result.alert->title.c_str()];
      NSString *message =
          [NSString stringWithUTF8String:result.alert->message.c_str()];
      [delegate undoRedoPresentRefusalWithTitle:title message:message];
    }
  }
  return NO;
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

  // The page scale of each takeoff, captured before the delete like the
  // snapshot: afterwards the takeoffs are gone from the project and the
  // delegate could no longer find their page (plan 4.7, 5.4).
  UndoEntry entry;
  for (const auto &takeoff : deletedTakeoffs)
    capturePageScale(delegate, entry, takeoff.uid);

  // A failed delete adds no history (Windows: a failed forward persistence
  // pushes no entry): nothing was removed and the selection stays.
  if (![delegate undoRedoRemoveTakeoffs:uidsOf(deletedTakeoffs)])
    return NO;

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
