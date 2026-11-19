// RPV-10, RPV-15, GEO-M02 (aliases SNP-05, GEO-M01, UND-M01): group (multi-selection) drag and the children of an area that is
// dragged by its body, extracted from ViewController.mm (planView:didDragTakeoff: isMulti branch, didEndDragTakeoff:) into
// Core/Geometry/drag_rules.hpp.
//
// Windows oracle: input_handler.py (_snapped_group_translation_delta, _expanded_takeoff_transform_uids, _area_child_parent_map,
// _compute_group_translation_positions, the release of an area body drag) and tests/presentation/components/plan_view/components/
// test_input_handler.py (test_group_translation_snaps_each_axis_independently, test_area_child_parent_map_*,
// test_group_translation_moves_nested_children_by_the_parent_displacement, test_group_translation_is_rejected_as_a_whole_when_*,
// test_area_parent_drag_translates_descendant_holes_with_before_edit,
// test_area_release_keeps_the_last_valid_polygon_and_moves_child_holes, test_child_holes_only_follow_a_body_drag,
// test_child_takeoffs_do_not_follow_the_body_drag_of_a_non_area_parent). Numbers marked "Windows run" come from running the
// Windows functions read-only (python -B, offscreen Qt) with the same fixtures; the validity answers are those of the real
// _takeoff_children_valid_for_geometry_changes (the Windows tests mock it, here the real hierarchy validation of Core runs).
#include "TestHarness.hpp"

#include "HierarchyTestSupport.hpp"

#include "Geometry/drag_rules.hpp"

#include <set>
#include <string>
#include <vector>

namespace
{
    using geo_test::Family;
    using geo_test::make_condition;
    using geo_test::make_takeoff;
    using ost_drag::PositionMap;
    using ost_domain::ConditionType;
    using ost_domain::ShapeId;

    void check_position(const std::vector<double> &actual, const std::vector<double> &expected, double tol = 1e-9)
    {
        REQUIRE_EQ(actual.size(), expected.size());
        for (size_t i = 0; i < actual.size(); ++i)
            CHECK_NEAR(actual[i], expected[i], tol);
    }

    Family area_family()
    {
        Family f;
        f.add_condition(make_condition("area", ConditionType::Area));
        f.add_condition(make_condition("line", ConditionType::Linear));
        f.add_condition(make_condition("cat", ConditionType::Attachment, ShapeId::Rectangle, 10.0, 4.0));
        return f;
    }
}

// Port of test_group_translation_snaps_each_axis_independently (inc 5: (7.6, 12.4) -> (10, 10); (-7.6, 2.4) -> (-10, 0)) plus the
// RPV-10 plan (inc 1: (2.4, -0.6) -> (2, -1); inc 0: the raw delta; inc 10: (7.6, 12.4) -> (10, 10), (-7.6, 2.4) -> (-10, 0)) and
// GEO-M01 (inc 1, delta (0.3, 0.3): every point gets snap(0.3) = 0).
TEST(GroupDelta, SnapsEachAxisToTheIncrement)
{
    using ost_drag::snapped_group_delta;
    CHECK_EQ(snapped_group_delta(7.6, 12.4, 5.0), (std::pair<double, double>{10.0, 10.0}));
    CHECK_EQ(snapped_group_delta(-7.6, 2.4, 5.0), (std::pair<double, double>{-10.0, 0.0}));
    CHECK_EQ(snapped_group_delta(2.4, -0.6, 1.0), (std::pair<double, double>{2.0, -1.0}));
    CHECK_EQ(snapped_group_delta(2.4, -0.6, 0.0), (std::pair<double, double>{2.4, -0.6}));
    CHECK_EQ(snapped_group_delta(2.4, -0.6, -1.0), (std::pair<double, double>{2.4, -0.6}));
    CHECK_EQ(snapped_group_delta(7.6, 12.4, 10.0), (std::pair<double, double>{10.0, 10.0}));
    CHECK_EQ(snapped_group_delta(-7.6, 2.4, 10.0), (std::pair<double, double>{-10.0, 0.0}));
    CHECK_EQ(snapped_group_delta(0.3, 0.3, 1.0), (std::pair<double, double>{0.0, 0.0}));
    CHECK_EQ(snapped_group_delta(0.5, 1.5, 1.0), (std::pair<double, double>{0.0, 2.0})); // ties to even
}

