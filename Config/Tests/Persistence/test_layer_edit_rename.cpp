// M1 layers (plan 5.3), DATA SAFETY of ost_store::rename_layer: the exact change (only Name), every refusal leaves the database byte-identical
// (snapshot_all), the ownership and guard rules (only the OPEN bid's own unlocked, non-template rows; a NULL flag is refused: fail closed), the
// name rules inside the store, the failure points, the bid lock, a failed read.
//
// Windows: layer_operations.py:385-410 update_layer_name (SQL guard IsTemplate = 0 AND IsLocked = 0, the result is "rows changed > 0"; no
// uniqueness, blank or length check: those live in the dialogs, layers_dialog.py:394-427, layers_sidebar.py:312-354, and the store re-runs them
// here); the ports are in test_layer_edit_store.cpp (:726, :752 shared half, :2017). Deliberate differences: LY-1 (Comments), LY-8 (50 units),
// LY-9 (the open bid, not "one bid"), LY-10 (a NULL flag is refused like `= 0` does, while the Windows reader shows such a row as editable),
// LY-14 (control characters). The world is seed_m1 (LayerEditTestSupport.hpp).
#include "LayerEditTestSupport.hpp"

#include <cstdio>
#include <string>
#include <vector>

using ost_ltest::add_bids;
using ost_ltest::kEmoji;
using ost_ltest::kM1Layers;
using ost_ltest::layers_text;
using ost_ltest::names_text;
using ost_ltest::put_layer;
using ost_ltest::repeat;
using ost_ltest::seed_m1;
using ost_ptest::TempDb;

namespace
{
    // A refusal: false, the expected user text, and the whole database byte-identical.
    void expect_refused(const TempDb &db, const std::string &bid, const std::string &layer, const std::string &name,
                        const std::string &expected_error)
    {
        const std::string before = db.snapshot_all();
        std::string error = "stale";
        CHECK_FALSE(ost_store::rename_layer(db.path(), bid, layer, name, &error));
        CHECK_STR_EQ(error, expected_error);
        CHECK_STR_EQ(db.snapshot_all(), before);
    }

    std::string duplicate_text(const std::string &name)
    {
        return "Layer " + name + " already exists.";
    }

    std::string replaced(std::string text, const std::string &from, const std::string &to)
    {
        const std::size_t at = text.find(from);
        REQUIRE(at != std::string::npos);
        return text.replace(at, from.size(), to);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the rows of a success
// ---------------------------------------------------------------------------------------------------------------

// layer_operations.py:396-401: only Name changes; Show, Sequence and the flags stay, no other row moves, and a rename never changes which
// conditions / annotations use the layer (they hold the UID: research 2.4).
TEST(LayerEditRename, OnlyTheNameChanges)
{
    TempDb db("ren_only_name");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    db.must("UPDATE BidLayers SET Show = 0 WHERE UID = 105");
    const std::string references = ost_ltest::references_text(db);
    std::string error = "stale";
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "Ground", &error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(layers_text(db), replaced(kM1Layers, "(105,10,'Walls',-1,4,0,0)", "(105,10,'Ground',0,4,0,0)"));
    CHECK_STR_EQ(ost_ltest::references_text(db), references);
}

// The name is stored stripped (layers_dialog.py:397 `new_name = item.text(2).strip()`).
TEST(LayerEditRename, TheNameIsStoredStripped)
{
    TempDb db("ren_stripped");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "  Roofing \t"));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 105"), "Roofing");
    CHECK(ost_store::rename_layer(db.path(), "10", "106", "\xC2\xA0" "Two  Words" "\xE2\x80\x83"));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 106"), "Two  Words");
}

// layers_dialog.py:401-402: a name equal to the stored one (exact, after the strip) is a success that writes nothing. The first rename is real,
// so the unchanged one is judged against a changed name.
TEST(LayerEditRename, TheSameNameIsASuccessThatWritesNothing)
{
    TempDb db("ren_unchanged");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "Ground"));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 105"), "Ground");
    const std::string before = db.snapshot_all();
    std::string error = "stale";
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "Ground", &error));
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "  Ground  ", &error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// A change of letter case alone IS a change (the exact compare, and the layer's own row never counts as a duplicate): written.
TEST(LayerEditRename, ACaseOnlyChangeIsWritten)
{
    TempDb db("ren_case");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "WALLS"));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 105"), "WALLS");
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "walls"));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 105"), "walls");
}

