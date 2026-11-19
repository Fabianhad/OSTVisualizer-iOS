// BRW-13 / BRW-M01 (decided): the order of the project browser. Bids are listed by BidNo ascending (a NULL BidNo first) with ties in reader
// order (BidProjectUID, then UID); folders by a case-insensitive ASCII compare of the name, then lowercase first, then UID.
// Windows oracle:
//   ost_visualizer/infrastructure/mdb/components/hierarchy_reader.py:40-43,72-81 (projects ORDER BY [Name], [UID]; bids ORDER BY [BidProjectUID],
//     [BidNo], [UID], NULLs first)
//   ost_visualizer/presentation/components/project_tree_view.py:453-454,689 (sortByColumn(0, Ascending), column 0 = bid.bid_no) and
//     SortableTreeWidgetItem.__lt__ (None first, then numeric; Qt's stable sort keeps the reader order of equal keys)
//   tests/infrastructure/mdb/components/test_hierarchy_reader.py::test_full_hierarchy_maps_every_bid_field_and_orders_bids_and_projects,
//     test_bids_with_equal_project_and_number_are_ordered_by_uid
//   tests/presentation/components/test_project_tree_view.py::test_default_bid_order_is_numeric_and_keeps_status_uid_separate_from_sort_value
//   HierarchyOrderOracleData.hpp: 40 random databases run through the real Windows reader and a real offscreen QTreeWidget
//   BRW-M01: Qt's own compare is locale aware and cannot be reproduced in portable C++; the user decided on the ASCII rule below.
#include "BrowserTestSupport.hpp"
#include "HierarchyOrderOracleData.hpp"

#include <string>

using ost_btest::TempDb;

namespace
{
    ost_domain::DatabaseHierarchy load(const TempDb &db)
    {
        ost_domain::DatabaseHierarchy hierarchy;
        std::string error;
        if (!ost_store::load_hierarchy(db.path(), hierarchy, &error))
            FAIL_TEST("load_hierarchy failed: " + error);
        return hierarchy;
    }

    // A database whose projects are exactly `projects` (the trash row of create_schema is removed so UID 1 is free).
    void reset_projects(const TempDb &db)
    {
        db.must("DELETE FROM BidProjects");
    }

