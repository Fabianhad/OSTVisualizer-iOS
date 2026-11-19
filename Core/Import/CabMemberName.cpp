// Layer: Import (pure C++)
// See CabMemberName.hpp.
#include "CabMemberName.hpp"
#include "UnicodeNonPrintableData.hpp"

#include <cstddef>
#include <utility>

namespace ost_import
{
    namespace
    {
        const char SEP = '\\';

        struct SplitRoot
        {
            std::string drive;
            std::string root;
            std::string rest;
        };

        bool is_ascii_letter(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

        std::string with_backslashes(const std::string &path)
        {
            std::string out = path;
            for (char &c : out)
                if (c == '/')
                    c = SEP;
            return out;
        }

        // Python 3.11 pathlib._WindowsFlavour.splitroot (+ _split_extended_path) on a string whose '/' are already
        // backslashes: the drive ("C:", "\\server\share", "\\?\C:", "\\?\UNC\server\share"), the root ("\") and the rest.
        // The first characters that decide the split are ASCII, so working on UTF-8 bytes gives the same split.
        SplitRoot split_root(std::string part)
        {
            auto at = [&part](size_t i) -> int { return i < part.size() ? static_cast<unsigned char>(part[i]) : -1; };
            int first = at(0);
            int second = at(1);
            std::string prefix;
            if (second == SEP && first == SEP)
            {
                if (part.compare(0, 4, "\\\\?\\") == 0)
                {
                    prefix = part.substr(0, 4);
                    part = part.substr(4);
                    if (part.compare(0, 4, "UNC\\") == 0)
                    {
                        prefix += part.substr(0, 3);
                        part = "\\" + part.substr(3);
                    }
                }
                first = at(0);
                second = at(1);
            }
            const int third = at(2);
            if (second == SEP && first == SEP && third != SEP)
            {
                // UNC path: \\machine\mountpoint\directory\...
                const size_t index = part.find(SEP, 2);
                if (index != std::string::npos)
                {
                    size_t index2 = part.find(SEP, index + 1);
                    if (index2 != index + 1) // a UNC path cannot have two slashes in a row after the first two
                    {
                        if (index2 == std::string::npos)
                            index2 = part.size();
                        const std::string rest = index2 + 1 <= part.size() ? part.substr(index2 + 1) : std::string();
                        if (!prefix.empty())
                            return {prefix + part.substr(1, index2 - 1), "\\", rest};
                        return {part.substr(0, index2), "\\", rest};
                    }
                }
            }
            std::string drive, root;
            if (second == ':' && is_ascii_letter(first))
            {
                drive = part.substr(0, 2);
                part = part.substr(2);
                first = third;
            }
            if (first == SEP)
            {
                root = "\\";
                size_t skip = 0;
                while (skip < part.size() && part[skip] == SEP)
                    ++skip;
                part = part.substr(skip);
            }
            return {prefix + drive, root, part};
        }

        // The components of the part after the anchor: split on '\', empty and "." components dropped, ".." kept.
        std::vector<std::string> relative_parts(const std::string &rest)
        {
            std::vector<std::string> parts;
            size_t start = 0;
            while (start <= rest.size())
            {
                size_t end = rest.find(SEP, start);
                if (end == std::string::npos)
                    end = rest.size();
                const std::string component = rest.substr(start, end - start);
                if (!component.empty() && component != ".")
                    parts.push_back(component);
                start = end + 1;
            }
            return parts;
        }

        bool ends_in_dot_or_space(const std::string &part)
        {
            return !part.empty() && (part.back() == '.' || part.back() == ' ');
        }

        // str.upper() of the base name, as far as it can matter for a device name: ASCII letters, and U+0131 (dotless i),
        // whose upper-case form is 'I'. Every other non-ASCII character is kept (its upper-case form is not an ASCII
        // letter of any device name).
        std::string upper_for_device_names(const std::string &text)
        {
            std::string out;
            for (size_t i = 0; i < text.size();)
            {
                const unsigned char c = static_cast<unsigned char>(text[i]);
                if (c >= 'a' && c <= 'z')
                {
                    out += static_cast<char>(c - 'a' + 'A');
                    ++i;
                }
                else if (c == 0xC4 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xB1)
                {
                    out += 'I';
                    i += 2;
                }
                else
                {
                    out += static_cast<char>(c);
                    ++i;
                }
            }
            return out;
        }
    }

