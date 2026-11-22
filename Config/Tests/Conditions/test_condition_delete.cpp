// COND-DEL-01 (aliases DBP-15, QTY-M01): deleting a condition removes every dependent of that condition in that bid,
// clears the self-references of the takeoffs that survive, and only deletes a condition that belongs to the bid.
//
// Windows: ost_visualizer/infrastructure/mdb/components/condition_operations.py delete_conditions /
// _delete_condition_cascade_chunk, constants.py TAKEOFF_ANNOTATION_REFERENCE_COLUMNS / TAKEOFF_SELF_REFERENCE_COLUMNS.
// iOS deletes one condition per call (Windows takes a list), so the batch tests are ported with a single condition.
#include "ConditionsTestSupport.hpp"

using ost_ctest::TempDb;

namespace
{
    // The iOS-schema counterpart of ConditionDeleteCascadeContractTests._db:
    //   conditions 10 and 11 (bid 1), 20 (bid 2)
    //   takeoffs 100 (cond 10), 101 (cond 11, parent/typ columns 100), 102 (cond 11, parent 101), 300 (bid 2, cond 10)
    //   annotations 1: 100->102, 2: 102->100, 3: 101->102 in BidDimensions, BidALines and BidArrows
    //   labor activities 1 (bid 1, cond 10), 2 (bid 1, cond 11), 3 (bid 2, cond 10)
    //   percents 1 (takeoff 100), 2 (takeoff 102, activity 1), 3 (takeoff 102, activity 2)
    //   type group views 1 (bid 1, cond 10), 2 (bid 2, cond 10), 3 (bid 1, cond 11) with AffectDPC rows 1, 2, 3
    //   BidTakeoffTotals / BidTypicalGroupTotals 1 (bid 1, cond 10), 2 (bid 2, cond 10), 3 (bid 1, cond 11)
    //   BidConditionUser 1 (cond 10), 2 (cond 11); ConditionSetStyles 1 (cond 10), 2 (cond 11)
    void seed_cascade_world(const TempDb &db)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid one', 1)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Bid two', 2)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 1, 'c', 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (11, 1, 1, 'c', 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (20, 2, 1, 'c', 0)");
        const char *takeoff = "INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, ParentUID, TypGroupTakeoffUID, "
                              "TypPageTakeoffUID, TypGroupMarkerUID, Position) VALUES ";
        db.must(std::string(takeoff) + "(100, 1, 10, NULL, NULL, NULL, NULL, '1;2;3;4')");
        db.must(std::string(takeoff) + "(101, 1, 11, 100, 100, 100, 100, '1;2;3;4')");
        db.must(std::string(takeoff) + "(102, 1, 11, 101, NULL, NULL, NULL, '1;2;3;4')");
        db.must(std::string(takeoff) + "(300, 2, 10, NULL, NULL, NULL, NULL, '1;2;3;4')");
        for (const char *table : {"BidDimensions", "BidALines", "BidArrows"})
        {
            const std::string insert = std::string("INSERT INTO ") + table +
                                       " (UID, BidTakeoffFromUID, BidTakeoffToUID) VALUES ";
            db.must(insert + "(1, 100, 102)");
            db.must(insert + "(2, 102, 100)");
            db.must(insert + "(3, 101, 102)");
        }
        db.must("INSERT INTO BidLaborActivity (UID, BidUID, BidConditionUID) VALUES (1, 1, 10)");
        db.must("INSERT INTO BidLaborActivity (UID, BidUID, BidConditionUID) VALUES (2, 1, 11)");
        db.must("INSERT INTO BidLaborActivity (UID, BidUID, BidConditionUID) VALUES (3, 2, 10)");
        db.must("INSERT INTO BidPercents (UID, BidTakeoffUID, BidLaborActivityUID) VALUES (1, 100, NULL)");
        db.must("INSERT INTO BidPercents (UID, BidTakeoffUID, BidLaborActivityUID) VALUES (2, 102, 1)");
        db.must("INSERT INTO BidPercents (UID, BidTakeoffUID, BidLaborActivityUID) VALUES (3, 102, 2)");
        db.must("INSERT INTO BidTypGroupViews (UID, BidUID, BidConditionUID) VALUES (1, 1, 10)");
        db.must("INSERT INTO BidTypGroupViews (UID, BidUID, BidConditionUID) VALUES (2, 2, 10)");
        db.must("INSERT INTO BidTypGroupViews (UID, BidUID, BidConditionUID) VALUES (3, 1, 11)");
        db.must("INSERT INTO AffectDPCTypGroupViews (UID, BidTypGroupViewUID) VALUES (1, 1)");
        db.must("INSERT INTO AffectDPCTypGroupViews (UID, BidTypGroupViewUID) VALUES (2, 2)");
        db.must("INSERT INTO AffectDPCTypGroupViews (UID, BidTypGroupViewUID) VALUES (3, 3)");
        for (const char *table : {"BidTakeoffTotals", "BidTypicalGroupTotals"})
        {
            const std::string insert = std::string("INSERT INTO ") + table + " (UID, BidUID, BidConditionUID) VALUES ";
            db.must(insert + "(1, 1, 10)");
            db.must(insert + "(2, 2, 10)");
            db.must(insert + "(3, 1, 11)");
        }
        db.must("INSERT INTO BidConditionUser (UID, ConditionUID) VALUES (1, 10)");
        db.must("INSERT INTO BidConditionUser (UID, ConditionUID) VALUES (2, 11)");
        db.must("INSERT INTO ConditionSetStyles (UID, ConditionStyleUID) VALUES (1, 10)");
        db.must("INSERT INTO ConditionSetStyles (UID, ConditionStyleUID) VALUES (2, 11)");
    }

    const std::vector<std::string> &cascade_tables()
    {
        static const std::vector<std::string> tables = {
            "BidConditions", "BidTakeoffs", "BidDimensions", "BidALines", "BidArrows", "BidLaborActivity",
            "BidPercents", "BidTypGroupViews", "AffectDPCTypGroupViews", "BidTakeoffTotals",
            "BidTypicalGroupTotals", "BidConditionUser", "ConditionSetStyles"};
        return tables;
    }
}

