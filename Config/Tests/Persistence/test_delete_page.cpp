// M1 Delete Page (plan 5.6): the Windows tests of delete_pages ported to ost_store::delete_pages, and the whole-database oracle of the
// cascade. Windows 49a2d59: settings_operations.py:562-577 (delete_pages) and :698-891 (_delete_pages_cascade).
//
// Windows tests ported here (the Windows test double _SqliteMdbOps runs the TOLERANT policy and derives the bid from the pages; every port
// keeps the inputs and the expected rows and says in a comment where the iOS call differs):
//   tests/infrastructure/mdb/components/test_settings_operations.py  SettingsOperationsDeletePagesTests (:240-662), :3401, :3516-3566,
//                                                                    :6274-6302 (strict half only), :6579, :6612-6642, :6644-6672
//   tests/integration/mdb/test_ownership_and_atomicity.py            :51 (pages row), :154-220 (pages part), :448-473
// What differs from every Windows test, on purpose:
//   - iOS takes the bid as an argument (register DP-6): every call names the bid the pages belong to.
//   - the Core refuses the LAST page of a bid (register DP-5, N-M1-23): a Windows test that deleted the only page of its bid gets a second
//     page of the same bid here, so that the refusal under test is the one Windows has; the same inputs without the extra page are
//     pinned as refusals in test_delete_page_refusals.cpp.
//   - the Windows tests with REFERENCES declare foreign keys; the iOS DDL has none, so the order is pinned with a trigger that fails the
//     parent delete while a child still references it (DeletePageTestSupport.hpp forbid_orphans).
//   - the bid lock reads Bids.JobStatusUID and fails closed when it is missing (plan 4.1): the loose Bids tables get that column.
// The oracle tests at the end seed a whole world, run the REAL Windows delete_pages on it (DeletePageOracleData.hpp) and compare every table.
#include "DeletePageTestSupport.hpp"

#include <initializer_list>
#include <string>

using ost_delpage::run;
using ost_delpage::Uids;
using ost_ptest::TempDb;

namespace
{
    // Bid 1 of the Windows tests (their `Bids (UID)` row); the iOS schema has the lock column and a project.
    void seed_bid_1(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (1, 1, 'job')");
    }

    void add_pages(const TempDb &db, std::initializer_list<int> uids, int bid = 1)
    {
        for (const int uid : uids)
            db.must("INSERT INTO BidPages (UID, BidUID) VALUES (" + std::to_string(uid) + ", " + std::to_string(bid) + ")");
    }

    std::string page_uids(const TempDb &db)
    {
        return ost_ptest::list(db, "SELECT UID FROM BidPages ORDER BY UID");
    }

