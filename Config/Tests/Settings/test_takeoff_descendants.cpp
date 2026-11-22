// UND-08 (+ RPV-M02): deleting a takeoff deletes EVERY descendant (through ParentUID links, at any depth, whether or not the
// selected takeoff is a root) and the undo restores the whole set with its parent links.
//
// Windows: ost_visualizer/domain/services/takeoff_domain_service.py expand_takeoff_uids_with_descendants and
// presentation/handlers/plan_view_action_handler.py on_elements_deleted (the saved set = the selected takeoffs, then every other
// descendant in list order). The database layer detaches the children of a deleted takeoff (ParentUID NULL, DBP-14); it is the
// application layer that expands the set. iOS keeps the original uid on restore (Windows allocates a new one), so the set is
// returned parent-first: a parent is always inserted before its children.
#include "SettingsTestSupport.hpp"

#include "Domain/Takeoff.hpp"
#include "Domain/TakeoffDeletion.hpp"

#include <set>
#include <string>
#include <vector>

using ost_domain::Takeoff;
using ost_stest::TempDb;

namespace
{
    // Port of tests/domain/services/test_takeoff_domain_service.py::_tk
    Takeoff tk(const std::string &uid, const std::string &parent_uid = "0")
    {
        Takeoff t;
        t.uid = uid;
        t.condition_uid = "c";
        t.page_uid = "p";
        t.parent_uid = parent_uid;
        return t;
    }

    std::vector<std::string> uids_of(const std::vector<Takeoff> &takeoffs)
    {
        std::vector<std::string> out;
        for (const Takeoff &t : takeoffs)
            out.push_back(t.uid);
        return out;
    }

    using UidSet = std::set<std::string>;
    using UidList = std::vector<std::string>;
}

// ---------------------------------------------------------------------------------------------------------------
// expand_takeoff_uids_with_descendants
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/domain/services/test_takeoff_domain_service.py::TakeoffDescendantExpansionTests::
// test_selection_expands_to_every_descendant_but_not_siblings_or_ancestors (same tree, same selections, same results)
TEST(TakeoffDescendants, SelectionExpandsToEveryDescendantButNotSiblingsOrAncestors)
{
    const std::vector<Takeoff> takeoffs = {tk("root"),        tk("a", "root"), tk("a1", "a"),
                                           tk("a1x", "a1"), tk("b", "root"), tk("other")};
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {"a"}) == (UidSet{"a", "a1", "a1x"}));
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {"a1x", "b"}) == (UidSet{"a1x", "b"}));
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {"root"}) ==
          (UidSet{"root", "a", "a1", "a1x", "b"}));
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {}).empty());
}

// Port of ...::test_unknown_selected_uids_are_kept_and_uids_are_compared_as_text (the Windows uids are ints there; iOS
// uids are text already): takeoffs 2 (parent 1) and 3 (parent 2), selection [1, "ghost"].
TEST(TakeoffDescendants, UnknownSelectedUidsAreKept)
{
    const std::vector<Takeoff> takeoffs = {tk("2", "1"), tk("3", "2")};
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {"1", "ghost"}) ==
          (UidSet{"1", "2", "3", "ghost"}));
}

// Port of ...::test_cyclic_parentage_terminates_with_each_member_once
TEST(TakeoffDescendants, CyclicParentageTerminatesWithEachMemberOnce)
{
    const std::vector<Takeoff> takeoffs = {tk("a", "b"), tk("b", "a"), tk("c", "b")};
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {"a"}) == (UidSet{"a", "b", "c"}));
}

// Port of ...::test_the_input_collections_are_not_modified
TEST(TakeoffDescendants, TheInputCollectionsAreNotModified)
{
    const UidList selected = {"a"};
    const std::vector<Takeoff> takeoffs = {tk("a"), tk("b", "a")};
    ost_domain::expand_takeoff_uids_with_descendants(takeoffs, selected);
    CHECK(selected == (UidList{"a"}));
    CHECK_EQ(takeoffs[0].parent_uid, std::string("0"));
    CHECK_EQ(takeoffs[1].parent_uid, std::string("a"));
}

// Test plan of UND-08: A (root), B (parent A), C (parent B), D (unrelated). Delete A: {A,B,C}. Delete B alone: {B,C}.
// Delete C: {C}. D is never included. (iOS used to answer {A,B} for A and {B} for B.)
TEST(TakeoffDescendants, ChainOfThreeDeletesTheWholeSubtree)
{
    const std::vector<Takeoff> takeoffs = {tk("A"), tk("B", "A"), tk("C", "B"), tk("D")};
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {"A"}) == (UidSet{"A", "B", "C"}));
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {"B"}) == (UidSet{"B", "C"}));
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {"C"}) == (UidSet{"C"}));
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {"D"}) == (UidSet{"D"}));
}

