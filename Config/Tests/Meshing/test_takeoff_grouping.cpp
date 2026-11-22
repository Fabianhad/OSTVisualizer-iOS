// Hole classification (MSH-14): the ONE function shared by the 3D view and the exports.
// Windows oracle: domain/services/takeoff_domain_service.py group_area_takeoffs_with_holes and
// presentation/visualization/meshing/mesh_builder.py get_holes_for_takeoff.
#include "MeshingTestUtil.hpp"

#include <string>
#include <unordered_map>
#include <vector>

using namespace mesh_test;
using ost_meshing::AreaHoleGrouping;

namespace
{
    using CondMap = std::unordered_map<std::string, Condition>;

    CondMap conditions()
    {
        CondMap m;
        m["area"] = make_condition("area", ConditionType::Area);
        m["line"] = make_condition("line", ConditionType::Linear);
        m["count"] = make_condition("count", ConditionType::Count);
        m["att"] = make_condition("att", ConditionType::Attachment);
        return m;
    }

    std::vector<const Takeoff *> ptrs(const std::vector<Takeoff> &v)
    {
        std::vector<const Takeoff *> out;
        for (const auto &t : v)
            out.push_back(&t);
        return out;
    }

    std::vector<std::string> holes_of(const AreaHoleGrouping &g, const std::string &parent)
    {
        auto it = g.holes.find(parent);
        return it == g.holes.end() ? std::vector<std::string>{} : uid_list(it->second);
    }

    Takeoff tk(const std::string &uid, const std::string &cond, const std::string &parent = "0")
    {
        return make_takeoff(uid, cond, "p", {0, 0, 1, 0, 1, 1}, parent);
    }
} // namespace

TEST(AreaHoleGrouping, AreaHolesAreGroupedUnderTheirAreaParent)
{
    // Port of tests/domain/services/test_takeoff_domain_service.py::TakeoffGroupingTests::
    //   test_area_holes_are_grouped_under_their_area_parent
    auto conds = conditions();
    std::vector<Takeoff> in{tk("parent", "area"),         tk("hole1", "area", "parent"), tk("line1", "line"),
                            tk("other", "area"),          tk("hole2", "area", "parent"), tk("lone", "area", "gone"),
                            tk("unknown", "missing")};
    auto g = ost_meshing::group_area_takeoffs_with_holes(ptrs(in), conds);
    // Non-area takeoffs (including unknown conditions) come first, then the area takeoffs that are not claimed as a
    // hole of an area parent.
    CHECK_EQ(uid_list(g.takeoffs), (std::vector<std::string>{"line1", "unknown", "parent", "other", "lone"}));
    CHECK_EQ(g.holes.size(), static_cast<size_t>(1));
    CHECK_EQ(holes_of(g, "parent"), (std::vector<std::string>{"hole1", "hole2"}));
}

TEST(AreaHoleGrouping, WithoutAnyAreaTakeoffTheInputListIsReturnedUnchanged)
{
    // Port of ...::test_without_any_area_takeoff_the_input_list_is_returned_unchanged
    auto conds = conditions();
    std::vector<Takeoff> in{tk("1", "line"), tk("2", "missing")};
    auto g = ost_meshing::group_area_takeoffs_with_holes(ptrs(in), conds);
    CHECK_EQ(uid_list(g.takeoffs), (std::vector<std::string>{"1", "2"}));
    CHECK(g.holes.empty());
    auto empty = ost_meshing::group_area_takeoffs_with_holes({}, conds);
    CHECK(empty.takeoffs.empty());
    CHECK(empty.holes.empty());
}

TEST(AreaHoleGrouping, AHoleUnderANonAreaParentStaysATopLevelTakeoff)
{
    // Port of ...::test_a_hole_under_a_non_area_parent_stays_a_top_level_takeoff
    auto conds = conditions();
    std::vector<Takeoff> in{tk("line1", "line"), tk("stray", "area", "line1")};
    auto g = ost_meshing::group_area_takeoffs_with_holes(ptrs(in), conds);
    CHECK_EQ(uid_list(g.takeoffs), (std::vector<std::string>{"line1", "stray"}));
    CHECK(g.holes.empty());
}

TEST(AreaHoleGrouping, LinearCountAndAttachmentChildrenOfAnAreaStayOrdinaryTakeoffs)
{
    // MSH-14 / GEO-04 plan: a parented Linear / Count / Attachment child of an area is meshed as its own element and is
    // NOT a hole of the parent (iOS used to skip it entirely).
    auto conds = conditions();
    std::vector<Takeoff> in{tk("slab", "area"), tk("l1", "line", "slab"), tk("c1", "count", "slab"),
                            tk("a1", "att", "slab")};
    auto g = ost_meshing::group_area_takeoffs_with_holes(ptrs(in), conds);
    CHECK_EQ(uid_list(g.takeoffs), (std::vector<std::string>{"l1", "c1", "a1", "slab"}));
    CHECK(g.holes.empty());
}

