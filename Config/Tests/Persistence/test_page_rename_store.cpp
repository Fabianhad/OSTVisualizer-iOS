// Rename Page, the store writer ost_store::rename_page (Core/Persistence/ProjectStore/Components/ProjectStore+PageRename.cpp; plan 4.4,
// 5.5; register RP-1 .. RP-9; decisions N-M1-01, N-M1-13, N-M1-24).
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/infrastructure/mdb/components/page_operations.py:130-152   save_page_name (one UPDATE after the ownership check)
//   ost_visualizer/infrastructure/database/bid_owned_identity.py:105-124, :144-152, :205-235   uid and ownership refusals
//   ost_visualizer/infrastructure/mdb/mdb_writer.py:276-302               _execute_update_values (ignores the row count: RP-8)
//   ost_visualizer/application/services/active_bid_write_guard.py:21-36    the locked-bid refusal (plan 4.1)
// Ported tests cite their Windows test file:line. The data-safety cases come from the plan (5.5 "Tests-first plan (M1)" and the research
// map section 4) and say so.
//
// iOS is deliberately STRICTER than the Windows writer, and every such test says so:
//   RP-7  the call has a bid and the page must belong to it (Windows: any page with one existing bid);
//   RP-8  exactly one row must change, else rollback (Windows: True whatever the row count);
//   RP-2 / N-M1-13  a name over 255 UTF-16 units, with U+0000 or invalid UTF-8, or not stripped, is refused by the store as well as by the
//         rule (the store never trusts the screen);
//   plan 4.1  a locked bid writes nothing (an unresolved status and a failed read count as locked).
//
// Every refusal test ends with a CONTROL: the same database accepts a valid rename, so a function that refuses everything cannot pass.
#include "StoreTestSupport.hpp"

#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore_PageRename.hpp"

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using ost_ptest::quoted;
using ost_ptest::TempDb;

namespace
{
    const std::string kEmoji = "\xF0\x9F\x98\x80"; // U+1F600: 2 UTF-16 units

    std::string repeat(const std::string &unit, std::size_t count)
    {
        std::string out;
        out.reserve(unit.size() * count);
        for (std::size_t i = 0; i < count; ++i)
            out += unit;
        return out;
    }

    // The stored Name of a page ("<none>" when there is no row).
    std::string page_name(const TempDb &db, const std::string &uid)
    {
        return db.text("SELECT Name FROM BidPages WHERE UID = " + uid);
    }

    // "UID=Name" of every page, by UID: a compact before / after of the whole page table.
    std::string page_names(const TempDb &db)
    {
        return ost_ptest::list(db, "SELECT UID || '=' || ifnull(Name, 'NULL') FROM BidPages ORDER BY UID");
    }

    // A refused call: false, the WHOLE database byte-identical afterwards, and a reason in `error` (cleared and then set by the call).
    void expect_refused(const TempDb &db, const std::string &bid, const std::string &page, const std::string &name)
    {
        const std::string before = db.snapshot_all();
        std::string error = "stale";
        CHECK_FALSE(ost_store::rename_page(db.path(), bid, page, name, &error));
        CHECK_EQ(db.snapshot_all(), before);
        CHECK(!error.empty());
        CHECK(error != "stale");
    }

    // A successful call: true, `error` empty, the name stored exactly as given.
    void expect_renamed(const TempDb &db, const std::string &bid, const std::string &page, const std::string &name,
                        const std::string &stored_uid)
    {
        std::string error = "stale";
        CHECK(ost_store::rename_page(db.path(), bid, page, name, &error));
        CHECK_EQ(error, std::string());
        CHECK_EQ(page_name(db, stored_uid), name);
    }

    // The fixture of tests/infrastructure/mdb/components/test_page_operations.py:1214-1260 (PageOperationsRealContractTests.database): bids
    // 1 and 2, pages 20 'Sheet A' and 21 'Sheet B' of bid 1, page 30 'Other bid' of bid 2, every page with the production column values of
    // _page_row (42 x 30, scale 0.125 / 12, 'plan.pdf', ...). The iOS schema has all those columns.
    void seed_windows_pages(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        ost_ptest::ensure_project_two(db);
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 2, 'one', 1)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 2, 'two', 2)");
        const struct
        {
            int uid;
            int bid;
            const char *name;
        } pages[] = {{20, 1, "Sheet A"}, {21, 1, "Sheet B"}, {30, 2, "Other bid"}};
        for (const auto &page : pages)
            db.must("INSERT INTO BidPages (UID, BidUID, Name, Width, Height, ScaleFactor1, ScaleFactor2, ImagePath, OverlayImagePath, "
                    "OverlayRect, OverlayOffsetX, OverlayOffsetY, OverlayRotation, OverlayResized, DeskewRotationOverlay, Show, ZoomFac, "
                    "CurrentX, CurrentY, Rotation, FlipX, FlipY, Invert, Bitonal) VALUES (" +
                    std::to_string(page.uid) + ", " + std::to_string(page.bid) + ", " + quoted(page.name) +
                    ", 42.0, 30.0, 0.125, 12.0, 'plan.pdf', '', '', 0.0, 0.0, 0.0, 0, 0.0, 0, 1.0, 5.0, 6.0, 0, 0, 0, 0, 0)");
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// Ported from the Windows tests
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/infrastructure/mdb/components/test_page_operations.py:1260 test_page_name_is_saved_for_the_requested_page_only: pages 20
// 'Sheet A', 21 'Sheet B' (bid 1), 30 'Other bid' (bid 2); save_page_name('x.mdb', '20', 'Renamed') is True and the names become
// Renamed / Sheet B / Other bid. (The iOS call also names the bid, '1'.) Nothing else of the three rows changes: restoring the name
// gives the byte-identical database.
TEST(PageRenameStore, Win1260_OnlyTheRequestedPageIsRenamed)
{
    TempDb db("rename_win1260");
    seed_windows_pages(db);
    const std::string before = db.snapshot_all();
    expect_renamed(db, "1", "20", "Renamed", "20");
    CHECK_EQ(page_names(db), std::string("20=Renamed,21=Sheet B,30=Other bid"));
    db.must("UPDATE BidPages SET Name = 'Sheet A' WHERE UID = 20");
    CHECK_EQ(db.snapshot_all(), before);
}