// Test plan of RPV-M02: tree root -> a -> a1 -> a1x, root -> b, plus an unrelated takeoff 'other'. Delete 'root' removes
// {root,a,a1,a1x,b} (iOS removed {root,a,b} and left a1, a1x); delete 'a' removes {a,a1,a1x} (iOS only {a}); delete
// 'a1x','b' only themselves.
TEST(TakeoffDescendants, TheCollectedTakeoffsAreTheDescendantSet)
{
    const std::vector<Takeoff> takeoffs = {tk("root"),        tk("a", "root"), tk("a1", "a"),
                                           tk("a1x", "a1"), tk("b", "root"), tk("other")};
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {"root"})) ==
          (UidList{"root", "a", "a1", "a1x", "b"}));
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {"a"})) == (UidList{"a", "a1", "a1x"}));
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {"a1x", "b"})) == (UidList{"a1x", "b"}));
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {"other"})) == (UidList{"other"}));
}

// ---------------------------------------------------------------------------------------------------------------
// collect_takeoffs_for_deletion
// ---------------------------------------------------------------------------------------------------------------

// plan_view_action_handler.on_elements_deleted: saved_takeoffs = the selected takeoffs that exist (selection order), then
// every other descendant in list order. A selected uid that is not a takeoff has nothing to save (it is not collected).
TEST(TakeoffDescendants, SelectedFirstThenDescendantsInListOrder)
{
    // list order of the descendants: a1, b1, a2 (those of a and b interleaved); selection order: b, a
    const std::vector<Takeoff> takeoffs = {tk("x"), tk("a"), tk("b"), tk("a1", "a"), tk("b1", "b"), tk("a2", "a")};
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {"b", "a"})) ==
          (UidList{"b", "a", "a1", "b1", "a2"}));
}

TEST(TakeoffDescendants, UnknownAndRepeatedSelectionsAreIgnored)
{
    const std::vector<Takeoff> takeoffs = {tk("a"), tk("a1", "a"), tk("b")};
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {"ghost"})).empty());
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {})).empty());
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {"ghost", "a", "a"})) == (UidList{"a", "a1"}));
}

// Reviewer addition. plan_view_action_handler.on_elements_deleted expands from the selected uids that ARE takeoffs
// (takeoff_uids = {t.uid for t in saved_takeoffs}), not from the raw selection: a selected uid that is no takeoff does not
// pull in the takeoffs whose parent_uid happens to name it. (Values computed by running the Windows code read-only:
// takeoffs x (parent ghost), y (parent x), z: selection [ghost] -> [], [ghost, z] -> [z]; the set function alone, which keeps
// unknown uids, gives {ghost, x, y} for [ghost].)
TEST(TakeoffDescendants, AnUnknownSelectedUidDoesNotPullInTheTakeoffsThatNameIt)
{
    const std::vector<Takeoff> takeoffs = {tk("x", "ghost"), tk("y", "x"), tk("z")};
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {"ghost"})).empty());
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {"ghost", "z"})) == (UidList{"z"}));
    CHECK(ost_domain::expand_takeoff_uids_with_descendants(takeoffs, {"ghost"}) == (UidSet{"ghost", "x", "y"}));
}

// iOS keeps the uid on restore, so a parent must come before its children even when the list holds the child first (the
// insert checks that the parent exists). For a list that is already parent-first this is exactly the Windows order.
TEST(TakeoffDescendants, ParentsAreCollectedBeforeTheirChildren)
{
    const std::vector<Takeoff> children_first = {tk("C", "B"), tk("B", "A"), tk("A"), tk("D")};
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(children_first, {"A"})) == (UidList{"A", "B", "C"}));
    // a selected child of a selected parent still follows its parent
    const std::vector<Takeoff> chain = {tk("A"), tk("B", "A"), tk("C", "B")};
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(chain, {"B", "A"})) == (UidList{"A", "B", "C"}));
    CHECK(uids_of(ost_domain::collect_takeoffs_for_deletion(chain, {"A"})) == (UidList{"A", "B", "C"}));
}

TEST(TakeoffDescendants, CyclicParentageCollectsEachTakeoffOnce)
{
    const std::vector<Takeoff> takeoffs = {tk("a", "b"), tk("b", "a"), tk("c", "b")};
    const std::vector<std::string> collected = uids_of(ost_domain::collect_takeoffs_for_deletion(takeoffs, {"a"}));
    CHECK_EQ(collected.size(), static_cast<size_t>(3));
    CHECK_EQ(collected.front(), std::string("a"));
    CHECK(UidSet(collected.begin(), collected.end()) == (UidSet{"a", "b", "c"}));
}

TEST(TakeoffDescendants, TheCollectedTakeoffsAreCompleteCopies)
{
    Takeoff child = tk("b", "a");
    child.position = {1.0, 2.0, 3.0, 4.0};
    child.rotation = 0.5;
    child.curve = 2;
    child.is_negative = true;
    child.area_uid = "60";
    const std::vector<Takeoff> takeoffs = {tk("a"), child};
    const std::vector<Takeoff> collected = ost_domain::collect_takeoffs_for_deletion(takeoffs, {"a"});
    REQUIRE_EQ(collected.size(), static_cast<size_t>(2));
    CHECK(collected[1].position == (std::vector<double>{1.0, 2.0, 3.0, 4.0}));
    CHECK_EQ(collected[1].rotation, 0.5);
    CHECK_EQ(collected[1].curve, 2);
    CHECK(collected[1].is_negative);
    CHECK_EQ(collected[1].area_uid, std::string("60"));
    CHECK_EQ(collected[1].parent_uid, std::string("a"));
}

