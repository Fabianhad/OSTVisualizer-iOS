// M1 layers (plan 5.3), the STORE: ost_store::insert_layer / rename_layer / delete_layers / swap_layer_sequence / read_layer_uids_in_use,
// ported from the Windows tests (same inputs, same expected rows), plus the lifecycle scenario of the real-Access test as ONE SQLite
// scenario. The data-safety cases are in test_layer_edit_insert.cpp, _rename.cpp, _delete.cpp, _swap.cpp and _usage.cpp.
//
// Windows oracle (49a2d59), tests/infrastructure/mdb/components/test_layer_operations.py (the writers: layer_operations.py :78-114 insert,
// :153-217 delete, :385-410 rename, :432-465 swap), test_identity_allocation.py (identity_allocation.py:5-39), test_settings_reader.py
// (settings_reader.py:450-482). Every Windows test builds a bare sqlite table; the iOS database comes from create_schema, so the ports first
// clear the four template rows of the schema (clear_layers) and seed the Windows rows verbatim (UID, BidUID, Name, Show, Sequence, IsTemplate,
// IsLocked). The Windows rows have no Name column in several tests: a name is added (the tests never read it). Where Windows RAISES
// (RuntimeError "Bids has no row for UID 99", "BidLayers has no row for UID 5", IncoherentBidOwnedScopeError) the store returns the negative
// result of the write pattern (plan 4.4: nothing throws across the Objective-C boundary) and the rows must be unchanged.
//
// NOT ported (plan 5.3 "tests-first plan"): :1122 (the dead lenient branch, LY-5), the duplicate-physical-UID tests :267 / :752 (first half) /
// :1962 (BidLayers.UID is INTEGER PRIMARY KEY on iOS), the legacy-schema and SQL-text tests (:744, :859, :1106, :1159, :1169, :1611-1634,
// :1662-1778, :1925, :2038-2061), the default-layer tests (N-M1-18: not in M1), the visibility tests (already ported: test_layer_bulk.cpp).
#include "LayerEditTestSupport.hpp"

#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore_Integrity.hpp"

#include <set>
#include <string>
#include <vector>

using ost_ltest::add_bids;
using ost_ltest::block_when;
using ost_ltest::clear_layers;
using ost_ltest::layers_text;
using ost_ltest::names_text;
using ost_ltest::put_layer;
using ost_ltest::put_reference;
using ost_ltest::reference_tables;
using ost_ltest::sequences_text;
using ost_ltest::table_text;
using ost_ptest::TempDb;

namespace
{
    // identity_allocation.py _next_uid_preserving_references on the connection of the test.
    long long alloc(const TempDb &db, const char *table = "BidLayers")
    {
        ost_db::OstDatabase connection;
        REQUIRE(connection.open(db.path()));
        return ost_store::integrity::next_uid_preserving_references(connection, table);
    }