// A stored name with surrounding spaces ("Walls ") differs from the stripped typed name, so it is rewritten stripped.
TEST(LayerEditRename, AStoredNameWithSpacesIsRewrittenStripped)
{
    TempDb db("ren_stored_spaces");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    db.must("UPDATE BidLayers SET Name = 'Walls ' WHERE UID = 105");
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "Walls"));
    CHECK_STR_EQ(db.text("SELECT '[' || Name || ']' FROM BidLayers WHERE UID = 105"), "[Walls]");
}

// Quotes and SQL text are bound, never executed.
TEST(LayerEditRename, QuotesAndSqlTextAreBoundNotExecuted)
{
    TempDb db("ren_injection");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string name = "O'Brien \"A\" \xC3\xA9; DROP TABLE BidLayers;--";
    CHECK(ost_store::rename_layer(db.path(), "10", "105", name));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 105"), name);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers"), 14);
}

// A name only ANOTHER bid uses is fine.
TEST(LayerEditRename, ANameUsedOnlyByAnotherBidIsAllowed)
{
    TempDb db("ren_other_bid_name");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "Other"));
    CHECK(ost_store::rename_layer(db.path(), "10", "106", "zebra"));
    CHECK_STR_EQ(names_text(db, "UID IN (105, 106)"), "(105,'Other');(106,'zebra')");
}

// ---------------------------------------------------------------------------------------------------------------
// ownership and guards
// ---------------------------------------------------------------------------------------------------------------

// layer_operations.py:395 + bid_owned_identity.py:205-235: an unknown uid; and the uid syntax of parse_uid: refused, nothing written, no text.
TEST(LayerEditRename, AnUnknownOrMalformedLayerUidIsRefused)
{
    TempDb db("ren_bad_uid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *uid : {"999", "0", "-5", "abc", "", " 105", "105 ", "1.0", "+105", "0x69", "99999999999999999999", "105;106"})
    {
        SCOPED_CONTEXT(std::string("uid '") + uid + "'");
        expect_refused(db, "10", uid, "Roofing", "");
    }
}

// "0105" is 105 (bid_owned_identity.py:105-124 accepts leading zeros, and iOS canonicalises the number: register LY-13).
TEST(LayerEditRename, ALeadingZeroUidIsTheSameLayer)
{
    TempDb db("ren_leading_zero");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::rename_layer(db.path(), "10", "0105", "Roofing"));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 105"), "Roofing");
    CHECK(ost_store::rename_layer(db.path(), "010", "105", "Roofing 2"));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 105"), "Roofing 2");
}

// LY-9 (Windows checks "one bid", not the open one): a layer of ANOTHER bid is refused for the open bid; the bid that owns it can rename it.
TEST(LayerEditRename, ALayerOfAnotherBidIsRefused)
{
    TempDb db("ren_foreign");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    expect_refused(db, "10", "201", "Roofing", "");
    expect_refused(db, "20", "105", "Roofing", "");
    CHECK(ost_store::rename_layer(db.path(), "20", "201", "Roofing"));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 201"), "Roofing");
}

// A shared template row (no bid: the rows 1-4 of the schema and 'Shared Template') is read only: layer_operations.py:395 raises
// IncoherentBidOwnedScopeError on it; the store refuses.
TEST(LayerEditRename, ASharedTemplateRowIsRefused)
{
    TempDb db("ren_shared");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *uid : {"1", "2", "3", "4", "8"})
    {
        SCOPED_CONTEXT(std::string("shared row ") + uid);
        expect_refused(db, "10", uid, "Roofing", "");
    }
}

// layer_operations.py:389-393 the SQL guard IsLocked = 0: the bid's own locked copies (Image, Annotation, Default, Comments) are refused,
// BEFORE the name rules (even with their own name, even with a duplicate or reserved one).
TEST(LayerEditRename, TheBidsOwnLockedCopiesAreRefusedBeforeTheNameRules)
{
    TempDb db("ren_locked_copies");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *uid : {"101", "102", "103", "104"})
    {
        SCOPED_CONTEXT(std::string("locked copy ") + uid);
        expect_refused(db, "10", uid, "Roofing", "");
        expect_refused(db, "10", uid, "Walls", "");     // a duplicate would be the text if the guard came later
        expect_refused(db, "10", uid, "Comments", "");  // so would a reserved one
    }
    expect_refused(db, "10", "101", "Image", "");        // its own name: not "unchanged", refused
}

