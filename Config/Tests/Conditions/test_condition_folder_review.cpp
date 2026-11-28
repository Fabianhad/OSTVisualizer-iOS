// Independent review additions for the condition folders (plan 5.2). Each test here pins a rule that a lazy implementation could break without
// failing any test of test_condition_folder_rules.cpp / _write.cpp / _delete.cpp / _deep.cpp (found by mutation thinking in the review of the
// first implementation). Nothing of the other test files is changed or loosened; every test passes against the implementation under review.
//
//   A  rows without an owner bid (BidUID NULL) are never the open bid's: every writer refuses them (Windows
//      bid_owned_identity.py:205-235 require_single_bid_scope_for_uids, :155-202 require_existing_bid_scoped_uid_matches: a NULL BidUID is wrong scope)
//   B  a malformed uid is refused before the database is opened by EVERY writer (the header contract, plan 4.4 item 2 step 1)
//   C  the order of the checks of delete: ownership before the in-use rule (a batch with a used folder and a foreign one is a FolderOfAnotherBid)
//   D  the lock comes before the name rules and before every "nothing to do" shortcut (unchanged rename, move to the same folder)
//   E  the iterative build_condition_tree keeps the nesting, the depth and the sibling order of the Windows tree on random forests (CF-10: the
//      traversal was rewritten from a recursion to a heap stack; the oracle tests pin fixed cases, this one pins the structure on many shapes)
#include "ConditionFoldersTestSupport.hpp"

#include "Domain/CdnType.hpp"
#include "Domain/ConditionTree.hpp"
#include "Persistence/OstDatabase.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using ost_domain::CdnType;
using ost_domain::Condition;
using ost_domain::ConditionFolder;
using ost_domain::ConditionTreeNode;
using ost_ftest::add_condition;
using ost_ftest::add_folder;
using ost_ftest::file_exists;
using ost_ftest::lock_bid;
using ost_ftest::seed_bids;
using ost_ftest::TempDb;
using R = ost_store::ConditionFolderRefusal;

namespace
{
    // bids 1 and 2; bid 1: folder 10 'Alpha', folder 11 'Ownerless' (BidUID NULL), condition 30 (no folder), condition 41 (BidUID NULL, no folder),
    // condition 31 in folder 10; bid 2: folder 20 'Other' and condition 40 in it.
    void seed_ownerless(const TempDb &db)
    {
        seed_bids(db);
        add_folder(db, 10, 1, "Alpha");
        add_folder(db, 11, 1, "Ownerless");
        add_folder(db, 20, 2, "Other");
        add_condition(db, 30, 1);
        add_condition(db, 31, 1, "10");
        add_condition(db, 41, 1);
        add_condition(db, 40, 2, "20");
        db.must("UPDATE BidConditionFolders SET BidUID = NULL WHERE UID = 11");
        db.must("UPDATE BidConditions SET BidUID = NULL WHERE UID = 41");
    }
}

// =================================================================================================================================
// A: a row whose BidUID is NULL belongs to no bid
// =================================================================================================================================

// Windows: a folder or condition with a NULL BidUID is the wrong scope (`matches[uid] is None`): the rename, the delete (alone or inside a batch with
// a good folder), a create under it, a move into it and a move of the ownerless condition are all refused, and the database is untouched.
TEST(ConditionFolderReview, ARowWithoutAnOwnerBidIsNeverTheOpenBids)
{
    TempDb db("freview_null_owner");
    REQUIRE(db.schema_ok());
    seed_ownerless(db);
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(ost_store::rename_condition_folder(db.path(), "1", "11", "Renamed"), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "1", {"11"}), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "1", {"10", "11"}), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "1", {"11", "10"}), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(ost_store::create_condition_folder(db.path(), "1", "Child", "11"), R::ParentOfAnotherBid);
    CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(db.path(), "1", "30", "11"), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(db.path(), "1", "41", "10"), R::ConditionOfAnotherBid);
    CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(db.path(), "1", "41", ""), R::ConditionOfAnotherBid);
    CHECK_EQ(db.snapshot_all(), before);

    // the good rows next to them still work: the refusals above left nothing half done
    CHECK_FOLDER_OK(ost_store::rename_condition_folder(db.path(), "1", "10", "Renamed"));
    CHECK_FOLDER_OK(ost_store::move_condition_to_folder(db.path(), "1", "30", "10"));
}

// =================================================================================================================================
// B: a malformed uid never opens the database
// =================================================================================================================================

// The header promises that the uid texts are read BEFORE the database is opened (a bad uid burns nothing and touches no file): with a database
// path that cannot be opened a malformed uid is still InvalidUid (not WriteFailed) for every writer and every uid argument, and no file appears.
// (test_condition_folder_delete.cpp pins this for delete only; the other writers had no such test.)
TEST(ConditionFolderReview, AMalformedUidIsRefusedBeforeAnyWriterOpensTheDatabase)
{
    const std::string missing = ost_test::temp_path("freview_missing_" + ost_ctest::run_token() + "/folders.db");
    for (const std::string bad : {"", "0", "-1", " 1", "1 ", "abc", "1.0", "+1", "99999999999999999999"})
    {
        SCOPED_CONTEXT("bad uid " + ost_test::detail::quote(bad));
        CHECK_FOLDER_REFUSED(ost_store::create_condition_folder(missing, bad, "N", ""), R::InvalidUid);
        CHECK_FOLDER_REFUSED(ost_store::create_condition_folder(missing, "1", "N", bad.empty() ? std::string("x") : bad), R::InvalidUid);
        CHECK_FOLDER_REFUSED(ost_store::rename_condition_folder(missing, bad, "10", "N"), R::InvalidUid);
        CHECK_FOLDER_REFUSED(ost_store::rename_condition_folder(missing, "1", bad, "N"), R::InvalidUid);
        CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(missing, bad, {"10"}), R::InvalidUid);
        CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(missing, "1", {"10", bad}), R::InvalidUid);
        CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(missing, bad, "30", "10"), R::InvalidUid);
        CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(missing, "1", bad, "10"), R::InvalidUid);
        if (!bad.empty()) // "" as the folder of a move is the root, a valid request
            CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(missing, "1", "30", bad), R::InvalidUid);
    }
    CHECK_FALSE(file_exists(missing));
}