// Port of test_page_operations.py:1269 test_page_name_without_a_name_column_fails_and_is_logged: Bids(UID) with row 1, BidPages(UID, BidUID)
// WITHOUT a Name column with page 20 of bid 1: save_page_name('x.mdb', '20', 'N') is False (Windows logs "Missing BidPages.Name"; that text
// is Windows-specific). On iOS these hand-built tables also lack Bids.JobStatusUID, so the lock read fails and refuses too (fail closed):
// the next test isolates the missing Name column on the full schema. Control: with both columns added the same call renames.
TEST(PageRenameStore, Win1269_BidPagesWithoutANameColumnFails)
{
    TempDb db("rename_win1269", false);
    db.must("CREATE TABLE Bids (UID INTEGER)");
    db.must("INSERT INTO Bids VALUES (1)");
    db.must("CREATE TABLE BidPages (UID INTEGER, BidUID INTEGER)");
    db.must("INSERT INTO BidPages VALUES (20, 1)");
    const std::string before = db.snapshot_all();
    std::string error = "stale";
    CHECK_FALSE(ost_store::rename_page(db.path(), "1", "20", "N", &error));
    CHECK_EQ(db.snapshot_all(), before);
    CHECK(!error.empty());
    CHECK(error != "stale");
    // control: the columns the writer needs exist
    db.must("ALTER TABLE BidPages ADD COLUMN Name TEXT");
    db.must("ALTER TABLE Bids ADD COLUMN JobStatusUID");
    expect_renamed(db, "1", "20", "N", "20");
}

// The same case on the full schema (research section 4 case 16): BidPages loses its Name column, everything else is as the app creates it
// (so the lock read works and the refusal can only be the missing column). Refused, nothing written; control: the column is back.
TEST(PageRenameStore, MissingNameColumn_OnTheFullSchemaIsRefused)
{
    TempDb db("rename_no_name_column");
    ost_ptest::seed_two_bids(db);
    db.must("ALTER TABLE BidPages DROP COLUMN Name");
    expect_refused(db, "10", "100", "Renamed");
    db.must("ALTER TABLE BidPages ADD COLUMN Name TEXT");
    expect_renamed(db, "10", "100", "Renamed", "100");
}

// Port of test_page_operations.py:1401 test_every_page_save_logs_its_own_failure_and_changes_nothing, the save_page_name row:
// save_page_name('x.mdb', '99', 'N') with no page 99 is False ("BidPages has no row for UID 99") and the BidPages rows are unchanged.
TEST(PageRenameStore, Win1401_AnUnknownPageIsRefusedAndChangesNothing)
{
    TempDb db("rename_win1401");
    seed_windows_pages(db);
    const std::string rows = db.dump("BidPages");
    expect_refused(db, "1", "99", "N");
    CHECK_EQ(db.dump("BidPages"), rows);
    expect_renamed(db, "1", "20", "N", "20"); // control
}

// Port of test_page_operations.py:1500 test_page_saves_reject_malformed_page_uids_before_touching_rows: 'abc', '0', '-5' and '' are
// refused (Windows: MalformedBidOwnedUidError, bid_owned_identity.py:105-124) and the rows are unchanged.
TEST(PageRenameStore, Win1500_MalformedPageUidsAreRefusedBeforeTouchingRows)
{
    TempDb db("rename_win1500");
    seed_windows_pages(db);
    const std::string rows = db.dump("BidPages");
    for (const char *bad : {"abc", "0", "-5", ""})
    {
        SCOPED_CONTEXT(std::string("page uid '") + bad + "'");
        expect_refused(db, "1", bad, "N");
        CHECK_EQ(db.dump("BidPages"), rows);
    }
    expect_renamed(db, "1", "20", "N", "20"); // control
}

// Port of tests/integration/mdb/test_ownership_and_atomicity.py:448 test_orphan_page_edit_and_delete_reject_before_mutation (the rename
// half): a page 7 with BidUID 99 and NO Bids row 99: save_page_name('malformed.mdb', '7', 'Changed') is False ("Bids has no row for UID
// 99") and the name stays 'Orphan page'; control: after INSERT INTO Bids VALUES (99) the rename succeeds ('Changed'). iOS names the bid:
// with bid 99 (no such bid) and with an existing bid (not the page's) it is refused.
TEST(PageRenameStore, Win448_AnOrphanPageIsRefusedUntilItsBidExists)
{
    TempDb db("rename_win448");
    ost_ptest::seed_two_bids(db);
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (7, 99, 'Orphan page')");
    expect_refused(db, "99", "7", "Changed");
    expect_refused(db, "10", "7", "Changed"); // an existing bid is not the owner of the page
    CHECK_EQ(page_name(db, "7"), std::string("Orphan page"));
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (99, 2, 'Job 99', 99)");
    expect_renamed(db, "99", "7", "Changed", "7");
}