// RPV-10 plan: inc 1.0, group delta (2.4, -0.6) -> every vertex of every selected takeoff moves by (2.0, -1.0); positions on the
// grid stay on the grid; inc 0 -> the raw delta (iOS applied (2.4, -0.6) unsnapped in every case).
TEST(GroupTranslation, EveryVertexMovesByTheSnappedGroupDelta)
{
    Family f = area_family();
    f.add(make_takeoff("sq", "area", {0.0, 0.0, 4.0, 0.0, 4.0, 4.0, 0.0, 4.0}));
    f.add(make_takeoff("ln", "line", {10.0, 10.0, 20.0, 12.0}));
    const auto model = f.model();
    const PositionMap orig = {{"sq", f.takeoffs[0].position}, {"ln", f.takeoffs[1].position}};

    const PositionMap snapped = ost_drag::compute_group_translation_positions(model, orig, 2.4, -0.6, 1.0);
    check_position(snapped.at("sq"), {2.0, -1.0, 6.0, -1.0, 6.0, 3.0, 2.0, 3.0});
    check_position(snapped.at("ln"), {12.0, 9.0, 22.0, 11.0});

    const PositionMap raw = ost_drag::compute_group_translation_positions(model, orig, 2.4, -0.6, 0.0);
    check_position(raw.at("sq"), {2.4, -0.6, 6.4, -0.6, 6.4, 3.4, 2.4, 3.4});
    check_position(raw.at("ln"), {12.4, 9.4, 22.4, 11.4});

    // GEO-M01: delta (0.3, 0.3) with inc 1.0 moves nothing (snap(0.3) = 0), no per-coordinate snapping of the vertices
    const PositionMap none = ost_drag::compute_group_translation_positions(model, orig, 0.3, 0.3, 1.0);
    check_position(none.at("sq"), f.takeoffs[0].position, 0.0);
    check_position(none.at("ln"), f.takeoffs[1].position, 0.0);
}

// SNP-05 plan: with inc 10, a group delta (7.6, 12.4) gives (10, 10) and (-7.6, 2.4) gives (-10, 0); every takeoff's new position equals
// its original plus the common snapped delta; trailing values stay.
TEST(GroupTranslation, EveryTakeoffGetsTheCommonDeltaAndTrailingValuesStay)
{
    Family f = area_family();
    f.add(make_takeoff("a", "line", {1.0, 2.0, 3.0, 4.0, 9.0}));
    f.add(make_takeoff("b", "line", {-5.5, 7.25}));
    const auto model = f.model();
    const PositionMap orig = {{"a", f.takeoffs[0].position}, {"b", f.takeoffs[1].position}};
    const PositionMap r = ost_drag::compute_group_translation_positions(model, orig, -7.6, 2.4, 10.0);
    check_position(r.at("a"), {-9.0, 2.0, -7.0, 4.0, 9.0});
    check_position(r.at("b"), {-15.5, 7.25});
}

// Port of test_area_child_parent_map_attributes_each_child_to_its_selected_ancestor (the Windows test patches
// _expanded_takeoff_transform_uids to {mid, leaf}; here the candidates are passed explicitly, the expansion of Core runs).
TEST(GroupAreaChildMap, AttributesEachChildToItsSelectedAncestor)
{
    Family f = area_family();
    f.add(make_takeoff("top", "area", std::vector<double>(6, 0.0)));
    f.add(make_takeoff("mid", "area", std::vector<double>(6, 0.0), "top"));
    f.add(make_takeoff("leaf", "area", std::vector<double>(6, 0.0), "mid"));
    f.add(make_takeoff("line", "line", std::vector<double>(4, 0.0), "top"));
    const auto model = f.model();
    const std::set<std::string> candidates = {"mid", "leaf"};
    using Map = std::unordered_map<std::string, std::string>;
    CHECK_EQ(ost_drag::area_child_parent_map(model, {"top", "mid"}, &candidates),
             (Map{{"mid", "top"}, {"leaf", "top"}}));
    CHECK_EQ(ost_drag::area_child_parent_map(model, {"mid"}, &candidates), (Map{{"leaf", "mid"}}));
    CHECK_EQ(ost_drag::area_child_parent_map(model, {"top"}, &candidates), (Map{{"mid", "top"}, {"leaf", "top"}}));
    // without candidates every descendant of a selected area follows, a linear child included (Windows run)
    CHECK_EQ(ost_drag::area_child_parent_map(model, {"top"}),
             (Map{{"mid", "top"}, {"leaf", "top"}, {"line", "top"}}));
    // the descendants of a LINEAR parent do not follow it (only areas expand)
    Family g = area_family();
    g.add(make_takeoff("l", "line", std::vector<double>(4, 0.0)));
    g.add(make_takeoff("c", "area", std::vector<double>(6, 0.0), "l"));
    CHECK(ost_drag::area_child_parent_map(g.model(), {"l"}).empty());
}

