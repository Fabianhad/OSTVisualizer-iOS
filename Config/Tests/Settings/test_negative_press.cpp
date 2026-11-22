// UND-09: "Count as Negative Quantity" on a takeoff changes ONLY the pressed takeoff, in ONE undoable entry.
//
// Windows: ost_visualizer/presentation/handlers/plan_view_action_handler.py on_set_negative (2072-2116): the flag is set for
// the takeoffs of the plan-view selection (_takeoff_uids_only(uids)); old_updates holds (uid, old flag) and new_updates
// (uid, new flag) of those takeoffs, and ONE _push_local_property_history entry restores the old values on undo and applies
// the new ones on redo. On iOS the selection of the takeoff context menu is the one pressed takeoff.
// Decision of 2026-11-19: follow Windows. A condition that is merely selected in the sidebar widens nothing (before: every
// takeoff of that condition on the open pages was toggled).
#include "SettingsTestSupport.hpp"

#include "Domain/NegativeChange.hpp"
#include "Domain/Takeoff.hpp"
#include "Domain/UndoHistory.hpp"

#include <string>
#include <utility>
#include <vector>

using ost_domain::NegativeChange;
using ost_domain::Takeoff;
using ost_stest::TempDb;

namespace
{
    Takeoff make_takeoff(const std::string &uid, const std::string &condition_uid, const std::string &page_uid, bool negative)
    {
        Takeoff t;
        t.uid = uid;
        t.condition_uid = condition_uid;
        t.page_uid = page_uid;
        t.position = {1.0, 2.0, 3.0, 4.0};
        t.is_negative = negative;
        return t;
    }

    // Condition 30 has three takeoffs on page 100 (500..502, mixed flags) and one on page 101 (504); condition 31 has one on
    // page 100 (503).
    std::vector<Takeoff> world()
    {
        return {make_takeoff("500", "30", "100", false), make_takeoff("501", "30", "100", false),
                make_takeoff("502", "30", "100", true),  make_takeoff("503", "31", "100", false),
                make_takeoff("504", "30", "101", false)};
    }

    // One press on "Count as Negative Quantity" of the takeoff `uid`. The tests below stand for the situation in which the
    // old rule toggled every takeoff of the condition on the open page (the condition of the pressed takeoff selected in the
    // sidebar, no takeoff selected); the function has no selection input any more.
    std::vector<NegativeChange> press(const std::vector<Takeoff> &takeoffs, const std::string &uid, bool negative)
    {
        return ost_domain::negative_changes_for_press(takeoffs, uid, negative);
    }

    using Flags = std::vector<std::pair<std::string, bool>>;

    std::string flags(const TempDb &db)
    {
        return db.text("SELECT group_concat(UID || ':' || ifnull(IsNegativeQuantity, 'N'), ',') FROM "
                       "(SELECT UID, IsNegativeQuantity FROM BidTakeoffs WHERE BidUID = 10 ORDER BY UID)");
    }

    // The takeoffs of world() as stored rows of bid 10 (the page does not matter to the flag; page 101 belongs to bid 11).
    void seed_takeoffs(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        db.seed_world();
        for (const Takeoff &t : world())
            db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position, IsNegativeQuantity) VALUES (" +
                    t.uid + ", 10, " + t.condition_uid + ", 100, '1.0;2.0;3.0;4.0\n', " + (t.is_negative ? "1" : "0") + ")");
    }
}

// The case of the decision: three takeoffs of one condition on the page, the condition selected, one of them pressed.
TEST(Und09_NegativePress, AConditionSelectedInTheSidebarWidensNothing)
{
    const std::vector<NegativeChange> changes = press(world(), "501", true);
    REQUIRE_EQ(changes.size(), static_cast<std::size_t>(1));
    CHECK_STR_EQ(changes[0].uid, "501");
    CHECK_FALSE(changes[0].old_value);
    CHECK(changes[0].new_value);
}