    // The Windows _layer_db(layers, bids=(7, 8)) of test_layer_operations.py:621: Bids 7 and 8 and these BidLayers rows (the iOS schema's four
    // template rows removed).
    void layer_db(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        clear_layers(db);
        add_bids(db, {7, 8});
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the BidLayers allocator (test_identity_allocation.py): ost_store::integrity::next_uid_preserving_references, which the S0 helper
// next_uid_within_access_limit and insert_layer build on. They run on loose tables like the Windows tests (a database made without the schema).
// ---------------------------------------------------------------------------------------------------------------

// test_identity_allocation.py:110 test_references_that_point_at_other_parent_tables_never_reserve_uids: BidPages 3, BidLayers empty,
// BidConditions (1, BidLayerUID 500), BidTakeoffs (1, BidPageUID 40): the next page is 41 and the next layer 501.
TEST(LayerEditAllocator, Windows_ReferencesToOtherParentTablesNeverReserveUids)
{
    TempDb db("alloc_other_parents", false);
    db.must("CREATE TABLE BidPages (UID INTEGER)");
    db.must("CREATE TABLE BidLayers (UID INTEGER)");
    db.must("INSERT INTO BidPages VALUES (3)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidLayerUID INTEGER)");
    db.must("INSERT INTO BidConditions VALUES (1, 500)");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidPageUID INTEGER)");
    db.must("INSERT INTO BidTakeoffs VALUES (1, 40)");
    CHECK_EQ(alloc(db, "BidPages"), 41LL);
    CHECK_EQ(alloc(db, "BidLayers"), 501LL);
}

// test_identity_allocation.py:122 test_dangling_child_references_reserve_the_highest_referenced_uid_plus_one: BidComments (1, 20), (2, 70),
// BidTexts (1, 55), BidLayers 10: 71.
TEST(LayerEditAllocator, Windows_DanglingChildReferencesReserveTheHighestReferencedUidPlusOne)
{
    TempDb db("alloc_dangling", false);
    db.must("CREATE TABLE BidLayers (UID INTEGER)");
    db.must("CREATE TABLE BidComments (UID INTEGER, BidLayerUID INTEGER)");
    db.must("CREATE TABLE BidTexts (UID INTEGER, BidLayerUID INTEGER)");
    db.must("INSERT INTO BidComments VALUES (1, 20)");
    db.must("INSERT INTO BidComments VALUES (2, 70)");
    db.must("INSERT INTO BidTexts VALUES (1, 55)");
    db.must("INSERT INTO BidLayers VALUES (10)");
    CHECK_EQ(alloc(db), 71LL);
}

// test_identity_allocation.py:134 test_child_tables_missing_the_reference_column_or_the_table_are_skipped: BidLayers 4, BidConditions without
// the reference column (UID 900): 5.
TEST(LayerEditAllocator, Windows_ChildTablesMissingTheReferenceColumnOrTheTableAreSkipped)
{
    TempDb db("alloc_skipped", false);
    db.must("CREATE TABLE BidLayers (UID INTEGER)");
    db.must("INSERT INTO BidLayers VALUES (4)");
    db.must("CREATE TABLE BidConditions (UID INTEGER)");
    db.must("INSERT INTO BidConditions VALUES (900)");
    CHECK_EQ(alloc(db), 5LL);
}

// test_identity_allocation.py:142 test_blank_text_references_are_ignored_and_numeric_text_is_honoured: a reference of '' or '0' is ignored
// (5), a reference stored as the text '12' counts (13).
TEST(LayerEditAllocator, Windows_BlankTextReferencesAreIgnoredAndNumericTextIsHonoured)
{
    TempDb db("alloc_blank_text", false);
    db.must("CREATE TABLE BidLayers (UID INTEGER)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidLayerUID)");
    db.must("INSERT INTO BidLayers VALUES (4)");
    for (const char *stored : {"''", "'0'"})
    {
        SCOPED_CONTEXT(std::string("") + stored);
        db.must("DELETE FROM BidConditions");
        db.must(std::string("INSERT INTO BidConditions VALUES (1, ") + stored + ")");
        CHECK_EQ(alloc(db), 5LL);
    }
    db.must("DELETE FROM BidConditions");
    db.must("INSERT INTO BidConditions VALUES (1, '12')");
    CHECK_EQ(alloc(db), 13LL);
}

// test_identity_allocation.py:178 test_a_reference_of_one_to_an_empty_parent_table_still_reserves_uid_one: BidLayers empty, BidComments
// (1, BidLayerUID 1): 2.
TEST(LayerEditAllocator, Windows_AReferenceOfOneToAnEmptyParentTableStillReservesUidOne)
{
    TempDb db("alloc_one_to_empty", false);
    db.must("CREATE TABLE BidLayers (UID INTEGER)");
    db.must("CREATE TABLE BidComments (UID INTEGER, BidLayerUID INTEGER)");
    db.must("INSERT INTO BidComments VALUES (1, 1)");
    CHECK_EQ(alloc(db), 2LL);
}

// test_identity_allocation.py:185 test_a_blank_reference_in_one_child_table_does_not_hide_a_later_dangling_one: BidConditions (1, NULL),
// BidZones (1, 60): 61.
TEST(LayerEditAllocator, Windows_ABlankReferenceInOneChildTableDoesNotHideALaterDanglingOne)
{
    TempDb db("alloc_blank_then_dangling", false);
    db.must("CREATE TABLE BidLayers (UID INTEGER)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidLayerUID INTEGER)");
    db.must("INSERT INTO BidConditions VALUES (1, NULL)");
    db.must("CREATE TABLE BidZones (UID INTEGER, BidLayerUID INTEGER)");
    db.must("INSERT INTO BidZones VALUES (1, 60)");
    CHECK_EQ(alloc(db), 61LL);
}

// test_identity_allocation.py:196 test_an_empty_child_table_does_not_hide_a_later_dangling_reference: BidConditions empty, BidZones (1, 60): 61.
TEST(LayerEditAllocator, Windows_AnEmptyChildTableDoesNotHideALaterDanglingReference)
{
    TempDb db("alloc_empty_then_dangling", false);
    db.must("CREATE TABLE BidLayers (UID INTEGER)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidLayerUID INTEGER)");
    db.must("CREATE TABLE BidZones (UID INTEGER, BidLayerUID INTEGER)");
    db.must("INSERT INTO BidZones VALUES (1, 60)");
    CHECK_EQ(alloc(db), 61LL);
}

// test_identity_allocation.py:204 test_every_blank_spelling_of_a_reference_is_skipped_not_just_the_first_table: BidLayers 3, BidConditions
// (1, <blank>), BidZones (1, 9): 10 for '', 0, '0' and NULL.
TEST(LayerEditAllocator, Windows_EveryBlankSpellingOfAReferenceIsSkippedNotJustTheFirstTable)
{
    for (const char *blank : {"''", "0", "'0'", "NULL"})
    {
        SCOPED_CONTEXT(std::string("") + blank);
        TempDb db(std::string("alloc_blank_") + std::to_string(static_cast<int>(blank[0])) + std::to_string(blank[1]), false);
        db.must("CREATE TABLE BidLayers (UID INTEGER)");
        db.must("INSERT INTO BidLayers VALUES (3)");
        db.must("CREATE TABLE BidConditions (UID INTEGER, BidLayerUID)");
        db.must(std::string("INSERT INTO BidConditions VALUES (1, ") + blank + ")");
        db.must("CREATE TABLE BidZones (UID INTEGER, BidLayerUID)");
        db.must("INSERT INTO BidZones VALUES (1, 9)");
        CHECK_EQ(alloc(db), 10LL);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// insert_layer
// ---------------------------------------------------------------------------------------------------------------

// test_layer_operations.py:226 test_layer_insert_reserves_dangling_layer_reference_uid_and_shifts_later_sequences: Bids {1}, layers
// (7, 1, 'Existing', -1, 1, 0, 0) and (6, 1, 'Below', -1, 2, 0, 0), BidComments (70, 1, 8): insert_layer(bid 1, "New", after 1) returns "9"
// (the comment's dangling reference 8 is reserved) and the rows are (6, Seq 3), (7, Seq 1), (9, 'New', Seq 2); no comment joins a layer.
TEST(LayerEditStore, Windows_InsertReservesDanglingLayerReferenceUidAndShiftsLaterSequences)
{
    TempDb db("w_insert_dangling");
    REQUIRE(db.schema_ok());
    clear_layers(db);
    add_bids(db, {1});
    put_layer(db, 7, "1", "Existing", "-1", "1");
    put_layer(db, 6, "1", "Below", "-1", "2");
    put_reference(db, "BidComments", 70, "1", "8");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "1", "New", 1), "9");
    CHECK_STR_EQ(layers_text(db), "(6,1,'Below',-1,3,0,0);(7,1,'Existing',-1,1,0,0);(9,1,'New',-1,2,0,0)");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidComments AS comment INNER JOIN BidLayers AS layer ON comment.BidLayerUID = layer.UID"), 0);
}

// test_layer_operations.py:834 test_bid_layer_insert_shifts_only_later_layers_of_that_bid: bid 7: A 1, B 2, C 3; (4, bid 8, 'Other', seq 3):
// insert_layer(bid 7, "New", after 2) returns "5"; C becomes 4, Other stays 3, the new row is (5, 7, 'New', -1, 3, 0, 0).
TEST(LayerEditStore, Windows_InsertShiftsOnlyLaterLayersOfThatBid)
{
    TempDb db("w_insert_scoped");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    put_layer(db, 2, "7", "B", "-1", "2");
    put_layer(db, 3, "7", "C", "-1", "3");
    put_layer(db, 4, "8", "Other", "-1", "3");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "7", "New", 2), "5");
    CHECK_STR_EQ(layers_text(db), "(1,7,'A',-1,1,0,0);(2,7,'B',-1,2,0,0);(3,7,'C',-1,4,0,0);(4,8,'Other',-1,3,0,0);(5,7,'New',-1,3,0,0)");
}