// =================================================================================================================================
// C: delete checks the ownership of the whole batch before the in-use rule
// =================================================================================================================================

// A batch that holds a folder in use AND a folder of another bid (or no folder at all) is refused for the ownership, with no folder listed as
// blocked: the in-use rule is evaluated only for a batch that is the open bid's. (The scenario of the real-Access port refuses [top, spare] for
// the ownership but has no used folder in it.)
TEST(ConditionFolderReview, TheOwnershipOfTheBatchIsCheckedBeforeTheInUseRule)
{
    TempDb db("freview_delete_order");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 1, 1, "Used");
    add_folder(db, 2, 1, "Free");
    add_folder(db, 20, 2, "Foreign");
    add_condition(db, 30, 1, "1");
    const std::string before = db.snapshot_all();

    const auto foreign = ost_store::delete_condition_folders(db.path(), "1", {"1", "20"});
    CHECK_FOLDER_REFUSED(foreign, R::FolderOfAnotherBid);
    CHECK(foreign.blocked.empty());
    const auto foreign_first = ost_store::delete_condition_folders(db.path(), "1", {"20", "1"});
    CHECK_FOLDER_REFUSED(foreign_first, R::FolderOfAnotherBid);
    CHECK(foreign_first.blocked.empty());
    const auto missing = ost_store::delete_condition_folders(db.path(), "1", {"1", "999"});
    CHECK_FOLDER_REFUSED(missing, R::FolderNotFound);
    CHECK(missing.blocked.empty());
    CHECK_EQ(db.snapshot_all(), before);

    // and the in-use refusal itself fills `blocked` and nothing else
    const auto in_use = ost_store::delete_condition_folders(db.path(), "1", {"2", "1"});
    CHECK_FOLDER_REFUSED(in_use, R::InUse);
    CHECK_STR_EQ(ost_ftest::join(in_use.blocked), "1");
    CHECK_EQ(db.snapshot_all(), before);

    // a refusal that is not InUse and a success never list blocked folders
    CHECK(ost_store::delete_condition_folders(db.path(), "1", {"2"}).blocked.empty());
}

// =================================================================================================================================
// D: the lock comes before the name rules and before every "nothing to do" shortcut
// =================================================================================================================================

// Rename on a locked bid: a blank, a too long, a duplicate and an UNCHANGED name are all BidLocked (the lock is checked before the rename rule and
// before the "same name, nothing to write" shortcut); the ownership of the folder is still checked first.
TEST(ConditionFolderReview, ALockedBidRefusesARenameBeforeTheNameRules)
{
    TempDb db("freview_rename_locked");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 10, 1, "Alpha");
    add_folder(db, 12, 1, "Other");
    add_folder(db, 20, 2, "Foreign");
    lock_bid(db, 1);
    const std::string before = db.snapshot_all();

    auto rename = [&db](const std::string &folder, const std::string &name) { return ost_store::rename_condition_folder(db.path(), "1", folder, name); };
    CHECK_FOLDER_REFUSED(rename("10", "   "), R::BidLocked);
    CHECK_FOLDER_REFUSED(rename("10", std::string(60, 'A')), R::BidLocked);
    CHECK_FOLDER_REFUSED(rename("10", " other "), R::BidLocked);
    CHECK_FOLDER_REFUSED(rename("10", "Alpha"), R::BidLocked);   // unchanged: not a silent success on a locked bid
    CHECK_FOLDER_REFUSED(rename("10", " Alpha "), R::BidLocked);
    CHECK_FOLDER_REFUSED(rename("10", std::string("A\0B", 3)), R::BidLocked);
    CHECK_FOLDER_REFUSED(rename("20", "Renamed"), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(rename("999", "Renamed"), R::FolderNotFound);
    CHECK_EQ(db.snapshot_all(), before);
}

// Move on a locked bid: moving a condition to the folder it already sits in (and to the root when it has no folder) is a success that changes nothing
// on an open bid, but on a locked bid it is BidLocked (the lock is checked before any shortcut).
TEST(ConditionFolderReview, ALockedBidRefusesAMoveThatWouldChangeNothing)
{
    TempDb db("freview_move_locked");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 10, 1, "Alpha");
    add_condition(db, 30, 1);
    add_condition(db, 31, 1, "10");

    CHECK_FOLDER_OK(ost_store::move_condition_to_folder(db.path(), "1", "31", "10")); // already there: a success while the bid is open
    CHECK_FOLDER_OK(ost_store::move_condition_to_folder(db.path(), "1", "30", ""));
    lock_bid(db, 1);
    const std::string before = db.snapshot_all();
    CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(db.path(), "1", "31", "10"), R::BidLocked);
    CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(db.path(), "1", "30", ""), R::BidLocked);
    CHECK_EQ(db.snapshot_all(), before);
}

