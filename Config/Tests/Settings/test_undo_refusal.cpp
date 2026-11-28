// A user-triggered undo / redo that does not run (parity pass 3, owner decision "PORT"; tests first): the alert texts of the four cases, the stale rule (the page or
// the item of an entry no longer exists), and the guarantee that a refused entry STAYS in the history with the stacks unchanged. The decision lives in
// Core/Domain/UndoRedoRules; the Objective-C part (UndoRedoController builds the references of an entry, ViewController shows the alert) is device-only.
// Windows source of truth (HEAD d1c430a): undo_redo_service.py:449-582 (undo / redo(user_initiated), _history_failure_message), texts.py:1752-1767 (titles and the four
// messages), plan_view_action_handler.py:1593-1597 and :1716-1735 (HistoryTargetUnavailableError for a page, a takeoff or an annotation that is gone).
// Windows tests ported: tests/presentation/services/test_undo_redo_service.py (a user undo / redo that cannot be submitted shows an error with the generic text; an
// automatic undo only logs and returns False; a submitted undo reports True), tests/integration/history/test_cross_system_workflows.py
// (test_a_delete_undo_whose_page_is_gone_is_refused_and_kept, test_a_move_undo_whose_takeoff_is_gone_is_refused_and_kept: the write count is unchanged, can_undo stays true,
// the stale text is shown once).
#include "TestHarness.hpp"

#include "Data/ProjectData.hpp"
#include "Domain/UndoRedoRules.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ost_domain;
using std::string;
using std::vector;

namespace
{
    const string kGenericUndo = "The change could not be undone. It is still in the undo history.";
    const string kGenericRedo = "The change could not be redone. It is still in the redo history.";
    const string kStaleUndo = "The change could not be undone because the page or item it belongs to no longer exists. It is still in the undo history.";
    const string kStaleRedo = "The change could not be redone because the page or item it belongs to no longer exists. It is still in the redo history.";

    void add_page(ost_data::ProjectData &project, const string &uid)
    {
        Page page;
        page.uid = uid;
        page.name = "Page " + uid;
        project.pages[uid] = page;
        project.page_order.push_back(uid);
    }

    void add_takeoff(ost_data::ProjectData &project, const string &uid, const string &page_uid)
    {
        Takeoff takeoff;
        takeoff.uid = uid;
        takeoff.page_uid = page_uid;
        takeoff.condition_uid = "1";
        project.takeoffs.push_back(takeoff);
    }

    ost_data::ProjectData project_with_page_and_takeoff()
    {
        ost_data::ProjectData project;
        add_page(project, "p1");
        add_takeoff(project, "t1", "p1");
        return project;
    }

    // an entry of a test history: its name, the kind and the references the controller would build
    struct Entry
    {
        string name;
        HistoryEntryKind kind;
        vector<string> takeoff_uids;
        vector<string> stored_page_uids;
    };
}

// ---- the four texts and the user / programmatic split (texts.py:1752-1767, undo_redo_service.py:549-562) ----

TEST(UndoRefusal, TheFourTextsAndTheTitlesAreTheWindowsOnes)
{
    const struct
    {
        HistoryDirection direction;
        HistoryRefusal refusal;
        string title;
        string message;
    } rows[] = {
        {HistoryDirection::Undo, HistoryRefusal::Failed, "Undo", kGenericUndo},
        {HistoryDirection::Redo, HistoryRefusal::Failed, "Redo", kGenericRedo},
        {HistoryDirection::Undo, HistoryRefusal::TargetUnavailable, "Undo", kStaleUndo},
        {HistoryDirection::Redo, HistoryRefusal::TargetUnavailable, "Redo", kStaleRedo},
    };
    for (const auto &row : rows)
    {
        SCOPED_CONTEXT(row.title + " / " + row.message);
        const std::optional<DialogText> dialog = history_refusal_dialog(row.direction, row.refusal, true);
        REQUIRE(dialog.has_value());
        CHECK_STR_EQ(dialog->title, row.title);
        CHECK_STR_EQ(dialog->message, row.message);
    }
}

