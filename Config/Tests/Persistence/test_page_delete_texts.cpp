// The two decisions of the Delete Page screen that the fix round moved out of the Objective-C layer into Core:
//   * which alert a delete the STORE refused gets (ost_domain::page_delete_store_failure_text): the last page has its own text, every
//     other refusal the generic "Failed to delete page. <DB_LOCKED_HINT>";
//   * which refusal of the PLAN gets an alert before any question (ost_data::page_delete_refusal_text): the last page yes, an unknown page
//     and an empty project no, an allowed plan nothing.
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5962-5968   "Delete Page" / "Failed to delete page. <DB_LOCKED_HINT>"
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5993-6001   an unknown page stages nothing and says nothing; the last
//                                                                                page is warned about: "Delete Page" / "Cannot delete the last page in the bid."
//   ost_visualizer/presentation/utils/messagebox.py:8                             DB_LOCKED_HINT (inside page_delete_failure_text)
// Windows has no test that pins these two choices: the rows are pinned from the source lines above.
#include "TestHarness.hpp"

#include "Data/PageDeletePlan.hpp"
#include "Data/PageMenuRules.hpp"
#include "Domain/Page.hpp"
#include "Domain/PageDeletion.hpp"
#include "Domain/PageDialogText.hpp"

#include <optional>
#include <string>
#include <vector>

using ost_data::PageDeleteRefusal;

namespace
{
    ost_data::ProjectData delete_project(const std::vector<std::string> &order)
    {
        ost_data::ProjectData project;
        project.bid.uid = "10";
        int sequence = 1;
        for (const std::string &uid : order)
        {
            ost_domain::Page page;
            page.uid = uid;
            page.name = "Page " + uid;
            page.sequence = sequence++;
            project.pages[uid] = page;
            project.page_order.push_back(uid);
        }
        return project;
    }

    void check_is_last_page_text(const ost_domain::DialogText &text)
    {
        CHECK_EQ(text.title, std::string("Delete Page"));
        CHECK_EQ(text.message, std::string("Cannot delete the last page in the bid."));
    }

    void check_is_generic_failure(const ost_domain::DialogText &text)
    {
        const ost_domain::DialogText generic = ost_domain::page_delete_failure_text(ost_domain::PageDeleteFailure::Delete);
        CHECK_EQ(text.title, generic.title);
        CHECK_EQ(text.message, generic.message);
        CHECK(text.message.rfind("Failed to delete page.", 0) == 0);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// page_delete_store_failure_text
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(PageDeleteTexts, StoreFailure_TheLastPageReason_IsTheLastPageText)
{
    check_is_last_page_text(ost_domain::page_delete_store_failure_text(ost_domain::LAST_PAGE_MESSAGE));
    check_is_last_page_text(ost_domain::page_delete_store_failure_text("Cannot delete the last page in the bid."));
}

TEST(PageDeleteTexts, StoreFailure_NoReason_IsTheGenericFailure)
{
    // A failed statement, a failed read and a locked bid give the bridge no reason text.
    check_is_generic_failure(ost_domain::page_delete_store_failure_text(""));
}

TEST(PageDeleteTexts, StoreFailure_AnIdentityCheckReason_IsTheGenericFailure_AndTheReasonIsNotShown)
{
    const std::string reason = "Page 7 does not belong to bid 10";
    const ost_domain::DialogText text = ost_domain::page_delete_store_failure_text(reason);
    check_is_generic_failure(text);
    CHECK(text.message.find(reason) == std::string::npos);
}

TEST(PageDeleteTexts, StoreFailure_TheLastPageComparisonIsExact)
{
    const std::string last = ost_domain::LAST_PAGE_MESSAGE;
    check_is_generic_failure(ost_domain::page_delete_store_failure_text(last + " "));
    check_is_generic_failure(ost_domain::page_delete_store_failure_text(" " + last));
    check_is_generic_failure(ost_domain::page_delete_store_failure_text(last.substr(0, last.size() - 1)));
    check_is_generic_failure(ost_domain::page_delete_store_failure_text("cannot delete the last page in the bid."));
    check_is_generic_failure(ost_domain::page_delete_store_failure_text(last + "\n" + last));
}

// ---------------------------------------------------------------------------------------------------------------------------------
// page_delete_refusal_text
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(PageDeleteTexts, PlanRefusal_LastPage_GetsTheLastPageAlert)
{
    const std::optional<ost_domain::DialogText> text = ost_data::page_delete_refusal_text(PageDeleteRefusal::LastPage);
    REQUIRE(text.has_value());
    check_is_last_page_text(*text);
}

TEST(PageDeleteTexts, PlanRefusal_UnknownPage_AndNoProject_AreSilent)
{
    CHECK(!ost_data::page_delete_refusal_text(PageDeleteRefusal::UnknownPage).has_value());
    CHECK(!ost_data::page_delete_refusal_text(PageDeleteRefusal::NoProject).has_value());
}

TEST(PageDeleteTexts, PlanRefusal_AnAllowedPlan_HasNothingToSay)
{
    CHECK(!ost_data::page_delete_refusal_text(PageDeleteRefusal::None).has_value());
}

TEST(PageDeleteTexts, PlanRefusal_FollowsTheRealPlans)
{
    // The same pairing the screen makes: plan_page_delete first, then the text of its refusal.
    const ost_data::ProjectData one_page = delete_project({"1"});
    const ost_data::PageDeletePlan last = ost_data::plan_page_delete(one_page, "1");
    CHECK(!last.allowed);
    const std::optional<ost_domain::DialogText> last_text = ost_data::page_delete_refusal_text(last.refusal);
    REQUIRE(last_text.has_value());
    check_is_last_page_text(*last_text);

    const ost_data::ProjectData three_pages = delete_project({"1", "2", "3"});
    const ost_data::PageDeletePlan unknown = ost_data::plan_page_delete(three_pages, "99");
    CHECK(!unknown.allowed);
    CHECK(!ost_data::page_delete_refusal_text(unknown.refusal).has_value());

    const ost_data::PageDeletePlan blank = ost_data::plan_page_delete(three_pages, "");
    CHECK(!blank.allowed);
    CHECK(!ost_data::page_delete_refusal_text(blank.refusal).has_value());

    const ost_data::PageDeletePlan allowed = ost_data::plan_page_delete(three_pages, "2");
    CHECK(allowed.allowed);
    CHECK(!ost_data::page_delete_refusal_text(allowed.refusal).has_value());

    const ost_data::ProjectData no_project;
    const ost_data::PageDeletePlan none = ost_data::plan_page_delete(no_project, "1");
    CHECK(!none.allowed);
    CHECK(!ost_data::page_delete_refusal_text(none.refusal).has_value());
}
