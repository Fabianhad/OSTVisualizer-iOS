// M1 layers (plan 5.3), DATA SAFETY of ost_store::insert_layer: every refusal leaves the database byte-identical (snapshot_all before and
// after), the exact rows of every success, the 2147483647 limits (UID and Sequence, no half shift), the failure points (shift, INSERT, COMMIT),
// the bid lock, malformed / missing bids, the name rules inside the store (they cannot be bypassed), a failed read.
//
// Windows has no test for any of this beyond the four ported in test_layer_edit_store.cpp (the research map section 4: "there is NO Windows test
// for name length, UID or Sequence limits"). The Windows behaviours the success rows pin: layer_operations.py:78-114 (shift of THIS bid's later
// rows, new row Show -1 / Sequence after + 1 / IsTemplate 0 / IsLocked 0, UID = MAX + 1 raised above dangling references); the deliberate
// differences: register LY-1 (Comments always taken), LY-7 (limits), LY-8 (50 UTF-16 units), LY-9 (the open bid), LY-14 (control characters),
// N-M1-01 (the bid lock). The world is seed_m1 (LayerEditTestSupport.hpp): bid 10 with the built-in copies Image / Annotation / Default / Comments
// (101-104, locked), Walls 105 (used by condition 301), Floors 106 (used by annotation 501), Unused 107, the shared template rows 1-4 and 8, bid 20 with
// Other 201 and Zebra 202; the next UID is 203.
#include "LayerEditTestSupport.hpp"

#include <climits>
#include <cstdio>
#include <string>
#include <vector>

using ost_ltest::add_bids;
using ost_ltest::clear_layers;
using ost_ltest::contains;
using ost_ltest::kEmoji;
using ost_ltest::kM1Layers;
using ost_ltest::layers_text;
using ost_ltest::put_layer;
using ost_ltest::put_reference;
using ost_ltest::reference_tables;
using ost_ltest::repeat;
using ost_ltest::seed_m1;
using ost_ptest::TempDb;

namespace
{
    // A refusal: no UID, the expected user text, and the whole database byte-identical.
    void expect_refused(const TempDb &db, const std::string &bid, const std::string &name, long long after, const std::string &expected_error)
    {
        const std::string before = db.snapshot_all();
        std::string error = "stale";
        CHECK_STR_EQ(ost_store::insert_layer(db.path(), bid, name, after, &error), "");
        CHECK_STR_EQ(error, expected_error);
        CHECK_STR_EQ(db.snapshot_all(), before);
    }

    std::string duplicate_text(const std::string &name)
    {
        return "Layer " + name + " already exists.";
    }

    const std::string kNoIdentity = "Cannot add a layer: no identity is left below 2147483647.";
    const std::string kSequenceLimit = "Cannot add a layer: no sequence number is left below 2147483647.";

