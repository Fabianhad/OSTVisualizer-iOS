#pragma once
#include "OrderedUidMap.hpp"
#include <string>
#include <vector>

namespace ost_domain
{

    struct Folder
    {
        std::string uid;
        std::string name;
        std::string description;
        std::vector<std::string> page_uids;
        // Insertion-ordered (Windows: subfolders appear in the order the rows were read, Name then UID).
        OrderedUidMap<Folder> subfolders;
        // The page tree walk of Windows (page_combo: subfolders first, in order, then the folder's own pages).
        std::vector<std::string> all_page_uids() const
        {
            std::vector<std::string> result;
            for (const auto &pair : subfolders)
            {
                auto sub = pair.second.all_page_uids();
                result.insert(result.end(), sub.begin(), sub.end());
            }
            result.insert(result.end(), page_uids.begin(), page_uids.end());
            return result;
        }

        bool empty() const
        {
            if (!page_uids.empty())
                return false;
            for (const auto &pair : subfolders)
            {
                if (!pair.second.empty())
                    return false;
            }
            return true;
        }
    };

}