    void add_bid(const TempDb &db, int uid, const std::string &project, const std::string &bid_no)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (" + std::to_string(uid) + ", " + project + ", 'B" +
                std::to_string(uid) + "', " + bid_no + ")");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-13: bid order
// ---------------------------------------------------------------------------------------------------------------

// Item test plan: folder 5 with BidNo (10:5, 11:5, 12:5, 20:1, 21:9) lists [20, 10, 11, 12, 21]: BidNo ascending, equal numbers by UID.
// Orphans (30: No 3, NULL project), (31: No 2, NULL), (32: No 1, project 0), (33: No 1, project 99) list [32, 33, 31, 30]: BidNo ascending,
// ties in reader order (the NULL project first, then 0, then the dangling 99). Before the fix: folder [21, 20, 12, 11, 10], orphans [33, 32, 31, 30]
// (UID descending). Reader order of the orphans is [31, 30, 32, 33].
TEST(Brw13_BidOrder, FolderBidsByBidNoAndOrphansByBidNoThenProject)
{
    TempDb db("order_item_plan");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (5, 'Five')");
    add_bid(db, 10, "5", "5");
    add_bid(db, 11, "5", "5");
    add_bid(db, 12, "5", "5");
    add_bid(db, 20, "5", "1");
    add_bid(db, 21, "5", "9");
    add_bid(db, 30, "NULL", "3");
    add_bid(db, 31, "NULL", "2");
    add_bid(db, 32, "0", "1");
    add_bid(db, 33, "99", "1");

    const ost_domain::DatabaseHierarchy hierarchy = load(db);
    REQUIRE_EQ(hierarchy.folders.size(), static_cast<size_t>(2)); // 'Deleted Bids' and 'Five'
    CHECK_EQ(ost_btest::hierarchy_text(hierarchy), std::string("p1{}|p5{20,10,11,12,21}||o{32,33,31,30}"));
    CHECK_STR_EQ(hierarchy.orphan_bids[0].folder_uid, "");
}

// Port of test_bids_with_equal_project_and_number_are_ordered_by_uid: Bids (12, 1, 'third', 5), (10, 1, 'first', 5), (11, 1, 'second', 5) -> [10, 11, 12].
TEST(Brw13_BidOrder, BidsWithEqualProjectAndNumberAreOrderedByUid)
{
    TempDb db("order_equal_numbers");
    REQUIRE(db.schema_ok());
    reset_projects(db);
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (1, 'P')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (12, 1, 'third', 5)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (10, 1, 'first', 5)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (11, 1, 'second', 5)");

    CHECK_EQ(ost_btest::hierarchy_text(load(db)), std::string("p1{10,11,12}||o{}"));
}

// Port of test_full_hierarchy_maps_every_bid_field_and_orders_bids_and_projects (the order parts): projects (2,'Beta'), (1,'Alpha'), (3,'Alpha') list
// as 1, 3, 2; the bids of project 1, 10 (BidNo 2) and 11 (BidNo 1), list [11, 10]; project 2 has bid 12 (BidNo 0), project 3 none; the orphans
// 13 (BidProjectUID 99) and 14 (BidProjectUID 0), both BidNo 0, list [14, 13] (reader order: 0 before 99).
TEST(Brw13_BidOrder, PortOfTheFullHierarchyOrder)
{
    TempDb db("order_full_hierarchy");
    REQUIRE(db.schema_ok());
    reset_projects(db);
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Beta')");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (1, 'Alpha')");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (3, 'Alpha')");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (10, 1, 'B10', 2)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (11, 1, 'B11', 1)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (12, 2, 'B12', 0)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (13, 99, 'Lost', 0)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (14, 0, 'Free', 0)");

    const ost_domain::DatabaseHierarchy hierarchy = load(db);
    CHECK_EQ(ost_btest::hierarchy_text(hierarchy), std::string("p1{11,10}|p3{}|p2{12}||o{14,13}"));
    REQUIRE_EQ(hierarchy.orphan_bids.size(), static_cast<size_t>(2));
    CHECK_STR_EQ(hierarchy.orphan_bids[0].name, "Free");
    CHECK_STR_EQ(hierarchy.orphan_bids[1].name, "Lost");
}

// Port of test_default_bid_order_is_numeric_and_keeps_status_uid_separate_from_sort_value: bid numbers 10, 2, 33, 1, 100 list as 1, 2, 10, 33, 100
// (numeric, never the text order 1, 10, 100, 2, 33).
TEST(Brw13_BidOrder, BidNumbersAreComparedAsNumbers)
{
    TempDb db("order_numeric");
    REQUIRE(db.schema_ok());
    reset_projects(db);
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (1, 'project-1')");
    for (const int number : {10, 2, 33, 1, 100})
        add_bid(db, 200 + number, "1", std::to_string(number));

    const ost_domain::DatabaseHierarchy hierarchy = load(db);
    CHECK_EQ(ost_btest::hierarchy_text(hierarchy), std::string("p1{201,202,210,233,300}||o{}"));
    REQUIRE_EQ(hierarchy.folders.size(), static_cast<size_t>(1));
    REQUIRE_EQ(hierarchy.folders[0].bids.size(), static_cast<size_t>(5));
    const long long expected[] = {1, 2, 10, 33, 100};
    for (size_t i = 0; i < 5; ++i)
    {
        SCOPED_CONTEXT("bid " + std::to_string(i));
        CHECK(hierarchy.folders[0].bids[i].has_bid_no);
        CHECK_EQ(hierarchy.folders[0].bids[i].bid_no, expected[i]);
    }
}

// A NULL BidNo sorts before every number, 0 included (SortableTreeWidgetItem.__lt__: None is less than any value; both None compare equal and
// keep the reader order); the summary says so.
TEST(Brw13_BidOrder, ANullBidNoSortsFirstAndIsReported)
{
    TempDb db("order_null_bid_no");
    REQUIRE(db.schema_ok());
    reset_projects(db);
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (1, 'P')");
    add_bid(db, 15, "1", "0");
    add_bid(db, 14, "1", "NULL");
    add_bid(db, 13, "1", "1");
    add_bid(db, 12, "1", "NULL");

    const ost_domain::DatabaseHierarchy hierarchy = load(db);
    CHECK_EQ(ost_btest::hierarchy_text(hierarchy), std::string("p1{12,14,15,13}||o{}"));
    REQUIRE_EQ(hierarchy.folders[0].bids.size(), static_cast<size_t>(4));
    CHECK_FALSE(hierarchy.folders[0].bids[0].has_bid_no);
    CHECK_FALSE(hierarchy.folders[0].bids[1].has_bid_no);
    CHECK(hierarchy.folders[0].bids[2].has_bid_no);
    CHECK_EQ(hierarchy.folders[0].bids[2].bid_no, 0LL);
}

// 40 random databases (BidNo NULL / 0 / ties, projects NULL / 0 / dangling / existing) through the real Windows reader and the real tree sort.
TEST(Brw13_BidOrder, MatchesTheWindowsReaderAndTreeSortOnRandomDatabases)
{
    int index = 0;
    for (const ost_btest::HierarchyOrderOracleCase &c : ost_btest::kHierarchyOrderOracleCases)
    {
        SCOPED_CONTEXT("oracle case " + std::to_string(index));
        TempDb db("order_oracle_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        reset_projects(db);
        db.must(std::string("INSERT INTO BidProjects (UID, Name) VALUES ") + c.projects);
        db.must(std::string("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES ") + c.bids);
        CHECK_EQ(ost_btest::hierarchy_text(load(db)), std::string(c.expected));
    }
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-M01 (decided): folder order
// ---------------------------------------------------------------------------------------------------------------

// Item test plan: folders 'Zebra', 'apple', 'Banana', 'alpha', 'Alpha' plus the trash 'Deleted Bids' list case-insensitively, lowercase first on a
// tie: alpha, Alpha, apple, Banana, Deleted Bids, Zebra. Before the fix (SQLite BINARY): Alpha, Banana, Deleted Bids, Zebra, alpha, apple.
// (The item text lists the expected order as "apple/alpha/Alpha/..."; the decided rule puts 'alpha' before 'apple' - 'l' < 'p'.)
TEST(Brwm01_FolderOrder, CaseInsensitiveThenLowercaseFirst)
{
    TempDb db("folder_order_item_plan");
    REQUIRE(db.schema_ok());
    for (const char *name : {"Zebra", "apple", "Banana", "alpha", "Alpha"})
        CHECK_NE(ost_store::create_folder(db.path(), name), std::string());

    CHECK_EQ(ost_btest::folder_names(load(db)), std::string("alpha,Alpha,apple,Banana,Deleted Bids,Zebra"));
}

// The names Qt's locale aware sort was measured on (PySide6 6.10.1, locale en_US, a plain QTreeWidget sorted ascending): beta, Alpha, alpha, Zed, zed,
// Deleted Bids, Eclair became alpha, Alpha, beta, Deleted Bids, Eclair, zed, Zed; 'a b' before 'ab' and '10 Main' before '9 Main'; ['Zebra', 'apple']
// became ['apple', 'Zebra']. The decided ASCII rule gives the same order for exactly this subset.
TEST(Brwm01_FolderOrder, AgreesWithTheQtOrderOnTheMeasuredSubset)
{
    TempDb db("folder_order_qt_subset");
    REQUIRE(db.schema_ok());
    for (const char *name : {"beta", "Alpha", "alpha", "Zed", "zed", "Eclair", "10 Main", "9 Main", "a b", "ab", "Zebra", "apple"})
        CHECK_NE(ost_store::create_folder(db.path(), name), std::string());

    CHECK_EQ(ost_btest::folder_names(load(db)),
             std::string("10 Main,9 Main,a b,ab,alpha,Alpha,apple,beta,Deleted Bids,Eclair,Zebra,zed,Zed"));
}

// Equal names order by UID ascending, numerically (9 before 10); equal up to case: lowercase first, then UID.
TEST(Brwm01_FolderOrder, EqualNamesOrderByUidNumerically)
{
    TempDb db("folder_order_uid");
    REQUIRE(db.schema_ok());
    reset_projects(db);
    for (const int uid : {30, 10, 9, 100})
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (" + std::to_string(uid) + ", 'Same')");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (50, 'same')");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (7, 'SAME')");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (8, 'sAme')");

    // the first character decides lowercase first ('s' before 'S'); among equal bytes the UID decides
    const ost_domain::DatabaseHierarchy hierarchy = load(db);
    std::string uids;
    for (const ost_domain::FolderGroup &group : hierarchy.folders)
        uids += (uids.empty() ? "" : ",") + group.folder.uid;
    CHECK_EQ(uids, std::string("50,8,9,10,30,100,7"));
}

// Only ASCII letters fold; other bytes compare as unsigned bytes, so punctuation, digits and UTF-8 text keep a fixed place (this is the decided rule,
// not Qt's: Qt puts '_x' before '10 Main'). Digits < '_' < lowercase letters; non-ASCII after all ASCII.
TEST(Brwm01_FolderOrder, OnlyAsciiLettersFold)
{
    TempDb db("folder_order_ascii");
    REQUIRE(db.schema_ok());
    reset_projects(db);
    int uid = 2;
    for (const char *name : {"_x", "\xC3\x89" "clair", "eclair", "Eclair", "(x)", "7", "Zed"})
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (" + std::to_string(uid++) + ", " + ost_btest::quoted(name) + ")");

    CHECK_EQ(ost_btest::folder_names(load(db)), std::string("(x),7,_x,eclair,Eclair,Zed,\xC3\x89" "clair"));
}

// The comparators themselves (Core/Domain/DatabaseHierarchy.hpp), so the page UI and any other caller get the same order. Strict weak order
// checks: irreflexive, asymmetric, and consistent with the three-way compare.
TEST(Brwm01_FolderOrder, TheComparatorsFollowTheDecidedRule)
{
    using ost_domain::compare_folder_names;
    using ost_domain::compare_folder_uids;
    using ost_domain::folder_precedes;
    CHECK_EQ(compare_folder_names("apple", "alpha"), 1);
    CHECK_EQ(compare_folder_names("alpha", "apple"), -1);
    CHECK_EQ(compare_folder_names("Zebra", "apple"), 1);          // binary order would say the opposite
    CHECK_EQ(compare_folder_names("alpha", "Alpha"), -1);         // equal up to case: lowercase first
    CHECK_EQ(compare_folder_names("Alpha", "alpha"), 1);
    CHECK_EQ(compare_folder_names("same", "same"), 0);
    CHECK_EQ(compare_folder_names("sAme", "Same"), -1);           // the first differing byte decides: 's' < 'S'
    CHECK_EQ(compare_folder_names("Same", "SAME"), -1);           // 'a' < 'A'
    CHECK_EQ(compare_folder_names("ab", "a b"), 1);               // ' ' < 'b'
    CHECK_EQ(compare_folder_names("ab", "abc"), -1);              // a prefix first
    CHECK_EQ(compare_folder_names("", "a"), -1);
    CHECK_EQ(compare_folder_names("", ""), 0);
    CHECK_EQ(compare_folder_names("_x", "a"), -1);                // '_' (0x5F) < 'a' once letters are folded to lowercase
    CHECK_EQ(compare_folder_names("_x", "A"), -1);
    CHECK_EQ(compare_folder_names("Z", "_"), 1);                  // 'z' (0x7A) > '_' (0x5F)
    CHECK_EQ(compare_folder_names("\xC3\xA9", "z"), 1);           // UTF-8 bytes are not folded and sort after ASCII

    CHECK_EQ(compare_folder_uids("9", "10"), -1);
    CHECK_EQ(compare_folder_uids("10", "9"), 1);
    CHECK_EQ(compare_folder_uids("7", "7"), 0);
    CHECK_EQ(compare_folder_uids("007", "7"), 0);
    CHECK_EQ(compare_folder_uids("99999999999999999999", "100000000000000000000"), -1); // longer than any integer type
    CHECK_EQ(compare_folder_uids("5", "x"), -1);
    CHECK_EQ(compare_folder_uids("x", "5"), 1);
    CHECK_EQ(compare_folder_uids("a", "b"), -1);
    CHECK_EQ(compare_folder_uids("", ""), 0);

    const ost_domain::DatabaseFolder alpha_9{"9", "alpha"};
    const ost_domain::DatabaseFolder alpha_10{"10", "alpha"};
    const ost_domain::DatabaseFolder upper_alpha{"1", "Alpha"};
    CHECK(folder_precedes(alpha_9, alpha_10));
    CHECK_FALSE(folder_precedes(alpha_10, alpha_9));
    CHECK_FALSE(folder_precedes(alpha_9, alpha_9));
    CHECK(folder_precedes(alpha_10, upper_alpha));                // lowercase first beats the smaller UID
    CHECK_FALSE(folder_precedes(upper_alpha, alpha_10));
}

// The bid comparator: a NULL BidNo first, then numeric; equal keys do not precede each other (the caller's stable sort keeps the reader order).
TEST(Brw13_BidOrder, TheBidComparatorPutsNullFirstThenNumbers)
{
    ost_domain::BidSummary none;
    ost_domain::BidSummary zero;
    zero.has_bid_no = true;
    zero.bid_no = 0;
    ost_domain::BidSummary five;
    five.has_bid_no = true;
    five.bid_no = 5;
    ost_domain::BidSummary negative;
    negative.has_bid_no = true;
    negative.bid_no = -3;

    CHECK(ost_domain::bid_precedes(none, zero));
    CHECK_FALSE(ost_domain::bid_precedes(zero, none));
    CHECK_FALSE(ost_domain::bid_precedes(none, none));
    CHECK(ost_domain::bid_precedes(zero, five));
    CHECK_FALSE(ost_domain::bid_precedes(five, zero));
    CHECK_FALSE(ost_domain::bid_precedes(five, five));
    CHECK(ost_domain::bid_precedes(negative, zero));
    CHECK(ost_domain::bid_precedes(none, negative));
}

// The system folder is an ordinary node of the order, wherever its name puts it (and wherever its UID is).
TEST(Brwm01_FolderOrder, TheSystemFolderSortsByNameLikeAnyOther)
{
    TempDb db("folder_order_system");
    REQUIRE(db.schema_ok());
    db.must("UPDATE BidProjects SET Name = 'Zulu' WHERE UID = 1");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'deleted bids')");
    db.must("INSERT INTO BidProjects (UID, Name) VALUES (3, 'Alpha')");

    CHECK_EQ(ost_btest::folder_names(load(db)), std::string("Alpha,deleted bids,Zulu"));
}
