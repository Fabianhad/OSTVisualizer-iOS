// GEO-M02: hole / attachment containment validation when a parent area is edited (and when a hole or an attachment is
// moved). Windows oracle: plan_view/components/drag_handler.py (_attachment_position_valid,
// _takeoff_children_valid_for_geometry_changes, _validate_parent_contains_holes, _validate_hole_position,
// _has_child_holes) and plan_view/components/geometry_utils.py (_attachment_footprint_path, attachment_fits_area).
// The generated tables (HierarchyOracleData.hpp, FootprintOracleData.hpp) are answers of the real Windows code run
// read-only (python -B, PySide6 6.10.1) on an identity coordinate system.
#include "FootprintOracleData.hpp"
#include "HierarchyOracleData.hpp"
#include "HierarchyTestSupport.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace geo_test;
namespace hier = ost_hierarchy;

namespace
{
    const std::vector<double> SQUARE_100 = {0.0, 0.0, 100.0, 0.0, 100.0, 100.0, 0.0, 100.0};
    const double kPi = 3.14159265358979323846;

    Takeoff T(const std::string &uid, const std::string &cond, std::vector<double> position,
              const std::string &parent = "0", const std::string &page = "pg1", double rotation = 0.0)
    {
        return make_takeoff(uid, cond, std::move(position), parent, page, rotation);
    }

    // Windows HierarchyDragTests._attachment_view: parent "p" (square 100), attachment "a" (cat at (20, 20)) + extras.
    struct AttachmentView
    {
        Family family;
        Takeoff attachment;
    };
    AttachmentView attachment_view(const std::vector<Takeoff> &extra = {}, const Takeoff *parent = nullptr,
                                   const Takeoff *attachment = nullptr)
    {
        const Takeoff p = parent ? *parent : T("p", "ca", SQUARE_100);
        const Takeoff a = attachment ? *attachment : T("a", "cat", {20.0, 20.0}, "p");
        std::vector<Takeoff> all = {p, a};
        all.insert(all.end(), extra.begin(), extra.end());
        return {windows_family(all), a};
    }

    bool av(const AttachmentView &v, const std::vector<double> &position,
            const std::vector<double> *parent_position = nullptr,
            const hier::PositionOverrides *takeoff_positions = nullptr,
            const std::optional<double> &rotation = std::nullopt)
    {
        const auto model = v.family.model();
        return hier::attachment_position_valid(model, v.attachment, position, parent_position, takeoff_positions,
                                               rotation);
    }

    Family family_ph()
    {
        return windows_family({
            T("p", "ca", SQUARE_100),
            T("h1", "ca", square(10, 10, 30, 30), "p"),
            T("h2", "ca", square(50, 50, 70, 70), "p"),
            T("a", "cat", {90.0, 90.0}, "p"),
            T("free", "cl", {1.0, 1.0, 2.0, 2.0}),
        });
    }

    bool cv(const Family &f, const hier::PositionOverrides &po, const hier::RotationOverrides &ro = {})
    {
        return hier::takeoff_children_valid_for_geometry_changes(f.model(), po, ro);
    }

    bool pc(const Family &f, const std::string &uid, const std::vector<double> &pos)
    {
        return hier::validate_parent_contains_holes(f.model(), uid, pos);
    }

    bool hp(Family &f, const Takeoff &hole, const std::vector<double> &pos)
    {
        return hier::validate_hole_position(f.model(), hole, pos);
    }
} // namespace

// ---- footprint (geometry_utils._attachment_footprint_path) ---------------------------------------------------------

namespace
{
    Condition footprint_condition(ConditionType type = ConditionType::Attachment)
    {
        return make_condition("attachment", type, ShapeId::Rectangle, 10.0, 4.0);
    }
    const std::vector<double> kParent100 = {0.0, 0.0, 100.0, 0.0, 100.0, 100.0, 0.0, 100.0};
} // namespace

// Port of tests/presentation/components/plan_view/components/test_geometry_utils.py::AttachmentFitTests
//   ::test_footprint_is_the_condition_rectangle_centred_on_the_position
TEST(AttachmentFootprint, IsTheConditionRectangleCentredOnThePosition)
{
    const Polygon p = hier::attachment_footprint(footprint_condition(), {50.0, 30.0}, 0.0);
    REQUIRE_EQ(p.size(), size_t(4));
    const double expected[4][2] = {{45.0, 28.0}, {55.0, 28.0}, {55.0, 32.0}, {45.0, 32.0}};
    for (size_t i = 0; i < 4; ++i)
    {
        CHECK_EQ(p[i][0], expected[i][0]);
        CHECK_EQ(p[i][1], expected[i][1]);
    }
    const BBox box = bounding_rect(p);
    CHECK_EQ(box.x, 45.0);
    CHECK_EQ(box.y, 28.0);
    CHECK_EQ(box.w, 10.0);
    CHECK_EQ(box.h, 4.0);
}

// Port of ...::AttachmentFitTests::test_footprint_rotation_is_in_radians_and_swaps_the_extents
TEST(AttachmentFootprint, RotationIsInRadiansAndSwapsTheExtents)
{
    const Polygon p = hier::attachment_footprint(footprint_condition(), {50.0, 30.0}, kPi / 2);
    REQUIRE_EQ(p.size(), size_t(4));
    const BBox box = bounding_rect(p);
    CHECK_NEAR(box.x, 48.0, 1e-6);
    CHECK_NEAR(box.y, 25.0, 1e-6);
    CHECK_NEAR(box.w, 4.0, 1e-6);
    CHECK_NEAR(box.h, 10.0, 1e-6);
}

