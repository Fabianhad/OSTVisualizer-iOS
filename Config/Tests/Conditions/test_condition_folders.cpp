// COND-FOLDER-01 (aliases QTY-20, DBP-32, DBP-30): condition folders form a tree through ParentUID; conditions without a
// folder (or with an unknown folder uid) sit at the root next to the root folders; empty folders are shown; folders and
// condition-type groups sort by name at every level; rows sort by reference number; a condition type that has no
// CdnTypes row is the group "Unknown".
//
// Windows: ost_visualizer/presentation/components/conditions_sidebar.py (_rebuild_tree, _build_folder_tree,
// _build_cdn_type_groups; default group-by-type and default sort "No." ascending), infrastructure/mdb/components/
// bid_data_reader.py _parse_bid_condition_folders_for_bid, infrastructure/database/bid_owned_identity.py
// require_acyclic_bid_owned_parent_graph. The expected trees of the oracle cases were produced by driving the Windows
// sidebar headless (see TreeOracleData.hpp).
#include "ConditionsTestSupport.hpp"
#include "CycleOracleData.hpp"
#include "TreeOracleData.hpp"

#include "Domain/ConditionTree.hpp"

#include <algorithm>
#include <map>

using ost_ctest::TempDb;
using ost_domain::CdnType;
using ost_domain::Condition;
using ost_domain::ConditionFolder;
using ost_domain::ConditionTreeNode;

namespace
{
    struct Inputs
    {
        std::unordered_map<std::string, Condition> conditions;
        std::unordered_map<std::string, ConditionFolder> folders;
        std::unordered_map<std::string, CdnType> cdn_types;
    };

    // Builds the inputs of a case; `reverse` inserts everything in the opposite order (an unordered_map has no
    // order, the result must not depend on it).
    Inputs make_inputs(const ost_ctest::TreeCase &c, bool reverse = false)
    {
        Inputs in;
        std::vector<ost_ctest::TreeFolderSpec> folders(c.folders.begin(), c.folders.end());
        std::vector<ost_ctest::TreeConditionSpec> conditions(c.conditions.begin(), c.conditions.end());
        std::vector<ost_ctest::TreeTypeSpec> types(c.types.begin(), c.types.end());
        if (reverse)
        {
            std::reverse(folders.begin(), folders.end());
            std::reverse(conditions.begin(), conditions.end());
            std::reverse(types.begin(), types.end());
        }
        for (const auto &f : folders)
        {
            ConditionFolder folder;
            folder.uid = f.uid;
            folder.name = f.name;
            folder.parent_uid = f.parent;
            in.folders[folder.uid] = folder;
        }
        for (const auto &t : types)
            in.cdn_types[t.uid] = CdnType{t.uid, t.name};
        for (const auto &s : conditions)
        {
            Condition condition;
            condition.uid = s.uid;
            condition.name = s.name;
            condition.ref_no = s.ref_no;
            condition.condition_folder_uid = s.folder;
            condition.cdn_type_uid = s.type;
            in.conditions[condition.uid] = condition;
        }
        return in;
    }

    // "<depth>|<kind>|<uid>|<name>" like the oracle data
    std::vector<std::string> flatten(const std::vector<ConditionTreeNode> &nodes)
    {
        std::vector<std::string> out;
        for (const ConditionTreeNode &n : nodes)
        {
            const char *kind = n.kind == ConditionTreeNode::Kind::Folder      ? "F"
                               : n.kind == ConditionTreeNode::Kind::TypeGroup ? "G"
                                                                              : "C";
            out.push_back(std::to_string(n.depth) + "|" + kind + "|" + n.uid + "|" + n.name);
        }
        return out;
    }

    std::vector<std::string> tree_of(const Inputs &in)
    {
        return flatten(ost_domain::build_condition_tree(in.conditions, in.folders, in.cdn_types));
    }

    std::string join(const std::vector<std::string> &lines)
    {
        std::string out;
        for (const std::string &line : lines)
            out += (out.empty() ? "" : "\n") + line;
        return out;
    }

