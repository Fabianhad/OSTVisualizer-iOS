// The bid lock GUARD of the store (plan 4.1, S0.a): ost_store::integrity::bid_is_locked(db, bid_uid, locked) for the writers (same
// connection, inside their transaction) and the db_path wrapper ost_store::bid_is_locked(db_path, bid_uid, locked) for the UI.
// Windows oracle (Windows 49a2d59), the rule of ost_domain::resolve_bid_status_lock applied to the stored values:
//   ost_visualizer/domain/entities/cover_sheet.py:15-23                      resolve_bid_status_lock
//   ost_visualizer/application/services/project_read_service.py:81-94        is_bid_locked: no status = unlocked, unresolved = locked
//   ost_visualizer/infrastructure/mdb/components/settings_reader.py:63-77    _parse_job_statuses: Locked is set unless its text is "0",
//                                                                           "False" or "" (a NULL reads as "NULL": locked); a duplicate
//                                                                           UID raises
//   ost_visualizer/infrastructure/mdb/components/settings_reader.py:361-370  get_job_statuses: any failed read gives an EMPTY list
//   tests/application/services/test_project_read_service.py:70-77, :79-99, :113-127, :129-139, :142-151 (is_bid_locked)
//   tests/infrastructure/mdb/components/test_settings_reader.py:892-910 (the Locked values), :1002-1018 (duplicate UIDs -> [])
//   tests/domain/entities/test_cover_sheet.py (the rule itself: test_bid_lock_rule.cpp)
// BidLockOracleData.hpp holds the outcome of the REAL Windows code on 25 Locked values (table 1) and 29 status layouts (table 2).
// iOS safe additions to the Windows rule (plan 4.1): a bid that does not exist is locked, a bid uid that is not a positive integer
// (wrapper) or a failed read (no Bids table, an unopenable database) is locked AND reported as a failed read (false).
// Not pinned (Windows reads a MISSING column as "no status" / "not locked", settings_reader.py:71, :143, test_settings_reader.py:912-920):
// a Bids table without JobStatusUID or a JobStatuses table without Locked; the iOS DDL always has both columns.
// Every case is asked through both entry points, with `locked` pre-set to both values, so a function that leaves it untouched is caught.
#include "StoreTestSupport.hpp"

#include "BidLockOracleData.hpp"

#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore_BidLock.hpp"
#include "Persistence/ProjectStore/ProjectStore_Integrity.hpp"

#include <string>
#include <utility>
#include <vector>

using ost_ptest::TempDb;

namespace
{
    struct Answer
    {
        bool read_ok = false;
        bool locked = false;
    };

    // `read_ok` of a case: the contract fixes it only where the lock state was determined (Ok) or the read failed (Failed).
    enum class Read
    {
        Any,
        Ok,
        Failed
    };

    // The writer entry point on a connection of its own (the tests of an open transaction open theirs).
    Answer ask_inside(const std::string &path, long long bid_uid, bool initial)
    {
        Answer answer;
        answer.locked = initial;
        ost_db::OstDatabase connection;
        if (!connection.open(path))
            return answer;
        answer.read_ok = ost_store::integrity::bid_is_locked(connection, bid_uid, answer.locked);
        return answer;
    }

    // The UI entry point.
    Answer ask_wrapper(const std::string &path, const std::string &bid_uid, bool initial)
    {
        Answer answer;
        answer.locked = initial;
        answer.read_ok = ost_store::bid_is_locked(path, bid_uid, answer.locked);
        return answer;
    }

    void check_read(const Answer &answer, Read read)
    {
        if (read == Read::Ok)
            CHECK(answer.read_ok);
        if (read == Read::Failed)
            CHECK_FALSE(answer.read_ok);
    }

    // Both entry points, both pre-set values of `locked`.
    void expect_lock(const TempDb &db, long long bid_uid, bool want_locked, Read read = Read::Any)
    {
        for (const bool initial : {false, true})
        {
            SCOPED_CONTEXT(std::string("bid ") + std::to_string(bid_uid) + ", locked pre-set to " + (initial ? "true" : "false"));
            const Answer inside = ask_inside(db.path(), bid_uid, initial);
            CHECK_EQ(inside.locked, want_locked);
            check_read(inside, read);
            const Answer wrapper = ask_wrapper(db.path(), std::to_string(bid_uid), initial);
            CHECK_EQ(wrapper.locked, want_locked);
            check_read(wrapper, read);
        }
    }

    using Statuses = std::vector<std::pair<std::string, std::string>>;