// test_layer_operations.py:872 test_bid_layer_insert_for_a_missing_bid_raises_and_writes_nothing: bid 99 has no row; Windows raises "Bids has
// no row for UID 99" and the rows stay; iOS returns "" (no UID) and the database stays byte-identical.
TEST(LayerEditStore, Windows_InsertForAMissingBidWritesNothing)
{
    TempDb db("w_insert_missing_bid");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    const std::string before = db.snapshot_all();
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "99", "New", 0, &error), "");
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(sequences_text(db), "(1,1)");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// test_layer_operations.py:2844 test_a_failed_insert_shift_propagates_before_any_row_is_added: the shift statement fails; layers (1, seq 1) and
// (2, seq 2) are unchanged and no row was added. (Windows raises; iOS returns "" after the rollback.)
TEST(LayerEditStore, Windows_AFailedInsertShiftLeavesTheRowsUnchanged)
{
    TempDb db("w_insert_shift_fails");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    put_layer(db, 2, "7", "B", "-1", "2");
    const std::string before = db.snapshot_all();
    db.block("BidLayers", "UPDATE");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "7", "New", 1), "");
    CHECK_STR_EQ(sequences_text(db), "(1,1);(2,2)");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers"), 2);
    CHECK_STR_EQ(layers_text(db), "(1,7,'A',-1,1,0,0);(2,7,'B',-1,2,0,0)");
    CHECK_STR_EQ(db.snapshot_all(), before); // the dump covers the tables, not the trigger
}

// ---------------------------------------------------------------------------------------------------------------
// delete_layers
// ---------------------------------------------------------------------------------------------------------------

// test_layer_operations.py:145 test_delete_layer_clears_indexed_annotation_shape_layer_refs: layers 30 (seq 1) and 31 (seq 2) of bid 1,
// BidAnnotationRects (20, layer 30): delete 30 returns True, the rectangle's reference is NULL and the row stays, layer 31 becomes seq 1.
TEST(LayerEditStore, Windows_DeleteClearsIndexedAnnotationShapeLayerRefs)
{
    TempDb db("w_delete_rect_ref");
    REQUIRE(db.schema_ok());
    clear_layers(db);
    add_bids(db, {1});
    put_layer(db, 30, "1", "L30", "-1", "1");
    put_layer(db, 31, "1", "L31", "-1", "2");
    put_reference(db, "BidAnnotationRects", 20, "NULL", "30");
    CHECK(ost_store::delete_layers(db.path(), "1", {"30"}));
    CHECK_STR_EQ(db.text("SELECT ifnull(BidLayerUID, 'NULL') FROM BidAnnotationRects WHERE UID = 20"), "NULL");
    CHECK_STR_EQ(sequences_text(db), "(31,1)");
    CHECK_STR_EQ(table_text(db, "BidAnnotationRects", "UID"), "(20)");
}