    namespace
    {
        // MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS): well-formed UTF-8 only (Unicode Table 3-7): no overlong forms, no
        // surrogates (U+D800..U+DFFF), nothing above U+10FFFF, no truncated sequence. Noncharacters (U+FFFE, U+FFFF) are fine.
        bool is_valid_utf8(const std::string &text)
        {
            const size_t n = text.size();
            size_t i = 0;
            while (i < n)
            {
                const unsigned char c = static_cast<unsigned char>(text[i]);
                if (c < 0x80)
                {
                    ++i;
                    continue;
                }
                size_t length = 0;
                unsigned char low = 0x80, high = 0xBF; // the allowed range of the first continuation byte
                if (c >= 0xC2 && c <= 0xDF)
                    length = 2;
                else if (c == 0xE0)
                {
                    length = 3;
                    low = 0xA0;
                }
                else if ((c >= 0xE1 && c <= 0xEC) || c == 0xEE || c == 0xEF)
                    length = 3;
                else if (c == 0xED)
                {
                    length = 3;
                    high = 0x9F;
                }
                else if (c == 0xF0)
                {
                    length = 4;
                    low = 0x90;
                }
                else if (c >= 0xF1 && c <= 0xF3)
                    length = 4;
                else if (c == 0xF4)
                {
                    length = 4;
                    high = 0x8F;
                }
                else
                    return false;
                if (i + length > n)
                    return false;
                const unsigned char first = static_cast<unsigned char>(text[i + 1]);
                if (first < low || first > high)
                    return false;
                for (size_t k = 2; k < length; ++k)
                {
                    const unsigned char next = static_cast<unsigned char>(text[i + k]);
                    if (next < 0x80 || next > 0xBF)
                        return false;
                }
                i += length;
            }
            return true;
        }

        // Windows code page 1252 as MultiByteToWideChar(1252, 0, ...) maps the bytes 0x80..0x9F (0xA0..0xFF are U+00A0..U+00FF,
        // 0x00..0x7F are ASCII). The five bytes the code page leaves undefined (0x81, 0x8D, 0x8F, 0x90, 0x9D) map to the C1
        // control of the same value, as the Windows API does (measured on Windows 11).
        const unsigned CP1252_HIGH[32] = {
            0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
            0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};

        void append_utf8(std::string &out, unsigned code_point)
        {
            if (code_point < 0x80)
            {
                out += static_cast<char>(code_point);
            }
            else if (code_point < 0x800)
            {
                out += static_cast<char>(0xC0 | (code_point >> 6));
                out += static_cast<char>(0x80 | (code_point & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xE0 | (code_point >> 12));
                out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (code_point & 0x3F));
            }
        }
    }

    bool decode_cab_member_name(const std::string &raw, uint16_t attribs, std::string &utf8)
    {
        if (attribs & 0x80) // _A_NAME_IS_UTF
        {
            if (!is_valid_utf8(raw))
                return false;
            utf8 = raw;
            return true;
        }
        std::string out;
        out.reserve(raw.size() * 2);
        for (char ch : raw)
        {
            const unsigned byte = static_cast<unsigned char>(ch);
            append_utf8(out, byte >= 0x80 && byte < 0xA0 ? CP1252_HIGH[byte - 0x80] : byte);
        }
        utf8 = std::move(out);
        return true;
    }

    namespace
    {
        // The well-formed UTF-8 sequence of two to four bytes that starts at text[i]: its length, and its code point in `code_point`.
        // 0 when the bytes there are not well-formed (a stray or truncated byte, an overlong form, a surrogate, above U+10FFFF).
        size_t decode_utf8_at(const std::string &text, size_t i, uint32_t &code_point)
        {
            const unsigned char first = static_cast<unsigned char>(text[i]);
            size_t length = 0;
            uint32_t value = 0;
            if (first >= 0xC2 && first <= 0xDF)
            {
                length = 2;
                value = first & 0x1Fu;
            }
            else if (first >= 0xE0 && first <= 0xEF)
            {
                length = 3;
                value = first & 0x0Fu;
            }
            else if (first >= 0xF0 && first <= 0xF4)
            {
                length = 4;
                value = first & 0x07u;
            }
            else
                return 0;
            if (i + length > text.size())
                return 0;
            for (size_t k = 1; k < length; ++k)
            {
                const unsigned char next = static_cast<unsigned char>(text[i + k]);
                if ((next & 0xC0) != 0x80)
                    return 0;
                value = (value << 6) | (next & 0x3Fu);
            }
            if ((length == 3 && value < 0x800) || (length == 4 && (value < 0x10000 || value > 0x10FFFF)) ||
                (value >= 0xD800 && value <= 0xDFFF))
                return 0;
            code_point = value;
            return length;
        }

        // str.isprintable() for a code point from U+0080 up. In the Latin-1 range only U+0080..U+00A0 (controls, no-break space) and
        // U+00AD (soft hyphen) are not printable; above it the generated table lists the ranges of the categories Cc, Cf, Cs, Co,
        // Cn, Zl, Zp and Zs.
        bool is_python_printable(uint32_t code_point)
        {
            if (code_point < 0x100)
                return !(code_point <= 0xA0 || code_point == 0xAD);
            const unicode_printable_data::Range *ranges = unicode_printable_data::NON_PRINTABLE_FROM_U0100;
            size_t low = 0;
            size_t high = sizeof(unicode_printable_data::NON_PRINTABLE_FROM_U0100) / sizeof(unicode_printable_data::Range);
            while (low < high)
            {
                const size_t middle = (low + high) / 2;
                if (code_point < ranges[middle].first)
                    high = middle;
                else if (code_point > ranges[middle].last)
                    low = middle + 1;
                else
                    return false;
            }
            return true;
        }

