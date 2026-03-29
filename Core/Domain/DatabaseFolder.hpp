// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
#include <string>

namespace ost_domain
{

    struct DatabaseFolder
    {
        std::string uid;
        std::string name;
    };

}