// Port of ...::AttachmentFitTests::test_footprint_is_empty_for_non_attachments_or_malformed_positions
TEST(AttachmentFootprint, IsEmptyForNonAttachmentsOrMalformedPositions)
{
    for (ConditionType type : {ConditionType::Linear, ConditionType::Area, ConditionType::Count})
    {
        SCOPED_CONTEXT("condition type " + std::to_string(static_cast<int>(type)));
        CHECK(hier::attachment_footprint(footprint_condition(type), {50.0, 30.0}, 0.0).empty());
    }
    const std::vector<std::vector<double>> bad = {{}, {50.0}, {50.0, 30.0, 1.0}};
    for (size_t i = 0; i < bad.size(); ++i)
    {
        SCOPED_CONTEXT("position " + std::to_string(i));
        CHECK(hier::attachment_footprint(footprint_condition(), bad[i], 0.0).empty());
    }
}

// Windows (read-only run of compute_count_vertices(x, y, *resolve_point_takeoff_shape(condition), rotation)) for every
// shape id incl. an unknown one (a rectangle), minimum sizes (width/depth below 1 or 0 or negative), SQUARE/CIRCLE
// taking depth = width, and a display size that an attachment ignores.
TEST(AttachmentFootprint, EveryShapeMatchesTheWindowsVertices)
{
    const size_t count = sizeof(kFootprintOracle) / sizeof(kFootprintOracle[0]);
    REQUIRE_EQ(count, size_t(135));
    for (size_t i = 0; i < count; ++i)
    {
        const FootprintOracleRow &row = kFootprintOracle[i];
        SCOPED_CONTEXT("row " + std::to_string(i) + " shape " + std::to_string(row.shape) + " w " +
                       std::to_string(row.width) + " d " + std::to_string(row.depth) + " rot " +
                       std::to_string(row.rotation));
        const Condition c = make_condition("c", ConditionType::Attachment, static_cast<ShapeId>(row.shape), row.width,
                                           row.depth, 250.0);
        const Polygon p = hier::attachment_footprint(c, {10.5, -3.25}, row.rotation);
        REQUIRE_EQ(p.size() * 2, row.vertices.size());
        for (size_t v = 0; v < p.size(); ++v)
        {
            // cos / sin of the platform libm: relative 1e-9 (bit identical on MSVC)
            CHECK_NEAR(p[v][0], row.vertices[v * 2], 1e-9 * std::max(1.0, std::fabs(row.vertices[v * 2])));
            CHECK_NEAR(p[v][1], row.vertices[v * 2 + 1], 1e-9 * std::max(1.0, std::fabs(row.vertices[v * 2 + 1])));
        }
    }
}

// Port of ...::AttachmentFitTests::test_attachment_fits_area_checks_containment_and_backout_exclusions
TEST(AttachmentFootprint, FitsAreaChecksContainmentAndBackoutExclusions)
{
    const Condition condition = footprint_condition();
    const std::vector<double> backout = {40.0, 20.0, 60.0, 20.0, 60.0, 40.0, 40.0, 40.0};
    const std::vector<double> far_backout = {80.0, 80.0, 90.0, 80.0, 90.0, 90.0, 80.0, 90.0};
    struct Case
    {
        const char *label;
        std::vector<double> position;
        std::vector<std::vector<double>> backouts;
        bool expected;
    };
    const Case cases[] = {
        {"inside, no backouts", {50.0, 60.0}, {}, true},
        {"inside, distant backout", {50.0, 60.0}, {far_backout}, true},
        {"overlaps a backout", {50.0, 30.0}, {far_backout, backout}, false},
        {"overlaps generator backout", {50.0, 30.0}, {backout}, false},
        {"sticks out of the area", {97.0, 60.0}, {}, false},
        {"entirely outside", {300.0, 300.0}, {}, false},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.label);
        CHECK_EQ(hier::attachment_fits_area(condition, c.position, 0.0, kParent100, c.backouts), c.expected);
    }
}

// Port of ...::AttachmentFitTests::test_attachment_fits_area_is_false_for_non_attachment_or_empty_parent
TEST(AttachmentFootprint, FitsAreaIsFalseForNonAttachmentOrEmptyParent)
{
    CHECK_FALSE(hier::attachment_fits_area(footprint_condition(ConditionType::Area), {50.0, 60.0}, 0.0, kParent100, {}));
    CHECK_FALSE(hier::attachment_fits_area(footprint_condition(), {50.0, 60.0}, 0.0, {0.0, 0.0}, {}));
}

// ---- _has_child_holes -----------------------------------------------------------------------------------------------

// Port of tests/presentation/components/plan_view/components/test_drag_handler.py::HierarchyDragTests
//   ::test_has_child_holes_reports_only_children_of_that_parent
TEST(HierarchyValidation, HasChildHolesReportsOnlyChildrenOfThatParent)
{
    Family f = windows_family({T("p", "ca", SQUARE_100), T("h", "ca", square(1, 1, 2, 2), "p"),
                               T("lonely", "ca", SQUARE_100)});
    const auto model = f.model();
    CHECK_EQ(hier::has_child_holes(model, "p"), true);
    CHECK_EQ(hier::has_child_holes(model, "lonely"), false);
    CHECK_EQ(hier::has_child_holes(model, "missing"), false);
}

