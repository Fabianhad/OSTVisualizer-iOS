// Data safety of the takeoff Delete (device report: "deleting a selected takeoff ... or deletes the condition").
//
// The Delete button of the app runs ost_domain::delete_enabled (ViewController _handleDeleteTakeoff), then ost_domain::
// takeoffs_to_delete (UndoRedoController deleteSelection:fromTakeoffs:), then ost_store::delete_takeoffs (ImportService
// deleteTakeoffs:), and the undo runs ost_store::insert_takeoffs with the complete rows. These tests run exactly those functions
// against a real database file and prove, for the takeoffs of the report, that no condition row, no takeoff of another condition that
// is not a descendant, and nothing of another bid is ever written, and that the undo restores every column of the deleted rows
// (all but GUID, which a restore regenerates) with the values they had.
//
// Windows: plan_view_action_handler.on_elements_deleted (3747-3779) deletes the selected takeoffs and their parent_uid descendants
// (_delete_takeoffs_fast 1830); the condition uids are only refresh metadata (4023-4025); conditions are deleted only by the
// confirmed condition command (condition_action_handler.on_delete_requested -> confirm_delete_conditions -> delete_conditions,
// 1209-1271; the Summary tab of Windows has a Delete key path to the same command, a tab iOS does not have).
//
// The code paths that can delete a CONDITION in the iOS app (none is reachable from the takeoff Delete button or the mode circle),
// listed for the audit of 2026-11-19:
//   1. Conditions sheet -> long press on a row -> action sheet "Delete" -> confirmation alert -> ConditionsViewController
//      didRequestDeleteConditionUID: -> ViewController+Conditions.mm -> ImportService deleteCondition:inBid: -> ost_store::
//      delete_condition (all-or-nothing, COND-DEL-01).
//   2. Bid level (not the plan view): DatabaseViewController bid / folder deletes -> ost_store::delete_project and
//      permanently_delete_project, which remove the conditions of the bid with the bid.
#include "SettingsTestSupport.hpp"

#include "Domain/EditMode.hpp"
#include "Domain/Takeoff.hpp"
#include "Domain/TakeoffDeletion.hpp"
#include "Domain/TakeoffSelection.hpp"

#include <algorithm>
#include <string>
#include <vector>

using ost_domain::EditMode;
using ost_domain::Takeoff;
using ost_stest::TempDb;

namespace
{
    using Uids = std::vector<std::string>;

    // Bid 10, page 100: condition 30 has the takeoffs 700, 701, 702 (a root and two children, 700 -> 701 -> 702) and 704 (a root);
    // condition 31 has the single takeoff 703. Bid 11 (condition 32) owns takeoff 800.
    void seed(const TempDb &db)
    {
        db.seed_world();
        const char *rows[] = {
            "(700, 10, 30, 100, 0,   '1.0;2.0;3.0;4.0\n')",
            "(701, 10, 30, 100, 700, '5.0;6.0;7.0;8.0\n')",
            "(702, 10, 30, 100, 701, '9.0;10.0;11.0;12.0\n')",
            "(703, 10, 31, 100, 0,   '0.0;0.0;1.0;1.0\n')",
            "(704, 10, 30, 100, 0,   '2.0;2.0;3.0;3.0\n')",
            "(800, 11, 32, 101, 0,   '4.0;4.0;5.0;5.0\n')",
        };
        for (const char *row : rows)
            db.must(std::string("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position) "
                                "VALUES ") + row);
        db.must("UPDATE BidTakeoffs SET Rotation = 0.0, Curve = -1, IsNegativeQuantity = 0, No = UID - 690, "
                "Quantity = UID / 10.0, GUID = '{G-' || UID || '}', FontName = 'Arial', GridOffsetX = 1.5");
        // a value in every other column too, so that a column the restore forgets shows
        db.must("UPDATE BidTakeoffs SET Count = UID % 7, GridOffsetY = -2.25, GridRotation = 0.125, FontColor = 255, "
                "FontSize = -12, FontBold = 1, FontItalic = 0, FontUnderline = 1, TypGroupTakeoffUID = 8, TypPageTakeoffUID = 9, "
                "TakeoffModified = 1, TypGroupUID = 4, TypGroupMarkerUID = 3, FlipX = 1, FlipY = 0, NameFontName = 'Verdana', "
                "NameFontColor = 65280, NameFontSize = 10, NameFontBold = 0, NameFontItalic = 1, NameFontUnderline = 0, "
                "BidZoneUID = 5, BidTypAreaUID = 6, BidAreaUID = 60");
    }