// Port of tests/infrastructure/mdb/components/test_condition_operations.py::ConditionDeleteCascadeContractTests::
// test_deleting_a_condition_removes_every_dependent_of_that_condition_in_that_bid_only (also DBP-15 and QTY-M01 plans)
TEST(ConditionDelete, RemovesEveryDependentOfThatConditionInThatBidOnly)
{
    TempDb db("delete_cascade");
    REQUIRE(db.schema_ok());
    seed_cascade_world(db);

    CHECK(ost_store::delete_condition(db.path(), "1", "10"));

    CHECK_EQ(db.uids("BidConditions"), "11,20");
    CHECK_EQ(db.uids("BidTakeoffs"), "101,102,300");
    for (const char *table : {"BidDimensions", "BidALines", "BidArrows"})
    {
        SCOPED_CONTEXT(std::string("table ") + table);
        CHECK_EQ(db.uids(table), "3");
    }
    CHECK_EQ(db.uids("BidPercents"), "3");
    CHECK_EQ(db.uids("BidLaborActivity"), "2,3");
    CHECK_EQ(db.uids("BidTypGroupViews"), "2,3");
    CHECK_EQ(db.uids("AffectDPCTypGroupViews"), "2,3");
    for (const char *table : {"BidTakeoffTotals", "BidTypicalGroupTotals"})
    {
        SCOPED_CONTEXT(std::string("table ") + table);
        CHECK_EQ(db.uids(table), "2,3");
    }
    CHECK_EQ(db.uids("BidConditionUser"), "2");
    CHECK_EQ(db.uids("ConditionSetStyles"), "2");
    CHECK_EQ(db.text("SELECT quote(ParentUID) || quote(TypGroupTakeoffUID) || quote(TypPageTakeoffUID) || "
                     "quote(TypGroupMarkerUID) FROM BidTakeoffs WHERE UID = 101"),
             "NULLNULLNULLNULL");
    CHECK_EQ(db.text("SELECT quote(ParentUID) FROM BidTakeoffs WHERE UID = 102"), "101");
}