// ---------------------------------------------------------------------------------------------------------------
// database level: delete the set, undo restores the set with its parent links
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    // bid 10: A 700 (root), B 701 (parent A), C 702 (parent B), D 703 (unrelated), all on page 100 / condition 30.
    void seed_tree(const TempDb &db)
    {
        db.seed_world();
        const char *rows[] = {
            "(700, 10, 30, 100, 0,   '1.0;2.0;3.0;4.0\n')",
            "(701, 10, 30, 100, 700, '5.0;6.0;7.0;8.0\n')",
            "(702, 10, 30, 100, 701, '9.0;10.0;11.0;12.0\n')",
            "(703, 10, 31, 100, 0,   '0.0;0.0;1.0;1.0\n')",
        };
        for (const char *row : rows)
            db.must(std::string("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position) "
                                "VALUES ") + row);
        // rows as the app writes them (the typed Rotation / Curve / IsNegativeQuantity are never NULL) plus extras
        db.must("UPDATE BidTakeoffs SET Rotation = 0.0, Curve = -1, IsNegativeQuantity = 0, No = UID - 690, "
                "Quantity = UID / 10.0, GUID = '{G-' || UID || '}', FontName = 'Arial', GridOffsetX = 1.5");
    }

    std::string whole_row(const TempDb &db, long long uid)
    {
        // every column but GUID (a restore regenerates it), values SQL-quoted so a type change shows
        return db.text("SELECT quote(UID)||'|'||quote(BidUID)||'|'||quote(BidConditionUID)||'|'||quote(BidPageUID)||'|'||"
                       "quote(BidAreaUID)||'|'||quote(ParentUID)||'|'||quote(Position)||'|'||quote(No)||'|'||quote(Quantity)||"
                       "'|'||quote(Count)||'|'||quote(Rotation)||'|'||quote(Curve)||'|'||quote(IsNegativeQuantity)||'|'||"
                       "quote(FontName)||'|'||quote(GridOffsetX) FROM BidTakeoffs WHERE UID = " + std::to_string(uid));
    }
}

// Test plan of UND-08 / RPV-M02 at the database level: the app layer expands the set, the database detaches children of a
// takeoff it deletes (ParentUID NULL), so deleting the whole set leaves only the unrelated takeoff; the restore (the
// original uids, parents first) brings the three rows back with their parent links.
TEST(TakeoffDescendants, DeletingTheSetLeavesTheRestAndTheRestoreBringsItAllBack)
{
    TempDb db("delete_restore");
    REQUIRE(db.schema_ok());
    seed_tree(db);
    const std::string before[] = {whole_row(db, 700), whole_row(db, 701), whole_row(db, 702), whole_row(db, 703)};

    const ost_data::ProjectData project = ost_store::load_project(db.path(), "10");
    REQUIRE_EQ(project.takeoffs.size(), static_cast<size_t>(4));
    const std::vector<Takeoff> doomed = ost_domain::collect_takeoffs_for_deletion(project.takeoffs, {"700"});
    REQUIRE(uids_of(doomed) == (UidList{"700", "701", "702"}));

    // the whole row is captured before anything is deleted
    std::vector<ost_domain::TakeoffExtras> extras(doomed.size());
    for (size_t i = 0; i < doomed.size(); ++i)
        REQUIRE(ost_store::read_takeoff_extras(db.path(), "10", doomed[i].uid, extras[i]));

    for (const Takeoff &t : doomed)
        REQUIRE(ost_store::delete_takeoff(db.path(), t.uid));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 703"), 1);
    CHECK_STR_EQ(whole_row(db, 703), before[3]);

    // undo: every row comes back with its own uid and parent, parents first
    for (size_t i = 0; i < doomed.size(); ++i)
    {
        Takeoff restored = doomed[i];
        restored.extras = extras[i];
        CHECK_EQ(ost_store::insert_takeoff(db.path(), "10", restored), doomed[i].uid);
    }
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 4);
    CHECK_EQ(db.count("SELECT ParentUID FROM BidTakeoffs WHERE UID = 700"), 0);
    CHECK_EQ(db.count("SELECT ParentUID FROM BidTakeoffs WHERE UID = 701"), 700);
    CHECK_EQ(db.count("SELECT ParentUID FROM BidTakeoffs WHERE UID = 702"), 701);
    for (int i = 0; i < 3; ++i)
    {
        SCOPED_CONTEXT("takeoff " + std::to_string(700 + i));
        CHECK_STR_EQ(whole_row(db, 700 + i), before[i]);
    }
}