// test_layer_operations.py:194 test_delete_layer_refuses_template_and_locked_layers_without_mutation: layers 30 (template), 31 (locked),
// 32 (plain) of bid 1, rectangles (20, 30) and (21, 31): delete 30 and delete 31 return False; the sequences (1, 2, 3) and the references are
// unchanged.
TEST(LayerEditStore, Windows_DeleteRefusesTemplateAndLockedLayersWithoutMutation)
{
    TempDb db("w_delete_refuses");
    REQUIRE(db.schema_ok());
    clear_layers(db);
    add_bids(db, {1});
    put_layer(db, 30, "1", "L30", "-1", "1", "-1", "0");
    put_layer(db, 31, "1", "L31", "-1", "2", "0", "-1");
    put_layer(db, 32, "1", "L32", "-1", "3", "0", "0");
    put_reference(db, "BidAnnotationRects", 20, "NULL", "30");
    put_reference(db, "BidAnnotationRects", 21, "NULL", "31");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::delete_layers(db.path(), "1", {"30"}));
    CHECK_FALSE(ost_store::delete_layers(db.path(), "1", {"31"}));
    CHECK_STR_EQ(sequences_text(db), "(30,1);(31,2);(32,3)");
    CHECK_STR_EQ(table_text(db, "BidAnnotationRects", "UID, BidLayerUID"), "(20,30);(21,31)");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// test_layer_operations.py:1064 test_bid_layer_delete_clears_every_layer_reference_table_and_scopes_renumbering: layers (1, 7, seq 1),
// (2, 7, seq 2), (3, 7, seq 3), (4, 8 'Other', seq 3); every one of the 11 tables holds (100, layer 2), (101, 3), (102, 4); delete 2 (bid 7):
// (100, NULL), (101, 3), (102, 4) in EVERY table; sequences (1, 1), (3, 2), (4, 3): the other bid's row is untouched.
TEST(LayerEditStore, Windows_DeleteClearsEveryLayerReferenceTableAndScopesRenumbering)
{
    TempDb db("w_delete_all_tables");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    put_layer(db, 2, "7", "B", "-1", "2");
    put_layer(db, 3, "7", "C", "-1", "3");
    put_layer(db, 4, "8", "Other", "-1", "3");
    for (const std::string &table : reference_tables())
    {
        put_reference(db, table, 100, "NULL", "2");
        put_reference(db, table, 101, "NULL", "3");
        put_reference(db, table, 102, "NULL", "4");
    }
    CHECK(ost_store::delete_layers(db.path(), "7", {"2"}));
    CHECK_STR_EQ(sequences_text(db), "(1,1);(3,2);(4,3)");
    for (const std::string &table : reference_tables())
    {
        SCOPED_CONTEXT(std::string("") + table);
        CHECK_STR_EQ(table_text(db, table, "UID, BidLayerUID"), "(100,NULL);(101,3);(102,4)");
    }
}

// test_layer_operations.py:1091 test_bid_layer_delete_skips_missing_and_columnless_reference_tables: BidTexts without the reference column
// (row 5), BidComments with it (row 6 -> layer 1), the other tables missing: delete 1 succeeds, BidTexts is untouched, the comment's reference
// is NULL, the other layer is renumbered to 1. (Windows asserts no warning is logged; iOS has no log to assert.)
TEST(LayerEditStore, Windows_DeleteSkipsMissingAndColumnlessReferenceTables)
{
    TempDb db("w_delete_skips_tables");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    put_layer(db, 2, "7", "B", "-1", "2");
    db.must("DROP TABLE BidTexts");
    db.must("CREATE TABLE BidTexts (UID INTEGER)");
    db.must("INSERT INTO BidTexts VALUES (5)");
    put_reference(db, "BidComments", 6, "NULL", "1");
    db.must("DROP TABLE BidHotLinks"); // a missing table
    db.must("DROP TABLE BidCallOuts");
    CHECK(ost_store::delete_layers(db.path(), "7", {"1"}));
    CHECK_STR_EQ(table_text(db, "BidTexts", "UID"), "(5)");
    CHECK_STR_EQ(table_text(db, "BidComments", "UID, BidLayerUID"), "(6,NULL)");
    CHECK_STR_EQ(sequences_text(db), "(2,1)");
}

// test_layer_operations.py:1116 test_bid_layer_delete_unknown_layer_is_a_missing_owner_error: layer 5 does not exist; Windows raises "BidLayers
// has no row for UID 5" and the rows stay; iOS returns false and the database stays byte-identical.
TEST(LayerEditStore, Windows_DeleteOfAnUnknownLayerIsRefused)
{
    TempDb db("w_delete_unknown");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::delete_layers(db.path(), "7", {"5"}));
    CHECK_STR_EQ(table_text(db, "BidLayers", "UID"), "(1)");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// test_layer_operations.py:1147 test_bid_layer_delete_reraises_a_failed_clear_when_the_error_is_fatal (the production behaviour, LY-5; the