// The Windows dataset again, but deleting through the other bid's view of the data: nothing of bid 2 is touched and the
// condition of bid 1 stays when the delete names the wrong bid.
TEST(ConditionDelete, ADeleteNamingTheWrongBidChangesNothing)
{
    TempDb db("delete_wrong_bid");
    REQUIRE(db.schema_ok());
    seed_cascade_world(db);
    const std::string before = db.snapshot(cascade_tables());

    // Port of ...::test_unusable_requests_are_refused_by_name_when_the_policy_is_strict (condition 20 belongs to bid 2)
    CHECK_FALSE(ost_store::delete_condition(db.path(), "1", "20"));
    // Port of ...::test_condition_delete_rejects_cross_bid_batch_before_cascade (single condition of another bid)
    CHECK_FALSE(ost_store::delete_condition(db.path(), "2", "10"));
    // an unknown condition (Windows MissingBidOwnedUidError)
    CHECK_FALSE(ost_store::delete_condition(db.path(), "1", "99"));
    CHECK(db.snapshot(cascade_tables()) == before);
}

// Port of ...::test_unusable_requests_are_refused_by_name_when_the_policy_is_strict (uids that are not numbers)
TEST(ConditionDelete, UidsThatAreNotNumbersAreRefused)
{
    TempDb db("delete_bad_uids");
    REQUIRE(db.schema_ok());
    seed_cascade_world(db);
    const std::string before = db.snapshot(cascade_tables());

    CHECK_FALSE(ost_store::delete_condition(db.path(), "1", "x"));
    CHECK_FALSE(ost_store::delete_condition(db.path(), "x", "10"));
    CHECK_FALSE(ost_store::delete_condition(db.path(), "1", ""));
    CHECK_FALSE(ost_store::delete_condition(db.path(), "", "10"));
    CHECK(db.snapshot(cascade_tables()) == before);
}

// Port of ...::ConditionOperationsPersistenceTests::test_delete_condition_removes_all_ancillary_condition_dependents:
// the foreign keys of the children (enforced by every connection of the store) force the Windows deletion order.
TEST(ConditionDelete, RemovesAncillaryDependentsInForeignKeyOrder)
{
    TempDb db("delete_fk_order", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY)");
    db.must("INSERT INTO Bids VALUES (1)");
    db.must("CREATE TABLE BidConditions (UID INTEGER PRIMARY KEY, BidUID INTEGER)");
    db.must("CREATE TABLE BidLaborActivity (UID INTEGER PRIMARY KEY, BidUID INTEGER, "
            "BidConditionUID INTEGER REFERENCES BidConditions(UID))");
    db.must("CREATE TABLE BidPercents (UID INTEGER PRIMARY KEY, BidLaborActivityUID INTEGER "
            "REFERENCES BidLaborActivity(UID))");
    db.must("CREATE TABLE BidTypGroupViews (UID INTEGER PRIMARY KEY, BidUID INTEGER, "
            "BidConditionUID INTEGER REFERENCES BidConditions(UID))");
    db.must("CREATE TABLE AffectDPCTypGroupViews (UID INTEGER PRIMARY KEY, BidTypGroupViewUID INTEGER "
            "REFERENCES BidTypGroupViews(UID))");
    db.must("INSERT INTO BidConditions (UID, BidUID) VALUES (10, 1)");
    db.must("INSERT INTO BidConditions (UID, BidUID) VALUES (11, 1)");
    for (const char *table : {"BidTakeoffTotals", "BidTypicalGroupTotals"})
    {
        db.must(std::string("CREATE TABLE ") + table + " (UID INTEGER PRIMARY KEY, BidUID INTEGER, "
                "BidConditionUID INTEGER REFERENCES BidConditions(UID))");
    }
    db.must("INSERT INTO BidTakeoffTotals (UID, BidUID, BidConditionUID) VALUES (100, 1, 10)");
    db.must("INSERT INTO BidTakeoffTotals (UID, BidUID, BidConditionUID) VALUES (300, 1, 11)");
    db.must("INSERT INTO BidTypicalGroupTotals (UID, BidUID, BidConditionUID) VALUES (101, 1, 10)");
    db.must("INSERT INTO BidTypicalGroupTotals (UID, BidUID, BidConditionUID) VALUES (301, 1, 11)");
    db.must("INSERT INTO BidLaborActivity (UID, BidUID, BidConditionUID) VALUES (20, 1, 10)");
    db.must("INSERT INTO BidLaborActivity (UID, BidUID, BidConditionUID) VALUES (21, 1, 11)");
    db.must("INSERT INTO BidPercents (UID, BidLaborActivityUID) VALUES (30, 20)");
    db.must("INSERT INTO BidPercents (UID, BidLaborActivityUID) VALUES (31, 21)");
    db.must("INSERT INTO BidTypGroupViews (UID, BidUID, BidConditionUID) VALUES (40, 1, 10)");
    db.must("INSERT INTO BidTypGroupViews (UID, BidUID, BidConditionUID) VALUES (41, 1, 11)");
    db.must("INSERT INTO AffectDPCTypGroupViews (UID, BidTypGroupViewUID) VALUES (50, 40)");
    db.must("INSERT INTO AffectDPCTypGroupViews (UID, BidTypGroupViewUID) VALUES (51, 41)");

    CHECK(ost_store::delete_condition(db.path(), "1", "10"));

    CHECK_EQ(db.uids("BidPercents"), "31");
    CHECK_EQ(db.uids("BidLaborActivity"), "21");
    CHECK_EQ(db.uids("AffectDPCTypGroupViews"), "51");
    CHECK_EQ(db.uids("BidTypGroupViews"), "41");
    CHECK_EQ(db.uids("BidTakeoffTotals"), "300");
    CHECK_EQ(db.uids("BidTypicalGroupTotals"), "301");
    CHECK_EQ(db.uids("BidConditions"), "11");
}