// LY-10 (bid_data_reader.py:657-668 reads a NULL flag as 0, the writer guard `= 0` does not match NULL): a NULL IsTemplate / IsLocked, and
// every nonzero flag, refuse the rename. Fail closed.
TEST(LayerEditRename, NullAndNonzeroFlagsAreRefused)
{
    TempDb db("ren_flags");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 110, "10", "NullTemplate", "-1", "8", "NULL", "0");
    put_layer(db, 111, "10", "NullLocked", "-1", "9", "0", "NULL");
    put_layer(db, 112, "10", "NullBoth", "-1", "10", "NULL", "NULL");
    put_layer(db, 113, "10", "OwnTemplate", "-1", "11", "-1", "0");
    put_layer(db, 114, "10", "PositiveLocked", "-1", "12", "0", "1");
    put_layer(db, 115, "10", "PositiveTemplate", "-1", "13", "1", "0");
    for (const char *uid : {"110", "111", "112", "113", "114", "115"})
    {
        SCOPED_CONTEXT(std::string("row ") + uid);
        expect_refused(db, "10", uid, "Roofing", "");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the name rules inside the store
// ---------------------------------------------------------------------------------------------------------------

TEST(LayerEditRename, ABlankNameIsRefusedSilently)
{
    TempDb db("ren_blank");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *name : {"", " ", "   ", "\t\n", "\xC2\xA0", "\xE3\x80\x80"})
    {
        SCOPED_CONTEXT(std::string("blank '") + name + "'");
        expect_refused(db, "10", "105", name, "");
    }
}

// A duplicate by strip().lower() against the other listed rows (the row itself is excluded): the Windows text with the stripped name.
TEST(LayerEditRename, ADuplicateNameIsRefusedWithTheWindowsText)
{
    TempDb db("ren_duplicate");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    struct Case
    {
        const char *typed;
        const char *shown;
    };
    const Case cases[] = {
        {"floors", "floors"}, {" UNUSED ", "UNUSED"},   {"Image", "Image"},     {"ANNOTATION", "ANNOTATION"}, {"default", "default"},
        {"Shared Template", "Shared Template"},         {"shared template", "shared template"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.typed);
        expect_refused(db, "10", "105", c.typed, duplicate_text(c.shown));
    }
}

// LY-1: "Comments" in every spelling is refused with the duplicate text (the hidden Comments row is not listed, the rule is the store's own).
TEST(LayerEditRename, CommentsIsRefusedInEverySpelling)
{
    TempDb db("ren_comments");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    expect_refused(db, "10", "105", "Comments", duplicate_text("Comments"));
    expect_refused(db, "10", "105", " comments ", duplicate_text("comments"));
    expect_refused(db, "10", "105", "COMMENTS", duplicate_text("COMMENTS"));
    expect_refused(db, "10", "105", "\xC2\xA0" "Comments" "\xE2\x80\x83", duplicate_text("Comments"));
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "Comments 2"));
    CHECK(ost_store::rename_layer(db.path(), "10", "106", "My Comments"));
}

TEST(LayerEditRename, TheNameLengthLimitIsFiftyUtf16Units)
{
    TempDb db("ren_length");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string hint = std::string("Failed to rename layer.\n") + ost_domain::kLayerNameTooLongHint;
    expect_refused(db, "10", "105", repeat("a", 51), hint);
    expect_refused(db, "10", "105", repeat(kEmoji, 26), hint);
    expect_refused(db, "10", "105", repeat("a", 49) + kEmoji, hint);
    CHECK(ost_store::rename_layer(db.path(), "10", "105", repeat("a", 50)));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 105"), repeat("a", 50));
    CHECK(ost_store::rename_layer(db.path(), "10", "106", repeat(kEmoji, 25)));
    CHECK_STR_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 106"), repeat(kEmoji, 25));
}

