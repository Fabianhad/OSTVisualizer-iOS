#pragma once
#include <string>
#include <vector>

namespace ost_domain
{

    struct Takeoff
    {
        std::string uid;
        std::string condition_uid;
        std::string page_uid;
        std::string area_uid = "0";

        std::vector<double> position;

        double rotation = 0.0;
        int curve = -1;

        std::string parent_uid = "0";
        bool is_negative = false;
        bool is_hole() const
        {
            return !parent_uid.empty() && parent_uid != "0";
        }

        bool has_valid_position() const
        {
            return position.size() >= 4;
        }
    };

}