    // The loosely typed Windows-test tables (StoreTestSupport.hpp seed_lock_oracle_case) in a database WITHOUT the iOS schema.
    void loose_case(const TempDb &db, const std::string &bid_status, const Statuses &statuses, bool with_status_table = true)
    {
        ost_ptest::seed_lock_oracle_case(db, 10, bid_status, statuses, with_status_table);
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// the rule on the iOS schema
// ---------------------------------------------------------------------------------------------------------------

// cover_sheet.py:17-18, project_read_service.py:82-83 and test_cover_sheet.py:19-24: no status is unlocked, whatever the statuses say
// (here a locked status exists and belongs to another bid).
TEST(BidLockStore, ABidWithoutAStatusIsUnlocked)
{
    TempDb db("bidlock_no_status");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db); // bid 10 and 20, JobStatusUID NULL
    ost_ptest::seed_job_status(db, 5, "-1", "Locked");
    ost_ptest::seed_bid_with_status(db, 30, "5"); // another bid is locked
    expect_lock(db, 10, false, Read::Ok);
    expect_lock(db, 20, false, Read::Ok);
    expect_lock(db, 30, true, Read::Ok);
}

// test_project_read_service.py:70-77 test_bid_lock_lookup_preserves_authoritative_status_uid: the status UID decides, not the name (two
// statuses called "Duplicate", one locked and one open). Also the scoping of tests/application/services/test_active_bid_write_guard.py:
// :34-57 (the lock is the lock of the REQUESTED bid: bid 7 blocked, bid 8 not): here bid 10 is open and bid 11 is locked, one bid never
// locks another (the iOS guard checks the bid of the write, a superset of the Windows "active bid" rule, plan 4.1).
TEST(BidLockStore, ABidWithALockedStatusIsLockedAndWithAnOpenStatusIsNot)
{
    TempDb db("bidlock_by_uid");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_job_status(db, 4, "0", "Duplicate");
    ost_ptest::seed_job_status(db, 5, "-1", "Duplicate");
    ost_ptest::seed_bid_with_status(db, 10, "4");
    ost_ptest::seed_bid_with_status(db, 11, "5");
    expect_lock(db, 10, false, Read::Ok);
    expect_lock(db, 11, true, Read::Ok);
}

// seed_locked_bid / seed_open_bid are the fixtures the other streams use: they must mean what they say.
TEST(BidLockStore, TheSeedHelpersMakeALockedAndAnOpenBid)
{
    TempDb db("bidlock_seed_helpers");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_locked_bid(db, 10, 5);
    ost_ptest::seed_open_bid(db, 11, 4);
    CHECK_STR_EQ(db.text("SELECT typeof(Locked) || ':' || Locked FROM JobStatuses WHERE UID = 5"), "integer:-1");
    CHECK_STR_EQ(db.text("SELECT typeof(Locked) || ':' || Locked FROM JobStatuses WHERE UID = 4"), "integer:0");
    CHECK_EQ(db.count("SELECT JobStatusUID FROM Bids WHERE UID = 10"), 5);
    expect_lock(db, 10, true, Read::Ok);
    expect_lock(db, 11, false, Read::Ok);
}

// test_project_read_service.py:142-151: a resolvable status reads its own flag, a status of None / "" is unlocked; and
// test_active_bid_write_guard.py:13-23 test_unlocking_the_active_bid_lifts_the_write_block: the same bid asked again after the status
// flag or the bid's status changed gives the new answer (nothing is cached: the lock is read from the database every time).
TEST(BidLockStore, TheAnswerIsReadFreshOnEveryCall)
{
    TempDb db("bidlock_fresh");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_locked_bid(db, 10, 5);
    expect_lock(db, 10, true, Read::Ok);
    db.must("UPDATE JobStatuses SET Locked = 0 WHERE UID = 5");
    expect_lock(db, 10, false, Read::Ok);
    db.must("UPDATE JobStatuses SET Locked = -1 WHERE UID = 5");
    expect_lock(db, 10, true, Read::Ok);
    db.must("UPDATE Bids SET JobStatusUID = NULL WHERE UID = 10");
    expect_lock(db, 10, false, Read::Ok);
    db.must("UPDATE Bids SET JobStatusUID = 5 WHERE UID = 10");
    expect_lock(db, 10, true, Read::Ok);
    db.must("DELETE FROM JobStatuses WHERE UID = 5"); // the status vanished: unresolved
    expect_lock(db, 10, true);
}

// test_project_read_service.py:79-99 (the status list is empty / does not contain the status) and :113-127 (an integer zero status
// uid is an unknown uid, not "no status"): fail closed, never unlocked.
TEST(BidLockStore, AStatusNoRowNamesIsLocked)
{
    struct Case
    {
        const char *bid_status;
        const char *what;
    };
    const Case cases[] = {{"3", "uid 3 is not a status"}, {"0", "uid 0 is not a status"}, {"-1", "uid -1 is not a status"}, {"9", "uid 9"}};
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.what);
        TempDb db(std::string("bidlock_unknown_") + c.bid_status);
        REQUIRE(db.schema_ok());
        ost_ptest::seed_job_status(db, 1, "0", "Open"); // the list holds another status
        ost_ptest::seed_bid_with_status(db, 10, c.bid_status);
        expect_lock(db, 10, true);
    }
}