// Port of test_area_child_parent_map_terminates_on_cycles_not_involving_the_child: a <-> b are each other's parent, c is a
// child of a; the map terminates and gives {c: a}.
TEST(GroupAreaChildMap, TerminatesOnCyclesNotInvolvingTheChild)
{
    Family f = area_family();
    f.add(make_takeoff("a", "area", std::vector<double>(6, 0.0), "b"));
    f.add(make_takeoff("b", "area", std::vector<double>(6, 0.0), "a"));
    f.add(make_takeoff("c", "area", std::vector<double>(6, 0.0), "a"));
    const std::set<std::string> candidates = {"c"};
    using Map = std::unordered_map<std::string, std::string>;
    CHECK_EQ(ost_drag::area_child_parent_map(f.model(), {"a"}, &candidates), (Map{{"c", "a"}}));
}

TEST(GroupExpandedUids, SelectedPlusDescendantsOfSelectedAreas)
{
    Family f = area_family();
    f.add(make_takeoff("top", "area", std::vector<double>(6, 0.0)));
    f.add(make_takeoff("mid", "area", std::vector<double>(6, 0.0), "top"));
    f.add(make_takeoff("leaf", "cat", {1.0, 1.0}, "mid"));
    f.add(make_takeoff("l", "line", std::vector<double>(4, 0.0)));
    f.add(make_takeoff("lc", "area", std::vector<double>(6, 0.0), "l"));
    const auto model = f.model();
    CHECK_EQ(ost_drag::expanded_transform_uids(model, {"top"}), (std::set<std::string>{"top", "mid", "leaf"}));
    CHECK_EQ(ost_drag::expanded_transform_uids(model, {"l"}), (std::set<std::string>{"l"}));
    CHECK_EQ(ost_drag::expanded_transform_uids(model, {"mid", "l"}), (std::set<std::string>{"mid", "leaf", "l"}));
    CHECK_EQ(ost_drag::expanded_transform_uids(model, {"unknown"}), (std::set<std::string>{"unknown"}));
}

// Port of test_group_translation_moves_nested_children_by_the_parent_displacement (inc 5): top = [1,2,11,2,1,12], mid (a hole of
// top) = [3,4,5,4,3,6]; delta (7.6, 12.4) -> top [11,12,21,12,11,22], mid [13,14,15,14,13,16] (the child moves by the parent's
// displacement, not by its own snapped delta).
TEST(GroupTranslation, NestedChildrenMoveByTheParentDisplacement)
{
    Family f;
    f.add_condition(make_condition("area", ConditionType::Area));
    f.add(make_takeoff("top", "area", {1.0, 2.0, 11.0, 2.0, 1.0, 12.0}));
    f.add(make_takeoff("mid", "area", {3.0, 4.0, 5.0, 4.0, 3.0, 6.0}, "top"));
    const auto model = f.model();
    const PositionMap orig = {{"top", f.takeoffs[0].position}, {"mid", f.takeoffs[1].position}};
    const PositionMap r = ost_drag::compute_group_translation_positions(model, orig, 7.6, 12.4, 5.0);
    check_position(r.at("top"), {11.0, 12.0, 21.0, 12.0, 11.0, 22.0});
    check_position(r.at("mid"), {13.0, 14.0, 15.0, 14.0, 13.0, 16.0});
}

