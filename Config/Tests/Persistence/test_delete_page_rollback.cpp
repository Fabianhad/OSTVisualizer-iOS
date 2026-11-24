// M1 Delete Page (plan 5.6): the delete is irreversible, so a failure anywhere must leave the database BYTE-IDENTICAL. Every test compares
// snapshot_all (every table) before and after a failed call. Failures are injected with BEFORE <event> triggers that RAISE(ABORT) (the device
// of Windows test_ownership_and_atomicity.py:154-220 and of PersistenceTestSupport.hpp TempDb::block), with a corrupted table page (a failed
// READ) and with a second connection that holds the write lock (the busy timeout).
//
// The (table, event) pairs below are exactly the statements of the Windows cascade (settings_operations.py:698-891); the real Windows
// delete_pages was run on the same world with each trigger and fails every time, so each trigger fires inside the cascade (checked when
// the tests were written, not part of the run). The plan (5.6 tests-first) asks for a seeded row on the 2nd, 12th and 23rd child table, on
// BidTakeoffs and on BidPages (the LAST statement): every one of them is covered here, and the last statement is also checked row by row so
// that the clears of steps 13 and 14 are proven rolled back together with the earlier steps (MasterPageUID, comment detach, takeoff
// self-references, percents).
#include "DeletePageTestSupport.hpp"

#include <string>
#include <utility>
#include <vector>

using ost_delpage::run;
using ost_delpage::Uids;
using ost_ptest::TempDb;

namespace
{
    // The statements of the cascade as (table, event): the 23 child tables DELETE (step 8, with the steps 5-7 that touch BidPercents,
    // BidDimensions, BidALines, BidArrows and AffectDPCTypGroupViews), and the writes of the other steps.
    std::vector<std::pair<std::string, std::string>> cascade_statements()
    {
        std::vector<std::pair<std::string, std::string>> out;
        for (const char *table : ost_delpage_oracle::page_delete_child_tables())
            out.emplace_back(table, "DELETE");
        const std::pair<const char *, const char *> others[] = {
            {"BidPages", "UPDATE"},                // step 1: MasterPageUID
            {"BidComments", "UPDATE"},             // step 2: ParentCommentUID
            {"BidTakeoffs", "UPDATE"},             // step 4: the four self references
            {"AffectDPCTypGroupViews", "DELETE"},  // step 7
            {"BidTakeoffs", "DELETE"},             // step 9
            {"BidHotLinks", "DELETE"},             // steps 10 and 11
            {"BidNamedViews", "DELETE"},           // step 12
            {"BidSettings", "UPDATE"},             // step 13
            {"Bids", "UPDATE"},                    // step 14
            {"BidPages", "DELETE"}};               // step 15, the last statement
        for (const auto &pair : others)
            out.emplace_back(pair.first, pair.second);
        return out;
    }

    void seed_a(const TempDb &db)
    {
        ost_delpage::seed_scenario(db, ost_delpage::scenario("A_DeleteOnePage"));
    }
} // namespace

// The failure of ANY statement of the cascade rolls the whole transaction back: for each of the 33 statements a trigger is installed on one database
// that holds a row for the page in every table, the call fails with an EMPTY error (a failed statement is not a refusal), the whole database is
// identical afterwards, the trigger is dropped and the next statement is tried on the same database (so a leaked half-write would show in the
// next round). The last round has no trigger: the control run commits and agrees with the Windows oracle.
TEST(DelPageRollback, AFailureOfAnyStatementOfTheCascadeLeavesTheDatabaseIdentical)
{
    TempDb db("delpage_rb_every_statement");
    seed_a(db);
    const auto statements = cascade_statements();
    REQUIRE_EQ(statements.size(), static_cast<size_t>(33));
    const std::string before = db.snapshot_all();
    for (const auto &statement : statements)
    {
        SCOPED_CONTEXT("blocked: " + statement.second + " on " + statement.first);
        const std::string trigger = db.block(statement.first, statement.second);
        std::string error;
        CHECK_FALSE(run(db, "1", {"10"}, error));
        CHECK_STR_EQ(error, "");
        CHECK_EQ(db.snapshot_all(), before);
        db.unblock(trigger);
    }
    const auto &scenario = ost_delpage::scenario("A_DeleteOnePage");
    CHECK(run(db, "1", {"10"}));
    CHECK_STR_EQ(ost_delpage::projection_differences(db, scenario.expected), "");
}