// ---------------------------------------------------------------------------------------------------------------
// Uids (research section 4 cases 9 to 13)
// ---------------------------------------------------------------------------------------------------------------

// Case 9: every text that is not a positive ASCII decimal is refused although a lenient parser (stoll, strtol, atoi) would find a page:
// pages 1, 7 and 16 exist in bid 10. ' 7', '7 ', '1.0', '+7', '0x10' (16), '99999999999999999999' (beyond 63 bits) and
// '9223372036854775808' (2^63) are malformed (integrity::parse_uid); 'abc', '0', '-5', '' as in the Windows test.
TEST(PageRenameStore, Uids_MalformedPageUidsAreRefusedEvenWhenALenientParserWouldFindAPage)
{
    TempDb db("rename_uids_bad");
    ost_ptest::seed_two_bids(db);
    for (const char *uid : {"1", "7", "16"})
        db.must(std::string("INSERT INTO BidPages (UID, BidUID, Name) VALUES (") + uid + ", 10, 'Page " + uid + "')");
    const std::vector<std::string> bad = {"abc", "0", "-5", "", " 7", "7 ", "1.0", "+7", "0x10", "99999999999999999999",
                                          "9223372036854775808", "7a", "-0", "1e0", "\t7", "7\n", std::string("7\0", 2), "\xEF\xBC\x97"};
    for (std::size_t i = 0; i < bad.size(); ++i)
    {
        SCOPED_CONTEXT("page uid #" + std::to_string(i) + " '" + bad[i] + "'");
        expect_refused(db, "10", bad[i], "Renamed");
    }
    expect_renamed(db, "10", "7", "Renamed", "7"); // control
}

// Case 9 (second half): Windows int() accepts leading zeros, so '007' is page 7 (and bid '010' is bid 10).
TEST(PageRenameStore, Uids_LeadingZerosNameTheSamePageAndBid)
{
    TempDb db("rename_uids_zeros");
    ost_ptest::seed_two_bids(db);
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (7, 10, 'Seven')");
    expect_renamed(db, "10", "007", "By zeros", "7");
    expect_renamed(db, "010", "7", "By zeros bid", "7");
    CHECK_EQ(page_name(db, "100"), std::string("P100")); // no other page moved
}

// Case 10: a bid uid that is malformed ('abc', '0', '', '-1', ' 10', '10 ', '1e1', '+10') or names no bid (99; 30 is a condition uid, 60 an
// area uid, 70 a layer uid) refuses the rename of an existing page.
TEST(PageRenameStore, BidUid_MalformedOrUnknownBidsAreRefused)
{
    TempDb db("rename_bid_bad");
    ost_ptest::seed_two_bids(db);
    for (const char *bid : {"abc", "0", "", "-1", " 10", "10 ", "1e1", "+10", "99", "30", "60", "70", "9223372036854775808", "1.0"})
    {
        SCOPED_CONTEXT(std::string("bid uid '") + bid + "'");
        expect_refused(db, bid, "100", "Renamed");
    }
    expect_renamed(db, "10", "100", "Renamed", "100"); // control
}

// Case 11 (RP-7, decision D8): page 200 belongs to bid 20. Called with bid 10 it is refused and bid 20's page is unchanged (stricter than the
// Windows writer, which has no bid argument and would rename it); the same call with bid 20 works; page 100 with bid 20 is refused.
TEST(PageRenameStore, ForeignBid_APageOfAnotherBidIsRefused)
{
    TempDb db("rename_foreign_bid");
    ost_ptest::seed_two_bids(db);
    expect_refused(db, "10", "200", "Stolen");
    CHECK_EQ(page_name(db, "200"), std::string("P200"));
    expect_refused(db, "20", "100", "Stolen");
    CHECK_EQ(page_name(db, "100"), std::string("P100"));
    expect_renamed(db, "20", "200", "Mine", "200");
    CHECK_EQ(page_name(db, "100"), std::string("P100"));
    CHECK_EQ(page_name(db, "101"), std::string("P101"));
}

// Case 12 (second half): a page with a NULL BidUID has no owner: refused for every bid (Windows: IncoherentBidOwnedScopeError).
TEST(PageRenameStore, NullBidUid_APageWithoutABidIsRefused)
{
    TempDb db("rename_null_bid");
    ost_ptest::seed_two_bids(db);
    db.must("INSERT INTO BidPages (UID, Name) VALUES (8, 'No bid')");
    expect_refused(db, "10", "8", "Renamed");
    expect_refused(db, "20", "8", "Renamed");
    expect_refused(db, "0", "8", "Renamed");
    expect_renamed(db, "10", "100", "Renamed", "100"); // control
}

// Case 13: a page UID of 2147483647 (the largest Access COUNTER) is renamed (UIDs are read as 64 bit); nothing is allocated, so the
// condition numbers (RefNo) and every other row are untouched.
TEST(PageRenameStore, Limits_PageUid2147483647IsRenamed)
{
    TempDb db("rename_limit_int32");
    ost_ptest::seed_two_bids(db);
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (2147483647, 10, 'Last Access uid')");
    const std::string refnos = ost_ptest::list(db, "SELECT UID || ':' || RefNo FROM BidConditions ORDER BY UID");
    expect_renamed(db, "10", "2147483647", "Renamed", "2147483647");
    CHECK_EQ(ost_ptest::list(db, "SELECT UID || ':' || RefNo FROM BidConditions ORDER BY UID"), refnos);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 4);
}

