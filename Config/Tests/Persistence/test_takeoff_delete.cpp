// DBP-14 (+UND-05): delete_takeoff against the Windows delete_takeoffs (TakeoffOperationsMixin._run_delete_takeoffs).
// Windows: the target must exist and belong to one bid; annotation rows (BidDimensions / BidALines / BidArrows) that start OR end at
// the takeoff and the takeoff's BidPercents are deleted; every surviving takeoff that referenced it through ParentUID,
// TypGroupTakeoffUID, TypPageTakeoffUID or TypGroupMarkerUID is set to NULL (holes survive as roots); then the takeoff is deleted.
// Windows oracle: tests/infrastructure/mdb/components/test_takeoff_operations.py:183-308 (test_takeoff_delete_clears_all_surviving_
// self_references, test_delete_parent_takeoff_clears_child_parent_uid, test_delete_takeoff_removes_annotations_linked_by_either_endpoint).
#include "StoreTestSupport.hpp"

#include <string>

using ost_ptest::TempDb;

namespace
{
    void seed_bid(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'job')");
    }

    // "uid:Parent,TypGroupTakeoff,TypPageTakeoff,TypGroupMarker" with N for NULL, one row per takeoff of bid 1.
    std::string self_references(const TempDb &db)
    {
        return ost_ptest::list(db,
                               "SELECT UID || ':' || ifnull(ParentUID, 'N') || ',' || ifnull(TypGroupTakeoffUID, 'N') || ',' || "
                               "ifnull(TypPageTakeoffUID, 'N') || ',' || ifnull(TypGroupMarkerUID, 'N') FROM BidTakeoffs ORDER BY UID");
    }
} // namespace

// Port of test_takeoff_delete_clears_all_surviving_self_references: rows (7; NULL), (8; 7,7,7,7), (9; 8,8,8,8), (10; 7,8,7,8);
// deleting 7 leaves (8; NULLs), (9; 8,8,8,8) and (10; NULL,8,NULL,8).
TEST(Dbp14_DeleteTakeoff, ClearsEverySurvivingSelfReference)
{
    TempDb db("deltk_self_refs");
    seed_bid(db);
    const char *rows[] = {"(7, 1, NULL, NULL, NULL, NULL)", "(8, 1, 7, 7, 7, 7)", "(9, 1, 8, 8, 8, 8)", "(10, 1, 7, 8, 7, 8)"};
    for (const char *row : rows)
        db.must(std::string("INSERT INTO BidTakeoffs (UID, BidUID, ParentUID, TypGroupTakeoffUID, TypPageTakeoffUID, TypGroupMarkerUID) VALUES ") + row);

    CHECK(ost_store::delete_takeoff(db.path(), "7"));
    CHECK_STR_EQ(self_references(db), "8:N,N,N,N,9:8,8,8,8,10:N,8,N,8");
}

// Port of test_delete_parent_takeoff_clears_child_parent_uid: (1 root), (2 child of 1), (3 root), (4 child of 3); deleting 1
// leaves [(2, NULL), (3, NULL), (4, 3)] - the hole is detached, not deleted with its parent.
TEST(Dbp14_DeleteTakeoff, DeletingAParentDetachesItsHolesInsteadOfDeletingThem)
{
    TempDb db("deltk_parent");
    seed_bid(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, ParentUID) VALUES (1, 1, NULL)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, ParentUID) VALUES (2, 1, 1)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, ParentUID) VALUES (3, 1, NULL)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, ParentUID) VALUES (4, 1, 3)");

    CHECK(ost_store::delete_takeoff(db.path(), "1"));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(ParentUID, 'N') FROM BidTakeoffs ORDER BY UID"), "2:N,3:N,4:3");
}

// The same with the parent written as 0 (what insert_takeoff stores for a root since DBP-19): the survivors' ParentUID is NULL, the
// loader reads both as "0".
TEST(Dbp14_DeleteTakeoff, ADetachedHoleReadsBackAsARoot)
{
    TempDb db("deltk_detach_load");
    seed_bid(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (2, 1, 'other')");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (20, 1, 'P')");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (30, 1, 1, 'C', 1)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position) VALUES (1, 1, 30, 20, 0, '0;0;9;0;9;9')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position) VALUES (2, 1, 30, 20, 1, '1;1;2;1;2;2')");
    CHECK(ost_store::delete_takeoff(db.path(), "1"));
    const ost_data::ProjectData project = ost_store::load_project(db.path(), "1");
    REQUIRE_EQ(project.takeoffs.size(), static_cast<size_t>(1));
    CHECK_STR_EQ(project.takeoffs[0].uid, "2");
    CHECK_STR_EQ(project.takeoffs[0].parent_uid, "0");
    CHECK_FALSE(project.takeoffs[0].is_hole());
}