// lenient test :1122 is deliberately not ported): a trigger blocks the clear in BidTexts (row 5 -> layer 1); the delete fails as a whole and
// the layers (1, seq 1), (2, seq 2) stay.
TEST(LayerEditStore, Windows_AFailedReferenceClearAbortsTheWholeDelete)
{
    TempDb db("w_delete_clear_fails");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    put_layer(db, 2, "7", "B", "-1", "2");
    put_reference(db, "BidTexts", 5, "NULL", "1");
    db.block("BidTexts", "UPDATE");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::delete_layers(db.path(), "7", {"1"}));
    CHECK_STR_EQ(sequences_text(db), "(1,1);(2,2)");
    CHECK_STR_EQ(table_text(db, "BidTexts", "UID, BidLayerUID"), "(5,1)");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// test_layer_operations.py:1180 test_bid_layer_delete_does_not_renumber_when_the_layer_has_no_sequence: (1, seq NULL), (2, seq 1): delete 1,
// the other stays 1.
TEST(LayerEditStore, Windows_DeleteDoesNotRenumberWhenTheLayerHasNoSequence)
{
    TempDb db("w_delete_null_sequence");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "NULL");
    put_layer(db, 2, "7", "B", "-1", "1");
    CHECK(ost_store::delete_layers(db.path(), "7", {"1"}));
    CHECK_STR_EQ(sequences_text(db), "(2,1)");
}

// test_layer_operations.py:1185 test_bid_layer_delete_refuses_a_locked_layer: (1, locked): refused, the row stays.
TEST(LayerEditStore, Windows_DeleteRefusesALockedLayer)
{
    TempDb db("w_delete_locked");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1", "0", "-1");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::delete_layers(db.path(), "7", {"1"}));
    CHECK_STR_EQ(table_text(db, "BidLayers", "UID"), "(1)");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// test_layer_operations.py:2832 test_a_failed_renumbering_after_a_bid_layer_delete_propagates: the renumber statement fails; the delete (and
// its clearing) is rolled back: layers 1 and 2 stay with their sequences.
TEST(LayerEditStore, Windows_AFailedRenumberingRollsTheDeleteBack)
{
    TempDb db("w_delete_renumber_fails");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    put_layer(db, 2, "7", "B", "-1", "2");
    put_reference(db, "BidTexts", 5, "NULL", "1");
    db.block("BidLayers", "UPDATE");
    CHECK_FALSE(ost_store::delete_layers(db.path(), "7", {"1"}));
    CHECK_STR_EQ(sequences_text(db), "(1,1);(2,2)");
    CHECK_STR_EQ(table_text(db, "BidTexts", "UID, BidLayerUID"), "(5,1)"); // the clear was rolled back with it
}

// ---------------------------------------------------------------------------------------------------------------
// rename_layer
// ---------------------------------------------------------------------------------------------------------------

// test_layer_operations.py:726 test_bid_layer_rename_changes_only_unlocked_non_template_layers: 10 'A' plain, 11 'B' locked, 12 'C' template, 20 'D'
// of bid 8: rename 10, 11 and 12; only 10 changes.
TEST(LayerEditStore, Windows_RenameChangesOnlyUnlockedNonTemplateLayers)
{
    TempDb db("w_rename_guards");
    layer_db(db);
    put_layer(db, 10, "7", "A", "-1", "1", "0", "0");
    put_layer(db, 11, "7", "B", "-1", "2", "0", "-1");
    put_layer(db, 12, "7", "C", "-1", "3", "-1", "0");
    put_layer(db, 20, "8", "D", "-1", "1", "0", "0");
    CHECK(ost_store::rename_layer(db.path(), "7", "10", "Renamed"));
    CHECK_FALSE(ost_store::rename_layer(db.path(), "7", "11", "Locked rename"));
    CHECK_FALSE(ost_store::rename_layer(db.path(), "7", "12", "Template rename"));
    CHECK_STR_EQ(names_text(db), "(10,'Renamed');(11,'B');(12,'C');(20,'D')");
}

// test_layer_operations.py:752 test_bid_layer_rename_rejects_duplicate_uid_and_unowned_layer, the second half (the duplicate physical UID is not
// possible on iOS): a shared template row (30, no bid) is refused (Windows raises IncoherentBidOwnedScopeError); the rows are unchanged.
TEST(LayerEditStore, Windows_RenameOfASharedTemplateRowIsRefused)
{
    TempDb db("w_rename_shared");
    layer_db(db);
    put_layer(db, 10, "7", "A", "-1", "1", "0", "0");
    put_layer(db, 30, "NULL", "Shared", "-1", "1", "-1", "-1");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::rename_layer(db.path(), "7", "30", "X"));
    CHECK_STR_EQ(names_text(db), "(10,'A');(30,'Shared')");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// test_layer_operations.py:2017 test_a_rename_blocked_by_the_locked_or_template_guard_reports_no_change: 10 plain, 11 locked, 12 template
// (IsTemplate -1, IsLocked 0): rename 11 and 12 return False and change nothing; rename 10 returns True.
TEST(LayerEditStore, Windows_ARenameBlockedByTheGuardReportsNoChange)
{
    TempDb db("w_rename_reports");
    layer_db(db);
    put_layer(db, 10, "7", "A", "-1", "1", "0", "0");
    put_layer(db, 11, "7", "Locked", "-1", "2", "0", "-1");
    put_layer(db, 12, "7", "Template", "-1", "3", "-1", "0");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::rename_layer(db.path(), "7", "11", "Renamed"));
    CHECK_FALSE(ost_store::rename_layer(db.path(), "7", "12", "Renamed"));
    CHECK_STR_EQ(names_text(db), "(10,'A');(11,'Locked');(12,'Template')");
    CHECK_STR_EQ(db.snapshot_all(), before);
    CHECK(ost_store::rename_layer(db.path(), "7", "10", "Renamed"));
    CHECK_STR_EQ(names_text(db), "(10,'Renamed');(11,'Locked');(12,'Template')");
}