// Case 13: UID 2147483648 exists in SQLite (INTEGER PRIMARY KEY; impossible in Access): documented, iOS accepts what the schema accepts
// and renames it; 2^63 - 1 is the largest uid that parse_uid takes (a row with it is renamed), 2^63 is malformed.
TEST(PageRenameStore, Limits_UidsAbove2147483647AreAcceptedUpTo2Pow63Minus1)
{
    TempDb db("rename_limit_int64");
    ost_ptest::seed_two_bids(db);
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (2147483648, 10, 'Above int32')");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (9223372036854775807, 10, 'Max int64')");
    expect_refused(db, "10", "9223372036854775806", "No row"); // a uid with no row
    expect_refused(db, "10", "9223372036854775808", "Malformed");
    expect_renamed(db, "10", "2147483648", "Renamed 2^31", "2147483648");
    expect_renamed(db, "10", "9223372036854775807", "Renamed 2^63-1", "9223372036854775807");
    // a bid uid above int32 that exists works the same way
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (4294967296, 2, 'Big bid', 5)");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (500, 4294967296, 'In the big bid')");
    expect_renamed(db, "4294967296", "500", "Renamed big", "500");
    expect_refused(db, "10", "500", "Foreign");
}

// ---------------------------------------------------------------------------------------------------------------
// Atomicity: rollback, exactly one row, busy, duplicate rows (cases 14 to 18)
// ---------------------------------------------------------------------------------------------------------------

// Case 14 (test_local_family_ownership.py:215 test_failed_rename_preserves_all_metadata, store half: the writer fails): a statement that
// fails (a trigger that aborts every UPDATE of BidPages) gives false and a byte-identical database; after the trigger is dropped the same
// call works.
TEST(PageRenameStore, Rollback_ABlockedUpdateLeavesTheDatabaseIdentical)
{
    TempDb db("rename_rollback");
    ost_ptest::seed_two_bids(db);
    const std::string trigger = db.block("BidPages", "UPDATE");
    expect_refused(db, "10", "100", "Renamed");
    CHECK_EQ(page_name(db, "100"), std::string("P100"));
    db.unblock(trigger);
    expect_renamed(db, "10", "100", "Renamed", "100");
}

// Case 14 (second half): a trigger that aborts only for ONE page: that page is refused and untouched, its sibling (same bid) and a page of
// the other bid are renamed.
TEST(PageRenameStore, Rollback_ATriggerForOnePageKeepsTheOthersRenameable)
{
    TempDb db("rename_rollback_one");
    ost_ptest::seed_two_bids(db);
    db.must("CREATE TRIGGER only100 BEFORE UPDATE ON BidPages WHEN OLD.UID = 100 BEGIN SELECT RAISE(ABORT, 'blocked'); END");
    expect_refused(db, "10", "100", "Renamed");
    expect_renamed(db, "10", "101", "Sibling", "101");
    expect_renamed(db, "20", "200", "Other bid", "200");
    CHECK_EQ(page_names(db), std::string("100=P100,101=Sibling,200=Other bid"));
}

// Case 15: another connection holds the database in a write transaction (a busy database): the rename gives up (false, not a crash) after
// the busy timeout (5 s), nothing is written; when the other writer is done the same call works.
TEST(PageRenameStore, Busy_AnotherWriterHoldsTheDatabase)
{
    TempDb db("rename_busy");
    ost_ptest::seed_two_bids(db);
    ost_db::OstDatabase other;
    REQUIRE(other.open(db.path()));
    REQUIRE(other.begin_transaction());
    {
        std::string error = "stale";
        CHECK_FALSE(ost_store::rename_page(db.path(), "10", "100", "Renamed", &error));
        CHECK(!error.empty());
        CHECK(error != "stale");
    }
    REQUIRE(other.rollback());
    CHECK_EQ(page_name(db, "100"), std::string("P100"));
    expect_renamed(db, "10", "100", "Renamed", "100");
}

// Case 17 (Windows DuplicateBidOwnedUidError at the scope check): two BidPages rows with the same UID (impossible on the iOS schema, where
// UID is the primary key; only a hand-built table can hold them, as uid_list_problem reads them) are refused rather than both renamed;
// both names stay.
TEST(PageRenameStore, DuplicateRows_TwoRowsWithTheSameUidAreRefused)
{
    TempDb db("rename_duplicate_rows");
    ost_ptest::seed_two_bids(db);
    db.must("DROP TABLE BidPages");
    db.must("CREATE TABLE BidPages (UID INTEGER, BidUID INTEGER, Name TEXT)");
    db.must("INSERT INTO BidPages VALUES (5, 10, 'First')");
    db.must("INSERT INTO BidPages VALUES (5, 10, 'Second')");
    db.must("INSERT INTO BidPages VALUES (6, 10, 'Single')");
    expect_refused(db, "10", "5", "Renamed");
    CHECK_EQ(ost_ptest::list(db, "SELECT Name FROM BidPages ORDER BY rowid"), std::string("First,Second,Single"));
    expect_renamed(db, "10", "6", "Renamed", "6"); // control: a unique uid on the same table
}