    std::string with_new_row(const std::string &rows_before, const std::string &new_row)
    {
        return rows_before + ";" + new_row;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the rows of a success
// ---------------------------------------------------------------------------------------------------------------

// layer_operations.py:88-113 "New Layer" at the end (after the highest own sequence 6): Show -1, IsTemplate 0, IsLocked 0, Sequence 7, UID 203
// (MAX + 1); nothing else in the database moves (snapshot of the other tables).
TEST(LayerEditInsert, ANewLayerAtTheEndIsTheOnlyChange)
{
    TempDb db("ins_end");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string references = ost_ltest::references_text(db);
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Roofing", 6, &error), "203");
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(layers_text(db), with_new_row(kM1Layers, "(203,10,'Roofing',-1,7,0,0)"));
    CHECK_STR_EQ(ost_ltest::references_text(db), references);
}

// "New Layer Below Walls" (after sequence 4): the new row gets 5; the own rows with a greater sequence move up by one (Floors 6, Unused 7), the
// ones below stay, the shared rows (no bid), the other bid's rows and the locked copies below 4 do not move.
TEST(LayerEditInsert, ALayerBelowASelectedRowShiftsExactlyTheLaterRowsOfThatBid)
{
    TempDb db("ins_middle");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Ground", 4), "203");
    CHECK_STR_EQ(layers_text(db),
                 "(1,NULL,'Default',-1,2,-1,-1);(2,NULL,'Annotation',-1,1,-1,-1);(3,NULL,'Image',-1,0,-1,-1);(4,NULL,'Comments',-1,3,-1,-1);"
                 "(8,NULL,'Shared Template',-1,7,-1,-1);(101,10,'Image',-1,0,0,-1);(102,10,'Annotation',-1,1,0,-1);(103,10,'Default',-1,2,0,-1);"
                 "(104,10,'Comments',-1,3,0,-1);(105,10,'Walls',-1,4,0,0);(106,10,'Floors',-1,6,0,0);(107,10,'Unused',-1,7,0,0);"
                 "(201,20,'Other',-1,1,0,0);(202,20,'Zebra',-1,2,0,0);(203,10,'Ground',-1,5,0,0)");
}

// after_sequence 0 (the first position): every own row with a sequence above 0 moves, the locked Image copy at 0 stays; the shared template row
// 3 'Image' at sequence 0 and the shared rows 1, 2, 4, 8 never move because they have no bid.
TEST(LayerEditInsert, AfterSequenceZeroShiftsEveryOwnRowAboveZero)
{
    TempDb db("ins_zero");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "First", 0), "203");
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,0);(102,2);(103,3);(104,4);(105,5);(106,6);(107,7);(203,1)");
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID IS NULL OR BidUID = 20"), "(1,2);(2,1);(3,0);(4,3);(8,7);(201,1);(202,2)");
}

// A negative after_sequence (Windows does not restrict it): the new row gets after + 1 = 0 and every own row above -1 (all of them) moves.
TEST(LayerEditInsert, ANegativeAfterSequenceIsAcceptedLikeWindows)
{
    TempDb db("ins_negative");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Zero", -1), "203");
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,1);(102,2);(103,3);(104,4);(105,5);(106,6);(107,7);(203,0)");
}

// A NULL Sequence never moves (`Sequence > ?` is not true for NULL), an after_sequence between the rows is exact, a visible-by-default row:
// Show is -1 even when Hide All hid every other layer (research 2.3).
TEST(LayerEditInsert, ARowWithoutASequenceDoesNotMoveAndTheNewLayerIsVisibleWhateverTheOthersAre)
{
    TempDb db("ins_null_sequence");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 108, "10", "NoSequence", "-1", "NULL");
    db.must("UPDATE BidLayers SET Show = 0 WHERE BidUID = 10");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Fresh", 0), "203");
    CHECK_STR_EQ(ost_ltest::table_text(db, "BidLayers", "UID, Show, Sequence", "BidUID = 10"),
                 "(101,0,0);(102,0,2);(103,0,3);(104,0,4);(105,0,5);(106,0,6);(107,0,7);(108,0,NULL);(203,-1,1)");
}

// The name is stored STRIPPED, with its case and its inner spaces (layers_dialog.py:430 `name = item.text(2).strip()`).
TEST(LayerEditInsert, TheNameIsStoredStrippedWithCaseAndInnerSpacesKept)
{
    TempDb db("ins_stripped");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "  Two  Words \t", 6), "203");
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 203"), "Two  Words");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "\xC2\xA0" "MiXeD Case" "\xE2\x80\x83", 7), "204");
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 204"), "MiXeD Case");
}

// Quotes, semicolons and SQL text are bound, never concatenated: the layer is created with the exact name and the table is intact.
TEST(LayerEditInsert, QuotesAndSqlTextAreBoundNotExecuted)
{
    TempDb db("ins_injection");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string name = "O'Brien \"A\" \xC3\xA9; DROP TABLE BidLayers;--";
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", name, 6), "203");
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 203"), name);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers"), 15);
    CHECK_STR_EQ(db.text("SELECT name FROM sqlite_master WHERE name = 'BidLayers'"), "BidLayers");
}

