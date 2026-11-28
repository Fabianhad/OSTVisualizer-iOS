// Layer: Domain. The contract is in ConditionQuantityChoices.hpp.
#include "ConditionQuantityChoices.hpp"
#include "UomService.hpp"

namespace ost_domain
{
    std::optional<int> unknown_quantity_calc_type(ConditionType style, int calc_type)
    {
        if (calc_type == 0)
            return std::nullopt;
        for (const auto &option : get_quantity_options_for_type(style))
            if (option.first == calc_type)
                return std::nullopt;
        return calc_type;
    }

    std::string unknown_quantity_label(int calc_type)
    {
        return "Unknown (" + std::to_string(calc_type) + ")";
    }

    std::string condition_quantity_label(ConditionType style, int calc_type)
    {
        if (calc_type == 0)
            return "(none)";
        for (const auto &option : get_quantity_options_for_type(style))
            if (option.first == calc_type)
                return option.second;
        return unknown_quantity_label(calc_type);
    }
}
