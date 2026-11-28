// DBP-18 (+BRW-03, COND-DUP-02): a new project folder / takeoff / condition gets MAX(UID) + 1 raised above the MAX of every
// column that references the table, so a new row never adopts a dangling reference.
// Windows oracle: identity_allocation.py::_next_uid_preserving_references with raw_bid_integrity.BID_RELATIONSHIPS
//   tests/infrastructure/mdb/components/test_project_operations.py::test_project_insert_does_not_claim_orphan_bid_project_uid
//   tests/infrastructure/mdb/components/test_takeoff_operations.py::test_takeoff_insert_reserves_every_inbound_takeoff_reference_column
//   tests/infrastructure/mdb/components/test_condition_operations.py::test_condition_duplicate_reserves_dangling_ancillary_condition_uid
#include "StoreTestSupport.hpp"

#include <string>
#include <vector>

using ost_ptest::TempDb;

namespace
{
    struct Reference
    {
        const char *table;
        const char *column;
    };

    // BID_RELATIONSHIPS with parent_table BidTakeoffs (sorted, as the Windows test asserts them).
    const Reference kTakeoffReferences[] = {
        {"BidALines", "BidTakeoffFromUID"},   {"BidALines", "BidTakeoffToUID"},   {"BidArrows", "BidTakeoffFromUID"},
        {"BidArrows", "BidTakeoffToUID"},     {"BidDimensions", "BidTakeoffFromUID"}, {"BidDimensions", "BidTakeoffToUID"},
        {"BidPercents", "BidTakeoffUID"},     {"BidTakeoffs", "ParentUID"},       {"BidTakeoffs", "TypGroupMarkerUID"},
        {"BidTakeoffs", "TypGroupTakeoffUID"}, {"BidTakeoffs", "TypPageTakeoffUID"},
    };

    // BID_RELATIONSHIPS with parent_table BidConditions.
    const Reference kConditionReferences[] = {
        {"BidConditionUser", "ConditionUID"}, {"BidLaborActivity", "BidConditionUID"},
        {"BidTakeoffTotals", "BidConditionUID"}, {"BidTakeoffs", "BidConditionUID"},
        {"BidTypGroupViews", "BidConditionUID"}, {"BidTypicalGroupTotals", "BidConditionUID"},
    };
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// create_folder
// ---------------------------------------------------------------------------------------------------------------

// Port of test_project_insert_does_not_claim_orphan_bid_project_uid: BidProjects (7, 'Existing') (+ the 'Deleted Bids' project
// 1 of create_schema), Bids (1, BidProjectUID 8) -> the new project is 9 and the orphan bid does not join a project.
TEST(Dbp18_UidAllocation, NewFolderDoesNotClaimAnOrphanBidProjectUid)
{
    TempDb db("alloc_folder_orphan");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (7, 'Existing')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 8, 'Orphan')");

    CHECK_STR_EQ(ost_store::create_folder(db.path(), "New"), "9");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || Name FROM BidProjects ORDER BY UID"), "1:Deleted Bids,7:Existing,9:New");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids AS bid INNER JOIN BidProjects AS project ON bid.BidProjectUID = project.UID"), 0);
}

// BRW-03: Bids.OrigBidProjectUID = 12 with the largest project UID 7 -> 13; the larger of the two columns wins; 0 / NULL
// references are ignored.
TEST(Dbp18_UidAllocation, NewFolderIsAboveEveryProjectReferenceOfTheBids)
{
    struct Case
    {
        const char *bid_sql; // values of (BidProjectUID, OrigBidProjectUID)
        const char *expected;
    };
    const Case cases[] = {
        {"NULL, 12", "13"}, {"8, 12", "13"}, {"20, 12", "21"}, {"0, 0", "8"}, {"NULL, NULL", "8"}, {"7, 7", "8"}, {"3, NULL", "8"},
    };
    int index = 0;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("(BidProjectUID, OrigBidProjectUID) = (") + c.bid_sql + ")");
        TempDb db("alloc_folder_orig_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (7, 'Existing')");
        db.must(std::string("INSERT INTO Bids (UID, BidProjectUID, OrigBidProjectUID, JobName) VALUES (1, ") + c.bid_sql + ", 'Orphan')");
        CHECK_STR_EQ(ost_store::create_folder(db.path(), "New"), c.expected);
    }
}

