// Page-level quantity aggregation: ost_domain::compute_page_quantities.
//
// Windows oracle: ost_visualizer/domain/services/condition_quantity_service.py::compute_page_quantities
// and tests/domain/services/test_condition_quantity_service.py (ported test by test below).
//
// Items: QTY-01 (non-area takeoffs with a parent_uid), QTY-02 (attachment footprint/perimeter),
// QTY-03 (child filtering by condition type and page), QTY-04 (rounding forwarded from the
// condition, applied per takeoff before the sign), QTY-17 (unsupported condition types),
// QTY-18 (the literal parent token 'None' is a root parent in the page quantities, as '' and '0'
// are; user decision: only compute_page_quantities uses this 3-token set, Takeoff::is_hole()
// keeps its 2-token rule).
//
// Not ported on purpose:
//  - tests that need only_condition_uids / compute_takeoff_cubic_yards / log capture (no iOS API).
#include "QtyHelpers.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace qty;

namespace
{
    using PageResult = std::unordered_map<std::string, QuantityResult>;

    QuantityResult at(const PageResult &r, const std::string &uid)
    {
        auto it = r.find(uid);
        if (it == r.end())
            FAIL_TEST("no result for condition '" + uid + "'");
        return it->second;
    }

    // setUp of TakeoffLifecycleQuantityTests
    struct Lifecycle
    {
        CondMap conditions;
        std::vector<Takeoff> takeoffs;
    };

    Lifecycle lifecycle()
    {
        Lifecycle l;
        l.conditions["area"] = make_condition("area", ConditionType::Area, 11, UOM_SQUARE_INCHES);
        l.conditions["backout"] = make_condition("backout", ConditionType::Area, 11, UOM_SQUARE_INCHES);
        Condition att = make_condition("attachment", ConditionType::Attachment, CALC_COUNT, 0);
        att.width = 2;
        att.depth = 2;
        l.conditions["attachment"] = att;
        l.takeoffs.push_back(make_takeoff("parent", "area", {0, 0, 10, 0, 10, 10, 0, 10}, "page"));
        l.takeoffs.push_back(make_takeoff("hole", "backout", {1, 1, 3, 1, 3, 3, 1, 3}, "page", "parent"));
        l.takeoffs.push_back(make_takeoff("point", "attachment", {5, 5}, "page", "parent"));
        return l;
    }

    // ConditionQuantityServiceExactTests.area_condition
    Condition exact_area_condition(const std::string &uid = "a")
    {
        Condition c = make_condition(uid, ConditionType::Area, 11, UOM_SQUARE_INCHES);
        c.calc_type2 = 11;
        c.uom2 = UOM_SQUARE_INCHES;
        c.calc_type3 = CALC_COUNT;
        c.uom3 = 0;
        return c;
    }
} // namespace

// ============================================================================================
// QTY-01: only AREA takeoffs with a parent are holes; every other type counts every takeoff.
// ============================================================================================

// Port of tests/domain/services/test_condition_quantity_service.py::TakeoffLifecycleQuantityTests::test_attachment_contributes_its_own_quantity
TEST(Qty01_NonAreaParents, AttachmentContributesItsOwnQuantity)
{
    Lifecycle l = lifecycle();
    PageResult r = compute_page_quantities(l.conditions, l.takeoffs);
    CHECK_EQ(at(r, "attachment").q1, 1.0);
}

// Port of tests/domain/services/test_condition_quantity_service.py::TakeoffLifecycleQuantityTests::test_scoped_area_quantities_include_backout_on_another_condition
// (full-page request; the only_condition_uids filter does not exist on iOS) plus the
// 'backout (0,0,0)' result of the QTY-01 test plan.
TEST(Qty01_NonAreaParents, LifecycleAreaBackoutAndAttachmentTotals)
{
    Lifecycle l = lifecycle();
    PageResult r = compute_page_quantities(l.conditions, l.takeoffs);
    CHECK_EQ(at(r, "area").q1, 96.0);
    CHECK_EQ(at(r, "area").q2, 0.0);
    CHECK_EQ(at(r, "area").q3, 0.0);
    CHECK_EQ(at(r, "backout").q1, 0.0);
    CHECK_EQ(at(r, "backout").q2, 0.0);
    CHECK_EQ(at(r, "backout").q3, 0.0);
    CHECK_EQ(r.size(), static_cast<size_t>(3));
}

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_non_area_conditions_count_every_takeoff_even_with_a_parent_uid
TEST(Qty01_NonAreaParents, NonAreaConditionsCountEveryTakeoffEvenWithAParentUid)
{
    CondMap conditions = cond_map({make_condition("l", ConditionType::Linear, 1, 1)});
    std::vector<Takeoff> takeoffs = {
        make_takeoff("1", "l", {0, 0, 5, 0}),
        make_takeoff("2", "l", {0, 0, 7, 0}, "", "1"),
    };
    PageResult r = compute_page_quantities(conditions, takeoffs);
    CHECK_EQ(at(r, "l").q1, 12.0);
}