// Port of ...::test_condition_delete_clears_surviving_takeoff_self_references
TEST(ConditionDelete, ClearsTheSelfReferencesOfSurvivingTakeoffs)
{
    TempDb db("delete_self_refs", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY)");
    db.must("INSERT INTO Bids VALUES (1)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER)");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidConditionUID INTEGER, ParentUID INTEGER, "
            "TypGroupTakeoffUID INTEGER, TypPageTakeoffUID INTEGER, TypGroupMarkerUID INTEGER)");
    db.must("INSERT INTO BidConditions VALUES (10, 1)");
    db.must("INSERT INTO BidConditions VALUES (11, 1)");
    db.must("INSERT INTO BidTakeoffs VALUES (70, 1, 10, NULL, NULL, NULL, NULL)");
    db.must("INSERT INTO BidTakeoffs VALUES (80, 1, 11, 70, 70, 70, 70)");
    db.must("INSERT INTO BidTakeoffs VALUES (81, 1, 11, 80, 80, 80, 80)");

    CHECK(ost_store::delete_condition(db.path(), "1", "10"));

    CHECK_EQ(db.list("SELECT UID || ':' || quote(ParentUID) || quote(TypGroupTakeoffUID) || quote(TypPageTakeoffUID) || "
                     "quote(TypGroupMarkerUID) FROM BidTakeoffs ORDER BY UID"),
             "80:NULLNULLNULLNULL,81:80808080");
}

// Port of ...::test_condition_delete_removes_condition_user_companion_rows
TEST(ConditionDelete, RemovesTheConditionUserCompanionRows)
{
    TempDb db("delete_condition_user", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY)");
    db.must("INSERT INTO Bids VALUES (1)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER)");
    db.must("CREATE TABLE BidConditionUser (UID INTEGER, BidUID INTEGER, ConditionUID INTEGER)");
    db.must("INSERT INTO BidConditions VALUES (10, 1)");
    db.must("INSERT INTO BidConditions VALUES (11, 1)");
    db.must("INSERT INTO BidConditionUser VALUES (20, 1, 10)");
    db.must("INSERT INTO BidConditionUser VALUES (21, 1, 11)");

    CHECK(ost_store::delete_condition(db.path(), "1", "10"));

    CHECK_EQ(db.list("SELECT UID || ',' || BidUID || ',' || ConditionUID FROM BidConditionUser"), "21,1,11");
    CHECK_EQ(db.list("SELECT UID || ',' || BidUID FROM BidConditions"), "11,1");
}