// Case 17, second half (Windows DuplicateBidOwnedUidError, bid_owned_identity.py:205-235, is raised for EVERY owner of the uid): the same
// UID in TWO bids is refused for both of them. Before the uniqueness check the call renamed the row of the bid that was asked for, because
// row_belongs_to_bid reads one row only; it must refuse for each bid and leave both rows alone. Only a hand-built table can hold this (the
// iOS schema has UID as primary key).
TEST(PageRenameStore, DuplicateRows_TheSameUidInTwoBidsIsRefusedForBoth)
{
    TempDb db("rename_duplicate_two_bids");
    ost_ptest::seed_two_bids(db);
    db.must("DROP TABLE BidPages");
    db.must("CREATE TABLE BidPages (UID INTEGER, BidUID INTEGER, Name TEXT)");
    db.must("INSERT INTO BidPages VALUES (5, 10, 'First in bid 10')");
    db.must("INSERT INTO BidPages VALUES (5, 20, 'Second in bid 20')");
    db.must("INSERT INTO BidPages VALUES (6, 10, 'Single')");
    expect_refused(db, "10", "5", "Renamed");
    expect_refused(db, "20", "5", "Renamed");
    CHECK_EQ(ost_ptest::list(db, "SELECT Name FROM BidPages ORDER BY rowid"), std::string("First in bid 10,Second in bid 20,Single"));
    expect_renamed(db, "10", "6", "Renamed", "6"); // control: a unique uid on the same table
}

// Case 18 (RP-8, rename_page of the plan: "exactly one row"): the UPDATE changes ZERO rows because a trigger swallows it (RAISE(IGNORE)): a
// page that vanished between the check and the write. Windows returns True (the row count is not examined, mdb_writer.py:298-302); iOS
// refuses and rolls back. (TempDb::block uses RAISE(ABORT), so the zero-row path needs its own trigger.) Control: the trigger watches
// page 100 only, page 101 is renamed.
TEST(PageRenameStore, ExactlyOneRow_AnUpdateThatChangesNothingIsRefused)
{
    TempDb db("rename_zero_rows");
    ost_ptest::seed_two_bids(db);
    db.must("CREATE TRIGGER swallow100 BEFORE UPDATE ON BidPages WHEN OLD.UID = 100 BEGIN SELECT RAISE(IGNORE); END");
    expect_refused(db, "10", "100", "Renamed");
    CHECK_EQ(page_name(db, "100"), std::string("P100"));
    expect_renamed(db, "10", "101", "Renamed", "101");
}

// Case 18 (stronger): the trigger also DELETES the row before it swallows the UPDATE (the page "deleted by another client"): the refusal
// must roll the delete back, so the page and everything else are byte-identical afterwards (a function that only reports false but commits
// would lose the page).
TEST(PageRenameStore, ExactlyOneRow_ARollbackBringsBackAPageDeletedInTheMeantime)
{
    TempDb db("rename_zero_rows_delete");
    ost_ptest::seed_two_bids(db);
    db.must("CREATE TRIGGER vanish100 BEFORE UPDATE ON BidPages WHEN OLD.UID = 100 "
            "BEGIN DELETE FROM BidPages WHERE UID = OLD.UID; SELECT RAISE(IGNORE); END");
    expect_refused(db, "10", "100", "Renamed");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE UID = 100"), 1);
    CHECK_EQ(page_name(db, "100"), std::string("P100"));
    expect_renamed(db, "10", "101", "Renamed", "101");
}

// ---------------------------------------------------------------------------------------------------------------
// What changes and what does not (cases 19 to 21)
// ---------------------------------------------------------------------------------------------------------------

