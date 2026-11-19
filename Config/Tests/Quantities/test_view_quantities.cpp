// QTY-11 / QTY-15: the sidebar quantities are computed for the pages of the OPEN view only.
//
// Windows: ost_visualizer/presentation/coordinators/sidebar_coordinator.py update_conditions_quantities (218-248):
//   3D view (view stack index 0): the selected 3D page uids; 2D view: only the active 2D page; no page at all: {} (every
//   condition shows zero and compute_quantities_for_pages is not even called);
//   project_data_service.compute_quantities_for_pages (1097-1104): the takeoffs of exactly those pages, page after page, fed
//   to compute_page_quantities.
// Windows tests: tests/presentation/coordinators/test_sidebar_coordinator.py::test_sidebar_quantities_use_selected_pages_in_3d_view,
//   ::test_sidebar_quantities_clear_without_active_page, ::test_sidebar_quantities_include_hidden_layer_conditions and
//   tests/integration/quantities/test_page_deletion_sidebar.py::test_conditions_spanning_deleted_and_remaining_pages_recalculate
//   (the counts below). No Windows unit test pins the page-set rule itself on the UI side, so the 2D / 3D cases are written
//   from the coordinator code above.
#include "QtyHelpers.hpp"

#include "Domain/QuantityPages.hpp"

#include <string>
#include <vector>

using namespace qty;

namespace
{
    // Port of PageDeletionQuantitySidebarTests: two count conditions, takeoffs 1 and 3 on page-1, takeoff 2 on page-2.
    struct Spanning
    {
        CondMap conditions;
        std::vector<Takeoff> takeoffs;
    };

    Spanning spanning()
    {
        Spanning s;
        s.conditions["spanning"] = make_condition("spanning", ConditionType::Count, CALC_COUNT, UOM_EACH);
        s.conditions["deleted-only"] = make_condition("deleted-only", ConditionType::Count, CALC_COUNT, UOM_EACH);
        s.takeoffs.push_back(make_takeoff("takeoff-1", "spanning", {}, "page-1"));
        s.takeoffs.push_back(make_takeoff("takeoff-2", "spanning", {}, "page-2"));
        s.takeoffs.push_back(make_takeoff("takeoff-3", "deleted-only", {}, "page-1"));
        return s;
    }

    double q1(const std::unordered_map<std::string, QuantityResult> &r, const std::string &uid)
    {
        auto it = r.find(uid);
        return it == r.end() ? 0.0 : it->second.q1;
    }
}

// 3D view: the selected 3D pages decide, the active 2D page does not count. test_sidebar_quantities_use_selected_pages_in_3d_view
// (active page p1, selected pages p3 and p4: only those two are computed) and the deletion test (all pages: 2 and 1).
TEST(ViewQuantities, TheThreeDViewUsesTheSelectedThreeDPages)
{
    const Spanning s = spanning();
    // both pages selected, the 2D page is page-2
    auto both = compute_view_quantities(s.conditions, s.takeoffs, true, "page-2", {"page-1", "page-2"});
    CHECK_EQ(q1(both, "spanning"), 2.0);
    CHECK_EQ(q1(both, "deleted-only"), 1.0);
    // only page-1 is selected in 3D while the 2D page is page-2: the 2D page does not add its takeoff
    auto first = compute_view_quantities(s.conditions, s.takeoffs, true, "page-2", {"page-1"});
    CHECK_EQ(q1(first, "spanning"), 1.0);
    CHECK_EQ(q1(first, "deleted-only"), 1.0);
}

// 2D view: only the active 2D page counts, even when more pages are selected for the 3D view (QTY-11: iOS summed the 2D page
// and every 3D page).
TEST(ViewQuantities, TheTwoDViewUsesOnlyTheActivePage)
{
    const Spanning s = spanning();
    auto page2 = compute_view_quantities(s.conditions, s.takeoffs, false, "page-2", {"page-1", "page-2"});
    CHECK_EQ(q1(page2, "spanning"), 1.0);      // test_page_deletion_sidebar: page-2 only -> spanning 1
    CHECK_EQ(q1(page2, "deleted-only"), 0.0);  //                                           deleted_only 0
    auto page1 = compute_view_quantities(s.conditions, s.takeoffs, false, "page-1", {"page-2"});
    CHECK_EQ(q1(page1, "spanning"), 1.0);
    CHECK_EQ(q1(page1, "deleted-only"), 1.0);
}

// No page: no quantities at all (test_sidebar_quantities_clear_without_active_page): a 2D view without an active page and a 3D
// view without selected pages.
TEST(ViewQuantities, NoPageGivesNoQuantities)
{
    const Spanning s = spanning();
    CHECK(compute_view_quantities(s.conditions, s.takeoffs, false, "", {"page-1", "page-2"}).empty());
    CHECK(compute_view_quantities(s.conditions, s.takeoffs, true, "page-1", {}).empty());
    CHECK(compute_view_quantities(s.conditions, s.takeoffs, true, "", {}).empty());
}

// A takeoff without a page (an empty page uid) is never counted for an empty page: the "no active page" case must not match
// takeoffs whose page uid is empty.
TEST(ViewQuantities, AnEmptyPageUidIsNoPage)
{
    Spanning s = spanning();
    s.takeoffs.push_back(make_takeoff("takeoff-4", "spanning", {}, ""));
    auto none = compute_view_quantities(s.conditions, s.takeoffs, false, "", {});
    CHECK(none.empty());
}

// compute_quantities_for_pages: the takeoffs are fed to compute_page_quantities in the order of the pages given, page after page
// (a page listed twice would count twice in Windows; the 3D selection never lists a page twice).
TEST(ViewQuantities, TakeoffsOfTheSelectedPagesAreAllCounted)
{
    Spanning s = spanning();
    s.takeoffs.push_back(make_takeoff("takeoff-5", "spanning", {}, "page-3"));
    auto all = compute_view_quantities(s.conditions, s.takeoffs, true, "page-1", {"page-1", "page-2", "page-3"});
    CHECK_EQ(q1(all, "spanning"), 3.0);
    CHECK_EQ(q1(all, "deleted-only"), 1.0);
}
