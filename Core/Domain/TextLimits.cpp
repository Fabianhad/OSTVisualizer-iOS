// Layer: Domain. The text-limit helpers of the M1 writers (see TextLimits.hpp for the contract and the reasons).
#include "TextLimits.hpp"

namespace ost_domain
{

    namespace
    {
        constexpr char32_t kInvalid = 0xFFFFFFFF;

        bool is_continuation(unsigned char b) { return b >= 0x80 && b <= 0xBF; }

        // Decodes the code point that starts at `text[i]` and moves `i` past it. Returns kInvalid (and leaves `i` where it was) when the
        // bytes there are not a valid UTF-8 sequence: the "well-formed UTF-8 byte sequences" table of the Unicode standard (RFC 3629), the same strictness as Python's
        // bytes.decode("utf-8"). The byte ranges of the second byte are what exclude the overlong forms (E0 80..9F, F0 80..8F), the
        // surrogates (ED A0..BF) and everything above U+10FFFF (F4 90..BF); the leads C0, C1 and F5..FF never start a sequence.
        char32_t next_code_point(const std::string &text, std::size_t &i)
        {
            const std::size_t size = text.size();
            const unsigned char b0 = static_cast<unsigned char>(text[i]);
            if (b0 < 0x80)
            {
                i += 1;
                return b0;
            }

            std::size_t extra = 0; // continuation bytes after the lead
            char32_t cp = 0;
            unsigned char lo = 0x80; // allowed range of the SECOND byte
            unsigned char hi = 0xBF;
            if (b0 >= 0xC2 && b0 <= 0xDF)
            {
                extra = 1;
                cp = b0 & 0x1F;
            }
            else if (b0 >= 0xE0 && b0 <= 0xEF)
            {
                extra = 2;
                cp = b0 & 0x0F;
                if (b0 == 0xE0)
                    lo = 0xA0; // not overlong
                else if (b0 == 0xED)
                    hi = 0x9F; // not a surrogate
            }
            else if (b0 >= 0xF0 && b0 <= 0xF4)
            {
                extra = 3;
                cp = b0 & 0x07;
                if (b0 == 0xF0)
                    lo = 0x90; // not overlong
                else if (b0 == 0xF4)
                    hi = 0x8F; // not above U+10FFFF
            }
            else
            {
                return kInvalid; // a stray continuation byte, C0, C1, F5..FF
            }

            if (size - i <= extra)
                return kInvalid; // truncated
            for (std::size_t k = 1; k <= extra; ++k)
            {
                const unsigned char b = static_cast<unsigned char>(text[i + k]);
                if (k == 1 ? (b < lo || b > hi) : !is_continuation(b))
                    return kInvalid;
                cp = (cp << 6) | (b & 0x3F);
            }
            i += extra + 1;
            return cp;
        }

        // true when `text` is not valid UTF-8 or when `matches(code point)` holds for one of its code points (the refusal predicates
        // fail closed on text they cannot decode).
        template <typename Predicate>
        bool invalid_or_any(const std::string &text, Predicate matches)
        {
            std::size_t i = 0;
            while (i < text.size())
            {
                const char32_t cp = next_code_point(text, i);
                if (cp == kInvalid || matches(cp))
                    return true;
            }
            return false;
        }
    }

    std::optional<std::size_t> utf16_length(const std::string &utf8)
    {
        std::size_t units = 0;
        std::size_t i = 0;
        while (i < utf8.size())
        {
            const char32_t cp = next_code_point(utf8, i);
            if (cp == kInvalid)
                return std::nullopt;
            units += cp >= 0x10000 ? 2 : 1; // a surrogate pair outside the BMP
        }
        return units;
    }

    bool contains_nul(const std::string &utf8)
    {
        return utf8.find('\0') != std::string::npos;
    }

    bool has_control_character(const std::string &utf8)
    {
        // Unicode category Cc: C0 (U+0000-U+001F), DEL (U+007F) and C1 (U+0080-U+009F).
        return invalid_or_any(utf8, [](char32_t cp) { return cp <= 0x1F || (cp >= 0x7F && cp <= 0x9F); });
    }

    bool has_line_break(const std::string &utf8)
    {
        // The code points Python's str.splitlines() splits on.
        return invalid_or_any(utf8, [](char32_t cp) {
            return (cp >= 0x0A && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1E) || cp == 0x85 || cp == 0x2028 || cp == 0x2029;
        });
    }

}