// Case 19 (page_operations.py:130-152: "only BidPages.Name of the one row"): page 100 sits in a page folder, has Sequence 3, Index1 2,
// takeoffs, a named view and is the saved selected page of the bid; after the rename every column of every row of EVERY table is
// identical except the Name (restoring the name gives the byte-identical database), and the named neighbours are checked directly.
TEST(PageRenameStore, OnlyTheNameOfThatRowChanges)
{
    TempDb db("rename_only_name");
    ost_ptest::seed_two_bids(db);
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (95, 10, 'F95')");
    db.must("UPDATE BidPages SET BidPageFolderUID = 95, Sequence = 3, Index1 = 2, SheetNo = 'S1', ScaleFactor1 = 0.25, ScaleFactor2 = 12 "
            "WHERE UID = 100");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (901, 10, 30, 100, '5;6;7;8')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (902, 10, 31, 101, '5;6;7;8')");
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (1, 10, 100)");
    const std::string before = db.snapshot_all();
    expect_renamed(db, "10", "100", "Renamed", "100");
    CHECK_EQ(db.text("SELECT BidPageFolderUID || '/' || Sequence || '/' || Index1 || '/' || SheetNo || '/' || ScaleFactor1 || '/' || "
                     "ScaleFactor2 FROM BidPages WHERE UID = 100"),
             std::string("95/3.0/2/S1/0.25/12.0"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE BidPageUID = 100"), 1);
    CHECK_EQ(db.count("SELECT BidPageSelectedUID FROM BidSettings WHERE BidUID = 10"), 100);
    db.must("UPDATE BidPages SET Name = 'P100' WHERE UID = 100");
    CHECK_EQ(db.snapshot_all(), before);
}

// Case 20 (plan 5.5 "Shared / locked": pages have no template concept): an id that is a LAYER (70), a CONDITION (30), an AREA (60), a
// TAKEOFF (900 of bid 20), a PAGE FOLDER (95), a CONDITION FOLDER (85) or a PROJECT (2) is never renamed through rename_page: there is no
// page with that uid. Their names stay.
TEST(PageRenameStore, OtherRowsWithTheSameUidAreNeverRenamed)
{
    TempDb db("rename_other_tables");
    ost_ptest::seed_two_bids(db);
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (95, 10, 'F95')");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (85, 10, 'CF85')");
    expect_refused(db, "10", "70", "Renamed");  // BidLayers 70 'L70'
    expect_refused(db, "10", "30", "Renamed");  // BidConditions 30 'C30'
    expect_refused(db, "10", "60", "Renamed");  // BidAreas 60 'A60'
    expect_refused(db, "20", "900", "Renamed"); // BidTakeoffs 900
    expect_refused(db, "10", "95", "Renamed");  // BidPageFolders 95 'F95'
    expect_refused(db, "10", "85", "Renamed");  // BidConditionFolders 85 'CF85'
    expect_refused(db, "10", "2", "Renamed");   // BidProjects 2 'Folder A'
    CHECK_EQ(db.text("SELECT Name FROM BidLayers WHERE UID = 70"), std::string("L70"));
    CHECK_EQ(db.text("SELECT Name FROM BidConditions WHERE UID = 30"), std::string("C30"));
    CHECK_EQ(db.text("SELECT Name FROM BidAreas WHERE UID = 60"), std::string("A60"));
    CHECK_EQ(db.text("SELECT Name FROM BidPageFolders WHERE UID = 95"), std::string("F95"));
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 85"), std::string("CF85"));
    CHECK_EQ(db.text("SELECT Name FROM BidProjects WHERE UID = 2"), std::string("Folder A"));
    expect_renamed(db, "10", "100", "Renamed", "100"); // control
}

// Case 21: renaming a page to the name it already has succeeds and changes nothing (the rule layer says Unchanged first, so the screen
// never gets here, but the store must still take it). A real rename first, so that "succeeds" cannot be a stub that does nothing.
TEST(PageRenameStore, Idempotent_RenamingToTheStoredNameSucceedsAndChangesNothing)
{
    TempDb db("rename_idempotent");
    ost_ptest::seed_two_bids(db);
    expect_renamed(db, "10", "100", "Once", "100");
    const std::string before = db.snapshot_all();
    expect_renamed(db, "10", "100", "Once", "100");
    CHECK_EQ(db.snapshot_all(), before);
}

// ---------------------------------------------------------------------------------------------------------------
// The name rule inside the store (the store never trusts the screen)
// ---------------------------------------------------------------------------------------------------------------

// RP-1 / RP-2 / N-M1-13: names that ost_domain::decide_page_rename would not write are refused by the store as well: blank, not stripped
// (Python strip would change them), over 255 UTF-16 units (including the astral character that crosses the limit), with U+0000, and
// text that is not valid UTF-8. The whole database stays identical.
TEST(PageRenameStore, Rule_NamesTheRuleWouldNotWriteAreRefused)
{
    TempDb db("rename_rule_refused");
    ost_ptest::seed_two_bids(db);
    const std::vector<std::string> refused = {
        "",
        " ",
        "   \t\n",
        std::string("\xC2\x85"),                  // U+0085 only: blank
        " A",                                     // not stripped
        "A ",
        "\tA",
        "A\n",
        "A\xE3\x80\x80",                          // trailing U+3000: not stripped
        "\xC2\x85" "A",                           // leading U+0085
        "\x1C" "A",                               // leading U+001C
        repeat("A", 256),                         // 256 units
        repeat("A", 1000),
        repeat("A", 254) + kEmoji,                // 255 code points, 256 units
        repeat(kEmoji, 128),                      // 256 units
        std::string("A\0B", 3),                   // NUL inside
        std::string("\0", 1),
        std::string("A\0", 2),
        "\xFF",                                   // not valid UTF-8
        "A\xC3\x28",
        "\xC0\x80",                               // overlong NUL
        "\xED\xA0\x80",                           // a surrogate
    };
    for (std::size_t i = 0; i < refused.size(); ++i)
    {
        SCOPED_CONTEXT("name #" + std::to_string(i));
        expect_refused(db, "10", "100", refused[i]);
    }
    expect_renamed(db, "10", "100", "Renamed", "100"); // control
}

// N-M1-13: the names right below the limits are written and read back exactly: 255 units as 255 'A' (255 bytes), as 255 x U+00E9 (510
// bytes: the limit is not a byte limit), as 127 emoji + 'A' (255 units, 509 bytes) and as 253 'A' + one emoji (255 units, the astral
// character that fits exactly).
TEST(PageRenameStore, Rule_NamesAtTheLimitAreWrittenExactly)
{
    TempDb db("rename_rule_limit");
    ost_ptest::seed_two_bids(db);
    const std::vector<std::string> accepted = {
        repeat("A", 255),
        repeat("\xC3\xA9", 255),
        repeat(kEmoji, 127) + "A",
        repeat("A", 253) + kEmoji,
        repeat("\xEF\xBF\xBF", 255), // U+FFFF, valid, one unit each
    };
    for (std::size_t i = 0; i < accepted.size(); ++i)
    {
        SCOPED_CONTEXT("name #" + std::to_string(i));
        expect_renamed(db, "10", "100", accepted[i], "100");
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 3);
    }
}

// Research section 4 case 8: a megabyte of text is refused by the store without a crash, and nothing is written.
TEST(PageRenameStore, Rule_OneMegabyteIsRefused)
{
    TempDb db("rename_rule_megabyte");
    ost_ptest::seed_two_bids(db);
    expect_refused(db, "10", "100", repeat("A", 1000000));
    expect_refused(db, "10", "100", repeat(kEmoji, 250000));
    expect_renamed(db, "10", "100", "Renamed", "100"); // control
}

// ---------------------------------------------------------------------------------------------------------------
// What a name may contain (cases 4 and 5) and what a name may be (unnamed pages)
// ---------------------------------------------------------------------------------------------------------------

