// Layer: Domain. See NumberText.hpp.
#include "NumberText.hpp"

#include <clocale>
#include <cmath>
#include <cstdio>
#include <vector>

namespace ost_domain
{
    double round_half_even(double x)
    {
        const double floor_value = std::floor(x);
        const double difference = x - floor_value; // exact: floor_value and x share their binade or floor_value is 0
        if (difference > 0.5)
            return floor_value + 1.0;
        if (difference < 0.5)
            return floor_value;
        return (std::fmod(floor_value, 2.0) == 0.0) ? floor_value : floor_value + 1.0;
    }

    std::string integer_digits(double x)
    {
        if (!(x >= 1.0))
            return "0";
        if (x < 18446744073709551616.0) // 2^64: an integer-valued double below it is an exact unsigned 64-bit value
            return std::to_string(static_cast<unsigned long long>(x));

        // x = mantissa * 2^shift with a 53-bit integer mantissa and shift >= 12: multiply the decimal digits
        // (little-endian limbs of base 1e9) by two, 29 bits at a time (limb * 2^29 + carry stays below 2^63)
        constexpr unsigned long long kBase = 1000000000ULL;
        int exponent = 0;
        const double fraction = std::frexp(x, &exponent);
        unsigned long long mantissa = static_cast<unsigned long long>(std::ldexp(fraction, 53));
        int shift = exponent - 53;
        std::vector<unsigned long long> limbs;
        while (mantissa > 0)
        {
            limbs.push_back(mantissa % kBase);
            mantissa /= kBase;
        }
        while (shift > 0)
        {
            const int step = shift > 29 ? 29 : shift;
            unsigned long long carry = 0;
            for (unsigned long long &limb : limbs)
            {
                const unsigned long long product = (limb << step) + carry;
                limb = product % kBase;
                carry = product / kBase;
            }
            while (carry > 0)
            {
                limbs.push_back(carry % kBase);
                carry /= kBase;
            }
            shift -= step;
        }
        std::string text = std::to_string(limbs.back());
        for (size_t i = limbs.size() - 1; i-- > 0;)
        {
            const std::string part = std::to_string(limbs[i]);
            text.append(9 - part.size(), '0');
            text += part;
        }
        return text;
    }

    std::string format_double(const char *format, double value)
    {
        const int length = std::snprintf(nullptr, 0, format, value);
        if (length < 0)
            return std::string();
        std::string text(static_cast<size_t>(length), '\0');
        std::snprintf(&text[0], text.size() + 1, format, value);
        const std::lconv *conv = std::localeconv();
        const std::string point = (conv && conv->decimal_point) ? conv->decimal_point : ".";
        if (!point.empty() && point != ".")
        {
            for (size_t pos = text.find(point); pos != std::string::npos; pos = text.find(point, pos + 1))
            {
                text.replace(pos, point.size(), ".");
            }
        }
        return text;
    }
}