    // the whole oracle case, compared as text so a failure shows both trees
    void check_case(const ost_ctest::TreeCase &c, bool reverse)
    {
        const Inputs in = make_inputs(c, reverse);
        const std::string got = join(tree_of(in));
        std::vector<std::string> expected(c.expected.begin(), c.expected.end());
        const std::string want = join(expected);
        if (got != want)
            ost_test::report_failure(__FILE__, __LINE__, "tree differs from the Windows sidebar:\n--- got\n" + got +
                                                             "\n--- expected (Windows)\n" + want);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// build_condition_tree
// ---------------------------------------------------------------------------------------------------------------------

// COND-FOLDER-01 test plan: folders {F1 'Zeta', F2 'Alpha' (parent F1), F3 'Empty'} and conditions {c1 no folder no type,
// c2 folder F2 no type, c3 folder 'ghost' no type, c4 type uid 'T9' absent from CdnTypes}. Windows tree (siblings sorted
// by name, rows by ref_no): root -> ['(unassigned)' group [c3, c1], 'Empty' (shown although empty), 'Unknown' group [c4],
// 'Zeta' -> 'Alpha' -> '(unassigned)' group [c2]].
TEST(ConditionTree, TheCondFolder01TestPlanCase)
{
    Inputs in;
    in.folders["F1"] = ConditionFolder{"F1", "Zeta", ""};
    in.folders["F2"] = ConditionFolder{"F2", "Alpha", "F1"};
    in.folders["F3"] = ConditionFolder{"F3", "Empty", ""};
    auto add = [&in](const char *uid, int ref_no, const char *folder, const char *type)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.ref_no = ref_no;
        c.condition_folder_uid = folder;
        c.cdn_type_uid = type;
        in.conditions[uid] = c;
    };
    add("c1", 5, "", "");
    add("c2", 1, "F2", "");
    add("c3", 2, "ghost", "");
    add("c4", 3, "", "T9");

    const std::vector<std::string> expected = {
        "0|G||(unassigned)", "1|C|c3|c3",       "1|C|c1|c1", "0|F|F3|Empty",        "0|G|T9|Unknown",
        "1|C|c4|c4",         "0|F|F1|Zeta",     "1|F|F2|Alpha", "2|G||(unassigned)", "3|C|c2|c2"};
    CHECK(tree_of(in) == expected);
}

// QTY-20 plan: folders F1 'Alpha', F2 'Beta' and one condition without a folder: the condition sits at the ROOT (in the
// "(unassigned)" group), not inside the alphabetically first folder, and both empty folders still appear.
TEST(ConditionTree, AFolderlessConditionSitsAtTheRootAndEmptyFoldersAreShown)
{
    Inputs in;
    in.folders["1"] = ConditionFolder{"1", "Alpha", ""};
    in.folders["2"] = ConditionFolder{"2", "Beta", ""};
    Condition c;
    c.uid = "10";
    c.name = "Wall A";
    c.ref_no = 1;
    in.conditions["10"] = c;

    const std::vector<std::string> expected = {"0|G||(unassigned)", "1|C|10|Wall A", "0|F|1|Alpha", "0|F|2|Beta"};
    CHECK(tree_of(in) == expected);
}

// No folders at all: the groups are the root's children (what iOS always did in this case).
TEST(ConditionTree, WithoutFoldersTheTypeGroupsAreTheRootChildren)
{
    Inputs in;
    in.cdn_types["T1"] = CdnType{"T1", "Concrete"};
    for (int i = 1; i <= 3; ++i)
    {
        Condition c;
        c.uid = std::to_string(i);
        c.name = "n" + std::to_string(i);
        c.ref_no = 4 - i;
        c.cdn_type_uid = i == 2 ? "T1" : "";
        in.conditions[c.uid] = c;
    }
    const std::vector<std::string> expected = {"0|G||(unassigned)", "1|C|3|n3", "1|C|1|n1", "0|G|T1|Concrete", "1|C|2|n2"};
    CHECK(tree_of(in) == expected);
}

TEST(ConditionTree, NothingGivesAnEmptyTree)
{
    Inputs in;
    CHECK(ost_domain::build_condition_tree(in.conditions, in.folders, in.cdn_types).empty());
}

// COND-FOLDER-01 / DBP-30: a condition type uid that has no CdnTypes row is titled "Unknown" (Windows cdn_type_name
// default), in a group of its own next to the real "(unassigned)" group (conditions without a type).
TEST(ConditionTree, ADanglingTypeUidIsTheUnknownGroupNextToTheUnassignedGroup)
{
    Inputs in;
    in.cdn_types["5"] = CdnType{"5", "Walls"};
    auto add = [&in](const char *uid, const char *type)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.ref_no = 1;
        c.cdn_type_uid = type;
        in.conditions[uid] = c;
    };
    add("1", "99"); // no such type
    add("2", "");   // no type
    add("3", "5");  // a real type
    const std::vector<std::string> expected = {"0|G||(unassigned)", "1|C|2|2", "0|G|99|Unknown", "1|C|1|1",
                                               "0|G|5|Walls",       "1|C|3|3"};
    CHECK(tree_of(in) == expected);
}

