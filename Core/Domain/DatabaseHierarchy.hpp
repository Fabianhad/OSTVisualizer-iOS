// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
#include "DatabaseFolder.hpp"
#include <string>
#include <vector>

namespace ost_domain
{

    struct BidSummary
    {
        std::string bid_uid;
        std::string name;
        std::string folder_uid;
    };

    struct FolderGroup
    {
        DatabaseFolder folder;
        std::vector<BidSummary> bids;
    };

    struct DatabaseHierarchy
    {
        std::vector<FolderGroup> folders;
        std::vector<BidSummary> orphan_bids;
    };

}
