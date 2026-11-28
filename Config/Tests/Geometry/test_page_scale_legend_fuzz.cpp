// Set Scale, REVIEWER differential test of the hand-written legend scanner (PageScaleLegend.cpp) against the REAL Windows function: xml.etree / expat decides whether a legend
// is well-formed, and the iOS scanner must agree on every document, because a legend that Windows accepts but iOS refuses would make Set Scale impossible on that page, and a
// legend that Windows refuses but iOS accepts would rewrite text that Windows would have refused to touch.
// The rows (scale_legend_fuzz::kRows) are templates and seeded random mutations of them (a deleted / inserted / replaced / truncated / duplicated piece), run through
// rescale_legend_position(text, 2.0) read-only (python -B -I). Every number in the documents is plain, so a Windows refusal is a well-formedness or root refusal.
// Register SS-10 (the in-place rewrite): only the accept / refuse decision is compared here, the rewritten text is pinned by test_page_scale_legend.cpp and the fuzz oracle.
//
// KNOWN differences from expat, found by this test (review of m1-set-scale), all on documents that no OST writer produces and all in the SAFE direction except the first:
//   - a DOCTYPE: expat accepts it, the scanner refuses the whole legend (so Set Scale refuses the page: safe, but Windows would have rewritten it);
//   - an XML declaration whose pseudo-attributes are damaged (`<?xml version="1.;0"?>`): expat refuses, the scanner only looks for `<?xml` + blank + `?>` and accepts;
//   - a name with a colon (an unbound prefix, `a:b`, `x:`): expat refuses (namespace processing), the scanner takes ':' as a name character and accepts;
//   - a default namespace declaration (`xmlns="urn:x"`): xml.etree reads the root as {urn:x}Legends and Windows refuses it as "not a Legends root", and it does not take a
//     {urn:x}Legend for a Legend (its dX stays); the scanner has no namespaces and rewrites both.
// A row that contains one of those four markers may differ in exactly the direction named above; EVERY OTHER row must agree exactly, and a row that agrees is always fine, so
// a later tightening of the scanner keeps this test green while a new kind of disagreement fails it.
// The last 280 rows are hand-picked: each aims at ONE branch of the scanner (a '<' or a control character in a value, a ]]> in text, every kind of reference, the comment, the
// processing instruction, the declaration, the tag and the prolog rules, duplicate attributes, a BOM). The Windows verdict of a row that Windows refuses only because the OUTPUT
// is not latin-1 (register SS-10: a character beyond U+00FF) is the verdict of the parse alone.
#include "TestHarness.hpp"

#include "Domain/PageScaleTransform.hpp"

#include "PageScaleLegendFuzzData.hpp"

#include <cstddef>
#include <string>

using namespace ost_domain;
using std::string;

namespace
{
    string printable(const char *text)
    {
        static const char *hex = "0123456789abcdef";
        string shown;
        for (const char *p = text; *p; ++p)
        {
            const unsigned char c = static_cast<unsigned char>(*p);
            if (c < 0x20 || c >= 0x7F)
            {
                shown += "\\x";
                shown += hex[c >> 4];
                shown += hex[c & 15];
            }
            else
                shown += static_cast<char>(c);
        }
        return shown;
    }

    bool is_name_char(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == ':' || c == '-';
    }

    bool is_blank(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    // Every `dX` / `dY` attribute value of a text, wherever it stands (also inside comments or other elements), in document order, joined by 0x1F. The same scan is applied by the
    // generator to the Windows output: a name that continues a longer name (xdX, a:dX) is not an attribute named dX, blanks around '=' are allowed, either quote.
    string dx_dy_values(const string &text)
    {
        string joined;
        bool first = true;
        for (size_t i = 0; i + 1 < text.size(); ++i)
        {
            if (text[i] != 'd' || (text[i + 1] != 'X' && text[i + 1] != 'Y') || (i > 0 && is_name_char(text[i - 1])))
                continue;
            size_t j = i + 2;
            while (j < text.size() && is_blank(text[j]))
                ++j;
            if (j >= text.size() || text[j] != '=')
                continue;
            ++j;
            while (j < text.size() && is_blank(text[j]))
                ++j;
            if (j >= text.size() || (text[j] != '"' && text[j] != '\''))
                continue;
            const char quote = text[j++];
            const size_t end = text.find(quote, j);
            if (end == string::npos)
                continue;
            if (!first)
                joined += '\x1f';
            first = false;
            joined += text.substr(j, end - j);
            i = end;
        }
        return joined;
    }
}

TEST(SetScale_LegendFuzz, TheScannerAgreesWithExpatOnEveryRowExceptTheKnownDifferences)
{
    size_t index = 0;
    size_t accepted = 0;
    size_t differences = 0;
    size_t compared = 0;
    for (const scale_legend_fuzz::Row &row : scale_legend_fuzz::kRows)
    {
        SCOPED_CONTEXT("row " + std::to_string(index++) + ": " + printable(row.xml) + (row.windows_ok ? "   [Windows accepts]" : "   [Windows refuses]"));
        const string xml = row.xml;
        string out = "<untouched>";
        string error;
        const bool ok = rescale_legend_xml(xml, 2.0, out, &error);
        if (ok)
            ++accepted;
        else
            CHECK_STR_EQ(out, "<untouched>"); // a refusal writes nothing
        // (a document with a namespace declaration is left out of the value comparison: xml.etree does not take {urn:x}Legend for a Legend and leaves its dX alone, the scanner has
        // no namespaces and scales it: the fourth known difference above)
        if (ok && row.windows_ok && string(row.values).rfind("\x1e", 0) != 0 && xml.find("xmlns") == string::npos)
        {
            // both rewrote it: every dX / dY value of the output (by the scan below) is the value of the Windows output, in the same order, so the scanner replaced exactly the
            // values that xml.etree replaced and no other
            CHECK_EQ(dx_dy_values(out), string(row.values));
            ++compared;
        }
        if (ok == row.windows_ok)
            continue;
        ++differences;
        const bool doctype = xml.find("<!DOCTYPE") != string::npos;
        const bool declaration = xml.find("<?xml") != string::npos;
        const bool colon = xml.find(':') != string::npos;
        const bool xmlns = xml.find("xmlns") != string::npos;
        if (ok && !row.windows_ok)
            CHECK(declaration || colon || xmlns); // the scanner is more permissive only for a damaged declaration, a name with a colon or a default namespace
        else
            CHECK(doctype); // the scanner is stricter only for a DOCTYPE
    }
    CHECK(index > 1000);
    CHECK(accepted > 100);
    CHECK(compared > 100);
    CHECK(differences < 60); // the known differences are a handful of rows, never a trend
}