// Port of ...::test_condition_delete_removes_annotations_linked_by_to_takeoff
TEST(ConditionDelete, RemovesAnnotationsLinkedByTheToTakeoff)
{
    TempDb db("delete_to_links", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY)");
    db.must("INSERT INTO Bids VALUES (1)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER)");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidConditionUID INTEGER)");
    db.must("CREATE TABLE BidDimensions (UID INTEGER, BidTakeoffFromUID INTEGER, BidTakeoffToUID INTEGER)");
    db.must("INSERT INTO BidConditions VALUES (10, 1)");
    db.must("INSERT INTO BidConditions VALUES (11, 1)");
    db.must("INSERT INTO BidTakeoffs VALUES (70, 1, 10)");
    db.must("INSERT INTO BidTakeoffs VALUES (80, 1, 11)");
    db.must("INSERT INTO BidDimensions VALUES (90, 80, 70)");
    db.must("INSERT INTO BidDimensions VALUES (91, 70, 80)");
    db.must("INSERT INTO BidDimensions VALUES (92, 80, 80)");

    CHECK(ost_store::delete_condition(db.path(), "1", "10"));

    CHECK_EQ(db.list("SELECT UID || ',' || BidTakeoffFromUID || ',' || BidTakeoffToUID FROM BidDimensions"), "92,80,80");
    CHECK_EQ(db.uids("BidTakeoffs"), "80");
}

// Port of ...::test_condition_delete_rejects_duplicate_physical_uid_before_cascade (a condition uid held by two rows)
TEST(ConditionDelete, RejectsAConditionUidThatExistsTwice)
{
    TempDb db("delete_dup_condition", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY)");
    db.must("INSERT INTO Bids VALUES (1)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER)");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidConditionUID INTEGER)");
    db.must("INSERT INTO BidConditions VALUES (7, 1)");
    db.must("INSERT INTO BidConditions VALUES (7, 1)");
    db.must("INSERT INTO BidTakeoffs VALUES (70, 1, 7)");

    CHECK_FALSE(ost_store::delete_condition(db.path(), "1", "7"));

    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 2);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 1);
}

// Port of ...::ConditionEdgeContractTests::test_a_takeoff_uid_that_exists_twice_stops_the_condition_delete_before_anything_is_removed
TEST(ConditionDelete, RejectsATakeoffUidThatExistsTwice)
{
    TempDb db("delete_dup_takeoff", false);
    db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY)");
    db.must("INSERT INTO Bids VALUES (1)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, RefNo INTEGER, Name TEXT, Type INTEGER)");
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidConditionUID INTEGER)");
    db.must("INSERT INTO BidTakeoffs VALUES (70, 1, 10)");
    db.must("INSERT INTO BidTakeoffs VALUES (70, 1, 10)");
    db.must("INSERT INTO BidConditions VALUES (10, 1, 1, 'c', 0)");

    CHECK_FALSE(ost_store::delete_condition(db.path(), "1", "10"));

    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 2);
}

// Port of ...::ConditionOperationsPersistenceTests::test_orphan_condition_edit_and_delete_reject_before_mutation
// (the delete part): the bid the delete names has no Bids row.
TEST(ConditionDelete, RejectsABidThatHasNoRow)
{
    TempDb db("delete_orphan_bid", false);
    db.must("CREATE TABLE Bids (UID INTEGER)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, Name TEXT, RefNo INTEGER)");
    db.must("INSERT INTO BidConditions VALUES (7, 99, 'Orphan condition', 4)");

    CHECK_FALSE(ost_store::delete_condition(db.path(), "99", "7"));

    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 1);
}

// Port of ...::ConditionDeleteCascadeContractTests::test_dependent_tables_that_cannot_be_matched_are_left_alone
// (a dependent table that has only a UID column is left alone; BidTakeoffs is the one table that must be matchable)
TEST(ConditionDelete, DependentTablesThatCannotBeMatchedAreLeftAlone)
{
    const char *tables[] = {"BidTakeoffs", "BidDimensions", "BidALines", "BidArrows", "BidPercents", "BidLaborActivity",
                            "BidTypGroupViews", "AffectDPCTypGroupViews", "BidTakeoffTotals", "BidTypicalGroupTotals",
                            "BidConditionUser", "ConditionSetStyles"};
    for (const char *table : tables)
    {
        SCOPED_CONTEXT(std::string("table ") + table);
        TempDb db(std::string("delete_unmatchable_") + table, false);
        db.must("CREATE TABLE Bids (UID INTEGER, MeasureBase INTEGER)");
        db.must("INSERT INTO Bids VALUES (1, 0)");
        db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, RefNo INTEGER, Name TEXT, Type INTEGER)");
        db.must(std::string("CREATE TABLE ") + table + " (UID INTEGER)");
        db.must(std::string("INSERT INTO ") + table + " VALUES (1)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 1, 'c', 0)");

        if (std::string(table) == "BidTakeoffs")
        {
            CHECK_FALSE(ost_store::delete_condition(db.path(), "1", "10"));
            CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 1);
        }
        else
        {
            CHECK(ost_store::delete_condition(db.path(), "1", "10"));
            CHECK_EQ(db.uids(table), "1");
            CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 0);
        }
    }
}