// Windows compute_page_quantities run on the COND-M01 data of the divergence record:
// area takeoff 1 (120 x 120 on page p), children with parent 1: Attachment takeoff 2 (w=12 d=12),
// Count takeoff 3; a Linear condition (calc 1, uom 1) with takeoff 4 (0,0,5,0) and takeoff 5
// (parent 4, 0,0,7,0).  Windows: attachment 1.0, count 1.0, linear 12.0, area calc 11 -> 100.0 SF,
// area calc 12 -> 99.0 SF.
TEST(Qty01_NonAreaParents, ImportedAttachmentCountAndLinearOnAnAreaScenario)
{
    const double windows_area[2] = {100.0, 99.0}; // calc 11, calc 12
    const int calcs[2] = {11, 12};
    for (int k = 0; k < 2; ++k)
    {
        SCOPED_CONTEXT("area calc " + std::to_string(calcs[k]));
        Condition att = make_condition("att", ConditionType::Attachment, 23, 0);
        att.width = 12;
        att.depth = 12;
        CondMap conditions = cond_map({
            make_condition("area", ConditionType::Area, calcs[k], UOM_SQUARE_FEET),
            att,
            make_condition("cnt", ConditionType::Count, 23, 0),
            make_condition("lin", ConditionType::Linear, 1, 1),
        });
        std::vector<Takeoff> takeoffs = {
            make_takeoff("1", "area", {0, 0, 120, 0, 120, 120, 0, 120}, "p"),
            make_takeoff("2", "att", {50, 50}, "p", "1"),
            make_takeoff("3", "cnt", {60, 60}, "p", "1"),
            make_takeoff("4", "lin", {0, 0, 5, 0}, "p"),
            make_takeoff("5", "lin", {0, 0, 7, 0}, "p", "4"),
        };
        PageResult r = compute_page_quantities(conditions, takeoffs);
        CHECK_EQ(at(r, "att").q1, 1.0);
        CHECK_EQ(at(r, "cnt").q1, 1.0);
        CHECK_EQ(at(r, "lin").q1, 12.0);
        CHECK_EQ(at(r, "area").q1, windows_area[k]);
    }
}

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_backouts_are_subtracted_only_from_their_parent_and_never_counted_as_primaries
// (QTY-18: the 'none-parent' takeoff, parent token 'None', is a root like the blank parent).
TEST(Qty01_NonAreaParents, BackoutsAreSubtractedOnlyFromTheirParentAndNeverCountedAsPrimaries)
{
    CondMap conditions = cond_map({exact_area_condition()});
    std::vector<Takeoff> takeoffs = {
        make_takeoff("p1", "a", {0, 0, 10, 0, 10, 10, 0, 10}, "pg"),
        make_takeoff("p2", "a", {0, 0, 10, 0, 10, 10, 0, 10}, "pg"),
        make_takeoff("h", "a", {0, 0, 2, 0, 2, 2, 0, 2}, "pg", "p1"),
        make_takeoff("none-parent", "a", {0, 0, 1, 0, 1, 1, 0, 1}, "pg", "None"),
        make_takeoff("blank-parent", "a", {0, 0, 1, 0, 1, 1, 0, 1}, "pg", ""),
    };
    // p1 (100 - 4) + p2 (100) + two root-like parents of 1 each.
    CHECK_EQ(at(compute_page_quantities(conditions, takeoffs), "a").q1, 96.0 + 100.0 + 1.0 + 1.0);
}

// ============================================================================================
// QTY-02: attachment footprint and perimeter of the CHILD'S condition feed calc 12, 22 and 33.
// ============================================================================================

// Port of tests/domain/services/test_condition_quantity_service.py::TakeoffLifecycleQuantityTests::test_attachment_aware_area_formula_uses_child_dimensions
TEST(Qty02_AttachmentAggregation, AttachmentAwareAreaFormulaUsesChildDimensions)
{
    Lifecycle l = lifecycle();
    l.conditions["area"].calc_type1 = 12;
    CHECK_EQ(at(compute_page_quantities(l.conditions, l.takeoffs), "area").q1, 92.0);
}

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_attachment_children_subtract_footprint_and_add_perimeter_to_the_parent
TEST(Qty02_AttachmentAggregation, AttachmentChildrenSubtractFootprintAndAddPerimeterToTheParent)
{
    Condition attachment = make_condition("att", ConditionType::Attachment, CALC_COUNT, 0);
    attachment.width = 2;
    attachment.depth = 3;
    struct Case
    {
        int calc;
        double expected;
    };
    const Case cases[] = {{12, 100.0 - 6.0}, {33, 40.0 + 10.0}, {22, 94.0}};
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT("calc_type " + std::to_string(c.calc));
        Condition area = exact_area_condition();
        area.calc_type1 = c.calc;
        area.uom1 = UOM_SQUARE_INCHES;
        area.thickness = 1.0;
        std::vector<Takeoff> takeoffs = {
            make_takeoff("p", "a", {0, 0, 10, 0, 10, 10, 0, 10}, "pg"),
            make_takeoff("c", "att", {5, 5}, "pg", "p"),
        };
        PageResult r = compute_page_quantities(cond_map({area, attachment}), takeoffs);
        CHECK_EQ(at(r, "a").q1, c.expected);
        // The attachment condition's own total is one per takeoff (QTY-01).
        CHECK_EQ(at(r, "att").q1, 1.0);
    }
}

// Port of tests/domain/services/test_condition_quantity_service.py::ChildKindAndRoundingBoundaryTests::test_only_attachment_children_contribute_a_footprint_to_the_parent
TEST(Qty02_AttachmentAggregation, OnlyAttachmentChildrenContributeAFootprintToTheParent)
{
    struct Case
    {
        ConditionType type;
        double expected;
    };
    const Case cases[] = {
        {ConditionType::Attachment, 100.0 - 6.0},
        {ConditionType::Linear, 100.0},
        {ConditionType::Count, 100.0},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT("child_type " + std::to_string(static_cast<int>(c.type)));
        Condition area = make_condition("a", ConditionType::Area, 12, UOM_SQUARE_INCHES);
        Condition child_condition = make_condition("k", c.type, CALC_COUNT, 0);
        child_condition.width = 2;
        child_condition.depth = 3;
        std::vector<Takeoff> takeoffs = {
            make_takeoff("p", "a", SQUARE_10, "pg"),
            make_takeoff("c", "k", {5, 5}, "pg", "p"),
        };
        PageResult r = compute_page_quantities(cond_map({area, child_condition}), takeoffs);
        QuantityResult a = at(r, "a");
        CHECK_EQ(a.q1, c.expected);
        CHECK_EQ(a.q2, 0.0);
        CHECK_EQ(a.q3, 0.0);
    }
}