// Windows has no per-bid name rule beyond the listed rows: a name that only ANOTHER bid uses is fine ('Other' belongs to bid 20).
TEST(LayerEditInsert, ANameUsedOnlyByAnotherBidIsAllowed)
{
    TempDb db("ins_other_bid_name");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Other", 6), "203");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "zebra", 7), "204");
    CHECK_STR_EQ(ost_ltest::names_text(db, "BidUID = 10 AND UID > 202"), "(203,'Other');(204,'zebra')");
}

// A bid without layers (but with a Bids row): the first layer gets Sequence after + 1 and the next free UID.
TEST(LayerEditInsert, ABidWithoutLayersGetsItsFirstLayer)
{
    TempDb db("ins_first_layer");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    add_bids(db, {30});
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "30", "First", 0), "203");
    CHECK_STR_EQ(layers_text(db, "BidUID = 30"), "(203,30,'First',-1,1,0,0)");
}

// test_layers_sidebar.py:222: an EMPTY BidLayers table (the last layer disappeared): the UID is 1 and the row is (1, bid, name, -1, 1, 0, 0).
TEST(LayerEditInsert, AnEmptyTableAllocatesUidOne)
{
    TempDb db("ins_empty_table");
    REQUIRE(db.schema_ok());
    clear_layers(db);
    add_bids(db, {10});
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "First", 0), "1");
    CHECK_STR_EQ(layers_text(db), "(1,10,'First',-1,1,0,0)");
}

// ---------------------------------------------------------------------------------------------------------------
// UID: the limit and the reserved references
// ---------------------------------------------------------------------------------------------------------------

// Plan 4.2: refuse BEFORE any write when the new UID would pass 2147483647. MAX(UID) = 2147483646 is the last one that works (the new row is
// 2147483647); MAX(UID) = 2147483647 is refused with the "no identity" text; nothing was shifted.
TEST(LayerEditInsert, TheLastFreeUidIsUsedAndPastItTheInsertIsRefused)
{
    {
        TempDb db("ins_uid_last");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        put_layer(db, 2147483646LL, "10", "Big", "-1", "7");
        CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Last", 7), "2147483647");
        CHECK_STR_EQ(layers_text(db, "UID >= 2147483646"), "(2147483646,10,'Big',-1,7,0,0);(2147483647,10,'Last',-1,8,0,0)");
    }
    {
        TempDb db("ins_uid_max");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        put_layer(db, 2147483647LL, "10", "Big", "-1", "7");
        SCOPED_CONTEXT("MAX(UID) = 2147483647");
        expect_refused(db, "10", "Overflow", 6, kNoIdentity);
        expect_refused(db, "10", "Overflow", 0, kNoIdentity);
    }
}

// Windows _next_uid_preserving_references: a dangling BidLayerUID in ANY of the 11 reference tables reserves its value + 1 (a UID some orphan row
// still points to is never reused), so with layer 1 and a dangling 500 the next layer is 501; a dangling 2147483647 leaves no UID: refused.
TEST(LayerEditInsert, ADanglingReferenceInEachOfTheElevenTablesReservesItsUid)
{
    for (const std::string &table : reference_tables())
    {
        SCOPED_CONTEXT(std::string("") + table);
        TempDb db("ins_dangling_" + table);
        REQUIRE(db.schema_ok());
        clear_layers(db);
        add_bids(db, {10});
        put_layer(db, 1, "10", "A", "-1", "1");
        put_reference(db, table, 70, "10", "500");
        CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "New", 1), "501");
        CHECK_STR_EQ(layers_text(db), "(1,10,'A',-1,1,0,0);(501,10,'New',-1,2,0,0)");
    }
    for (const std::string &table : reference_tables())
    {
        SCOPED_CONTEXT(table + " at the limit");
        TempDb db("ins_dangling_max_" + table);
        REQUIRE(db.schema_ok());
        clear_layers(db);
        add_bids(db, {10});
        put_layer(db, 1, "10", "A", "-1", "1");
        put_reference(db, table, 70, "10", "2147483647");
        expect_refused(db, "10", "New", 1, kNoIdentity);
    }
}