// iOS API contract (a Python dict keeps the last value of a repeated key): a repeated uid resolves to the last entry.
TEST(HierarchyValidation, RepeatedUidResolvesToTheLastTakeoff)
{
    Family f = windows_family({T("x", "ca", square(0, 0, 1, 1)), T("x", "cl", {5.0, 5.0, 6.0, 6.0})});
    const auto model = f.model();
    REQUIRE(model.find_takeoff("x") != nullptr);
    CHECK_EQ(model.find_takeoff("x")->condition_uid, std::string("cl"));
    CHECK(model.find_takeoff("nope") == nullptr);
    CHECK(model.find_condition("ca") != nullptr);
    CHECK(model.find_condition("nope") == nullptr);
}

// ---- _attachment_position_valid -------------------------------------------------------------------------------------

// Port of ...::HierarchyDragTests::test_attachment_inside_its_area_is_valid
TEST(AttachmentPosition, InsideItsAreaIsValid)
{
    const AttachmentView v = attachment_view();
    CHECK_EQ(av(v, {20.0, 20.0}), true);
    CHECK_EQ(av(v, {150.0, 150.0}), false);
    CHECK_EQ(av(v, {99.0, 50.0}), false);
}

// Port of ...::HierarchyDragTests::test_attachment_needs_a_parent_on_the_same_page
TEST(AttachmentPosition, NeedsAParentOnTheSamePage)
{
    const Takeoff orphan = T("orphan", "cat", {20.0, 20.0}, "ghost");
    const Takeoff other_page = T("moved", "cat", {20.0, 20.0}, "p", "pg2");
    AttachmentView v = attachment_view({orphan, other_page});
    const auto model = v.family.model();
    CHECK_EQ(hier::attachment_position_valid(model, orphan, {20.0, 20.0}), false);
    CHECK_EQ(hier::attachment_position_valid(model, other_page, {20.0, 20.0}), false);
}

// Port of ...::HierarchyDragTests::test_attachment_parent_must_be_a_top_level_area_with_a_known_condition
TEST(AttachmentPosition, ParentMustBeATopLevelAreaWithAKnownCondition)
{
    const struct
    {
        const char *label;
        Takeoff parent;
    } cases[] = {
        {"unknown parent condition", T("p", "ghost", SQUARE_100)},
        {"non-area parent", T("p", "cl", SQUARE_100)},
        {"hole parent", T("p", "ca", SQUARE_100, "grand")},
    };
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT(c.label);
        const AttachmentView v = attachment_view({}, &c.parent);
        CHECK_EQ(av(v, {20.0, 20.0}), false);
    }
}

// Port of ...::HierarchyDragTests::test_attachment_condition_must_exist_and_be_an_attachment
TEST(AttachmentPosition, ConditionMustExistAndBeAnAttachment)
{
    for (const char *condition_uid : {"ghost", "ca", "cl"})
    {
        SCOPED_CONTEXT(std::string("condition ") + condition_uid);
        const Takeoff attachment = T("a", condition_uid, {20.0, 20.0}, "p");
        const AttachmentView v = attachment_view({}, nullptr, &attachment);
        CHECK_EQ(av(v, {20.0, 20.0}), false);
    }
}

// Port of ...::HierarchyDragTests::test_attachment_uses_an_overriding_parent_position
TEST(AttachmentPosition, UsesAnOverridingParentPosition)
{
    const AttachmentView v = attachment_view();
    const std::vector<double> small = square(0, 0, 10, 10);
    CHECK_EQ(av(v, {20.0, 20.0}, &small), false);
    const std::vector<double> large = square(0, 0, 500, 500);
    CHECK_EQ(av(v, {300.0, 300.0}, &large), true);
}

// Port of ...::HierarchyDragTests::test_attachment_avoids_area_backouts_but_not_other_children
TEST(AttachmentPosition, AvoidsAreaBackoutsButNotOtherChildren)
{
    const Takeoff hole = T("h", "ca", square(40, 40, 60, 60), "p");
    const Takeoff sibling_attachment = T("b", "cat", {50.0, 50.0}, "p");
    const Takeoff foreign_hole = T("fh", "ca", square(40, 40, 60, 60), "q");
    AttachmentView v = attachment_view({sibling_attachment, foreign_hole});
    CHECK_EQ(av(v, {50.0, 50.0}), true);
    v.family.add(hole);
    CHECK_EQ(av(v, {50.0, 50.0}), false);
    CHECK_EQ(av(v, {20.0, 20.0}), true);
}

// Port of ...::HierarchyDragTests::test_attachment_backout_positions_can_be_overridden_per_takeoff
TEST(AttachmentPosition, BackoutPositionsCanBeOverriddenPerTakeoff)
{
    const Takeoff hole = T("h", "ca", square(40, 40, 60, 60), "p");
    const AttachmentView v = attachment_view({hole});
    const hier::PositionOverrides moved = {{"h", square(80, 80, 90, 90)}};
    CHECK_EQ(av(v, {50.0, 50.0}, nullptr, &moved), true);
    const hier::PositionOverrides other = {{"other", square(0, 0, 1, 1)}};
    CHECK_EQ(av(v, {50.0, 50.0}, nullptr, &other), false);
    const hier::PositionOverrides none;
    CHECK_EQ(av(v, {50.0, 50.0}, nullptr, &none), false);
}