// =================================================================================================================================
// E: the iterative tree on random forests
// =================================================================================================================================

namespace
{
    // A small deterministic generator (no <random> distribution: the same forests on every compiler and standard library).
    struct Lcg
    {
        unsigned long long state;
        explicit Lcg(unsigned long long seed) : state(seed * 6364136223846793005ull + 1442695040888963407ull) {}
        unsigned next(unsigned bound)
        {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            return static_cast<unsigned>((state >> 33) % bound);
        }
    };

    struct Forest
    {
        std::unordered_map<std::string, Condition> conditions;
        std::unordered_map<std::string, ConditionFolder> folders;
        std::unordered_map<std::string, CdnType> types;
    };

    // Up to 40 folders (a parent among the EARLIER folders, so there is no cycle, or the root, a dangling uid, or "0"), names with ties and case
    // variants, up to 60 conditions (a folder, none, or a dangling one; a type, none, or an unknown one; reference numbers with ties).
    Forest make_forest(unsigned seed)
    {
        Lcg rng(seed);
        Forest forest;
        forest.types["T1"] = CdnType{"T1", "Type B"};
        forest.types["T2"] = CdnType{"T2", "Type A"};
        static const char *names[] = {"A", "B", "a", "b", "Walls", "walls", " A", "Zed"};
        const unsigned folder_count = 1 + rng.next(40);
        for (unsigned i = 1; i <= folder_count; ++i)
        {
            ConditionFolder folder;
            folder.uid = std::to_string(i);
            folder.name = names[rng.next(8)];
            switch (rng.next(9))
            {
            case 0: folder.parent_uid = ""; break;
            case 1: folder.parent_uid = "999"; break;
            case 2: folder.parent_uid = "0"; break;
            default: folder.parent_uid = i > 1 ? std::to_string(1 + rng.next(i - 1)) : std::string(); break;
            }
            forest.folders[folder.uid] = folder;
        }
        const unsigned condition_count = rng.next(60);
        static const char *types[] = {"", "T1", "T2", "T9", ""};
        for (unsigned k = 0; k < condition_count; ++k)
        {
            Condition condition;
            condition.uid = std::to_string(100 + k);
            condition.name = "C" + std::to_string(k);
            condition.ref_no = static_cast<int>(1 + rng.next(5));
            const unsigned folder_choice = rng.next(6);
            condition.condition_folder_uid = folder_choice == 0 ? std::string() : folder_choice == 1 ? std::string("888")
                                                                                                      : std::to_string(1 + rng.next(folder_count));
            condition.cdn_type_uid = types[rng.next(5)];
            forest.conditions[condition.uid] = condition;
        }
        return forest;
    }

    std::string kind_name(ConditionTreeNode::Kind kind)
    {
        return kind == ConditionTreeNode::Kind::Folder ? "Folder" : kind == ConditionTreeNode::Kind::TypeGroup ? "TypeGroup" : "Condition";
    }

    // The folder a folder is listed under / a condition sits in: "" = the root (empty, "0" or unknown).
    std::string folder_parent_of(const Forest &forest, const ConditionFolder &folder)
    {
        return !folder.parent_uid.empty() && forest.folders.count(folder.parent_uid) > 0 ? folder.parent_uid : std::string();
    }

    std::string condition_folder_of(const Forest &forest, const Condition &condition)
    {
        return !condition.condition_folder_uid.empty() && forest.folders.count(condition.condition_folder_uid) > 0 ? condition.condition_folder_uid
                                                                                                                  : std::string();
    }