// Port of ...::test_a_schema_with_only_the_condition_table_deletes_the_condition
TEST(ConditionDelete, ASchemaWithOnlyTheConditionTableDeletesTheCondition)
{
    TempDb db("delete_only_conditions", false);
    db.must("CREATE TABLE Bids (UID INTEGER, MeasureBase INTEGER)");
    db.must("INSERT INTO Bids VALUES (1, 0)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, RefNo INTEGER, Name TEXT, Type INTEGER)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 1, 'c', 0)");

    CHECK(ost_store::delete_condition(db.path(), "1", "10"));

    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 0);
}

// Port of ...::test_a_failing_style_cleanup_is_reported_but_never_stops_the_delete (the Windows run logs a warning)
TEST(ConditionDelete, AFailingStyleCleanupNeverStopsTheDelete)
{
    TempDb db("delete_style_failure");
    REQUIRE(db.schema_ok());
    seed_cascade_world(db);
    db.block("ConditionSetStyles", "DELETE");

    CHECK(ost_store::delete_condition(db.path(), "1", "10"));

    CHECK_EQ(db.uids("BidConditions"), "11,20");
    CHECK_EQ(db.uids("BidTakeoffs"), "101,102,300");
    CHECK_EQ(db.uids("ConditionSetStyles"), "1,2"); // the cleanup itself failed, the rest was done
}

// The self-reference clearing, the cascade and the condition delete are ONE transaction (Windows runs them on one
// connection and commits once): a statement that fails at the very end undoes the clearing as well.
TEST(ConditionDelete, AFailedStatementUndoesTheSelfReferenceClearingToo)
{
    TempDb db("delete_atomic");
    REQUIRE(db.schema_ok());
    seed_cascade_world(db);
    db.block("BidConditions", "DELETE");
    const std::string before = db.snapshot(cascade_tables());

    CHECK_FALSE(ost_store::delete_condition(db.path(), "1", "10"));

    CHECK(db.snapshot(cascade_tables()) == before);
    CHECK_EQ(db.text("SELECT quote(ParentUID) FROM BidTakeoffs WHERE UID = 101"), "100");
}

// The new cascade statements each fail the whole delete (none is best effort except ConditionSetStyles).
TEST(ConditionDelete, AFailureInAnyDependentCleanupFailsTheDeleteAndRollsBack)
{
    const char *tables[] = {"BidTypGroupViews", "AffectDPCTypGroupViews", "BidTakeoffTotals", "BidTypicalGroupTotals",
                            "BidConditionUser", "BidPercents", "BidDimensions", "BidALines", "BidArrows",
                            "BidLaborActivity", "BidTakeoffs"};
    for (const char *table : tables)
    {
        SCOPED_CONTEXT(std::string("blocked delete on ") + table);
        TempDb db(std::string("delete_blocked_") + table);
        REQUIRE(db.schema_ok());
        seed_cascade_world(db);
        db.block(table, "DELETE");
        const std::string before = db.snapshot(cascade_tables());

        CHECK_FALSE(ost_store::delete_condition(db.path(), "1", "10"));
        CHECK(db.snapshot(cascade_tables()) == before);
    }
}

// The self-reference clearing is an UPDATE of the takeoffs: when it fails the delete fails (Windows raises).
TEST(ConditionDelete, AFailingSelfReferenceUpdateFailsTheDelete)
{
    TempDb db("delete_blocked_update");
    REQUIRE(db.schema_ok());
    seed_cascade_world(db);
    db.block("BidTakeoffs", "UPDATE");
    const std::string before = db.snapshot(cascade_tables());

    CHECK_FALSE(ost_store::delete_condition(db.path(), "1", "10"));
    CHECK(db.snapshot(cascade_tables()) == before);
}

// Windows runs no self-reference UPDATE when the condition has no takeoffs: a blocked UPDATE does not matter then.
TEST(ConditionDelete, NoSelfReferenceUpdateRunsForAConditionWithoutTakeoffs)
{
    TempDb db("delete_no_takeoffs");
    REQUIRE(db.schema_ok());
    seed_cascade_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (12, 1, 2, 'empty', 0)");
    db.block("BidTakeoffs", "UPDATE");

    CHECK(ost_store::delete_condition(db.path(), "1", "12"));
    CHECK_EQ(db.uids("BidConditions"), "10,11,20");
}