// Port of test_group_translation_is_rejected_as_a_whole_when_children_would_become_invalid: a hole moved alone (its parent is not
// selected) out of the parent -> the ORIGINAL positions of the whole group are returned (Windows run: delta (7.6, 12.4) at inc 5 and
// at inc 0 gives the hole back unchanged; (2, 1) at inc 0 still fits and moves; so does a small move inside).
TEST(GroupTranslation, MoveIsRejectedAsAWholeWhenAHoleWouldLeaveItsParent)
{
    Family f;
    f.add_condition(make_condition("area", ConditionType::Area));
    f.add_condition(make_condition("line", ConditionType::Linear));
    f.add(make_takeoff("top", "area", {1.0, 2.0, 11.0, 2.0, 1.0, 12.0}));
    f.add(make_takeoff("mid", "area", {3.0, 4.0, 5.0, 4.0, 3.0, 6.0}, "top"));
    f.add(make_takeoff("ln", "line", {0.0, 0.0, 1.0, 1.0}));
    const auto model = f.model();
    const std::vector<double> hole = f.takeoffs[1].position;
    const std::vector<double> line = f.takeoffs[2].position;

    // the hole and an unrelated linear are selected: the linear would move, but the whole group stays
    const PositionMap orig = {{"mid", hole}, {"ln", line}};
    const PositionMap rejected = ost_drag::compute_group_translation_positions(model, orig, 7.6, 12.4, 5.0);
    check_position(rejected.at("mid"), hole, 0.0);
    check_position(rejected.at("ln"), line, 0.0);
    const PositionMap rejected_raw = ost_drag::compute_group_translation_positions(model, orig, 7.6, 12.4, 0.0);
    check_position(rejected_raw.at("mid"), hole, 0.0);
    check_position(rejected_raw.at("ln"), line, 0.0);

    // still inside the parent: accepted (Windows run, inc 0: [5,5,7,5,5,7] for delta (2, 1))
    const PositionMap accepted = ost_drag::compute_group_translation_positions(model, orig, 2.0, 1.0, 0.0);
    check_position(accepted.at("mid"), {5.0, 5.0, 7.0, 5.0, 5.0, 7.0});
    check_position(accepted.at("ln"), {2.0, 1.0, 3.0, 2.0});
}

// Port of test_group_translation_skips_children_whose_parent_has_no_position, with the real validation instead of the mock the
// Windows test uses: the follower of a parent without position is not translated (no displacement to derive), and an area without a
// polygon cannot contain its hole, so the whole group stays (Windows run: {top: [], mid: [3, 4]} unchanged).
TEST(GroupTranslation, ChildOfAParentWithoutPositionIsNotTranslated)
{
    Family f;
    f.add_condition(make_condition("area", ConditionType::Area));
    f.add(make_takeoff("top", "area", {}));
    f.add(make_takeoff("mid", "area", {3.0, 4.0}, "top"));
    const PositionMap orig = {{"top", {}}, {"mid", {3.0, 4.0}}};
    const PositionMap r = ost_drag::compute_group_translation_positions(f.model(), orig, 5.0, 5.0, 5.0);
    CHECK_EQ(r.size(), static_cast<size_t>(2));
    REQUIRE(r.count("top") == 1);
    REQUIRE(r.count("mid") == 1);
    CHECK(r.at("top").empty());
    check_position(r.at("mid"), {3.0, 4.0}, 0.0);
}

// GEO-M01 / GEO-M02: a group move of a parent selected with its hole and attachment moves them together by the parent's
// displacement and is valid (Windows run: inc 5, delta (7.6, 12.4) -> every shape by (10, 10)).
TEST(GroupTranslation, ParentHoleAndAttachmentMoveTogether)
{
    Family f = area_family();
    const std::vector<double> parent = {0.0, 0.0, 100.0, 0.0, 100.0, 100.0, 0.0, 100.0};
    f.add(make_takeoff("p", "area", parent));
    f.add(make_takeoff("h", "area", {20.0, 20.0, 40.0, 20.0, 40.0, 40.0, 20.0, 40.0}, "p"));
    f.add(make_takeoff("a", "cat", {60.0, 60.0}, "p"));
    const auto model = f.model();
    const PositionMap orig = {{"p", parent}, {"h", f.takeoffs[1].position}, {"a", f.takeoffs[2].position}};
    const PositionMap r = ost_drag::compute_group_translation_positions(model, orig, 7.6, 12.4, 5.0);
    check_position(r.at("p"), {10.0, 10.0, 110.0, 10.0, 110.0, 110.0, 10.0, 110.0});
    check_position(r.at("h"), {30.0, 30.0, 50.0, 30.0, 50.0, 50.0, 30.0, 50.0});
    check_position(r.at("a"), {70.0, 70.0});
}

// ---------------------------------------------------------------------------------------------------------------
// RPV-15: the children of an area that is dragged by its body
// ---------------------------------------------------------------------------------------------------------------