// Port of the sidebar tests: tests/presentation/components/test_conditions_sidebar.py::
// test_condition_sidebar_groups_unassigned_conditions_before_named_types (an untyped condition's group comes first).
TEST(ConditionTree, TheUnassignedGroupSortsBeforeNamedTypes)
{
    Inputs in;
    in.cdn_types["type-1"] = CdnType{"type-1", "Type B"};
    Condition typed;
    typed.uid = "c1";
    typed.name = "Typed";
    typed.ref_no = 1;
    typed.cdn_type_uid = "type-1";
    Condition untyped;
    untyped.uid = "c2";
    untyped.name = "Untyped";
    untyped.ref_no = 2;
    in.conditions["c1"] = typed;
    in.conditions["c2"] = untyped;
    const std::vector<std::string> expected = {"0|G||(unassigned)", "1|C|c2|Untyped", "0|G|type-1|Type B",
                                               "1|C|c1|Typed"};
    CHECK(tree_of(in) == expected);
}

// A folder whose ParentUID names a folder that does not exist (or another bid's) is a root folder; a deep chain nests.
TEST(ConditionTree, ADanglingParentMakesARootFolderAndChainsNest)
{
    Inputs in;
    in.folders["1"] = ConditionFolder{"1", "A", ""};
    in.folders["2"] = ConditionFolder{"2", "B", "1"};
    in.folders["3"] = ConditionFolder{"3", "C", "2"};
    in.folders["5"] = ConditionFolder{"5", "Orphan", "99"};
    Condition c;
    c.uid = "10";
    c.name = "x";
    c.ref_no = 1;
    c.condition_folder_uid = "3";
    in.conditions["10"] = c;
    const std::vector<std::string> expected = {"0|F|1|A",      "1|F|2|B",           "2|F|3|C", "3|G||(unassigned)",
                                               "4|C|10|x",     "0|F|5|Orphan"};
    CHECK(tree_of(in) == expected);
}

// Rows of a group sort by reference number (numeric, negative and zero included); equal numbers keep database read order
// (ascending UID here: the iOS containers carry no order), at every level.
TEST(ConditionTree, RowsSortByReferenceNumberAndTiesKeepUidOrder)
{
    Inputs in;
    struct Row
    {
        const char *uid;
        int ref;
    };
    for (const Row &r : {Row{"7", 2}, Row{"3", 2}, Row{"12", 2}, Row{"2", -3}, Row{"9", 0}, Row{"4", 100}, Row{"20", 1}})
    {
        Condition c;
        c.uid = r.uid;
        c.name = r.uid;
        c.ref_no = r.ref;
        in.conditions[r.uid] = c;
    }
    // uid order 2,3,4,7,9,12,20 is the load order: refs -3 (2), 0 (9), 1 (20), 2 (3, 7, 12), 100 (4)
    const std::vector<std::string> expected = {"0|G||(unassigned)", "1|C|2|2", "1|C|9|9", "1|C|20|20",
                                               "1|C|3|3",           "1|C|7|7", "1|C|12|12", "1|C|4|4"};
    CHECK(tree_of(in) == expected);
}

// UIDs compare numerically for the load order ("9" before "12", not after).
TEST(ConditionTree, LoadOrderComparesUidsNumerically)
{
    Inputs in;
    for (const char *uid : {"12", "9", "100", "2"})
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.ref_no = 1;
        in.conditions[uid] = c;
    }
    const std::vector<std::string> expected = {"0|G||(unassigned)", "1|C|2|2", "1|C|9|9", "1|C|12|12", "1|C|100|100"};
    CHECK(tree_of(in) == expected);
}