// Port of test_delete_takeoff_removes_annotations_linked_by_either_endpoint: takeoffs 1, 2, 3; each annotation table holds
// (x1: 1 -> 2), (x2: 2 -> 3), (x3: 1 -> 3); deleting 2 removes the rows that end at it and the rows that start at it; only the link
// between the two survivors remains.
TEST(Dbp14_DeleteTakeoff, RemovesAnnotationsLinkedByEitherEndpoint)
{
    TempDb db("deltk_annotations");
    seed_bid(db);
    for (int uid : {1, 2, 3})
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, ParentUID) VALUES (" + std::to_string(uid) + ", 1, NULL)");
    int index = 1;
    const char *tables[] = {"BidDimensions", "BidALines", "BidArrows"};
    for (const char *table : tables)
    {
        const std::string t = table;
        const std::string base = std::to_string(index * 10);
        db.must("INSERT INTO " + t + " (UID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (" + std::to_string(index * 10 + 1) + ", 1, 2)");
        db.must("INSERT INTO " + t + " (UID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (" + std::to_string(index * 10 + 2) + ", 2, 3)");
        db.must("INSERT INTO " + t + " (UID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (" + std::to_string(index * 10 + 3) + ", 1, 3)");
        ++index;
    }
    CHECK(ost_store::delete_takeoff(db.path(), "2"));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidTakeoffs ORDER BY UID"), "1,3");
    index = 1;
    for (const char *table : tables)
    {
        SCOPED_CONTEXT(std::string("annotation table: ") + table);
        CHECK_STR_EQ(ost_ptest::list(db, std::string("SELECT UID || ':' || BidTakeoffFromUID || '>' || BidTakeoffToUID FROM ") + table +
                                             " ORDER BY UID"),
                     std::to_string(index * 10 + 3) + ":1>3");
        ++index;
    }
}

// UND-05: a dimension from takeoff 1 to takeoff 2 goes when takeoff 2 (the 'To' end) is deleted; the percents of the takeoff go too,
// the rows of other takeoffs stay.
TEST(Dbp14_DeleteTakeoff, RemovesToEndAnnotationsAndPercentsOfTheTakeoffOnly)
{
    TempDb db("deltk_to_end");
    seed_bid(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID) VALUES (1, 1)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID) VALUES (2, 1)");
    db.must("INSERT INTO BidDimensions (UID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (5, 1, 2)");
    db.must("INSERT INTO BidPercents (UID, BidTakeoffUID) VALUES (6, 2)");
    db.must("INSERT INTO BidPercents (UID, BidTakeoffUID) VALUES (7, 1)");
    CHECK(ost_store::delete_takeoff(db.path(), "2"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidDimensions"), 0);
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidPercents ORDER BY UID"), "7");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidTakeoffs ORDER BY UID"), "1");
}

// The takeoffs and annotations of another bid are never touched, even when they reference the deleted UID by accident.
TEST(Dbp14_DeleteTakeoff, LeavesTheRowsOfOtherBidsAlone)
{
    TempDb db("deltk_other_bid");
    seed_bid(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (2, 1, 'other')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID) VALUES (1, 1)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, ParentUID) VALUES (9, 2, NULL)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID) VALUES (10, 2)");
    db.must("INSERT INTO BidDimensions (UID, BidUID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (5, 2, 9, 10)");
    CHECK(ost_store::delete_takeoff(db.path(), "1"));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidTakeoffs ORDER BY UID"), "9,10");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidDimensions"), 1);
}

// Windows require_single_bid_scope_for_uids: a takeoff that does not exist, has no bid, or whose bid does not exist is not deleted
// (False, nothing written). A uid that is not an integer is rejected too.
TEST(Dbp14_DeleteTakeoff, UnknownOrOwnerlessTargetsReturnFalseAndWriteNothing)
{
    TempDb db("deltk_unknown");
    seed_bid(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID) VALUES (1, 1)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID) VALUES (2, NULL)"); // no bid
    db.must("INSERT INTO BidTakeoffs (UID, BidUID) VALUES (3, 77)");   // bid 77 does not exist
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, ParentUID) VALUES (4, 1, 3)");
    db.must("INSERT INTO BidDimensions (UID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (5, 2, 3)");
    const std::string before = db.snapshot({"BidTakeoffs", "BidDimensions", "BidPercents"});

    CHECK_FALSE(ost_store::delete_takeoff(db.path(), "999"));
    CHECK_FALSE(ost_store::delete_takeoff(db.path(), "2"));
    CHECK_FALSE(ost_store::delete_takeoff(db.path(), "3"));
    CHECK_FALSE(ost_store::delete_takeoff(db.path(), "abc"));
    CHECK_FALSE(ost_store::delete_takeoff(db.path(), ""));
    CHECK_FALSE(ost_store::delete_takeoff(db.path(), "-1"));
    CHECK_EQ(db.snapshot({"BidTakeoffs", "BidDimensions", "BidPercents"}), before);

    CHECK(ost_store::delete_takeoff(db.path(), "1")); // control
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidTakeoffs ORDER BY UID"), "2,3,4");
}
