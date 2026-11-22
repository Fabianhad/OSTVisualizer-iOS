#include "PageNavigation.hpp"
#include <algorithm>
#include <unordered_set>

namespace ost_data
{

    std::vector<std::string> page_tree_order(const ProjectData &project)
    {
        std::vector<std::string> out;
        std::unordered_set<std::string> seen;
        auto add = [&](const std::string &uid)
        {
            if (project.pages.count(uid) && seen.insert(uid).second)
                out.push_back(uid);
        };

        // load_bid: the folders in order, each with its subfolders first and then its own pages (Folder::all_page_uids)
        for (const auto &folder : project.folders)
            for (const std::string &uid : folder.second.all_page_uids())
                add(uid);
        // then the pages without a folder, in page order
        for (const std::string &uid : project.page_order)
        {
            auto page = project.pages.find(uid);
            if (page != project.pages.end() && page->second.folder_uid.empty())
                add(uid);
        }
        // a page that is in neither list (a project that was not read by load_project) must not vanish
        std::vector<const ost_domain::Page *> unlisted;
        for (const auto &pair : project.pages)
            if (!seen.count(pair.first))
                unlisted.push_back(&pair.second);
        std::sort(unlisted.begin(), unlisted.end(), [](const ost_domain::Page *a, const ost_domain::Page *b)
                  {
                      if (a->sequence != b->sequence)
                          return a->sequence < b->sequence;
                      if (a->name != b->name)
                          return a->name < b->name;
                      return a->uid < b->uid;
                  });
        for (const ost_domain::Page *page : unlisted)
            out.push_back(page->uid);
        return out;
    }

    std::vector<std::string> selected_pages_in_tree_order(const ProjectData &project, const std::set<std::string> &selected)
    {
        std::vector<std::string> out;
        for (const std::string &uid : page_tree_order(project))
            if (selected.count(uid))
                out.push_back(uid);
        return out;
    }

    std::string initial_page_uid(const ProjectData &project)
    {
        if (!project.selected_page_uid.empty() && project.pages.count(project.selected_page_uid))
            return project.selected_page_uid;
        const std::vector<std::string> order = page_tree_order(project);
        return order.empty() ? std::string() : order.front();
    }

    bool toggle_folder_pages(std::set<std::string> &selected, const std::vector<std::string> &folder_pages)
    {
        if (folder_pages.empty())
            return false;
        const bool all_selected = std::all_of(folder_pages.begin(), folder_pages.end(),
                                              [&selected](const std::string &uid) { return selected.count(uid) > 0; });
        if (all_selected)
        {
            for (const std::string &uid : folder_pages)
                selected.erase(uid);
            return true;
        }
        bool changed = false;
        for (const std::string &uid : folder_pages)
            changed = selected.insert(uid).second || changed;
        return changed;
    }

    bool activate_page(std::string &active_uid, const std::string &uid)
    {
        if (active_uid == uid)
            return false;
        active_uid = uid;
        return true;
    }

    std::string page_label(const ost_domain::Page &page, bool show_page_index, bool show_sheet_number)
    {
        std::string label;
        auto append = [&label](const std::string &part)
        {
            label += (label.empty() ? "" : " - ") + part;
        };
        if (show_page_index && page.sequence > 0)
            append(std::to_string(page.sequence));
        if (show_sheet_number && !page.sheet_no.empty())
            append(page.sheet_no);
        if (!page.name.empty())
            append(page.name);
        // `" - ".join(parts) if parts else page.uid`: a part is never empty, so an empty label means "no part"
        return label.empty() ? page.uid : label;
    }

}