// Names compare by code point (Python str), i.e. byte order of UTF-8: upper case before lower case, digits before
// letters, accented letters after 'z'; a folder comes before a type group of the same name.
TEST(ConditionTree, SiblingsSortByCodePointAndAFolderPrecedesAGroupOfTheSameName)
{
    Inputs in;
    in.folders["1"] = ConditionFolder{"1", "zeta", ""};
    in.folders["2"] = ConditionFolder{"2", "Zeta", ""};
    in.folders["3"] = ConditionFolder{"3", "\xC3\x89psilon", ""}; // U+00C9
    in.folders["4"] = ConditionFolder{"4", "(unassigned)", ""};
    in.folders["5"] = ConditionFolder{"5", "9", ""};
    in.folders["6"] = ConditionFolder{"6", "10", ""};
    Condition c;
    c.uid = "10";
    c.name = "x";
    c.ref_no = 1;
    in.conditions["10"] = c;
    const std::vector<std::string> expected = {
        "0|F|4|(unassigned)", "0|G||(unassigned)", "1|C|10|x", "0|F|6|10", "0|F|5|9",
        "0|F|2|Zeta",         "0|F|1|zeta",        "0|F|3|\xC3\x89psilon"};
    CHECK(tree_of(in) == expected);
}

// Folders with the same name keep read order (ascending uid), also the groups of two types with the same name.
TEST(ConditionTree, EqualNamesKeepLoadOrder)
{
    Inputs in;
    in.folders["8"] = ConditionFolder{"8", "Same", ""};
    in.folders["3"] = ConditionFolder{"3", "Same", ""};
    in.cdn_types["20"] = CdnType{"20", "Wall"};
    in.cdn_types["10"] = CdnType{"10", "Wall"};
    auto add = [&in](const char *uid, const char *type, const char *folder)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.ref_no = 1;
        c.cdn_type_uid = type;
        c.condition_folder_uid = folder;
        in.conditions[uid] = c;
    };
    add("1", "20", "");
    add("2", "10", "");
    add("4", "20", "3");
    add("5", "20", "8");
    // root groups: type 20 first appears with condition 1, type 10 with condition 2 -> 20 before 10 (equal titles)
    const std::vector<std::string> expected = {"0|F|3|Same", "1|G|20|Wall", "2|C|4|4",  "0|F|8|Same", "1|G|20|Wall",
                                               "2|C|5|5",    "0|G|20|Wall", "1|C|1|1",  "0|G|10|Wall", "1|C|2|2"};
    CHECK(tree_of(in) == expected);
}

// The result does not depend on the order the containers were filled in.
TEST(ConditionTree, TheResultDoesNotDependOnInsertionOrder)
{
    for (const ost_ctest::TreeCase &c : ost_ctest::tree_oracle_cases())
    {
        SCOPED_CONTEXT(std::string("case ") + c.label);
        CHECK(tree_of(make_inputs(c, false)) == tree_of(make_inputs(c, true)));
    }
}

// Folders that form a ParentUID cycle can never be reached from the root (the loader rejects such a database); building
// the tree must still terminate.
TEST(ConditionTree, AParentCycleTerminates)
{
    Inputs in;
    in.folders["1"] = ConditionFolder{"1", "A", "2"};
    in.folders["2"] = ConditionFolder{"2", "B", "1"};
    in.folders["3"] = ConditionFolder{"3", "Self", "3"};
    in.folders["4"] = ConditionFolder{"4", "Fine", ""};
    const std::vector<ConditionTreeNode> nodes = ost_domain::build_condition_tree(in.conditions, in.folders, in.cdn_types);
    REQUIRE_EQ(nodes.size(), size_t(1));
    CHECK_EQ(nodes[0].uid, "4");
}

