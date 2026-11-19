// The page-folder tree load_project builds (database-persistence / project-browser parity):
//   DBP-20 (+BRW-14, LP-09)  nested folders of any depth survive, whatever the UID layout (no object moved out of a live map)
//   BRW-15 (+DBP-M01)        empty branches are pruned at every level (remove_empty_folders)
//   BRW-17                   roots and subfolders keep Windows order: Name, then UID; page UIDs keep (Sequence, Name, UID)
// Windows oracle: hierarchy_reader.py::_get_bid_folder_page_structure and parsers/utils/parser.py::remove_empty_folders
// (tests/infrastructure/mdb/components/test_hierarchy_reader.py). Cycles in ParentUID are BRW-16's (a decided item:
// failing the load); nothing here pins what a cyclic parent chain loads as.
#include "StoreTestSupport.hpp"

#include "FolderOracleData.hpp"

#include <algorithm>
#include <string>
#include <vector>

using ost_ptest::TempDb;

namespace
{
    // Bid 1 (opened) and bid 2 (foreign), both in the 'Deleted Bids' project that create_schema made.
    void seed_bids(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'one', 1)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'two', 2)");
    }

    void add_folder(const TempDb &db, int uid, int bid, const std::string &name, const std::string &parent_sql)
    {
        db.must("INSERT INTO BidPageFolders (UID, BidUID, Name, ParentUID) VALUES (" + std::to_string(uid) + ", " +
                std::to_string(bid) + ", " + ost_ptest::quoted(name) + ", " + parent_sql + ")");
    }

    void add_page(const TempDb &db, int uid, int bid, const std::string &name, const std::string &folder_sql,
                  const std::string &sequence_sql = "NULL")
    {
        db.must("INSERT INTO BidPages (UID, BidUID, Name, BidPageFolderUID, Sequence) VALUES (" + std::to_string(uid) +
                ", " + std::to_string(bid) + ", " + ost_ptest::quoted(name) + ", " + folder_sql + ", " + sequence_sql + ")");
    }

    ost_data::ProjectData load(const TempDb &db, const std::string &bid = "1")
    {
        return ost_store::load_project(db.path(), bid);
    }

    // uid{name;p=<pages>;s=<subfolders>} - the format of FolderOracleData.hpp.
    std::string dump_folder(const std::string &uid, const ost_domain::Folder &folder)
    {
        std::string out = uid + "{" + folder.name + ";p=";
        for (size_t i = 0; i < folder.page_uids.size(); ++i)
            out += (i ? "," : "") + folder.page_uids[i];
        if (!folder.subfolders.empty())
        {
            out += ";s=";
            bool first = true;
            for (const auto &pair : folder.subfolders)
            {
                out += (first ? "" : ",") + dump_folder(pair.first, pair.second);
                first = false;
            }
        }
        return out + "}";
    }

    std::string dump_top(const ost_data::ProjectData &project)
    {
        std::string out;
        bool first = true;
        for (const auto &pair : project.folders)
        {
            out += (first ? "" : "|") + dump_folder(pair.first, pair.second);
            first = false;
        }
        return out;
    }

    std::vector<int> sorted_loose(const ost_data::ProjectData &project)
    {
        std::vector<int> loose;
        for (const auto &pair : project.pages)
            if (pair.second.folder_uid.empty())
                loose.push_back(std::stoi(pair.first));
        std::sort(loose.begin(), loose.end());
        return loose;
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// DBP-20
// ---------------------------------------------------------------------------------------------------------------

// Port of test_hierarchy_reader.py::test_page_hierarchy_accepts_valid_multi_level_folder_chain: folders (10 Root, NULL),
// (11 Middle, 10), (12 Leaf, 11), page 20 in folder 12 -> Root > Middle > Leaf, the page only in the leaf.
TEST(Dbp20_PageFolderTree, ChainWithAscendingUidsIsNestedToTheLeaf)
{
    TempDb db("folders_chain_asc");
    seed_bids(db);
    add_folder(db, 10, 1, "Root", "NULL");
    add_folder(db, 11, 1, "Middle", "10");
    add_folder(db, 12, 1, "Leaf", "11");
    add_page(db, 20, 1, "A-101", "12", "1");

    const ost_data::ProjectData project = load(db);
    REQUIRE_EQ(project.folders.size(), static_cast<size_t>(1));
    CHECK_EQ(dump_top(project), std::string("10{Root;p=;s=11{Middle;p=;s=12{Leaf;p=20}}}"));
    const ost_domain::Folder &root = project.folders.at("10");
    CHECK_EQ(root.all_page_uids(), std::vector<std::string>{"20"});
    CHECK(root.page_uids.empty());
    CHECK_STR_EQ(project.pages.at("20").folder_uid, "12"); // the page keeps its folder, it is not loose
}

// BRW-14 plan: the child UID is smaller than the parent UID (rows are read in UID order, so the child comes first).
TEST(Dbp20_PageFolderTree, ChainWithDescendingUidsIsNestedToTheLeaf)
{
    TempDb db("folders_chain_desc");
    seed_bids(db);
    add_folder(db, 30, 1, "Root", "NULL");
    add_folder(db, 20, 1, "Middle", "30");
    add_folder(db, 10, 1, "Leaf", "20");
    add_page(db, 20, 1, "A-101", "10", "1");

    const ost_data::ProjectData project = load(db);
    CHECK_EQ(dump_top(project), std::string("30{Root;p=;s=20{Middle;p=;s=10{Leaf;p=20}}}"));
}

// LP-09 plan: every assignment of root / middle / leaf to three UIDs, for several UID ranges (the result must not depend
// on the order of the rows or of any hash table).
TEST(Dbp20_PageFolderTree, EveryUidLayoutOfAThreeLevelChainGivesTheSameTree)
{
    const int ranges[][3] = {{1, 2, 3}, {10, 11, 12}, {100, 200, 300}, {7, 3, 5}};
    for (const auto &range : ranges)
    {
        std::vector<int> roles = {range[0], range[1], range[2]};
        std::sort(roles.begin(), roles.end());
        int layout = 0;
        do
        {
            SCOPED_CONTEXT("uids root/middle/leaf = " + std::to_string(roles[0]) + "/" + std::to_string(roles[1]) + "/" +
                           std::to_string(roles[2]));
            TempDb db("folders_perm_" + std::to_string(range[0]) + "_" + std::to_string(layout++));
            seed_bids(db);
            add_folder(db, roles[0], 1, "Root", "NULL");
            add_folder(db, roles[1], 1, "Middle", std::to_string(roles[0]));
            add_folder(db, roles[2], 1, "Leaf", std::to_string(roles[1]));
            add_page(db, 900, 1, "P", std::to_string(roles[2]), "1");

            const ost_data::ProjectData project = load(db);
            const std::string expected = std::to_string(roles[0]) + "{Root;p=;s=" + std::to_string(roles[1]) +
                                         "{Middle;p=;s=" + std::to_string(roles[2]) + "{Leaf;p=900}}}";
            CHECK_EQ(dump_top(project), expected);
        } while (std::next_permutation(roles.begin(), roles.end()));
    }
}

// BRW-14 / DBP-20 note: a four-level chain lost its third and fourth folder (3 of 5 pages reachable on the MSVC host).
TEST(Dbp20_PageFolderTree, FourLevelChainKeepsEveryPage)
{
    TempDb db("folders_chain4");
    seed_bids(db);
    add_folder(db, 1, 1, "A", "NULL");
    add_folder(db, 2, 1, "B", "1");
    add_folder(db, 3, 1, "C", "2");
    add_folder(db, 4, 1, "D", "3");
    add_page(db, 50, 1, "pa", "1", "1");
    add_page(db, 51, 1, "pb", "2", "1");
    add_page(db, 52, 1, "pc", "3", "1");
    add_page(db, 53, 1, "pd1", "4", "1");
    add_page(db, 54, 1, "pd2", "4", "2");

    const ost_data::ProjectData project = load(db);
    CHECK_EQ(dump_top(project), std::string("1{A;p=50;s=2{B;p=51;s=3{C;p=52;s=4{D;p=53,54}}}}"));
    CHECK_EQ(project.folders.at("1").all_page_uids().size(), static_cast<size_t>(5));
    CHECK(sorted_loose(project).empty());
}

// Windows: a folder with a missing or cross-bid parent becomes a root; pages follow their folder.
// Port of test_orphaned_page_folder_is_recovered_as_a_root and test_cross_bid_page_folder_parent_is_recovered_as_a_root.
TEST(Dbp20_PageFolderTree, FolderWithAMissingOrForeignParentIsARoot)
{
    TempDb db("folders_orphan");
    seed_bids(db);
    add_folder(db, 10, 1, "Recovered", "999");          // parent does not exist
    add_folder(db, 11, 1, "ForeignParent", "99");       // parent 99 is a folder of bid 2
    add_folder(db, 99, 2, "Other bid", "NULL");
    add_page(db, 20, 1, "A-101", "10", "1");
    add_page(db, 21, 1, "A-102", "11", "1");
    add_page(db, 22, 2, "Other bid page", "99", "1");

    const ost_data::ProjectData project = load(db);
    CHECK_EQ(dump_top(project), std::string("11{ForeignParent;p=21}|10{Recovered;p=20}")); // Name order
    CHECK_STR_EQ(project.pages.at("20").folder_uid, "10");
    // load_project("2"): its folder is a root of its own bid, not related to bid 1
    CHECK_EQ(dump_top(load(db, "2")), std::string("99{Other bid;p=22}"));
}

// Port of test_folder_whose_parent_uid_is_one_nests_under_it and test_a_textual_zero_parent_uid_is_a_root_folder
// ('0' as text, a ParentUID of 0 and NULL are all "no parent").
TEST(Dbp20_PageFolderTree, ParentUidOneNestsAndZeroLikeValuesAreRoots)
{
    TempDb db("folders_parent_one");
    seed_bids(db);
    add_folder(db, 1, 1, "Root", "NULL");
    add_folder(db, 2, 1, "Kid", "1");
    add_folder(db, 3, 1, "ZeroText", "'0'");
    add_folder(db, 4, 1, "ZeroInt", "0");
    add_page(db, 20, 1, "p", "2", "1");
    add_page(db, 21, 1, "p", "3", "1");
    add_page(db, 22, 1, "p", "4", "1");

    const ost_data::ProjectData project = load(db);
    CHECK_EQ(dump_top(project), std::string("1{Root;p=;s=2{Kid;p=20}}|4{ZeroInt;p=22}|3{ZeroText;p=21}"));
    CHECK(sorted_loose(project).empty());
}

// Port of test_folders_without_a_parent_column_are_all_roots (ParentUID is an optional column) and
// test_page_structure_without_a_folder_table_lists_pages_loose (BidPageFolders is an optional table).
TEST(Dbp20_PageFolderTree, OptionalParentColumnAndOptionalFolderTable)
{
    {
        TempDb db("folders_no_parent_column");
        seed_bids(db);
        db.must("DROP TABLE BidPageFolders");
        db.must("CREATE TABLE BidPageFolders (UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT, Description BLOB)");
        db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (1, 1, 'Root')");
        db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (2, 1, 'Kid')");
        add_page(db, 20, 1, "p", "1", "1");
        add_page(db, 21, 1, "q", "2", "1");
        CHECK_EQ(dump_top(load(db)), std::string("2{Kid;p=21}|1{Root;p=20}")); // all roots, Name order
    }
    {
        TempDb db("folders_no_description_column");
        seed_bids(db);
        db.must("DROP TABLE BidPageFolders");
        db.must("CREATE TABLE BidPageFolders (UID INTEGER PRIMARY KEY, BidUID INTEGER, ParentUID INTEGER, Name TEXT)");
        db.must("INSERT INTO BidPageFolders (UID, BidUID, Name, ParentUID) VALUES (1, 1, 'Root', NULL)");
        db.must("INSERT INTO BidPageFolders (UID, BidUID, Name, ParentUID) VALUES (2, 1, 'Kid', 1)");
        add_page(db, 21, 1, "q", "2", "1");
        CHECK_EQ(dump_top(load(db)), std::string("1{Root;p=;s=2{Kid;p=21}}"));
    }
    {
        TempDb db("folders_no_folder_table");
        seed_bids(db);
        db.must("DROP TABLE BidPageFolders");
        add_page(db, 20, 1, "p", "10", "1");
        const ost_data::ProjectData project = load(db);
        REQUIRE(!project.empty());
        CHECK(project.folders.empty());
        CHECK_EQ(sorted_loose(project), std::vector<int>{20});
        CHECK_STR_EQ(project.pages.at("20").folder_uid, "");
    }
}

// The load of a cyclic ParentUID chain must terminate (what it loads is BRW-16's decision).
TEST(Dbp20_PageFolderTree, ACyclicParentChainDoesNotHangTheLoad)
{
    TempDb db("folders_cycle_terminates");
    seed_bids(db);
    add_folder(db, 10, 1, "A", "11");
    add_folder(db, 11, 1, "B", "10");
    add_folder(db, 12, 1, "Self", "12");
    add_folder(db, 13, 1, "Fine", "NULL");
    add_page(db, 20, 1, "p", "13", "1");
    (void)load(db);
    CHECK(true);
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-15 / BRW-17
// ---------------------------------------------------------------------------------------------------------------

// Port of test_hierarchy_reader.py::test_folders_nest_by_parent_drop_empty_branches_and_keep_name_order (same rows):
// roots in Name order Alpha(11), Orphan(15), Zeta(10); Zeta's subfolders Child A(13), Child B(12) in that order; the empty
// folder 14 is gone; page 24 (a folder of the other bid) is loose.
TEST(Brw17_PageFolderOrder, WindowsFixtureNestsPrunesAndKeepsNameOrder)
{
    TempDb db("folders_windows_fixture");
    seed_bids(db);
    add_folder(db, 10, 1, "Zeta", "NULL");
    add_folder(db, 11, 1, "Alpha", "0");
    add_folder(db, 12, 1, "Child B", "10");
    add_folder(db, 13, 1, "Child A", "10");
    add_folder(db, 14, 1, "Empty", "NULL");
    add_folder(db, 15, 1, "Orphan", "999");
    add_folder(db, 16, 2, "Other bid", "NULL");
    db.must("UPDATE BidPageFolders SET Description = 'zd' WHERE UID = 10");
    db.must("UPDATE BidPageFolders SET Description = 'ad' WHERE UID = 11");
    db.must("UPDATE BidPageFolders SET Description = 'cd' WHERE UID = 13");
    add_page(db, 20, 1, "P-a", "13", "1");
    add_page(db, 21, 1, "P-b", "12", "1");
    add_page(db, 22, 1, "P-c", "11", "1");
    add_page(db, 23, 1, "P-d", "15", "1");
    add_page(db, 24, 1, "P-e", "16", "1");
    add_page(db, 25, 1, "P-f", "14", "1");
    db.must("DELETE FROM BidPages WHERE UID = 25");

    const ost_data::ProjectData project = load(db);
    CHECK_EQ(dump_top(project),
             std::string("11{Alpha;p=22}|15{Orphan;p=23}|10{Zeta;p=;s=13{Child A;p=20},12{Child B;p=21}}"));
    CHECK_STR_EQ(project.folders.at("11").description, "ad");
    CHECK_STR_EQ(project.folders.at("10").description, "zd");
    CHECK_STR_EQ(project.folders.at("10").subfolders.at("13").description, "cd");
    CHECK_STR_EQ(project.folders.at("10").subfolders.at("12").description, "");
    CHECK_EQ(project.folders.count("14"), static_cast<size_t>(0));
    CHECK_EQ(sorted_loose(project), std::vector<int>{24});
    CHECK_STR_EQ(project.pages.at("24").folder_uid, "");
}

// BRW-15 plan: a nested empty folder under 10 must be absent too (BRW-15: "only roots are pruned" today).
TEST(Brw15_EmptyBranches, EmptyNestedFoldersAreRemovedAtEveryLevel)
{
    {
        // Folders (10 Root,NULL), (11 EmptyChild,10), page 20 in folder 10 -> {10: pages [20], subfolders {}}.
        TempDb db("empty_child");
        seed_bids(db);
        add_folder(db, 10, 1, "Root", "NULL");
        add_folder(db, 11, 1, "EmptyChild", "10");
        add_page(db, 20, 1, "p", "10", "1");
        const ost_data::ProjectData project = load(db);
        CHECK_EQ(dump_top(project), std::string("10{Root;p=20}"));
        CHECK(project.folders.at("10").subfolders.empty());
    }
    {
        // (10 Root), (11 Mid,10), (12 Leaf,11), (13 EmptyLeaf,12), page 20 in 10, page 21 in 12:
        // 10{pages [20], subfolders {11{subfolders {12{pages [21]}}}}} with 13 pruned.
        TempDb db("empty_leaf");
        seed_bids(db);
        add_folder(db, 10, 1, "Root", "NULL");
        add_folder(db, 11, 1, "Mid", "10");
        add_folder(db, 12, 1, "Leaf", "11");
        add_folder(db, 13, 1, "EmptyLeaf", "12");
        add_page(db, 20, 1, "p", "10", "1");
        add_page(db, 21, 1, "q", "12", "1");
        CHECK_EQ(dump_top(load(db)), std::string("10{Root;p=20;s=11{Mid;p=;s=12{Leaf;p=21}}}"));
    }
    {
        // a whole branch without pages disappears with all its folders; an unrelated empty root too
        TempDb db("empty_branch");
        seed_bids(db);
        add_folder(db, 1, 1, "Keep", "NULL");
        add_folder(db, 2, 1, "Branch", "NULL");
        add_folder(db, 3, 1, "BranchChild", "2");
        add_folder(db, 4, 1, "BranchGrandChild", "3");
        add_folder(db, 5, 1, "Lonely", "NULL");
        add_page(db, 20, 1, "p", "1", "1");
        CHECK_EQ(dump_top(load(db)), std::string("1{Keep;p=20}"));
    }
    {
        // Windows fixture + a nested empty folder 17 under 10: absent
        TempDb db("empty_nested_in_fixture");
        seed_bids(db);
        add_folder(db, 10, 1, "Zeta", "NULL");
        add_folder(db, 12, 1, "Child B", "10");
        add_folder(db, 17, 1, "Nested empty", "10");
        add_page(db, 21, 1, "P-b", "12", "1");
        CHECK_EQ(dump_top(load(db)), std::string("10{Zeta;p=;s=12{Child B;p=21}}"));
    }
}

// BRW-17: equal names are ordered by UID (SQL ORDER BY [Name], [UID]), as roots and as subfolders.
TEST(Brw17_PageFolderOrder, EqualNamesAreOrderedByUid)
{
    TempDb db("folders_same_name");
    seed_bids(db);
    add_folder(db, 30, 1, "Same", "NULL");
    add_folder(db, 10, 1, "Same", "NULL");
    add_folder(db, 20, 1, "Same", "NULL");
    add_folder(db, 40, 1, "Parent", "NULL");
    add_folder(db, 33, 1, "Kid", "40");
    add_folder(db, 31, 1, "Kid", "40");
    add_folder(db, 32, 1, "Kid", "40");
    for (int uid : {30, 10, 20, 33, 31, 32})
        add_page(db, 100 + uid, 1, "p", std::to_string(uid), "1");

    CHECK_EQ(dump_top(load(db)),
             std::string("40{Parent;p=;s=31{Kid;p=131},32{Kid;p=132},33{Kid;p=133}}|10{Same;p=110}|20{Same;p=120}|30{Same;p=130}"));
}

// The page UIDs of a folder keep the Windows page order: Sequence, then Name, then UID (NULL first), not the order of a hash
// table. Port of test_pages_are_ordered_by_sequence_then_name_then_uid (BidPages of a folder).
TEST(Brw17_PageFolderOrder, PagesOfAFolderAreOrderedBySequenceNameUid)
{
    TempDb db("folders_page_order");
    seed_bids(db);
    add_folder(db, 1, 1, "F", "NULL");
    add_page(db, 5, 1, "A", "1", "2");
    add_page(db, 4, 1, "B", "1", "2");
    add_page(db, 3, 1, "B", "1", "2");
    add_page(db, 2, 1, "Z", "1", "1");
    add_page(db, 1, 2, "Foreign", "1", "0");
    add_page(db, 6, 1, "N", "1", "NULL");
    CHECK_EQ(dump_top(load(db)), std::string("1{F;p=6,2,5,3,4}"));
}

// LP-08 fixture (Windows page_combo walk): Folder{Nested[p3, p4]; own pages p1, p2}, Other[p5], loose p6. The walk lists the
// subfolders before the folder's own pages: p3, p4, p1, p2, p5; roots in Name order.
TEST(Brw17_PageFolderOrder, AllPageUidsWalksSubfoldersBeforeOwnPages)
{
    TempDb db("folders_walk");
    seed_bids(db);
    add_folder(db, 10, 1, "Folder", "NULL");
    add_folder(db, 11, 1, "Nested", "10");
    add_folder(db, 12, 1, "Other", "NULL");
    add_page(db, 1, 1, "p1", "10", "1");
    add_page(db, 2, 1, "p2", "10", "2");
    add_page(db, 3, 1, "p3", "11", "1");
    add_page(db, 4, 1, "p4", "11", "2");
    add_page(db, 5, 1, "p5", "12", "1");
    add_page(db, 6, 1, "p6", "NULL", "1");

    const ost_data::ProjectData project = load(db);
    std::vector<std::string> walk;
    for (const auto &pair : project.folders)
    {
        const std::vector<std::string> pages = pair.second.all_page_uids();
        walk.insert(walk.end(), pages.begin(), pages.end());
    }
    CHECK_EQ(walk, (std::vector<std::string>{"3", "4", "1", "2", "5"}));
    CHECK_EQ(sorted_loose(project), std::vector<int>{6});
}

// Differential: 60 random acyclic page-folder trees (names with case / accent / punctuation / duplicates, orphan, foreign
// and zero parents, pages in missing or foreign folders, NULL / equal sequences) against the real Windows reader.
TEST(Brw17_PageFolderOrder, RandomTreesMatchTheWindowsReader)
{
    int index = 0;
    for (const ost_ptest::FolderOracleCase &oracle : ost_ptest::kFolderOracleCases)
    {
        SCOPED_CONTEXT("oracle case " + std::to_string(index));
        TempDb db("folders_oracle_" + std::to_string(index++));
        seed_bids(db);
        db.must(std::string("INSERT INTO BidPageFolders (UID, BidUID, Name, Description, ParentUID) VALUES ") + oracle.folders);
        db.must(std::string("INSERT INTO BidPages (UID, BidUID, Name, BidPageFolderUID, Sequence) VALUES ") + oracle.pages);

        const ost_data::ProjectData project = load(db);
        const std::string expected(oracle.expected);
        const size_t loose_at = expected.find("||loose=");
        REQUIRE(loose_at != std::string::npos);
        CHECK_EQ(dump_top(project), expected.substr(0, loose_at));

        std::vector<int> expected_loose;
        std::string rest = expected.substr(loose_at + 8);
        size_t from = 0;
        while (from < rest.size())
        {
            size_t comma = rest.find(',', from);
            if (comma == std::string::npos)
                comma = rest.size();
            expected_loose.push_back(std::stoi(rest.substr(from, comma - from)));
            from = comma + 1;
        }
        std::sort(expected_loose.begin(), expected_loose.end());
        CHECK_EQ(sorted_loose(project), expected_loose);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The ordered container behind Folder::subfolders / ProjectData::folders
// ---------------------------------------------------------------------------------------------------------------

TEST(Brw17_OrderedUidMap, KeepsInsertionOrderAndOffersTheUnorderedMapLookups)
{
    ost_domain::OrderedUidMap<int> map;
    CHECK(map.empty());
    map["b"] = 2;
    map["a"] = 1;
    map["c"] = 3;
    map["a"] = 10; // an existing key keeps its position
    CHECK_EQ(map.size(), static_cast<size_t>(3));
    std::string order;
    for (const auto &pair : map)
        order += pair.first;
    CHECK_STR_EQ(order, "bac");
    CHECK_EQ(map.at("a"), 10);
    CHECK_EQ(map.count("c"), static_cast<size_t>(1));
    CHECK_EQ(map.count("z"), static_cast<size_t>(0));
    CHECK(map.find("z") == map.end());
    CHECK_EQ(map.find("b")->second, 2);
    CHECK(map.emplace("d", 4).second);
    CHECK_FALSE(map.emplace("d", 5).second);
    CHECK_EQ(map.at("d"), 4);
    bool threw = false;
    try
    {
        (void)map.at("missing");
    }
    catch (const std::out_of_range &)
    {
        threw = true;
    }
    CHECK(threw);
    const ost_domain::OrderedUidMap<int> &view = map;
    CHECK_EQ(view.at("c"), 3);
    CHECK_EQ(view.size(), static_cast<size_t>(4));
    map.clear();
    CHECK(map.empty());
}

TEST(Brw17_OrderedUidMap, AFolderHoldsItsSubfoldersInOrderThroughCopies)
{
    ost_domain::Folder root;
    root.uid = "1";
    root.subfolders["z"].name = "Zed";
    root.subfolders["a"].name = "Aye";
    root.subfolders["a"].page_uids = {"7"};
    ost_domain::Folder copy = root; // deep copy keeps the order and the nested values
    root.subfolders["z"].name = "changed";
    std::string order;
    for (const auto &pair : copy.subfolders)
        order += pair.first + ":" + pair.second.name + ";";
    CHECK_STR_EQ(order, "z:Zed;a:Aye;");
    CHECK_EQ(copy.all_page_uids(), std::vector<std::string>{"7"});
    CHECK_FALSE(copy.empty());
    CHECK(root.subfolders.at("z").page_uids.empty());
}