// Port of ...::HierarchyDragTests::test_attachment_rotation_defaults_to_the_takeoff_rotation_unless_overridden
TEST(AttachmentPosition, RotationDefaultsToTheTakeoffRotationUnlessOverridden)
{
    const double quarter_turn = kPi / 2;
    const Takeoff upright = T("a", "cat", {50.0, 3.0}, "p", "pg1", 0.0);
    const Takeoff turned = T("a", "cat", {50.0, 3.0}, "p", "pg1", quarter_turn);
    AttachmentView v = attachment_view({}, nullptr, &upright);
    CHECK_EQ(av(v, {50.0, 3.0}), true);
    CHECK_EQ(av(v, {50.0, 3.0}, nullptr, nullptr, quarter_turn), false);
    AttachmentView t = attachment_view({}, nullptr, &turned);
    CHECK_EQ(av(t, {50.0, 3.0}), false);
    CHECK_EQ(av(t, {50.0, 3.0}, nullptr, nullptr, 0.0), true);
}

// Port of ...::HierarchyDragGapTests::test_linear_siblings_are_not_attachment_backouts
TEST(AttachmentPosition, LinearSiblingsAreNotBackouts)
{
    const Takeoff line = T("line", "cl", {10.0, 10.0, 30.0, 10.0, 30.0, 30.0, 10.0, 30.0}, "p");
    const AttachmentView v = attachment_view({line});
    CHECK_EQ(av(v, {20.0, 20.0}), true);
}

// ---- _takeoff_children_valid_for_geometry_changes ------------------------------------------------------------------

// Port of ...::HierarchyDragTests::test_overrides_that_touch_no_hierarchy_are_always_valid
TEST(ChildrenValid, OverridesThatTouchNoHierarchyAreAlwaysValid)
{
    Family f = family_ph();
    CHECK_EQ(cv(f, {}), true);
    CHECK_EQ(cv(f, {{"unknown", {0.0, 0.0}}}), true);
    CHECK_EQ(cv(f, {{"free", {5.0, 5.0, 6.0, 6.0}}}), true);
    f.add(T("nocond", "ghost", {1.0, 1.0}));
    CHECK_EQ(cv(f, {{"nocond", {5.0, 5.0}}}), true);
}

// Port of ...::HierarchyDragTests::test_moving_a_hole_keeps_it_inside_the_parent_and_off_its_siblings
TEST(ChildrenValid, MovingAHoleKeepsItInsideTheParentAndOffItsSiblings)
{
    const Family f = family_ph();
    CHECK_EQ(cv(f, {{"h1", square(10, 10, 25, 25)}}), true);
    CHECK_EQ(cv(f, {{"h1", square(45, 45, 65, 65)}}), false);
    CHECK_EQ(cv(f, {{"h1", square(150, 150, 170, 170)}}), false);
    CHECK_EQ(cv(f, {{"h1", square(10, 10, 25, 25)}, {"h2", square(20, 20, 40, 40)}}), false);
}

// Port of ...::HierarchyDragTests::test_resizing_the_parent_checks_holes_and_attachments
TEST(ChildrenValid, ResizingTheParentChecksHolesAndAttachments)
{
    const Family f = family_ph();
    CHECK_EQ(cv(f, {{"p", SQUARE_100}}), true);
    CHECK_EQ(cv(f, {{"p", square(0, 0, 60, 60)}}), false);
    // Holes fit in an 80x80 parent but the attachment at (90, 90) does not.
    CHECK_EQ(cv(f, {{"p", square(0, 0, 80, 80)}}), false);
    CHECK_EQ(cv(f, {{"p", square(0, 0, 80, 80)}, {"a", {60.0, 20.0}}}), true);
}

// Port of ...::HierarchyDragTests::test_moving_an_attachment_must_keep_it_in_the_area_and_out_of_the_holes
TEST(ChildrenValid, MovingAnAttachmentKeepsItInTheAreaAndOutOfTheHoles)
{
    const Family f = family_ph();
    CHECK_EQ(cv(f, {{"a", {90.0, 20.0}}}), true);
    CHECK_EQ(cv(f, {{"a", {150.0, 20.0}}}), false);
    CHECK_EQ(cv(f, {{"a", {60.0, 60.0}}}), false);
}

// Port of ...::HierarchyDragTests::test_attachment_rotation_overrides_are_validated
TEST(ChildrenValid, AttachmentRotationOverridesAreValidated)
{
    Family f = windows_family({T("p", "ca", SQUARE_100), T("a", "cat", {50.0, 3.0}, "p")});
    CHECK_EQ(cv(f, {}, {{"a", 0.0}}), true);
    CHECK_EQ(cv(f, {}, {{"a", kPi / 2}}), false);
    CHECK_EQ(cv(f, {{"a", {50.0, 3.0}}}), true);
    f.at("a").rotation = kPi / 2;
    CHECK_EQ(cv(f, {{"a", {50.0, 3.0}}}, {}), false);
    CHECK_EQ(cv(f, {{"a", {50.0, 3.0}}}, {{"a", 0.0}}), true);
}

// Port of ...::HierarchyDragTests::test_children_with_unknown_conditions_or_inconsistent_parents_are_invalid
TEST(ChildrenValid, ChildrenWithUnknownConditionsOrInconsistentParentsAreInvalid)
{
    Family f = family_ph();
    f.add(T("mystery", "ghost", {1.0, 1.0}, "p"));
    CHECK_EQ(cv(f, {{"p", SQUARE_100}}), false);
    f = family_ph();
    f.at("h2").page_uid = "pg2";
    CHECK_EQ(cv(f, {{"p", SQUARE_100}}), false);
}