// ---------------------------------------------------------------------------------------------------------------
// swap_layer_sequence
// ---------------------------------------------------------------------------------------------------------------

// test_layer_operations.py:311 test_swap_layer_sequence_exchanges_sequences_and_rejects_cross_bid_pair: (10, bid 7, 1), (11, 7, 2), (12, 7, 3),
// (20, 8, 1): swap 10 and 12 gives 10 -> 3, 12 -> 1; swap 10 and 20 (another bid) is refused (Windows raises IncoherentBidOwnedScopeError) and
// the rows are unchanged.
TEST(LayerEditStore, Windows_SwapExchangesSequencesAndRejectsACrossBidPair)
{
    TempDb db("w_swap_cross_bid");
    layer_db(db);
    put_layer(db, 10, "7", "A", "-1", "1");
    put_layer(db, 11, "7", "B", "-1", "2");
    put_layer(db, 12, "7", "C", "-1", "3");
    put_layer(db, 20, "8", "D", "-1", "1");
    CHECK(ost_store::swap_layer_sequence(db.path(), "7", "10", "12"));
    CHECK_STR_EQ(sequences_text(db), "(10,3);(11,2);(12,1);(20,1)");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "7", "10", "20"));
    CHECK_STR_EQ(sequences_text(db), "(10,3);(11,2);(12,1);(20,1)");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// test_layer_operations.py:1238 test_bid_layer_swap_refuses_a_pair_with_an_unset_sequence: (1, seq 1), (2, seq NULL): swap is refused,
// unchanged.
TEST(LayerEditStore, Windows_SwapRefusesAPairWithAnUnsetSequence)
{
    TempDb db("w_swap_null_second");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    put_layer(db, 2, "7", "B", "-1", "NULL");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "7", "1", "2"));
    CHECK_STR_EQ(sequences_text(db), "(1,1);(2,NULL)");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// test_layer_operations.py:1243 test_bid_layer_swap_refuses_a_pair_whose_first_sequence_is_unset: (1, seq NULL), (2, seq 2): refused, unchanged.
TEST(LayerEditStore, Windows_SwapRefusesAPairWhoseFirstSequenceIsUnset)
{
    TempDb db("w_swap_null_first");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "NULL");
    put_layer(db, 2, "7", "B", "-1", "2");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "7", "1", "2"));
    CHECK_STR_EQ(sequences_text(db), "(1,NULL);(2,2)");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// test_layer_operations.py:1257 test_swapping_a_bid_layer_with_itself_is_refused_without_writing: swap 1 with 1: refused, unchanged.
TEST(LayerEditStore, Windows_SwappingALayerWithItselfIsRefused)
{
    TempDb db("w_swap_self");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    put_layer(db, 2, "7", "B", "-1", "2");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "7", "1", "1"));
    CHECK_STR_EQ(sequences_text(db), "(1,1);(2,2)");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// test_layer_operations.py:1262 test_bid_layer_swap_of_a_missing_layer_is_a_missing_owner_error: layer 9 does not exist; Windows raises
// "BidLayers has no row for UID 9"; iOS refuses; the rows are unchanged.
TEST(LayerEditStore, Windows_SwapWithAMissingLayerIsRefused)
{
    TempDb db("w_swap_missing");
    layer_db(db);
    put_layer(db, 1, "7", "A", "-1", "1");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "7", "1", "9"));
    CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "7", "9", "1"));
    CHECK_STR_EQ(sequences_text(db), "(1,1)");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// ---------------------------------------------------------------------------------------------------------------
// read_layer_uids_in_use (test_settings_reader.py, settings_reader.py:450-482)
// ---------------------------------------------------------------------------------------------------------------

// test_settings_reader.py:268 test_layer_uids_in_use_includes_condition_and_annotation_tables: BidConditions (bid 7, layer 101) and (8, 102),
// BidAnnotationRects (7, 201) and (8, 202), BidHotLinks (7, NULL), BidTexts (7, 301): bid 7 uses {101, 201, 301}.
TEST(LayerEditUsagePorts, Windows_ConditionAndAnnotationTablesAreIncluded)
{
    TempDb db("w_usage_tables");
    REQUIRE(db.schema_ok());
    add_bids(db, {7, 8});
    put_reference(db, "BidConditions", 1, "7", "101");
    put_reference(db, "BidConditions", 2, "8", "102");
    put_reference(db, "BidAnnotationRects", 1, "7", "201");
    put_reference(db, "BidAnnotationRects", 2, "8", "202");
    put_reference(db, "BidHotLinks", 1, "7", "NULL");
    put_reference(db, "BidTexts", 1, "7", "301");
    std::set<std::string> used;
    CHECK(ost_store::read_layer_uids_in_use(db.path(), "7", used));
    CHECK(used == (std::set<std::string>{"101", "201", "301"}));
}