// Port of test_area_release_keeps_the_last_valid_polygon_and_moves_child_holes: the parent [1,2,11,2,11,12,1,12] ends at
// [4,9,14,9,14,19,4,19] (displacement (3, 7)); the hole [3,4,5,4,5,6,3,6] follows to [6,11,8,11,8,13,6,13]. RPV-15 plan: the same
// with a three-point hole [3,4,5,4,5,6] -> [6,11,8,11,8,13].
TEST(BodyDragChildren, HolesFollowTheirParentByTheFirstPointDisplacement)
{
    Family f = area_family();
    const std::vector<double> square = {1.0, 2.0, 11.0, 2.0, 11.0, 12.0, 1.0, 12.0};
    const std::vector<double> moved = {4.0, 9.0, 14.0, 9.0, 14.0, 19.0, 4.0, 19.0};
    f.add(make_takeoff("t1", "area", square));
    f.add(make_takeoff("hole", "area", {3.0, 4.0, 5.0, 4.0, 5.0, 6.0, 3.0, 6.0}, "t1"));
    f.add(make_takeoff("tri", "area", {3.0, 4.0, 5.0, 4.0, 5.0, 6.0}, "t1"));
    const auto model = f.model();
    const auto changes = ost_drag::descendants_following_body_drag(model, "t1", -1, square, moved);
    REQUIRE_EQ(changes.size(), static_cast<size_t>(2));
    CHECK_EQ(changes[0].uid, std::string("hole"));
    check_position(changes[0].new_position, {6.0, 11.0, 8.0, 11.0, 8.0, 13.0, 6.0, 13.0});
    CHECK_EQ(changes[0].old_position, (std::vector<double>{3.0, 4.0, 5.0, 4.0, 5.0, 6.0, 3.0, 6.0}));
    CHECK_EQ(changes[1].uid, std::string("tri"));
    check_position(changes[1].new_position, {6.0, 11.0, 8.0, 11.0, 8.0, 13.0});
}

// Port of test_child_holes_only_follow_a_body_drag (a vertex drag, handle 1, leaves the hole) and
// test_child_takeoffs_do_not_follow_the_body_drag_of_a_non_area_parent (a linear parent), RPV-15 plan: a corner-handle drag
// (handle 1) leaves the hole at [3,4,5,4,5,6]; UND-M01: handle 0 or an edge handle leave it too.
TEST(BodyDragChildren, OnlyAnAreaDraggedByItsBodyMovesItsChildren)
{
    Family f = area_family();
    const std::vector<double> square = {1.0, 2.0, 11.0, 2.0, 11.0, 12.0, 1.0, 12.0};
    const std::vector<double> moved = {4.0, 9.0, 14.0, 9.0, 14.0, 19.0, 4.0, 19.0};
    f.add(make_takeoff("t1", "area", square));
    f.add(make_takeoff("hole", "area", {3.0, 4.0, 5.0, 4.0, 5.0, 6.0}, "t1"));
    f.add(make_takeoff("l1", "line", {0.0, 0.0, 10.0, 0.0}));
    f.add(make_takeoff("child", "area", {3.0, 4.0, 5.0, 4.0}, "l1"));
    const auto model = f.model();
    for (int handle : {0, 1, 3, 4, 7})
    {
        SCOPED_CONTEXT("handle " + std::to_string(handle));
        CHECK(ost_drag::descendants_following_body_drag(model, "t1", handle, square, moved).empty());
    }
    CHECK(ost_drag::descendants_following_body_drag(model, "l1", -1, {0.0, 0.0, 10.0, 0.0}, {3.0, 7.0, 13.0, 7.0}).empty());
    CHECK(ost_drag::descendants_following_body_drag(model, "missing", -1, square, moved).empty());
    // fewer than two values before or after: nothing to derive a displacement from
    CHECK(ost_drag::descendants_following_body_drag(model, "t1", -1, {}, moved).empty());
    CHECK(ost_drag::descendants_following_body_drag(model, "t1", -1, square, {7.0}).empty());
}