// Port of ...::HierarchyDragTests::test_holes_of_a_missing_or_non_area_parent_are_invalid
TEST(ChildrenValid, HolesOfAMissingOrNonAreaParentAreInvalid)
{
    Family f = windows_family({T("h", "ca", square(1, 1, 2, 2), "ghost")});
    CHECK_EQ(cv(f, {{"h", square(1, 1, 3, 3)}}), false);
    f = windows_family({T("p", "cl", {0.0, 0.0, 9.0, 9.0}), T("h", "ca", square(1, 1, 2, 2), "p")});
    CHECK_EQ(cv(f, {{"h", square(1, 1, 3, 3)}}), false);
}

// Port of ...::HierarchyDragTests::test_children_that_are_neither_areas_nor_attachments_are_not_checked
TEST(ChildrenValid, ChildrenThatAreNeitherAreasNorAttachmentsAreNotChecked)
{
    Family f = family_ph();
    f.add(T("line", "cl", {500.0, 500.0, 600.0, 600.0}, "p"));
    CHECK_EQ(cv(f, {{"p", SQUARE_100}}), true);
}

// Windows code (read-only run, reviewer): an attachment whose parent_uid is a root marker ("0" or "") is no hole, so
// overriding its position or rotation affects no parent and is always valid (it must not be looked up as a child of a
// parent named "0").
TEST(ChildrenValid, ARootLevelAttachmentOverrideTouchesNoHierarchy)
{
    Family f = windows_family({T("a", "cat", {1.0, 1.0}, "0"), T("b", "cat", {2.0, 2.0}, "")});
    CHECK_EQ(cv(f, {{"a", {5.0, 5.0}}}), true);
    CHECK_EQ(cv(f, {}, {{"a", 1.0}}), true);
    CHECK_EQ(cv(f, {{"b", {5.0, 5.0}}}), true);
}

// Port of ...::HierarchyDragGapTests::test_children_of_a_non_area_parent_are_invalid_even_when_geometrically_inside
TEST(ChildrenValid, ChildrenOfANonAreaParentAreInvalidEvenWhenInside)
{
    Family f = windows_family({T("p", "cl", SQUARE_100), T("h", "ca", square(10, 10, 20, 20), "p")});
    CHECK_EQ(cv(f, {{"h", square(10, 10, 25, 25)}}), false);
}

// Port of ...::HierarchyDragGapTests::test_children_of_a_parent_without_a_known_condition_are_invalid
TEST(ChildrenValid, ChildrenOfAParentWithoutAKnownConditionAreInvalid)
{
    Family f = windows_family({T("p", "ghost", SQUARE_100), T("h", "ca", square(10, 10, 20, 20), "p")});
    CHECK_EQ(cv(f, {{"h", square(10, 10, 25, 25)}}), false);
}

// Port of ...::HierarchyDragGapTests::test_a_child_whose_parent_is_on_another_page_is_invalid
TEST(ChildrenValid, AChildWhoseParentIsOnAnotherPageIsInvalid)
{
    Family f = windows_family({T("p", "ca", SQUARE_100, "0", "pg2"), T("h", "ca", square(10, 10, 20, 20), "p")});
    CHECK_EQ(cv(f, {{"h", square(10, 10, 25, 25)}}), false);
}

// Port of ...::HierarchyDragGapTests::test_parent_and_page_checks_are_independent_of_the_override_order
TEST(ChildrenValid, ResultIsIndependentOfTheOverrideOrder)
{
    const Family f = windows_family({T("p", "ca", SQUARE_100), T("h", "ca", square(10, 10, 20, 20), "p"),
                                     T("h2", "ca", square(30, 30, 40, 40), "p")});
    hier::PositionOverrides first;
    first["h"] = square(10, 10, 25, 25);
    first["h2"] = square(30, 30, 45, 45);
    hier::PositionOverrides second;
    second["h2"] = square(30, 30, 45, 45);
    second["h"] = square(10, 10, 25, 25);
    CHECK_EQ(cv(f, first), true);
    CHECK_EQ(cv(f, second), true);
}

// Port of tests/presentation/components/plan_view/components/test_drag_handler.py::HoleOutlineAndFamilySweepTests
//   ::test_overriding_a_plain_child_does_not_validate_its_parents_other_children
TEST(ChildrenValid, OverridingAPlainChildDoesNotValidateItsParentsOtherChildren)
{
    Family f = windows_family({T("p", "ca", SQUARE_100), T("bad", "ca", square(200, 200, 210, 210), "p"),
                               T("line", "cl", {1.0, 1.0, 5.0, 5.0}, "p")});
    CHECK_EQ(cv(f, {{"line", {2.0, 2.0, 6.0, 6.0}}}), true);
    // Control: the same stored hole does veto an override of a sibling hole.
    f.add(T("good", "ca", square(10, 10, 20, 20), "p"));
    CHECK_EQ(cv(f, {{"good", square(10, 10, 25, 25)}}), false);
}

// ---- _validate_parent_contains_holes --------------------------------------------------------------------------------

// Port of ...::HierarchyDragTests::test_parent_must_contain_every_hole_and_attachment
TEST(ParentContainsHoles, ParentMustContainEveryHoleAndAttachment)
{
    const Family f = family_ph();
    CHECK_EQ(pc(f, "p", SQUARE_100), true);
    CHECK_EQ(pc(f, "p", square(0, 0, 60, 60)), false);
    CHECK_EQ(pc(f, "p", square(0, 0, 80, 80)), false);
    CHECK_EQ(pc(f, "p", square(0, 0, 500, 500)), true);
}