// An unreferenced database allocates MAX + 1 as before; the undo path (explicit UIDs) is not involved.
TEST(Dbp18_UidAllocation, NewFolderIsMaxPlusOneWithoutReferences)
{
    TempDb db("alloc_folder_plain");
    REQUIRE(db.schema_ok());
    CHECK_STR_EQ(ost_store::create_folder(db.path(), "A"), "2");
    CHECK_STR_EQ(ost_store::create_folder(db.path(), "B"), "3");
    db.must("DELETE FROM BidProjects WHERE UID = 3");
    CHECK_STR_EQ(ost_store::create_folder(db.path(), "C"), "3");
}

// ---------------------------------------------------------------------------------------------------------------
// insert_takeoff
// ---------------------------------------------------------------------------------------------------------------

// Port of test_takeoff_insert_reserves_every_inbound_takeoff_reference_column: takeoff 7 exists, a dangling reference to 8
// sits in one of the 11 inbound reference columns -> the new takeoff is 9, never 8.
TEST(Dbp18_UidAllocation, NewTakeoffIsAboveEveryInboundTakeoffReference)
{
    for (const Reference &reference : kTakeoffReferences)
    {
        SCOPED_CONTEXT(std::string(reference.table) + "." + reference.column);
        TempDb db(std::string("alloc_takeoff_") + reference.table + "_" + reference.column);
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (10, 1, 'job')");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (5, 10, 1, 'C', 0)");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (3, 10, 'P')");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (7, 10, 5, 3, '0;0;1;1')");
        if (std::string(reference.table) == "BidTakeoffs")
            db.must(std::string("UPDATE BidTakeoffs SET ") + reference.column + " = 8 WHERE UID = 7");
        else
            db.must(std::string("INSERT INTO ") + reference.table + " (UID, " + reference.column + ") VALUES (70, 8)");

        const std::string uid = ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("5", "3"));
        CHECK_STR_EQ(uid, "9");
        // nothing joins the dangling reference to a takeoff row
        if (std::string(reference.table) != "BidTakeoffs")
            CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM ") + reference.table + " AS child INNER JOIN BidTakeoffs AS takeoff ON child." +
                              reference.column + " = takeoff.UID"),
                     0);
    }
}

// References that are NULL, 0 or '' reserve nothing; the plain case is MAX(UID) + 1 (1 for an empty table).
TEST(Dbp18_UidAllocation, NewTakeoffIgnoresNullZeroAndEmptyReferences)
{
    TempDb db("alloc_takeoff_plain");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (10, 1, 'job')");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (5, 10, 1, 'C', 0)");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (3, 10, 'P')");
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("5", "3")), "1");
    db.must("INSERT INTO BidPercents (UID, BidTakeoffUID) VALUES (70, 0)");
    db.must("INSERT INTO BidPercents (UID, BidTakeoffUID) VALUES (71, '')");
    db.must("INSERT INTO BidPercents (UID, BidTakeoffUID) VALUES (72, NULL)");
    db.must("INSERT INTO BidDimensions (UID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (80, 0, NULL)");
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("5", "3")), "2");
    db.must("DELETE FROM BidTakeoffs WHERE UID = 2");
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("5", "3")), "2");
}

// The undo path re-inserts a deleted takeoff with its old UID: that must keep working, whatever the references say.
TEST(Dbp18_UidAllocation, ExplicitTakeoffUidIsKept)
{
    TempDb db("alloc_takeoff_explicit");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (10, 1, 'job')");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (5, 10, 1, 'C', 0)");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (3, 10, 'P')");
    db.must("INSERT INTO BidDimensions (UID, BidTakeoffFromUID) VALUES (80, 41)");
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("5", "3", "0", "0", "41")), "41");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 41"), 1);
}

// ---------------------------------------------------------------------------------------------------------------
// duplicate_condition
// ---------------------------------------------------------------------------------------------------------------

// Port of test_condition_duplicate_reserves_dangling_ancillary_condition_uid (COND-DUP-02): BidConditions (7, bid 1, 'source-guid',
// RefNo 1, 'Source'), BidTakeoffTotals (1, BidConditionUID 8) -> the duplicate is 9, no totals row joins a condition.
TEST(Dbp18_UidAllocation, DuplicatedConditionReservesADanglingTotalsReference)
{
    TempDb db("alloc_condition_totals");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'job')");
    db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (7, 1, 'source-guid', 1, 'Source', 0)");
    db.must("INSERT INTO BidTakeoffTotals (UID, BidConditionUID) VALUES (1, 8)");

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "7"), "9");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffTotals AS totals INNER JOIN BidConditions AS conditions "
                      "ON totals.BidConditionUID = conditions.UID"),
             0);
}