TEST(UndoRefusal, AProgrammaticCallShowsNothing)
{
    for (const HistoryDirection direction : {HistoryDirection::Undo, HistoryDirection::Redo})
        for (const HistoryRefusal refusal : {HistoryRefusal::Failed, HistoryRefusal::TargetUnavailable})
            CHECK_FALSE(history_refusal_dialog(direction, refusal, false).has_value());
}

// ---- which references an entry replays (plan_view_action_handler.py:1593, :1798, :1847, :1865) ----

TEST(UndoRefusal, TheTargetsOfEachEntryKindAndDirection)
{
    const vector<string> takeoffs = {"t1", "t2"};
    const vector<string> pages = {"p1", "p2"};
    // Move, MoveGroup, SetNegative: the takeoffs, both directions, no page of their own
    for (const HistoryEntryKind kind : {HistoryEntryKind::Move, HistoryEntryKind::MoveGroup, HistoryEntryKind::SetNegative})
        for (const HistoryDirection direction : {HistoryDirection::Undo, HistoryDirection::Redo})
        {
            const HistoryReplayTargets targets = history_replay_targets(kind, direction, takeoffs, pages);
            CHECK_EQ(targets.takeoff_uids, takeoffs);
            CHECK_EQ(targets.page_uids, vector<string>());
        }
    // Delete: the takeoffs come back on undo (their pages must exist), redo only removes them
    const HistoryReplayTargets delete_undo = history_replay_targets(HistoryEntryKind::Delete, HistoryDirection::Undo, takeoffs, pages);
    CHECK_EQ(delete_undo.takeoff_uids, vector<string>());
    CHECK_EQ(delete_undo.page_uids, pages);
    const HistoryReplayTargets delete_redo = history_replay_targets(HistoryEntryKind::Delete, HistoryDirection::Redo, takeoffs, pages);
    CHECK_EQ(delete_redo.takeoff_uids, vector<string>());
    CHECK_EQ(delete_redo.page_uids, vector<string>());
    // Place: the takeoff comes back on redo, undo only removes it
    const HistoryReplayTargets place_redo = history_replay_targets(HistoryEntryKind::Place, HistoryDirection::Redo, takeoffs, pages);
    CHECK_EQ(place_redo.takeoff_uids, vector<string>());
    CHECK_EQ(place_redo.page_uids, pages);
    const HistoryReplayTargets place_undo = history_replay_targets(HistoryEntryKind::Place, HistoryDirection::Undo, takeoffs, pages);
    CHECK_EQ(place_undo.takeoff_uids, vector<string>());
    CHECK_EQ(place_undo.page_uids, vector<string>());
}

// ---- the stale rule (_replay_page_scale: the page is gone; _replay_takeoff_scale: the takeoff or its page is gone) ----

TEST(UndoRefusal, ATakeoffAndItsPageThatExistAreNotStale)
{
    const ost_data::ProjectData project = project_with_page_and_takeoff();
    CHECK_FALSE(history_targets_unavailable(project, {{"t1"}, {}}));
    CHECK_FALSE(history_targets_unavailable(project, {{}, {"p1"}}));
    CHECK_FALSE(history_targets_unavailable(project, {{"t1"}, {"p1"}}));
}

TEST(UndoRefusal, NothingToCheckIsNotStale)
{
    CHECK_FALSE(history_targets_unavailable(ost_data::ProjectData(), {}));
}

TEST(UndoRefusal, ATakeoffThatIsGoneIsStale_CrossSystemMoveUndo)
{
    ost_data::ProjectData project = project_with_page_and_takeoff();
    project.takeoffs.clear();
    CHECK(history_targets_unavailable(project, {{"t1"}, {}}));
}

TEST(UndoRefusal, ATakeoffOnAPageThatIsGoneIsStale)
{
    ost_data::ProjectData project = project_with_page_and_takeoff();
    project.pages.clear();
    project.page_order.clear();
    CHECK(history_targets_unavailable(project, {{"t1"}, {}}));
}

TEST(UndoRefusal, APageThatIsGoneIsStale_CrossSystemDeleteUndo)
{
    ost_data::ProjectData project = project_with_page_and_takeoff();
    project.pages.erase("p1");
    CHECK(history_targets_unavailable(project, {{}, {"p1"}}));
    // one gone page among existing ones is enough
    add_page(project, "p2");
    CHECK(history_targets_unavailable(project, {{}, {"p2", "p1"}}));
    CHECK_FALSE(history_targets_unavailable(project, {{}, {"p2"}}));
}

