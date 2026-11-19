#include "export_text.hpp"
#include "UnicodeDigitData.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ost_geometry
{
    namespace export_text
    {

        namespace
        {
            // One code point of a UTF-8 string starting at `i`: returns its length in bytes and sets `cp`. Anything that is
            // not well-formed UTF-8 (stray continuation byte, truncated or overlong sequence, surrogate, > U+10FFFF) is
            // one byte and U+FFFD, the way a decoder with the 'replace' policy consumes it.
            size_t decode_utf8(const std::string &s, size_t i, uint32_t &cp)
            {
                const unsigned char c = static_cast<unsigned char>(s[i]);
                if (c < 0x80)
                {
                    cp = c;
                    return 1;
                }
                size_t len = 0;
                uint32_t min_cp = 0;
                if (c >= 0xC2 && c <= 0xDF)
                {
                    len = 2;
                    cp = c & 0x1F;
                    min_cp = 0x80;
                }
                else if (c >= 0xE0 && c <= 0xEF)
                {
                    len = 3;
                    cp = c & 0x0F;
                    min_cp = 0x800;
                }
                else if (c >= 0xF0 && c <= 0xF4)
                {
                    len = 4;
                    cp = c & 0x07;
                    min_cp = 0x10000;
                }
                else
                {
                    cp = 0xFFFD;
                    return 1;
                }
                if (i + len > s.size())
                {
                    cp = 0xFFFD;
                    return 1;
                }
                for (size_t k = 1; k < len; ++k)
                {
                    const unsigned char cc = static_cast<unsigned char>(s[i + k]);
                    if ((cc & 0xC0) != 0x80)
                    {
                        cp = 0xFFFD;
                        return 1;
                    }
                    cp = (cp << 6) | (cc & 0x3F);
                }
                if (cp < min_cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
                {
                    cp = 0xFFFD;
                    return 1;
                }
                return len;
            }

            // Python str.isdigit() of one character.
            bool is_python_digit(uint32_t cp)
            {
                for (const auto &r : unicode_digit_data::DIGIT_RANGES)
                {
                    if (cp < r[0])
                        return false; // the table is sorted
                    if (cp <= r[1])
                        return true;
                }
                return false;
            }

            void append_u_escape(std::string &out, unsigned unit)
            {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%04x", unit);
                out += '\\';
                out += 'u';
                out += buf;
            }

            // "%.*e" of `value` with `digits` significant digits reads back as `value`.
            bool round_trips(double value, int significant, char *buf, size_t size)
            {
                std::snprintf(buf, size, "%.*e", significant - 1, value);
                return std::strtod(buf, nullptr) == value;
            }

            // `digits` (a decimal digit string) plus one in the last place; the result is longer by one digit on a carry out.
            std::string increment_digits(std::string digits)
            {
                for (size_t i = digits.size(); i-- > 0;)
                {
                    if (digits[i] == '9')
                    {
                        digits[i] = '0';
                        continue;
                    }
                    ++digits[i];
                    return digits;
                }
                return "1" + digits;
            }

            // The digits and the decimal exponent (value = d.ddd * 10^exp10) repr() prints for a positive power of two.
            // Below a power of two the doubles are twice as dense as above it, so the rounding interval is lopsided: the
            // correctly rounded p-digit decimal can fall outside its narrow lower half while the other p-digit neighbour of
            // the value is inside (2^595 prints as 6.483618076376552e+178, not as ...551). repr() takes, among the shortest
            // strings that read back as the value, the one nearest to it; so for every length try the nearer of the two
            // p-digit neighbours of the exact expansion first, then the other.
            void shortest_digits_of_power_of_two(double value, std::string &digits, int &exp10)
            {
                char exact[160];
                std::snprintf(exact, sizeof(exact), "%.80e", value);
                std::string all;
                const char *p = exact;
                while (*p && *p != 'e')
                {
                    if (*p >= '0' && *p <= '9')
                        all += *p;
                    ++p;
                }
                const int exact_exp10 = std::atoi(p + 1);
                for (size_t len = 1; len <= 17; ++len)
                {
                    const std::string down = all.substr(0, len);
                    const std::string up = increment_digits(down);
                    // an exact tie goes to the even last digit, like the correctly rounded printf and Python's repr
                    const bool tie = all[len] == '5' && all.find_first_not_of('0', len + 1) == std::string::npos;
                    const bool nearer_is_up = tie ? ((down.back() - '0') % 2 != 0) : all[len] >= '5';
                    const std::string *candidates[2] = {nearer_is_up ? &up : &down, nearer_is_up ? &down : &up};
                    for (const std::string *c : candidates)
                    {
                        const bool carried = c->size() > len;
                        const int e = exact_exp10 + (carried ? 1 : 0);
                        std::string text = c->substr(0, 1) + "." + c->substr(1) + "e" + std::to_string(e);
                        if (std::strtod(text.c_str(), nullptr) == value)
                        {
                            digits = *c;
                            exp10 = e;
                            return;
                        }
                    }
                }
                digits = all.substr(0, 17);
                exp10 = exact_exp10;
            }
        } // namespace

        std::string sanitize_material_name(const std::string &name)
        {
            std::string out;
            out.reserve(name.size());
            for (char c : name)
            {
                switch (c)
                {
                case ' ':
                case '/':
                case '\\':
                case ':':
                case ';':
                case '*':
                case '?':
                case '<':
                case '>':
                case '|':
                case '.':
                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                    out += '_';
                    break;
                case '"':
                case '\'':
                case ',':
                    break;
                default:
                    // Not ported, kept by user decision 2026-11-19 (EXP-12, see PORTING_BLOCKERS.md): Windows keeps control
                    // characters, so a name with a line break or a tab splits the 'newmtl' / 'usemtl' / 'g' line of the OBJ / MTL
                    // (or the DXF layer name) into several statements. iOS has always dropped them from these names and still does.
                    if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F)
                        break;
                    out += c;
                }
            }
            if (!out.empty())
            {
                uint32_t first = 0;
                decode_utf8(out, 0, first);
                if (is_python_digit(first))
                    out.insert(0, "Mat_");
            }
            return out.empty() ? std::string("Unknown") : out;
        }

        std::string python_float_repr(double value)
        {
            if (std::isnan(value))
                return "NaN";
            if (std::isinf(value))
                return value < 0 ? "-Infinity" : "Infinity";
            if (value == 0.0)
                return std::signbit(value) ? "-0.0" : "0.0";

            // The shortest digit string that reads back as `value`, which is what repr() prints (David Gay's mode 0). The
            // interval of the doubles that read as `value` is symmetric (except at a power of two), so the correctly
            // rounded decimal with a given number of digits, the closest one, round-trips whenever any decimal of that
            // length does; and a longer string round-trips whenever a shorter one does. The length is therefore found by
            // bisection. A power of two has its own search (see shortest_digits_of_power_of_two).
            const bool negative = std::signbit(value);
            const double magnitude = std::fabs(value);
            std::string digits;
            int exp10 = 0;
            int frexp_exp = 0;
            if (std::frexp(magnitude, &frexp_exp) == 0.5)
            {
                shortest_digits_of_power_of_two(magnitude, digits, exp10);
            }
            else
            {
                char buf[48];
                int lo = 1, hi = 17;
                while (lo < hi)
                {
                    const int mid = (lo + hi) / 2;
                    if (round_trips(magnitude, mid, buf, sizeof(buf)))
                        hi = mid;
                    else
                        lo = mid + 1;
                }
                std::snprintf(buf, sizeof(buf), "%.*e", hi - 1, magnitude);
                // buf = d[.ddd]e[+-]XX: the digits and the decimal exponent
                const char *p = buf;
                while (*p && *p != 'e' && *p != 'E')
                {
                    if (*p >= '0' && *p <= '9')
                        digits += *p;
                    ++p;
                }
                exp10 = (*p == 'e' || *p == 'E') ? std::atoi(p + 1) : 0;
            }
            while (digits.size() > 1 && digits.back() == '0')
                digits.pop_back();
            const int decpt = exp10 + 1; // value = 0.DIGITS * 10^decpt
            const int n = static_cast<int>(digits.size());

            std::string out = negative ? "-" : "";
            if (decpt <= -4 || decpt > 16)
            {
                // exponent form: d[.ddd]e+XX with at least two exponent digits
                out += digits[0];
                if (n > 1)
                {
                    out += '.';
                    out.append(digits, 1, std::string::npos);
                }
                const int e = decpt - 1;
                char eb[16];
                std::snprintf(eb, sizeof(eb), "e%c%02d", e < 0 ? '-' : '+', e < 0 ? -e : e);
                out += eb;
            }
            else if (decpt <= 0)
            {
                out += "0.";
                out.append(static_cast<size_t>(-decpt), '0');
                out += digits;
            }
            else if (n <= decpt)
            {
                out += digits;
                out.append(static_cast<size_t>(decpt - n), '0');
                out += ".0";
            }
            else
            {
                out.append(digits, 0, static_cast<size_t>(decpt));
                out += '.';
                out.append(digits, static_cast<size_t>(decpt), std::string::npos);
            }
            return out;
        }

        std::string json_string(const std::string &utf8)
        {
            std::string out = "\"";
            size_t i = 0;
            while (i < utf8.size())
            {
                uint32_t cp = 0;
                i += decode_utf8(utf8, i, cp);
                switch (cp)
                {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '<': append_u_escape(out, 0x3C); break;
                default:
                    if (cp >= 0x20 && cp <= 0x7E)
                        out += static_cast<char>(cp);
                    else if (cp > 0xFFFF)
                    {
                        const uint32_t v = cp - 0x10000;
                        append_u_escape(out, 0xD800 + (v >> 10));
                        append_u_escape(out, 0xDC00 + (v & 0x3FF));
                    }
                    else
                        append_u_escape(out, cp);
                }
            }
            out += '"';
            return out;
        }

        std::string html_escape(const std::string &text)
        {
            std::string out;
            out.reserve(text.size());
            for (char c : text)
            {
                switch (c)
                {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                default: out += c;
                }
            }
            return out;
        }

        double colour_channel(float value)
        {
            const double k = std::floor(static_cast<double>(value) * 255.0 + 0.5);
            if (k >= 0.0 && k <= 255.0 && static_cast<float>(k) / 255.0f == value)
                return k / 255.0;
            return static_cast<double>(value);
        }

        double shortest_double_of(float value)
        {
            if (!std::isfinite(value))
                return static_cast<double>(value);
            char buf[32];
            for (int p = 1; p <= 9; ++p)
            {
                std::snprintf(buf, sizeof(buf), "%.*g", p, static_cast<double>(value));
                if (std::strtof(buf, nullptr) == value)
                    return std::strtod(buf, nullptr);
            }
            return static_cast<double>(value);
        }

    } // namespace export_text
} // namespace ost_geometry
