// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
#include <string>

namespace ost_domain
{

    struct Layer
    {
        std::string uid;
        std::string name;
        bool visible = true;
        int sequence = 0;
    };

}