TEST(UndoRefusal, AnEmptyUidDoesNotExist)
{
    const ost_data::ProjectData project = project_with_page_and_takeoff();
    CHECK(history_targets_unavailable(project, {{""}, {}}));
    CHECK(history_targets_unavailable(project, {{}, {""}}));
}

// ---- the step: refused entries stay, the stacks do not change, a normal undo / redo still works ----

namespace
{
    using History = UndoHistory<Entry>;

    // the stale check of the controller: the references of the entry against the project, for the direction of the call
    auto stale_in(const ost_data::ProjectData &project, HistoryDirection direction)
    {
        return [&project, direction](const Entry &entry)
        {
            return history_targets_unavailable(project, history_replay_targets(entry.kind, direction, entry.takeoff_uids, entry.stored_page_uids));
        };
    }

    auto applying(vector<string> &calls, bool result)
    {
        return [&calls, result](Entry &entry)
        {
            calls.push_back(entry.name);
            return result;
        };
    }
}

// test_a_delete_undo_whose_page_is_gone_is_refused_and_kept: nothing is written, the entry is still the newest undo entry, the redo stack is empty, the stale text
// is shown; a second tap shows it again.
TEST(UndoRefusal, AStaleDeleteUndoIsRefusedAndTheEntryStays)
{
    ost_data::ProjectData project = project_with_page_and_takeoff();
    History history;
    history.push({"older", HistoryEntryKind::Move, {"t1"}, {"p1"}});
    history.push({"delete", HistoryEntryKind::Delete, {"t1"}, {"p1"}});
    project.pages.erase("p1");
    project.takeoffs.clear();

    vector<string> writes;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_CONTEXT("attempt " + std::to_string(attempt));
        const HistoryStepResult result = run_history_step(history, HistoryDirection::Undo, true, stale_in(project, HistoryDirection::Undo), applying(writes, true));
        CHECK_FALSE(result.advanced);
        REQUIRE(result.refusal.has_value());
        CHECK(*result.refusal == HistoryRefusal::TargetUnavailable);
        REQUIRE(result.alert.has_value());
        CHECK_STR_EQ(result.alert->title, "Undo");
        CHECK_STR_EQ(result.alert->message, kStaleUndo);
        CHECK_EQ(writes, vector<string>()); // nothing was written
        CHECK_EQ(history.undo_count(), static_cast<size_t>(2));
        CHECK_EQ(history.redo_count(), static_cast<size_t>(0));
    }
}

// test_a_move_undo_whose_takeoff_is_gone_is_refused_and_kept
TEST(UndoRefusal, AStaleMoveUndoIsRefusedAndTheEntryStays)
{
    ost_data::ProjectData project = project_with_page_and_takeoff();
    History history;
    history.push({"move", HistoryEntryKind::Move, {"t1"}, {}});
    project.takeoffs.clear();
    vector<string> writes;
    const HistoryStepResult result = run_history_step(history, HistoryDirection::Undo, true, stale_in(project, HistoryDirection::Undo), applying(writes, true));
    CHECK_FALSE(result.advanced);
    REQUIRE(result.alert.has_value());
    CHECK_STR_EQ(result.alert->message, kStaleUndo);
    CHECK_EQ(writes, vector<string>());
    CHECK(history.can_undo());
}

// the redo version: an entry that was undone, whose page is gone, is refused on redo with the redo sentence and stays on the REDO stack
TEST(UndoRefusal, AStaleRedoIsRefusedAndTheEntryStaysOnTheRedoStack)
{
    ost_data::ProjectData project = project_with_page_and_takeoff();
    History history;
    history.push({"place", HistoryEntryKind::Place, {"t1"}, {"p1"}});
    vector<string> writes;
    // the undo of a Place only removes the takeoff: nothing to check, it runs while the page is still there
    CHECK(run_history_step(history, HistoryDirection::Undo, true, stale_in(project, HistoryDirection::Undo), applying(writes, true)).advanced);
    CHECK_EQ(history.redo_count(), static_cast<size_t>(1));
    project.pages.erase("p1");

    const HistoryStepResult result = run_history_step(history, HistoryDirection::Redo, true, stale_in(project, HistoryDirection::Redo), applying(writes, true));
    CHECK_FALSE(result.advanced);
    REQUIRE(result.alert.has_value());
    CHECK_STR_EQ(result.alert->title, "Redo");
    CHECK_STR_EQ(result.alert->message, kStaleRedo);
    CHECK_EQ(writes, vector<string>({"place"})); // only the first undo wrote
    CHECK_EQ(history.undo_count(), static_cast<size_t>(0));
    CHECK_EQ(history.redo_count(), static_cast<size_t>(1));
}