// Port of ...::HierarchyDragTests::test_parent_validation_ignores_unrelated_unknown_and_short_children
TEST(ParentContainsHoles, IgnoresUnrelatedUnknownAndShortChildren)
{
    Family f = family_ph();
    f.add(T("unknown", "ghost", square(500, 500, 600, 600), "p"));
    f.add(T("short", "ca", {500.0, 500.0, 1.0, 1.0}, "p"));
    f.add(T("line", "cl", {500.0, 500.0, 600.0, 600.0}, "p"));
    f.add(T("foreign", "ca", square(500, 500, 600, 600), "q"));
    CHECK_EQ(pc(f, "p", SQUARE_100), true);
}

// Port of ...::HierarchyDragTests::test_parent_validation_attachment_check_uses_the_new_parent_shape
TEST(ParentContainsHoles, AttachmentCheckUsesTheNewParentShape)
{
    const Family f = windows_family({T("p", "ca", SQUARE_100), T("a", "cat", {90.0, 90.0}, "p")});
    CHECK_EQ(pc(f, "p", square(0, 0, 96, 96)), true);
    CHECK_EQ(pc(f, "p", square(0, 0, 80, 80)), false);
}

// Port of ...::HierarchyDragGapTests::test_parent_validation_checks_triangular_holes_and_ignores_other_shapes
TEST(ParentContainsHoles, ChecksTriangularHolesAndIgnoresOtherShapes)
{
    Family f = windows_family({T("p", "ca", SQUARE_100), T("tri", "ca", {200.0, 200.0, 210.0, 200.0, 200.0, 210.0}, "p")});
    CHECK_EQ(pc(f, "p", SQUARE_100), false);
    f = windows_family({T("p", "ca", SQUARE_100),
                        T("line", "cl", {200.0, 200.0, 210.0, 200.0, 200.0, 210.0}, "p"),
                        T("bad", "ca", {200.0, 200.0, 210.0, 200.0, 205.0}, "p")});
    CHECK_EQ(pc(f, "p", SQUARE_100), true);
}

// Windows code (read-only run): an area child whose position has an odd number of values >= 7 is no polygon (empty
// path), so it is "not inside" and the parent edit is rejected; one with fewer than 6 values is skipped.
TEST(ParentContainsHoles, ChildWithAnOddPositionLengthOfSevenOrMoreRejectsTheEdit)
{
    Family f = windows_family({T("p", "ca", SQUARE_100), T("odd", "ca", {10.0, 10.0, 20.0, 10.0, 20.0, 20.0, 10.0}, "p")});
    CHECK_EQ(pc(f, "p", SQUARE_100), false);
    f = windows_family({T("p", "ca", SQUARE_100), T("five", "ca", {10.0, 10.0, 20.0, 10.0, 20.0}, "p")});
    CHECK_EQ(pc(f, "p", SQUARE_100), true);
}

// Port of tests/presentation/components/plan_view/components/test_drag_handler.py::AttachmentMovementTests
//   ::test_parent_resize_cannot_exclude_attachment (the default attachment condition is a 1 x 1 square)
TEST(ParentContainsHoles, ParentResizeCannotExcludeAttachment)
{
    Family f;
    f.add_condition(make_condition("area", ConditionType::Area));
    f.add_condition(make_condition("attachment", ConditionType::Attachment));
    f.add(T("parent", "area", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, "0", "page"));
    f.add(T("attachment", "attachment", {5.0, 5.0}, "parent", "page"));
    CHECK_FALSE(pc(f, "parent", {0.0, 0.0, 3.0, 0.0, 3.0, 3.0, 0.0, 3.0}));
    CHECK(pc(f, "parent", {0.0, 0.0, 8.0, 0.0, 8.0, 8.0, 0.0, 8.0}));
    CHECK(pc(f, "parent", {0.0, 0.0, 12.0, 0.0, 12.0, 12.0, 0.0, 12.0}));
}

// Port of ...::AttachmentMovementTests::test_nested_backout_vertex_edit_cannot_exclude_its_child: the vertex drag of a
// hole runs validate_hole_position and then validate_parent_contains_holes (drag_handler.update_drag_handle_positions).
TEST(ParentContainsHoles, NestedBackoutVertexEditCannotExcludeItsChild)
{
    Family f;
    f.add_condition(make_condition("area", ConditionType::Area));
    f.add(T("parent", "area", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, "0", "page"));
    f.add(T("backout", "area", {1.0, 1.0, 9.0, 1.0, 9.0, 9.0, 1.0, 9.0}, "parent", "page"));
    f.add(T("nested", "area", {7.0, 7.0, 8.0, 7.0, 8.0, 8.0, 7.0, 8.0}, "backout", "page"));
    const Takeoff backout = f.at("backout");
    auto drag_valid = [&](const std::vector<double> &pos)
    { return hp(f, backout, pos) && pc(f, "backout", pos); };
    CHECK_FALSE(drag_valid({1.0, 1.0, 9.0, 1.0, 5.0, 5.0, 1.0, 9.0}));
    CHECK(drag_valid({1.0, 1.0, 9.0, 1.0, 8.5, 8.5, 1.0, 9.0}));
    CHECK(drag_valid({1.0, 1.0, 9.0, 1.0, 9.0, 9.0, 1.0, 9.0}));
}

