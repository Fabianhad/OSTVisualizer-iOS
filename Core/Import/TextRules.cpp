// Layer: Import (pure C++)
// See TextRules.hpp. The Unicode tables come from CPython 3.11.4 (the Windows app's interpreter), see
// UnicodeCaseData.hpp.
#include "TextRules.hpp"
#include "UnicodeCaseData.hpp"
#include "UnicodeDecimalData.hpp"
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace ost_import
{
    namespace text_rules
    {
        namespace
        {
            namespace data = unicode_case_data;

            // One decoded unit of the input: a code point, or one raw byte of an invalid UTF-8 sequence.
            struct Piece
            {
                uint32_t cp;
                bool raw;
                size_t start; // byte offset in the input
                size_t length;
            };

            std::vector<Piece> decode(const std::string &text)
            {
                std::vector<Piece> pieces;
                pieces.reserve(text.size());
                const size_t n = text.size();
                size_t i = 0;
                while (i < n)
                {
                    const unsigned char c = static_cast<unsigned char>(text[i]);
                    size_t len = 1;
                    uint32_t cp = c;
                    bool valid = true;
                    if (c >= 0x80)
                    {
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
                            valid = false;
                        }
                        if (valid && i + len > n)
                            valid = false;
                        for (size_t k = 1; valid && k < len; ++k)
                        {
                            const unsigned char cc = static_cast<unsigned char>(text[i + k]);
                            if ((cc & 0xC0) != 0x80)
                                valid = false;
                            else
                                cp = (cp << 6) | (cc & 0x3F);
                        }
                        if (valid && (cp < min_cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)))
                            valid = false;
                    }
                    if (valid)
                    {
                        pieces.push_back({cp, false, i, len});
                        i += len;
                    }
                    else
                    {
                        pieces.push_back({c, true, i, 1});
                        i += 1;
                    }
                }
                return pieces;
            }

            void encode(uint32_t cp, std::string &out)
            {
                if (cp < 0x80)
                {
                    out += static_cast<char>(cp);
                }
                else if (cp < 0x800)
                {
                    out += static_cast<char>(0xC0 | (cp >> 6));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                }
                else if (cp < 0x10000)
                {
                    out += static_cast<char>(0xE0 | (cp >> 12));
                    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                }
                else
                {
                    out += static_cast<char>(0xF0 | (cp >> 18));
                    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (cp & 0x3F));
                }
            }

            bool in_ranges(uint32_t cp, const data::Range *ranges, unsigned count)
            {
                unsigned lo = 0, hi = count;
                while (lo < hi)
                {
                    const unsigned mid = (lo + hi) / 2;
                    if (cp < ranges[mid].first)
                        hi = mid;
                    else if (cp > ranges[mid].last)
                        lo = mid + 1;
                    else
                        return true;
                }
                return false;
            }

            // Looks cp up in the single-code-point runs; true and *target set when it maps to something else.
            bool find_run(uint32_t cp, const data::Run *runs, unsigned count, uint32_t &target)
            {
                unsigned lo = 0, hi = count;
                while (lo < hi)
                {
                    const unsigned mid = (lo + hi) / 2;
                    if (cp < runs[mid].first)
                        hi = mid;
                    else if (cp > runs[mid].last)
                        lo = mid + 1;
                    else
                    {
                        if ((cp - runs[mid].first) % runs[mid].step != 0)
                            return false;
                        target = static_cast<uint32_t>(static_cast<int64_t>(cp) + runs[mid].delta);
                        return true;
                    }
                }
                return false;
            }

            const data::Multi *find_multi(uint32_t cp, const data::Multi *multi, unsigned count)
            {
                unsigned lo = 0, hi = count;
                while (lo < hi)
                {
                    const unsigned mid = (lo + hi) / 2;
                    if (cp < multi[mid].cp)
                        hi = mid;
                    else if (cp > multi[mid].cp)
                        lo = mid + 1;
                    else
                        return &multi[mid];
                }
                return nullptr;
            }

            bool is_space(const Piece &p)
            {
                return !p.raw && in_ranges(p.cp, data::SPACE_RANGES, data::SPACE_RANGES_COUNT);
            }

            bool is_cased(const Piece &p)
            {
                return !p.raw && in_ranges(p.cp, data::CASED_RANGES, data::CASED_RANGES_COUNT);
            }

            bool is_case_ignorable(const Piece &p)
            {
                return !p.raw && in_ranges(p.cp, data::CASE_IGNORABLE_RANGES, data::CASE_IGNORABLE_RANGES_COUNT);
            }

            // CPython handle_capital_sigma: U+03A3 lower-cases to the final sigma U+03C2 when a cased letter precedes
            // it (skipping case-ignorable characters) and none follows it (skipping case-ignorable characters).
            uint32_t lower_capital_sigma(const std::vector<Piece> &pieces, size_t i)
            {
                size_t j = i;
                bool preceded = false;
                while (j > 0)
                {
                    --j;
                    if (!is_case_ignorable(pieces[j]))
                    {
                        preceded = is_cased(pieces[j]);
                        break;
                    }
                }
                if (!preceded)
                    return 0x3C3;
                for (size_t k = i + 1; k < pieces.size(); ++k)
                {
                    if (!is_case_ignorable(pieces[k]))
                        return is_cased(pieces[k]) ? 0x3C3 : 0x3C2;
                }
                return 0x3C2;
            }

            std::string map_text(const std::string &text, bool fold)
            {
                const std::vector<Piece> pieces = decode(text);
                std::string out;
                out.reserve(text.size());
                for (size_t i = 0; i < pieces.size(); ++i)
                {
                    const Piece &p = pieces[i];
                    if (p.raw)
                    {
                        out.append(text, p.start, p.length);
                        continue;
                    }
                    if (!fold && p.cp == 0x3A3)
                    {
                        encode(lower_capital_sigma(pieces, i), out);
                        continue;
                    }
                    const data::Multi *multi = fold ? find_multi(p.cp, data::FOLD_MULTI, data::FOLD_MULTI_COUNT)
                                                    : find_multi(p.cp, data::LOWER_MULTI, data::LOWER_MULTI_COUNT);
                    if (multi)
                    {
                        for (uint32_t k = 0; k < multi->count; ++k)
                            encode(multi->out[k], out);
                        continue;
                    }
                    uint32_t target = p.cp;
                    if (fold)
                        find_run(p.cp, data::FOLD_RUNS, data::FOLD_RUNS_COUNT, target);
                    else
                        find_run(p.cp, data::LOWER_RUNS, data::LOWER_RUNS_COUNT, target);
                    encode(target, out);
                }
                return out;
            }

            bool is_digit(char c)
            {
                return c >= '0' && c <= '9';
            }

            // Value 0-9 of a Unicode decimal digit above ASCII (Py_UNICODE_TODECIMAL), -1 for every other code point.
            int decimal_value(uint32_t cp)
            {
                const uint32_t *starts = unicode_decimal_data::DIGIT_RUN_STARTS;
                unsigned lo = 0, hi = sizeof(unicode_decimal_data::DIGIT_RUN_STARTS) / sizeof(uint32_t);
                while (lo < hi)
                {
                    const unsigned mid = (lo + hi) / 2;
                    if (cp < starts[mid])
                        hi = mid;
                    else if (cp >= starts[mid] + 10)
                        lo = mid + 1;
                    else
                        return static_cast<int>(cp - starts[mid]);
                }
                return -1;
            }

            // How CPython prepares the text of int()/float() (_PyUnicode_TransformDecimalAndSpaceToASCII): a character
            // below 127 stays as it is (so \x1c..\x1f, which str.strip() removes, are not whitespace here), any other
            // Unicode whitespace becomes ' ', a Unicode decimal digit becomes its ASCII digit, and anything else makes
            // the text invalid (CPython writes '?' and stops). `valid` is false then.
            std::string numeric_ascii(const std::string &text, bool &valid)
            {
                valid = true;
                std::string out;
                out.reserve(text.size());
                for (const Piece &p : decode(text))
                {
                    if (p.raw)
                    {
                        valid = false;
                        return std::string();
                    }
                    if (p.cp < 127)
                    {
                        out += static_cast<char>(p.cp);
                    }
                    else if (is_space(p))
                    {
                        out += ' ';
                    }
                    else
                    {
                        const int digit = decimal_value(p.cp);
                        if (digit < 0)
                        {
                            valid = false;
                            return std::string();
                        }
                        out += static_cast<char>('0' + digit);
                    }
                }
                return out;
            }

            // Py_ISSPACE: the whitespace int()/float() skip around the number once the text is ASCII.
            bool is_ascii_space(char c)
            {
                return c == ' ' || (c >= '\t' && c <= '\r');
            }

            std::string trim_ascii_space(const std::string &s)
            {
                size_t first = 0;
                while (first < s.size() && is_ascii_space(s[first]))
                    ++first;
                size_t last = s.size();
                while (last > first && is_ascii_space(s[last - 1]))
                    --last;
                return s.substr(first, last - first);
            }

            // digits with single underscores between digits ("1_000"); false for anything else. Appends the digits
            // (without underscores) to out.
            bool read_digit_group(const std::string &s, size_t &pos, std::string &out)
            {
                if (pos >= s.size() || !is_digit(s[pos]))
                    return false;
                while (pos < s.size())
                {
                    if (is_digit(s[pos]))
                    {
                        out += s[pos++];
                    }
                    else if (s[pos] == '_' && pos + 1 < s.size() && is_digit(s[pos + 1]))
                    {
                        ++pos;
                    }
                    else
                    {
                        break;
                    }
                }
                return true;
            }
        }

        bool py_isdecimal(const std::string &text)
        {
            if (text.empty())
                return false;
            for (const Piece &p : decode(text))
            {
                if (p.raw)
                    return false;
                if (p.cp < 0x80)
                {
                    if (!is_digit(static_cast<char>(p.cp)))
                        return false;
                }
                else if (decimal_value(p.cp) < 0)
                {
                    return false;
                }
            }
            return true;
        }

        std::string py_strip(const std::string &text)
        {
            const std::vector<Piece> pieces = decode(text);
            size_t first = 0;
            while (first < pieces.size() && is_space(pieces[first]))
                ++first;
            if (first == pieces.size())
                return std::string();
            size_t last = pieces.size();
            while (last > first && is_space(pieces[last - 1]))
                --last;
            const size_t begin = pieces[first].start;
            const size_t end = pieces[last - 1].start + pieces[last - 1].length;
            return text.substr(begin, end - begin);
        }

        std::vector<std::string> py_split(const std::string &text)
        {
            const std::vector<Piece> pieces = decode(text);
            std::vector<std::string> parts;
            size_t i = 0;
            while (i < pieces.size())
            {
                if (is_space(pieces[i]))
                {
                    ++i;
                    continue;
                }
                const size_t first = i;
                while (i < pieces.size() && !is_space(pieces[i]))
                    ++i;
                const size_t begin = pieces[first].start;
                const size_t end = pieces[i - 1].start + pieces[i - 1].length;
                parts.push_back(text.substr(begin, end - begin));
            }
            return parts;
        }

        std::string py_lower(const std::string &text)
        {
            return map_text(text, false);
        }

        std::string py_casefold(const std::string &text)
        {
            return map_text(text, true);
        }

        NumberParse py_parse_int(const std::string &text, long long &out)
        {
            bool prepared = false;
            const std::string ascii = numeric_ascii(text, prepared);
            if (!prepared)
                return NumberParse::Invalid;
            const std::string s = trim_ascii_space(ascii);
            size_t pos = 0;
            bool negative = false;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-'))
            {
                negative = (s[pos] == '-');
                ++pos;
            }
            std::string digits;
            if (!read_digit_group(s, pos, digits) || pos != s.size())
                return NumberParse::Invalid;
            // CPython 3.11 (sys.int_info.default_max_str_digits): int() of more than 4300 digits (underscores do not
            // count, leading zeros do) is a ValueError, not a big integer. The Windows code then reads the text as
            // unparsable (NULL / 0) where a big integer would fail the INSERT.
            if (digits.size() > 4300)
                return NumberParse::Invalid;
            // Magnitude as unsigned 64-bit with overflow detection.
            unsigned long long magnitude = 0;
            for (char c : digits)
            {
                const unsigned d = static_cast<unsigned>(c - '0');
                if (magnitude > (0xFFFFFFFFFFFFFFFFull - d) / 10ull)
                    return NumberParse::Overflow;
                magnitude = magnitude * 10ull + d;
            }
            if (negative)
            {
                if (magnitude > 0x8000000000000000ull)
                    return NumberParse::Overflow;
                out = (magnitude == 0x8000000000000000ull) ? static_cast<long long>(0x8000000000000000ull)
                                                           : -static_cast<long long>(magnitude);
            }
            else
            {
                if (magnitude > 0x7FFFFFFFFFFFFFFFull)
                    return NumberParse::Overflow;
                out = static_cast<long long>(magnitude);
            }
            return NumberParse::Ok;
        }

        bool py_parse_float(const std::string &text, double &out)
        {
            bool prepared = false;
            const std::string ascii = numeric_ascii(text, prepared);
            if (!prepared)
                return false;
            const std::string s = trim_ascii_space(ascii);
            size_t pos = 0;
            std::string cleaned;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-'))
                cleaned += s[pos++];
            std::string integer_part;
            const bool has_integer = read_digit_group(s, pos, integer_part);
            cleaned += integer_part;
            bool has_fraction = false;
            if (pos < s.size() && s[pos] == '.')
            {
                ++pos;
                cleaned += '.';
                std::string fraction;
                has_fraction = read_digit_group(s, pos, fraction);
                cleaned += fraction;
            }
            if (!has_integer && !has_fraction)
                return false;
            if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E'))
            {
                ++pos;
                cleaned += 'e';
                if (pos < s.size() && (s[pos] == '+' || s[pos] == '-'))
                    cleaned += s[pos++];
                std::string exponent;
                if (!read_digit_group(s, pos, exponent))
                    return false;
                cleaned += exponent;
            }
            if (pos != s.size())
                return false;
            out = std::strtod(cleaned.c_str(), nullptr);
            return true;
        }
    }
}