// Case 4 (RP: duplicates are allowed, decision D5): a name another page has is written, in the same bid, in another bid and when it differs
// only by case. There is no uniqueness check anywhere in the Windows writer; both rows keep their own UID.
TEST(PageRenameStore, Names_DuplicatesAreAllowed)
{
    TempDb db("rename_duplicates");
    ost_ptest::seed_two_bids(db);
    expect_renamed(db, "10", "101", "P100", "101"); // same bid
    CHECK_EQ(ost_ptest::list(db, "SELECT UID FROM BidPages WHERE Name = 'P100' ORDER BY UID"), std::string("100,101"));
    expect_renamed(db, "20", "200", "P100", "200"); // another bid
    CHECK_EQ(ost_ptest::list(db, "SELECT UID FROM BidPages WHERE Name = 'P100' ORDER BY UID"), std::string("100,101,200"));
    expect_renamed(db, "10", "101", "p100", "101"); // differing only by case
    CHECK_EQ(page_names(db), std::string("100=P100,101=p100,200=P100"));
    // a page in another folder
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (95, 10, 'F95')");
    db.must("UPDATE BidPages SET BidPageFolderUID = 95 WHERE UID = 101");
    expect_renamed(db, "10", "101", "P100", "101");
    // the project loads both
    const ost_data::ProjectData project = ost_store::load_project(db.path(), "10");
    CHECK_EQ(project.pages.size(), static_cast<std::size_t>(2));
    CHECK_EQ(project.pages.at("100").name, std::string("P100"));
    CHECK_EQ(project.pages.at("101").name, std::string("P100"));
}

// Case 5 (the statement is parameterised: nothing is filtered, D13: control characters other than NUL are stored as typed): each name is
// read back byte for byte as TEXT, no row is added or lost.
TEST(PageRenameStore, Names_SpecialCharactersAreStoredAsTyped)
{
    TempDb db("rename_specials");
    ost_ptest::seed_two_bids(db);
    const std::vector<std::string> names = {
        "O'Brien",
        "Say \"x\"",
        "[A]",
        "A;B",
        "A--B",
        "A/*B*/",
        "100%",
        "A\\B",
        "A/B",
        "A:B",
        "?",
        ":name",
        "NULL",
        "null",
        "123",
        "0",
        "1e3",
        kEmoji,
        "e\xCC\x81",                           // combining acute
        "\xE6\x9D\xB1\xE4\xBA\xAC",            // CJK
        "\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D",    // RTL
        "\xE2\x80\x8F" "A",                    // RTL mark + A
        "A\tB",                                // inner control characters are kept (D13)
        "A\nB",
        "A\x1F" "B",
        "A\x7F" "B",
        "\xEF\xBB\xBF" "A",                    // BOM
        "\xE2\x80\x8B" "A",                    // zero width space (not stripped by Python either)
        "A  B",
        "Sheet 1 of 2",
    };
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        SCOPED_CONTEXT("name #" + std::to_string(i) + " '" + names[i] + "'");
        expect_renamed(db, "10", "100", names[i], "100");
        CHECK_EQ(db.text("SELECT typeof(Name) FROM BidPages WHERE UID = 100"), std::string("text"));
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 3);
    }
    CHECK_EQ(page_name(db, "101"), std::string("P101"));
}

// Case 5, the injection attempt: a name that closes a string and drops the table runs as data, nothing else: the table and its other rows
// survive and the next rename works.
TEST(PageRenameStore, Names_AnInjectionAttemptIsOnlyText)
{
    TempDb db("rename_injection");
    ost_ptest::seed_two_bids(db);
    const std::string attack = "x'); DROP TABLE BidPages; --";
    expect_renamed(db, "10", "100", attack, "100");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 3);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM sqlite_master WHERE name = 'BidPages'"), 1);
    CHECK_EQ(page_name(db, "101"), std::string("P101"));
    expect_renamed(db, "10", "101", "Fine", "101");
    // a name that looks like a second statement for a statement built by text
    expect_renamed(db, "10", "100", "'; UPDATE BidPages SET Name = 'hacked' WHERE 1=1; --", "100");
    CHECK_EQ(page_name(db, "101"), std::string("Fine"));
    CHECK_EQ(page_name(db, "200"), std::string("P200"));
}

// RP-5 / RP-6 (research section 7 oddity 6): a page whose stored name is empty ('') or NULL (the Cover Sheet can store one) can be given a
// name: the rename works on the row, not on the old text.
TEST(PageRenameStore, Names_AnUnnamedPageCanBeRenamed)
{
    TempDb db("rename_unnamed");
    ost_ptest::seed_two_bids(db);
    db.must("UPDATE BidPages SET Name = NULL WHERE UID = 100");
    db.must("UPDATE BidPages SET Name = '' WHERE UID = 101");
    expect_renamed(db, "10", "100", "From null", "100");
    expect_renamed(db, "10", "101", "From empty", "101");
    CHECK_EQ(page_names(db), std::string("100=From null,101=From empty,200=P200"));
}

// ---------------------------------------------------------------------------------------------------------------
// The bid lock (plan 4.1: checked INSIDE the transaction, after the ownership checks)
// ---------------------------------------------------------------------------------------------------------------

// A locked bid writes nothing (the job status is Locked): refused, the whole database identical, a reason given. The lock is per bid:
// the same database renames a page of the other (unlocked) bid.
TEST(PageRenameStore, Lock_ALockedBidWritesNothing)
{
    TempDb db("rename_locked");
    ost_ptest::seed_two_bids(db);
    ost_ptest::seed_locked_bid(db, 10, 5);
    expect_refused(db, "10", "100", "Renamed");
    expect_refused(db, "10", "101", "Renamed");
    CHECK_EQ(page_name(db, "100"), std::string("P100"));
    expect_renamed(db, "20", "200", "Other bid is open", "200"); // control: bid 20 has no status
}