// Port of tests/integration/geometry/test_attachment_validation.py::AttachmentMovementTests
//   ::test_legacy_curved_linear_sibling_is_not_a_polygon_backout (validate_hole_position part; the placement-mode
//   _check_hole_overlap part is not ported, iOS cannot place holes)
TEST(HoleValidation, LegacyCurvedLinearSiblingIsNotAPolygonBackout)
{
    Family f;
    f.add_condition(make_condition("area", ConditionType::Area));
    f.add_condition(make_condition("attachment", ConditionType::Attachment));
    f.add_condition(make_condition("linear", ConditionType::Linear));
    f.add(T("parent", "area", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, "0", "page"));
    f.add(T("backout", "area", {5.0, 5.0, 6.0, 5.0, 6.0, 6.0, 5.0, 6.0}, "parent", "page"));
    Takeoff linear = T("linear", "linear", {3.0, 3.0, 9.0, 3.0, 6.0, 9.0}, "parent", "page");
    linear.curve = 0;
    f.add(linear);
    const Takeoff backout = f.at("backout");
    CHECK(hp(f, backout, backout.position));
    // Positive control: the very same sibling geometry, once its condition is an Area, does overlap the backout.
    f.conditions["linear"].condition_type = ConditionType::Area;
    CHECK_FALSE(hp(f, backout, backout.position));
}

// ---- _validate_hole_position ----------------------------------------------------------------------------------------

namespace
{
    Family hole_family()
    {
        return windows_family({
            T("p", "ca", SQUARE_100),
            T("h", "ca", square(10, 10, 20, 20), "p"),
            T("sib", "ca", square(50, 50, 70, 70), "p"),
            T("a", "cat", {90.0, 90.0}, "p"),
        });
    }
} // namespace

// Port of ...::HierarchyDragTests::test_hole_position_must_be_inside_the_parent
TEST(HoleValidation, PositionMustBeInsideTheParent)
{
    Family f = hole_family();
    const Takeoff hole = f.at("h");
    CHECK_EQ(hp(f, hole, square(10, 10, 30, 30)), true);
    CHECK_EQ(hp(f, hole, square(90, 90, 120, 120)), false);
}

// Port of ...::HierarchyDragTests::test_hole_without_a_usable_parent_is_invalid
TEST(HoleValidation, HoleWithoutAUsableParentIsInvalid)
{
    Family f = hole_family();
    const Takeoff hole = f.at("h");
    const Takeoff orphan = T("o", "ca", square(1, 1, 2, 2), "ghost");
    CHECK_EQ(hp(f, orphan, square(1, 1, 2, 2)), false);
    f.at("p").position = {0.0, 0.0, 10.0, 10.0};
    CHECK_EQ(hp(f, hole, square(1, 1, 2, 2)), false);
    f.at("p").position = {};
    CHECK_EQ(hp(f, hole, square(1, 1, 2, 2)), false);
}

// Port of ...::HierarchyDragTests::test_hole_position_must_not_overlap_sibling_holes_or_attachments
TEST(HoleValidation, MustNotOverlapSiblingHolesOrAttachments)
{
    Family f = hole_family();
    const Takeoff hole = f.at("h");
    CHECK_EQ(hp(f, hole, square(40, 40, 60, 60)), false);
    CHECK_EQ(hp(f, hole, square(80, 80, 98, 98)), false);
    CHECK_EQ(hp(f, hole, square(10, 10, 20, 20)), true);
}

// Port of ...::HierarchyDragTests::test_hole_validation_ignores_unrelated_children
TEST(HoleValidation, IgnoresUnrelatedChildren)
{
    Family f = hole_family();
    const Takeoff hole = f.at("h");
    f.add(T("foreign", "ca", square(30, 30, 40, 40), "q"));
    f.add(T("ghost", "nope", square(30, 30, 40, 40), "p"));
    f.add(T("line", "cl", {30.0, 30.0, 40.0, 40.0}, "p"));
    f.add(T("short", "ca", {30.0, 30.0}, "p"));
    f.add(T("foreign_attachment", "cat", {35.0, 35.0}, "q"));
    CHECK_EQ(hp(f, hole, square(25, 25, 45, 45)), true);
}

// Port of ...::HierarchyDragTests::test_hole_validation_moves_its_own_old_position_out_of_the_way
TEST(HoleValidation, MovesItsOwnOldPositionOutOfTheWay)
{
    Family f = hole_family();
    const Takeoff hole = f.at("h");
    CHECK_EQ(hp(f, hole, square(12, 12, 22, 22)), true);
}

// Port of ...::HierarchyDragGapTests::test_hole_validation_accepts_a_triangular_parent_and_checks_triangular_siblings
TEST(HoleValidation, AcceptsATriangularParentAndChecksTriangularSiblings)
{
    Family f = windows_family({T("p", "ca", {0.0, 0.0, 50.0, 0.0, 0.0, 50.0}),
                               T("h", "ca", {2.0, 2.0, 6.0, 2.0, 2.0, 6.0}, "p"),
                               T("s", "ca", {10.0, 10.0, 20.0, 10.0, 10.0, 20.0}, "p"),
                               T("l", "cl", {4.0, 4.0, 9.0, 4.0, 4.0, 9.0}, "p")});
    const Takeoff hole = f.at("h");
    CHECK_EQ(hp(f, hole, {2.0, 2.0, 8.0, 2.0, 2.0, 8.0}), true);
    CHECK_EQ(hp(f, hole, {12.0, 12.0, 16.0, 12.0, 12.0, 16.0}), false);
}

// Port of ...::HierarchyDragGapTests::test_hole_validation_rejects_a_five_value_parent
TEST(HoleValidation, RejectsAFiveValueParent)
{
    Family f = windows_family({T("p", "ca", {0.0, 0.0, 50.0, 0.0, 0.0}), T("h", "ca", {2.0, 2.0, 6.0, 2.0, 2.0, 6.0}, "p")});
    const Takeoff hole = f.at("h");
    CHECK_EQ(hp(f, hole, {2.0, 2.0, 6.0, 2.0, 2.0, 6.0}), false);
}