// The plan's last statement: with BidPages DELETE blocked the call fails after the 14 earlier steps have run, so every one of their effects must be
// undone. Checked row by row on the world of scenario A: the master chain, the comment thread, the takeoff references, the percents, the annotations,
// the hot links, the named views, the saved selection of three bids and the cover sheet selection of bid 1; and again after the control run.
TEST(DelPageRollback, TheLastStatementFailingUndoesEveryEarlierStep)
{
    TempDb db("delpage_rb_last_statement");
    seed_a(db);
    const std::string trigger = db.block("BidPages", "DELETE");
    const std::string before = db.snapshot_all();

    std::string error;
    CHECK_FALSE(run(db, "1", {"10"}, error));
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(MasterPageUID, 'N') FROM BidPages ORDER BY UID"), "10:N,11:10,12:11,20:10"); // step 1
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(ParentCommentUID, 'N') FROM BidComments WHERE UID IN (101, 102, 103, 106) ORDER BY UID"),
                 "101:100,102:101,103:101,106:105"); // step 2
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(ParentUID, 'N') || ',' || ifnull(TypGroupTakeoffUID, 'N') FROM BidTakeoffs WHERE UID IN (501, 502, 506) ORDER BY UID"),
                 "501:500,500,502:501,N,506:505,N"); // step 4
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPercents"), 5);                                                    // step 5
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidDimensions"), 5);                                                  // step 6
    CHECK_EQ(db.count("SELECT COUNT(*) FROM AffectDPCTypGroupViews"), 3);                                         // step 7
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidHighlights"), 3);                                                  // step 8
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 7);                                                    // step 9
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidHotLinks"), 6);                                                    // steps 10, 11
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidNamedViews"), 3);                                                  // step 12
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(BidPageSelectedUID, 'N') FROM BidSettings ORDER BY UID"), "2000:10,2001:11,2002:20,2003:10"); // step 13
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(CoverSheetSelItemUID, 'N') FROM Bids WHERE UID <= 3 ORDER BY UID"), "1:10,2:10,3:11"); // step 14

    db.unblock(trigger);
    CHECK(run(db, "1", {"10"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(MasterPageUID, 'N') FROM BidPages ORDER BY UID"), "11:N,12:11,20:10");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(BidPageSelectedUID, 'N') FROM BidSettings ORDER BY UID"), "2000:N,2001:11,2002:20,2003:N");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(CoverSheetSelItemUID, 'N') FROM Bids WHERE UID <= 3 ORDER BY UID"), "1:N,2:10,3:11");
}

// The same at the chunk border (scenario C: 110 pages, 1..100 in two chunks): a failure in the SECOND chunk of BidPages leaves the first chunk's
// effects undone too, a failure in the second chunk of a child table likewise.
TEST(DelPageRollback, AFailureInTheSecondChunkOfAnyTableUndoesTheFirstChunk)
{
    TempDb db("delpage_rb_second_chunk");
    const auto &scenario = ost_delpage::scenario("C_ChunkBoundary_Pages1To100");
    ost_delpage::seed_scenario(db, scenario);
    const std::string before = db.snapshot_all();
    const Uids uids = ost_delpage::scenario_uids(scenario);
    struct Where
    {
        const char *table;
        const char *event;
        const char *when;
    };
    const Where cases[] = {{"BidPages", "DELETE", "OLD.UID = 100"},          {"BidPages", "UPDATE", "OLD.MasterPageUID = 60"},
                           {"BidTakeoffs", "DELETE", "OLD.BidPageUID = 99"}, {"BidComments", "UPDATE", "OLD.ParentCommentUID = 3067"},
                           {"BidHighlights", "DELETE", "OLD.BidPageUID = 75"}, {"BidNamedViews", "DELETE", "OLD.BidPageUID = 51"},
                           {"BidPercents", "DELETE", "OLD.BidTakeoffUID = 1100"}};
    for (const Where &c : cases)
    {
        SCOPED_CONTEXT(std::string("fails on ") + c.event + " of " + c.table + " when " + c.when);
        db.must(std::string("CREATE TRIGGER chunk_fail BEFORE ") + c.event + " ON [" + c.table + "] WHEN " + c.when +
                " BEGIN SELECT RAISE(ABORT, 'later chunk'); END");
        std::string error;
        CHECK_FALSE(run(db, "1", uids, error));
        CHECK_STR_EQ(error, "");
        CHECK_EQ(db.snapshot_all(), before);
        db.must("DROP TRIGGER chunk_fail");
    }
    CHECK(run(db, "1", uids));
    CHECK_STR_EQ(ost_delpage::projection_differences(db, scenario.expected), "");
}