// the undo of a Place and the redo of a Delete only remove takeoffs: they run although the page is gone (Windows has no replay position there)
TEST(UndoRefusal, ARemovalReplayIsNotStale)
{
    ost_data::ProjectData project = project_with_page_and_takeoff();
    project.pages.clear();
    History history;
    history.push({"place", HistoryEntryKind::Place, {"t1"}, {"p1"}});
    vector<string> writes;
    const HistoryStepResult undone = run_history_step(history, HistoryDirection::Undo, true, stale_in(project, HistoryDirection::Undo), applying(writes, true));
    CHECK(undone.advanced);
    CHECK_FALSE(undone.refusal.has_value());
    CHECK_FALSE(undone.alert.has_value());
}

// test_a_user_undo_or_redo_that_cannot_be_submitted_shows_an_error: a failing user undo gives the generic text, the entry stays; a failing redo the redo sentence
TEST(UndoRefusal, AFailingUserUndoAndRedoGiveTheGenericTextAndTheEntryStays)
{
    const ost_data::ProjectData project = project_with_page_and_takeoff();
    History history;
    history.push({"move", HistoryEntryKind::Move, {"t1"}, {}});
    vector<string> writes;

    const HistoryStepResult undo = run_history_step(history, HistoryDirection::Undo, true, stale_in(project, HistoryDirection::Undo), applying(writes, false));
    CHECK_FALSE(undo.advanced);
    REQUIRE(undo.refusal.has_value());
    CHECK(*undo.refusal == HistoryRefusal::Failed);
    REQUIRE(undo.alert.has_value());
    CHECK_STR_EQ(undo.alert->title, "Undo");
    CHECK_STR_EQ(undo.alert->message, kGenericUndo);
    CHECK_EQ(history.undo_count(), static_cast<size_t>(1));
    CHECK_EQ(history.redo_count(), static_cast<size_t>(0));
    CHECK_EQ(writes, vector<string>({"move"})); // the write was tried and failed

    // undo once for real, then a failing redo
    CHECK(run_history_step(history, HistoryDirection::Undo, true, stale_in(project, HistoryDirection::Undo), applying(writes, true)).advanced);
    const HistoryStepResult redo = run_history_step(history, HistoryDirection::Redo, true, stale_in(project, HistoryDirection::Redo), applying(writes, false));
    CHECK_FALSE(redo.advanced);
    REQUIRE(redo.alert.has_value());
    CHECK_STR_EQ(redo.alert->title, "Redo");
    CHECK_STR_EQ(redo.alert->message, kGenericRedo);
    CHECK_EQ(history.undo_count(), static_cast<size_t>(0));
    CHECK_EQ(history.redo_count(), static_cast<size_t>(1));
}

// an action that throws is a failure too (UndoHistory catches it): the generic text, the entry stays
TEST(UndoRefusal, AnActionThatThrowsIsAGenericFailure)
{
    const ost_data::ProjectData project = project_with_page_and_takeoff();
    History history;
    history.push({"move", HistoryEntryKind::Move, {"t1"}, {}});
    const HistoryStepResult result = run_history_step(history, HistoryDirection::Undo, true, stale_in(project, HistoryDirection::Undo),
                                                      [](Entry &) -> bool { throw std::runtime_error("write failed"); });
    CHECK_FALSE(result.advanced);
    REQUIRE(result.alert.has_value());
    CHECK_STR_EQ(result.alert->message, kGenericUndo);
    CHECK(history.can_undo());
}