TEST(LayerEditRename, ControlCharactersAndLineBreaksAreRefused)
{
    TempDb db("ren_control");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string hint = std::string("Failed to rename layer.\n") + ost_domain::kLayerNameControlHint;
    const std::string bad[] = {"A\tB", "A\nB", "A\rB", std::string("A\0B", 3), "A\x01" "B", "A\x7F" "B", "A\xC2\x85" "B", "A\xE2\x80\xA8" "B",
                               "A\xE2\x80\xA9" "B", "A\xFF" "B", "A\xED\xA0\x80" "B", "Walls2\x01"};
    int index = 0;
    for (const std::string &name : bad)
    {
        SCOPED_CONTEXT("name #" + std::to_string(index++));
        expect_refused(db, "10", "105", name, hint);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the bid
// ---------------------------------------------------------------------------------------------------------------

TEST(LayerEditRename, AMalformedOrMissingBidIsRefused)
{
    TempDb db("ren_bad_bid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *bid : {"0", "-10", "abc", "", " 10", "10 ", "1.0", "99", "99999999999999999999"})
    {
        SCOPED_CONTEXT(std::string("bid '") + bid + "'");
        expect_refused(db, bid, "105", "Roofing", "");
    }
}

// plan 4.1 / N-M1-01: the lock is re-checked inside the transaction (a locked bid, an unresolved status), and it is the bid's own.
TEST(LayerEditRename, ALockedBidIsRefused)
{
    TempDb db("ren_locked");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ptest::seed_locked_bid(db, 10, 5);
    expect_refused(db, "10", "105", "Roofing", "");
    expect_refused(db, "10", "105", "Walls", ""); // even an unchanged name: the lock comes first
    // an unresolved status is locked as well (fail closed)
    TempDb db2("ren_unresolved");
    REQUIRE(db2.schema_ok());
    seed_m1(db2);
    ost_ptest::seed_bid_with_status(db2, 10, "77");
    expect_refused(db2, "10", "105", "Roofing", "");
}

TEST(LayerEditRename, AnOpenBidAndALockedOtherBidAreNotBlocked)
{
    TempDb db("ren_open");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ptest::seed_open_bid(db, 10, 4);
    ost_ptest::seed_locked_bid(db, 20, 5);
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "Roofing"));
    expect_refused(db, "20", "201", "Attic", "");
}

// ---------------------------------------------------------------------------------------------------------------
// failure points
// ---------------------------------------------------------------------------------------------------------------

TEST(LayerEditRename, AFailedUpdateLeavesTheNameAlone)
{
    TempDb db("ren_fail_update");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string before = db.snapshot_all();
    const std::string blocker = db.block("BidLayers", "UPDATE");
    CHECK_FALSE(ost_store::rename_layer(db.path(), "10", "105", "Roofing"));
    CHECK_STR_EQ(db.snapshot_all(), before);
    db.unblock(blocker);
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "Roofing")); // and the database is usable again
}

TEST(LayerEditRename, AFailedCommitLeavesTheNameAlone)
{
    TempDb db("ren_fail_commit");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ltest::make_commit_fail(db, "BidLayers", "UPDATE");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::rename_layer(db.path(), "10", "105", "Roofing"));
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// D13: a table without UID or Name cannot be renamed in: refused, nothing written.
TEST(LayerEditRename, ATableWithoutARequiredColumnIsRefused)
{
    for (const char *column : {"UID", "Name"})
    {
        SCOPED_CONTEXT(std::string("") + column);
        TempDb db(std::string("ren_no_") + column);
        REQUIRE(db.schema_ok());
        add_bids(db, {10});
        ost_ltest::rebuild_layers_without(db, column);
        const std::string before = db.snapshot_all();
        CHECK_FALSE(ost_store::rename_layer(db.path(), "10", "105", "Roofing"));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
}

TEST(LayerEditRename, AFailedReadIsARefusal)
{
    std::string error = "stale";
    CHECK_FALSE(ost_store::rename_layer(ost_ltest::unopenable_db_path(), "10", "105", "Roofing", &error));
    CHECK_STR_EQ(error, "");
    const std::string garbage = ost_ltest::garbage_db_path("rename");
    error = "stale";
    CHECK_FALSE(ost_store::rename_layer(garbage, "10", "105", "Roofing", &error));
    CHECK_STR_EQ(error, "");
    std::remove(garbage.c_str());
}