// ---- the item's case (GEO-M02 test plan), checked against the Windows methods ----------------------------------------

// Parent square [0,0,10,0,10,10,0,10] with hole [2,2,4,2,4,4,2,4]: dragging the parent corner 2 (10,10) to (3,3) leaves the
// hole's corner (4,4) outside and is rejected; dragging it to (8,8) is accepted (Windows
// DragHandlerMixin._validate_parent_contains_holes on the real code: False / True).
TEST(ParentContainsHoles, ParentCornerDragCannotCutThroughAHole)
{
    Family f;
    f.add_condition(make_condition("ca", ConditionType::Area));
    f.add(T("p", "ca", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}));
    f.add(T("h", "ca", {2.0, 2.0, 4.0, 2.0, 4.0, 4.0, 2.0, 4.0}, "p"));
    CHECK_FALSE(pc(f, "p", {0.0, 0.0, 10.0, 0.0, 3.0, 3.0, 0.0, 10.0}));
    CHECK(pc(f, "p", {0.0, 0.0, 10.0, 0.0, 8.0, 8.0, 0.0, 10.0}));
    CHECK(pc(f, "p", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}));
    // the same through the hierarchy check used by move / rotate / flip
    CHECK_FALSE(cv(f, {{"p", {0.0, 0.0, 10.0, 0.0, 3.0, 3.0, 0.0, 10.0}}}));
    CHECK(cv(f, {{"p", {0.0, 0.0, 10.0, 0.0, 8.0, 8.0, 0.0, 10.0}}}));
}

// ---- generated oracle: the real Windows methods on random families --------------------------------------------------

namespace
{
    struct OracleFamily
    {
        int id = -1;
        Family family;
        std::vector<std::vector<std::string>> ops;
    };

    size_t run_family(OracleFamily &of, size_t &counts_true)
    {
        size_t checked = 0;
        for (const auto &f : of.ops)
        {
            SCOPED_CONTEXT("family " + std::to_string(of.id) + " op: " + f[0] + " " + f[1]);
            const auto model = of.family.model();
            bool got = false;
            bool expected = false;
            if (f[0] == "pc")
            {
                got = hier::validate_parent_contains_holes(model, f[1], parse_numbers(f[2]));
                expected = f[3] == "1";
            }
            else if (f[0] == "hp")
            {
                const Takeoff *hole = model.find_takeoff(f[1]);
                REQUIRE(hole != nullptr);
                got = hier::validate_hole_position(model, *hole, parse_numbers(f[2]));
                expected = f[3] == "1";
            }
            else if (f[0] == "av")
            {
                const Takeoff *a = model.find_takeoff(f[1]);
                REQUIRE(a != nullptr);
                const std::vector<double> position = parse_numbers(f[2]);
                std::vector<double> parent_position;
                const std::vector<double> *pp = nullptr;
                if (f[3] != "-")
                {
                    parent_position = parse_numbers(f[3]);
                    pp = &parent_position;
                }
                hier::PositionOverrides tp;
                const hier::PositionOverrides *tpp = nullptr;
                if (f[4] != "-")
                {
                    tp = parse_position_map(f[4]);
                    tpp = &tp;
                }
                std::optional<double> rotation;
                if (f[5] != "-")
                    rotation = std::strtod(f[5].c_str(), nullptr);
                got = hier::attachment_position_valid(model, *a, position, pp, tpp, rotation);
                expected = f[6] == "1";
            }
            else if (f[0] == "cv")
            {
                got = hier::takeoff_children_valid_for_geometry_changes(model, parse_position_map(f[1]),
                                                                        parse_number_map(f[2]));
                expected = f[3] == "1";
            }
            else
            {
                FAIL_TEST("unknown op " + f[0]);
            }
            CHECK_EQ(got, expected);
            counts_true += expected;
            ++checked;
        }
        return checked;
    }
} // namespace

TEST(HierarchyOracle, RandomFamiliesMatchTheWindowsMethods)
{
    const size_t count = sizeof(kHierarchyOracle) / sizeof(kHierarchyOracle[0]);
    OracleFamily cur;
    size_t checked = 0, families = 0, true_count = 0;
    auto flush = [&]()
    {
        if (cur.id >= 0)
        {
            checked += run_family(cur, true_count);
            ++families;
        }
        cur = OracleFamily();
    };
    for (size_t i = 0; i < count; ++i)
    {
        const std::vector<std::string> f = split_fields(kHierarchyOracle[i]);
        if (f[0] == "F")
        {
            flush();
            cur.id = std::atoi(f[1].c_str());
        }
        else if (f[0] == "C")
        {
            Condition c = make_condition(f[1], static_cast<ConditionType>(std::atoi(f[2].c_str())),
                                         static_cast<ShapeId>(std::atoi(f[3].c_str())),
                                         std::strtod(f[4].c_str(), nullptr), std::strtod(f[5].c_str(), nullptr),
                                         std::strtod(f[6].c_str(), nullptr));
            cur.family.add_condition(c);
        }
        else if (f[0] == "T")
        {
            cur.family.add(make_takeoff(f[1], f[2], parse_numbers(f[6]), f[3], f[4], std::strtod(f[5].c_str(), nullptr)));
        }
        else
        {
            cur.ops.push_back(f);
        }
    }
    flush();
    CHECK_EQ(families, size_t(140));
    CHECK(checked > 800);
    // both outcomes are well represented
    CHECK(true_count > 250 && true_count < checked - 250);
}