// A dangling reference of another bid's row reserves the UID as well (the reference table is read whole, like Windows); a NULL, 0 and '' do not.
TEST(LayerEditInsert, OnlyRealReferencesReserveUids)
{
    TempDb db("ins_reserve_kinds");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_reference(db, "BidZones", 1, "20", "300");   // another bid's dangling reference
    put_reference(db, "BidTexts", 1, "10", "NULL");
    put_reference(db, "BidHighlights", 1, "10", "0");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "New", 6), "301");
}

// ---------------------------------------------------------------------------------------------------------------
// Sequence: the limit and no half shift
// ---------------------------------------------------------------------------------------------------------------

// after_sequence 2147483646: the new row gets 2147483647, the limit itself: allowed (no later row exists, nothing shifts).
TEST(LayerEditInsert, TheLastSequenceIsAllowed)
{
    TempDb db("ins_seq_last");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Top", 2147483646LL), "203");
    CHECK_STR_EQ(layers_text(db, "UID = 203"), "(203,10,'Top',-1,2147483647,0,0)");
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,0);(102,1);(103,2);(104,3);(105,4);(106,5);(107,6);(203,2147483647)");
}

// after_sequence 2147483647 (new row would be 2147483648), a larger value, and the extremes of long long: refused with the limit text, nothing
// shifted, no row (no signed overflow in the check itself).
TEST(LayerEditInsert, AnAfterSequenceAtOrPastTheLimitIsRefusedBeforeAnyShift)
{
    TempDb db("ins_seq_past");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const long long after : {2147483647LL, 2147483648LL, 4000000000LL, LLONG_MAX})
    {
        SCOPED_CONTEXT("after_sequence " + std::to_string(after));
        expect_refused(db, "10", "Top", after, kSequenceLimit);
    }
    for (const long long after : {-2147483650LL, -4000000000LL, LLONG_MIN})
    {
        SCOPED_CONTEXT("after_sequence " + std::to_string(after));
        expect_refused(db, "10", "Bottom", after, kSequenceLimit);
    }
    // the smallest after_sequence whose new row (-2147483648) still fits
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Bottom", -2147483649LL), "203");
    CHECK_STR_EQ(layers_text(db, "UID = 203"), "(203,10,'Bottom',-1,-2147483648,0,0)");
}

// A later row of the bid that is ALREADY at 2147483647 would pass the limit when it is shifted: refused before the shift; so is one beyond 32 bit
// (SQLite stores it, Access would not), whatever after_sequence below it is. Nothing is written, no half shift of the rows below.
TEST(LayerEditInsert, ALaterRowAtOrPastTheLimitRefusesTheInsertBeforeAnyShift)
{
    for (const char *sequence : {"2147483647", "3000000000", "9000000000000"})
    {
        SCOPED_CONTEXT(std::string("") + sequence);
        TempDb db(std::string("ins_seq_later_") + sequence);
        REQUIRE(db.schema_ok());
        seed_m1(db);
        put_layer(db, 108, "10", "Max", "-1", sequence);
        expect_refused(db, "10", "New", 6, kSequenceLimit);
        expect_refused(db, "10", "New", 0, kSequenceLimit);
        expect_refused(db, "10", "New", 2147483646LL, kSequenceLimit);
    }
}

// The limit is per bid and per kind of row: the same value on ANOTHER bid's row, on a shared row (no bid) or as a NULL does not block bid 10's
// insert (those rows are never shifted); a later row at 2147483646 shifts to the limit and the insert works.
TEST(LayerEditInsert, RowsThatDoNotShiftNeverBlockTheInsert)
{
    TempDb db("ins_seq_not_shifted");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 209, "20", "OtherMax", "-1", "2147483647");
    put_layer(db, 9, "NULL", "SharedMax", "-1", "2147483647", "-1", "-1");
    put_layer(db, 109, "10", "NoSequence", "-1", "NULL");
    put_layer(db, 108, "10", "AlmostMax", "-1", "2147483646");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "New", 6), "210");
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "UID IN (108, 109, 209, 9, 210)"), "(9,2147483647);(108,2147483647);(109,NULL);(209,2147483647);(210,7)");
}