// Windows compute_page_quantities: one area parent (100 x 60 in, uom LF/SF/CF) with attachment
// children of two attachment conditions (2 x 3, 1.5 x 4, 2 x 3 again) and one area hole 20 x 10.
// Calc 33 (perimeter + hole perimeter + attachment perimeters, in LF) = 411 in = 34.25,
// calc 12 (area - hole - footprints, in SF) and calc 22 (the same times thickness 3, in CF).
TEST(Qty02_AttachmentAggregation, SeveralAttachmentsAndAHoleAccumulateInChildOrder)
{
    Condition area = make_condition("a", ConditionType::Area, 33, UOM_LINEAR_FEET);
    area.calc_type2 = 12;
    area.uom2 = UOM_SQUARE_FEET;
    area.calc_type3 = 22;
    area.uom3 = UOM_CUBIC_FEET;
    area.thickness = 3.0;
    Condition att = make_condition("att", ConditionType::Attachment, CALC_COUNT, 0);
    att.width = 2;
    att.depth = 3;
    Condition att2 = make_condition("att2", ConditionType::Attachment, CALC_COUNT, 0);
    att2.width = 1.5;
    att2.depth = 4;
    Condition hole = make_condition("h", ConditionType::Area, 11, UOM_SQUARE_INCHES);
    std::vector<Takeoff> takeoffs = {
        make_takeoff("p", "a", {0, 0, 100, 0, 100, 60, 0, 60}, "pg"),
        make_takeoff("c1", "att", {5, 5}, "pg", "p"),
        make_takeoff("c2", "att2", {8, 8}, "pg", "p"),
        make_takeoff("c3", "att", {9, 9}, "pg", "p"),
        make_takeoff("h1", "h", {10, 10, 30, 10, 30, 20, 10, 20}, "pg", "p"),
    };
    PageResult r = compute_page_quantities(cond_map({area, att, att2, hole}), takeoffs);
    QuantityResult a = at(r, "a");
    CHECK_EQ(a.q1, 34.25);
    CHECK_NEAR(a.q2, 40.15277777777778, 40.15277777777778 * 1e-12);
    CHECK_NEAR(a.q3, 10.038194444444445, 10.038194444444445 * 1e-12);
    CHECK_EQ(at(r, "att").q1, 2.0);
    CHECK_EQ(at(r, "att2").q1, 1.0);
    CHECK_EQ(at(r, "h").q1, 0.0);
}

// ============================================================================================
// QTY-03: children are used only on the same page and only as area holes / attachments.
// ============================================================================================

// Port of tests/domain/services/test_condition_quantity_service.py::TakeoffLifecycleQuantityTests::test_foreign_page_and_missing_condition_children_do_not_subtract_area
TEST(Qty03_ChildFiltering, ForeignPageAndMissingConditionChildrenDoNotSubtractArea)
{
    Lifecycle l = lifecycle();
    Takeoff parent = l.takeoffs[0], hole = l.takeoffs[1], attachment = l.takeoffs[2];
    Takeoff foreign_hole = hole;
    foreign_hole.page_uid = "other-page";
    Takeoff orphan_attachment = attachment;
    orphan_attachment.condition_uid = "deleted"; // not in the condition map
    l.conditions["area"].calc_type1 = 12;
    CHECK_EQ(at(compute_page_quantities(l.conditions, {parent, foreign_hole, orphan_attachment}), "area").q1,
             100.0);
    CHECK_EQ(at(compute_page_quantities(l.conditions, l.takeoffs), "area").q1, 92.0);
}

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_a_child_on_a_different_page_is_ignored_whichever_page_uid_sorts_first
TEST(Qty03_ChildFiltering, AChildOnADifferentPageIsIgnoredWhicheverPageUidSortsFirst)
{
    Condition condition = exact_area_condition();
    condition.uom1 = UOM_SQUARE_FEET;
    condition.uom2 = UOM_SQUARE_FEET;
    Takeoff parent = make_takeoff("p", "a", {0, 0, 12, 0, 12, 12, 0, 12}, "m-page");
    for (const char *page_uid : {"a-page", "z-page"})
    {
        SCOPED_CONTEXT(std::string("child_page ") + page_uid);
        Takeoff child = make_takeoff("c", "a", {0, 0, 6, 0, 6, 6, 0, 6}, page_uid, "p");
        PageResult r = compute_page_quantities(cond_map({condition}), {parent, child});
        CHECK_NEAR(at(r, "a").q1, 1.0, TOL7);
    }
    Takeoff same_page = make_takeoff("c", "a", {0, 0, 6, 0, 6, 6, 0, 6}, "m-page", "p");
    PageResult r = compute_page_quantities(cond_map({condition}), {parent, same_page});
    CHECK_NEAR(at(r, "a").q1, 0.75, TOL7);
}

// Port of tests/domain/services/test_condition_quantity_service.py::PageUidComparisonTests
TEST(Qty03_ChildFiltering, PageUidsMatchByEqualityNotByPrefixOrEmptiness)
{
    auto net = [](const std::string &parent_page, const std::string &child_page)
    {
        Condition c = make_condition("c", ConditionType::Area, 11, UOM_SQUARE_INCHES);
        Takeoff parent = make_takeoff("p", "c", {0, 0, 10, 0, 10, 10, 0, 10}, parent_page);
        Takeoff hole = make_takeoff("h", "c", {0, 0, 2, 0, 2, 2, 0, 2}, child_page, "p");
        return at(compute_page_quantities(cond_map({c}), {parent, hole}), "c").q1;
    };
    CHECK_EQ(net(std::string("pa") + "ge", std::string("pa") + "ge"), 96.0);
    for (const char *other : {"page-1", "", "page-100", "PAGE-10"})
    {
        SCOPED_CONTEXT(std::string("child_page '") + other + "'");
        CHECK_EQ(net("page-10", other), 100.0);
    }
    CHECK_EQ(net("page-10", "page-10"), 96.0);
}

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_a_child_that_is_neither_area_nor_attachment_does_not_affect_the_parent
TEST(Qty03_ChildFiltering, AChildThatIsNeitherAreaNorAttachmentDoesNotAffectTheParent)
{
    Condition line = make_condition("l", ConditionType::Linear, 1, 1);
    Takeoff parent = make_takeoff("p", "a", {0, 0, 10, 0, 10, 10, 0, 10}, "pg");
    Takeoff child = make_takeoff("c", "l", {0, 0, 5, 5}, "pg", "p");
    PageResult r = compute_page_quantities(cond_map({exact_area_condition(), line}), {parent, child});
    CHECK_EQ(at(r, "a").q1, 100.0);
}

