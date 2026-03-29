// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
#include "../Domain/Page.hpp"
#include "../Domain/Condition.hpp"
#include "../Domain/Takeoff.hpp"
#include "../Domain/Folder.hpp"
#include "../Domain/Layer.hpp"
#include "../Domain/CdnType.hpp"
#include "../Domain/ConditionFolder.hpp"
#include "../Domain/Bid.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace ost_data
{

    struct ProjectData
    {
        ost_domain::Bid bid;
        std::string project_name;
        std::string bid_name;
        std::unordered_map<std::string, ost_domain::Page> pages;
        std::unordered_map<std::string, ost_domain::Condition> conditions;
        std::unordered_map<std::string, ost_domain::Layer> layers;
        std::unordered_map<std::string, ost_domain::CdnType> cdn_types;
        std::unordered_map<std::string, ost_domain::ConditionFolder> condition_folders;
        std::unordered_map<std::string, ost_domain::Folder> folders;
        std::vector<ost_domain::Takeoff> takeoffs;

        bool empty() const { return bid.uid.empty(); }
        double page_scale_ratio(const std::string &page_uid) const
        {
            auto it = pages.find(page_uid);
            if (it == pages.end())
                return 1.0;
            double sf1 = it->second.scale_factor1;
            double sf2 = it->second.scale_factor2;
            if (sf1 == 0.0)
                return 1.0;
            return sf2 / sf1;
        }
        bool is_condition_visible(const ost_domain::Condition &cdn) const
        {
            if (cdn.layer_uid.empty())
                return true;
            auto it = layers.find(cdn.layer_uid);
            if (it == layers.end())
                return true;
            return it->second.visible;
        }
    };

}