// The Windows oracle cases: the sidebar tree for 16 inputs (the plan case, ties, code point order, dangling parents, zero
// and negative reference numbers and ten random worlds).
TEST(ConditionTree, MatchesTheWindowsSidebarOnTheOracleCases)
{
    for (const ost_ctest::TreeCase &c : ost_ctest::tree_oracle_cases())
    {
        SCOPED_CONTEXT(std::string("case ") + c.label);
        check_case(c, false);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// the loader: ParentUID of the folders (DBP-32) and the cycle rule
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
    void seed_bid(const TempDb &db)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid one', 1)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Bid two', 2)");
    }

    std::string parents_of(const ost_data::ProjectData &project)
    {
        std::map<std::string, std::string> sorted;
        for (const auto &pair : project.condition_folders)
            sorted[pair.first] = pair.second.parent_uid;
        std::string out;
        for (const auto &pair : sorted)
            out += (out.empty() ? "" : ",") + pair.first + ":" + pair.second;
        return out;
    }
}

// Port of tests/infrastructure/mdb/components/test_bid_data_reader.py::
// test_condition_folder_parent_reads_as_empty_for_null_zero_and_blank
TEST(ConditionFolderLoad, ParentReadsAsEmptyForNullZeroAndBlank)
{
    TempDb db("folders_parent_empty");
    REQUIRE(db.schema_ok());
    seed_bid(db);
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (1, 1, 'Null', NULL)");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (2, 1, 'Zero', 0)");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (3, 1, 'Zero text', '0')");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (4, 1, 'Blank', '')");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (5, 1, 'Child of four', 4)");

    const ost_data::ProjectData project = ost_store::load_project(db.path(), "1");
    REQUIRE(!project.empty());
    CHECK_EQ(parents_of(project), "1:,2:,3:,4:,5:4");
}

// DBP-32 plan: (1,'Root',0), (2,'Child',1) -> folder 2's parent is '1' and folder 1 has an empty parent.
TEST(ConditionFolderLoad, AChildFolderKnowsItsParent)
{
    TempDb db("folders_child");
    REQUIRE(db.schema_ok());
    seed_bid(db);
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (1, 1, 'Root', 0)");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (2, 1, 'Child', 1)");

    const ost_data::ProjectData project = ost_store::load_project(db.path(), "1");
    REQUIRE_EQ(project.condition_folders.size(), size_t(2));
    CHECK_EQ(project.condition_folders.at("2").parent_uid, "1");
    CHECK_EQ(project.condition_folders.at("1").parent_uid, "");
    CHECK_EQ(project.condition_folders.at("2").name, "Child");
}

// Port of ...::test_condition_folder_reader_preserves_orphan_and_valid_chain_contracts: a chain loads as a chain, a
// parent that is missing or belongs to another bid is kept as written (the tree puts such a folder at the root).
TEST(ConditionFolderLoad, OrphanAndValidChainParentsArePreserved)
{
    TempDb db("folders_orphans");
    REQUIRE(db.schema_ok());
    seed_bid(db);
    const char *rows[] = {"(7, 1, 'Root', NULL)", "(8, 1, 'Middle', 7)", "(9, 1, 'Leaf', 8)",
                          "(10, 1, 'Missing parent', 99)", "(99, 2, 'Other bid', NULL)", "(11, 1, 'Cross-bid parent', 99)"};
    for (const char *row : rows)
        db.must(std::string("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES ") + row);

    const ost_data::ProjectData project = ost_store::load_project(db.path(), "1");
    REQUIRE(!project.empty());
    CHECK_EQ(project.condition_folders.size(), size_t(5));
    CHECK_EQ(parents_of(project), "10:99,11:99,7:,8:7,9:8");
    CHECK_EQ(project.condition_folders.at("9").name, "Leaf");
}

// Port of ...::test_condition_folder_without_a_parent_column_reads_as_empty: an older database without ParentUID.
TEST(ConditionFolderLoad, ADatabaseWithoutAParentColumnReadsFlatFolders)
{
    TempDb db("folders_no_parent_column");
    REQUIRE(db.schema_ok());
    seed_bid(db);
    db.must("DROP TABLE BidConditionFolders");
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER, BidUID INTEGER, Name TEXT)");
    db.must("INSERT INTO BidConditionFolders VALUES (1, 1, 'Old file')");

    const ost_data::ProjectData project = ost_store::load_project(db.path(), "1");
    REQUIRE_EQ(project.condition_folders.size(), size_t(1));
    CHECK_EQ(project.condition_folders.at("1").parent_uid, "");
    CHECK_EQ(project.condition_folders.at("1").name, "Old file");
}

