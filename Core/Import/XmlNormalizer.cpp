// Layer: Import (pure C++)
// See XmlNormalizer.hpp. The rules are those of expat 2.5.0 as used by Python's xml.etree.ElementTree (namespace
// processing on, no DTD loading), checked against pyexpat on a large differential corpus (Fixtures/import_xml_oracle.txt,
// Config/Tests/Import/test_xml_strict.cpp).
#include "XmlNormalizer.hpp"
#include "XmlCharData.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ost_import
{
    namespace
    {
        const uint32_t CP_EOF = 0xFFFFFFFFu; // end of input
        const uint32_t CP_BAD = 0xFFFFFFFEu; // an invalid or truncated byte sequence

        const char *const XML_NAMESPACE = "http://www.w3.org/XML/1998/namespace";
        const char *const XMLNS_NAMESPACE = "http://www.w3.org/2000/xmlns/";

        const char *const MSG_INVALID_TOKEN = "not well-formed (invalid token)";
        const char *const MSG_UNCLOSED_TOKEN = "unclosed token";
        const char *const MSG_NO_ELEMENT = "no element found";
        const char *const MSG_SYNTAX = "syntax error";
        const char *const MSG_JUNK = "junk after document element";
        const char *const MSG_MISPLACED_DECL = "XML or text declaration not at start of entity";

        // The character sets the parser reads: the encodings of expat (UTF-8, UTF-16, US-ASCII, ISO-8859-1) and the
        // single-byte codecs Python's unknown-encoding handler adds. Latin1 is expat's own ISO-8859-1 (U+00AA, U+00B5 and
        // U+00BA count as letters there, as in UTF-16 text); Latin1Table is Python's latin_1 codec, classified by the
        // general name tables.
        enum class Enc
        {
            Utf8,
            Utf16LE,
            Utf16BE,
            Latin1,
            Ascii,
            Latin1Table,
            Cp1252
        };

        struct ParseError
        {
            std::string message;
            size_t offset;
            bool positioned;
        };

        bool in_ranges(const xml_data::Range *ranges, size_t count, uint32_t cp)
        {
            size_t lo = 0, hi = count;
            while (lo < hi)
            {
                const size_t mid = (lo + hi) / 2;
                if (cp < ranges[mid].first)
                    hi = mid;
                else if (cp > ranges[mid].last)
                    lo = mid + 1;
                else
                    return true;
            }
            return false;
        }

        // #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | [#x10000-#x10FFFF]
        bool is_xml_char(uint32_t c)
        {
            return c == 0x09 || c == 0x0A || c == 0x0D || (c >= 0x20 && c <= 0xD7FF) || (c >= 0xE000 && c <= 0xFFFD) ||
                   (c >= 0x10000 && c <= 0x10FFFF);
        }

        bool is_space(uint32_t c) { return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D; }

        bool is_digit(uint32_t c) { return c >= '0' && c <= '9'; }

        int hex_value(uint32_t c)
        {
            if (c >= '0' && c <= '9')
                return static_cast<int>(c - '0');
            if (c >= 'a' && c <= 'f')
                return static_cast<int>(c - 'a' + 10);
            if (c >= 'A' && c <= 'F')
                return static_cast<int>(c - 'A' + 10);
            return -1;
        }

        void append_utf8(std::string &out, uint32_t cp)
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

        std::string ascii_lower(const std::string &s)
        {
            std::string out = s;
            for (char &c : out)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            return out;
        }

        // Python's encodings.normalize_encoding: runs of characters that are neither alphanumeric nor '.' become one '_'
        // (none at the ends). The label of an XML declaration only holds [A-Za-z0-9._-].
        std::string normalize_encoding(const std::string &label)
        {
            std::string out;
            bool punct = false;
            for (char c : label)
            {
                const bool alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                if (alnum || c == '.')
                {
                    if (punct && !out.empty())
                        out += '_';
                    out += c;
                    punct = false;
                }
                else
                {
                    punct = true;
                }
            }
            return out;
        }

        template <size_t N>
        bool contains_name(const char *const (&names)[N], const std::string &name)
        {
            for (const char *n : names)
                if (name == n)
                    return true;
            return false;
        }

        struct CodecChoice
        {
            bool known = false;
            Enc enc = Enc::Ascii;
        };

        // Python's codec lookup for a declared label that expat does not know (encodings.search_function): the alias table
        // (tried with the name as it is and with '.' read as '_'), else the module of that name. Only the codecs listed in
        // XmlCharData.hpp are supported; a name that resolves to ascii or utf_8 decodes 7-bit text only (pyexpat builds
        // the byte table by decoding 0..255 with errors='replace', so every byte >= 0x80 is rejected).
        CodecChoice python_codec(const std::string &lower_label)
        {
            const std::string norm = normalize_encoding(lower_label);
            std::string dotted = norm;
            std::replace(dotted.begin(), dotted.end(), '.', '_');
            auto has_alias = [&](auto &names) { return contains_name(names, norm) || contains_name(names, dotted); };
            CodecChoice choice;
            choice.known = true;
            if (has_alias(xml_data::CP1252_ALIASES) || norm == "cp1252")
                choice.enc = Enc::Cp1252;
            else if (has_alias(xml_data::LATIN1_ALIASES) || norm == "latin_1")
                choice.enc = Enc::Latin1Table;
            else if (has_alias(xml_data::ASCII_ALIASES) || norm == "ascii")
                choice.enc = Enc::Ascii;
            else if (has_alias(xml_data::UTF8_ALIASES) || norm == "utf_8")
                choice.enc = Enc::Ascii;
            else
                choice.known = false;
            return choice;
        }

        class Parser
        {
        public:
            Parser(const char *data, size_t length) : d_(reinterpret_cast<const unsigned char *>(data)), n_(length)
            {
                out_.reserve(length);
            }

            NormalizedXml run()
            {
                NormalizedXml result;
                try
                {
                    detect_encoding();
                    load();
                    prolog();
                    element();
                    epilog();
                    result.ok = true;
                    result.utf8.swap(out_);
                }
                catch (const ParseError &e)
                {
                    result.error = e.positioned ? e.message + ": " + position(e.offset) : e.message;
                }
                return result;
            }

        private:
            struct Attr
            {
                std::string name;
                std::string value;
                size_t offset;       // where the attribute name starts
                bool failed = false; // the value holds an undefined entity / invalid character number (reported after the tag is read)
                ParseError failure{std::string(), 0, true};
            };

            struct Binding
            {
                std::string prefix;
                std::string uri;
            };

            struct Open
            {
                std::string raw; // the name as written, matched by the end tag
                std::string out; // the name written to the output
                size_t bindings; // namespace bindings in force before this element
            };

            // ---- input ---------------------------------------------------------------------------------------------

            // The character at byte offset pos; CP_EOF at the end, CP_BAD for an invalid or truncated sequence.
            uint32_t decode(size_t pos, size_t &len) const
            {
                if (pos >= n_)
                {
                    len = 0;
                    return CP_EOF;
                }
                const uint32_t b0 = d_[pos];
                switch (enc_)
                {
                case Enc::Utf8:
                {
                    if (b0 < 0x80)
                    {
                        len = 1;
                        return b0;
                    }
                    size_t need = 0;
                    uint32_t cp = 0;
                    if (b0 >= 0xC2 && b0 <= 0xDF)
                    {
                        need = 1;
                        cp = b0 & 0x1F;
                    }
                    else if (b0 >= 0xE0 && b0 <= 0xEF)
                    {
                        need = 2;
                        cp = b0 & 0x0F;
                    }
                    else if (b0 >= 0xF0 && b0 <= 0xF4)
                    {
                        need = 3;
                        cp = b0 & 0x07;
                    }
                    else
                    {
                        len = 1;
                        return CP_BAD;
                    }
                    if (pos + need >= n_)
                    {
                        len = 1;
                        return CP_BAD;
                    }
                    for (size_t k = 1; k <= need; ++k)
                    {
                        const uint32_t b = d_[pos + k];
                        if ((b & 0xC0) != 0x80)
                        {
                            len = 1;
                            return CP_BAD;
                        }
                        cp = (cp << 6) | (b & 0x3F);
                    }
                    if ((need == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) || (need == 3 && (cp < 0x10000 || cp > 0x10FFFF)))
                    {
                        len = 1;
                        return CP_BAD;
                    }
                    len = need + 1;
                    return cp;
                }
                case Enc::Utf16LE:
                case Enc::Utf16BE:
                {
                    if (pos + 1 >= n_)
                    {
                        len = n_ - pos;
                        return CP_BAD;
                    }
                    const uint32_t unit = unit16(pos);
                    if (unit >= 0xD800 && unit <= 0xDBFF)
                    {
                        if (pos + 3 < n_)
                        {
                            const uint32_t low = unit16(pos + 2);
                            if (low >= 0xDC00 && low <= 0xDFFF)
                            {
                                len = 4;
                                return 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                            }
                        }
                        len = 2;
                        return CP_BAD;
                    }
                    len = 2;
                    if (unit >= 0xDC00 && unit <= 0xDFFF)
                        return CP_BAD;
                    return unit;
                }
                case Enc::Latin1:
                case Enc::Latin1Table:
                    len = 1;
                    return b0;
                case Enc::Ascii:
                    len = 1;
                    return b0 < 0x80 ? b0 : CP_BAD;
                case Enc::Cp1252:
                {
                    len = 1;
                    if (b0 < 0x80)
                        return b0;
                    const uint16_t mapped = xml_data::CP1252_HIGH[b0 - 0x80];
                    return mapped == 0xFFFF ? CP_BAD : mapped;
                }
                }
                len = 1;
                return CP_BAD;
            }

            uint32_t unit16(size_t pos) const
            {
                return enc_ == Enc::Utf16LE ? (d_[pos] | (d_[pos + 1] << 8)) : ((d_[pos] << 8) | d_[pos + 1]);
            }

            void load() { cp_ = decode(pos_, len_); }

            void advance()
            {
                pos_ += len_;
                load();
            }

            // true when the next characters are the ASCII text s (nothing is consumed)
            bool match(const char *s) const
            {
                size_t p = pos_;
                for (; *s; ++s)
                {
                    size_t l = 0;
                    if (decode(p, l) != static_cast<uint32_t>(static_cast<unsigned char>(*s)))
                        return false;
                    p += l;
                }
                return true;
            }

            void skip(size_t count)
            {
                while (count--)
                    advance();
            }

            size_t skip_space()
            {
                size_t count = 0;
                while (is_space(cp_))
                {
                    advance();
                    ++count;
                }
                return count;
            }

            // ElementTree.ParseError position: 1-based line, 0-based column (in characters).
            std::string position(size_t offset) const
            {
                size_t line = 1, column = 0;
                const bool wide = enc_ == Enc::Utf16LE || enc_ == Enc::Utf16BE;
                const size_t step = wide ? 2 : 1;
                auto unit_at = [&](size_t i) -> uint32_t
                {
                    if (i + step > n_)
                        return 0xFFFFFFFFu;
                    return wide ? unit16(i) : d_[i];
                };
                for (size_t i = start_; i + step <= offset && i + step <= n_; i += step)
                {
                    const uint32_t c = unit_at(i);
                    if (c == 0x0A)
                    {
                        ++line;
                        column = 0;
                    }
                    else if (c == 0x0D)
                    {
                        if (unit_at(i + step) != 0x0A)
                        {
                            ++line;
                            column = 0;
                        }
                    }
                    else if (!wide && (c & 0xC0) == 0x80)
                    {
                        // UTF-8 continuation byte: part of the previous character
                    }
                    else if (wide && c >= 0xDC00 && c <= 0xDFFF)
                    {
                        // low surrogate: part of the previous character
                    }
                    else
                    {
                        ++column;
                    }
                }
                return "line " + std::to_string(line) + ", column " + std::to_string(column);
            }

            // An invalid token at the end of the input is an unfinished token for expat (reported where the token starts).
            [[noreturn]] void fail(const char *message) const
            {
                if (message == MSG_INVALID_TOKEN && cp_ == CP_EOF)
                    throw ParseError{MSG_UNCLOSED_TOKEN, tok_, true};
                throw ParseError{message, pos_, true};
            }
            [[noreturn]] void fail_at(size_t offset, const char *message) const { throw ParseError{message, offset, true}; }
            [[noreturn]] void fail_text(const std::string &message) const { throw ParseError{message, pos_, false}; }

            // ---- encoding ------------------------------------------------------------------------------------------

            // expat's initScan: a byte order mark, else a NUL in the first two bytes means UTF-16 (the NUL says which
            // byte order), else UTF-8.
            void detect_encoding()
            {
                enc_ = Enc::Utf8;
                start_ = 0;
                if (n_ >= 2)
                {
                    const unsigned b0 = d_[0], b1 = d_[1];
                    if (b0 == 0xFE && b1 == 0xFF)
                    {
                        enc_ = Enc::Utf16BE;
                        start_ = 2;
                    }
                    else if (b0 == 0xFF && b1 == 0xFE)
                    {
                        enc_ = Enc::Utf16LE;
                        start_ = 2;
                    }
                    else if (b0 == 0xEF && b1 == 0xBB)
                    {
                        if (n_ == 2)
                            throw ParseError{MSG_UNCLOSED_TOKEN, 0, true};
                        if (d_[2] == 0xBF)
                            start_ = 3;
                    }
                    else if (b0 == 0)
                    {
                        enc_ = Enc::Utf16BE;
                    }
                    else if (b1 == 0)
                    {
                        enc_ = Enc::Utf16LE;
                    }
                }
                pos_ = start_;
            }

            bool wide() const { return enc_ == Enc::Utf16LE || enc_ == Enc::Utf16BE; }

            // The encoding named by the XML declaration (expat's processXmlDecl + pyexpat's unknown-encoding handler).
            void declared_encoding(const std::string &label, size_t value_offset)
            {
                const std::string lower = ascii_lower(label);
                static const char *const incorrect = "encoding specified in XML declaration is incorrect";
                if (lower == "utf-16")
                {
                    if (!wide())
                        fail_at(value_offset, incorrect);
                    return;
                }
                if (lower == "utf-16le" || lower == "utf-16be")
                {
                    if (enc_ != (lower == "utf-16le" ? Enc::Utf16LE : Enc::Utf16BE))
                        fail_at(value_offset, incorrect);
                    return;
                }
                Enc target;
                if (lower == "utf-8")
                    target = Enc::Utf8;
                else if (lower == "iso-8859-1")
                    target = Enc::Latin1;
                else if (lower == "us-ascii")
                    target = Enc::Ascii;
                else
                {
                    const CodecChoice codec = python_codec(lower);
                    if (!codec.known)
                        fail_text("unsupported encoding: " + label);
                    if (wide())
                        fail(MSG_INVALID_TOKEN); // expat would switch to a one-byte encoding in the middle of UTF-16 text
                    target = codec.enc;
                }
                if (wide())
                    fail_at(value_offset, incorrect);
                enc_ = target;
                load();
            }

            // ---- names ---------------------------------------------------------------------------------------------

            // expat classifies the characters below U+0100 of its own ISO-8859-1 and of UTF-16 text by its Latin-1 byte table, in
            // which U+00AA, U+00B5 and U+00BA are letters; every other path (UTF-8, Python's codecs) uses the general name tables.
            bool latin1_letters() const { return enc_ == Enc::Latin1 || enc_ == Enc::Utf16LE || enc_ == Enc::Utf16BE; }

            bool name_start(uint32_t cp) const
            {
                if (cp < 0x80)
                    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '_';
                if (latin1_letters() && (cp == 0xAA || cp == 0xB5 || cp == 0xBA))
                    return true;
                return cp < 0x10000 && in_ranges(xml_data::NAME_START, xml_data::NAME_START_COUNT, cp);
            }

            bool name_char(uint32_t cp) const
            {
                if (cp < 0x80)
                    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') || cp == '_' || cp == '-' ||
                           cp == '.';
                if (latin1_letters() && (cp == 0xAA || cp == 0xB5 || cp == 0xBA))
                    return true;
                return cp < 0x10000 && in_ranges(xml_data::NAME_CHAR, xml_data::NAME_CHAR_COUNT, cp);
            }

            // A name without a colon (processing instruction targets, entity names, DOCTYPE keywords); "" when the
            // current character cannot start a name.
            std::string scan_ncname()
            {
                std::string name;
                if (!name_start(cp_))
                    return name;
                while (name_char(cp_))
                {
                    append_utf8(name, cp_);
                    advance();
                }
                return name;
            }

            // A qualified name NCName | NCName ':' NCName (expat with namespace processing rejects every other use of
            // the colon); "" when the current character cannot start a name.
            std::string scan_qname()
            {
                std::string name;
                if (!name_start(cp_))
                    return name;
                bool after_colon = false;
                bool seen_colon = false;
                for (;;)
                {
                    if (cp_ == ':')
                    {
                        if (seen_colon || after_colon)
                            fail(MSG_INVALID_TOKEN);
                        seen_colon = true;
                        after_colon = true;
                        name += ':';
                        advance();
                        continue;
                    }
                    if (after_colon)
                    {
                        if (!name_start(cp_))
                            fail(MSG_INVALID_TOKEN);
                        after_colon = false;
                    }
                    else if (!name_char(cp_))
                    {
                        break;
                    }
                    append_utf8(name, cp_);
                    advance();
                }
                if (after_colon)
                    fail(MSG_INVALID_TOKEN);
                return name;
            }

            // ---- document structure --------------------------------------------------------------------------------

            // Anything but markup or white space before the root element. expat reads the first token of the garbage
            // (a name, up to a character that cannot follow a name) and reports it as a syntax error, or the character
            // that broke the token as an invalid token; this follows it for the common cases (text, binary data).
            [[noreturn]] void garbage(const char *syntax_message)
            {
                if (name_start(cp_) || cp_ == ':')
                {
                    const size_t token_start = pos_;
                    while (name_char(cp_) || cp_ == ':')
                        advance();
                    if (cp_ == CP_EOF || is_space(cp_) || cp_ == '>' || cp_ == ')' || cp_ == ',' || cp_ == '|' || cp_ == '[' ||
                        cp_ == '%')
                        fail_at(token_start, syntax_message);
                    fail(MSG_INVALID_TOKEN);
                }
                if (cp_ == '(' || cp_ == ']')
                    fail(syntax_message);
                fail(MSG_INVALID_TOKEN);
            }

            void prolog()
            {
                bool seen_doctype = false;
                const size_t first = pos_;
                for (;;)
                {
                    skip_space();
                    tok_ = pos_;
                    if (cp_ == CP_EOF)
                        fail(MSG_NO_ELEMENT);
                    if (cp_ != '<')
                        garbage(MSG_SYNTAX);
                    if (match("<?"))
                    {
                        processing_instruction(pos_ == first, MSG_MISPLACED_DECL);
                        continue;
                    }
                    if (match("<!--"))
                    {
                        comment();
                        continue;
                    }
                    if (match("<!DOCTYPE"))
                    {
                        if (seen_doctype)
                            fail(MSG_SYNTAX);
                        seen_doctype = true;
                        doctype();
                        continue;
                    }
                    if (match("<!"))
                        fail(MSG_SYNTAX);
                    return; // the root element
                }
            }

            void epilog()
            {
                for (;;)
                {
                    skip_space();
                    tok_ = pos_;
                    if (cp_ == CP_EOF)
                        return;
                    if (cp_ == '<')
                    {
                        if (match("<?"))
                        {
                            processing_instruction(false, MSG_JUNK);
                            continue;
                        }
                        if (match("<!--"))
                        {
                            comment();
                            continue;
                        }
                        fail_at(pos_, MSG_JUNK);
                    }
                    garbage(MSG_JUNK);
                }
            }

            void comment()
            {
                const size_t token_start = pos_;
                skip(4); // "<!--"
                for (;;)
                {
                    if (cp_ == CP_EOF)
                        fail_at(token_start, MSG_UNCLOSED_TOKEN);
                    if (cp_ == '-')
                    {
                        if (match("-->"))
                        {
                            skip(3);
                            return;
                        }
                        if (match("--"))
                            fail(MSG_INVALID_TOKEN); // "--" inside a comment
                        advance();
                        continue;
                    }
                    if (!is_xml_char(cp_))
                        fail(MSG_INVALID_TOKEN);
                    advance();
                }
            }

            // "<?target data?>". The target "xml" is the XML declaration: only valid as the very first thing of the
            // document (`decl_allowed`), anywhere else it is `misplaced`; "XML", "Xml" ... are invalid.
            void processing_instruction(bool decl_allowed, const char *misplaced)
            {
                const size_t token_start = pos_;
                skip(2); // "<?"
                const std::string target = scan_ncname();
                if (target.empty())
                    fail(MSG_INVALID_TOKEN);
                if (cp_ == CP_EOF)
                    fail_at(token_start, MSG_UNCLOSED_TOKEN);
                if (cp_ != '?' && !is_space(cp_))
                    fail(MSG_INVALID_TOKEN); // e.g. a colon in the target
                if (target.size() == 3 && ascii_lower(target) == "xml")
                {
                    if (target != "xml")
                        fail_at(pos_, MSG_INVALID_TOKEN);
                    if (!decl_allowed)
                        fail_at(token_start, misplaced);
                    xml_declaration(token_start);
                    return;
                }
                if (cp_ == '?')
                {
                    if (!match("?>"))
                        fail(MSG_INVALID_TOKEN);
                    skip(2);
                    return;
                }
                for (;;)
                {
                    if (cp_ == CP_EOF)
                        fail_at(token_start, MSG_UNCLOSED_TOKEN);
                    if (cp_ == '?' && match("?>"))
                    {
                        skip(2);
                        return;
                    }
                    if (!is_xml_char(cp_))
                        fail(MSG_INVALID_TOKEN);
                    advance();
                }
            }

            // ---- the XML declaration (expat's doParseXmlDecl) ------------------------------------------------------

            static int decl_char(const std::string &text, size_t i)
            {
                if (i >= text.size())
                    return -1;
                const unsigned char c = static_cast<unsigned char>(text[i]);
                return c < 0x80 ? static_cast<int>(c) : -1;
            }

            static bool decl_space(int c) { return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D; }

            // One name="value" pair (expat's parsePseudoAttribute). `have` is false when only white space is left. false:
            // malformed, `ptr` then points at the offending character.
            static bool pseudo_attribute(const std::string &t, size_t &ptr, size_t &name_start, size_t &name_end, size_t &value_start,
                                         size_t &value_end, bool &have)
            {
                have = false;
                const size_t end = t.size();
                if (ptr == end)
                    return true;
                if (!decl_space(decl_char(t, ptr)))
                    return false;
                do
                    ++ptr;
                while (decl_space(decl_char(t, ptr)));
                if (ptr == end)
                    return true;
                name_start = ptr;
                for (;;)
                {
                    int c = decl_char(t, ptr);
                    if (c == -1)
                        return false;
                    if (c == '=')
                    {
                        name_end = ptr;
                        break;
                    }
                    if (decl_space(c))
                    {
                        name_end = ptr;
                        do
                            ++ptr;
                        while (decl_space(c = decl_char(t, ptr)));
                        if (c != '=')
                            return false;
                        break;
                    }
                    ++ptr;
                }
                if (ptr == name_start)
                    return false;
                ++ptr;
                int c = decl_char(t, ptr);
                while (decl_space(c))
                {
                    ++ptr;
                    c = decl_char(t, ptr);
                }
                if (c != '"' && c != '\'')
                    return false;
                const int open = c;
                ++ptr;
                value_start = ptr;
                for (;; ++ptr)
                {
                    c = decl_char(t, ptr);
                    if (c == open)
                        break;
                    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_'))
                        return false;
                }
                value_end = ptr;
                ++ptr;
                have = true;
                return true;
            }

            void xml_declaration(size_t token_start)
            {
                const size_t text_offset = pos_; // right after "<?xml"
                const size_t unit = wide() ? 2 : 1;
                std::string text;
                for (;;)
                {
                    if (cp_ == CP_EOF)
                        fail_at(token_start, MSG_UNCLOSED_TOKEN);
                    if (cp_ == '?' && match("?>"))
                    {
                        skip(2);
                        break;
                    }
                    if (!is_xml_char(cp_))
                        fail(MSG_INVALID_TOKEN);
                    text += cp_ < 0x80 ? static_cast<char>(cp_) : '\x80';
                    advance();
                }
                auto bad = [&](size_t index)
                {
                    fail_at(text_offset + index * unit, "XML declaration not well-formed");
                };
                size_t ptr = 0, name_start = 0, name_end = 0, value_start = 0, value_end = 0;
                bool have = false;
                if (!pseudo_attribute(text, ptr, name_start, name_end, value_start, value_end, have) || !have)
                    bad(ptr);
                if (text.substr(name_start, name_end - name_start) != "version")
                    bad(name_start);
                if (!pseudo_attribute(text, ptr, name_start, name_end, value_start, value_end, have))
                    bad(ptr);
                if (!have)
                    return;
                std::string encoding;
                size_t encoding_offset = 0;
                bool has_encoding = false;
                if (text.substr(name_start, name_end - name_start) == "encoding")
                {
                    has_encoding = true;
                    const int first = decl_char(text, value_start);
                    if (!((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z')))
                        bad(value_start);
                    encoding = text.substr(value_start, value_end - value_start);
                    encoding_offset = text_offset + value_start * unit;
                    if (!pseudo_attribute(text, ptr, name_start, name_end, value_start, value_end, have))
                        bad(ptr);
                    if (!have)
                    {
                        declared_encoding(encoding, encoding_offset);
                        return;
                    }
                }
                if (text.substr(name_start, name_end - name_start) != "standalone")
                    bad(name_start);
                const std::string standalone = text.substr(value_start, value_end - value_start);
                if (standalone == "yes")
                    standalone_ = true;
                else if (standalone != "no")
                    bad(value_start);
                while (decl_space(decl_char(text, ptr)))
                    ++ptr;
                if (ptr != text.size())
                    bad(ptr);
                if (has_encoding)
                    declared_encoding(encoding, encoding_offset);
            }

            // ---- DOCTYPE -------------------------------------------------------------------------------------------

            static bool pubid_char(uint32_t c)
            {
                if (c == 0x20 || c == 0x0D || c == 0x0A)
                    return true;
                if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                    return true;
                return c < 0x80 && std::strchr("-'()+,./:=?;!*#@$_%", static_cast<int>(c)) != nullptr;
            }

            // expat's prolog tokenizer: a name must be followed by white space, '>', ')', ',', '|', '[' or '%', a literal by
            // white space, '>', '[' or '%'; anything else is an invalid token right there (the end of the input is fine).
            void prolog_token_end(bool is_literal)
            {
                if (cp_ == CP_EOF || is_space(cp_) || cp_ == '>' || cp_ == '[' || cp_ == '%')
                    return;
                if (!is_literal && (cp_ == ')' || cp_ == ',' || cp_ == '|'))
                    return;
                fail(MSG_INVALID_TOKEN);
            }

            void literal(bool public_id)
            {
                const size_t token_start = pos_;
                const uint32_t quote = cp_;
                advance();
                for (;;)
                {
                    if (cp_ == CP_EOF)
                        fail_at(token_start, MSG_UNCLOSED_TOKEN);
                    if (cp_ == quote)
                    {
                        advance();
                        prolog_token_end(true);
                        return;
                    }
                    if (!is_xml_char(cp_))
                        fail(MSG_INVALID_TOKEN);
                    if (public_id && !pubid_char(cp_))
                        fail("illegal character(s) in public id");
                    advance();
                }
            }

            // <!DOCTYPE name [ExternalID] ['[' internal subset ']'] '>' with expat's token-by-token leniency about the white
            // space between the parts. Only an internal subset without declarations is supported.
            void doctype()
            {
                skip(9); // "<!DOCTYPE"
                if (!is_space(cp_))
                    fail(cp_ == CP_EOF ? MSG_NO_ELEMENT : MSG_INVALID_TOKEN);
                skip_space();
                const size_t name_offset = pos_;
                {
                    std::string name; // a Name token, then the namespace rules for its colon
                    int colons = 0;
                    bool last_colon = false;
                    if (name_start(cp_) || cp_ == ':')
                    {
                        while (name_char(cp_) || cp_ == ':')
                        {
                            if (cp_ == ':')
                            {
                                ++colons;
                                last_colon = true;
                            }
                            else
                            {
                                last_colon = false;
                            }
                            append_utf8(name, cp_);
                            advance();
                        }
                    }
                    if (name.empty())
                        fail(cp_ == CP_EOF ? MSG_NO_ELEMENT : MSG_SYNTAX);
                    prolog_token_end(false);
                    if (colons > 1 || last_colon || name[0] == ':')
                        fail_at(name_offset, MSG_SYNTAX);
                }
                skip_space();
                bool external = false;
                if (name_start(cp_))
                {
                    const size_t keyword_offset = pos_;
                    const std::string keyword = scan_ncname();
                    prolog_token_end(false);
                    if (keyword == "SYSTEM")
                    {
                        external = true;
                        skip_space();
                        if (cp_ != '"' && cp_ != '\'')
                            fail(cp_ == CP_EOF ? MSG_NO_ELEMENT : MSG_SYNTAX);
                        literal(false);
                    }
                    else if (keyword == "PUBLIC")
                    {
                        external = true;
                        skip_space();
                        if (cp_ != '"' && cp_ != '\'')
                            fail(cp_ == CP_EOF ? MSG_NO_ELEMENT : MSG_SYNTAX);
                        literal(true);
                        skip_space();
                        if (cp_ != '"' && cp_ != '\'')
                            fail(cp_ == CP_EOF ? MSG_NO_ELEMENT : MSG_SYNTAX);
                        literal(false);
                    }
                    else
                    {
                        fail_at(keyword_offset, MSG_SYNTAX);
                    }
                    skip_space();
                }
                if (cp_ == '[')
                {
                    advance();
                    internal_subset();
                    skip_space();
                }
                if (cp_ != '>')
                    fail(cp_ == CP_EOF ? MSG_NO_ELEMENT : MSG_SYNTAX);
                advance();
                has_external_ = external;
            }

            void internal_subset()
            {
                for (;;)
                {
                    skip_space();
                    if (cp_ == CP_EOF)
                        fail(MSG_NO_ELEMENT);
                    if (cp_ == ']')
                    {
                        advance();
                        return;
                    }
                    if (match("<!--"))
                    {
                        comment();
                        continue;
                    }
                    if (match("<?"))
                    {
                        processing_instruction(false, MSG_MISPLACED_DECL);
                        continue;
                    }
                    if (cp_ == '%' || match("<!"))
                        fail("unsupported DOCTYPE declaration (entity, attribute list, element, notation or parameter entity)");
                    fail(MSG_SYNTAX);
                }
            }

            // ---- elements ------------------------------------------------------------------------------------------

            // The root element and everything inside it, up to the end tag that closes the root.
            void element()
            {
                start_tag();
                while (!stack_.empty())
                {
                    tok_ = pos_;
                    const uint32_t c = cp_;
                    if (c == '<')
                    {
                        if (match("</"))
                            end_tag();
                        else if (match("<!--"))
                            comment();
                        else if (match("<![CDATA["))
                            cdata();
                        else if (match("<?"))
                            processing_instruction(false, MSG_MISPLACED_DECL);
                        else if (match("<!"))
                            fail(MSG_INVALID_TOKEN);
                        else
                            start_tag();
                    }
                    else if (c == '&')
                    {
                        std::string text;
                        reference(false, text, pos_);
                        append_text(text);
                    }
                    else if (c == CP_EOF)
                    {
                        fail(MSG_NO_ELEMENT);
                    }
                    else if (c == ']' && match("]]>"))
                    {
                        fail(MSG_INVALID_TOKEN);
                    }
                    else if (c == '\r')
                    {
                        advance();
                        if (cp_ == '\n')
                            advance();
                        out_ += '\n';
                    }
                    else if (!is_xml_char(c))
                    {
                        fail(MSG_INVALID_TOKEN);
                    }
                    else
                    {
                        append_text_char(c);
                        advance();
                    }
                }
            }

            void append_text_char(uint32_t c)
            {
                switch (c)
                {
                case '&':
                    out_ += "&amp;";
                    break;
                case '<':
                    out_ += "&lt;";
                    break;
                case '>':
                    out_ += "&gt;";
                    break;
                default:
                    if (c < 0x80)
                        out_ += static_cast<char>(c);
                    else
                        append_utf8(out_, c);
                }
            }

            // Text as the output spells it: markup characters escaped, a carriage return (only possible through a
            // character reference) as a reference so that pugixml's line-end conversion leaves it alone.
            void append_text(const std::string &text)
            {
                for (char c : text)
                {
                    switch (c)
                    {
                    case '&':
                        out_ += "&amp;";
                        break;
                    case '<':
                        out_ += "&lt;";
                        break;
                    case '>':
                        out_ += "&gt;";
                        break;
                    case '\r':
                        out_ += "&#13;";
                        break;
                    default:
                        out_ += c;
                    }
                }
            }

            void append_attribute_text(const std::string &text)
            {
                for (char c : text)
                {
                    switch (c)
                    {
                    case '&':
                        out_ += "&amp;";
                        break;
                    case '<':
                        out_ += "&lt;";
                        break;
                    case '"':
                        out_ += "&quot;";
                        break;
                    case '\t':
                        out_ += "&#9;";
                        break;
                    case '\n':
                        out_ += "&#10;";
                        break;
                    case '\r':
                        out_ += "&#13;";
                        break;
                    default:
                        out_ += c;
                    }
                }
            }

            void cdata()
            {
                skip(9); // "<![CDATA["
                for (;;)
                {
                    if (cp_ == CP_EOF)
                        fail("unclosed CDATA section");
                    if (cp_ == ']' && match("]]>"))
                    {
                        skip(3);
                        return;
                    }
                    if (cp_ == '\r')
                    {
                        advance();
                        if (cp_ == '\n')
                            advance();
                        out_ += '\n';
                        continue;
                    }
                    if (!is_xml_char(cp_))
                        fail(MSG_INVALID_TOKEN);
                    append_text_char(cp_);
                    advance();
                }
            }

            // At '&': a character reference, one of the five predefined entities, or an undefined entity (an error,
            // except in an attribute value of a document with an external DTD subset that is not standalone: expat skips
            // it and the value just lacks it). `error_offset` is where expat reports an undefined entity or a bad
            // character number. The resolved text is appended to `dest` unescaped.
            void reference(bool in_attribute, std::string &dest, size_t error_offset, Attr *attribute = nullptr)
            {
                const size_t amp = pos_;
                advance();
                if (cp_ == '#')
                {
                    advance();
                    int base = 10;
                    if (cp_ == 'x')
                    {
                        base = 16;
                        advance();
                    }
                    uint64_t value = 0;
                    size_t digits = 0;
                    for (;;)
                    {
                        const int digit = base == 16 ? hex_value(cp_) : (is_digit(cp_) ? static_cast<int>(cp_ - '0') : -1);
                        if (digit < 0)
                            break;
                        if (value <= 0x10FFFF)
                            value = value * static_cast<uint64_t>(base) + static_cast<uint64_t>(digit);
                        ++digits;
                        advance();
                    }
                    if (digits == 0 || cp_ != ';')
                        fail(MSG_INVALID_TOKEN);
                    advance();
                    if (value > 0x10FFFF || !is_xml_char(static_cast<uint32_t>(value)))
                    {
                        defer_or_throw(attribute, ParseError{"reference to invalid character number", amp, true});
                        return;
                    }
                    append_utf8(dest, static_cast<uint32_t>(value));
                    return;
                }
                const std::string name = scan_ncname();
                if (name.empty() || cp_ != ';')
                    fail(MSG_INVALID_TOKEN);
                advance();
                if (name == "amp")
                    dest += '&';
                else if (name == "lt")
                    dest += '<';
                else if (name == "gt")
                    dest += '>';
                else if (name == "quot")
                    dest += '"';
                else if (name == "apos")
                    dest += '\'';
                else if (in_attribute && has_external_ && !standalone_)
                    return;
                else if (!in_attribute && has_external_ && !standalone_)
                    throw ParseError{"undefined entity &" + name + ";", error_offset, true}; // ElementTree's own message
                else
                    defer_or_throw(attribute, ParseError{"undefined entity", error_offset, true});
            }

            // expat reads the whole start tag first (a malformed token anywhere in it wins) and only then expands the
            // references in the attribute values, attribute after attribute: an error in a value is kept until then.
            static void defer_or_throw(Attr *attribute, const ParseError &error)
            {
                if (!attribute)
                    throw error;
                if (!attribute->failed)
                {
                    attribute->failed = true;
                    attribute->failure = error;
                }
            }

            void attribute_value(uint32_t quote, size_t token_start, Attr &attribute)
            {
                std::string &value = attribute.value;
                for (;;)
                {
                    const uint32_t c = cp_;
                    if (c == CP_EOF)
                        fail_at(token_start, MSG_UNCLOSED_TOKEN);
                    if (c == quote)
                    {
                        advance();
                        return;
                    }
                    if (c == '<')
                        fail(MSG_INVALID_TOKEN);
                    if (c == '&')
                    {
                        reference(true, value, token_start, &attribute);
                        continue;
                    }
                    if (c == '\t' || c == '\n')
                    {
                        value += ' ';
                        advance();
                        continue;
                    }
                    if (c == '\r')
                    {
                        advance();
                        if (cp_ == '\n')
                            advance();
                        value += ' ';
                        continue;
                    }
                    if (!is_xml_char(c))
                        fail(MSG_INVALID_TOKEN);
                    append_utf8(value, c);
                    advance();
                }
            }

            void start_tag()
            {
                const size_t token_start = pos_;
                advance(); // '<'
                const std::string raw = scan_qname();
                if (raw.empty())
                    fail(MSG_INVALID_TOKEN);
                attrs_.clear();
                seen_.clear();
                bool empty = false;
                for (;;)
                {
                    const bool spaced = skip_space() > 0;
                    if (cp_ == '>')
                    {
                        advance();
                        break;
                    }
                    if (cp_ == '/')
                    {
                        advance();
                        if (cp_ == CP_EOF)
                            fail_at(token_start, MSG_UNCLOSED_TOKEN);
                        if (cp_ != '>')
                            fail(MSG_INVALID_TOKEN);
                        advance();
                        empty = true;
                        break;
                    }
                    if (cp_ == CP_EOF)
                        fail_at(token_start, MSG_UNCLOSED_TOKEN);
                    if (!spaced)
                        fail(MSG_INVALID_TOKEN);
                    Attr attribute;
                    attribute.offset = pos_;
                    attribute.name = scan_qname();
                    if (attribute.name.empty())
                        fail(MSG_INVALID_TOKEN);
                    skip_space();
                    if (cp_ == CP_EOF)
                        fail_at(token_start, MSG_UNCLOSED_TOKEN);
                    if (cp_ != '=')
                        fail(MSG_INVALID_TOKEN);
                    advance();
                    skip_space();
                    if (cp_ == CP_EOF)
                        fail_at(token_start, MSG_UNCLOSED_TOKEN);
                    if (cp_ != '"' && cp_ != '\'')
                        fail(MSG_INVALID_TOKEN);
                    const uint32_t quote = cp_;
                    advance();
                    attribute_value(quote, token_start, attribute);
                    attrs_.push_back(std::move(attribute));
                }
                for (const Attr &a : attrs_)
                {
                    if (!seen_.insert(a.name).second)
                        fail_at(a.offset, "duplicate attribute");
                    if (a.failed)
                        throw a.failure;
                }
                open_element(token_start, raw, empty);
            }

            // ---- namespaces (expat's storeAtts / addBinding) -------------------------------------------------------

            const std::string *lookup(const std::string &prefix) const
            {
                for (size_t i = bindings_.size(); i-- > 0;)
                    if (bindings_[i].prefix == prefix)
                        return &bindings_[i].uri;
                if (prefix == "xml")
                {
                    static const std::string xml_uri = XML_NAMESPACE;
                    return &xml_uri;
                }
                return nullptr;
            }

            void declare(const std::string &prefix, const std::string &uri, size_t token_start)
            {
                if (!prefix.empty() && uri.empty())
                    fail_at(token_start, "must not undeclare prefix");
                if (prefix == "xmlns")
                    fail_at(token_start, "reserved prefix (xmlns) must not be declared or undeclared");
                const bool must_be_xml = prefix == "xml";
                const bool is_xml_uri = uri == XML_NAMESPACE;
                if (uri.find('}') != std::string::npos)
                    fail_at(token_start, MSG_SYNTAX);
                if (must_be_xml != is_xml_uri)
                    fail_at(token_start, must_be_xml ? "reserved prefix (xml) must not be undeclared or bound to another namespace name"
                                                     : "prefix must not be bound to one of the reserved namespace names");
                if (uri == XMLNS_NAMESPACE)
                    fail_at(token_start, "prefix must not be bound to one of the reserved namespace names");
                bindings_.push_back({prefix, uri});
            }

            // {uri}local in a spelling pugixml accepts and no plain name can have (a plain name has no ':').
            static std::string namespaced_name(const std::string &uri, const std::string &local)
            {
                static const char hex[] = "0123456789abcdef";
                std::string name = "_ns:";
                for (char c : uri)
                {
                    const unsigned char u = static_cast<unsigned char>(c);
                    name += hex[u >> 4];
                    name += hex[u & 0x0F];
                }
                name += ':';
                name += local;
                return name;
            }

            void open_element(size_t token_start, const std::string &raw, bool empty)
            {
                const size_t mark = bindings_.size();
                for (const Attr &a : attrs_)
                {
                    if (a.name == "xmlns")
                        declare(std::string(), a.value, token_start);
                    else if (a.name.compare(0, 6, "xmlns:") == 0)
                        declare(a.name.substr(6), a.value, token_start);
                }

                std::string out_name;
                const size_t colon = raw.find(':');
                if (colon == std::string::npos)
                {
                    const std::string *uri = lookup(std::string());
                    out_name = (uri && !uri->empty()) ? namespaced_name(*uri, raw) : raw;
                }
                else
                {
                    const std::string *uri = lookup(raw.substr(0, colon));
                    if (!uri)
                        fail_at(token_start, "unbound prefix");
                    out_name = namespaced_name(*uri, raw.substr(colon + 1));
                }

                out_ += '<';
                out_ += out_name;
                std::vector<std::pair<std::string, std::string>> expanded; // (uri, local) of the prefixed attributes
                for (const Attr &a : attrs_)
                {
                    if (a.name == "xmlns" || a.name.compare(0, 6, "xmlns:") == 0)
                        continue;
                    std::string out_attribute;
                    const size_t attribute_colon = a.name.find(':');
                    if (attribute_colon == std::string::npos)
                    {
                        out_attribute = a.name;
                    }
                    else
                    {
                        const std::string *uri = lookup(a.name.substr(0, attribute_colon));
                        if (!uri)
                            fail_at(token_start, "unbound prefix");
                        const std::string local = a.name.substr(attribute_colon + 1);
                        for (const auto &e : expanded)
                            if (e.first == *uri && e.second == local)
                                fail_at(token_start, "duplicate attribute");
                        expanded.emplace_back(*uri, local);
                        out_attribute = namespaced_name(*uri, local);
                    }
                    out_ += ' ';
                    out_ += out_attribute;
                    out_ += "=\"";
                    append_attribute_text(a.value);
                    out_ += '"';
                }
                if (empty)
                {
                    out_ += "/>";
                    bindings_.resize(mark);
                }
                else
                {
                    out_ += '>';
                    stack_.push_back({raw, out_name, mark});
                }
            }

            void end_tag()
            {
                const size_t token_start = pos_;
                skip(2); // "</"
                const size_t name_offset = pos_;
                const std::string raw = scan_qname();
                if (raw.empty())
                    fail(MSG_INVALID_TOKEN);
                skip_space();
                if (cp_ == CP_EOF)
                    fail_at(token_start, MSG_UNCLOSED_TOKEN);
                if (cp_ != '>')
                    fail(MSG_INVALID_TOKEN);
                if (raw != stack_.back().raw)
                    fail_at(name_offset, "mismatched tag");
                advance();
                out_ += "</";
                out_ += stack_.back().out;
                out_ += '>';
                bindings_.resize(stack_.back().bindings);
                stack_.pop_back();
            }

            const unsigned char *d_;
            size_t n_;
            Enc enc_ = Enc::Utf8;
            size_t start_ = 0; // byte offset of the first character after the byte order mark
            size_t pos_ = 0;
            size_t tok_ = 0; // where the token being read starts (for "unclosed token")
            uint32_t cp_ = CP_EOF;
            size_t len_ = 0;
            bool standalone_ = false;
            bool has_external_ = false;
            std::string out_;
            std::vector<Attr> attrs_;
            std::unordered_set<std::string> seen_;
            std::vector<Binding> bindings_;
            std::vector<Open> stack_;
        };
    }

    NormalizedXml normalize_xml(const char *data, size_t length)
    {
        Parser parser(data, length);
        return parser.run();
    }
}
