// UND-01 / UND-02: the undo / redo history is bounded (50 entries) and an entry only changes stacks when its undo / redo
// action reports success.
//
// Windows: ost_visualizer/presentation/services/undo_redo_service.py
//   _push_entry (lines 173-195): append, drop the OLDEST entry when len > max_size (default 50), clear the redo stack;
//   _submit_history_transition (469-524): the entry moves to the other stack only when the action COMMITTED; a False result or
//   an exception leaves it on the source stack, where the next undo / redo retries it;
//   clear (518-531): both stacks are emptied.
// Windows tests ported below: tests/presentation/services/test_undo_redo_service.py
//   test_history_is_bounded_and_drops_oldest_entries (403-414), test_failed_synchronous_undo_does_not_advance_history (86-95),
//   test_failed_synchronous_redo_keeps_entry_on_redo_stack (97-103), test_exception_during_redo_leaves_entry_on_redo_stack
//   (105-117), test_exception_during_undo_leaves_entry_on_undo_stack (119-127), test_new_push_discards_redo_stack (395-401),
//   test_committed_history_operation_advances_for_mdb_and_sql (619-638).
#include "TestHarness.hpp"

#include "Domain/UndoHistory.hpp"

#include <stdexcept>
#include <string>
#include <vector>

using ost_domain::UndoHistory;

namespace
{
    using History = UndoHistory<std::string>;
    using Calls = std::vector<std::string>;

    // an action that records the entry it ran for and reports `result`
    auto recording(Calls &calls, bool result)
    {
        return [&calls, result](std::string &entry)
        {
            calls.push_back(entry);
            return result;
        };
    }

    std::string join(const Calls &calls)
    {
        std::string out;
        for (const std::string &call : calls)
            out += (out.empty() ? "" : ",") + call;
        return out;
    }
}

// Port of test_history_is_bounded_and_drops_oldest_entries: capacity 2, push one, two, three, undo three times: the executed
// actions are three and two and nothing is left to undo.
TEST(UndoHistory, TheOldestEntryIsDroppedWhenTheCapacityIsExceeded)
{
    History history(2);
    Calls calls;
    for (const char *name : {"one", "two", "three"})
        history.push(name);
    for (int attempt = 0; attempt < 3; ++attempt)
        history.undo(recording(calls, true));
    CHECK_STR_EQ(join(calls), "three,two");
    CHECK_FALSE(history.can_undo());
}

// Test plan of UND-01: with the default capacity push 51 entries and undo 60 times: exactly 50 execute, the oldest is gone.
TEST(UndoHistory, TheDefaultCapacityIsFifty)
{
    History history;
    Calls calls;
    for (int i = 1; i <= 51; ++i)
        history.push("e" + std::to_string(i));
    CHECK_EQ(history.undo_count(), static_cast<std::size_t>(50));
    for (int attempt = 0; attempt < 60; ++attempt)
        history.undo(recording(calls, true));
    CHECK_EQ(calls.size(), static_cast<std::size_t>(50));
    CHECK_STR_EQ(calls.front(), "e51"); // the newest first
    CHECK_STR_EQ(calls.back(), "e2");   // e1, the oldest, was dropped
    CHECK_FALSE(history.can_undo());
    CHECK_EQ(ost_domain::kUndoHistoryCapacity, static_cast<std::size_t>(50));
}

// Exactly 50 entries are kept without dropping anything (the limit is len > max_size).
TEST(UndoHistory, FiftyEntriesAreAllKept)
{
    History history;
    for (int i = 1; i <= 50; ++i)
        history.push("e" + std::to_string(i));
    CHECK_EQ(history.undo_count(), static_cast<std::size_t>(50));
    Calls calls;
    for (int attempt = 0; attempt < 60; ++attempt)
        history.undo(recording(calls, true));
    CHECK_EQ(calls.size(), static_cast<std::size_t>(50));
    CHECK_STR_EQ(calls.back(), "e1");
}

// Port of test_new_push_discards_redo_stack.
TEST(UndoHistory, ANewPushDiscardsTheRedoStack)
{
    History history;
    Calls calls;
    history.push("a");
    history.undo(recording(calls, true));
    CHECK(history.can_redo());
    history.push("b");
    CHECK_FALSE(history.can_redo());
    CHECK(history.can_undo());
}

// A push that drops the oldest entry still clears the redo stack, and the redo stack holds only what was undone.
TEST(UndoHistory, UndoneEntriesRedoInReverseOrder)
{
    History history;
    Calls calls;
    history.push("a");
    history.push("b");
    history.push("c");
    history.undo(recording(calls, true));
    history.undo(recording(calls, true));
    history.redo(recording(calls, true));
    history.redo(recording(calls, true));
    CHECK_STR_EQ(join(calls), "c,b,b,c");
    CHECK_FALSE(history.can_redo());
    CHECK_EQ(history.undo_count(), static_cast<std::size_t>(3));
}