// GEO-M01 plan: parent [0.4,0.4,10.4,0.4,10.4,10.4,0.4,10.4], hole [2,2,4,2,4,4,2,4], attachment [5,5] (both children of the parent),
// snap increment 1.0, whole-shape drag with OST delta (0.3, 0.3): the parent ends at [1,1,11,1,11,11,1,11] (compute_new_position),
// the children follow by its first-point displacement 0.6: hole [2.6,2.6,4.6,2.6,4.6,4.6,2.6,4.6], attachment [5.6,5.6].
TEST(BodyDragChildren, ChildrenFollowTheSnappedParentDisplacementNotTheRawDelta)
{
    Family f = area_family();
    const std::vector<double> parent = {0.4, 0.4, 10.4, 0.4, 10.4, 10.4, 0.4, 10.4};
    f.add(make_takeoff("p", "area", parent));
    f.add(make_takeoff("h", "area", {2.0, 2.0, 4.0, 2.0, 4.0, 4.0, 2.0, 4.0}, "p"));
    f.add(make_takeoff("a", "cat", {5.0, 5.0}, "p"));
    const auto model = f.model();
    const auto new_parent = ost_drag::compute_new_position(parent, 0.3, 0.3, -1, 4, 1.0, 0.0);
    check_position(new_parent, {1.0, 1.0, 11.0, 1.0, 11.0, 11.0, 1.0, 11.0});
    const auto changes = ost_drag::descendants_following_body_drag(model, "p", -1, parent, new_parent);
    REQUIRE_EQ(changes.size(), static_cast<size_t>(2));
    CHECK_EQ(changes[0].uid, std::string("h"));
    check_position(changes[0].new_position, {2.6, 2.6, 4.6, 2.6, 4.6, 4.6, 2.6, 4.6});
    CHECK_EQ(changes[1].uid, std::string("a"));
    check_position(changes[1].new_position, {5.6, 5.6});
}

// UND-M01 plan (Windows fixture of test_area_parent_drag_translates_descendant_holes_with_before_edit): parent (area)
// [0,0,10,0,10,10,0,10], hole (parent_uid = parent) [2,2,4,2,4,4,2,4], unrelated area "other" [50,50,60,50,60,60,50,60]. A body drag of
// the parent by (3, 2) without snapping gives parent [3,2,13,2,13,12,3,12], hole [5,4,7,4,7,6,5,6], other unchanged: the followers are
// exactly the hole, with its old position for the history entry.
TEST(BodyDragChildren, OnlyDescendantsOfTheDraggedParentAreCommittedWithIt)
{
    Family f = area_family();
    const std::vector<double> parent = {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0};
    f.add(make_takeoff("parent", "area", parent));
    f.add(make_takeoff("hole", "area", {2.0, 2.0, 4.0, 2.0, 4.0, 4.0, 2.0, 4.0}, "parent"));
    f.add(make_takeoff("other", "area", {50.0, 50.0, 60.0, 50.0, 60.0, 60.0, 50.0, 60.0}));
    const auto model = f.model();
    const auto new_parent = ost_drag::compute_new_position(parent, 3.0, 2.0, -1, 4, 0.0, 0.0);
    check_position(new_parent, {3.0, 2.0, 13.0, 2.0, 13.0, 12.0, 3.0, 12.0});
    const auto changes = ost_drag::descendants_following_body_drag(model, "parent", -1, parent, new_parent);
    REQUIRE_EQ(changes.size(), static_cast<size_t>(1));
    CHECK_EQ(changes[0].uid, std::string("hole"));
    CHECK_EQ(changes[0].old_position, (std::vector<double>{2.0, 2.0, 4.0, 2.0, 4.0, 4.0, 2.0, 4.0}));
    check_position(changes[0].new_position, {5.0, 4.0, 7.0, 4.0, 7.0, 6.0, 5.0, 6.0});
}

// Descendants at every depth follow (expand_takeoff_uids_with_descendants), children without a position are skipped.
TEST(BodyDragChildren, NestedDescendantsFollowAndPositionlessChildrenAreSkipped)
{
    Family f = area_family();
    const std::vector<double> parent = {0.0, 0.0, 100.0, 0.0, 100.0, 100.0, 0.0, 100.0};
    f.add(make_takeoff("p", "area", parent));
    f.add(make_takeoff("h", "area", {10.0, 10.0, 60.0, 10.0, 60.0, 60.0, 10.0, 60.0}, "p"));
    f.add(make_takeoff("n", "area", {20.0, 20.0, 30.0, 20.0, 30.0, 30.0, 20.0, 30.0}, "h"));
    f.add(make_takeoff("empty", "area", {}, "p"));
    const auto model = f.model();
    const auto changes = ost_drag::descendants_following_body_drag(
        model, "p", -1, parent, {5.0, 6.0, 105.0, 6.0, 105.0, 106.0, 5.0, 106.0});
    REQUIRE_EQ(changes.size(), static_cast<size_t>(2));
    CHECK_EQ(changes[0].uid, std::string("h"));
    check_position(changes[0].new_position, {15.0, 16.0, 65.0, 16.0, 65.0, 66.0, 15.0, 66.0});
    CHECK_EQ(changes[1].uid, std::string("n"));
    check_position(changes[1].new_position, {25.0, 26.0, 35.0, 26.0, 35.0, 36.0, 25.0, 36.0});
}