// ---------------------------------------------------------------------------------------------------------------
// the bid
// ---------------------------------------------------------------------------------------------------------------

// integrity::parse_uid: a bid UID is positive ASCII decimal text; "0", "-1", "abc", "", spaces, "1.0", a sign, hex and a value beyond 63 bits
// are refused with nothing written and no text.
TEST(LayerEditInsert, AMalformedBidUidIsRefused)
{
    TempDb db("ins_bad_bid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *bid : {"0", "-1", "abc", "", " 10", "10 ", "1.0", "+10", "0x10", "1e1", "99999999999999999999", "10;20", "10\n"})
    {
        SCOPED_CONTEXT(std::string("bid '") + bid + "'");
        expect_refused(db, bid, "New", 6, "");
    }
}

// "007" is 7 (the same text Windows accepts, bid_owned_identity.py:105-124): the bid "010" is bid 10.
TEST(LayerEditInsert, ALeadingZeroBidUidIsTheSameBid)
{
    TempDb db("ins_leading_zero");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "010", "New", 6), "203");
    CHECK_STR_EQ(layers_text(db, "UID = 203"), "(203,10,'New',-1,7,0,0)");
}

// A bid that has no Bids row (layer_operations.py:85 require_existing_unique_bid_owned_uid_matches(Bids)): refused, nothing written.
TEST(LayerEditInsert, ABidWithoutARowIsRefused)
{
    TempDb db("ins_missing_bid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    expect_refused(db, "99", "New", 6, "");
    expect_refused(db, "2147483647", "New", 6, "");
}

// ---------------------------------------------------------------------------------------------------------------
// the bid lock (plan 4.1, N-M1-01): re-checked INSIDE the transaction of every writer
// ---------------------------------------------------------------------------------------------------------------

TEST(LayerEditInsert, ALockedBidIsRefusedAndNothingIsWritten)
{
    TempDb db("ins_locked");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ptest::seed_locked_bid(db, 10, 5);
    const std::string before = db.snapshot_all();
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Roofing", 6), "");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// A status that no JobStatuses row names is locked (fail closed, cover_sheet.py:15-23 + project_read_service.py:81-94).
TEST(LayerEditInsert, AnUnresolvedJobStatusIsLockedToo)
{
    TempDb db("ins_unresolved");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ptest::seed_bid_with_status(db, 10, "77");
    const std::string before = db.snapshot_all();
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Roofing", 6), "");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// An open status (Locked 0) does not block; another bid being locked does not block this bid.
TEST(LayerEditInsert, AnOpenBidAndALockedOtherBidAreNotBlocked)
{
    TempDb db("ins_open");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ptest::seed_open_bid(db, 10, 4);
    ost_ptest::seed_locked_bid(db, 20, 5);
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Roofing", 6), "203");
    // and the locked bid refuses its own
    const std::string before = db.snapshot_all();
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "20", "Attic", 2), "");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// ---------------------------------------------------------------------------------------------------------------
// the name rules inside the store (the screens run them first, the store never trusts them)
// ---------------------------------------------------------------------------------------------------------------

// A blank name (after Python's strip) is refused silently (no text): "", "   ", a tab and a line break, the no-break space, an ideographic space.
TEST(LayerEditInsert, ABlankNameIsRefusedSilently)
{
    TempDb db("ins_blank");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *name : {"", " ", "   ", "\t\n", "\xC2\xA0", "\xE3\x80\x80", "\x0B\x0C"})
    {
        SCOPED_CONTEXT(std::string("blank '") + name + "'");
        expect_refused(db, "10", name, 6, "");
    }
}

// A duplicate by strip().lower() against every listed row: the bid's own layers, its locked copies, the shared template rows (layers_dialog.py:
// 293-298, :433-436); the text is "Layer {name} already exists." with the stripped typed name.
TEST(LayerEditInsert, ADuplicateNameIsRefusedWithTheWindowsText)
{
    TempDb db("ins_duplicate");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    struct Case
    {
        const char *typed;
        const char *shown;
    };
    const Case cases[] = {
        {"Walls", "Walls"},     {"walls", "walls"},           {" WALLS ", "WALLS"},      {"floors\t", "floors"},     {"Unused", "Unused"},
        {"Image", "Image"},     {" image ", "image"},         {"ANNOTATION", "ANNOTATION"}, {"default", "default"},
        {"Shared Template", "Shared Template"},               {"shared template", "shared template"},
        {"\xC2\xA0" "Walls" "\xE2\x80\x83", "Walls"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.typed);
        expect_refused(db, "10", c.typed, 6, duplicate_text(c.shown));
    }
}

// Unicode lower-casing is Python's: "ÉCOLE" is a duplicate of a layer called "école" (layer.py:9-10; the oracle rows of test_layer_edit_rules.cpp).
TEST(LayerEditInsert, ADuplicateIsFoundWithPythonsLowerCasing)
{
    TempDb db("ins_unicode_duplicate");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 108, "10", "\xC3\xA9" "cole", "-1", "8");
    expect_refused(db, "10", "\xC3\x89" "COLE", 8, duplicate_text("\xC3\x89" "COLE"));
    expect_refused(db, "10", " \xC3\x89" "cole ", 8, duplicate_text("\xC3\x89" "cole"));
    put_layer(db, 109, "10", "\xCF\x83\xCE\xB1\xCF\x82", "-1", "9"); // lower-case 'sigma alpha final-sigma'
    expect_refused(db, "10", "\xCE\xA3\xCE\x91\xCE\xA3", 9, duplicate_text("\xCE\xA3\xCE\x91\xCE\xA3")); // 'ΣΑΣ'.lower() == 'σας'
}

