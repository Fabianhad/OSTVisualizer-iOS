// UND-09: "Count as Negative Quantity" on a single takeoff changes ONLY the pressed takeoff, in ONE undoable entry; on a pasted
// GROUP (owner decision 3, suite Und09_NegativeGroup below) it applies to the whole group as Windows does, still ONE entry.
//
// Windows (d1c430a): ost_visualizer/presentation/handlers/plan_view_action_handler.py on_set_negative (2134-2181): the flag is set for
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

// ---------------------------------------------------------------------------------------------------------------------------------
// Owner decision 3: "Count as Negative Quantity" on a pasted GROUP (the selection that paste leaves behind, N-M2-04) is the Windows rule:
//   view_context_menu.py:162-196  offered only when the selection has a root takeoff and NO hole / attachment (is_hole = parent_uid not in {"", "0"});
//                                 checked (all_negative) only when EVERY selected takeoff is negative
//   input_handler.py:3247-3251    the press emits (selected uids, not all_negative): every selected takeoff gets the same value
//   plan_view_action_handler.py:2134-2181  old_updates holds EVERY selected takeoff (also those already at the target), ONE history entry
// Windows has no test with two or more uids on on_set_negative; the rule is pinned by the all_negative state tests
// (test_view_context_menu.py:458, :485) and by the code. The single-takeoff behaviour above is unchanged.
// ---------------------------------------------------------------------------------------------------------------------------------
namespace
{
    Takeoff make_child(const std::string &uid, const std::string &parent_uid, bool negative)
    {
        Takeoff t = make_takeoff(uid, "30", "100", negative);
        t.parent_uid = parent_uid;
        return t;
    }

    using Group = std::vector<std::string>;

    // Roots 600 (positive), 601 (negative), 602 (positive); 610 a hole of 600; 611 an attachment of 602 (both positive); 620 and 621 negative roots.
    std::vector<Takeoff> group_world()
    {
        return {make_takeoff("600", "30", "100", false), make_takeoff("601", "30", "100", true),
                make_takeoff("602", "31", "101", false), make_child("610", "600", false),
                make_child("611", "602", false),         make_takeoff("620", "32", "100", true),
                make_takeoff("621", "32", "100", true)};
    }

    std::string describe(const std::vector<NegativeChange> &changes)
    {
        std::string text;
        for (const NegativeChange &c : changes)
            text += c.uid + ":" + (c.old_value ? "T" : "F") + ">" + (c.new_value ? "T" : "F") + " ";
        return text;
    }
}

// (a) a pasted group that is all positive: every takeoff becomes negative, the item is not checked.
TEST(Und09_NegativeGroup, AnAllPositiveGroupBecomesAllNegative)
{
    const ost_domain::NegativeToggle toggle = ost_domain::negative_toggle_for_press(group_world(), Group{"600", "602", "600"}, "602");
    CHECK(toggle.offered);
    CHECK_FALSE(toggle.checked);
    // the duplicate "600" counts once
    CHECK_STR_EQ(describe(toggle.changes), std::string("600:F>T 602:F>T "));
}

// (a) three members, the pressed one in the middle
TEST(Und09_NegativeGroup, AnAllPositiveGroupOfThreeIsOneEntryOfThree)
{
    std::vector<Takeoff> takeoffs = group_world();
    takeoffs.push_back(make_takeoff("603", "30", "100", false));
    takeoffs[1].is_negative = false; // 601 positive too
    const ost_domain::NegativeToggle toggle = ost_domain::negative_toggle_for_press(takeoffs, Group{"600", "601", "603"}, "601");
    CHECK(toggle.offered);
    CHECK_FALSE(toggle.checked);
    CHECK_STR_EQ(describe(toggle.changes), std::string("600:F>T 601:F>T 603:F>T "));
}