// Windows compute_page_quantities: SQ10 area parent with a 2-point linear child [0,0,5,5].
// calc 15 (uom 1): 40.0 (the linear child adds no hole perimeter); calc 11: 100.0.
TEST(Qty03_ChildFiltering, ALinearChildAddsNoHolePerimeterToAnAreaParent)
{
    const int calcs[2] = {15, 11};
    const double windows[2] = {40.0, 100.0};
    for (int k = 0; k < 2; ++k)
    {
        SCOPED_CONTEXT("area calc " + std::to_string(calcs[k]));
        Condition line = make_condition("l", ConditionType::Linear, 1, 1);
        Takeoff parent = make_takeoff("p", "a", {0, 0, 10, 0, 10, 10, 0, 10}, "pg");
        Takeoff child = make_takeoff("c", "l", {0, 0, 5, 5}, "pg", "p");
        Condition area = make_condition("a", ConditionType::Area, calcs[k], 1);
        PageResult r = compute_page_quantities(cond_map({area, line}), {parent, child});
        CHECK_EQ(at(r, "a").q1, windows[k]);
    }
}

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_an_area_child_without_geometry_does_not_subtract
TEST(Qty03_ChildFiltering, AnAreaChildWithoutGeometryDoesNotSubtract)
{
    Takeoff parent = make_takeoff("p", "a", {0, 0, 10, 0, 10, 10, 0, 10}, "pg");
    Takeoff hole = make_takeoff("h", "a", {}, "pg", "p");
    CHECK_EQ(at(compute_page_quantities(cond_map({exact_area_condition()}), {parent, hole}), "a").q1, 100.0);
}

// Port of tests/domain/services/test_condition_quantity_service.py::CurvedHoleTests
TEST(Qty03_ChildFiltering, AnAreaHoleWhoseCurveFlagIsZeroIsStillSubtracted)
{
    Condition condition = make_condition("c", ConditionType::Area, 11, UOM_SQUARE_INCHES);
    Takeoff parent = make_takeoff("p", "c", {0, 0, 10, 0, 10, 10, 0, 10}, "pg");
    for (int curve : {0, -1})
    {
        SCOPED_CONTEXT("hole_curve " + std::to_string(curve));
        Takeoff hole = make_takeoff("h", "c", {0, 0, 2, 0, 2, 2, 0, 2}, "pg", "p", false, curve);
        CHECK_EQ(at(compute_page_quantities(cond_map({condition}), {parent, hole}), "c").q1, 96.0);
    }
}

// ============================================================================================
// QTY-04 (page level): round_quantity / round_up forwarded from the condition, per takeoff.
// ============================================================================================

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceConditionBehaviorTests::test_round_quantity_rounds_linear_and_area_results_without_mutating_geometry
TEST(Qty04_Rounding, RoundQuantityRoundsLinearAndAreaResultsWithoutMutatingGeometry)
{
    Condition linear = make_condition("linear", ConditionType::Linear, 1, 1);
    linear.round_quantity = true;
    linear.round_up = 12.0;
    Condition area = make_condition("area", ConditionType::Area, 11, 4);
    area.round_quantity = true;
    area.round_up = 12.0;
    Takeoff linear_takeoff = make_takeoff("t1", "linear", {0.0, 0.0, 13.0, 0.0});
    Takeoff area_takeoff = make_takeoff("t2", "area", {0.0, 0.0, 13.0, 0.0, 13.0, 13.0, 0.0, 13.0});
    std::vector<Takeoff> takeoffs = {linear_takeoff, area_takeoff};
    PageResult r = compute_page_quantities(cond_map({linear, area}), takeoffs);
    CHECK_EQ(at(r, "linear").q1, 24.0);
    CHECK_EQ(at(r, "area").q1, 180.0);
    CHECK(takeoffs[0].position == std::vector<double>({0.0, 0.0, 13.0, 0.0}));
    CHECK(takeoffs[1].position == std::vector<double>({0.0, 0.0, 13.0, 0.0, 13.0, 13.0, 0.0, 13.0}));
}

// Port of tests/domain/services/test_condition_quantity_service.py::ChildKindAndRoundingBoundaryTests::test_rounding_applies_to_each_takeoff_before_its_sign
TEST(Qty04_Rounding, RoundingAppliesToEachTakeoffBeforeItsSign)
{
    Condition linear = make_condition("l", ConditionType::Linear, 1, 1);
    linear.round_quantity = true;
    linear.round_up = 12.0;
    std::vector<Takeoff> takeoffs = {
        make_takeoff("1", "l", {0, 0, 13, 0}),
        make_takeoff("2", "l", {0, 0, 13, 0}),
        make_takeoff("3", "l", {0, 0, 5, 0}, "", "0", true),
    };
    // Per takeoff: 13 -> 24, 13 -> 24, 5 -> 12 (negative), so 24 + 24 - 12 = 36.
    // Rounding the net length instead (13 + 13 - 5 = 21) would give 24.
    CHECK_EQ(at(compute_page_quantities(cond_map({linear}), takeoffs), "l").q1, 36.0);
    CHECK_EQ(at(compute_page_quantities(cond_map({linear}), {takeoffs[2]}), "l").q1, -12.0);
}

// Port of tests/domain/services/test_condition_quantity_service.py::ForwardedConditionParameterTests::test_round_up_is_ignored_unless_rounding_is_switched_on
TEST(Qty04_Rounding, RoundUpIsIgnoredUnlessRoundingIsSwitchedOn)
{
    Takeoff takeoff = make_takeoff("t", "c", {0, 0, 13, 0});
    struct Case
    {
        bool enabled;
        double expected;
    };
    for (const Case &c : {Case{false, 13.0}, Case{true, 24.0}})
    {
        SCOPED_CONTEXT(std::string("round_quantity ") + (c.enabled ? "true" : "false"));
        Condition condition = make_condition("c", ConditionType::Linear, 1, UOM_EACH);
        condition.round_quantity = c.enabled;
        condition.round_up = 12.0;
        CHECK_EQ(at(compute_page_quantities(cond_map({condition}), {takeoff}), "c").q1, c.expected);
    }
}

// ============================================================================================
// QTY-17 (page level): an unsupported condition type reports (0, 0, 0) and never disturbs others.
// ============================================================================================

namespace
{
    Condition bad_condition(int type)
    {
        Condition c = make_condition("bad", static_cast<ConditionType>(type), 11, UOM_SQUARE_INCHES);
        c.name = "Foreign";
        return c;
    }
} // namespace

