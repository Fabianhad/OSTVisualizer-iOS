// CF-10 (plan 5.2, "Windows oddities": an 8000-deep chain of folders is read by Windows; the iOS tree build was recursive and a main-thread stack is
// 1 MB): the chain of 8000 nested folders loads, passes the cycle check in linear time, shows in the Conditions tree without a stack overflow, and
// the store writers cope with its depth. The tree test is the one that overflows the stack of a recursive build_condition_tree.
//
// Windows sources (HEAD 49a2d59):
//   tests/infrastructure/database/test_bid_owned_identity.py:493-499 test_parent_graph_walk_is_linear_for_long_acyclic_chains (8000 nodes, under 1.5 s,
//       then chain[1] = 8000 closes the cycle and names UID=1)
//   ost_visualizer/presentation/components/conditions_sidebar.py:629-635 (the recursive Python tree build: Windows' own limit is the recursion limit,
//       inferred, not run)
#include "ConditionFoldersTestSupport.hpp"

#include "Domain/ConditionFolder.hpp"
#include "Domain/ConditionTree.hpp"

#include <chrono>

#if defined(_MSC_VER)
#include <excpt.h>
#include <malloc.h>
#endif

using ost_domain::Condition;
using ost_domain::ConditionFolder;
using ost_domain::ConditionTreeNode;
using ost_ftest::add_condition;
using ost_ftest::seed_bids;
using ost_ftest::TempDb;
using ost_ftest::uid_range;
using R = ost_store::ConditionFolderRefusal;

namespace
{
    // folders 1 .. depth, folder n a child of n - 1 (folder 1 has parent "0", like the Windows chain `{uid: uid - 1}`).
    std::unordered_map<std::string, ConditionFolder> chain_of(int depth)
    {
        std::unordered_map<std::string, ConditionFolder> folders;
        for (int uid = 1; uid <= depth; ++uid)
            folders[std::to_string(uid)] = ConditionFolder{std::to_string(uid), "F" + std::to_string(uid), std::to_string(uid - 1)};
        return folders;
    }

    // One call of build_condition_tree, shaped so that a stack overflow can be caught on MSVC (see build_without_overflow).
    struct TreeRun
    {
        const std::unordered_map<std::string, Condition> *conditions = nullptr;
        const std::unordered_map<std::string, ConditionFolder> *folders = nullptr;
        std::vector<ConditionTreeNode> nodes;
    };

    void run_tree(void *raw)
    {
        TreeRun &run = *static_cast<TreeRun *>(raw);
        run.nodes = ost_domain::build_condition_tree(*run.conditions, *run.folders, {});
    }

#if defined(_MSC_VER)
    // A recursive traversal of 8000 levels overflows the 1 MB stack of an MSVC test process: the process would die (exit code 0xC00000FD) and
    // take every later test of the area with it, instead of failing this one. The structured exception is caught here (a function with no C++
    // objects, so that __try is allowed) and the stack guard is re-armed: the test then fails like any other. Other compilers run the call plainly.
    bool run_guarded(void (*call)(void *), void *argument)
    {
        __try
        {
            call(argument);
            return true;
        }
        __except (GetExceptionCode() == 0xC00000FDul ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            _resetstkoflw();
            return false;
        }
    }
#endif

    // true when the tree was built; false when building it overflowed the stack (MSVC only: elsewhere an overflow ends the process).
    bool build_without_overflow(TreeRun &run)
    {
#if defined(_MSC_VER)
        return run_guarded(&run_tree, &run);
#else
        run_tree(&run);
        return true;
#endif
    }

    // The chain as database rows (folder n is a child of n - 1; folder 1 has ParentUID 0 like the Windows chain).
    void seed_chain_rows(const TempDb &db, int depth)
    {
        db.must("WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i + 1 FROM n WHERE i < " + std::to_string(depth) +
                ") INSERT INTO BidConditionFolders (UID, BidUID, ParentUID, Name, ExpandState) SELECT i, 1, i - 1, 'F' || i, -1 FROM n");
    }
}

// Port of tests/infrastructure/database/test_bid_owned_identity.py:493-499 test_parent_graph_walk_is_linear_for_long_acyclic_chains: a chain of 8000
// folders has no cycle (the check is linear: the same walk without the explored set would be ~32M steps); closing it (folder 1 under 8000) is a cycle
// that names UID 1. The Windows bound is 1.5 s.
TEST(ConditionFolderDeep, TheParentGraphWalkIsLinearForALongAcyclicChain)
{
    auto folders = chain_of(8000);
    const auto started = std::chrono::steady_clock::now();
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders), "");
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    CHECK(elapsed_ms < 1500);

    folders["1"].parent_uid = "8000";
    CHECK_STR_EQ(ost_domain::find_condition_folder_cycle(folders), "1");
}

