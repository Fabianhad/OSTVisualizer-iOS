#import "UndoRedoController.h"

enum class UndoAction { Move, Place, Delete };

struct UndoEntry {
  UndoAction action;
  std::string takeoff_uid;
  std::vector<double> old_position;
  std::vector<double> new_position;
  ost_domain::Takeoff placed_takeoff;
  std::vector<ost_domain::Takeoff> deleted_takeoffs;
};

@implementation UndoRedoController {
  std::vector<UndoEntry> _undoStack;
  std::vector<UndoEntry> _redoStack;
}

- (BOOL)canUndo {
  return !_undoStack.empty();
}
- (BOOL)canRedo {
  return !_redoStack.empty();
}

- (void)pushMoveForTakeoff:(const std::string &)uid
               oldPosition:(const std::vector<double> &)oldPos
               newPosition:(const std::vector<double> &)newPos {
  UndoEntry entry;
  entry.action = UndoAction::Move;
  entry.takeoff_uid = uid;
  entry.old_position = oldPos;
  entry.new_position = newPos;
  _undoStack.push_back(std::move(entry));
  _redoStack.clear();
}

- (void)pushPlaceForTakeoff:(const ost_domain::Takeoff &)takeoff {
  UndoEntry entry;
  entry.action = UndoAction::Place;
  entry.takeoff_uid = takeoff.uid;
  entry.placed_takeoff = takeoff;
  _undoStack.push_back(std::move(entry));
  _redoStack.clear();
}

- (void)performUndo {
  if (_undoStack.empty())
    return;
  UndoEntry entry = std::move(_undoStack.back());
  _undoStack.pop_back();

  if (entry.action == UndoAction::Place) {
    [_delegate undoRedoClearSelection];
    [_delegate undoRedoRemoveTakeoff:entry.takeoff_uid];
  } else if (entry.action == UndoAction::Delete) {
    [_delegate undoRedoClearSelection];
    for (auto &takeoff : entry.deleted_takeoffs) {
      [_delegate undoRedoAddTakeoff:takeoff];
    }
  } else {
    [_delegate undoRedoSelectTakeoff:entry.takeoff_uid];
    [_delegate undoRedoApplyPosition:entry.old_position
                           toTakeoff:entry.takeoff_uid];
  }

  _redoStack.push_back(std::move(entry));
  [_delegate undoRedoDidModify];
}

- (void)performRedo {
  if (_redoStack.empty())
    return;
  UndoEntry entry = std::move(_redoStack.back());
  _redoStack.pop_back();

  if (entry.action == UndoAction::Place) {
    [_delegate undoRedoAddTakeoff:entry.placed_takeoff];
    [_delegate undoRedoSelectTakeoff:entry.takeoff_uid];
  } else if (entry.action == UndoAction::Delete) {
    [_delegate undoRedoClearSelection];
    for (const auto &takeoff : entry.deleted_takeoffs) {
      [_delegate undoRedoRemoveTakeoff:takeoff.uid];
    }
  } else {
    [_delegate undoRedoSelectTakeoff:entry.takeoff_uid];
    [_delegate undoRedoApplyPosition:entry.new_position
                           toTakeoff:entry.takeoff_uid];
  }

  _undoStack.push_back(std::move(entry));
  [_delegate undoRedoDidModify];
}

- (void)deleteSelection:(const std::string &)selectedUID
           fromTakeoffs:(const std::vector<ost_domain::Takeoff> &)takeoffs {
  if (selectedUID.empty())
    return;

  std::vector<ost_domain::Takeoff> deletedTakeoffs;
  bool selectedIsRoot = false;
  for (const auto &tk : takeoffs) {
    if (tk.uid != selectedUID)
      continue;
    deletedTakeoffs.push_back(tk);
    selectedIsRoot = tk.parent_uid.empty() || tk.parent_uid == "0";
    break;
  }
  if (deletedTakeoffs.empty())
    return;
  if (selectedIsRoot) {
    for (const auto &tk : takeoffs) {
      if (tk.parent_uid == selectedUID) {
        deletedTakeoffs.push_back(tk);
      }
    }
  }

  UndoEntry entry;
  entry.action = UndoAction::Delete;
  entry.takeoff_uid = selectedUID;
  entry.deleted_takeoffs = deletedTakeoffs;
  _undoStack.push_back(std::move(entry));
  _redoStack.clear();

  for (const auto &tk : deletedTakeoffs) {
    [_delegate undoRedoRemoveTakeoff:tk.uid];
  }
  [_delegate undoRedoClearSelection];
  [_delegate undoRedoDidModify];
}

- (void)clear {
  _undoStack.clear();
  _redoStack.clear();
}

@end