// test_project_read_service.py:129-139: status uid 0 exists -> an ordinary uid with its own flag.
TEST(BidLockStore, AStatusWithUidZeroIsAnOrdinaryStatusWhenItExists)
{
    for (const bool locked : {true, false})
    {
        SCOPED_CONTEXT(locked ? "zero is locked" : "zero is open");
        TempDb db(std::string("bidlock_zero_") + (locked ? "locked" : "open"));
        REQUIRE(db.schema_ok());
        db.must(std::string("INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (0, 'Zero', ") + (locked ? "-1" : "0") + ", 0)");
        ost_ptest::seed_job_status(db, 1, locked ? "0" : "-1", "Other");
        ost_ptest::seed_bid_with_status(db, 10, "0");
        expect_lock(db, 10, locked, Read::Ok);
    }
}

// test_project_read_service.py:79-99 "the status list is empty": an empty JobStatuses table and a MISSING one (Windows
// optional_table_missing -> []) both leave a bid with a status unresolved = locked; a bid without a status never reads the table
// (test_project_read_service.py:317-330 test_absent_status_skips_read_and_unknown_status_does_not_match_name).
TEST(BidLockStore, AnEmptyOrMissingJobStatusesTableLocksABidWithAStatusOnly)
{
    TempDb db("bidlock_no_table");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_bid_with_status(db, 10, "5");
    ost_ptest::seed_bid_with_status(db, 11, "NULL");
    expect_lock(db, 10, true); // empty table
    expect_lock(db, 11, false, Read::Ok);
    db.must("DROP TABLE JobStatuses");
    expect_lock(db, 10, true); // missing table
    expect_lock(db, 11, false, Read::Ok); // never read
}

// ---------------------------------------------------------------------------------------------------------------
// duplicates (the Windows read raises, the list is empty)
// ---------------------------------------------------------------------------------------------------------------

// settings_reader.py:63-65 require_unique_master_data_uids + :361-370 (get_job_statuses -> []), test_settings_reader.py:1002-1018:
// a duplicate JobStatuses UID anywhere makes EVERY status unresolved, the unique ones included; a bid without status is unaffected.
TEST(BidLockStore, DuplicateJobStatusUidsLockEveryBidWithAStatus)
{
    struct Case
    {
        const char *name;
        Statuses statuses;
        const char *bid_status;
        bool locked;
    };
    const Case cases[] = {
        {"7 twice, open first", {{"7", "0"}, {"7", "-1"}}, "7", true},
        {"7 twice, locked first", {{"7", "-1"}, {"7", "0"}}, "7", true},
        {"7 twice, both open", {{"7", "0"}, {"7", "0"}}, "7", true},
        {"8 twice spoils the unique 7", {{"7", "0"}, {"8", "0"}, {"8", "-1"}}, "7", true},
        {"7 twice, the bid has no status", {{"7", "0"}, {"7", "-1"}}, "NULL", false},
        {"7 as integer and as text", {{"7", "0"}, {"'7'", "0"}}, "7", true},
        {"two NULL uids", {{"NULL", "0"}, {"NULL", "0"}}, "3", true},
    };
    int index = 0;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.name);
        TempDb db("bidlock_dup_" + std::to_string(index++), false);
        loose_case(db, c.bid_status, c.statuses);
        expect_lock(db, 10, c.locked, c.locked ? Read::Any : Read::Ok);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the Windows oracle rows
// ---------------------------------------------------------------------------------------------------------------