// test_settings_reader.py:299 test_layer_uids_in_use_skips_missing_tables_and_layerless_columns: only BidConditions (bid 7, layer 101) and
// BidTexts WITHOUT a BidLayerUID column exist: bid 7 uses {101}, bid 8 nothing.
TEST(LayerEditUsagePorts, Windows_MissingTablesAndLayerlessColumnsAreSkipped)
{
    TempDb db("w_usage_skips");
    REQUIRE(db.schema_ok());
    add_bids(db, {7, 8});
    put_reference(db, "BidConditions", 1, "7", "101");
    for (const char *table : {"BidZones", "BidComments", "BidAnnotationClouds", "BidAnnotationOvals", "BidAnnotationPolygons",
                              "BidAnnotationRects", "BidHighlights", "BidHotLinks", "BidCallOuts", "BidTexts"})
        db.must(std::string("DROP TABLE ") + table);
    db.must("CREATE TABLE BidTexts (UID INTEGER, BidUID INTEGER)");
    db.must("INSERT INTO BidTexts VALUES (1, 7)");
    std::set<std::string> used;
    CHECK(ost_store::read_layer_uids_in_use(db.path(), "7", used));
    CHECK(used == (std::set<std::string>{"101"}));
    CHECK(ost_store::read_layer_uids_in_use(db.path(), "8", used));
    CHECK(used.empty());
}

// test_settings_reader.py:874 test_layer_usage_filters_by_bid_when_the_table_has_one_and_skips_nulls: BidConditions (1, 7, 101), (2, 7, 101),
// (3, 8, 102), (4, 7, NULL); BidHotLinks WITHOUT a BidUID column (1, 301), (2, NULL); BidTexts (1, 7) without BidLayerUID: bid 7 {101, 301},
// bid 8 {102, 301}, bid 9 {301}: a table without the bid column counts its references for every bid.
TEST(LayerEditUsagePorts, Windows_UsageFiltersByBidWhenTheTableHasOneAndSkipsNulls)
{
    TempDb db("w_usage_filter");
    REQUIRE(db.schema_ok());
    add_bids(db, {7, 8, 9});
    put_reference(db, "BidConditions", 1, "7", "101");
    put_reference(db, "BidConditions", 2, "7", "101");
    put_reference(db, "BidConditions", 3, "8", "102");
    put_reference(db, "BidConditions", 4, "7", "NULL");
    db.must("DROP TABLE BidHotLinks");
    db.must("CREATE TABLE BidHotLinks (UID INTEGER, BidLayerUID INTEGER)");
    db.must("INSERT INTO BidHotLinks VALUES (1, 301)");
    db.must("INSERT INTO BidHotLinks VALUES (2, NULL)");
    db.must("DROP TABLE BidTexts");
    db.must("CREATE TABLE BidTexts (UID INTEGER, BidUID INTEGER)");
    db.must("INSERT INTO BidTexts VALUES (1, 7)");
    std::set<std::string> used;
    CHECK(ost_store::read_layer_uids_in_use(db.path(), "7", used));
    CHECK(used == (std::set<std::string>{"101", "301"}));
    CHECK(ost_store::read_layer_uids_in_use(db.path(), "8", used));
    CHECK(used == (std::set<std::string>{"102", "301"}));
    CHECK(ost_store::read_layer_uids_in_use(db.path(), "9", used));
    CHECK(used == (std::set<std::string>{"301"}));
}

// ---------------------------------------------------------------------------------------------------------------
// the lifecycle scenario (test_layer_operations.py:2226-2530 _bid_layer_lifecycle_through_real_access)
// ---------------------------------------------------------------------------------------------------------------