        // repr's escape of a non-printable character: backslash, x and two hex digits up to U+00FF, backslash, u and four up to
        // U+FFFF, backslash, capital U and eight above (lower-case hex digits).
        void append_python_escape(std::string &out, uint32_t code_point)
        {
            static const char digits[] = "0123456789abcdef";
            const int width = code_point <= 0xFF ? 2 : (code_point <= 0xFFFF ? 4 : 8);
            out += '\\';
            out += width == 2 ? 'x' : (width == 4 ? 'u' : 'U');
            for (int shift = (width - 1) * 4; shift >= 0; shift -= 4)
                out += digits[(code_point >> shift) & 0xF];
        }
    }

    std::string python_repr(const std::string &text)
    {
        // Python repr(str): single quotes unless the text has a ' and no ".
        const bool double_quotes = text.find('\'') != std::string::npos && text.find('"') == std::string::npos;
        const char quote = double_quotes ? '"' : '\'';
        static const char hex[] = "0123456789abcdef";
        std::string out;
        out += quote;
        for (size_t i = 0; i < text.size();)
        {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            if (c == static_cast<unsigned char>(quote) || c == '\\')
            {
                out += '\\';
                out += static_cast<char>(c);
            }
            else if (c == '\t')
                out += "\\t";
            else if (c == '\n')
                out += "\\n";
            else if (c == '\r')
                out += "\\r";
            else if (c < 0x20 || c == 0x7F)
            {
                out += "\\x";
                out += hex[c >> 4];
                out += hex[c & 0x0F];
            }
            else if (c >= 0xC2)
            {
                // A well-formed multi-byte character: written as it is when str.isprintable() accepts it, otherwise as the
                // escape repr uses (append_python_escape).
                uint32_t code_point = 0;
                const size_t length = decode_utf8_at(text, i, code_point);
                if (length > 0 && !is_python_printable(code_point))
                {
                    append_python_escape(out, code_point);
                    i += length;
                    continue;
                }
                if (length > 0)
                {
                    out.append(text, i, length);
                    i += length;
                    continue;
                }
                out += static_cast<char>(c); // not well-formed UTF-8: the byte stays
            }
            else
                out += static_cast<char>(c);
            ++i;
        }
        out += quote;
        return out;
    }

    std::string unsafe_member_path_message(const std::string &member_name)
    {
        return "unsafe CAB member path: " + python_repr(member_name);
    }

    std::string windows_path_name(const std::string &path)
    {
        if (path.empty())
            return std::string();
        const SplitRoot split = split_root(with_backslashes(path));
        const std::vector<std::string> parts = relative_parts(split.rest);
        return parts.empty() ? std::string() : parts.back();
    }

    std::string windows_path_suffix(const std::string &path)
    {
        const std::string name = windows_path_name(path);
        const size_t dot = name.rfind('.');
        // pathlib: `0 < i < len(name) - 1`
        if (dot == std::string::npos || dot == 0 || dot + 1 >= name.size())
            return std::string();
        return name.substr(dot);
    }

    std::string windows_path_stem(const std::string &path)
    {
        const std::string name = windows_path_name(path);
        const std::string suffix = windows_path_suffix(path);
        return name.substr(0, name.size() - suffix.size());
    }

    bool is_windows_device_name(const std::string &part)
    {
        std::string base = part.substr(0, part.find('.'));
        while (!base.empty() && base.back() == ' ')
            base.pop_back();
        const std::string upper = upper_for_device_names(base);
        if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" || upper == "CONIN$" || upper == "CONOUT$")
            return true;
        if (upper.size() >= 4 && (upper.compare(0, 3, "COM") == 0 || upper.compare(0, 3, "LPT") == 0))
        {
            const std::string digit = upper.substr(3);
            if (digit.size() == 1 && digit[0] >= '1' && digit[0] <= '9')
                return true;
            // the superscript digits U+00B9, U+00B2, U+00B3
            if (digit == "\xC2\xB9" || digit == "\xC2\xB2" || digit == "\xC2\xB3")
                return true;
        }
        return false;
    }

    std::vector<std::string> windows_path_parts(const std::string &path)
    {
        std::vector<std::string> parts;
        if (path.empty())
            return parts;
        const SplitRoot split = split_root(with_backslashes(path));
        if (!split.drive.empty() || !split.root.empty())
            parts.push_back(split.drive + split.root);
        for (std::string &component : relative_parts(split.rest))
            parts.push_back(std::move(component));
        return parts;
    }

    bool is_safe_cab_member_name(const std::string &member_name)
    {
        if (member_name.empty())
            return false;
        const SplitRoot split = split_root(with_backslashes(member_name));
        if (!split.drive.empty() || !split.root.empty()) // absolute, drive-relative, rooted, UNC or "\\?\" prefixed
            return false;
        const std::vector<std::string> parts = relative_parts(split.rest);
        if (parts.empty())
            return false;
        for (const std::string &part : parts)
        {
            if (ends_in_dot_or_space(part) || part.find(':') != std::string::npos || is_windows_device_name(part))
                return false;
        }
        return true;
    }
}
