// Layer: Domain. See DimensionFormat.hpp for the Windows sources this file ports.
#include "DimensionFormat.hpp"

#include "NumberText.hpp"
#include "PositionParser.hpp"

#include <cmath>
#include <cstddef>
#include <string>

#if defined(__clang__)
// Windows evaluates feet * 12 + inches + fraction as separate IEEE double operations (CPython floats, no FMA); clang on
// arm64 would otherwise fuse a * b + c.
#pragma STDC FP_CONTRACT OFF
#endif

namespace ost_domain
{
    namespace
    {
        using u32 = std::u32string;

        struct Span
        {
            size_t begin = 0;
            size_t end = 0;
        };

        // str.strip()
        u32 strip(const u32 &s)
        {
            size_t b = 0, e = s.size();
            while (b < e && py_is_space(s[b]))
                ++b;
            while (e > b && py_is_space(s[e - 1]))
                --e;
            return s.substr(b, e - b);
        }

        std::string encode_utf8(const u32 &s, size_t begin, size_t end)
        {
            std::string out;
            for (size_t i = begin; i < end; ++i)
            {
                const char32_t c = s[i];
                if (c < 0x80)
                {
                    out.push_back(static_cast<char>(c));
                }
                else if (c < 0x800)
                {
                    out.push_back(static_cast<char>(0xC0 | (c >> 6)));
                    out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                }
                else if (c < 0x10000)
                {
                    out.push_back(static_cast<char>(0xE0 | (c >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                }
                else
                {
                    out.push_back(static_cast<char>(0xF0 | (c >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                    out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                }
            }
            return out;
        }

        // _finite_float(text): float(text) when it is finite, else None (ValueError, nan, inf, overflow)
        bool finite_float(const std::string &utf8, double &out)
        {
            double value = 0.0;
            if (!parse_python_float(utf8, value) || !std::isfinite(value))
                return false;
            out = value;
            return true;
        }

        bool finite_float(const u32 &text, Span span, double &out)
        {
            return finite_float(encode_utf8(text, span.begin, span.end), out);
        }

        // _finite_float(number) for a computed value
        bool finite_number(double value, double &out)
        {
            if (!std::isfinite(value))
                return false;
            out = value;
            return true;
        }

        // _FRACTION_MAP.get(text): only the seven ASCII eighths
        bool parse_fraction(const u32 &text, Span span, double &out)
        {
            const std::string key = encode_utf8(text, span.begin, span.end);
            if (key == "1/8") out = 0.125;
            else if (key == "1/4") out = 0.25;
            else if (key == "3/8") out = 0.375;
            else if (key == "1/2") out = 0.5;
            else if (key == "5/8") out = 0.625;
            else if (key == "3/4") out = 0.75;
            else if (key == "7/8") out = 0.875;
            else return false;
            return true;
        }

        // A cursor over the stripped text for the anchored layouts of dimension_format_service.py. The Windows regexes are
        // unambiguous token sequences (\s* between tokens, \d+ runs, single-digit fractions), so a left-to-right scan
        // accepts exactly the strings they accept and captures the same groups. \s is str.isspace(), \d is str.isdecimal().
        class Scanner
        {
        public:
            explicit Scanner(const u32 &text) : _text(text) {}

            bool at_end() const { return _pos == _text.size(); }

            // \s*
            void space()
            {
                while (_pos < _text.size() && py_is_space(_text[_pos]))
                    ++_pos;
            }
            // \s+
            bool space_required()
            {
                const size_t before = _pos;
                space();
                return _pos > before;
            }
            bool accept(char32_t c)
            {
                if (_pos < _text.size() && _text[_pos] == c)
                {
                    ++_pos;
                    return true;
                }
                return false;
            }
            // \d+
            bool digits(Span &out)
            {
                const size_t begin = _pos;
                while (_pos < _text.size() && py_decimal_value(_text[_pos]) >= 0)
                    ++_pos;
                if (_pos == begin)
                    return false;
                out = {begin, _pos};
                return true;
            }
            // \d+(?:\.\d+)?
            bool decimal(Span &out)
            {
                if (!digits(out))
                    return false;
                if (_pos < _text.size() && _text[_pos] == U'.')
                {
                    ++_pos;
                    Span fraction;
                    if (!digits(fraction))
                        return false; // "4." leaves the dot behind, which no layout accepts
                    out.end = fraction.end;
                }
                return true;
            }
            // \d/\d
            bool fraction(Span &out)
            {
                if (_pos + 3 > _text.size() || py_decimal_value(_text[_pos]) < 0 || _text[_pos + 1] != U'/' ||
                    py_decimal_value(_text[_pos + 2]) < 0)
                    return false;
                out = {_pos, _pos + 3};
                _pos += 3;
                return true;
            }
            // (-?)\s*
            bool sign()
            {
                const bool negative = accept(U'-');
                space();
                return negative;
            }
            // \s*"?\s*$
            bool closing()
            {
                space();
                accept(U'"');
                space();
                return at_end();
            }
            // \s*'
            bool feet_marker()
            {
                space();
                return accept(U'\'');
            }
            // \s*'\s*-?\s*
            bool feet_marker_and_gap()
            {
                if (!feet_marker())
                    return false;
                space();
                accept(U'-');
                space();
                return true;
            }
            // \s*$
            bool only_space_left()
            {
                space();
                return at_end();
            }

        private:
            const u32 &_text;
            size_t _pos = 0;
        };

        // display_to_inches(text) for imperial text that is already stripped and not blank. The first layout whose syntax
        // matches decides the result, as in the Windows if-chain (a matching layout with an invalid part returns None,
        // it does not fall through to the next layout).
        std::optional<double> imperial_to_inches(const u32 &text)
        {
            double out = 0.0;
            Span feet, inches, frac;
            bool negative = false;

            // 1. feet, inches and a fraction:  -?\s*\d+\s*'\s*-?\s*\d+\s+\d/\d\s*"?\s*$
            {
                Scanner s(text);
                negative = s.sign();
                if (s.digits(feet) && s.feet_marker_and_gap() && s.digits(inches) && s.space_required() &&
                    s.fraction(frac) && s.closing())
                {
                    const double sign = negative ? -1.0 : 1.0;
                    double feet_value = 0.0, whole = 0.0, fraction_value = 0.0;
                    if (!finite_float(text, feet, feet_value) || !finite_float(text, inches, whole) ||
                        !parse_fraction(text, frac, fraction_value))
                        return std::nullopt;
                    if (!finite_number(sign * (feet_value * 12.0 + whole + fraction_value), out))
                        return std::nullopt;
                    return out;
                }
            }
            // 2. feet and inches (decimals allowed):  -?\s*\d+\s*'\s*-?\s*\d+(\.\d+)?\s*"?\s*$
            {
                Scanner s(text);
                negative = s.sign();
                if (s.digits(feet) && s.feet_marker_and_gap() && s.decimal(inches) && s.closing())
                {
                    const double sign = negative ? -1.0 : 1.0;
                    double feet_value = 0.0, inches_value = 0.0;
                    if (!finite_float(text, feet, feet_value) || !finite_float(text, inches, inches_value))
                        return std::nullopt;
                    if (!finite_number(sign * (feet_value * 12.0 + inches_value), out))
                        return std::nullopt;
                    return out;
                }
            }
            // 3. feet and a fraction:  -?\s*\d+\s*'\s*-?\s*\d/\d\s*"?\s*$
            {
                Scanner s(text);
                negative = s.sign();
                if (s.digits(feet) && s.feet_marker_and_gap() && s.fraction(frac) && s.closing())
                {
                    const double sign = negative ? -1.0 : 1.0;
                    double feet_value = 0.0, fraction_value = 0.0;
                    if (!finite_float(text, feet, feet_value) || !parse_fraction(text, frac, fraction_value))
                        return std::nullopt;
                    if (!finite_number(sign * (feet_value * 12.0 + fraction_value), out))
                        return std::nullopt;
                    return out;
                }
            }
            // 4. feet only:  -?\s*\d+\s*'\s*$
            {
                Scanner s(text);
                negative = s.sign();
                if (s.digits(feet) && s.feet_marker() && s.only_space_left())
                {
                    const double sign = negative ? -1.0 : 1.0;
                    double feet_value = 0.0;
                    if (!finite_float(text, feet, feet_value))
                        return std::nullopt;
                    if (!finite_number(sign * feet_value * 12.0, out))
                        return std::nullopt;
                    return out;
                }
            }
            // 5. inches and a fraction:  -?\s*\d+\s+\d/\d\s*"?\s*$
            {
                Scanner s(text);
                negative = s.sign();
                if (s.digits(inches) && s.space_required() && s.fraction(frac) && s.closing())
                {
                    const double sign = negative ? -1.0 : 1.0;
                    double whole = 0.0, fraction_value = 0.0;
                    if (!finite_float(text, inches, whole) || !parse_fraction(text, frac, fraction_value))
                        return std::nullopt;
                    if (!finite_number(sign * (whole + fraction_value), out))
                        return std::nullopt;
                    return out;
                }
            }
            // 6. a fraction only:  -?\s*\d/\d\s*"?\s*$
            {
                Scanner s(text);
                negative = s.sign();
                if (s.fraction(frac) && s.closing())
                {
                    const double sign = negative ? -1.0 : 1.0;
                    double fraction_value = 0.0;
                    if (!parse_fraction(text, frac, fraction_value))
                        return std::nullopt;
                    return sign * fraction_value;
                }
            }
            // 7. inches only:  -?\s*\d+(\.\d+)?\s*"?\s*$
            {
                Scanner s(text);
                negative = s.sign();
                if (s.decimal(inches) && s.closing())
                {
                    const double sign = negative ? -1.0 : 1.0;
                    double inches_value = 0.0;
                    if (!finite_float(text, inches, inches_value))
                        return std::nullopt;
                    return sign * inches_value;
                }
            }
            // anything else: float(text) such as "1e2", ".5", "+7", "5.", "1_000"
            if (finite_float(text, {0, text.size()}, out))
                return out;
            return std::nullopt;
        }

        // The digits of the (integer valued, >= 0) number of eighths divided by 96 = eighths per foot: the quotient is
        // the feet (as decimal text, exact for any size) and the remainder splits into whole inches and eighths.
        void split_eighths(const std::string &digits, std::string &feet, int &whole_inches, int &eighths)
        {
            feet.clear();
            int remainder = 0;
            for (char c : digits)
            {
                remainder = remainder * 10 + (c - '0');
                const int quotient_digit = remainder / 96;
                remainder %= 96;
                if (!feet.empty() || quotient_digit != 0)
                    feet.push_back(static_cast<char>('0' + quotient_digit));
            }
            if (feet.empty())
                feet = "0";
            whole_inches = remainder / 8;
            eighths = remainder % 8;
        }
    }

    double mm_to_inches(double mm)
    {
        return mm / DIM_MM_PER_INCH;
    }

    double inches_to_mm(double inches)
    {
        return inches * DIM_MM_PER_INCH;
    }

    std::string inches_to_display(double inches, bool metric)
    {
        if (!std::isfinite(inches))
            return "";
        if (metric)
            return mm_to_display(inches_to_mm(inches));
        if (inches == 0.0)
            return "";
        const char *sign = (inches < 0) ? "-" : "";
        const double scaled = std::fabs(inches) * 8.0; // exact (power of two) unless it overflows
        if (!std::isfinite(scaled))
            return ""; // Windows: OverflowError from round(inf)
        std::string feet;
        int whole_inches = 0;
        int eighths = 0;
        split_eighths(integer_digits(round_half_even(scaled)), feet, whole_inches, eighths);

        static const char *const kEighthsFractions[] = {"", "1/8", "1/4", "3/8", "1/2", "5/8", "3/4", "7/8"};
        const std::string fraction = kEighthsFractions[eighths];
        const std::string inch_text = std::to_string(whole_inches);
        if (feet != "0")
        {
            if (whole_inches > 0 && !fraction.empty())
                return sign + feet + "' " + inch_text + " " + fraction + "\"";
            if (whole_inches > 0)
                return sign + feet + "' " + inch_text + "\"";
            if (!fraction.empty())
                return sign + feet + "' 0 " + fraction + "\"";
            return sign + feet + "' 0\"";
        }
        if (whole_inches > 0 && !fraction.empty())
            return sign + inch_text + " " + fraction + "\"";
        if (whole_inches > 0)
            return sign + inch_text + "\"";
        if (!fraction.empty())
            return sign + fraction + "\"";
        return "";
    }

    std::string mm_to_display(double mm)
    {
        if (!std::isfinite(mm))
            return "";
        if (mm == 0.0)
            return "";
        const double whole = std::trunc(mm);
        if (mm == whole) // mm == int(mm)
            return (mm < 0 ? "-" : "") + integer_digits(std::fabs(mm));
        return format_double("%g", mm); // f"{mm:g}"
    }

    std::optional<double> display_to_mm(const std::string &text)
    {
        const u32 stripped = strip(py_decode_utf8(text));
        if (stripped.empty())
            return 0.0;
        size_t end = stripped.size();
        while (end > 0 && stripped[end - 1] == U'm')
            --end;
        while (end > 0 && stripped[end - 1] == U'M')
            --end;
        const u32 cleaned = strip(stripped.substr(0, end));
        double value = 0.0;
        if (!finite_float(cleaned, {0, cleaned.size()}, value))
            return std::nullopt;
        return value;
    }

    std::optional<double> display_to_inches(const std::string &text, bool metric)
    {
        if (metric)
        {
            const std::optional<double> mm = display_to_mm(text);
            if (!mm.has_value())
                return std::nullopt;
            return mm_to_inches(*mm);
        }
        const u32 stripped = strip(py_decode_utf8(text));
        if (stripped.empty())
            return 0.0;
        return imperial_to_inches(stripped);
    }

    std::string plain_number_text(double value)
    {
        if (value == 0.0)
            return "";
        return python_float_repr(value);
    }

    std::string display_size_text(double value)
    {
        if (value == 0.0 || !std::isfinite(value))
            return "";
        const double whole = std::trunc(value); // int(value)
        const std::string digits = integer_digits(std::fabs(whole));
        if (digits == "0")
            return "0";
        return (whole < 0 ? "-" : "") + digits;
    }

    PlainNumberStatus parse_plain_number(const std::string &text, double &out)
    {
        double value = 0.0;
        if (!parse_python_float(text, value))
            return PlainNumberStatus::NotANumber;
        if (!std::isfinite(value))
            return PlainNumberStatus::NotFinite;
        out = value;
        return PlainNumberStatus::Ok;
    }
}
