// Layer: Domain. The legend XML of Set Scale (rescale_legend_xml, declared in PageScaleTransform.hpp).
// Windows source of truth: ost_visualizer/infrastructure/mdb/components/legend_position.py:6-26 (xml.etree parse, every Legends / Legend element's dX and dY
// times the factor, f"{v:.3f}".rstrip("0").rstrip(".")), with the deliberate difference of register SS-10: the values are replaced IN PLACE (the rest of the text is
// kept byte for byte, UTF-8) instead of re-serialising the tree.
//
// The scanner below is a small well-formedness checker (the checks expat makes that matter for a legend: one root, matching tags, quoted attribute values, no '<' in a
// value, only the five predefined entities and valid character references, no duplicate attribute, comments / processing instructions / CDATA in the right places, no
// DOCTYPE) that records where the dX / dY values of the Legends / Legend elements are. Nothing is written unless the whole document is well formed and every value
// scales; then exactly those value bytes are replaced.
//
// Known differences from expat (addendum of SS-10; no OST writer produces such a document, the direction is pinned by test_page_scale_legend_fuzz.cpp):
//  (1) a DOCTYPE is refused (expat accepts it): Set Scale on such a page is refused where Windows would rewrite it (fails closed);
//  (2) a damaged XML declaration is accepted (only `<?xml`, a blank and `?>` are looked for; expat checks version / encoding / standalone);
//  (3) a name with a colon is accepted (expat refuses an unbound prefix);
//  (4) a default namespace (xmlns="...") is ignored: Windows refuses the root and leaves a namespaced Legend alone, the scanner rewrites both.
#include "NumberText.hpp"
#include "PageScaleTransform.hpp"
#include "PositionParser.hpp"

#include <algorithm>
#include <cmath>

namespace ost_domain
{
    namespace
    {
        struct CoordinateAttribute
        {
            bool present = false;
            std::size_t begin = 0; // the value bytes, between the quotes
            std::size_t end = 0;
            std::string decoded; // the value as the parser reports it (references decoded, blanks normalised)
        };

        struct LegendElement
        {
            CoordinateAttribute dx;
            CoordinateAttribute dy;
        };

        struct ParsedLegend
        {
            std::string root;
            std::vector<LegendElement> elements; // Legends / Legend elements in document order
        };

        bool is_xml_space(char c)
        {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r';
        }