// The pressed takeoff alone, also when it is not the first of its condition and when other takeoffs of the condition are
// on other pages or already negative.
TEST(Und09_NegativePress, TheOtherTakeoffsOfTheConditionStayOutOfTheEntry)
{
    for (const char *uid : {"500", "501", "502", "504"})
    {
        SCOPED_CONTEXT(std::string("pressed ") + uid);
        const std::vector<NegativeChange> changes = press(world(), uid, true);
        REQUIRE_EQ(changes.size(), static_cast<std::size_t>(1));
        CHECK_STR_EQ(changes[0].uid, uid);
    }
    // a takeoff of another condition is a single change as well
    const std::vector<NegativeChange> other = press(world(), "503", true);
    REQUIRE_EQ(other.size(), static_cast<std::size_t>(1));
    CHECK_STR_EQ(other[0].uid, "503");
}

// The entry records the value the takeoff had (Windows old_updates), so one undo can write it back.
TEST(Und09_NegativePress, TheOldFlagOfThePressedTakeoffIsRecorded)
{
    const std::vector<NegativeChange> on = press(world(), "500", true); // was positive
    REQUIRE_EQ(on.size(), static_cast<std::size_t>(1));
    CHECK_FALSE(on[0].old_value);
    CHECK(on[0].new_value);

    const std::vector<NegativeChange> off = press(world(), "502", false); // was negative
    REQUIRE_EQ(off.size(), static_cast<std::size_t>(1));
    CHECK(off[0].old_value);
    CHECK_FALSE(off[0].new_value);
}

// A uid that is not in the project gives no change (and so no history entry).
TEST(Und09_NegativePress, AnUnknownUidGivesNoChange)
{
    CHECK(press(world(), "9999", true).empty());
    CHECK(press(world(), "", true).empty());
    CHECK(press({}, "500", true).empty());
}

// The pairs an entry writes: the old flag on undo, the new one on redo (the same pairs ProjectStore writes in one transaction).
TEST(Und09_NegativePress, AnEntryWritesTheOldFlagOnUndoAndTheNewFlagOnRedo)
{
    const std::vector<NegativeChange> changes = press(world(), "502", false);
    CHECK_EQ(ost_domain::negative_values(changes, true), (Flags{{"502", true}}));
    CHECK_EQ(ost_domain::negative_values(changes, false), (Flags{{"502", false}}));
    CHECK(ost_domain::negative_values({}, true).empty());
}

// The whole chain with the real pieces: the press, the stored write, the history entry, one undo, one redo. The takeoffs
// 500 and 502 of the condition keep their stored flags the whole time.
TEST(Und09_NegativePress, OneUndoRestoresThePressedTakeoffAndOneRedoAppliesItAgain)
{
    TempDb db("neg_press_chain");
    seed_takeoffs(db);
    REQUIRE_EQ(flags(db), std::string("500:0,501:0,502:1,503:0,504:0"));

    ost_domain::UndoHistory<std::vector<NegativeChange>> history;
    const std::vector<NegativeChange> changes = press(world(), "501", true);
    REQUIRE(ost_store::set_takeoffs_negative(db.path(), "10", ost_domain::negative_values(changes, false)));
    history.push(changes);
    CHECK_EQ(flags(db), std::string("500:0,501:1,502:1,503:0,504:0"));
    CHECK_EQ(history.undo_count(), static_cast<std::size_t>(1));

    const bool undone = history.undo(
        [&](std::vector<NegativeChange> &entry)
        { return ost_store::set_takeoffs_negative(db.path(), "10", ost_domain::negative_values(entry, true)); });
    CHECK(undone);
    CHECK_EQ(flags(db), std::string("500:0,501:0,502:1,503:0,504:0")); // exactly the state before the press
    CHECK_FALSE(history.can_undo());
    CHECK_EQ(history.redo_count(), static_cast<std::size_t>(1));

    const bool redone = history.redo(
        [&](std::vector<NegativeChange> &entry)
        { return ost_store::set_takeoffs_negative(db.path(), "10", ost_domain::negative_values(entry, false)); });
    CHECK(redone);
    CHECK_EQ(flags(db), std::string("500:0,501:1,502:1,503:0,504:0"));
    CHECK_EQ(history.undo_count(), static_cast<std::size_t>(1));
}