// test_an_automatic_undo_that_cannot_be_submitted_only_logs_and_returns_false: no alert for a programmatic call, the refusal is still reported (the caller logs)
TEST(UndoRefusal, AProgrammaticRefusalOnlyReportsAndShowsNothing)
{
    ost_data::ProjectData project = project_with_page_and_takeoff();
    History history;
    history.push({"move", HistoryEntryKind::Move, {"t1"}, {}});
    vector<string> writes;
    const HistoryStepResult failing = run_history_step(history, HistoryDirection::Undo, false, stale_in(project, HistoryDirection::Undo), applying(writes, false));
    CHECK_FALSE(failing.advanced);
    CHECK(failing.refusal.has_value());
    CHECK_FALSE(failing.alert.has_value());
    CHECK(history.can_undo());

    project.takeoffs.clear();
    const HistoryStepResult stale = run_history_step(history, HistoryDirection::Undo, false, stale_in(project, HistoryDirection::Undo), applying(writes, true));
    CHECK_FALSE(stale.advanced);
    REQUIRE(stale.refusal.has_value());
    CHECK(*stale.refusal == HistoryRefusal::TargetUnavailable);
    CHECK_FALSE(stale.alert.has_value());
    CHECK_EQ(writes, vector<string>({"move"})); // the stale call wrote nothing
}

// test_a_submitted_undo_reports_true: a normal undo and redo advance, move the entry between the stacks and show nothing
TEST(UndoRefusal, ANormalUndoAndRedoStillWork)
{
    const ost_data::ProjectData project = project_with_page_and_takeoff();
    History history;
    history.push({"move", HistoryEntryKind::Move, {"t1"}, {}});
    history.push({"delete", HistoryEntryKind::Delete, {"t1"}, {"p1"}});
    vector<string> writes;

    const HistoryStepResult undo = run_history_step(history, HistoryDirection::Undo, true, stale_in(project, HistoryDirection::Undo), applying(writes, true));
    CHECK(undo.advanced);
    CHECK_FALSE(undo.refusal.has_value());
    CHECK_FALSE(undo.alert.has_value());
    CHECK_EQ(history.undo_count(), static_cast<size_t>(1));
    CHECK_EQ(history.redo_count(), static_cast<size_t>(1));

    const HistoryStepResult redo = run_history_step(history, HistoryDirection::Redo, true, stale_in(project, HistoryDirection::Redo), applying(writes, true));
    CHECK(redo.advanced);
    CHECK_FALSE(redo.alert.has_value());
    CHECK_EQ(history.undo_count(), static_cast<size_t>(2));
    CHECK_EQ(history.redo_count(), static_cast<size_t>(0));
    CHECK_EQ(writes, vector<string>({"delete", "delete"}));
}

// an empty history is no failure: no refusal, no alert, nothing called (the buttons are disabled; the call is a no-op)
TEST(UndoRefusal, AnEmptyHistoryIsNotAFailure)
{
    const ost_data::ProjectData project = project_with_page_and_takeoff();
    History history;
    vector<string> writes;
    for (const HistoryDirection direction : {HistoryDirection::Undo, HistoryDirection::Redo})
    {
        const HistoryStepResult result = run_history_step(history, direction, true, stale_in(project, direction), applying(writes, true));
        CHECK_FALSE(result.advanced);
        CHECK_FALSE(result.refusal.has_value());
        CHECK_FALSE(result.alert.has_value());
    }
    CHECK_EQ(writes, vector<string>());
}

// the stale check comes first: a stale entry whose action would have failed is still reported as stale (not as a generic failure)
TEST(UndoRefusal, TheStaleCheckRunsBeforeTheWrite)
{
    ost_data::ProjectData project = project_with_page_and_takeoff();
    project.takeoffs.clear();
    History history;
    history.push({"negative", HistoryEntryKind::SetNegative, {"t1"}, {}});
    vector<string> writes;
    const HistoryStepResult result = run_history_step(history, HistoryDirection::Undo, true, stale_in(project, HistoryDirection::Undo), applying(writes, false));
    REQUIRE(result.refusal.has_value());
    CHECK(*result.refusal == HistoryRefusal::TargetUnavailable);
    CHECK_EQ(writes, vector<string>());
}
