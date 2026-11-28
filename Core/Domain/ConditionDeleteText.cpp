// Layer: Domain. The contract is in ConditionDeleteText.hpp.
#include "ConditionDeleteText.hpp"

namespace ost_domain
{
    std::string condition_delete_title(const std::string &name)
    {
        return "Delete \"" + name + "\"?";
    }

    std::string condition_delete_message(int condition_count)
    {
        const std::string closing =
            "takeoffs on all pages within this bid.\n\nThis cannot be undone.\n\nAre you sure you want to delete this condition?";
        if (condition_count == 1)
            return "Deleting this condition will delete all its " + closing;
        return "You are deleting " + std::to_string(condition_count) + " conditions. Deleting a condition will delete all its " +
               closing;
    }
}