// Every column of BID_RELATIONSHIPS that references BidConditions reserves its UIDs (the Windows aliases: BidConditionUser.ConditionUID,
// BidLaborActivity / BidTypGroupViews / BidTypicalGroupTotals / BidTakeoffs .BidConditionUID).
TEST(Dbp18_UidAllocation, DuplicatedConditionIsAboveEveryInboundConditionReference)
{
    for (const Reference &reference : kConditionReferences)
    {
        SCOPED_CONTEXT(std::string(reference.table) + "." + reference.column);
        TempDb db(std::string("alloc_condition_") + reference.table);
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'job')");
        db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (7, 1, 'source-guid', 1, 'Source', 0)");
        if (std::string(reference.table) == "BidTakeoffs")
            db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, Position) VALUES (70, 1, 8, '0;0;1;1')");
        else
            db.must(std::string("INSERT INTO ") + reference.table + " (UID, " + reference.column + ") VALUES (70, 8)");
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "7"), "9");
    }
}

// The plain case: UIDs 1..100 present -> the duplicate is 101; a reference below the maximum changes nothing.
TEST(Dbp18_UidAllocation, DuplicatedConditionIsMaxPlusOneWithoutDanglingReferences)
{
    TempDb db("alloc_condition_plain");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'job')");
    db.must("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < 100) "
            "INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) SELECT i, 1, 'g' || i, i, 'c' || i, 0 FROM n");
    db.must("INSERT INTO BidTakeoffTotals (UID, BidConditionUID) VALUES (1, 50)");
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "1"), "101");
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "1"), "102");
}

// ===============================================================================================================
// M1 step 0 (S0.b, plan 4.2, owner decision N-M1-04): the UID limit of a Windows Long column.
// ===============================================================================================================
// ost_store::integrity::next_uid_within_access_limit(db, table, count, first_out) = next_uid_preserving_references plus a refusal BEFORE any
// write when first + count - 1 > 2147483647, in 64-bit arithmetic, and ost_store::duplicate_condition switched to it.
// Windows keeps NO UID ceiling: identity_allocation.py:5-47 (_next_uid_preserving_references: MAX(UID) + 1 raised above every referencing
// column, nothing else) and database_creator.py:157 (condition UID), :467, :474 (takeoff UID, ParentUID) are Access COUNTER / Long columns;
// only RefNo is guarded (condition_operations.py:56-62, the CondAlloc01_* tests of the Conditions area). The refusal is therefore a
// DELIBERATE SAFE DEVIATION (iOS SQLite allows 63-bit UIDs, a row above 2147483647 could never be exported into a Windows database, M3):
// there is no Windows oracle for it, the expected values below are the limit arithmetic itself (2147483647 = 2^31 - 1).
// The first test group calls the helper on a connection (inside a transaction, like the writers), the second group drives
// duplicate_condition. Refusal text (plan 4.2 / 4.14): "Cannot add a condition: no identity is left below 2147483647."

#include "Persistence/ProjectStore/ProjectStore_Integrity.hpp"

#include <climits>
#include <cstddef>

namespace
{
    const char *const kNoIdentityText = "Cannot add a condition: no identity is left below 2147483647.";

    struct Allocation
    {
        bool ok = false;
        long long first = 777; // sentinel: the helper must always write first_out
    };

    // The helper on a connection of its own inside a transaction (the writers' place), rolled back afterwards.
    Allocation allocate(const TempDb &db, const char *table, long long count, bool in_transaction = true)
    {
        Allocation out;
        ost_db::OstDatabase connection;
        if (!connection.open(db.path()))
            return out;
        if (in_transaction && !connection.begin_transaction())
            return out;
        out.ok = ost_store::integrity::next_uid_within_access_limit(connection, table, count, out.first);
        if (in_transaction)
            connection.rollback();
        return out;
    }