        bool is_name_start(char c)
        {
            const unsigned char u = static_cast<unsigned char>(c);
            return (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || u == '_' || u == ':' || u >= 0x80;
        }

        bool is_name_char(char c)
        {
            const unsigned char u = static_cast<unsigned char>(c);
            return is_name_start(c) || (u >= '0' && u <= '9') || u == '-' || u == '.';
        }

        bool is_forbidden_control(char c)
        {
            const unsigned char u = static_cast<unsigned char>(c);
            return u < 0x20 && u != '\t' && u != '\n' && u != '\r';
        }

        void append_utf8(std::string &out, unsigned long cp)
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

        bool is_xml_char(unsigned long cp)
        {
            return cp == 0x9 || cp == 0xA || cp == 0xD || (cp >= 0x20 && cp <= 0xD7FF) || (cp >= 0xE000 && cp <= 0xFFFD) || (cp >= 0x10000 && cp <= 0x10FFFF);
        }

        int hex_value(char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            return -1;
        }

        class Scanner
        {
        public:
            explicit Scanner(const std::string &text) : _s(text), _n(text.size()) {}

            bool parse(ParsedLegend &out)
            {
                // Bytes outside ASCII are opaque: a legend a Windows build wrote as latin-1 must still scale (the rewrite only touches ASCII digits, every other
                // byte is copied as it is), so the text is NOT required to be valid UTF-8.
                std::size_t i = 0;
                if (_s.compare(0, 3, "\xEF\xBB\xBF") == 0)
                    i = 3;
                if (_s.compare(i, 5, "<?xml") == 0 && i + 5 < _n && is_xml_space(_s[i + 5]))
                {
                    const std::size_t end = _s.find("?>", i);
                    if (end == std::string::npos)
                        return false;
                    i = end + 2;
                }
                if (!skip_misc(i))
                    return false;
                if (i >= _n || _s[i] != '<')
                    return false; // text before the root element, or no root at all
                bool self_closing = false;
                if (!start_tag(i, out, true, self_closing))
                    return false;
                while (!_open.empty())
                {
                    if (i >= _n)
                        return false; // an element that is never closed
                    if (_s[i] != '<')
                    {
                        if (!text(i))
                            return false;
                    }
                    else if (_s.compare(i, 2, "</") == 0)
                    {
                        if (!end_tag(i))
                            return false;
                    }
                    else if (_s.compare(i, 4, "<!--") == 0)
                    {
                        if (!comment(i))
                            return false;
                    }
                    else if (_s.compare(i, 9, "<![CDATA[") == 0)
                    {
                        const std::size_t end = _s.find("]]>", i + 9);
                        if (end == std::string::npos)
                            return false;
                        for (std::size_t k = i + 9; k < end; ++k)
                            if (is_forbidden_control(_s[k]))
                                return false;
                        i = end + 3;
                    }
                    else if (_s.compare(i, 2, "<?") == 0)
                    {
                        if (!processing_instruction(i))
                            return false;
                    }
                    else if (i + 1 < _n && _s[i + 1] == '!')
                    {
                        return false; // a DOCTYPE or anything else of the "<!" family inside the content
                    }
                    else if (!start_tag(i, out, false, self_closing))
                    {
                        return false;
                    }
                }
                if (!skip_misc(i))
                    return false;
                return i == _n; // nothing but comments, processing instructions and blanks after the root
            }

        private:
            bool skip_misc(std::size_t &i)
            {
                for (;;)
                {
                    while (i < _n && is_xml_space(_s[i]))
                        ++i;
                    if (_s.compare(i, 4, "<!--") == 0)
                    {
                        if (!comment(i))
                            return false;
                    }
                    else if (_s.compare(i, 2, "<?") == 0)
                    {
                        if (!processing_instruction(i))
                            return false;
                    }
                    else
                    {
                        return true;
                    }
                }
            }

            bool comment(std::size_t &i)
            {
                const std::size_t dashes = _s.find("--", i + 4);
                if (dashes == std::string::npos || dashes + 2 >= _n || _s[dashes + 2] != '>')
                    return false; // unterminated, or "--" inside the comment
                for (std::size_t k = i + 4; k < dashes; ++k)
                    if (is_forbidden_control(_s[k]))
                        return false;
                i = dashes + 3;
                return true;
            }

            bool processing_instruction(std::size_t &i)
            {
                std::size_t k = i + 2;
                if (k >= _n || !is_name_start(_s[k]))
                    return false;
                const std::size_t target_begin = k;
                while (k < _n && is_name_char(_s[k]))
                    ++k;
                std::string target = _s.substr(target_begin, k - target_begin);
                std::transform(target.begin(), target.end(), target.begin(), [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; });
                if (target == "xml")
                    return false; // the XML declaration is only valid at the very start
                if (k >= _n || !(is_xml_space(_s[k]) || _s.compare(k, 2, "?>") == 0))
                    return false;
                const std::size_t end = _s.find("?>", k);
                if (end == std::string::npos)
                    return false;
                for (std::size_t m = k; m < end; ++m)
                    if (is_forbidden_control(_s[m]))
                        return false;
                i = end + 2;
                return true;
            }

            // A reference at `i` (_s[i] == '&'): the five predefined entities and valid character references. On success `i` is past the ';' and `decoded` gets the text.
            bool reference(std::size_t &i, std::string &decoded)
            {
                std::size_t j = i + 1;
                if (j >= _n)
                    return false;
                if (_s[j] == '#')
                {
                    ++j;
                    bool hex = false;
                    if (j < _n && _s[j] == 'x')
                    {
                        hex = true;
                        ++j;
                    }
                    const std::size_t digits = j;
                    unsigned long cp = 0;
                    while (j < _n && _s[j] != ';')
                    {
                        const int d = hex ? hex_value(_s[j]) : ((_s[j] >= '0' && _s[j] <= '9') ? _s[j] - '0' : -1);
                        if (d < 0)
                            return false;
                        cp = cp * (hex ? 16 : 10) + static_cast<unsigned long>(d);
                        if (cp > 0x10FFFF)
                            return false;
                        ++j;
                    }
                    if (j >= _n || j == digits || !is_xml_char(cp))
                        return false;
                    append_utf8(decoded, cp);
                    i = j + 1;
                    return true;
                }
                std::size_t k = j;
                while (k < _n && _s[k] != ';' && k - j < 6)
                    ++k;
                if (k >= _n || _s[k] != ';')
                    return false;
                const std::string name = _s.substr(j, k - j);
                if (name == "lt")
                    decoded += '<';
                else if (name == "gt")
                    decoded += '>';
                else if (name == "amp")
                    decoded += '&';
                else if (name == "quot")
                    decoded += '"';
                else if (name == "apos")
                    decoded += '\'';
                else
                    return false; // an undefined entity (there is no DTD)
                i = k + 1;
                return true;
            }

            bool text(std::size_t &i)
            {
                while (i < _n && _s[i] != '<')
                {
                    if (_s[i] == '&')
                    {
                        std::string ignored;
                        if (!reference(i, ignored))
                            return false;
                        continue;
                    }
                    if (is_forbidden_control(_s[i]) || _s.compare(i, 3, "]]>") == 0)
                        return false;
                    ++i;
                }
                return true;
            }

            bool end_tag(std::size_t &i)
            {
                std::size_t k = i + 2;
                if (k >= _n || !is_name_start(_s[k]))
                    return false;
                const std::size_t begin = k;
                while (k < _n && is_name_char(_s[k]))
                    ++k;
                if (_s.compare(begin, k - begin, _open.back()) != 0 || _open.back().size() != k - begin)
                    return false;
                while (k < _n && is_xml_space(_s[k]))
                    ++k;
                if (k >= _n || _s[k] != '>')
                    return false;
                _open.pop_back();
                i = k + 1;
                return true;
            }

            bool start_tag(std::size_t &i, ParsedLegend &out, bool is_root, bool &self_closing)
            {
                self_closing = false;
                std::size_t k = i + 1;
                if (k >= _n || !is_name_start(_s[k]))
                    return false;
                const std::size_t name_begin = k;
                while (k < _n && is_name_char(_s[k]))
                    ++k;
                const std::string name = _s.substr(name_begin, k - name_begin);
                if (is_root)
                    out.root = name;
                const bool tracked = name == "Legends" || name == "Legend";
                LegendElement element;
                std::vector<std::string> names;
                for (;;)
                {
                    const std::size_t before = k;
                    while (k < _n && is_xml_space(_s[k]))
                        ++k;
                    const bool separated = k > before;
                    if (k >= _n)
                        return false;
                    if (_s[k] == '>')
                    {
                        ++k;
                        break;
                    }
                    if (_s[k] == '/')
                    {
                        if (k + 1 < _n && _s[k + 1] == '>')
                        {
                            k += 2;
                            self_closing = true;
                            break;
                        }
                        return false;
                    }
                    if (!separated || !is_name_start(_s[k]))
                        return false; // two attributes need a blank between them
                    const std::size_t attribute_begin = k;
                    while (k < _n && is_name_char(_s[k]))
                        ++k;
                    const std::string attribute = _s.substr(attribute_begin, k - attribute_begin);
                    while (k < _n && is_xml_space(_s[k]))
                        ++k;
                    if (k >= _n || _s[k] != '=')
                        return false;
                    ++k;
                    while (k < _n && is_xml_space(_s[k]))
                        ++k;
                    if (k >= _n || (_s[k] != '"' && _s[k] != '\''))
                        return false;
                    const char quote = _s[k++];
                    const std::size_t value_begin = k;
                    std::string decoded;
                    for (;;)
                    {
                        if (k >= _n)
                            return false;
                        const char c = _s[k];
                        if (c == quote)
                            break;
                        if (c == '<' || is_forbidden_control(c))
                            return false;
                        if (c == '&')
                        {
                            if (!reference(k, decoded))
                                return false;
                            continue;
                        }
                        decoded += (c == '\t' || c == '\n' || c == '\r') ? ' ' : c;
                        ++k;
                    }
                    const std::size_t value_end = k;
                    ++k; // the closing quote
                    if (std::find(names.begin(), names.end(), attribute) != names.end())
                        return false; // a duplicate attribute
                    names.push_back(attribute);
                    if (tracked && (attribute == "dX" || attribute == "dY"))
                    {
                        CoordinateAttribute &target = attribute == "dX" ? element.dx : element.dy;
                        target.present = true;
                        target.begin = value_begin;
                        target.end = value_end;
                        target.decoded = decoded;
                    }
                }
                if (tracked)
                    out.elements.push_back(element);
                if (!self_closing)
                    _open.push_back(name);
                i = k;
                return true;
            }

            const std::string &_s;
            std::size_t _n;
            std::vector<std::string> _open;
        };

        bool refuse(std::string *error, const std::string &text)
        {
            if (error)
                *error = text;
            return false;
        }
    }

