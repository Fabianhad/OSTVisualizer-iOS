// Layer: Domain. The rules of Delete Page without a database: see PageDeletion.hpp (plan 5.6).
#include "PageDeletion.hpp"
#include <set>

namespace ost_domain
{
    // constants.py:83-110: PAGE_ANNOTATION_TABLES + PAGE_AUXILIARY_CHILD_TABLES, in this order (the order of the step 8 statements).
    const char *const PAGE_DELETE_CHILD_TABLES[] = {
        "BidDimensions", "BidALines", "BidArrows", "BidHighlights", "BidTexts", "BidCallOuts", "BidAnnotationRects", "BidAnnotationOvals",
        "BidAnnotationPolygons", "BidAnnotationClouds", "BidAnnoInk", "BidLegends", "BidComments", "BidPageSettings", "BidAreaTranslations",
        "BidMarkedPages", "BidPercents", "BidTakeoffTotals", "BidLaborCostCodeTotals", "BidTypicalGroupTotals", "BidTypGroupViews", "Boost",
        "DPCCalcFilter"};
    const std::size_t PAGE_DELETE_CHILD_TABLE_COUNT = sizeof(PAGE_DELETE_CHILD_TABLES) / sizeof(PAGE_DELETE_CHILD_TABLES[0]);

    // constants.py:111-121: ("BidTakeoffs",) + PAGE_ANNOTATION_TABLES + ("BidNamedViews", "BidHotLinks") without "BidLegends".
    const char *const PAGE_DELETE_CONFIRMATION_TABLES[] = {
        "BidTakeoffs", "BidDimensions", "BidALines", "BidArrows", "BidHighlights", "BidTexts", "BidCallOuts", "BidAnnotationRects",
        "BidAnnotationOvals", "BidAnnotationPolygons", "BidAnnotationClouds", "BidAnnoInk", "BidComments", "BidNamedViews", "BidHotLinks"};
    const std::size_t PAGE_DELETE_CONFIRMATION_TABLE_COUNT =
        sizeof(PAGE_DELETE_CONFIRMATION_TABLES) / sizeof(PAGE_DELETE_CONFIRMATION_TABLES[0]);

    // ui_event_coordinator.py:6009-6015: page.image_path.rsplit("\\", 1)[-1].rsplit("/", 1)[-1].
    std::string page_delete_display_name(const Page &page)
    {
        if (!page.name.empty())
            return page.name;
        if (!page.image_path.empty())
        {
            std::string path = page.image_path;
            std::size_t cut = path.rfind('\\');
            if (cut != std::string::npos)
                path = path.substr(cut + 1);
            cut = path.rfind('/');
            if (cut != std::string::npos)
                path = path.substr(cut + 1);
            return path;
        }
        return "Page " + page.uid;
    }

    // project_write_service.py:6884-6892 _unique_nonempty_uids.
    std::vector<std::string> normalize_page_uids(const std::vector<std::string> &uids)
    {
        std::vector<std::string> out;
        std::set<std::string> seen;
        for (const std::string &uid : uids)
            if (!uid.empty() && seen.insert(uid).second)
                out.push_back(uid);
        return out;
    }

    DialogText page_delete_confirmation(const std::string &display_name, bool has_content)
    {
        DialogText text;
        text.title = "Delete Verification";
        if (has_content)
            text.message = "The page \"" + display_name +
                           "\" contains takeoffs, annotations, comments, Named Views or hotlinks. Deleting this page will remove the page "
                           "and everything on it from this bid.\n\n"
                           "Deleting a page cannot be undone. Are you sure you want to delete this page?";
        else
            text.message = "Delete the page \"" + display_name + "\"?\n\nDeleting a page cannot be undone.";
        return text;
    }

    DialogText page_delete_failure_text(PageDeleteFailure failure)
    {
        DialogText text;
        text.title = LAST_PAGE_TITLE; // the same title "Delete Page" as the last-page warning
        text.message = std::string(failure == PageDeleteFailure::VerifyContent ? "Failed to verify page contents. " : "Failed to delete page. ") +
                       DB_LOCKED_HINT;
        return text;
    }
}