TEST(AreaHoleGrouping, ParentOfAnEmptyOrZeroUidIsNeverAHole)
{
    // Windows Takeoff.is_hole: parent_uid not in {"", "0"}.
    auto conds = conditions();
    std::vector<Takeoff> in{tk("a", "area", "0"), tk("b", "area", ""), tk("c", "area", "a")};
    auto g = ost_meshing::group_area_takeoffs_with_holes(ptrs(in), conds);
    CHECK_EQ(uid_list(g.takeoffs), (std::vector<std::string>{"a", "b"}));
    CHECK_EQ(holes_of(g, "a"), (std::vector<std::string>{"c"}));
}

TEST(AreaHoleGrouping, NestedHolesAreAllClaimed)
{
    // Windows: hole2 -> hole1 -> parent, all area: both are in child_uids (only `parent` stays a root) and each is
    // registered under its own parent.
    auto conds = conditions();
    std::vector<Takeoff> in{tk("parent", "area"), tk("hole1", "area", "parent"), tk("hole2", "area", "hole1")};
    auto g = ost_meshing::group_area_takeoffs_with_holes(ptrs(in), conds);
    CHECK_EQ(uid_list(g.takeoffs), (std::vector<std::string>{"parent"}));
    CHECK_EQ(holes_of(g, "parent"), (std::vector<std::string>{"hole1"}));
    CHECK_EQ(holes_of(g, "hole1"), (std::vector<std::string>{"hole2"}));
}

TEST(AreaHoleGrouping, AnAreaTakeoffWithItselfAsParentVanishesLikeOnWindows)
{
    // Windows: parent_uid == own uid is a hole of itself (its uid is in the area map): it leaves the root list.
    auto conds = conditions();
    std::vector<Takeoff> in{tk("self", "area", "self"), tk("x", "line")};
    auto g = ost_meshing::group_area_takeoffs_with_holes(ptrs(in), conds);
    CHECK_EQ(uid_list(g.takeoffs), (std::vector<std::string>{"x"}));
    CHECK_EQ(holes_of(g, "self"), (std::vector<std::string>{"self"}));
}

TEST(AreaHoleGrouping, HolesKeepInputOrderPerParent)
{
    auto conds = conditions();
    std::vector<Takeoff> in{tk("h2", "area", "p2"), tk("p1", "area"), tk("h1b", "area", "p1"), tk("p2", "area"),
                            tk("h1a", "area", "p1"), tk("h2b", "area", "p2")};
    auto g = ost_meshing::group_area_takeoffs_with_holes(ptrs(in), conds);
    CHECK_EQ(uid_list(g.takeoffs), (std::vector<std::string>{"p1", "p2"}));
    CHECK_EQ(holes_of(g, "p1"), (std::vector<std::string>{"h1b", "h1a"}));
    CHECK_EQ(holes_of(g, "p2"), (std::vector<std::string>{"h2", "h2b"}));
}

TEST(HolesForTakeoff, RingsNeedSixValuesAndThreePoints)
{
    // Windows get_holes_for_takeoff / test_mesh_processor::test_area_holes_and_display_options_reach_their_collaborators:
    // the hole [1,1,2,1,2,2] is used, the one-point child [1,1] is not; an odd trailing value is ignored.
    auto conds = conditions();
    std::vector<Takeoff> in{make_takeoff("outer", "area", "p", {}), make_takeoff("hole", "area", "p", {1, 1, 2, 1, 2, 2}, "outer"),
                            make_takeoff("flat", "area", "p", {1, 1}, "outer"),
                            make_takeoff("odd", "area", "p", {5, 5, 6, 5, 6, 6, 9}, "outer"),
                            make_takeoff("five", "area", "p", {1, 1, 2, 2, 3}, "outer")};
    auto g = ost_meshing::group_area_takeoffs_with_holes(ptrs(in), conds);
    CHECK_EQ(holes_of(g, "outer"), (std::vector<std::string>{"hole", "flat", "odd", "five"}));
    auto rings = ost_meshing::holes_for_takeoff("outer", g);
    REQUIRE_EQ(rings.size(), static_cast<size_t>(2));
    using P = std::pair<double, double>;
    CHECK(rings[0] == (std::vector<P>{{1, 1}, {2, 1}, {2, 2}}));
    CHECK(rings[1] == (std::vector<P>{{5, 5}, {6, 5}, {6, 6}})); // trailing 9 ignored
    CHECK(ost_meshing::holes_for_takeoff("nobody", g).empty());
    CHECK(ost_meshing::holes_for_takeoff("", g).empty());
}
