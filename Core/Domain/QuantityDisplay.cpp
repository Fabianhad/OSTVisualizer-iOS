// Layer: Domain. See QuantityDisplay.hpp for the Windows sources this file ports.
#include "QuantityDisplay.hpp"

#include "NumberText.hpp"

#include <cmath>

namespace ost_domain
{
    namespace
    {
        // The "," grouping of format(x, ",.0f"): every three digits of the integer part from the right
        std::string group_thousands(const std::string &digits)
        {
            std::string out;
            out.reserve(digits.size() + digits.size() / 3);
            for (size_t i = 0; i < digits.size(); ++i)
            {
                if (i > 0 && (digits.size() - i) % 3 == 0)
                    out.push_back(',');
                out.push_back(digits[i]);
            }
            return out;
        }
    }

    std::string format_quantity_number(double value, int uom_code, const std::string &zero_text)
    {
        if (value == 0.0)
            return zero_text;
        if (std::isnan(value))
            return "nan";
        if (std::isinf(value))
            return value < 0 ? "-inf" : "inf";

        const double magnitude = std::fabs(value);
        std::string body;
        if (is_metric_uom(uom_code) && magnitude < 100.0)
            body = format_double("%.2f", magnitude); // f"{value:,.2f}": below 1000, so there is nothing to group
        else
            body = group_thousands(integer_digits(round_half_even(magnitude))); // f"{value:,.0f}"
        // the sign survives rounding to zero: -0.4 -> "-0" (Python keeps it, so does the Windows list)
        return std::signbit(value) ? "-" + body : body;
    }

    std::string format_quantity_with_uom(double value, int uom_code, const UomLabelFn &uom_label_fn)
    {
        const std::string label = uom_label_fn(uom_code);
        const std::string number = format_quantity_number(value, uom_code);
        if (!label.empty())
            return (number.empty() ? std::string("0") : number) + " " + label;
        return number;
    }

    std::string format_quantity_with_uom(double value, int uom_code)
    {
        return format_quantity_with_uom(value, uom_code, [](int code) { return get_uom_label(code); });
    }

    std::array<std::string, 3> format_condition_quantity_texts(const QuantityResult *quantities, int uom1, int uom2,
                                                               int uom3)
    {
        const double q1 = quantities ? quantities->q1 : 0.0;
        const double q2 = quantities ? quantities->q2 : 0.0;
        const double q3 = quantities ? quantities->q3 : 0.0;
        return {format_quantity_with_uom(q1, uom1), format_quantity_with_uom(q2, uom2),
                format_quantity_with_uom(q3, uom3)};
    }
}