    // The tables of PAGE_DELETE_CHILD_TABLES (the Windows constants, from the oracle) as a vector of std::string.
    Uids child_tables()
    {
        Uids out;
        for (const char *table : ost_delpage_oracle::page_delete_child_tables())
            out.push_back(table);
        return out;
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------------------------
// SettingsOperationsDeletePagesTests (test_settings_operations.py:240-662)
// ---------------------------------------------------------------------------------------------------------------------------------

// Port of test_settings_operations.py:243 test_delete_page_clears_bid_settings_selected_page_before_page_delete: pages 10, 11; settings 2
// (page 10), 3 (page 11); delete ["10"]; pages [11], settings [(2, NULL), (3, 11)]. Windows declares FOREIGN KEYs, so the selected page
// has to be cleared BEFORE the page row goes: here a trigger fails the page delete while a setting still names it.
TEST(DelPageWindows, ClearsTheSelectedPageBeforeThePageDelete)
{
    TempDb db("delpage_win_selected");
    seed_bid_1(db);
    add_pages(db, {10, 11});
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (2, 1, 10)");
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (3, 1, 11)");
    ost_delpage::forbid_orphans(db, "BidPages", "BidSettings", "BidPageSelectedUID");

    std::string error;
    CHECK(run(db, "1", {"10"}, error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(page_uids(db), "11");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(BidPageSelectedUID, 'N') FROM BidSettings ORDER BY UID"), "2:N,3:11");
}

// Port of :284 test_delete_page_clears_only_page_typed_cover_sheet_selection: pages 10, 11; Bids (1, type 1, uid 10), (2, type 2, uid 10),
// (3, type 1, uid 11); delete ["10"] gives (1, 1, NULL), (2, 2, 10), (3, 1, 11): only the selection of TYPE 1 (a page) is cleared.
TEST(DelPageWindows, ClearsOnlyThePageTypedCoverSheetSelection)
{
    TempDb db("delpage_win_cover");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, CoverSheetSelItemType, CoverSheetSelItemUID) VALUES (1, 1, 'a', 1, 10)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, CoverSheetSelItemType, CoverSheetSelItemUID) VALUES (2, 1, 'b', 2, 10)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, CoverSheetSelItemType, CoverSheetSelItemUID) VALUES (3, 1, 'c', 1, 11)");
    add_pages(db, {10, 11});

    CHECK(run(db, "1", {"10"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || CoverSheetSelItemType || ':' || ifnull(CoverSheetSelItemUID, 'N') FROM Bids WHERE UID <= 3 ORDER BY UID"),
                 "1:1:N,2:2:10,3:1:11");
}

// Port of :318 test_delete_pages_preserves_conditions_uoms_and_remaining_takeoffs: pages 10, 11, 12; conditions (20, UOM1 7), (21, UOM1 9);
// takeoffs (30; page 10, cond 20), (31; page 11, cond 20), (32; page 12, cond 21); delete ["10", "12"]: the conditions are unchanged,
// only takeoff 31 is left, only page 11 is left.
TEST(DelPageWindows, PreservesConditionsUomsAndRemainingTakeoffs)
{
    TempDb db("delpage_win_conditions");
    seed_bid_1(db);
    add_pages(db, {10, 11, 12});
    db.must("INSERT INTO BidConditions (UID, BidUID, UOM1) VALUES (20, 1, 7)");
    db.must("INSERT INTO BidConditions (UID, BidUID, UOM1) VALUES (21, 1, 9)");
    db.must("INSERT INTO BidTakeoffs (UID, BidPageUID, BidConditionUID) VALUES (30, 10, 20)");
    db.must("INSERT INTO BidTakeoffs (UID, BidPageUID, BidConditionUID) VALUES (31, 11, 20)");
    db.must("INSERT INTO BidTakeoffs (UID, BidPageUID, BidConditionUID) VALUES (32, 12, 21)");

    CHECK(run(db, "1", {"10", "12"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || UOM1 FROM BidConditions ORDER BY UID"), "20:7,21:9");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || BidPageUID || ':' || BidConditionUID FROM BidTakeoffs"), "31:11:20");
    CHECK_STR_EQ(page_uids(db), "11");
}

// Port of :364 test_delete_page_removes_indexed_annotation_shape_rows: BidAnnotationRects 20 (page 10), 21 (page 11), layer 30; delete
// ["10"] leaves (21, 11) and page 11.
TEST(DelPageWindows, RemovesIndexedAnnotationShapeRows)
{
    TempDb db("delpage_win_rects");
    seed_bid_1(db);
    add_pages(db, {10, 11});
    db.must("INSERT INTO BidAnnotationRects (UID, BidPageUID, BidLayerUID) VALUES (20, 10, 30)");
    db.must("INSERT INTO BidAnnotationRects (UID, BidPageUID, BidLayerUID) VALUES (21, 11, 30)");

    CHECK(run(db, "1", {"10"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || BidPageUID FROM BidAnnotationRects"), "21:11");
    CHECK_STR_EQ(page_uids(db), "11");
}

// Port of :394 test_delete_page_removes_all_ancillary_page_dependents: BidTypGroupViews 20 (page 10), 21 (page 11); AffectDPCTypGroupViews 30
// (view 20), 31 (view 21); BidPercents, BidTakeoffTotals, BidLaborCostCodeTotals, BidTypicalGroupTotals, Boost, DPCCalcFilter one row on page
// 10 and one on page 11 (uid 100 + len(name) / 200 + len(name)); delete ["10"]: only the page-11 rows are left. Windows has foreign keys
// (the child rows reference the page, the Affect rows reference the view): the triggers pin the order.
TEST(DelPageWindows, RemovesAllAncillaryPageDependents)
{
    TempDb db("delpage_win_ancillary");
    seed_bid_1(db);
    add_pages(db, {10, 11});
    const char *tables[] = {"BidPercents", "BidTakeoffTotals", "BidLaborCostCodeTotals", "BidTypicalGroupTotals", "Boost", "DPCCalcFilter"};
    for (const char *table : tables)
    {
        const std::string name = table;
        db.must("INSERT INTO [" + name + "] (UID, BidPageUID) VALUES (" + std::to_string(100 + name.size()) + ", 10)");
        db.must("INSERT INTO [" + name + "] (UID, BidPageUID) VALUES (" + std::to_string(200 + name.size()) + ", 11)");
        ost_delpage::forbid_orphans(db, "BidPages", name, "BidPageUID");
    }
    db.must("INSERT INTO BidTypGroupViews (UID, BidPageUID) VALUES (20, 10)");
    db.must("INSERT INTO BidTypGroupViews (UID, BidPageUID) VALUES (21, 11)");
    db.must("INSERT INTO AffectDPCTypGroupViews (UID, BidTypGroupViewUID) VALUES (30, 20)");
    db.must("INSERT INTO AffectDPCTypGroupViews (UID, BidTypGroupViewUID) VALUES (31, 21)");
    ost_delpage::forbid_orphans(db, "BidPages", "BidTypGroupViews", "BidPageUID");
    ost_delpage::forbid_orphans(db, "BidTypGroupViews", "AffectDPCTypGroupViews", "BidTypGroupViewUID");

    CHECK(run(db, "1", {"10"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM AffectDPCTypGroupViews"), "31");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || BidPageUID FROM BidTypGroupViews"), "21:11");
    for (const char *table : tables)
    {
        SCOPED_CONTEXT(std::string("table: ") + table);
        CHECK_STR_EQ(ost_ptest::list(db, std::string("SELECT BidPageUID FROM [") + table + "]"), "11");
    }
    CHECK_STR_EQ(page_uids(db), "11");
}

// Port of :461 test_page_delete_removes_only_target_page_rows_from_every_child_table: every table of PAGE_DELETE_CHILD_TABLES plus BidTakeoffs
// holds (1, page 10) and (2, page 11); BidNamedViews the same; BidHotLinks (1, 10, view 2), (2, 11, view 1), (3, 11, view 2), (4, 11, NULL).
// Delete ["10"]: (2, 11) is left in every table and in BidNamedViews; of the hot links the one ON page 10 and the one on page 11 that jumps
// to the view of page 10 go, (3, 11, 2) and (4, 11, NULL) stay.
TEST(DelPageWindows, RemovesOnlyTheTargetPageRowsFromEveryChildTable)
{
    TempDb db("delpage_win_every_child");
    seed_bid_1(db);
    add_pages(db, {10, 11});
    Uids tables = child_tables();
    tables.push_back("BidTakeoffs");
    for (const std::string &table : tables)
    {
        db.must("INSERT INTO [" + table + "] (UID, BidPageUID) VALUES (1, 10)");
        db.must("INSERT INTO [" + table + "] (UID, BidPageUID) VALUES (2, 11)");
    }
    db.must("INSERT INTO BidNamedViews (UID, BidPageUID) VALUES (1, 10)");
    db.must("INSERT INTO BidNamedViews (UID, BidPageUID) VALUES (2, 11)");
    db.must("INSERT INTO BidHotLinks (UID, BidPageUID, BidPageViewUID) VALUES (1, 10, 2)");
    db.must("INSERT INTO BidHotLinks (UID, BidPageUID, BidPageViewUID) VALUES (2, 11, 1)");
    db.must("INSERT INTO BidHotLinks (UID, BidPageUID, BidPageViewUID) VALUES (3, 11, 2)");
    db.must("INSERT INTO BidHotLinks (UID, BidPageUID, BidPageViewUID) VALUES (4, 11, NULL)");

    CHECK(run(db, "1", {"10"}));
    REQUIRE_EQ(tables.size(), static_cast<size_t>(24));
    for (const std::string &table : tables)
    {
        SCOPED_CONTEXT("table: " + table);
        CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || BidPageUID FROM [" + table + "]"), "2:11");
    }
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || BidPageUID FROM BidNamedViews"), "2:11");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || BidPageUID || ':' || ifnull(BidPageViewUID, 'N') FROM BidHotLinks ORDER BY UID"), "3:11:2,4:11:N");
    CHECK_STR_EQ(page_uids(db), "11");
}

// Port of :503 test_page_delete_clears_master_page_and_cross_page_comment_parent: pages (7, bid 1), (8, bid 1, master 7), (9, bid 2, master 7);
// comments 70 (bid 1, page 7), 80 (bid 1, page 8, parent 70), 90 (bid 2, page 9, parent 70); delete ["7"]: page 8's master is cleared, page 9
// (ANOTHER bid) keeps master 7; comment 80 becomes a root comment, comment 90 (another bid) keeps parent 70. (Register DP-7: a cross-bid
// master stays dangling, pinned by Windows.)
TEST(DelPageWindows, ClearsTheMasterPageAndTheCrossPageCommentParentOfTheSameBidOnly)
{
    TempDb db("delpage_win_master_comment");
    seed_bid_1(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (2, 1, 'other')");
    db.must("INSERT INTO BidPages (UID, BidUID, MasterPageUID) VALUES (7, 1, NULL)");
    db.must("INSERT INTO BidPages (UID, BidUID, MasterPageUID) VALUES (8, 1, 7)");
    db.must("INSERT INTO BidPages (UID, BidUID, MasterPageUID) VALUES (9, 2, 7)");
    db.must("INSERT INTO BidComments (UID, BidUID, BidPageUID, ParentCommentUID) VALUES (70, 1, 7, NULL)");
    db.must("INSERT INTO BidComments (UID, BidUID, BidPageUID, ParentCommentUID) VALUES (80, 1, 8, 70)");
    db.must("INSERT INTO BidComments (UID, BidUID, BidPageUID, ParentCommentUID) VALUES (90, 2, 9, 70)");

    CHECK(run(db, "1", {"7"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(MasterPageUID, 'N') FROM BidPages ORDER BY UID"), "8:N,9:7");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(ParentCommentUID, 'N') FROM BidComments ORDER BY UID"), "80:N,90:70");
}

// Port of :538 test_page_delete_clears_surviving_takeoff_self_references: pages (7, bid 1), (8, bid 1), (9, bid 2); takeoffs 70 (bid 1, page 7),
// 80 (bid 1, page 8; Parent, TypGroupTakeoff, TypPageTakeoff, TypGroupMarker all 70), 90 (bid 2, page 9, the same four columns 70); delete ["7"]:
// takeoff 80 has four NULLs, takeoff 90 (another bid) keeps 70 in all four.
TEST(DelPageWindows, ClearsSurvivingTakeoffSelfReferencesOfTheSameBidOnly)
{
    TempDb db("delpage_win_takeoff_refs");
    seed_bid_1(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (2, 1, 'other')");
    add_pages(db, {7, 8}, 1);
    add_pages(db, {9}, 2);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID, ParentUID, TypGroupTakeoffUID, TypPageTakeoffUID, TypGroupMarkerUID) VALUES (70, 1, 7, NULL, NULL, NULL, NULL)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID, ParentUID, TypGroupTakeoffUID, TypPageTakeoffUID, TypGroupMarkerUID) VALUES (80, 1, 8, 70, 70, 70, 70)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID, ParentUID, TypGroupTakeoffUID, TypPageTakeoffUID, TypGroupMarkerUID) VALUES (90, 2, 9, 70, 70, 70, 70)");

    CHECK(run(db, "1", {"7"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(ParentUID, 'N') || ',' || ifnull(TypGroupTakeoffUID, 'N') || ',' || ifnull(TypPageTakeoffUID, 'N') || ',' || ifnull(TypGroupMarkerUID, 'N') FROM BidTakeoffs ORDER BY UID"),
                 "80:N,N,N,N,90:70,70,70,70");
}

// Port of :567 test_page_delete_removes_annotations_linked_by_either_takeoff_endpoint: pages 7, 8; takeoffs 70 (page 7), 80, 81 (page 8); in each
// of BidDimensions, BidALines, BidArrows three rows ON page 8 from/to (80, 70), (70, 80), (80, 81) (uid index * 10 + 1, 2, 3); delete ["7"]:
// only the (80, 81) row is left in each table, takeoffs 80 and 81 stay.
TEST(DelPageWindows, RemovesAnnotationsLinkedByEitherTakeoffEndpoint)
{
    TempDb db("delpage_win_endpoints");
    seed_bid_1(db);
    add_pages(db, {7, 8});
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID) VALUES (70, 1, 7)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID) VALUES (80, 1, 8)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID) VALUES (81, 1, 8)");
    const char *tables[] = {"BidDimensions", "BidALines", "BidArrows"};
    int index = 1;
    for (const char *table : tables)
    {
        const std::string t = table;
        const std::string base = "INSERT INTO [" + t + "] (UID, BidUID, BidPageUID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (";
        db.must(base + std::to_string(index * 10 + 1) + ", 1, 8, 80, 70)");
        db.must(base + std::to_string(index * 10 + 2) + ", 1, 8, 70, 80)");
        db.must(base + std::to_string(index * 10 + 3) + ", 1, 8, 80, 81)");
        ++index;
    }

    CHECK(run(db, "1", {"7"}));
    index = 1;
    for (const char *table : tables)
    {
        SCOPED_CONTEXT(std::string("table: ") + table);
        CHECK_STR_EQ(ost_ptest::list(db, std::string("SELECT UID || ':' || BidTakeoffFromUID || '>' || BidTakeoffToUID FROM [") + table + "]"),
                     std::to_string(index * 10 + 3) + ":80>81");
        ++index;
    }
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidTakeoffs ORDER BY UID"), "80,81");
}

// Port of :609 test_page_delete_rejects_duplicate_physical_uid_before_cascade: two BidPages rows with uid 7 (bids 1 and 2) and a takeoff on 7:
// False, the log holds "BidPages contains duplicate UID 7", both rows and the takeoff stay. iOS: BidPages here has no primary key (it is
// recreated loosely, like the Windows table), the bids exist, and a page 8 of bid 1 is added so that the last-page rule cannot be the reason.
TEST(DelPageWindows, RejectsADuplicatePhysicalPageUidBeforeTheCascade)
{
    TempDb db("delpage_win_dup_page");
    seed_bid_1(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (2, 1, 'other')");
    db.must("DROP TABLE BidPages");
    db.must("CREATE TABLE BidPages (UID INTEGER, BidUID INTEGER)");
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (7, 1)");
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (7, 2)");
    db.must("INSERT INTO BidPages (UID, BidUID) VALUES (8, 1)");
    db.must("INSERT INTO BidTakeoffs (UID, BidPageUID) VALUES (70, 7)");
    const std::string before = db.snapshot_all();

    std::string error;
    CHECK_FALSE(run(db, "1", {"7"}, error));
    CHECK_STR_EQ(error, ost_delpage_oracle::kMsgDuplicatePage7);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 3);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 1);
    CHECK_EQ(db.snapshot_all(), before);
}

// Port of :623 test_bulk_page_delete_batches_cascade_statements: 101 pages in bid 1, page 101 has MasterPageUID 100; delete 1..100 (two chunks of
// 50 uids): only (101, NULL) is left. Windows also counts the SQL (3 row selects, 2 updates, 2 deletes); iOS has no statement trace, so the
// DATA outcome is ported and the chunk boundary is walked: with n pages deleted out of 101 (n = 1, 49, 50, 51, 99, 100) and a master chain
// (page p has master p - 1), only the page that followed the last deleted one loses its master.
TEST(DelPageWindows, TheBulkDeleteCrossesTheFiftyUidChunk)
{
    for (const int n : {1, 49, 50, 51, 99, 100})
    {
        SCOPED_CONTEXT("deleting 1.." + std::to_string(n) + " of 101");
        TempDb db("delpage_win_bulk_" + std::to_string(n));
        seed_bid_1(db);
        std::string sql = "INSERT INTO BidPages (UID, BidUID, MasterPageUID) VALUES ";
        for (int uid = 1; uid <= 101; ++uid)
            sql += std::string(uid == 1 ? "" : ", ") + "(" + std::to_string(uid) + ", 1, " + (uid == 1 ? "NULL" : std::to_string(uid - 1)) + ")";
        db.must(sql);
        Uids uids;
        for (int uid = 1; uid <= n; ++uid)
            uids.push_back(std::to_string(uid));

        CHECK(run(db, "1", uids));
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 101 - n);
        CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(MasterPageUID, 'N') FROM BidPages WHERE UID IN (" + std::to_string(n + 1) + ", " +
                                             std::to_string(n + 2 > 101 ? 101 : n + 2) + ") ORDER BY UID"),
                     n == 100 ? "101:N" : std::to_string(n + 1) + ":N," + std::to_string(n + 2) + ":" + std::to_string(n + 1));
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE UID <= " + std::to_string(n)), 0);
    }
}

// The Windows test of :623 exactly: pages 1..101, page 101 has master 100, delete 1..100 leaves [(101, NULL)].
TEST(DelPageWindows, TheBulkDeleteOfTheWindowsTestLeavesOnlyPage101WithoutAMaster)
{
    TempDb db("delpage_win_bulk_exact");
    seed_bid_1(db);
    std::string sql = "INSERT INTO BidPages (UID, BidUID, MasterPageUID) VALUES ";
    for (int uid = 1; uid <= 101; ++uid)
        sql += std::string(uid == 1 ? "" : ", ") + "(" + std::to_string(uid) + ", 1, " + (uid == 101 ? "100" : "NULL") + ")";
    db.must(sql);
    Uids uids;
    for (int uid = 1; uid <= 100; ++uid)
        uids.push_back(std::to_string(uid));

    CHECK(run(db, "1", uids));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(MasterPageUID, 'N') FROM BidPages ORDER BY UID"), "101:N");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Other persistence tests of test_settings_operations.py
// ---------------------------------------------------------------------------------------------------------------------------------

// Port of :3401 test_deleting_pages_needs_a_page_uid_column: BidPages(BidUID, Name) without UID and one row (1, 'p'); delete ["5"] is False and the
// row stays.
TEST(DelPageWindows, ADeleteNeedsAPageUidColumn)
{
    TempDb db("delpage_win_no_uid_column");
    seed_bid_1(db);
    db.must("DROP TABLE BidPages");
    db.must("CREATE TABLE BidPages (BidUID INTEGER, Name TEXT)");
    db.must("INSERT INTO BidPages (BidUID, Name) VALUES (1, 'p')");
    const std::string before = db.snapshot_all();

    CHECK_FALSE(run(db, "1", {"5"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 1);
    CHECK_EQ(db.snapshot_all(), before);
}

// Port of :3516 test_deleting_pages_skips_dependents_that_lack_the_referencing_columns: Bids (UID) with bid 1, BidPages (UID, BidUID) (11, 1),
// (21, 1), and EVERY dependent table (the 23 child tables, the three annotation link tables, BidComments, BidTakeoffs, BidPercents,
// AffectDPCTypGroupViews, BidTypGroupViews, BidHotLinks, BidNamedViews, BidSettings) exists with the single column UID and one row (1); delete
// ["11"] succeeds, page 21 stays and every bare table is untouched. iOS: Bids also has JobStatusUID (NULL), because the lock guard reads that
// column and fails closed without it (plan 4.1).
TEST(DelPageWindows, SkipsDependentsThatLackTheReferencingColumns)
{
    TempDb db("delpage_win_sparse", /*with_schema=*/false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID INTEGER)");
    db.must("INSERT INTO Bids VALUES (1, NULL)");
    db.must("CREATE TABLE BidPages (UID INTEGER, BidUID INTEGER)");
    db.must("INSERT INTO BidPages VALUES (11, 1)");
    db.must("INSERT INTO BidPages VALUES (21, 1)");
    Uids bare = child_tables();
    for (const char *extra : {"BidComments", "BidTakeoffs", "BidPercents", "AffectDPCTypGroupViews", "BidTypGroupViews", "BidHotLinks", "BidNamedViews",
                              "BidSettings", "BidDimensions", "BidALines", "BidArrows"})
        if (std::find(bare.begin(), bare.end(), extra) == bare.end())
            bare.push_back(extra);
    for (const std::string &table : bare)
    {
        db.must("CREATE TABLE [" + table + "] (UID INTEGER)");
        db.must("INSERT INTO [" + table + "] VALUES (1)");
    }

    std::string error;
    CHECK(run(db, "1", {"11"}, error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidPages"), "21");
    for (const std::string &table : bare)
    {
        SCOPED_CONTEXT("bare table: " + table);
        CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM [" + table + "]"), "1");
    }
}

// Port of :3548 test_deleting_pages_removes_the_percentages_of_the_pages_takeoffs_only: pages (11, 1), (21, 1); takeoffs (101; bid 1, page 11),
// (201; bid 1, page 21); BidPercents (1; takeoff 101), (2; takeoff 201); delete ["11"] leaves percent 2 and takeoff 201.
TEST(DelPageWindows, RemovesThePercentagesOfThePagesTakeoffsOnly)
{
    TempDb db("delpage_win_percents");
    seed_bid_1(db);
    add_pages(db, {11, 21});
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID) VALUES (101, 1, 11)");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID) VALUES (201, 1, 21)");
    db.must("INSERT INTO BidPercents (UID, BidTakeoffUID) VALUES (1, 101)");
    db.must("INSERT INTO BidPercents (UID, BidTakeoffUID) VALUES (2, 201)");

    CHECK(run(db, "1", {"11"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidPercents"), "2");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID FROM BidTakeoffs"), "201");
}

// Port of :6579 test_page_delete_labels_invalid_identifiers_by_table: delete ["x"] is False and the logged text is "Invalid BidPages UID: 'x'".
TEST(DelPageWindows, LabelsAnInvalidIdentifierByTable)
{
    TempDb db("delpage_win_invalid_x");
    seed_bid_1(db);
    add_pages(db, {10, 11});
    const std::string before = db.snapshot_all();

    std::string error;
    CHECK_FALSE(run(db, "1", {"x"}, error));
    CHECK_STR_EQ(error, ost_delpage_oracle::kMsgInvalidX);
    CHECK_EQ(db.snapshot_all(), before);
}

// Port of :6612 test_page_cascade_rejects_duplicate_comment_and_takeoff_uids_with_table_labels: page (10, 1); two comments with uid 7 on page 10
// (bid 1) give False and "BidComments contains duplicate UID 7"; two takeoffs with uid 8 give "BidTakeoffs contains duplicate UID 8". iOS: BidComments /
// BidTakeoffs are recreated loosely (no primary key) as in the Windows DDL, and a page 11 is added so that the last-page rule is not the reason.
TEST(DelPageWindows, RejectsDuplicateCommentAndTakeoffUidsWithTableLabels)
{
    {
        TempDb db("delpage_win_dup_comment");
        seed_bid_1(db);
        add_pages(db, {10, 11});
        db.must("DROP TABLE BidComments");
        db.must("CREATE TABLE BidComments (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, ParentCommentUID INTEGER)");
        db.must("INSERT INTO BidComments VALUES (7, 1, 10, NULL)");
        db.must("INSERT INTO BidComments VALUES (7, 1, 10, NULL)");
        const std::string before = db.snapshot_all();

        std::string error;
        CHECK_FALSE(run(db, "1", {"10"}, error));
        CHECK_STR_EQ(error, ost_delpage_oracle::kMsgDuplicateComment7);
        CHECK_EQ(db.snapshot_all(), before);
    }
    {
        TempDb db("delpage_win_dup_takeoff");
        seed_bid_1(db);
        add_pages(db, {10, 11});
        db.must("DROP TABLE BidTakeoffs");
        db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER)");
        db.must("INSERT INTO BidTakeoffs VALUES (8, 1, 10)");
        db.must("INSERT INTO BidTakeoffs VALUES (8, 1, 10)");
        const std::string before = db.snapshot_all();

        std::string error;
        CHECK_FALSE(run(db, "1", {"10"}, error));
        CHECK_STR_EQ(error, ost_delpage_oracle::kMsgDuplicateTakeoff8);
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// Port of :6644 test_page_cascade_skips_percent_and_annotation_tables_that_lack_the_takeoff_columns: page (10, 1); takeoff (8; bid 1, page 10);
// BidPercents (UID only) with row 1; BidDimensions (UID, BidPageUID) with row (1, page 99); BidALines (UID, BidTakeoffFromUID) with row (1, 8):
// the delete succeeds, BidPercents and BidDimensions keep their row, the BidALines row (from takeoff 8) goes, the page goes. iOS: the page
// 11 is added (the last-page rule), the three tables are recreated loosely and the delete is checked for success and the rows.
TEST(DelPageWindows, SkipsPercentAndAnnotationTablesThatLackTheTakeoffColumns)
{
    TempDb db("delpage_win_missing_takeoff_columns");
    seed_bid_1(db);
    add_pages(db, {10, 11});
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidPageUID) VALUES (8, 1, 10)");
    db.must("DROP TABLE BidPercents");
    db.must("CREATE TABLE BidPercents (UID INTEGER)");
    db.must("INSERT INTO BidPercents VALUES (1)");
    db.must("DROP TABLE BidDimensions");
    db.must("CREATE TABLE BidDimensions (UID INTEGER, BidPageUID INTEGER)");
    db.must("INSERT INTO BidDimensions VALUES (1, 99)");
    db.must("DROP TABLE BidALines");
    db.must("CREATE TABLE BidALines (UID INTEGER, BidTakeoffFromUID INTEGER)");
    db.must("INSERT INTO BidALines VALUES (1, 8)");

    CHECK(run(db, "1", {"10"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPercents"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidDimensions"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidALines"), 0);
    CHECK_STR_EQ(page_uids(db), "11");
}

// Port of :6274 test_a_failing_page_child_delete_warns_and_continues_unless_strict, the STRICT half ONLY (register DP-1: the tolerant half is a
// test-double artefact, in production Windows re-raises and rolls back): BidLegends (1, page 10) with a BEFORE DELETE trigger that aborts, delete
// ["10"]. Windows strict: pyodbc.Error propagates. iOS: False, the page row 10 is still there (never deleted with orphan children) and nothing
// changed. A page 11 is added for the last-page rule.
TEST(DelPageWindows, AFailingPageChildDeleteIsStrictAndLeavesThePageRowInPlace)
{
    TempDb db("delpage_win_strict_legend");
    seed_bid_1(db);
    add_pages(db, {10, 11});
    db.must("INSERT INTO BidLegends (UID, BidPageUID) VALUES (1, 10)");
    db.block("BidLegends", "DELETE");
    const std::string before = db.snapshot_all();

    std::string error;
    CHECK_FALSE(run(db, "1", {"10"}, error));
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE UID = 10"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLegends"), 1);
    CHECK_EQ(db.snapshot_all(), before);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// tests/integration/mdb/test_ownership_and_atomicity.py
// ---------------------------------------------------------------------------------------------------------------------------------

// Port of :51 test_bid_owned_bulk_deletes_reject_cross_bid_batches, the pages row: BidPages (7, bid 1), (8, bid 2), both bids exist; delete
// ["7", "8"] is False and both stay; the control ["7"] (a batch confined to one bid) is accepted and only 8 stays. iOS: the call names bid 1, and a
// page 9 of bid 1 is added so that the control does not delete the last page of bid 1.
TEST(DelPageWindows, ACrossBidBatchIsRefusedAndAOneBidBatchIsAccepted)
{
    TempDb db("delpage_win_cross_bid");
    seed_bid_1(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (2, 1, 'other')");
    add_pages(db, {7, 9}, 1);
    add_pages(db, {8}, 2);
    const std::string before = db.snapshot_all();

    std::string error;
    CHECK_FALSE(run(db, "1", {"7", "8"}, error));
    CHECK(!error.empty());
    CHECK_STR_EQ(page_uids(db), "7,8,9");
    CHECK_EQ(db.snapshot_all(), before);

    CHECK(run(db, "1", {"7"}));
    CHECK_STR_EQ(page_uids(db), "8,9");
}

// Port of :154 test_later_chunk_delete_failure_rolls_back_page_condition_and_folder_batches, the pages part: BidPages 1..51 (bid 1) plus page 100
// with MasterPageUID 1; a trigger fails the delete of uid 51 (the second 50-uid chunk). Delete 1..51: False, all 52 rows stay AND page 100 still
// has master 1 (the clear of the first statement is rolled back too). Control: without the trigger the same batch commits, page 100 is the only
// page left and its master is NULL.
TEST(DelPageWindows, ALaterChunkFailureRollsTheWholePageBatchBack)
{
    TempDb db("delpage_win_later_chunk");
    seed_bid_1(db);
    std::string sql = "INSERT INTO BidPages (UID, BidUID, MasterPageUID) VALUES ";
    for (int uid = 1; uid <= 51; ++uid)
        sql += std::string(uid == 1 ? "" : ", ") + "(" + std::to_string(uid) + ", 1, NULL)";
    db.must(sql);
    db.must("INSERT INTO BidPages (UID, BidUID, MasterPageUID) VALUES (100, 1, 1)");
    db.must("CREATE TRIGGER fail_second_BidPages BEFORE DELETE ON [BidPages] WHEN OLD.UID = 51 BEGIN SELECT RAISE(ABORT, 'later chunk'); END");
    Uids uids;
    for (int uid = 1; uid <= 51; ++uid)
        uids.push_back(std::to_string(uid));
    const std::string before = db.snapshot_all();

    std::string error;
    CHECK_FALSE(run(db, "1", uids, error));
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 52);
    CHECK_EQ(db.count("SELECT MasterPageUID FROM BidPages WHERE UID = 100"), 1);
    CHECK_EQ(db.snapshot_all(), before);

    db.must("DROP TRIGGER fail_second_BidPages");
    CHECK(run(db, "1", uids));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(MasterPageUID, 'N') FROM BidPages"), "100:N");
}

// Port of :448 test_orphan_page_edit_and_delete_reject_before_mutation, the delete half: BidPages (7, bid 99, 'Orphan page') and no Bids row for
// 99: False, the text is "Bids has no row for UID 99; ..." and the row stays; control: after Bids 99 is inserted the delete goes through.
// iOS: the call names bid 99, a page 8 of bid 99 is added (the last-page rule), and the table keeps its iOS definition.
TEST(DelPageWindows, AnOrphanPageIsRefusedBeforeAnyMutationAndDeletedOnceItsBidExists)
{
    TempDb db("delpage_win_orphan");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (7, 99, 'Orphan page')");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (8, 99, 'Other page')");
    const std::string before = db.snapshot_all();

    std::string error;
    CHECK_FALSE(run(db, "99", {"7"}, error));
    CHECK_STR_EQ(error, ost_delpage_oracle::kMsgOrphanBid99);
    CHECK_EQ(db.snapshot_all(), before);

    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName) VALUES (99, 1, 'now it exists')");
    CHECK(run(db, "99", {"7"}));
    CHECK_STR_EQ(page_uids(db), "8");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// The whole-database oracle: the REAL Windows delete_pages on the same world (DeletePageOracleData.hpp)
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
    void expect_the_windows_result(const char *scenario_name, const std::string &db_name)
    {
        const ost_delpage_oracle::Scenario &scenario = ost_delpage::scenario(scenario_name);
        REQUIRE(!scenario.seed.empty());
        TempDb db(db_name);
        ost_delpage::seed_scenario(db, scenario);

        std::string error;
        CHECK(run(db, "1", ost_delpage::scenario_uids(scenario), error));
        CHECK_STR_EQ(error, "");
        CHECK_STR_EQ(ost_delpage::projection_differences(db, scenario.expected), "");
    }
}

// Scenario A: 4 pages of two bids, takeoffs with self references over the page border, comment threads, annotations from and to the page's takeoffs,
// named views and hot links, a selected page and a cover sheet selection in three bids, one row per page in every one of the 23 child tables.
// Windows leaves, in every table, exactly what the oracle holds. Includes the Windows behaviours that are NOT bid-scoped (steps 6, 10, 13: a bid-2
// annotation from a deleted takeoff and a bid-2 hot link to a deleted view go; a bid-3 setting that selected the page is cleared) and DP-11 (a comment
// reply whose parent has another BidUID keeps its dangling parent).
TEST(DelPageOracle, ADeleteOfOnePageLeavesEveryTableLikeWindows)
{
    expect_the_windows_result("A_DeleteOnePage", "delpage_oracle_a");
}

// Scenario B: the same world, two pages of bid 1 (10 and 12) in one call; the master chain 10 <- 11 <- 12 and the references between the two pages and
// the survivor are all resolved the Windows way.
TEST(DelPageOracle, ADeleteOfTwoPagesInOneCallLeavesEveryTableLikeWindows)
{
    expect_the_windows_result("B_DeleteTwoPagesOfThree", "delpage_oracle_b");
}

// Scenario C: 110 pages in bid 1, deleting 1..100 (two 50-uid chunks and a tail): masters, comment parents, takeoff references, annotations, hot links
// and the views they jump to, percents and a selected page, all crossing the chunk border in both directions.
TEST(DelPageOracle, TheChunkBoundaryOfOneHundredPagesLeavesEveryTableLikeWindows)
{
    expect_the_windows_result("C_ChunkBoundary_Pages1To100", "delpage_oracle_c");
}

// Scenario D: the same 110 pages, the list 110, 1, 60, 101, 60, 50, 51 (not sorted, a repeat, the chunk border 50 / 51 inside the list).
TEST(DelPageOracle, AnUnsortedListWithARepeatLeavesEveryTableLikeWindows)
{
    expect_the_windows_result("D_ChunkBoundary_Mixed", "delpage_oracle_d");
}

// The oracle scenarios themselves: every scenario deletes at least one page and never the last page of its bid, so that a Core that refuses wrongly
// (or deletes nothing) cannot match; the seed has at least one row in each of the 26 tables with a page column.
TEST(DelPageOracle, TheScenariosAreMeaningful)
{
    for (const char *name : {"A_DeleteOnePage", "B_DeleteTwoPagesOfThree", "C_ChunkBoundary_Pages1To100", "D_ChunkBoundary_Mixed"})
    {
        SCOPED_CONTEXT(std::string("scenario ") + name); // not std::string(name): that parses as a function declaration (-Wvexing-parse)
        const ost_delpage_oracle::Scenario &scenario = ost_delpage::scenario(name);
        REQUIRE(!scenario.seed.empty());
        TempDb db(std::string("delpage_oracle_meaning_") + name);
        ost_delpage::seed_scenario(db, scenario);
        const std::vector<std::string> before = ost_delpage::project_all(db);
        REQUIRE_EQ(before.size(), scenario.expected.size());
        int differing = 0;
        for (std::size_t i = 0; i < before.size(); ++i)
            if (before[i] != scenario.expected[i])
                ++differing;
        CHECK(differing >= 10); // many tables change in every scenario (17 or more with the seeds above)
        CHECK(!ost_delpage::scenario_uids(scenario).empty());
    }
}