// The one scenario Windows runs against a real Access database, as one SQLite scenario with the same seeds and expected rows: bids 11 and 12,
// layers 101-104 of bid 11 (104 locked) and 201, 202 of bid 12; BidConditions 301 (bid 11, layer 102), 302 (12, 202), 303 (11, 103), BidComments
// 401 (11, 102), BidAnnotationRects 501 (11, 102) and 502 (12, 202). The visibility steps of the Windows scenario (already ported in
// test_layer_bulk.cpp) are seeded as their outcome: 101-104 hidden (Show 0), 201, 202 visible. The failing writers of Windows (a subclass that
// raises at the renumber / at the INSERT) become triggers; the "unrelated committing write after the failure" (hiding layer 202) runs in the same
// way, so a rollback that only happens at connection close would show.
TEST(LayerEditLifecycle, Windows_BidLayerLifecycle)
{
    TempDb db("w_lifecycle");
    REQUIRE(db.schema_ok());
    clear_layers(db);
    add_bids(db, {11, 12});
    put_layer(db, 101, "11", "A", "0", "1");
    put_layer(db, 102, "11", "B", "0", "2");
    put_layer(db, 103, "11", "C", "0", "3");
    put_layer(db, 104, "11", "Locked", "0", "4", "0", "-1");
    put_layer(db, 201, "12", "X", "-1", "1");
    put_layer(db, 202, "12", "Y", "-1", "2");
    db.must("INSERT INTO BidConditions (UID, BidUID, BidLayerUID, Name) VALUES (301, 11, 102, 'condition')");
    db.must("INSERT INTO BidConditions (UID, BidUID, BidLayerUID, Name) VALUES (302, 12, 202, 'condition')");
    db.must("INSERT INTO BidConditions (UID, BidUID, BidLayerUID, Name) VALUES (303, 11, 103, 'condition')");
    put_reference(db, "BidComments", 401, "11", "102");
    put_reference(db, "BidAnnotationRects", 501, "11", "102");
    put_reference(db, "BidAnnotationRects", 502, "12", "202");
    const std::string own_layers = "BidUID IS NOT NULL";

    // rename: quoting survives binding; the locked guard is reported; an unknown layer is refused (Windows raises)
    const std::string name = "O'Brien \"A\" \xC3\xA9; DROP";
    CHECK(ost_store::rename_layer(db.path(), "11", "101", name));
    CHECK_FALSE(ost_store::rename_layer(db.path(), "11", "104", "Nope"));
    CHECK_FALSE(ost_store::rename_layer(db.path(), "11", "999", "Nope"));
    CHECK_STR_EQ(names_text(db, own_layers),
                 "(101,'O''Brien \"A\" \xC3\xA9; DROP');(102,'B');(103,'C');(104,'Locked');(201,'X');(202,'Y')");

    // insert shifts only the later layers of that bid
    const std::string new_uid = ost_store::insert_layer(db.path(), "11", "New", 1);
    REQUIRE(!new_uid.empty());
    CHECK_STR_EQ(new_uid, "203"); // MAX(UID) + 1 (Windows: any uid not among the seeds)
    CHECK_STR_EQ(layers_text(db, own_layers),
                 "(101,11,'O''Brien \"A\" \xC3\xA9; DROP',0,1,0,0);(102,11,'B',0,3,0,0);(103,11,'C',0,4,0,0);(104,11,'Locked',0,5,0,-1);"
                 "(201,12,'X',-1,1,0,0);(202,12,'Y',-1,2,0,0);(203,11,'New',-1,2,0,0)");

    // swap exchanges two sequences inside one bid and refuses a cross-bid pair
    CHECK(ost_store::swap_layer_sequence(db.path(), "11", "101", "103"));
    CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "11", "101", "201"));
    CHECK_STR_EQ(sequences_text(db, own_layers), "(101,4);(102,3);(103,1);(104,5);(201,1);(202,2);(203,2)");

    // a failing renumber statement rolls back the cascade and the delete; an unrelated write afterwards must not carry it along
    const std::string before_layers = layers_text(db, own_layers);
    const std::string before_refs = ost_ltest::references_text(db);
    const std::string blocker = db.block("BidLayers", "UPDATE");
    CHECK_FALSE(ost_store::delete_layers(db.path(), "11", {"102"}));
    db.unblock(blocker);
    CHECK_STR_EQ(layers_text(db, own_layers), before_layers);
    CHECK_STR_EQ(ost_ltest::references_text(db), before_refs);
    CHECK(ost_store::set_layer_visibility(db.path(), "11", "202", false)); // the unrelated committing write (Windows hides layer 202)
    CHECK_STR_EQ(table_text(db, "BidLayers", "UID, Show", "BidUID = 12"), "(201,-1);(202,0)");
    CHECK_STR_EQ(layers_text(db, own_layers),
                 "(101,11,'O''Brien \"A\" \xC3\xA9; DROP',0,4,0,0);(102,11,'B',0,3,0,0);(103,11,'C',0,1,0,0);(104,11,'Locked',0,5,0,-1);"
                 "(201,12,'X',-1,1,0,0);(202,12,'Y',0,2,0,0);(203,11,'New',-1,2,0,0)");
    CHECK(ost_store::set_layer_visibility(db.path(), "11", "202", true));

    // the delete: references of that layer only are cleared, only later layers of that bid are renumbered, guarded layers are refused
    CHECK(ost_store::delete_layers(db.path(), "11", {"102"}));
    CHECK_FALSE(ost_store::delete_layers(db.path(), "11", {"104"}));
    CHECK_STR_EQ(layers_text(db, own_layers),
                 "(101,11,'O''Brien \"A\" \xC3\xA9; DROP',0,3,0,0);(103,11,'C',0,1,0,0);(104,11,'Locked',0,4,0,-1);(201,12,'X',-1,1,0,0);"
                 "(202,12,'Y',-1,2,0,0);(203,11,'New',-1,2,0,0)");
    CHECK_STR_EQ(table_text(db, "BidConditions", "UID, BidLayerUID"), "(301,NULL);(302,202);(303,103)");
    CHECK_STR_EQ(table_text(db, "BidComments", "UID, BidLayerUID"), "(401,NULL)");
    CHECK_STR_EQ(table_text(db, "BidAnnotationRects", "UID, BidLayerUID"), "(501,NULL);(502,202)");

    // a failing insert leaves the sequences it had already shifted alone (the INSERT fails after the shift)
    const std::string before_insert = layers_text(db, own_layers);
    const std::string insert_blocker = db.block("BidLayers", "INSERT");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "11", "Doomed", 0), "");
    db.unblock(insert_blocker);
    CHECK_STR_EQ(layers_text(db, own_layers), before_insert);
    CHECK(ost_store::set_layer_visibility(db.path(), "11", "202", false)); // an unrelated write commits only itself
    CHECK_STR_EQ(table_text(db, "BidLayers", "UID, Sequence", own_layers),
                 "(101,3);(103,1);(104,4);(201,1);(202,2);(203,2)");
}