    // A database where BidConditions has the given highest UID (and one more small one), nothing else.
    void seed_highest_condition_uid(const TempDb &db, long long highest)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
        db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (10, 1, 'g10', 1, 'Ten', 0)");
        if (highest > 10)
            db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (" + std::to_string(highest) +
                    ", 1, 'gh', 2, 'High', 0)");
    }

    void expect_allocated(const Allocation &a, long long first)
    {
        CHECK(a.ok);
        CHECK_EQ(a.first, first);
    }

    void expect_refused(const Allocation &a)
    {
        CHECK_FALSE(a.ok);
        CHECK_EQ(a.first, 0LL); // never a usable value after a refusal
    }
} // namespace

// Below the limit the helper IS next_uid_preserving_references: MAX(UID) + 1 raised above every referencing column, 1 for an empty table,
// the same first UID whatever `count` is (the UIDs first .. first + count - 1 are the ones reserved).
TEST(UidLimit_Helper, BelowTheLimitItIsTheNextUidPreservingReferences)
{
    TempDb db("limit_plain");
    REQUIRE(db.schema_ok());
    expect_allocated(allocate(db, "BidConditions", 1), 1); // empty table
    expect_allocated(allocate(db, "BidConditions", 5), 1);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'Bid')");
    db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (10, 1, 'g10', 1, 'Ten', 0)");
    expect_allocated(allocate(db, "BidConditions", 1), 11);
    expect_allocated(allocate(db, "BidConditions", 3), 11);
    // a dangling reference above MAX(UID) raises the start, as for every other allocation (DBP-18)
    db.must("INSERT INTO BidTakeoffTotals (UID, BidConditionUID) VALUES (1, 50)");
    expect_allocated(allocate(db, "BidConditions", 1), 51);
    // other tables: the project table (Bids.BidProjectUID / OrigBidProjectUID reference it), takeoffs, layers, pages
    expect_allocated(allocate(db, "BidProjects", 1), 2);
    expect_allocated(allocate(db, "BidTakeoffs", 4), 1);
    expect_allocated(allocate(db, "BidPages", 2), 1);
    // the answer does not depend on being inside a transaction
    expect_allocated(allocate(db, "BidConditions", 2, false), 51);
}

// The allocation of several identities at exactly the limit: the last one is 2147483647 and still allowed (Access Long maximum).
TEST(UidLimit_Helper, TheLastIdentityBelowTheLimitIsAllowed)
{
    struct Case
    {
        long long highest; // MAX(UID) of BidConditions, 0 for an empty table
        long long count;
        long long first;
    };
    const Case cases[] = {
        {2147483646LL, 1, 2147483647LL}, // one identity left: count 1
        {2147483645LL, 2, 2147483646LL}, // 2147483646 and 2147483647
        {2147483644LL, 3, 2147483645LL},
        {2147483640LL, 7, 2147483641LL},
        {1000LL, 2147482647LL, 1001LL}, // 1001 + 2147482647 - 1 = 2147483647
        {0LL, 2147483647LL, 1LL},       // an empty table, the whole range
    };
    int index = 0;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT("highest " + std::to_string(c.highest) + ", count " + std::to_string(c.count));
        TempDb db("limit_exact_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        if (c.highest > 0)
            seed_highest_condition_uid(db, c.highest);
        expect_allocated(allocate(db, "BidConditions", c.count), c.first);
    }
}

// One past the limit is refused: first + count - 1 = 2147483648 (and above): nothing is returned to use.
TEST(UidLimit_Helper, OneIdentityPastTheLimitIsRefused)
{
    struct Case
    {
        long long highest;
        long long count;
    };
    const Case cases[] = {
        {2147483646LL, 2},      // 2147483647 + 2 - 1 = 2147483648
        {2147483647LL, 1},      // the next UID is 2147483648
        {2147483645LL, 3},      // 2147483646 + 3 - 1 = 2147483648
        {2147483644LL, 4},
        {2147483640LL, 8},
        {1000LL, 2147482648LL}, // 1001 + 2147482648 - 1 = 2147483648
        {0LL, 2147483648LL},    // an empty table, one more than the range
        {10LL, 2147483647LL},   // 11 + 2147483647 - 1 = 2147483657
    };
    int index = 0;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT("highest " + std::to_string(c.highest) + ", count " + std::to_string(c.count));
        TempDb db("limit_past_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        if (c.highest > 0)
            seed_highest_condition_uid(db, c.highest);
        expect_refused(allocate(db, "BidConditions", c.count));
    }
}