// BidLockOracleData.hpp table 1 on the iOS DDL for the values that survive its INTEGER affinity as written (NULL, 0, 1, -1, 2, 'False',
// '' and 'True': -1 is what the importer stores for Yes): the answer of the real Windows reader + is_bid_locked.
TEST(BidLockStore, TheLockedColumnFollowsTheWindowsRuleOnTheIosSchema)
{
    const std::vector<std::string> stable = {"NULL", "0", "1", "-1", "2", "'False'", "''", "'True'"};
    int index = 0;
    int used = 0;
    for (const auto &row : ost_ptest::bidlock::flag_rows())
    {
        bool is_stable = false;
        for (const std::string &literal : stable)
            is_stable = is_stable || literal == row.locked_literal;
        if (!is_stable)
            continue;
        ++used;
        SCOPED_CONTEXT(std::string("Locked = ") + row.locked_literal);
        TempDb db("bidlock_ios_flag_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        ost_ptest::seed_job_status(db, 5, row.locked_literal, "Status");
        ost_ptest::seed_bid_with_status(db, 10, "5");
        // the stored value is what the oracle row says it is (the DDL did not change it)
        expect_lock(db, 10, row.locked, Read::Ok);
    }
    CHECK_EQ(used, 8);
}

// BidLockOracleData.hpp table 1 on the loosely typed Windows-test tables: all 25 stored values, as written.
TEST(BidLockStore, TheLockedColumnMatchesTheWindowsOracleOnEveryStoredValue)
{
    const auto &rows = ost_ptest::bidlock::flag_rows();
    REQUIRE(rows.size() == 25);
    int index = 0;
    for (const auto &row : rows)
    {
        SCOPED_CONTEXT(std::string("Locked = ") + row.locked_literal);
        TempDb db("bidlock_flag_" + std::to_string(index++), false);
        loose_case(db, "5", {{"5", row.locked_literal}});
        expect_lock(db, 10, row.locked, Read::Ok);
    }
}

// BidLockOracleData.hpp table 2: 29 layouts of statuses and bid status (no status, matching, text compare, zero, duplicates, empty
// and missing table) with the answer of the real Windows reader + is_bid_locked.
TEST(BidLockStore, TheStatusLookupMatchesTheWindowsOracleOnEveryLayout)
{
    const auto &rows = ost_ptest::bidlock::resolve_rows();
    REQUIRE(rows.size() == 29);
    int index = 0;
    for (const auto &row : rows)
    {
        SCOPED_CONTEXT(row.name);
        TempDb db("bidlock_layout_" + std::to_string(index++), false);
        Statuses statuses;
        for (const auto &status : row.statuses)
            statuses.emplace_back(status.uid, status.locked);
        loose_case(db, row.bid_status_literal, statuses, row.with_status_table);
        // an unlocked answer can only come from a resolved lookup; for a locked one the contract does not say whether it is a
        // locked status or an unresolved one
        expect_lock(db, 10, row.locked, row.locked ? Read::Any : Read::Ok);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// missing bid, failed reads (fail closed)
// ---------------------------------------------------------------------------------------------------------------

// plan 4.1: a missing bid = locked (Windows has no such lookup: the bid row comes first).
TEST(BidLockStore, AMissingBidIsLocked)
{
    TempDb db("bidlock_missing_bid");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_open_bid(db, 10, 4);
    expect_lock(db, 10, false, Read::Ok);
    expect_lock(db, 99, true);
    expect_lock(db, 1, true); // Bids has no row 1 (UID 1 is the project "Deleted Bids")
}

// project_read_service.py:79-99 "the status read failed" + plan 4.1: a failed read is locked and reported as a failed read.
TEST(BidLockStore, AFailedReadIsLockedAndReportedAsFailed)
{
    {
        SCOPED_CONTEXT("no Bids table at all");
        TempDb db("bidlock_no_bids_table", false);
        db.must("CREATE TABLE Unrelated (UID INTEGER)");
        expect_lock(db, 10, true, Read::Failed);
    }
    {
        // the wrapper is the only entry point that can be pointed at a path
        SCOPED_CONTEXT("a database that cannot be opened (no such folder)");
        for (const bool initial : {false, true})
        {
            const Answer wrapper =
                ask_wrapper(ost_test::temp_path("bidlock_no_such_dir_" + ost_ptest::run_token() + "/db.sqlite"), "10", initial);
            CHECK_FALSE(wrapper.read_ok);
            CHECK(wrapper.locked);
        }
    }
}

// plan 4.1 (wrapper): a bid uid that is not a positive integer (integrity::parse_uid) is a failed read: locked, false. Bid 7 is an open
// bid, so a lenient parse (trimming, signs, decimals) would answer "unlocked" for it.
TEST(BidLockStore, TheWrapperRefusesAMalformedBidUid)
{
    TempDb db("bidlock_bad_uid");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_open_bid(db, 7, 4);
    for (const char *uid : {"abc", "", "0", "-7", "+7", " 7", "7 ", "7.0", "7a", "0x7", "99999999999999999999"})
    {
        SCOPED_CONTEXT(std::string("bid uid '") + uid + "'");
        for (const bool initial : {false, true})
        {
            const Answer answer = ask_wrapper(db.path(), uid, initial);
            CHECK_FALSE(answer.read_ok);
            CHECK(answer.locked);
        }
    }
    // the same bid with a well-formed uid, leading zeros allowed (integrity::parse_uid "007" is 7)
    for (const char *uid : {"7", "007"})
    {
        SCOPED_CONTEXT(std::string("bid uid '") + uid + "'");
        const Answer answer = ask_wrapper(db.path(), uid, true);
        CHECK(answer.read_ok);
        CHECK_FALSE(answer.locked);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// how the writers use it
// ---------------------------------------------------------------------------------------------------------------

// plan 4.1: the writers call it INSIDE their transaction (BEGIN IMMEDIATE) on the same connection: it must use THAT connection (it sees
// the writer's own uncommitted change) and must leave the transaction open and untouched.
TEST(BidLockStore, ItReadsOnTheCallersOwnConnectionInsideAnOpenTransaction)
{
    TempDb db("bidlock_in_transaction");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_open_bid(db, 10, 4);
    ost_ptest::seed_job_status(db, 5, "-1", "Locked");
    ost_db::OstDatabase connection;
    REQUIRE(connection.open(db.path()));
    REQUIRE(connection.begin_transaction());

    bool locked = true;
    CHECK(ost_store::integrity::bid_is_locked(connection, 10, locked));
    CHECK_FALSE(locked);

    REQUIRE(connection.exec("UPDATE Bids SET JobStatusUID = 5 WHERE UID = 10")); // uncommitted: only this connection sees it
    locked = false;
    CHECK(ost_store::integrity::bid_is_locked(connection, 10, locked));
    CHECK(locked);

    CHECK(connection.in_transaction());
    CHECK(connection.rollback());
    locked = true;
    CHECK(ost_store::integrity::bid_is_locked(connection, 10, locked)); // after the rollback the bid is open again
    CHECK_FALSE(locked);
    CHECK_EQ(db.count("SELECT JobStatusUID FROM Bids WHERE UID = 10"), 4);
}

// The lock read writes nothing (the whole database is compared before and after, every case of the guard) and holds no lock afterwards:
// a writer can begin at once. The answers themselves are checked too, so that "nothing written" cannot pass for "nothing read".
TEST(BidLockStore, ReadingTheLockWritesNothingAndLeavesNoLockBehind)
{
    TempDb db("bidlock_read_only");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db);
    ost_ptest::seed_locked_bid(db, 30, 5);
    ost_ptest::seed_open_bid(db, 31, 4);
    ost_ptest::seed_bid_with_status(db, 32, "77"); // unresolved
    const std::string before = db.snapshot_all();
    struct Case
    {
        long long bid;
        bool locked;
    };
    const Case cases[] = {{10, false}, {20, false}, {30, true}, {31, false}, {32, true}, {99, true}};
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT("bid " + std::to_string(c.bid));
        CHECK_EQ(ask_inside(db.path(), c.bid, !c.locked).locked, c.locked);
        CHECK_EQ(ask_wrapper(db.path(), std::to_string(c.bid), !c.locked).locked, c.locked);
        CHECK_EQ(db.snapshot_all(), before);
    }
    ost_db::OstDatabase writer;
    REQUIRE(writer.open(db.path()));
    CHECK(writer.begin_transaction());
    CHECK(writer.rollback());
    CHECK_EQ(db.snapshot_all(), before);
}

// The lock read does not depend on the rest of the bid: a bid with takeoffs, pages and conditions answers as before (seed_pages is the
// fixture of the page features).
TEST(BidLockStore, ABidWithPagesAndTakeoffsAnswersLikeAnEmptyOne)
{
    TempDb db("bidlock_with_pages");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_pages(db, 10, 3, 2);
    ost_ptest::seed_locked_bid(db, 10, 5);
    ost_ptest::seed_pages(db, 11, 2, 1, 2000, 9500, 7100);
    expect_lock(db, 10, true, Read::Ok);
    expect_lock(db, 11, false, Read::Ok); // no status
}