// The lock follows the bid's status at the time of the call: a status that is not Locked (Locked 0) renames; making it Locked refuses
// again; unlocking (a status without the flag, then no status at all) renames again.
TEST(PageRenameStore, Lock_TheCurrentStatusDecides)
{
    TempDb db("rename_lock_follows");
    ost_ptest::seed_two_bids(db);
    ost_ptest::seed_open_bid(db, 10, 4); // status 4 'Open', Locked 0
    expect_renamed(db, "10", "100", "Open status", "100");
    ost_ptest::seed_job_status(db, 5, "-1", "Locked");
    db.must("UPDATE Bids SET JobStatusUID = 5 WHERE UID = 10");
    expect_refused(db, "10", "100", "While locked");
    CHECK_EQ(page_name(db, "100"), std::string("Open status"));
    db.must("UPDATE JobStatuses SET Locked = 0 WHERE UID = 5");
    expect_renamed(db, "10", "100", "Unlocked", "100");
    db.must("UPDATE Bids SET JobStatusUID = NULL WHERE UID = 10");
    expect_renamed(db, "10", "100", "No status", "100");
}

// Windows resolve_bid_status_lock: a status that no JobStatuses row names is unresolved, and an unresolved status counts as locked (fail
// closed): refused. Control: the bid without a status renames.
TEST(PageRenameStore, Lock_AnUnresolvedStatusFailsClosed)
{
    TempDb db("rename_lock_unresolved");
    ost_ptest::seed_two_bids(db);
    ost_ptest::seed_bid_with_status(db, 10, "77"); // no JobStatuses row 77
    expect_refused(db, "10", "100", "Renamed");
    expect_renamed(db, "20", "200", "No status", "200");
}

// An unresolved status fails closed: the bid has a status but the JobStatuses table is gone (dropped), so the status cannot be resolved
// and the bid counts as locked: refused. A bid without a status does not read JobStatuses at all (plan 4.1), so the other bid still
// renames. (A lock READ that itself fails is pinned in test_page_rename_review.cpp.)
TEST(PageRenameStore, Lock_AFailedReadFailsClosed)
{
    TempDb db("rename_lock_failed_read");
    ost_ptest::seed_two_bids(db);
    ost_ptest::seed_open_bid(db, 10, 4);
    db.must("DROP TABLE JobStatuses");
    expect_refused(db, "10", "100", "Renamed");
    expect_renamed(db, "20", "200", "No status", "200");
}

// ---------------------------------------------------------------------------------------------------------------
// The result contract and the failures that are not about the data
// ---------------------------------------------------------------------------------------------------------------

// `error` (optional): cleared on entry, a reason on every refusal, empty on success; a null pointer is accepted on every path.
TEST(PageRenameStore, Error_IsSetOnRefusalAndEmptyOnSuccessAndMayBeNull)
{
    TempDb db("rename_error");
    ost_ptest::seed_two_bids(db);
    std::string error = "stale";
    CHECK_FALSE(ost_store::rename_page(db.path(), "10", "99", "N", &error));
    CHECK(!error.empty());
    CHECK(error != "stale");
    CHECK_FALSE(ost_store::rename_page(db.path(), "10", "100", "", &error));
    CHECK(!error.empty());
    error = "stale";
    CHECK(ost_store::rename_page(db.path(), "10", "100", "Renamed", &error));
    CHECK_EQ(error, std::string());
    // no error pointer
    CHECK_FALSE(ost_store::rename_page(db.path(), "10", "99", "N"));
    CHECK(ost_store::rename_page(db.path(), "10", "100", "Renamed again"));
    CHECK_EQ(page_name(db, "100"), std::string("Renamed again"));
}

// A database that cannot be used never throws and never crashes: no such path, an empty path, a directory, a file that is not a database, a
// valid database without any table. Each gives false with a reason. A valid rename first and last, so that "false" cannot be a stub.
TEST(PageRenameStore, Failures_AnUnusableDatabaseGivesFalseAndNeverThrows)
{
    TempDb db("rename_unusable");
    ost_ptest::seed_two_bids(db);
    expect_renamed(db, "10", "100", "First", "100");

    const std::string garbage_path = ost_test::temp_path("rename_not_a_database_" + ost_ptest::run_token() + ".db");
    {
        std::ofstream garbage(garbage_path, std::ios::binary | std::ios::trunc);
        garbage << std::string(8192, 'x');
    }
    TempDb empty("rename_no_tables", false);
    const std::vector<std::string> paths = {
        "",
        ost_test::temp_path("rename_no_such_folder_" + ost_ptest::run_token() + "/db.sqlite"),
        ost_test::temp_path(""), // a directory
        garbage_path,
        empty.path(),
    };
    for (std::size_t i = 0; i < paths.size(); ++i)
    {
        SCOPED_CONTEXT("path #" + std::to_string(i) + " " + paths[i]);
        std::string error = "stale";
        bool result = true;
        try
        {
            result = ost_store::rename_page(paths[i], "10", "100", "Renamed", &error);
        }
        catch (...)
        {
            FAIL_TEST("rename_page threw");
        }
        CHECK_FALSE(result);
        CHECK(!error.empty());
        CHECK(error != "stale");
    }
    std::remove(garbage_path.c_str());
    std::remove((garbage_path + "-wal").c_str());
    std::remove((garbage_path + "-shm").c_str());
    expect_renamed(db, "10", "100", "Last", "100");
}