// The arithmetic is 64 bit: a UID or a count that would wrap a 32-bit (or, for the count, a 64-bit signed) sum is still refused. A
// 32-bit unsigned sum of 4294967295 + 1 is 0, a signed one of 2147483647 + 1 is negative: both would pass `<= 2147483647`.
TEST(UidLimit_Helper, TheArithmeticIs64Bit)
{
    struct Case
    {
        long long highest;
        long long count;
    };
    const Case cases[] = {
        {2147483648LL, 1},                  // just above the signed 32-bit range
        {4294967294LL, 1},                  // next 4294967295 = -1 as int32
        {4294967295LL, 1},                  // next 4294967296 = 0 as uint32
        {4294967296LL, 1},
        {5000000000LL, 1},
        {9223372036854775806LL, 1},         // next = 9223372036854775807 (the largest 63-bit UID)
        {9223372036854775806LL, 2},         // first + count - 1 overflows a signed 64-bit sum
        {9223372036854775806LL, LLONG_MAX}, // the same with the largest count
        {0LL, 4294967297LL},                // count wraps to 1 as uint32
        {0LL, 4294967296LL + 2147483647LL}, // count wraps to a valid-looking value as uint32
        {0LL, LLONG_MAX},                   // 1 + LLONG_MAX overflows before the - 1 of `first + count - 1`
        {10LL, LLONG_MAX},
    };
    int index = 0;
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT("highest " + std::to_string(c.highest) + ", count " + std::to_string(c.count));
        TempDb db("limit_64_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        if (c.highest > 0)
            seed_highest_condition_uid(db, c.highest);
        expect_refused(allocate(db, "BidConditions", c.count));
    }
}

// MAX(UID) = LLONG_MAX: no UID is left above it at all (next_uid_preserving_references fails): refused, whatever count is.
TEST(UidLimit_Helper, ALargestPossibleUidLeavesNothing)
{
    TempDb db("limit_llong_max");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, LLONG_MAX);
    expect_refused(allocate(db, "BidConditions", 1));
    expect_refused(allocate(db, "BidConditions", 5));
}

// A count below one is a caller bug and is refused (nothing to reserve): the safe answer, so a loop that ran dry cannot write a row.
// (An S0 addition: Windows has no such call. The reviewer may drop the case if count 0 should succeed.)
TEST(UidLimit_Helper, ACountBelowOneIsRefused)
{
    TempDb db("limit_count_zero");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, 10);
    for (const long long count : {0LL, -1LL, -2147483647LL, LLONG_MIN})
    {
        SCOPED_CONTEXT("count " + std::to_string(count));
        expect_refused(allocate(db, "BidConditions", count));
    }
    expect_allocated(allocate(db, "BidConditions", 1), 11); // a count of one is fine
}