// Port of ...::test_condition_folder_reader_rejects_parent_cycles: a ParentUID cycle is rejected (Windows raises
// "BidConditionFolders.UID=7 participates in a ParentUID cycle"); iOS reports an unreadable bid as an empty project and
// the load overload that reports the cause carries the same message (the Windows test asserts UID=7 for all three).
TEST(ConditionFolderLoad, AParentCycleIsRejected)
{
    struct Row
    {
        int uid;
        int parent;
    };
    const std::vector<std::vector<Row>> fixtures = {{{7, 7}}, {{7, 8}, {8, 7}}, {{7, 8}, {8, 9}, {9, 7}}};
    int index = 0;
    for (const auto &rows : fixtures)
    {
        SCOPED_CONTEXT("fixture " + std::to_string(index++));
        TempDb db("folders_cycle");
        REQUIRE(db.schema_ok());
        seed_bid(db);
        for (const Row &row : rows)
            db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (" + std::to_string(row.uid) +
                    ", 1, 'Folder', " + std::to_string(row.parent) + ")");
        CHECK(ost_store::load_project(db.path(), "1").empty());

        ost_data::ProjectData project;
        std::string error;
        CHECK_FALSE(ost_store::load_project(db.path(), "1", project, &error));
        CHECK(project.empty());
        CHECK_STR_EQ(error, "BidConditionFolders.UID=7 participates in a ParentUID cycle.");
    }
}

// A cycle in ANOTHER bid's folders does not stop this bid from loading.
TEST(ConditionFolderLoad, AParentCycleOfAnotherBidIsNoProblem)
{
    TempDb db("folders_cycle_other_bid");
    REQUIRE(db.schema_ok());
    seed_bid(db);
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (1, 2, 'A', 2)");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (2, 2, 'B', 1)");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name, ParentUID) VALUES (3, 1, 'Fine', NULL)");

    const ost_data::ProjectData project = ost_store::load_project(db.path(), "1");
    REQUIRE(!project.empty());
    CHECK_EQ(project.condition_folders.size(), size_t(1));
}

// The rule itself, with Windows' choice of the uid that names the cycle: the smallest uid of every cycle found, decimal
// uids numerically before the others (bid_owned_identity.require_acyclic_bid_owned_parent_graph).
TEST(ConditionFolderLoad, TheCycleCheckNamesTheSmallestUidOfTheCycles)
{
    auto folders = [](std::initializer_list<std::pair<const char *, const char *>> edges)
    {
        std::unordered_map<std::string, ConditionFolder> map;
        for (const auto &edge : edges)
            map[edge.first] = ConditionFolder{edge.first, "F", edge.second};
        return map;
    };
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders({{"7", "7"}})), "7");
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders({{"7", "8"}, {"8", "7"}})), "7");
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders({{"7", "8"}, {"8", "9"}, {"9", "7"}})), "7");
    // numeric, not textual: 9 < 10
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders({{"10", "9"}, {"9", "10"}})), "9");
    // a tail leading into the cycle is not part of it
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders({{"1", "5"}, {"5", "6"}, {"6", "5"}})), "5");
    // two cycles: the smallest uid of all of them
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders({{"20", "21"}, {"21", "20"}, {"4", "4"}})), "4");
    // decimal uids come before non-decimal ones
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders({{"b", "a"}, {"a", "b"}, {"30", "30"}})), "30");
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders({{"b", "a"}, {"a", "b"}})), "a");
    // acyclic: chains, roots, parents that do not exist, "0"
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders({{"1", ""}, {"2", "1"}, {"3", "2"}, {"4", "99"}, {"5", "0"}})), "");
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders({})), "");
}

// The rule against the Windows function on 120 random parent graphs (decimal and non-decimal uids, "0" and missing parents,
// self parents): same answer, including which uid names the cycle.
TEST(ConditionFolderLoad, TheCycleCheckMatchesWindowsOnRandomGraphs)
{
    int index = 0;
    for (const ost_ctest::CycleCase &c : ost_ctest::cycle_oracle_cases())
    {
        SCOPED_CONTEXT("graph " + std::to_string(index++));
        std::unordered_map<std::string, ConditionFolder> folders;
        for (const auto &edge : c.edges)
            folders[edge.first] = ConditionFolder{edge.first, "F", edge.second};
        CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders), c.expected);
    }
}