// (b) a mixed group: the check mark is off (not every member is negative), every member gets TRUE (also the pressed one, which was negative: it is in
// the entry as true -> true), not "toggle each" and not the value of the pressed takeoff.
TEST(Und09_NegativeGroup, AMixedGroupSetsEveryMemberNegativeAndTheNegativeOnesStayInTheEntry)
{
    const ost_domain::NegativeToggle toggle = ost_domain::negative_toggle_for_press(group_world(), Group{"600", "601", "602"}, "601");
    CHECK(toggle.offered);
    CHECK_FALSE(toggle.checked);
    CHECK_STR_EQ(describe(toggle.changes), std::string("600:F>T 601:T>T 602:F>T "));
    // pressing a positive member of the same group gives the same entry
    const ost_domain::NegativeToggle other = ost_domain::negative_toggle_for_press(group_world(), Group{"600", "601", "602"}, "600");
    CHECK_STR_EQ(describe(other.changes), describe(toggle.changes));
}

// (c) a group that is all negative: the item is checked and every member becomes positive.
TEST(Und09_NegativeGroup, AnAllNegativeGroupBecomesAllPositive)
{
    const ost_domain::NegativeToggle toggle = ost_domain::negative_toggle_for_press(group_world(), Group{"620", "621"}, "620");
    CHECK(toggle.offered);
    CHECK(toggle.checked);
    CHECK_STR_EQ(describe(toggle.changes), std::string("620:T>F 621:T>F "));
}

// (d) a hole or an attachment in the group hides the item and gives no change, whichever member is pressed (Windows show_negative).
TEST(Und09_NegativeGroup, AHoleOrAnAttachmentInTheGroupHidesTheItem)
{
    for (const char *pressed : {"600", "610"})
    {
        SCOPED_CONTEXT(std::string("hole, pressed ") + pressed);
        const ost_domain::NegativeToggle toggle = ost_domain::negative_toggle_for_press(group_world(), Group{"600", "610"}, pressed);
        CHECK_FALSE(toggle.offered);
        CHECK(toggle.changes.empty());
    }
    for (const char *pressed : {"602", "611"})
    {
        SCOPED_CONTEXT(std::string("attachment, pressed ") + pressed);
        const ost_domain::NegativeToggle toggle = ost_domain::negative_toggle_for_press(group_world(), Group{"602", "611", "601"}, pressed);
        CHECK_FALSE(toggle.offered);
        CHECK(toggle.changes.empty());
    }
}

// (e) a pressed takeoff outside the group acts alone (the iOS adaptation, N-M2-04), with today's single rule.
TEST(Und09_NegativeGroup, APressedTakeoffOutsideTheGroupActsAlone)
{
    const ost_domain::NegativeToggle toggle = ost_domain::negative_toggle_for_press(group_world(), Group{"600", "601"}, "602");
    CHECK(toggle.offered);
    CHECK_FALSE(toggle.checked);
    CHECK_STR_EQ(describe(toggle.changes), std::string("602:F>T "));
    const ost_domain::NegativeToggle negative = ost_domain::negative_toggle_for_press(group_world(), Group{"600", "602"}, "601");
    CHECK(negative.checked);
    CHECK_STR_EQ(describe(negative.changes), std::string("601:T>F "));
}

// (e) annotation uids and unknown uids of the group are skipped; the group is what is left.
TEST(Und09_NegativeGroup, AnnotationAndUnknownUidsOfTheGroupAreSkipped)
{
    const ost_domain::NegativeToggle toggle =
        ost_domain::negative_toggle_for_press(group_world(), Group{"600", "ann7", "9999", "602"}, "600");
    CHECK(toggle.offered);
    CHECK_STR_EQ(describe(toggle.changes), std::string("600:F>T 602:F>T "));
    // a group that is a single takeoff plus skipped uids is the single path, which also keeps the item on a lone hole
    const ost_domain::NegativeToggle lone = ost_domain::negative_toggle_for_press(group_world(), Group{"ann7", "610"}, "610");
    CHECK(lone.offered);
    CHECK_STR_EQ(describe(lone.changes), std::string("610:F>T "));
}

