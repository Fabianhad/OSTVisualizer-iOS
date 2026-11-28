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

    // ---- Python formatters of Set Scale (plan 4.6). They do not use the C library for anything that Python decides differently: the digits
    // come from the EXACT decimal expansion of the binary value (little-endian base 1e9 limbs, no printf), so MSVC, AppleClang and CPython
    // agree on ties, on huge values and on subnormals, whatever the C library prints for %f / %g.
    namespace
    {
        // The exact decimal expansion of a finite double >= 0: `digits` is the integer part (at least one digit, no leading zeros except
        // the single "0") followed by the whole fraction; `int_len` is the length of the integer part. A double with a fraction has at most
        // 1074 fraction digits and the last one is not 0.
        struct ExactDecimal
        {
            std::string digits;
            size_t int_len = 1;
        };

        // value = limbs * m, limbs little-endian base 1e9
        void limbs_multiply(std::vector<unsigned long long> &limbs, unsigned long long multiplier)
        {
            constexpr unsigned long long kBase = 1000000000ULL;
            unsigned long long carry = 0;
            for (unsigned long long &limb : limbs)
            {
                const unsigned long long product = limb * multiplier + carry; // limb < 1e9, multiplier <= 5^13 < 2^31: the product stays below 2^61
                limb = product % kBase;
                carry = product / kBase;
            }
            while (carry > 0)
            {
                limbs.push_back(carry % kBase);
                carry /= kBase;
            }
        }

        std::string limbs_to_text(const std::vector<unsigned long long> &limbs)
        {
            if (limbs.empty())
                return "0";
            std::string text = std::to_string(limbs.back());
            for (size_t i = limbs.size() - 1; i-- > 0;)
            {
                const std::string part = std::to_string(limbs[i]);
                text.append(9 - part.size(), '0');
                text += part;
            }
            return text;
        }

        ExactDecimal exact_decimal(double value)
        {
            ExactDecimal result;
            value = std::fabs(value);
            if (value == 0.0)
            {
                result.digits = "0";
                result.int_len = 1;
                return result;
            }
            int exponent = 0;
            const double fraction = std::frexp(value, &exponent); // value = fraction * 2^exponent, fraction in [0.5, 1)
            unsigned long long mantissa = static_cast<unsigned long long>(std::ldexp(fraction, 53));
            int shift = exponent - 53; // value = mantissa * 2^shift
            while ((mantissa & 1ULL) == 0)
            {
                mantissa >>= 1;
                ++shift;
            }
            if (shift >= 0)
            {
                result.digits = integer_digits(value); // an integer-valued double: exact for any magnitude
                result.int_len = result.digits.size();
                return result;
            }
            // value = mantissa / 2^k = mantissa * 5^k / 10^k
            int k = -shift;
            std::vector<unsigned long long> limbs;
            for (unsigned long long m = mantissa; m > 0; m /= 1000000000ULL)
                limbs.push_back(m % 1000000000ULL);
            constexpr unsigned long long kFiveToThirteen = 1220703125ULL;
            for (int remaining = k; remaining > 0;)
            {
                const int step = remaining >= 13 ? 13 : remaining;
                unsigned long long multiplier = 1;
                for (int i = 0; i < step; ++i)
                    multiplier *= 5;
                limbs_multiply(limbs, step == 13 ? kFiveToThirteen : multiplier);
                remaining -= step;
            }
            const std::string scaled = limbs_to_text(limbs); // the integer mantissa * 5^k; the value is scaled / 10^k
            if (scaled.size() > static_cast<size_t>(k))
            {
                result.digits = scaled;
                result.int_len = scaled.size() - static_cast<size_t>(k);
            }
            else
            {
                result.digits = "0" + std::string(static_cast<size_t>(k) - scaled.size(), '0') + scaled;
                result.int_len = 1;
            }
            return result;
        }

        // Rounds the digit string to its first `keep` digits, a tie (the dropped digits are exactly "5000...") going to the even last digit
        // (Python rounds the exact binary value, so an exactly representable tie is the only tie there is). `digits` is padded with zeros
        // when it is shorter. `carry` is set when the rounding made the string one digit longer ("999" -> "1000").
        std::string round_digits(const std::string &digits, size_t keep, bool &carry)
        {
            carry = false;
            std::string kept = digits.size() >= keep ? digits.substr(0, keep) : digits + std::string(keep - digits.size(), '0');
            bool up = false;
            if (digits.size() > keep)
            {
                const char next = digits[keep];
                if (next > '5')
                    up = true;
                else if (next == '5')
                {
                    bool rest_is_zero = true;
                    for (size_t i = keep + 1; i < digits.size() && rest_is_zero; ++i)
                        rest_is_zero = digits[i] == '0';
                    if (!rest_is_zero)
                        up = true;
                    else
                        up = keep > 0 && ((kept[keep - 1] - '0') % 2 == 1);
                }
            }
            if (up)
            {
                size_t i = kept.size();
                while (i > 0 && kept[i - 1] == '9')
                    kept[--i] = '0';
                if (i == 0)
                {
                    kept.insert(kept.begin(), '1');
                    carry = true;
                }
                else
                    ++kept[i - 1];
            }
            return kept;
        }

        // "nan", "inf" or "-inf": Python never writes "-nan", "1.#INF" or "nan(ind)". Empty for a finite value.
        std::string non_finite_text(double value)
        {
            if (std::isnan(value))
                return "nan";
            if (std::isinf(value))
                return value < 0 ? "-inf" : "inf";
            return std::string();
        }
    }

    std::string format_general(double value, int precision)
    {
        const std::string special = non_finite_text(value);
        if (!special.empty())
            return special;
        const size_t digits_wanted = precision < 1 ? 1 : static_cast<size_t>(precision);
        const std::string sign = std::signbit(value) ? "-" : "";
        if (value == 0.0)
            return sign + "0";

        const ExactDecimal exact = exact_decimal(value);
        size_t first = 0;
        while (first < exact.digits.size() && exact.digits[first] == '0')
            ++first;
        int exponent10 = static_cast<int>(exact.int_len) - 1 - static_cast<int>(first); // value = d.ddd x 10^exponent10
        bool carry = false;
        std::string rounded = round_digits(exact.digits.substr(first), digits_wanted, carry);
        if (carry)
        {
            rounded.pop_back(); // "1000...0" one digit too long: the value is now 1 x 10^(exponent10 + 1)
            ++exponent10;
        }

        std::string body;
        if (exponent10 < -4 || exponent10 >= static_cast<int>(digits_wanted))
        {
            std::string mantissa = rounded.substr(0, 1);
            std::string fraction = rounded.substr(1);
            while (!fraction.empty() && fraction.back() == '0')
                fraction.pop_back();
            if (!fraction.empty())
                mantissa += "." + fraction;
            const int magnitude = exponent10 < 0 ? -exponent10 : exponent10;
            std::string exponent_text = std::to_string(magnitude);
            if (exponent_text.size() < 2)
                exponent_text.insert(exponent_text.begin(), '0');
            body = mantissa + "e" + (exponent10 < 0 ? "-" : "+") + exponent_text;
        }
        else if (exponent10 >= 0)
        {
            const size_t int_digits = static_cast<size_t>(exponent10) + 1;
            std::string fraction = rounded.substr(int_digits);
            while (!fraction.empty() && fraction.back() == '0')
                fraction.pop_back();
            body = rounded.substr(0, int_digits);
            if (!fraction.empty())
                body += "." + fraction;
        }
        else
        {
            std::string fraction = std::string(static_cast<size_t>(-exponent10 - 1), '0') + rounded;
            while (!fraction.empty() && fraction.back() == '0')
                fraction.pop_back();
            body = "0." + fraction;
        }
        return sign + body;
    }

    std::string format_g(double value)
    {
        return format_general(value, 6);
    }

    std::string format_fixed(double value, int decimals)
    {
        const std::string special = non_finite_text(value);
        if (!special.empty())
            return special;
        const size_t places = decimals < 0 ? 0 : static_cast<size_t>(decimals);
        const std::string sign = std::signbit(value) ? "-" : "";
        const ExactDecimal exact = exact_decimal(value);
        bool carry = false;
        const std::string rounded = round_digits(exact.digits, exact.int_len + places, carry);
        const size_t int_len = exact.int_len + (carry ? 1 : 0);
        std::string text = sign + rounded.substr(0, int_len);
        if (places > 0)
            text += "." + rounded.substr(int_len);
        return text;
    }

    std::string format_fixed_trimmed(double value, int decimals)
    {
        // Python: f"{round(v, d):.<d>f}".rstrip("0").rstrip("."). round() is correct rounding of the exact binary value and the result, formatted
        // with the same number of decimals, gives the same digits as formatting v directly (checked on the whole oracle table). The strip
        // is the literal Python one: with decimals == 0 the zeros of the integer part go too ("100" -> "1"); the callers use 3.
        std::string text = format_fixed(value, decimals);
        while (!text.empty() && text.back() == '0')
            text.pop_back();
        while (!text.empty() && text.back() == '.')
            text.pop_back();
        return text;
    }
}