// A failed READ (not a failed write): the root pages of one table are overwritten, so the first statement that reads it fails. The call returns
// false and every other table is exactly as before (a before / after snapshot shows the broken table as "<select failed>" both times). One database
// per table: the tables of the steps that SELECT (BidTakeoffs, BidComments) and of the sub-selects (BidNamedViews, BidTypGroupViews), a child table,
// the tables the cascade writes last (BidSettings, Bids) and the ownership tables (BidPages, Bids). For the tables the cascade reads inside its
// statements the error is empty (a failed statement); the ownership checks of integrity.* report a failed read like a refused check, so their text is
// not pinned.
TEST(DelPageRollback, AFailedReadOfAnyTableOfTheCascadeLeavesTheDatabaseIdentical)
{
    struct Broken
    {
        const char *table;
        bool error_is_empty;
    };
    const Broken tables[] = {{"BidTakeoffs", true}, {"BidComments", true},  {"BidNamedViews", true}, {"BidTypGroupViews", true}, {"BidHighlights", true},
                             {"BidPercents", true}, {"BidSettings", true}, {"BidPages", false},     {"Bids", false}};
    for (const Broken &broken : tables)
    {
        SCOPED_CONTEXT(std::string("unreadable table ") + broken.table);
        TempDb db("delpage_rb_corrupt");
        seed_a(db);
        REQUIRE(ost_delpage::corrupt_table(db, broken.table));
        const std::string before = db.snapshot_all();

        std::string error;
        CHECK_FALSE(run(db, "1", {"10"}, error));
        if (broken.error_is_empty)
            CHECK_STR_EQ(error, "");
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// The injector of the test above does what it says (a test of the test): after corrupt_table the table cannot be read and the others still can.
TEST(DelPageRollback, TheReadFailureInjectorBreaksExactlyOneTable)
{
    TempDb db("delpage_rb_injector");
    seed_a(db);
    CHECK(db.count("SELECT COUNT(*) FROM BidTakeoffs") > 0);
    REQUIRE(ost_delpage::corrupt_table(db, "BidTakeoffs"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), -1);
    CHECK(db.count("SELECT COUNT(*) FROM BidPages") > 0);
    CHECK(db.count("SELECT COUNT(*) FROM BidComments") > 0);
}

// After a failed call no transaction stays open and no lock is held: another writer can write at once, the file opens and create_schema still
// succeeds (research 4: "the file must open and create_schema must succeed afterwards").
TEST(DelPageRollback, AFailedCallLeavesNoLockAndAUsableFile)
{
    TempDb db("delpage_rb_no_lock");
    seed_a(db);
    const std::string trigger = db.block("BidPages", "DELETE");
    CHECK_FALSE(run(db, "1", {"10"}));
    db.unblock(trigger);

    CHECK(db.exec("INSERT INTO BidAreas (UID, BidUID) VALUES (4242, 1)"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidAreas WHERE UID = 4242"), 1);
    CHECK(ost_store::create_schema(db.path()));
    CHECK(run(db, "1", {"10"}));
}

// A second connection that holds the write lock (BEGIN IMMEDIATE) makes the call wait the busy timeout (OstDatabase::kBusyTimeoutMs, 5000 ms) and then
// fail; nothing is written. SLOW: the test waits the timeout once. After the other writer lets go the same call succeeds.
TEST(DelPageSlow, ABusyDatabaseFailsAfterTheTimeoutAndChangesNothing)
{
    TempDb db("delpage_rb_busy");
    seed_a(db);
    const std::string before = db.snapshot_all();
    ost_db::OstDatabase other;
    REQUIRE(other.open(db.path()));
    REQUIRE(other.begin_transaction());

    std::string error;
    CHECK_FALSE(run(db, "1", {"10"}, error));
    CHECK_STR_EQ(error, "");
    REQUIRE(other.rollback());
    other.close();
    CHECK_EQ(db.snapshot_all(), before);

    CHECK(run(db, "1", {"10"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE UID = 10"), 0);
}