    // Checks the structure of the pre-order node list of one forest. Returns the deepest depth seen; reports the first violation (and stops).
    int check_forest_tree(const Forest &forest, unsigned seed)
    {
        const std::vector<ConditionTreeNode> nodes = ost_domain::build_condition_tree(forest.conditions, forest.folders, forest.types);
        const std::string where = "seed " + std::to_string(seed) + ": ";
        auto fail = [&](std::size_t index, const std::string &why)
        {
            const ConditionTreeNode &n = nodes[index];
            ost_test::report_failure(__FILE__, __LINE__,
                                     where + "node " + std::to_string(index) + " (" + kind_name(n.kind) + " uid '" + n.uid + "' name '" + n.name +
                                         "' depth " + std::to_string(n.depth) + "): " + why);
        };

        std::vector<const ConditionTreeNode *> path;          // path[d] = the last node seen at depth d (the ancestors of the next node)
        std::vector<const ConditionTreeNode *> last_sibling;  // last_sibling[d] = the previous node under the same parent at depth d
        std::unordered_set<std::string> seen_folders;
        std::unordered_set<std::string> seen_conditions;
        int deepest = 0;

        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            const ConditionTreeNode &node = nodes[i];
            const std::size_t d = node.depth < 0 ? 0 : static_cast<std::size_t>(node.depth);
            if (node.depth < 0 || d > path.size())
            {
                fail(i, "the depth jumps by more than one level below the previous node");
                return deepest;
            }
            path.resize(d);
            const ConditionTreeNode *parent = d > 0 ? path[d - 1] : nullptr;
            path.push_back(&node);
            last_sibling.resize(d + 1, nullptr);
            const ConditionTreeNode *previous = last_sibling[d];
            last_sibling[d] = &node;
            deepest = std::max(deepest, node.depth);

            if (node.kind == ConditionTreeNode::Kind::Folder)
            {
                const auto folder = forest.folders.find(node.uid);
                if (folder == forest.folders.end() || !seen_folders.insert(node.uid).second)
                {
                    fail(i, "a folder that does not exist or is listed twice");
                    return deepest;
                }
                const std::string expected_parent = folder_parent_of(forest, folder->second);
                const std::string actual_parent = parent == nullptr ? std::string() : parent->kind == ConditionTreeNode::Kind::Folder ? parent->uid : "<not a folder>";
                if (actual_parent != expected_parent || node.name != folder->second.name)
                {
                    fail(i, "the folder is listed under '" + actual_parent + "' but belongs under '" + expected_parent + "'");
                    return deepest;
                }
            }
            else if (node.kind == ConditionTreeNode::Kind::TypeGroup)
            {
                if (parent != nullptr && parent->kind != ConditionTreeNode::Kind::Folder)
                {
                    fail(i, "a type group below something that is not a folder");
                    return deepest;
                }
            }
            else
            {
                const auto condition = forest.conditions.find(node.uid);
                if (condition == forest.conditions.end() || !seen_conditions.insert(node.uid).second)
                {
                    fail(i, "a condition that does not exist or is listed twice");
                    return deepest;
                }
                if (parent == nullptr || parent->kind != ConditionTreeNode::Kind::TypeGroup)
                {
                    fail(i, "a condition that is not the child of a type group");
                    return deepest;
                }
                const ConditionTreeNode *grand = d >= 2 ? path[d - 2] : nullptr;
                const std::string group_folder = grand == nullptr ? std::string() : grand->kind == ConditionTreeNode::Kind::Folder ? grand->uid : "<not a folder>";
                if (group_folder != condition_folder_of(forest, condition->second))
                {
                    fail(i, "the condition sits in folder '" + group_folder + "' but belongs in '" + condition_folder_of(forest, condition->second) + "'");
                    return deepest;
                }
                const std::string &type = condition->second.cdn_type_uid;
                const std::string title = type.empty() ? "(unassigned)" : forest.types.count(type) > 0 ? forest.types.at(type).name : "Unknown";
                if (parent->uid != type || parent->name != title || node.name != condition->second.name)
                {
                    fail(i, "the condition is below the wrong type group ('" + parent->uid + "' / '" + parent->name + "')");
                    return deepest;
                }
                if (previous != nullptr) // the conditions of a group: by reference number, equal numbers in uid (read) order
                {
                    const Condition &before = forest.conditions.at(previous->uid);
                    const bool ordered = before.ref_no < condition->second.ref_no ||
                                         (before.ref_no == condition->second.ref_no && ost_domain::uid_load_order_less(before.uid, condition->second.uid));
                    if (!ordered)
                    {
                        fail(i, "the conditions of a group are not in reference number order");
                        return deepest;
                    }
                }
                continue;
            }

            // folders and type groups under one parent: by name (byte order), a folder before a group of the same name, equal folders in uid order
            if (previous != nullptr)
            {
                bool ordered = previous->name < node.name;
                if (previous->name == node.name)
                {
                    if (previous->kind == ConditionTreeNode::Kind::Folder && node.kind == ConditionTreeNode::Kind::Folder)
                        ordered = ost_domain::uid_load_order_less(previous->uid, node.uid);
                    else
                        ordered = previous->kind == ConditionTreeNode::Kind::Folder && node.kind == ConditionTreeNode::Kind::TypeGroup;
                    // two groups with the same title (a type name shared by two type uids) keep the order of their first condition: any order is fine
                    if (previous->kind == ConditionTreeNode::Kind::TypeGroup && node.kind == ConditionTreeNode::Kind::TypeGroup)
                        ordered = true;
                }
                if (!ordered)
                {
                    fail(i, "the siblings are not in name order (previous '" + previous->name + "' " + kind_name(previous->kind) + ")");
                    return deepest;
                }
            }
        }
        if (seen_folders.size() != forest.folders.size())
            ost_test::report_failure(__FILE__, __LINE__, where + "a folder is missing from the tree");
        if (seen_conditions.size() != forest.conditions.size())
            ost_test::report_failure(__FILE__, __LINE__, where + "a condition is missing from the tree");
        return deepest;
    }
}

// Three hundred random forests: every folder and condition appears exactly once; a folder is listed under its effective parent (the root when the
// parent is empty, "0" or unknown); a type group sits under the folder of its conditions and every condition under the group of its type; the depth
// grows by at most one per node; siblings are in name order (a folder first on a tie, equal folders in uid order); a group lists its conditions by
// reference number. The traversal is the iterative one (CF-10): a wrong frame order or depth would break one of these on some forest.
TEST(ConditionFolderReview, TheIterativeTreeKeepsTheStructureOfTheWindowsTreeOnRandomForests)
{
    int deepest = 0;
    for (unsigned seed = 1; seed <= 300; ++seed)
        deepest = std::max(deepest, check_forest_tree(make_forest(seed), seed));
    CHECK(deepest >= 4); // the generator does produce nested forests (a shallow-only run would prove little)
}

// =================================================================================================================================
// Second review pass (the mutants the first pass did not think of)
//   F  every identifier a writer binds is a 64-bit integer (a 32-bit bind aliases 4294967297 with 1)
//   G  a COMMIT that fails is a failure of the call, never a success (a deferred foreign key fails only at COMMIT)
//   H  the parent of a new folder is the same folder whatever zeros its uid text carries
//   I  every writer reads its lock, its rows and its rules AFTER it holds the write lock (a second connection changes the database while the
//      call waits at BEGIN IMMEDIATE)
//   J  the strip works on code points: a name whose last byte alone would be NEL or NBSP is not cut
// =================================================================================================================================