// The UIDs of the rows that REFERENCE the table count as much as its own rows (DBP-18): a dangling reference at 2147483647 leaves no
// identity, one at 2147483646 leaves exactly one. All six condition reference columns and all eleven takeoff reference columns.
TEST(UidLimit_Helper, ReferencesCountTowardsTheLimit)
{
    struct Reference
    {
        const char *table;
        const char *column;
    };
    const Reference condition_references[] = {
        {"BidConditionUser", "ConditionUID"}, {"BidLaborActivity", "BidConditionUID"}, {"BidTakeoffTotals", "BidConditionUID"},
        {"BidTakeoffs", "BidConditionUID"},   {"BidTypGroupViews", "BidConditionUID"}, {"BidTypicalGroupTotals", "BidConditionUID"},
    };
    const Reference takeoff_references[] = {
        {"BidALines", "BidTakeoffFromUID"},    {"BidALines", "BidTakeoffToUID"},       {"BidArrows", "BidTakeoffFromUID"},
        {"BidArrows", "BidTakeoffToUID"},      {"BidDimensions", "BidTakeoffFromUID"}, {"BidDimensions", "BidTakeoffToUID"},
        {"BidPercents", "BidTakeoffUID"},      {"BidTakeoffs", "ParentUID"},           {"BidTakeoffs", "TypGroupMarkerUID"},
        {"BidTakeoffs", "TypGroupTakeoffUID"}, {"BidTakeoffs", "TypPageTakeoffUID"},
    };
    struct Group
    {
        const char *table; // the allocated table
        const Reference *references;
        std::size_t size;
    };
    const Group groups[] = {{"BidConditions", condition_references, sizeof condition_references / sizeof condition_references[0]},
                            {"BidTakeoffs", takeoff_references, sizeof takeoff_references / sizeof takeoff_references[0]}};
    int index = 0;
    for (const Group &group : groups)
    {
        for (std::size_t i = 0; i < group.size; ++i)
        {
            const Reference &reference = group.references[i];
            for (const long long referenced : {2147483646LL, 2147483647LL, 4294967296LL})
            {
                SCOPED_CONTEXT(std::string(reference.table) + "." + reference.column + " = " + std::to_string(referenced));
                TempDb db("limit_ref_" + std::to_string(index++));
                REQUIRE(db.schema_ok());
                db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'Bid')");
                if (std::string(group.table) == "BidConditions")
                    db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (5, 1, 'g5', 1, 'C', 0)");
                else
                    db.must("INSERT INTO BidTakeoffs (UID, BidUID, Position) VALUES (7, 1, '0;0;1;1')");
                if (std::string(reference.table) == "BidTakeoffs")
                    db.must(std::string("INSERT INTO BidTakeoffs (UID, BidUID, ") + reference.column + ", Position) VALUES (70, 1, " +
                            std::to_string(referenced) + ", '0;0;1;1')");
                else
                    db.must(std::string("INSERT INTO ") + reference.table + " (UID, " + reference.column + ") VALUES (70, " +
                            std::to_string(referenced) + ")");
                if (referenced == 2147483646LL)
                {
                    expect_allocated(allocate(db, group.table, 1), 2147483647LL); // the last identity
                    expect_refused(allocate(db, group.table, 2));
                }
                else
                {
                    expect_refused(allocate(db, group.table, 1)); // nothing is left
                }
            }
        }
    }
}

// Several tables: the same limit for every table the new writers allocate in (conditions, folders, layers, pages, takeoffs, projects).
TEST(UidLimit_Helper, EveryAllocatedTableHasTheSameLimit)
{
    const char *const tables[] = {"BidConditions", "BidConditionFolders", "BidLayers", "BidPages", "BidTakeoffs", "BidProjects"};
    int index = 0;
    for (const char *table : tables)
    {
        SCOPED_CONTEXT(std::string("table ") + table);
        TempDb db("limit_table_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        db.must(std::string("INSERT INTO ") + table + " (UID) VALUES (2147483646)");
        expect_allocated(allocate(db, table, 1), 2147483647LL);
        expect_refused(allocate(db, table, 2));
        db.must(std::string("INSERT INTO ") + table + " (UID) VALUES (2147483647)");
        expect_refused(allocate(db, table, 1));
    }
}

// The helper never writes: before and after, a refused call, an allowed call and a failed call leave the WHOLE database as it was (every
// table compared), inside a transaction and outside one.
TEST(UidLimit_Helper, ARefusalAndASuccessWriteNothing)
{
    TempDb db("limit_snapshot");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, 2147483646LL);
    db.must("INSERT INTO BidTakeoffTotals (UID, BidConditionUID) VALUES (1, 3)");
    const std::string before = db.snapshot_all();

    expect_allocated(allocate(db, "BidConditions", 1), 2147483647LL);
    CHECK_EQ(db.snapshot_all(), before);
    expect_refused(allocate(db, "BidConditions", 2));
    CHECK_EQ(db.snapshot_all(), before);
    expect_refused(allocate(db, "BidConditions", LLONG_MAX));
    CHECK_EQ(db.snapshot_all(), before);
    expect_refused(allocate(db, "BidConditions", 0));
    CHECK_EQ(db.snapshot_all(), before);
    expect_refused(allocate(db, "NoSuchTable", 1));
    CHECK_EQ(db.snapshot_all(), before);
    expect_refused(allocate(db, "BidConditions", 2, false));
    CHECK_EQ(db.snapshot_all(), before);
    expect_allocated(allocate(db, "BidConditions", 1, false), 2147483647LL);
    CHECK_EQ(db.snapshot_all(), before);
}