// The single path: an empty group, a group of one and an unknown uid behave as negative_changes_for_press (the six tests above).
TEST(Und09_NegativeGroup, TheSinglePathIsTheOldBehaviour)
{
    for (const Group &group : {Group{}, Group{"601"}})
    {
        SCOPED_CONTEXT("group size " + std::to_string(group.size()));
        for (const Takeoff &t : group_world())
        {
            const ost_domain::NegativeToggle toggle = ost_domain::negative_toggle_for_press(group_world(), group, t.uid);
            CHECK(toggle.offered);
            CHECK_EQ(toggle.checked, t.is_negative);
            const std::vector<NegativeChange> old_rule = ost_domain::negative_changes_for_press(group_world(), t.uid, !t.is_negative);
            CHECK_STR_EQ(describe(toggle.changes), describe(old_rule));
        }
    }
    const ost_domain::NegativeToggle unknown = ost_domain::negative_toggle_for_press(group_world(), Group{"600", "601"}, "9999");
    CHECK_FALSE(unknown.offered);
    CHECK_FALSE(unknown.checked);
    CHECK(unknown.changes.empty());
    CHECK_FALSE(ost_domain::negative_toggle_for_press({}, Group{}, "600").offered);
}

// The whole chain with the real pieces: a mixed group (600 F, 601 T, 602 F) is pressed, written in one transaction with each takeoff's own value,
// pushed as ONE history entry; one undo restores F,T,F and one redo gives T,T,T.
TEST(Und09_NegativeGroup, OneUndoRestoresEachFlagOfTheGroupAndOneRedoAppliesThemAgain)
{
    TempDb db("neg_group_chain");
    REQUIRE(db.schema_ok());
    db.seed_world();
    for (const Takeoff &t : group_world())
        if (t.uid == "600" || t.uid == "601" || t.uid == "602")
            db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position, IsNegativeQuantity) VALUES (" + t.uid +
                    ", 10, " + t.condition_uid + ", 100, '1.0;2.0;3.0;4.0\n', " + (t.is_negative ? "1" : "0") + ")");
    REQUIRE_EQ(flags(db), std::string("600:0,601:1,602:0"));

    ost_domain::UndoHistory<std::vector<NegativeChange>> history;
    const ost_domain::NegativeToggle toggle = ost_domain::negative_toggle_for_press(group_world(), Group{"600", "601", "602"}, "601");
    REQUIRE(toggle.offered);
    REQUIRE_EQ(toggle.changes.size(), static_cast<std::size_t>(3));
    REQUIRE(ost_store::set_takeoffs_negative(db.path(), "10", ost_domain::negative_values(toggle.changes, false)));
    history.push(toggle.changes);
    CHECK_EQ(flags(db), std::string("600:1,601:1,602:1"));
    CHECK_EQ(history.undo_count(), static_cast<std::size_t>(1)); // ONE entry for the whole group

    const bool undone = history.undo(
        [&](std::vector<NegativeChange> &entry)
        { return ost_store::set_takeoffs_negative(db.path(), "10", ost_domain::negative_values(entry, true)); });
    CHECK(undone);
    CHECK_EQ(flags(db), std::string("600:0,601:1,602:0")); // each takeoff's own old flag
    CHECK_FALSE(history.can_undo());
    CHECK_EQ(history.redo_count(), static_cast<std::size_t>(1));

    const bool redone = history.redo(
        [&](std::vector<NegativeChange> &entry)
        { return ost_store::set_takeoffs_negative(db.path(), "10", ost_domain::negative_values(entry, false)); });
    CHECK(redone);
    CHECK_EQ(flags(db), std::string("600:1,601:1,602:1"));
    CHECK_EQ(history.undo_count(), static_cast<std::size_t>(1));
}
