#pragma once
#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ost_domain
{

    struct ConditionFolder
    {
        std::string uid;
        std::string name;
        // BidConditionFolders.ParentUID. "" = no parent (NULL, "", 0 and "0" all read as "", like Windows
        // BidConditionFolder.parent_uid). A parent that is not one of the bid's folders makes the folder a root folder.
        std::string parent_uid;
    };

    // ParentUID cycles among the folders of one bid. Windows refuses to read such a bid (bid_owned_identity.
    // require_acyclic_bid_owned_parent_graph, called by bid_data_reader._parse_bid_condition_folders_for_bid) and names
    // the smallest uid of the cycles it found (decimal uids numerically before the others). Returns "" when the parents
    // form no cycle. A parent that is "" / "0" or not one of the folders ends the chain; a folder that is its own parent
    // is a cycle.
    inline std::string find_condition_folder_cycle(const std::unordered_map<std::string, ConditionFolder> &folders)
    {
        std::unordered_map<std::string, std::string> parent_of;
        for (const auto &pair : folders)
        {
            const std::string &parent = pair.second.parent_uid;
            if (!parent.empty() && parent != "0")
                parent_of[pair.first] = parent;
        }

        std::unordered_set<std::string> completed;
        std::unordered_set<std::string> cycle_uids;
        for (const auto &start : parent_of)
        {
            if (completed.count(start.first))
                continue;
            std::vector<std::string> path;
            std::unordered_map<std::string, std::size_t> path_index;
            std::string current = start.first;
            while (parent_of.count(current) && !completed.count(current))
            {
                auto seen = path_index.find(current);
                if (seen != path_index.end())
                {
                    for (std::size_t i = seen->second; i < path.size(); ++i)
                        cycle_uids.insert(path[i]);
                    break;
                }
                path_index[current] = path.size();
                path.push_back(current);
                current = parent_of.at(current);
            }
            for (const std::string &uid : path)
                completed.insert(uid);
        }
        if (cycle_uids.empty())
            return "";

        auto is_decimal = [](const std::string &uid)
        {
            if (uid.empty())
                return false;
            for (const char c : uid)
                if (c < '0' || c > '9')
                    return false;
            return true;
        };
        // numeric order of decimal uids without overflow: strip leading zeros, then shorter is smaller, then text
        auto numeric_less = [](const std::string &a, const std::string &b)
        {
            const std::size_t a_start = std::min(a.find_first_not_of('0'), a.size());
            const std::size_t b_start = std::min(b.find_first_not_of('0'), b.size());
            const std::size_t a_len = a.size() - a_start;
            const std::size_t b_len = b.size() - b_start;
            if (a_len != b_len)
                return a_len < b_len;
            return a.compare(a_start, a_len, b, b_start, b_len) < 0;
        };
        const std::string *smallest = nullptr;
        for (const std::string &uid : cycle_uids)
        {
            if (!smallest)
            {
                smallest = &uid;
                continue;
            }
            const bool uid_decimal = is_decimal(uid);
            const bool smallest_decimal = is_decimal(*smallest);
            bool less;
            if (uid_decimal != smallest_decimal)
                less = uid_decimal;
            else if (uid_decimal)
                less = numeric_less(uid, *smallest);
            else
                less = uid < *smallest;
            if (less)
                smallest = &uid;
        }
        return *smallest;
    }

}