// Port of test_committed_history_operation_advances_for_mdb_and_sql (the MDB branch): a successful undo moves the entry to the
// redo stack.
TEST(UndoHistory, ASuccessfulUndoAdvancesTheHistory)
{
    History history;
    Calls calls;
    history.push("a");
    CHECK(history.undo(recording(calls, true)));
    CHECK_FALSE(history.can_undo());
    CHECK(history.can_redo());
}

// Port of test_failed_synchronous_undo_does_not_advance_history: a failed undo leaves the entry on the undo stack (canUndo true,
// canRedo false) and the next undo retries the same entry.
TEST(UndoHistory, AFailedUndoDoesNotAdvanceTheHistoryAndIsRetried)
{
    History history;
    Calls calls;
    history.push("a");
    CHECK_FALSE(history.undo(recording(calls, false)));
    CHECK_STR_EQ(join(calls), "a");
    CHECK(history.can_undo());
    CHECK_FALSE(history.can_redo());
    CHECK_FALSE(history.undo(recording(calls, false)));
    CHECK_STR_EQ(join(calls), "a,a");
    CHECK(history.undo(recording(calls, true))); // the retry that finally works
    CHECK_FALSE(history.can_undo());
    CHECK(history.can_redo());
}

// Port of test_failed_synchronous_redo_keeps_entry_on_redo_stack.
TEST(UndoHistory, AFailedRedoKeepsTheEntryOnTheRedoStack)
{
    History history;
    Calls calls;
    history.push("a");
    history.undo(recording(calls, true));
    CHECK_FALSE(history.redo(recording(calls, false)));
    CHECK_STR_EQ(join(calls), "a,a");
    CHECK_FALSE(history.can_undo());
    CHECK(history.can_redo());
}

// Port of test_exception_during_redo_leaves_entry_on_redo_stack and test_exception_during_undo_leaves_entry_on_undo_stack: an
// action that throws counts as a failed one (Windows logs it and keeps the entry where it was).
TEST(UndoHistory, AnActionThatThrowsLeavesTheEntryWhereItWas)
{
    History history;
    Calls calls;
    history.push("a");
    auto throwing = [](std::string &) -> bool { throw std::runtime_error("write failed"); };
    CHECK_FALSE(history.undo(throwing));
    CHECK(history.can_undo());
    CHECK_FALSE(history.can_redo());

    history.undo(recording(calls, true));
    CHECK_FALSE(history.redo(throwing));
    CHECK_FALSE(history.can_undo());
    CHECK(history.can_redo());
}

// A failed undo does not disturb the order: the entry stays the newest one and the entries below it are untouched.
TEST(UndoHistory, AFailedUndoKeepsTheOrderOfTheStack)
{
    History history;
    Calls calls;
    history.push("a");
    history.push("b");
    history.undo(recording(calls, false));
    history.undo(recording(calls, true));
    history.undo(recording(calls, true));
    CHECK_STR_EQ(join(calls), "b,b,a");
    CHECK_FALSE(history.can_undo());
    CHECK_EQ(history.redo_count(), static_cast<std::size_t>(2));
}

// Nothing to undo / redo: the action is not called and the call reports false.
TEST(UndoHistory, UndoAndRedoOnAnEmptyHistoryDoNothing)
{
    History history;
    Calls calls;
    CHECK_FALSE(history.undo(recording(calls, true)));
    CHECK_FALSE(history.redo(recording(calls, true)));
    CHECK(calls.empty());
}

// Windows clear(): both stacks are emptied.
TEST(UndoHistory, ClearEmptiesBothStacks)
{
    History history;
    Calls calls;
    history.push("a");
    history.push("b");
    history.undo(recording(calls, true));
    history.clear();
    CHECK_FALSE(history.can_undo());
    CHECK_FALSE(history.can_redo());
    CHECK_EQ(history.undo_count(), static_cast<std::size_t>(0));
    CHECK_EQ(history.redo_count(), static_cast<std::size_t>(0));
}

// The action sees the entry itself (not a copy): a change it makes (UND-06 snapshots) is kept when the entry moves.
TEST(UndoHistory, TheActionMayChangeTheEntry)
{
    History history;
    history.push("a");
    history.undo([](std::string &entry)
                 {
                     entry += "!";
                     return true;
                 });
    Calls calls;
    history.redo(recording(calls, true));
    CHECK_STR_EQ(join(calls), "a!");
}