// Port of tests/domain/services/test_condition_quantity_service.py::UnsupportedConditionTypePageTests::test_an_unsupported_condition_is_skipped_and_the_rest_of_the_page_is_kept
// (the Windows log warning has no iOS Core counterpart).
TEST(Qty17_UnsupportedTypes, AnUnsupportedConditionIsSkippedAndTheRestOfThePageIsKept)
{
    for (int bad_type : {-1, 4, 7, 99})
    {
        SCOPED_CONTEXT("bad_type " + std::to_string(bad_type));
        Condition good = make_condition("good", ConditionType::Area, 11, UOM_SQUARE_INCHES);
        good.name = "Slab";
        std::vector<Takeoff> takeoffs = {
            make_takeoff("1", "good", SQUARE_10),
            make_takeoff("2", "bad", SQUARE_10),
            make_takeoff("3", "bad", SQUARE_10),
        };
        PageResult r = compute_page_quantities(cond_map({good, bad_condition(bad_type)}), takeoffs);
        CHECK_EQ(at(r, "good").q1, 100.0);
        QuantityResult bad = at(r, "bad");
        CHECK_EQ(bad.q1, 0.0);
        CHECK_EQ(bad.q2, 0.0);
        CHECK_EQ(bad.q3, 0.0);
    }
}

// Windows compute_page_quantities for an unsupported type with calc 11 AND calc 23 (iOS returned one
// per takeoff for 23): always (0.0, 0.0, 0.0), and a negative takeoff must not leave a negative zero
// (Port of tests/domain/services/test_condition_quantity_service.py::UnsupportedConditionTypeBoundaryTests::test_each_unsupported_condition_gets_its_own_warning_and_zero_entry).
TEST(Qty17_UnsupportedTypes, UnsupportedTypesReportPositiveZeroForEveryCalcType)
{
    for (int bad_type : {-3, -1, 4, 7, 99})
    {
        for (int calc : {11, 23, 19})
        {
            SCOPED_CONTEXT("bad_type " + std::to_string(bad_type) + " calc " + std::to_string(calc));
            Condition good = make_condition("good", ConditionType::Area, 11, UOM_SQUARE_INCHES);
            Condition bad = bad_condition(bad_type);
            bad.calc_type1 = calc;
            bad.calc_type2 = calc;
            bad.calc_type3 = calc;
            // Two positive takeoffs of the unsupported condition (iOS returned 1 per takeoff for
            // calc 19/23), and one negative takeoff alone (no negative zero).
            std::vector<std::vector<Takeoff>> pages = {
                {make_takeoff("1", "bad", SQUARE_10), make_takeoff("2", "bad", SQUARE_10),
                 make_takeoff("3", "good", SQUARE_10)},
                {make_takeoff("1", "bad", SQUARE_10, "", "0", true), make_takeoff("3", "good", SQUARE_10)},
            };
            for (size_t p = 0; p < pages.size(); ++p)
            {
                SCOPED_CONTEXT("page " + std::to_string(p));
                PageResult r = compute_page_quantities(cond_map({good, bad}), pages[p]);
                CHECK_EQ(r.size(), static_cast<size_t>(2));
                QuantityResult q = at(r, "bad");
                for (double v : {q.q1, q.q2, q.q3})
                {
                    CHECK_EQ(v, 0.0);
                    CHECK_FALSE(std::signbit(v));
                }
                CHECK_EQ(at(r, "good").q1, 100.0);
                CHECK_EQ(at(r, "good").q2, 0.0);
            }
        }
    }
}

// Port of tests/domain/services/test_condition_quantity_service.py::UnsupportedConditionTypeBoundaryTests::test_a_child_of_an_unsupported_type_never_changes_a_supported_parent
TEST(Qty17_UnsupportedTypes, AChildOfAnUnsupportedTypeNeverChangesASupportedParent)
{
    for (int bad_type : {3 + 5, -1})
    {
        SCOPED_CONTEXT("bad_type " + std::to_string(bad_type));
        Condition bad = bad_condition(bad_type);
        bad.width = 2;
        bad.depth = 3;
        Condition good = make_condition("good", ConditionType::Area, 12, UOM_SQUARE_INCHES);
        good.name = "Slab";
        Takeoff parent = make_takeoff("p", "good", SQUARE_10, "pg");
        Takeoff child = make_takeoff("c", "bad", {1, 1, 3, 1, 3, 3, 1, 3}, "pg", "p");
        PageResult r = compute_page_quantities(cond_map({good, bad}), {parent, child});
        QuantityResult g = at(r, "good");
        CHECK_EQ(g.q1, 100.0);
        CHECK_EQ(g.q2, 0.0);
        CHECK_EQ(g.q3, 0.0);
        QuantityResult b = at(r, "bad");
        CHECK_EQ(b.q1, 0.0);
        CHECK_EQ(b.q2, 0.0);
        CHECK_EQ(b.q3, 0.0);
    }
}

// ============================================================================================
// Regression ports (no divergence expected): the rest of test_condition_quantity_service.py.
// ============================================================================================

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_all_three_quantities_and_negative_signs_are_accumulated_per_condition
TEST(Qty00_PageRegression, AllThreeQuantitiesAndNegativeSignsAreAccumulatedPerCondition)
{
    Condition condition = exact_area_condition();
    condition.calc_type2 = 14;
    condition.uom2 = UOM_SQUARE_INCHES;
    Takeoff positive = make_takeoff("1", "a", {0, 0, 10, 0, 10, 10, 0, 10});
    Takeoff negative = make_takeoff("2", "a", {0, 0, 4, 0, 4, 4, 0, 4}, "", "0", true);
    QuantityResult total = at(compute_page_quantities(cond_map({condition}), {positive, negative}), "a");
    CHECK_EQ(total.q1, 100.0 - 16.0);
    CHECK_EQ(total.q2, 40.0 - 16.0);
    CHECK_EQ(total.q3, 1.0 - 1.0);
    QuantityResult only_negative = at(compute_page_quantities(cond_map({condition}), {negative}), "a");
    CHECK_EQ(only_negative.q1, -16.0);
    CHECK_EQ(only_negative.q2, -16.0);
    CHECK_EQ(only_negative.q3, -1.0);
}

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_conditions_are_totaled_separately_and_unknown_conditions_are_skipped
TEST(Qty00_PageRegression, ConditionsAreTotaledSeparatelyAndUnknownConditionsAreSkipped)
{
    CondMap conditions = cond_map({exact_area_condition("a"), exact_area_condition("b")});
    std::vector<Takeoff> takeoffs = {
        make_takeoff("1", "a", {0, 0, 2, 0, 2, 2, 0, 2}),
        make_takeoff("2", "b", {0, 0, 3, 0, 3, 3, 0, 3}),
        make_takeoff("3", "a", {0, 0, 1, 0, 1, 1, 0, 1}),
        make_takeoff("4", "gone", {0, 0, 9, 0, 9, 9, 0, 9}),
    };
    PageResult r = compute_page_quantities(conditions, takeoffs);
    CHECK_EQ(r.size(), static_cast<size_t>(2));
    CHECK_EQ(at(r, "a").q1, 5.0);
    CHECK_EQ(at(r, "b").q1, 9.0);
}

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityServiceExactTests::test_fractional_quantities_are_accumulated_without_integer_truncation
TEST(Qty00_PageRegression, FractionalQuantitiesAreAccumulatedWithoutIntegerTruncation)
{
    Condition condition = exact_area_condition();
    condition.uom1 = UOM_SQUARE_FEET;
    condition.uom2 = UOM_SQUARE_FEET;
    condition.calc_type3 = 11;
    condition.uom3 = UOM_SQUARE_FEET;
    Takeoff positive = make_takeoff("1", "a", {0, 0, 12, 0, 12, 9, 0, 9});
    Takeoff negative = make_takeoff("2", "a", {0, 0, 6, 0, 6, 6, 0, 6}, "", "0", true);
    QuantityResult total = at(compute_page_quantities(cond_map({condition}), {positive, negative}), "a");
    CHECK_NEAR(total.q1, 0.75 - 0.25, TOL7);
    CHECK_NEAR(total.q2, 0.75 - 0.25, TOL7);
    CHECK_NEAR(total.q3, 0.75 - 0.25, TOL7);
    QuantityResult only_negative = at(compute_page_quantities(cond_map({condition}), {negative}), "a");
    CHECK_NEAR(only_negative.q1, -0.25, TOL7);
    CHECK_NEAR(only_negative.q2, -0.25, TOL7);
    CHECK_NEAR(only_negative.q3, -0.25, TOL7);
}