namespace
{
    std::string hex_of_text(const std::string &text)
    {
        static const char *digits = "0123456789ABCDEF";
        std::string hex;
        for (const unsigned char c : text)
        {
            hex += digits[c >> 4];
            hex += digits[c & 15];
        }
        return hex;
    }
}

// =================================================================================================================================
// F: 64-bit identifiers
// =================================================================================================================================

// 4294967297 is 2^32 + 1: squeezed into 32 bits it reads 1, and 4294967301 reads 5, 4294967298 reads 2. A writer that binds a uid with a 32-bit call
// would rename, move or DELETE the wrong row (folder 1, condition 5, bid 2). The low rows are there to be hit by such an alias; the high rows must
// be the ones that change. (The 2147483647 tests of the other files all fit in 32 bits.)
TEST(ConditionFolderReview, EveryWriterBindsItsIdentifiersAsSixtyFourBitIntegers)
{
    TempDb db("freview_wide_uids");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 1, 1, "Small");
    add_folder(db, 4294967297LL, 1, "Big");
    add_folder(db, 4294967299LL, 1, "Big child", "4294967297");
    add_condition(db, 5, 1, "", 5);
    add_condition(db, 4294967301LL, 1, "", 77);
    add_condition(db, 9, 1, "4294967299", 9);

    // rename: the folder of the UPDATE
    CHECK_FOLDER_OK(ost_store::rename_condition_folder(db.path(), "1", "4294967297", "Bigger"));
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 1"), "Small");
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 4294967297"), "Bigger");

    // move: the folder that is stored and the condition that is updated
    CHECK_FOLDER_OK(ost_store::move_condition_to_folder(db.path(), "1", "5", "4294967297"));
    CHECK_FOLDER_OK(ost_store::move_condition_to_folder(db.path(), "1", "4294967301", "4294967299"));
    CHECK_EQ(db.list("SELECT UID || ':' || ifnull(BidConditionFolderUID, 'NULL') FROM BidConditions ORDER BY UID"),
             "5:4294967297,9:4294967299,4294967301:4294967299");

    // delete: the in-use rule over the wide uids, then the re-parenting and the DELETE
    const std::string before = db.snapshot_all();
    const auto in_use = ost_store::delete_condition_folders(db.path(), "1", {"4294967297"});
    CHECK_FOLDER_REFUSED(in_use, R::InUse);
    CHECK_STR_EQ(ost_ftest::join(in_use.blocked), "4294967297");
    CHECK_EQ(db.snapshot_all(), before);
    db.must("UPDATE BidConditions SET BidConditionFolderUID = NULL");
    CHECK_FOLDER_OK(ost_store::delete_condition_folders(db.path(), "1", {"4294967297"}));
    CHECK_EQ(db.list("SELECT UID || ':' || ifnull(ParentUID, 'NULL') || ':' || Name FROM BidConditionFolders ORDER BY UID"),
             "1:NULL:Small,4294967299:NULL:Big child");
}

// The bid of a call is a 64-bit value too: bid 4294967298 reads as bid 2 when squeezed into 32 bits. The folder stored for it carries that bid, the
// rename and the move that name it change their rows, and bid 2's own folder is not touched.
TEST(ConditionFolderReview, TheBidUidOfEveryWriterIsAlsoSixtyFourBit)
{
    TempDb db("freview_wide_bid");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 7, 2, "Walls"); // bid 2: the bid a 32-bit bind would write to
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (4294967298, 1, 'Wide bid', 3)");
    add_condition(db, 50, 4294967298LL, "", 50);

    const auto created = ost_store::create_condition_folder(db.path(), "4294967298", "Walls", ""); // 'Walls' also exists in bid 2: another bid
    CHECK_FOLDER_OK(created);
    CHECK_EQ(db.text("SELECT BidUID FROM BidConditionFolders WHERE UID = " + created.uid), "4294967298");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE BidUID = 2"), 1);

    CHECK_FOLDER_OK(ost_store::rename_condition_folder(db.path(), "4294967298", created.uid, "Renamed"));
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = " + created.uid), "Renamed");
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 7"), "Walls");

    CHECK_FOLDER_OK(ost_store::move_condition_to_folder(db.path(), "4294967298", "50", created.uid));
    CHECK_EQ(db.text("SELECT BidConditionFolderUID FROM BidConditions WHERE UID = 50"), created.uid);
    CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(db.path(), "2", "50", "7"), R::ConditionOfAnotherBid);
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "2", {created.uid}), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "4294967298", {created.uid}), R::InUse);
    CHECK_FOLDER_OK(ost_store::move_condition_to_folder(db.path(), "4294967298", "50", ""));
    CHECK_FOLDER_OK(ost_store::delete_condition_folders(db.path(), "4294967298", {created.uid}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 1);
}

// =================================================================================================================================
// G: a failed COMMIT is a failed call
// =================================================================================================================================