// QTY-M01: after the delete the surviving takeoffs whose parent was removed load as ROOT takeoffs (parent "0"), exactly
// like the in-memory project looks after the delete (ProjectData::remove_condition).
TEST(ConditionDelete, SurvivingChildrenLoadAsRootTakeoffs)
{
    TempDb db("delete_load_roots");
    REQUIRE(db.schema_ok());
    seed_cascade_world(db);
    // The bid has to open: since DBP-31 a takeoff must lie on a page of the bid (the cascade world has none, the delete does not care).
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (500, 1, 'P500')");
    db.must("UPDATE BidTakeoffs SET BidPageUID = 500 WHERE BidUID = 1");

    ost_data::ProjectData in_memory = ost_store::load_project(db.path(), "1");
    REQUIRE(!in_memory.empty());
    REQUIRE_EQ(in_memory.takeoffs.size(), size_t(3)); // 100, 101, 102 (300 belongs to bid 2)

    CHECK(ost_store::delete_condition(db.path(), "1", "10"));
    in_memory.remove_condition("10");

    ost_data::ProjectData reloaded = ost_store::load_project(db.path(), "1");
    REQUIRE_EQ(reloaded.takeoffs.size(), size_t(2));
    CHECK_EQ(reloaded.conditions.count("10"), size_t(0));

    auto parent_of = [](const ost_data::ProjectData &project, const std::string &uid) -> std::string
    {
        for (const ost_domain::Takeoff &takeoff : project.takeoffs)
            if (takeoff.uid == uid)
                return takeoff.parent_uid;
        return "<missing>";
    };
    CHECK_EQ(parent_of(reloaded, "101"), "0");
    CHECK_EQ(parent_of(reloaded, "102"), "101");
    CHECK_EQ(parent_of(reloaded, "100"), "<missing>");
    // the in-memory result of the delete equals the database result
    CHECK_EQ(in_memory.takeoffs.size(), reloaded.takeoffs.size());
    CHECK_EQ(parent_of(in_memory, "101"), "0");
    CHECK_EQ(parent_of(in_memory, "102"), "101");
    CHECK_EQ(parent_of(in_memory, "100"), "<missing>");
    CHECK_EQ(in_memory.conditions.count("10"), size_t(0));
    CHECK_EQ(in_memory.conditions.count("11"), size_t(1));
    for (const ost_domain::Takeoff &takeoff : in_memory.takeoffs)
        if (takeoff.uid == "101")
            CHECK_FALSE(takeoff.is_hole());
}

// In-memory only (no database): the takeoffs of the removed condition go, and every survivor that named one of them as
// its parent becomes a root takeoff; a survivor of another parent keeps it.
TEST(ConditionDelete, RemoveConditionDetachesTheChildrenOfTheRemovedTakeoffs)
{
    ost_data::ProjectData project;
    for (const char *uid : {"10", "11", "12"})
    {
        ost_domain::Condition condition;
        condition.uid = uid;
        project.conditions[uid] = condition;
    }
    auto add = [&project](const char *uid, const char *condition, const char *parent)
    {
        ost_domain::Takeoff takeoff;
        takeoff.uid = uid;
        takeoff.condition_uid = condition;
        takeoff.parent_uid = parent;
        project.takeoffs.push_back(takeoff);
    };
    add("1", "10", "0");
    add("2", "10", "1");
    add("3", "11", "1");  // child of a removed takeoff of ANOTHER condition
    add("4", "11", "3");  // child of a survivor
    add("5", "12", "0");

    project.remove_condition("10");

    CHECK_EQ(project.conditions.size(), size_t(2));
    REQUIRE_EQ(project.takeoffs.size(), size_t(3));
    CHECK_EQ(project.takeoffs[0].uid, "3");
    CHECK_EQ(project.takeoffs[0].parent_uid, "0");
    CHECK_EQ(project.takeoffs[1].uid, "4");
    CHECK_EQ(project.takeoffs[1].parent_uid, "3");
    CHECK_EQ(project.takeoffs[2].uid, "5");
    CHECK_EQ(project.takeoffs[2].parent_uid, "0");
}