    bool rescale_legend_xml(const std::string &xml, double factor, std::string &out, std::string *error)
    {
        ParsedLegend parsed;
        Scanner scanner(xml);
        if (!scanner.parse(parsed))
            return refuse(error, "Legend Position is not well-formed XML");
        if (parsed.root != "Legends")
            return refuse(error, "Legend Position requires a Legends XML root");

        struct Replacement
        {
            std::size_t begin;
            std::size_t end;
            std::string text;
        };
        std::vector<Replacement> replacements;
        for (const LegendElement &element : parsed.elements)
        {
            for (const CoordinateAttribute *coordinate : {&element.dx, &element.dy})
            {
                if (!coordinate->present)
                    continue;
                double value = 0.0;
                if (!parse_python_float(coordinate->decoded, value))
                    return refuse(error, "Legend Position coordinate \"" + coordinate->decoded + "\" is not a number");
                const double scaled = value * factor;
                if (!std::isfinite(scaled))
                    return refuse(error, "Legend Position coordinates must be finite");
                // SS-2 hardening: a non-zero value must not be destroyed by the multiplication (a factor of exactly 0 is the pinned Windows collapse to 0).
                if (value != 0.0 && scaled == 0.0 && factor != 0.0)
                    return refuse(error, "Legend Position coordinates underflow");
                replacements.push_back({coordinate->begin, coordinate->end, format_fixed_trimmed(scaled, 3)});
            }
        }
        std::sort(replacements.begin(), replacements.end(), [](const Replacement &a, const Replacement &b) { return a.begin < b.begin; });
        std::string result;
        result.reserve(xml.size());
        std::size_t position = 0;
        for (const Replacement &replacement : replacements)
        {
            result.append(xml, position, replacement.begin - position);
            result += replacement.text;
            position = replacement.end;
        }
        result.append(xml, position, std::string::npos);
        out = result;
        return true;
    }
}