// A failed statement is a refusal as well: a table that does not exist.
TEST(UidLimit_Helper, AMissingTableIsARefusal)
{
    TempDb db("limit_missing_table");
    REQUIRE(db.schema_ok());
    expect_refused(allocate(db, "NoSuchTable", 1));
    expect_refused(allocate(db, "NoSuchTable", 3));
}

// The allocation is a read inside the writer's own transaction: an uncommitted row of that transaction counts (the helper uses the
// caller's connection), and a row that was rolled back does not.
TEST(UidLimit_Helper, ItSeesTheUncommittedRowsOfTheCallersTransaction)
{
    TempDb db("limit_uncommitted");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, 10);
    ost_db::OstDatabase connection;
    REQUIRE(connection.open(db.path()));
    REQUIRE(connection.begin_transaction());
    long long first = 0;
    CHECK(ost_store::integrity::next_uid_within_access_limit(connection, "BidConditions", 1, first));
    CHECK_EQ(first, 11LL);
    REQUIRE(connection.exec(
        "INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (2147483646, 1, 'gx', 3, 'X', 0)"));
    first = 0;
    CHECK(ost_store::integrity::next_uid_within_access_limit(connection, "BidConditions", 1, first));
    CHECK_EQ(first, 2147483647LL);
    first = 5;
    CHECK_FALSE(ost_store::integrity::next_uid_within_access_limit(connection, "BidConditions", 2, first));
    CHECK_EQ(first, 0LL);
    REQUIRE(connection.rollback());
    CHECK(ost_store::integrity::next_uid_within_access_limit(connection, "BidConditions", 2, first));
    CHECK_EQ(first, 11LL);
}

// ---------------------------------------------------------------------------------------------------------------
// duplicate_condition switched to the helper (plan 4.2 / 4.14, N-M1-04)
// ---------------------------------------------------------------------------------------------------------------

// A copy whose UID would pass 2147483647 is refused BEFORE anything is written, with the new text through the error out-parameter.
// Every table of the database is compared before and after (no row, no UID, no number moved).
TEST(UidLimit_Duplicate, ACopyWhoseUidWouldPassTheLimitIsRefusedAndWritesNothing)
{
    TempDb db("limit_dup_refused");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, 2147483647LL);
    const std::string before = db.snapshot_all();

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, kNoIdentityText);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 2);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID > 2147483647"), 0);

    // the source does not matter (the identity is the table's), and a call without the error channel refuses the same way
    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "2147483647", &error), "");
    CHECK_STR_EQ(error, kNoIdentityText);
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "");
    CHECK_EQ(db.snapshot_all(), before);
}

// The boundary is exclusive: 2147483647 itself is an identity that can be used (the last one); the copy after it is refused.
TEST(UidLimit_Duplicate, TheLastIdentityBelowTheLimitCanStillBeUsed)
{
    TempDb db("limit_dup_last");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, 2147483646LL);

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "2147483647");
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(db.text("SELECT typeof(UID) || ':' || UID FROM BidConditions WHERE Name = 'Ten' AND UID > 10"), "integer:2147483647");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 3);
    const std::string after_first = db.snapshot_all();

    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, kNoIdentityText);
    CHECK_EQ(db.snapshot_all(), after_first);
}

// SQLite holds UIDs a Windows Long cannot: a table whose highest UID is above 32 bits (or the largest 63-bit one) is refused too, never
// wrapped (a 32-bit sum of 4294967295 + 1 is 0, and the old allocator would have written 4294967296).
TEST(UidLimit_Duplicate, UidsAbove32BitsAreRefusedNotWrapped)
{
    const long long highs[] = {2147483648LL, 4294967294LL, 4294967295LL, 4294967296LL, 5000000000LL, 9223372036854775806LL};
    int index = 0;
    for (const long long high : highs)
    {
        SCOPED_CONTEXT("highest UID " + std::to_string(high));
        TempDb db("limit_dup_big_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        seed_highest_condition_uid(db, high);
        const std::string before = db.snapshot_all();
        std::string error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
        CHECK_STR_EQ(error, kNoIdentityText);
        CHECK_EQ(db.snapshot_all(), before);
    }
    // the largest possible UID leaves nothing above it: refused, nothing written (the text is not part of the contract here)
    TempDb db("limit_dup_llong_max");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, LLONG_MAX);
    const std::string before = db.snapshot_all();
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "");
    CHECK_EQ(db.snapshot_all(), before);
}

