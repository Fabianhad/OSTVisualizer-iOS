// Layer: Data. The decision of Delete Page that needs the loaded project: see PageDeletePlan.hpp (plan 5.6).
#include "PageDeletePlan.hpp"
#include "PageNavigation.hpp"
#include "../Domain/PageDeletion.hpp"
#include <algorithm>
#include <vector>

namespace ost_data
{
    PageDeletePlan plan_page_delete(const ProjectData &project, const std::string &page_uid)
    {
        PageDeletePlan plan;
        if (project.empty())
        {
            plan.refusal = PageDeleteRefusal::NoProject;
            return plan;
        }
        const auto page = project.pages.find(page_uid);
        if (page_uid.empty() || page == project.pages.end())
        {
            plan.refusal = PageDeleteRefusal::UnknownPage;
            return plan;
        }
        // _stage_selection_after_page_delete (:5992-5994): the page must be in the tree order, else nothing is staged.
        const std::vector<std::string> order = page_tree_order(project);
        const auto found = std::find(order.begin(), order.end(), page_uid);
        if (found == order.end())
        {
            plan.refusal = PageDeleteRefusal::UnknownPage;
            return plan;
        }
        plan.display_name = ost_domain::page_delete_display_name(page->second);
        std::vector<std::string> remaining;
        remaining.reserve(order.size());
        for (const std::string &uid : order)
            if (uid != page_uid)
                remaining.push_back(uid);
        if (remaining.empty())
        {
            plan.refusal = PageDeleteRefusal::LastPage;
            return plan;
        }
        // :6003-6005: next_index = min(deleted_index, len(remaining) - 1)
        const std::size_t index = static_cast<std::size_t>(found - order.begin());
        plan.next_page_uid = remaining[std::min(index, remaining.size() - 1)];
        plan.allowed = true;
        return plan;
    }
}