// The same chain through the loader: 8000 nested folders of one bid load (folder 1 has ParentUID 0 = root), and closing the cycle makes the bid unreadable
// with the Windows message (the existing loader tests pin the small cases; this is the depth).
TEST(ConditionFolderDeep, AChainOfEightThousandFoldersLoadsAndAClosedChainIsRefused)
{
    TempDb db("fdeep_load");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    seed_chain_rows(db, 8000);

    const ost_data::ProjectData project = ost_store::load_project(db.path(), "1");
    REQUIRE(!project.empty());
    CHECK_EQ(project.condition_folders.size(), size_t(8000));
    CHECK_STR_EQ(project.condition_folders.at("1").parent_uid, "");
    CHECK_STR_EQ(project.condition_folders.at("8000").parent_uid, "7999");
    CHECK_STR_EQ(project.condition_folders.at("4000").name, "F4000");

    db.must("UPDATE BidConditionFolders SET ParentUID = 8000 WHERE UID = 1");
    ost_data::ProjectData broken;
    std::string error;
    CHECK_FALSE(ost_store::load_project(db.path(), "1", broken, &error));
    CHECK_STR_EQ(error, "BidConditionFolders.UID=1 participates in a ParentUID cycle.");
}

// Plan 5.2 (data safety): a chain of 8000 nested folders SHOWS: build_condition_tree returns the 8000 folder nodes in nesting order (depth 0 to 7999) and
// the type group and the condition of the deepest folder below them (depth 8000 and 8001), without a stack overflow. A recursive emit_children needs
// one stack frame per level: the stack of a main thread (1 MB on iOS and in an MSVC test) does not hold 8000 of them. The traversal is iterative.
TEST(ConditionFolderDeep, AChainOfEightThousandFoldersShowsInTheTree)
{
    const auto folders = chain_of(8000);
    std::unordered_map<std::string, Condition> conditions;
    Condition condition;
    condition.uid = "10";
    condition.name = "Deep condition";
    condition.ref_no = 1;
    condition.condition_folder_uid = "8000";
    conditions["10"] = condition;

    TreeRun run;
    run.conditions = &conditions;
    run.folders = &folders;
    if (!build_without_overflow(run))
        FAIL_TEST("build_condition_tree overflowed the stack on a chain of 8000 folders: emit_children recurses once per level (it must be iterative)");
    const std::vector<ConditionTreeNode> &nodes = run.nodes;
    REQUIRE_EQ(nodes.size(), size_t(8002));
    bool in_order = true;
    for (int i = 0; i < 8000; ++i)
    {
        const ConditionTreeNode &node = nodes[static_cast<std::size_t>(i)];
        if (node.kind != ConditionTreeNode::Kind::Folder || node.depth != i || node.uid != std::to_string(i + 1) || node.name != "F" + std::to_string(i + 1))
        {
            in_order = false;
            ost_test::report_failure(__FILE__, __LINE__, "folder node " + std::to_string(i) + " is wrong: depth " + std::to_string(node.depth) + " uid " + node.uid);
            break;
        }
    }
    CHECK(in_order);
    CHECK(nodes[8000].kind == ConditionTreeNode::Kind::TypeGroup);
    CHECK_EQ(nodes[8000].depth, 8000);
    CHECK_STR_EQ(nodes[8000].name, "(unassigned)");
    CHECK(nodes[8001].kind == ConditionTreeNode::Kind::Condition);
    CHECK_EQ(nodes[8001].depth, 8001);
    CHECK_STR_EQ(nodes[8001].uid, "10");
}

// The store functions cope with the depth (their in-use walk and name checks read the 8000 rows and walk the chain iteratively): a folder is created
// under the deepest one, the deepest is renamed, the whole chain is refused while the deepest holds a condition (blocked: 1 .. 8000 in request order,
// the new child below the condition's folder is not blocked), and after the condition moves out the whole chain is deleted.
TEST(ConditionFolderDeep, TheStoreWritersWorkOnAChainOfEightThousandFolders)
{
    TempDb db("fdeep_store");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    seed_chain_rows(db, 8000);
    add_condition(db, 30, 1, "8000");

    const auto child = ost_store::create_condition_folder(db.path(), "1", "Deep child", "8000");
    CHECK_FOLDER_OK(child);
    CHECK_STR_EQ(child.uid, "8001");
    CHECK_EQ(db.text("SELECT ParentUID FROM BidConditionFolders WHERE UID = 8001"), "8000");
    CHECK_FOLDER_OK(ost_store::rename_condition_folder(db.path(), "1", "8000", "Renamed deep"));
    CHECK_FOLDER_REFUSED(ost_store::create_condition_folder(db.path(), "1", "RENAMED DEEP", "7999"), R::DuplicateName);

    const std::string before = db.snapshot_all();
    const auto refused = ost_store::delete_condition_folders(db.path(), "1", uid_range(1, 8001));
    CHECK_FOLDER_REFUSED(refused, R::InUse);
    CHECK_STR_EQ(ost_ftest::join(refused.blocked), ost_ftest::join(uid_range(1, 8000)));
    CHECK_EQ(db.snapshot_all(), before);

    CHECK_FOLDER_OK(ost_store::move_condition_to_folder(db.path(), "1", "30", ""));
    CHECK_FOLDER_OK(ost_store::delete_condition_folders(db.path(), "1", uid_range(1, 8001)));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 0);
}