// Port of tests/domain/services/test_condition_quantity_service.py::ChildKindAndRoundingBoundaryTests::test_an_area_takeoff_whose_parent_is_not_on_the_page_is_not_counted
TEST(Qty00_PageRegression, AnAreaTakeoffWhoseParentIsNotOnThePageIsNotCounted)
{
    Condition area = make_condition("a", ConditionType::Area, 11, UOM_SQUARE_INCHES);
    std::vector<Takeoff> takeoffs = {
        make_takeoff("p", "a", SQUARE_10, "pg"),
        make_takeoff("orphan", "a", {0, 0, 2, 0, 2, 2, 0, 2}, "pg", "missing"),
    };
    CHECK_EQ(at(compute_page_quantities(cond_map({area}), takeoffs), "a").q1, 100.0);
    CHECK_EQ(at(compute_page_quantities(cond_map({area}), {takeoffs[1]}), "a").q1, 0.0);
}

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityRootSentinelTests::test_a_takeoff_with_a_blank_uid_does_not_adopt_the_blank_parent_roots_as_holes
TEST(Qty00_PageRegression, ATakeoffWithABlankUidDoesNotAdoptTheBlankParentRootsAsHoles)
{
    Condition condition = make_condition("a", ConditionType::Area, 11, UOM_SQUARE_INCHES);
    std::vector<Takeoff> takeoffs = {
        make_takeoff("", "a", {0, 0, 10, 0, 10, 10, 0, 10}, "pg", ""),
        make_takeoff("r1", "a", {0, 0, 2, 0, 2, 2, 0, 2}, "pg", ""),
        make_takeoff("r2", "a", {0, 0, 2, 0, 2, 2, 0, 2}, "pg", ""),
    };
    CHECK_EQ(at(compute_page_quantities(cond_map({condition}), takeoffs), "a").q1, 100.0 + 4.0 + 4.0);
}

// ============================================================================================
// QTY-18: the parent token 'None' is a root parent in compute_page_quantities ('', '0', 'None').
// Takeoff::is_hole() keeps the 2-token rule ('', '0'); that is the user's decision.
// ============================================================================================

// Port of tests/domain/services/test_condition_quantity_service.py::ConditionQuantityRootSentinelTests::test_root_sentinel_parents_never_make_a_takeoff_the_hole_of_a_takeoff_named_like_the_sentinel
TEST(Qty18_NoneParentToken, RootSentinelParentsNeverMakeATakeoffTheHoleOfATakeoffNamedLikeTheSentinel)
{
    Condition condition = make_condition("a", ConditionType::Area, 11, UOM_SQUARE_INCHES);
    std::vector<Takeoff> takeoffs = {
        make_takeoff("0", "a", SQUARE_10, "pg", ""),
        make_takeoff("None", "a", SQUARE_10, "pg", ""),
    };
    const char *parents[] = {"0", "None", "0"};
    for (int i = 0; i < 3; ++i)
        takeoffs.push_back(make_takeoff("r" + std::to_string(i), "a", {0, 0, 2, 0, 2, 2, 0, 2}, "pg", parents[i]));
    // Windows 212.0 (the 'None' root r1 counts; iOS had 204 because it was skipped as a hole).
    CHECK_EQ(at(compute_page_quantities(cond_map({condition}), takeoffs), "a").q1, 100.0 + 100.0 + 3 * 4.0);
}

// Windows (read-only run of compute_page_quantities): only the exact token 'None' is a root.
// Other spellings are real parent uids; their takeoff is a hole of a takeoff that does not
// exist, so it is not counted (Windows 100.0 with the 100 SQ IN root p).
TEST(Qty18_NoneParentToken, OnlyTheExactTokenNoneIsARootParent)
{
    Condition condition = make_condition("a", ConditionType::Area, 11, UOM_SQUARE_INCHES);
    for (const char *token : {"none", "NONE", "None ", " None", "null", "True"})
    {
        SCOPED_CONTEXT(std::string("parent '") + token + "'");
        std::vector<Takeoff> takeoffs = {
            make_takeoff("p", "a", SQUARE_10, "pg", "0"),
            make_takeoff("x", "a", {0, 0, 1, 0, 1, 1, 0, 1}, "pg", token),
        };
        CHECK_EQ(at(compute_page_quantities(cond_map({condition}), takeoffs), "a").q1, 100.0);
    }
}