// LY-1: the Comments layer is always taken, in every spelling, with the duplicate text (Windows lets "Comments" through and creates an invisible,
// undeletable layer). The bid's own locked Comments copy (104) is not listed, so the store must know the rule itself.
TEST(LayerEditInsert, CommentsIsRefusedInEverySpelling)
{
    TempDb db("ins_comments");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    expect_refused(db, "10", "Comments", 6, duplicate_text("Comments"));
    expect_refused(db, "10", "comments", 6, duplicate_text("comments"));
    expect_refused(db, "10", " COMMENTS ", 6, duplicate_text("COMMENTS"));
    expect_refused(db, "10", "\xC2\xA0" "Comments" "\xE2\x80\x83", 6, duplicate_text("Comments"));
    expect_refused(db, "10", "coMMents\t", 6, duplicate_text("coMMents"));
    // a bid without its own Comments row refuses it too
    add_bids(db, {30});
    expect_refused(db, "30", "Comments", 0, duplicate_text("Comments"));
    // lookalikes are fine
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Comments 2", 6), "203");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "My Comments", 7), "204");
}

// If a bid has neither its own nor a shared row named Image, a user-created layer named "Image" becomes the Image layer (research 2.7): allowed.
TEST(LayerEditInsert, ImageIsAllowedWhenNoImageRowExists)
{
    TempDb db("ins_image_free");
    REQUIRE(db.schema_ok());
    clear_layers(db);
    add_bids(db, {10});
    put_layer(db, 5, "10", "Walls", "-1", "1");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Image", 1), "6");
}

// N-M1-13 / LY-8: Name is VARCHAR(50): 50 UTF-16 units are stored, 51 are refused with the generic failure text plus the hint; an emoji counts 2.
TEST(LayerEditInsert, TheNameLengthLimitIsFiftyUtf16Units)
{
    TempDb db("ins_length");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string hint = std::string("Failed to create layer.\n") + ost_domain::kLayerNameTooLongHint;
    expect_refused(db, "10", repeat("a", 51), 6, hint);
    expect_refused(db, "10", repeat(kEmoji, 26), 6, hint);
    expect_refused(db, "10", repeat("a", 49) + kEmoji, 6, hint);
    CHECK(contains(hint, "50"));
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", repeat("a", 50), 6), "203");
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 203"), repeat("a", 50));
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", repeat(kEmoji, 25), 7), "204");
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 204"), repeat(kEmoji, 25));
}