    // Every BidConditions row of every bid, in one string.
    std::string conditions(const TempDb &db)
    {
        return db.text("SELECT group_concat(quote(UID)||'|'||quote(BidUID)||'|'||quote(RefNo)||'|'||quote(Name)||'|'||quote(Type), "
                       "';') FROM (SELECT * FROM BidConditions ORDER BY UID)");
    }

    std::string takeoff_uids(const TempDb &db, const std::string &where = "1=1")
    {
        return db.text("SELECT ifnull(group_concat(UID, ','), '') FROM (SELECT UID FROM BidTakeoffs WHERE " + where +
                       " ORDER BY UID)");
    }

    // EVERY column of the table but GUID (a restore regenerates it), values SQL-quoted so a type change shows; the column list is
    // read from the table, so a column added later is compared too.
    std::string whole_row(const TempDb &db, long long uid)
    {
        const std::string columns =
            db.text("SELECT group_concat('quote([' || name || '])', '||''|''||') FROM pragma_table_info('BidTakeoffs') "
                    "WHERE name <> 'GUID'");
        return db.text("SELECT " + columns + " FROM BidTakeoffs WHERE UID = " + std::to_string(uid));
    }

    // What a press on Delete does to the database for the tapped takeoff `uid`, in the given mode, with the functions the app calls:
    // delete_enabled, takeoffs_to_delete, then the store delete of that set. Returns the deleted uids (empty when the button is off).
    Uids press_delete(const TempDb &db, const std::string &uid, EditMode mode)
    {
        const ost_data::ProjectData project = ost_store::load_project(db.path(), "10");
        ost_domain::SelectionState state;
        state.mode = mode;
        state = ost_domain::select_takeoff(project.takeoffs, state, uid);
        Uids uids;
        if (!ost_domain::delete_enabled(project.takeoffs, state))
            return uids;
        for (const Takeoff &takeoff : ost_domain::takeoffs_to_delete(project.takeoffs, state.takeoff_uid))
            uids.push_back(takeoff.uid);
        REQUIRE(ost_store::delete_takeoffs(db.path(), uids));
        return uids;
    }
}

// The takeoffs of the report: a takeoff of a condition that has more takeoffs, and the only takeoff of a condition. In every mode,
// the delete removes exactly the Core set; every condition row, the other takeoffs of the condition and the other bid are unchanged.
TEST(TakeoffDeleteSafety, DeletingATakeoffNeverDeletesAConditionOrAnotherTakeoff)
{
    for (EditMode mode : {EditMode::View, EditMode::Select, EditMode::PlaceTakeoff})
    {
        SCOPED_CONTEXT("mode " + std::to_string(static_cast<int>(mode)));
        TempDb db("safety_one");
        REQUIRE(db.schema_ok());
        seed(db);
        const std::string conditions_before = conditions(db);
        const std::string other_bid_before = whole_row(db, 800);
        REQUIRE_EQ(takeoff_uids(db), std::string("700,701,702,703,704,800"));

        // 704: a root of condition 30, which keeps 700, 701, 702
        CHECK_EQ(press_delete(db, "704", mode), (Uids{"704"}));
        CHECK_EQ(takeoff_uids(db), std::string("700,701,702,703,800"));
        CHECK_STR_EQ(conditions(db), conditions_before);

        // 703: the ONLY takeoff of condition 31: the takeoff goes, the condition stays
        CHECK_EQ(press_delete(db, "703", mode), (Uids{"703"}));
        CHECK_EQ(takeoff_uids(db), std::string("700,701,702,800"));
        CHECK_STR_EQ(conditions(db), conditions_before);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 31"), 1);

        // 702: the last child: only that row; its parent and the siblings of the condition stay
        CHECK_EQ(press_delete(db, "702", mode), (Uids{"702"}));
        CHECK_EQ(takeoff_uids(db), std::string("700,701,800"));
        CHECK_STR_EQ(conditions(db), conditions_before);

        // the other bid is never touched
        CHECK_STR_EQ(whole_row(db, 800), other_bid_before);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 32"), 1);
    }
}