// Windows: a 'None'-parent takeoff alone counts (100.0); a negative one subtracts (100 - 4 = 96.0).
TEST(Qty18_NoneParentToken, ANoneParentAreaTakeoffCountsAndKeepsItsSign)
{
    Condition condition = make_condition("a", ConditionType::Area, 11, UOM_SQUARE_INCHES);
    CHECK_EQ(at(compute_page_quantities(cond_map({condition}), {make_takeoff("x", "a", SQUARE_10, "pg", "None")}), "a").q1,
             100.0);
    std::vector<Takeoff> takeoffs = {
        make_takeoff("p", "a", SQUARE_10, "pg", "0"),
        make_takeoff("n", "a", {0, 0, 2, 0, 2, 2, 0, 2}, "pg", "None", true),
    };
    CHECK_EQ(at(compute_page_quantities(cond_map({condition}), takeoffs), "a").q1, 100.0 - 4.0);
}

// Windows: a 'None'-parent root can still own holes (96.0), and its holes are found by its uid.
TEST(Qty18_NoneParentToken, ANoneParentRootStillOwnsItsHoles)
{
    Condition condition = make_condition("a", ConditionType::Area, 11, UOM_SQUARE_INCHES);
    std::vector<Takeoff> takeoffs = {
        make_takeoff("p", "a", SQUARE_10, "pg", "None"),
        make_takeoff("h", "a", {0, 0, 2, 0, 2, 2, 0, 2}, "pg", "p"),
    };
    CHECK_EQ(at(compute_page_quantities(cond_map({condition}), takeoffs), "a").q1, 96.0);
    // The hole is on another page than its parent: not subtracted (Windows 100.0).
    takeoffs[0].page_uid = "pg1";
    takeoffs[1].page_uid = "pg2";
    CHECK_EQ(at(compute_page_quantities(cond_map({condition}), takeoffs), "a").q1, 100.0);
}

// Windows: an attachment whose parent token is 'None' is nobody's child: it does not shrink an
// area takeoff that happens to be named 'None' (area calc 12 = area minus attachment footprint,
// Windows 100.0), while an attachment under a real parent does (94.0 = 100 - 2*3) and the
// attachment condition counts each takeoff once either way (1.0).
TEST(Qty18_NoneParentToken, AnAttachmentWithTheNoneParentTokenIsNotAdoptedByATakeoffNamedNone)
{
    Condition area = make_condition("a", ConditionType::Area, 12, UOM_SQUARE_INCHES);
    Condition attachment = make_condition("att", ConditionType::Attachment, CALC_COUNT, 0);
    attachment.width = 2;
    attachment.depth = 3;
    {
        std::vector<Takeoff> takeoffs = {
            make_takeoff("None", "a", SQUARE_10, "pg", ""),
            make_takeoff("c", "att", {5, 5}, "pg", "None"),
        };
        PageResult r = compute_page_quantities(cond_map({area, attachment}), takeoffs);
        CHECK_EQ(at(r, "a").q1, 100.0);
        CHECK_EQ(at(r, "att").q1, 1.0);
    }
    {
        std::vector<Takeoff> takeoffs = {
            make_takeoff("p", "a", SQUARE_10, "pg", "0"),
            make_takeoff("c", "att", {5, 5}, "pg", "p"),
        };
        PageResult r = compute_page_quantities(cond_map({area, attachment}), takeoffs);
        CHECK_EQ(at(r, "a").q1, 100.0 - 6.0);
        CHECK_EQ(at(r, "att").q1, 1.0);
    }
}

// Windows: non-area conditions count every takeoff whatever its parent token (12.0).
TEST(Qty18_NoneParentToken, NonAreaConditionsCountEveryTakeoffWithANoneParent)
{
    Condition linear = make_condition("l", ConditionType::Linear, 1, 1);
    std::vector<Takeoff> takeoffs = {
        make_takeoff("1", "l", {0, 0, 5, 0}, "", "None"),
        make_takeoff("2", "l", {0, 0, 7, 0}, "", "1"),
    };
    CHECK_EQ(at(compute_page_quantities(cond_map({linear}), takeoffs), "l").q1, 12.0);
}

// User decision (QTY-18 a): the 'None' root set belongs to the page quantities only; the
// Takeoff::is_hole() predicate (used by rendering, export and persistence) keeps '' and '0'.
TEST(Qty18_NoneParentToken, TakeoffIsHoleKeepsItsTwoTokenRule)
{
    Takeoff t;
    t.parent_uid = "";
    CHECK_FALSE(t.is_hole());
    t.parent_uid = "0";
    CHECK_FALSE(t.is_hole());
    t.parent_uid = "None";
    CHECK(t.is_hole());
    t.parent_uid = "p";
    CHECK(t.is_hole());
}

// Port of tests/domain/services/test_condition_quantity_service.py::ChildKindAndRoundingBoundaryTests::test_a_takeoff_without_geometry_totals_zero
TEST(Qty00_PageRegression, ATakeoffWithoutGeometryTotalsZero)
{
    struct Case
    {
        ConditionType type;
        int calc;
        int uom;
    };
    for (const Case &c : {Case{ConditionType::Linear, 1, 1}, Case{ConditionType::Area, 11, UOM_SQUARE_INCHES}})
    {
        SCOPED_CONTEXT("condition_type " + std::to_string(static_cast<int>(c.type)));
        Condition condition = make_condition("c", c.type, c.calc, c.uom);
        Takeoff takeoff = make_takeoff("t", "c", {});
        PageResult r = compute_page_quantities(cond_map({condition}), {takeoff});
        CHECK_EQ(r.size(), static_cast<size_t>(1));
        QuantityResult q = at(r, "c");
        CHECK_EQ(q.q1, 0.0);
        CHECK_EQ(q.q2, 0.0);
        CHECK_EQ(q.q3, 0.0);
    }
}

// Port of tests/domain/services/test_condition_quantity_service.py::ChildKindAndRoundingBoundaryTests::test_very_large_geometry_is_summed_without_overflow
TEST(Qty00_PageRegression, VeryLargeGeometryIsSummedWithoutOverflow)
{
    Condition condition = make_condition("c", ConditionType::Area, 11, UOM_SQUARE_INCHES);
    const double side = 1e12;
    Takeoff big = make_takeoff("t", "c", {0, 0, side, 0, side, side, 0, side});
    double twice = at(compute_page_quantities(cond_map({condition}), {big, big}), "c").q1;
    CHECK_NEAR(twice / 2e24, 1.0, TOL7);
}

