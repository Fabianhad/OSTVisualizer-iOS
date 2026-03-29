#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace ost_domain
{

    struct Folder
    {
        std::string uid;
        std::string name;
        std::string description;
        std::vector<std::string> page_uids;
        std::unordered_map<std::string, Folder> subfolders;
        std::vector<std::string> all_page_uids() const
        {
            std::vector<std::string> result = page_uids;
            for (const auto &pair : subfolders)
            {
                auto sub = pair.second.all_page_uids();
                result.insert(result.end(), sub.begin(), sub.end());
            }
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