// A deferred foreign key is checked at COMMIT: the statement of the writer succeeds and the COMMIT fails (the newer tests of the other M1 writers use
// the same device). The writer must report the failure (WriteFailed), and the database must be as it was: a `commit()` whose result is ignored would
// answer "created / renamed / deleted / moved" for a write that never happened. Each writer has a fixture whose own statement trips the deferred key;
// with the key satisfied the same call succeeds (the control).
TEST(ConditionFolderReview, ACreateWhoseCommitFailsIsAFailureAndWritesNothing)
{
    TempDb db("freview_commit_create", false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE Names (Name TEXT PRIMARY KEY)");
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER PRIMARY KEY, BidUID INTEGER, ParentUID INTEGER, "
            "Name TEXT REFERENCES Names(Name) DEFERRABLE INITIALLY DEFERRED, ExpandState INTEGER)");
    const std::string before = db.snapshot_all();

    const auto failed = ost_store::create_condition_folder(db.path(), "1", "Walls", "");
    CHECK_FOLDER_REFUSED(failed, R::WriteFailed);
    CHECK_STR_EQ(failed.uid, "");
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 0);

    db.must("INSERT INTO Names (Name) VALUES ('Walls')");
    const auto control = ost_store::create_condition_folder(db.path(), "1", "Walls", "");
    CHECK_FOLDER_OK(control);
    CHECK_STR_EQ(control.uid, "1"); // the failed call burnt nothing
}

TEST(ConditionFolderReview, ARenameWhoseCommitFailsIsAFailureAndChangesNothing)
{
    TempDb db("freview_commit_rename", false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE Names (Name TEXT PRIMARY KEY)");
    db.must("INSERT INTO Names (Name) VALUES ('Old')");
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER PRIMARY KEY, BidUID INTEGER, ParentUID INTEGER, "
            "Name TEXT REFERENCES Names(Name) DEFERRABLE INITIALLY DEFERRED, ExpandState INTEGER)");
    db.must("INSERT INTO BidConditionFolders VALUES (5, 1, NULL, 'Old', -1)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(ost_store::rename_condition_folder(db.path(), "1", "5", "New"), R::WriteFailed);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 5"), "Old");

    db.must("INSERT INTO Names (Name) VALUES ('New')");
    CHECK_FOLDER_OK(ost_store::rename_condition_folder(db.path(), "1", "5", "New"));
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 5"), "New");
}

TEST(ConditionFolderReview, ADeleteWhoseCommitFailsIsAFailureAndDeletesNothing)
{
    TempDb db("freview_commit_delete", false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER PRIMARY KEY, BidUID INTEGER, ParentUID INTEGER, Name TEXT, ExpandState INTEGER)");
    db.must("CREATE TABLE Pins (Folder INTEGER REFERENCES BidConditionFolders(UID) DEFERRABLE INITIALLY DEFERRED)");
    db.must("INSERT INTO BidConditionFolders VALUES (5, 1, NULL, 'Parent', -1)");
    db.must("INSERT INTO BidConditionFolders VALUES (6, 1, 5, 'Child', -1)");
    db.must("INSERT INTO Pins (Folder) VALUES (5)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "1", {"5"}), R::WriteFailed);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.text("SELECT ParentUID FROM BidConditionFolders WHERE UID = 6"), "5"); // the re-parenting was rolled back with it

    db.must("DELETE FROM Pins");
    CHECK_FOLDER_OK(ost_store::delete_condition_folders(db.path(), "1", {"5"}));
    CHECK_EQ(db.list("SELECT UID || ':' || ifnull(ParentUID, 'NULL') FROM BidConditionFolders"), "6:NULL");
}

TEST(ConditionFolderReview, AMoveWhoseCommitFailsIsAFailureAndMovesNothing)
{
    TempDb db("freview_commit_move", false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE Allowed (Id INTEGER PRIMARY KEY)");
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER PRIMARY KEY, BidUID INTEGER, ParentUID INTEGER, Name TEXT, ExpandState INTEGER)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, "
            "BidConditionFolderUID INTEGER REFERENCES Allowed(Id) DEFERRABLE INITIALLY DEFERRED)");
    db.must("INSERT INTO BidConditionFolders VALUES (10, 1, NULL, 'Folder', -1)");
    db.must("INSERT INTO BidConditions VALUES (30, 1, NULL)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(db.path(), "1", "30", "10"), R::WriteFailed);
    CHECK_EQ(db.snapshot_all(), before);

    db.must("INSERT INTO Allowed (Id) VALUES (10)");
    CHECK_FOLDER_OK(ost_store::move_condition_to_folder(db.path(), "1", "30", "10"));
    CHECK_EQ(db.text("SELECT BidConditionFolderUID FROM BidConditions WHERE UID = 30"), "10");
}

// =================================================================================================================================
// H: the zeros of a uid text do not change which folder it is
// =================================================================================================================================

// Windows reads "010" as 10. The siblings of a new folder are the folders below THAT parent: a name that exists below folder 10 is a duplicate whether
// the parent was written "10", "010" or "0000010"; the same name is fine at the root and below another folder. (LeadingZerosInUidsMeanTheSameRows
// only creates a name nobody has, so it cannot see which parent the sibling check was run against.)
TEST(ConditionFolderReview, TheSiblingsOfANewFolderAreThoseOfTheParentWhateverZerosItsUidCarries)
{
    TempDb db("freview_padded_parent");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 10, 1, "Alpha");
    add_folder(db, 11, 1, "Child", "10");
    add_folder(db, 12, 1, "Other");
    const std::string before = db.snapshot_all();

    for (const char *parent : {"10", "010", "0000010"})
    {
        SCOPED_CONTEXT(std::string("parent '") + parent + "'");
        const auto duplicate = ost_store::create_condition_folder(db.path(), "001", " child ", parent);
        CHECK_FOLDER_REFUSED(duplicate, R::DuplicateName);
        CHECK_STR_EQ(duplicate.message, "Folder child already exists.");
        CHECK_EQ(db.snapshot_all(), before);
    }
    const auto at_root = ost_store::create_condition_folder(db.path(), "1", "child", "");
    CHECK_FOLDER_OK(at_root); // 'Child' lives below 10, not at the root
    const auto below_other = ost_store::create_condition_folder(db.path(), "1", "child", "012");
    CHECK_FOLDER_OK(below_other);
    CHECK_EQ(db.text("SELECT ParentUID FROM BidConditionFolders WHERE UID = " + below_other.uid), "12");
}