// Port of tests/domain/services/test_condition_quantity_service.py::EverySupportedConditionTypeQuantityTests
TEST(Qty00_PageRegression, EverySupportedConditionTypeMeasuresItsOwnGeometry)
{
    auto q1 = [](ConditionType type, int calc, int uom, std::vector<double> position)
    {
        Condition condition = make_condition("c", type, calc, uom);
        PageResult r = compute_page_quantities(cond_map({condition}), {make_takeoff("t", "c", std::move(position))});
        return at(r, "c").q1;
    };
    CHECK_EQ(q1(ConditionType::Linear, 1, 1, {0, 0, 5, 0}), 5.0);
    CHECK_EQ(q1(ConditionType::Area, 11, UOM_SQUARE_INCHES, SQUARE_10), 100.0);
    for (ConditionType type : {ConditionType::Count, ConditionType::Attachment})
    {
        CHECK_EQ(q1(type, CALC_COUNT, 0, {5, 5}), 1.0);
        CHECK_EQ(q1(type, CALC_COUNT, 0, SQUARE_10), 1.0);
    }
}

// Port of tests/domain/services/test_condition_quantity_service.py::ForwardedConditionParameterTests
// (rise/run, grid sizes and gap, curve flag, count dimensions and per-slot units).
TEST(Qty00_PageRegression, ConditionParametersReachTheCalculatorSlotBySlot)
{
    const std::vector<double> BOX = {0, 0, 22, 0, 22, 10, 0, 10};
    {
        // test_rise_and_run_scale_area_and_perimeter_by_the_absolute_slope
        struct RR
        {
            double rise, run;
        };
        for (const RR &rr : {RR{3.0, 4.0}, RR{-3.0, 4.0}, RR{3.0, -4.0}})
        {
            SCOPED_CONTEXT("rise " + std::to_string(rr.rise) + " run " + std::to_string(rr.run));
            Condition c = make_condition("c", ConditionType::Area, 11, UOM_SQUARE_INCHES);
            c.calc_type2 = 14;
            c.uom2 = UOM_EACH;
            c.rise = rr.rise;
            c.run = rr.run;
            QuantityResult q = at(compute_page_quantities(cond_map({c}), {make_takeoff("t", "c", SQUARE_10)}), "c");
            CHECK_EQ(q.q1, 125.0);
            CHECK_EQ(q.q2, 50.0);
            CHECK_EQ(q.q3, 0.0);
        }
    }
    {
        // test_rise_and_run_scale_a_linear_length_only_for_a_positive_slope
        struct Case
        {
            double rise, run, expected;
        };
        for (const Case &k : {Case{3.0, 4.0, 12.5}, Case{-3.0, 4.0, 10.0}, Case{3.0, 0.0, 10.0}})
        {
            SCOPED_CONTEXT("rise " + std::to_string(k.rise) + " run " + std::to_string(k.run));
            Condition c = make_condition("c", ConditionType::Linear, 1, UOM_EACH);
            c.rise = k.rise;
            c.run = k.run;
            CHECK_EQ(at(compute_page_quantities(cond_map({c}), {make_takeoff("t", "c", {0, 0, 10, 0})}), "c").q1,
                     k.expected);
        }
    }
    {
        // test_grid_sizes_and_gap_reach_the_grid_quantities_in_slot_order
        Condition c = make_condition("c", ConditionType::Area, 18, UOM_EACH);
        c.calc_type2 = 16;
        c.uom2 = UOM_EACH;
        c.calc_type3 = 17;
        c.uom3 = UOM_EACH;
        c.grid_size1 = 3.0;
        c.grid_size2 = 1.0;
        c.gap = 1.0;
        QuantityResult q = at(compute_page_quantities(cond_map({c}), {make_takeoff("t", "c", BOX)}), "c");
        CHECK_EQ(q.q1, 5 * 5);
        CHECK_EQ(q.q2, 6 * 10 + 5 * 22);
        CHECK_EQ(q.q3, 220.0 / 8);
    }
    {
        // test_a_different_gap_and_swapped_grid_sizes_change_those_quantities
        Condition c = make_condition("c", ConditionType::Area, 18, UOM_EACH);
        c.calc_type2 = 16;
        c.uom2 = UOM_EACH;
        c.calc_type3 = 17;
        c.uom3 = UOM_EACH;
        c.grid_size1 = 1.0;
        c.grid_size2 = 3.0;
        c.gap = 0.0;
        QuantityResult q = at(compute_page_quantities(cond_map({c}), {make_takeoff("t", "c", BOX)}), "c");
        CHECK_EQ(q.q1, 22 * 3);
        CHECK_EQ(q.q2, 22 * 10 + 4 * 22);
        CHECK_EQ(q.q3, 220.0 / 3);
    }
    {
        // test_an_enabled_curve_lengthens_the_linear_quantity_and_curve_minus_one_does_not
        Condition c = make_condition("c", ConditionType::Linear, 1, UOM_EACH);
        const std::vector<double> position = {0, 0, 10, 0, 5, 5};
        CHECK_EQ(at(compute_page_quantities(cond_map({c}), {make_takeoff("t", "c", position, "", "0", false, -1)}), "c").q1,
                 10.0);
        CHECK_NEAR(at(compute_page_quantities(cond_map({c}), {make_takeoff("t", "c", position, "", "0", false, 0)}), "c").q1,
                   15.576465376942007, TOL7);
    }
    {
        // test_count_dimensions_and_per_slot_units_are_forwarded
        Condition c = make_condition("c", ConditionType::Count, CALC_WIDTH_SIDE, UOM_EACH);
        c.width = 4.0;
        c.height = 7.0;
        c.depth = 3.0;
        c.calc_type2 = CALC_DEPTH_SIDE;
        c.uom2 = UOM_LINEAR_FEET;
        c.calc_type3 = CALC_HEIGHT_OR_LENGTH;
        c.uom3 = UOM_LINEAR_YARDS;
        QuantityResult q = at(compute_page_quantities(cond_map({c}), {make_takeoff("t", "c", {1, 1})}), "c");
        CHECK_NEAR(q.q1, 4.0 * 7.0, TOL7);
        CHECK_NEAR(q.q2, 3.0 * 7.0 / 12.0, TOL7);
        CHECK_NEAR(q.q3, 7.0 / 36.0, TOL7);
    }
}