// LY-14: control characters, line breaks (U+2028 included), a NUL and invalid UTF-8 are refused with the generic failure text plus the hint.
TEST(LayerEditInsert, ControlCharactersAndLineBreaksAreRefused)
{
    TempDb db("ins_control");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string hint = std::string("Failed to create layer.\n") + ost_domain::kLayerNameControlHint;
    const std::string bad[] = {"A\tB", "A\nB", "A\rB", std::string("A\0B", 3), "A\x01" "B", "A\x7F" "B", "A\xC2\x85" "B", "A\xE2\x80\xA8" "B",
                               "A\xE2\x80\xA9" "B", "A\xFF" "B", "A\xED\xA0\x80" "B", "Walls2\x01"};
    int index = 0;
    for (const std::string &name : bad)
    {
        SCOPED_CONTEXT("name #" + std::to_string(index++));
        expect_refused(db, "10", name, 6, hint);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// failure points: nothing is left behind (one transaction)
// ---------------------------------------------------------------------------------------------------------------

// The shift statement fails (BEFORE UPDATE on BidLayers): no row, no shifted sequence.
TEST(LayerEditInsert, AFailedShiftLeavesNothingBehind)
{
    TempDb db("ins_fail_shift");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string before = db.snapshot_all();
    db.block("BidLayers", "UPDATE");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Ground", 4), "");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// The INSERT fails AFTER the shift worked: the shift is rolled back with it (Windows: a failing insert leaves the sequences it had already
// shifted alone, the lifecycle scenario).
TEST(LayerEditInsert, AFailedInsertRollsTheShiftBack)
{
    TempDb db("ins_fail_insert");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string before = db.snapshot_all();
    db.block("BidLayers", "INSERT");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Ground", 4), "");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// The COMMIT fails (a deferred foreign key violated at the very end): the row and the shift are gone and no UID is reported.
TEST(LayerEditInsert, AFailedCommitLeavesNothingBehind)
{
    TempDb db("ins_fail_commit");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ltest::make_commit_fail(db, "BidLayers", "INSERT");
    const std::string before = db.snapshot_all();
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Ground", 4), "");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// After a failed insert the database is usable and the same insert works (no lock or half transaction left).
TEST(LayerEditInsert, AFailedInsertLeavesTheDatabaseUsable)
{
    TempDb db("ins_after_failure");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string blocker = db.block("BidLayers", "INSERT");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Ground", 4), "");
    db.unblock(blocker);
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Ground", 4), "203");
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,0);(102,1);(103,2);(104,3);(105,4);(106,6);(107,7);(203,5)");
}

// D13: a BidLayers table without UID, BidUID or Name cannot take a layer: refused, nothing written (Windows: "Missing BidLayers.<col>").
TEST(LayerEditInsert, ATableWithoutARequiredColumnIsRefused)
{
    for (const char *column : {"UID", "BidUID", "Name"})
    {
        SCOPED_CONTEXT(std::string("") + column);
        TempDb db(std::string("ins_no_") + column);
        REQUIRE(db.schema_ok());
        add_bids(db, {10});
        ost_ltest::rebuild_layers_without(db, column);
        const std::string before = db.snapshot_all();
        CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "New", 0), "");
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
}

// A failed read: a database that cannot be opened and a file that is not a database: "", no throw, nothing written.
TEST(LayerEditInsert, AFailedReadIsARefusal)
{
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_layer(ost_ltest::unopenable_db_path(), "10", "New", 0, &error), "");
    CHECK_STR_EQ(error, "");
    const std::string garbage = ost_ltest::garbage_db_path("insert");
    error = "stale";
    CHECK_STR_EQ(ost_store::insert_layer(garbage, "10", "New", 0, &error), "");
    CHECK_STR_EQ(error, "");
    std::remove(garbage.c_str());
}