// Deleting a parent removes its descendants (UND-08, Windows parity) and only them: the condition rows and the unrelated takeoffs of
// the same condition are unchanged; the undo (the original uids, parents first, the whole rows) restores every row byte for byte.
TEST(TakeoffDeleteSafety, UndoRestoresExactlyWhatWasDeleted)
{
    TempDb db("safety_undo");
    REQUIRE(db.schema_ok());
    seed(db);
    const std::string conditions_before = conditions(db);
    const long long all[] = {700, 701, 702, 703, 704, 800};
    std::vector<std::string> rows_before;
    for (long long uid : all)
        rows_before.push_back(whole_row(db, uid));

    const ost_data::ProjectData project = ost_store::load_project(db.path(), "10");
    ost_domain::SelectionState state;
    state.mode = EditMode::Select;
    state = ost_domain::select_takeoff(project.takeoffs, state, "700");
    REQUIRE(ost_domain::delete_enabled(project.takeoffs, state));

    // the history entry (UndoRedoController deleteSelection): the complete takeoffs of the Core decision with their stored rows,
    // taken before the delete
    std::vector<Takeoff> deleted = ost_domain::takeoffs_to_delete(project.takeoffs, state.takeoff_uid);
    REQUIRE_EQ(deleted.size(), static_cast<size_t>(3));
    Uids uids;
    for (const Takeoff &takeoff : deleted)
        uids.push_back(takeoff.uid);
    REQUIRE_EQ(uids, (Uids{"700", "701", "702"}));
    for (Takeoff &takeoff : deleted)
        REQUIRE(ost_store::read_takeoff_extras(db.path(), "10", takeoff.uid, takeoff.extras));

    REQUIRE(ost_store::delete_takeoffs(db.path(), uids));
    CHECK_EQ(takeoff_uids(db), std::string("703,704,800")); // the unrelated takeoffs of condition 30 and 31 stay
    CHECK_STR_EQ(conditions(db), conditions_before);

    std::vector<std::string> new_uids;
    REQUIRE(ost_store::insert_takeoffs(db.path(), "10", deleted, new_uids));
    CHECK_EQ(new_uids, (Uids{"700", "701", "702"})); // iOS restores a row with its own uid
    CHECK_EQ(takeoff_uids(db), std::string("700,701,702,703,704,800"));
    CHECK_STR_EQ(conditions(db), conditions_before);
    for (size_t i = 0; i < std::size(all); ++i)
    {
        SCOPED_CONTEXT("takeoff " + std::to_string(all[i]));
        CHECK_STR_EQ(whole_row(db, all[i]), rows_before[i]);
    }

    // after the undo the restored takeoff is selected, and Delete would act on exactly that family again
    const ost_data::ProjectData restored = ost_store::load_project(db.path(), "10");
    ost_domain::SelectionState after = ost_domain::clear_selection(state);
    after = ost_domain::select_after_history(restored.takeoffs, after, "700", "100");
    CHECK_EQ(after.takeoff_uid, std::string("700"));
    CHECK_EQ(ost_domain::takeoff_uids_to_delete(restored.takeoffs, after), (Uids{"700", "701", "702"}));
}