// =================================================================================================================================
// I: every writer reads its state after it holds the write lock
// =================================================================================================================================

namespace
{
    // bids 1 and 2; bid 1: folders 10 'Alpha', 12 'Other', 13 'Spare' (all root), condition 30 (no folder).
    void seed_waiting_world(const TempDb &db)
    {
        seed_bids(db);
        add_folder(db, 10, 1, "Alpha");
        add_folder(db, 12, 1, "Other");
        add_folder(db, 13, 1, "Spare");
        add_condition(db, 30, 1);
    }

    // Runs `call` on a second thread while an open connection of this thread (BEGIN IMMEDIATE, then `changes`) holds the write lock for about
    // 300 ms and commits. The call waits at its own BEGIN IMMEDIATE (busy timeout 5 s) and must read the lock, the rows and the names AFTER it
    // has the lock, i.e. from the state the other connection committed. (A call that starts late sees the same final state: the test cannot
    // flake, it can only be less sharp.) The check macros run on this thread, after the join.
    ost_store::ConditionFolderResult run_while_the_database_changes(const TempDb &db, const std::vector<std::string> &changes,
                                                                    const std::function<ost_store::ConditionFolderResult()> &call)
    {
        ost_db::OstDatabase other;
        REQUIRE(other.open(db.path()));
        REQUIRE(other.begin_transaction());
        ost_store::ConditionFolderResult result;
        std::thread caller([&]() { result = call(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        bool changed = true;
        for (const std::string &sql : changes)
            changed = other.exec(sql) && changed;
        const bool committed = other.commit();
        caller.join();
        CHECK(changed);
        CHECK(committed);
        return result;
    }

    const std::vector<std::string> kLockBid1 = {"INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (5, 'Locked', -1, 5)",
                                                "UPDATE Bids SET JobStatusUID = 5 WHERE UID = 1"};
}

// The bid becomes locked while the call waits for the write lock: every writer refuses with BidLocked (a lock read before the transaction began would
// have seen an open bid) and writes nothing. The rename asks for the name the folder has: still BidLocked, not a silent success.
TEST(ConditionFolderReview, ALockThatAppearsWhileTheCallWaitsForTheWriteLockStopsEveryWriter)
{
    struct Writer
    {
        const char *label;
        std::function<ost_store::ConditionFolderResult(const TempDb &)> call;
    };
    const std::vector<Writer> writers = {
        {"create", [](const TempDb &db) { return ost_store::create_condition_folder(db.path(), "1", "Walls", ""); }},
        {"create under a parent", [](const TempDb &db) { return ost_store::create_condition_folder(db.path(), "1", "Walls", "10"); }},
        {"rename", [](const TempDb &db) { return ost_store::rename_condition_folder(db.path(), "1", "10", "Renamed"); }},
        {"rename to the same name", [](const TempDb &db) { return ost_store::rename_condition_folder(db.path(), "1", "10", "Alpha"); }},
        {"delete", [](const TempDb &db) { return ost_store::delete_condition_folders(db.path(), "1", {"13"}); }},
        {"move", [](const TempDb &db) { return ost_store::move_condition_to_folder(db.path(), "1", "30", "12"); }},
    };
    for (const Writer &writer : writers)
    {
        SCOPED_CONTEXT(writer.label);
        TempDb db("freview_wait_lock");
        REQUIRE(db.schema_ok());
        seed_waiting_world(db);
        const std::string folders_before = db.dump("BidConditionFolders");
        const std::string conditions_before = db.dump("BidConditions");

        const auto result = run_while_the_database_changes(db, kLockBid1, [&]() { return writer.call(db); });
        CHECK_FOLDER_REFUSED(result, R::BidLocked);
        CHECK_EQ(db.dump("BidConditionFolders"), folders_before);
        CHECK_EQ(db.dump("BidConditions"), conditions_before);
    }
}

// The names, the rows and the usage are read after the lock is held too: another connection takes the name, deletes the target folder or fills the
// folder while the call waits, and the call decides on what it finds then (DuplicateName, FolderNotFound, InUse), writing nothing.
TEST(ConditionFolderReview, TheRowsAndTheRulesAreReadAfterTheWriteLockIsHeld)
{
    {
        SCOPED_CONTEXT("rename: a sibling takes the name while the call waits");
        TempDb db("freview_wait_rename");
        REQUIRE(db.schema_ok());
        seed_waiting_world(db);
        const auto result = run_while_the_database_changes(db, {"UPDATE BidConditionFolders SET Name = 'Taken' WHERE UID = 12"},
                                                           [&]() { return ost_store::rename_condition_folder(db.path(), "1", "10", "taken"); });
        CHECK_FOLDER_REFUSED(result, R::DuplicateName);
        CHECK_STR_EQ(result.message, "Folder taken already exists.");
        CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 10"), "Alpha");
    }
    {
        SCOPED_CONTEXT("create: a sibling takes the name while the call waits");
        TempDb db("freview_wait_create");
        REQUIRE(db.schema_ok());
        seed_waiting_world(db);
        const auto result = run_while_the_database_changes(db, {"UPDATE BidConditionFolders SET Name = 'Walls' WHERE UID = 12"},
                                                           [&]() { return ost_store::create_condition_folder(db.path(), "1", "walls", ""); });
        CHECK_FOLDER_REFUSED(result, R::DuplicateName);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 3);
    }
    {
        SCOPED_CONTEXT("delete: a condition is moved into the folder while the call waits");
        TempDb db("freview_wait_delete_use");
        REQUIRE(db.schema_ok());
        seed_waiting_world(db);
        const auto result = run_while_the_database_changes(db, {"UPDATE BidConditions SET BidConditionFolderUID = 13 WHERE UID = 30"},
                                                           [&]() { return ost_store::delete_condition_folders(db.path(), "1", {"13"}); });
        CHECK_FOLDER_REFUSED(result, R::InUse);
        CHECK_STR_EQ(ost_ftest::join(result.blocked), "13");
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID = 13"), 1);
    }
    {
        SCOPED_CONTEXT("delete: the folder is deleted by someone else while the call waits");
        TempDb db("freview_wait_delete_gone");
        REQUIRE(db.schema_ok());
        seed_waiting_world(db);
        const auto result = run_while_the_database_changes(db, {"DELETE FROM BidConditionFolders WHERE UID = 13"},
                                                           [&]() { return ost_store::delete_condition_folders(db.path(), "1", {"12", "13"}); });
        CHECK_FOLDER_REFUSED(result, R::FolderNotFound);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID = 12"), 1); // the rest of the batch was not deleted
    }
    {
        SCOPED_CONTEXT("move: the target folder is deleted while the call waits");
        TempDb db("freview_wait_move");
        REQUIRE(db.schema_ok());
        seed_waiting_world(db);
        const auto result = run_while_the_database_changes(db, {"DELETE FROM BidConditionFolders WHERE UID = 13"},
                                                           [&]() { return ost_store::move_condition_to_folder(db.path(), "1", "30", "13"); });
        CHECK_FOLDER_REFUSED(result, R::FolderNotFound);
        CHECK_EQ(db.text("SELECT typeof(BidConditionFolderUID) FROM BidConditions WHERE UID = 30"), "null");
    }
    {
        SCOPED_CONTEXT("control: a change that matters to nobody lets the call through");
        TempDb db("freview_wait_control");
        REQUIRE(db.schema_ok());
        seed_waiting_world(db);
        const auto result = run_while_the_database_changes(db, {"UPDATE Bids SET JobName = 'Changed' WHERE UID = 2"},
                                                           [&]() { return ost_store::create_condition_folder(db.path(), "1", "Walls", ""); });
        CHECK_FOLDER_OK(result);
        CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = " + result.uid), "Walls");
    }
}

// =================================================================================================================================
// J: the strip works on code points
// =================================================================================================================================

// U+0105 is C4 85, U+00E0 is C3 A0 and U+3005 is E3 80 85: their last byte, taken alone, is the NEL or the NBSP that Python's strip() removes. strip()
// looks at whole characters, so none of these names is cut or blank; the real spaces around them go (NBSP, ideographic space). Through the rule and
// through the store, byte for byte.
TEST(ConditionFolderReview, ANameThatEndsInTheByteOfASpaceCharacterIsNotCutShort)
{
    const std::vector<std::string> names = {"Pi\xC4\x85", "Caf\xC3\xA0", "\xC4\x85", "\xC3\xA0", "\xE3\x80\x85", "A\xC3\xA0" "B", "\xC3\xA0\xC4\x85"};
    const std::unordered_map<std::string, ConditionFolder> none;
    TempDb db("freview_strip_bytes");
    REQUIRE(db.schema_ok());
    seed_bids(db);

    for (const std::string &name : names)
    {
        SCOPED_CONTEXT("name " + hex_of_text(name));
        const ost_domain::FolderNameCheck plain = ost_domain::check_condition_folder_name(none, "", "", name);
        CHECK(plain.issue == ost_domain::FolderNameIssue::None);
        CHECK_STR_EQ(hex_of_text(plain.name), hex_of_text(name));
        const ost_domain::FolderNameCheck padded = ost_domain::check_condition_folder_name(none, "", "", "\xC2\xA0 " + name + " \xE3\x80\x80");
        CHECK(padded.issue == ost_domain::FolderNameIssue::None);
        CHECK_STR_EQ(hex_of_text(padded.name), hex_of_text(name));

        const auto created = ost_store::create_condition_folder(db.path(), "1", "\xC2\xA0" + name + "\xC2\x85", "");
        CHECK_FOLDER_OK(created);
        CHECK_STR_EQ(db.text("SELECT hex(Name) FROM BidConditionFolders WHERE UID = " + created.uid), hex_of_text(name));
        // typing the stored name again (padded or not) is the same name: a success that writes nothing
        const std::string trigger = db.block("BidConditionFolders", "UPDATE");
        CHECK_FOLDER_OK(ost_store::rename_condition_folder(db.path(), "1", created.uid, name));
        CHECK_FOLDER_OK(ost_store::rename_condition_folder(db.path(), "1", created.uid, " " + name + "\xC2\xA0"));
        db.unblock(trigger);
        CHECK_STR_EQ(db.text("SELECT hex(Name) FROM BidConditionFolders WHERE UID = " + created.uid), hex_of_text(name));
    }
}
