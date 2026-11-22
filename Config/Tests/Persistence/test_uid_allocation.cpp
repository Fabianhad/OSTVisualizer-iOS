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