// A dangling reference counts like a row (DBP-18): BidTakeoffTotals.BidConditionUID = 2147483647 leaves no identity, 2147483646 leaves the
// last one.
TEST(UidLimit_Duplicate, ADanglingReferenceCountsTowardsTheLimit)
{
    {
        SCOPED_CONTEXT("reference at 2147483647");
        TempDb db("limit_dup_ref_full");
        REQUIRE(db.schema_ok());
        seed_highest_condition_uid(db, 10);
        db.must("INSERT INTO BidTakeoffTotals (UID, BidConditionUID) VALUES (1, 2147483647)");
        const std::string before = db.snapshot_all();
        std::string error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
        CHECK_STR_EQ(error, kNoIdentityText);
        CHECK_EQ(db.snapshot_all(), before);
    }
    {
        SCOPED_CONTEXT("reference at 2147483646");
        TempDb db("limit_dup_ref_last");
        REQUIRE(db.schema_ok());
        seed_highest_condition_uid(db, 10);
        db.must("INSERT INTO BidTakeoffTotals (UID, BidConditionUID) VALUES (1, 2147483646)");
        std::string error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "2147483647");
        CHECK_STR_EQ(error, "");
    }
}

// The refusal comes BEFORE any write: with an INSERT that would fail anyway, the caller still gets the identity text, not the generic
// failure (the same rule the number guard pins in CondAlloc01_DuplicateLimit.AnUnknownOrForeignConditionAtTheLimitIsTheGenericFailure).
TEST(UidLimit_Duplicate, TheRefusalComesBeforeTheInsert)
{
    TempDb db("limit_dup_before_insert");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, 2147483647LL);
    const std::string trigger = db.block("BidConditions", "INSERT");
    const std::string before = db.snapshot_all();
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, kNoIdentityText);
    CHECK_EQ(db.snapshot_all(), before);
    db.unblock(trigger);
}

// The ownership check comes first (plan 4.4: parse, bid, ownership, ..., UID limit): an unknown or foreign condition, a malformed uid or an
// unknown bid at an exhausted table is the generic failure (empty error), never the identity text.
TEST(UidLimit_Duplicate, AnUnknownOrForeignConditionAtTheLimitIsTheGenericFailure)
{
    TempDb db("limit_dup_generic");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, 2147483647LL);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Other', 2)");
    db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (20, 2, 'g20', 1, 'Foreign', 0)");
    const std::string before = db.snapshot_all();

    struct Case
    {
        const char *bid;
        const char *condition;
        const char *what;
    };
    const Case cases[] = {
        {"1", "99", "unknown condition"},
        {"1", "20", "a condition of bid 2"},
        {"2", "10", "named under the wrong bid"},
        {"1", "abc", "not a uid"},
        {"99", "10", "unknown bid"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.what);
        std::string error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), c.bid, c.condition, &error), "");
        CHECK_STR_EQ(error, "");
    }
    CHECK_EQ(db.snapshot_all(), before);
}

// A refusal consumes nothing: no UID and no state is kept, so once an identity is free the next copy gets the UID the refused one would
// have had.
TEST(UidLimit_Duplicate, ARefusalConsumesNoIdentity)
{
    TempDb db("limit_dup_no_uid");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, 2147483647LL);
    std::string error;
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, kNoIdentityText);

    db.must("DELETE FROM BidConditions WHERE UID = 2147483647"); // the identity at the top is gone: MAX(UID) is 10 again
    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "11");
    CHECK_STR_EQ(error, "");
}

// The error channel is optional and always reset (a success clears it, a refusal sets it, a call without it works).
TEST(UidLimit_Duplicate, TheErrorChannelIsClearedAndOptionalAtTheLimit)
{
    TempDb db("limit_dup_channel");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, 2147483645LL);

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "2147483646");
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "2147483647"); // no error channel at all
    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, kNoIdentityText);
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "");
}

// Below the limit nothing changes: the copy gets MAX(UID) + 1 as before and the number guard of job 6 is untouched (RefNo = highest + 1).
TEST(UidLimit_Duplicate, BelowTheLimitTheCopyIsNumberedAsBefore)
{
    TempDb db("limit_dup_plain");
    REQUIRE(db.schema_ok());
    seed_highest_condition_uid(db, 500);
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "501");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 501"), 3); // RefNo 1 and 2 exist: highest + 1
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "501"), "502");
}
