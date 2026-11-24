// Set Scale, the legend XML scanner (plan 5.4, SS-10; added with the implementation). The in-place rewrite of rescale_legend_xml needs its own well-formedness check, because
// the Windows function gets one for free from xml.etree (expat) and the rewrite must never touch a text that expat would refuse. These tests pin the decisions of that scanner
// that no Windows test pins one by one (test_page_scale_legend.cpp holds the ported Windows cases): what is refused, and that bytes outside ASCII are opaque.
// Windows source of truth: legend_position.py:6-26 (ET.fromstring raises ParseError for every text refused below).
#include "TestHarness.hpp"

#include "Domain/PageScaleTransform.hpp"

#include <string>

using namespace ost_domain;
using std::string;

namespace
{
    // true when the rewrite is refused; `out` must stay as it was.
    bool refused(const string &xml, double factor = 2.0)
    {
        string out = "<untouched>";
        string error;
        const bool ok = rescale_legend_xml(xml, factor, out, &error);
        CHECK_STR_EQ(out, "<untouched>");
        if (!ok)
            CHECK(!error.empty());
        return !ok;
    }

    string rewritten(const string &xml, double factor = 2.0)
    {
        string out = "<untouched>";
        string error;
        if (!rescale_legend_xml(xml, factor, out, &error))
            FAIL_TEST("legend refused: " + xml + " (" + error + ")");
        return out;
    }
}

// What expat refuses is refused: a duplicate attribute, an undefined entity, a character reference that is no XML character, a '<' or a bare '&' in an attribute value, a '&' that is
// no reference in the text, "]]>" in the text, two attributes with no blank between them, an attribute without a value or without quotes, a name that does not start like a name.
TEST(SetScale_LegendScanner, WhatExpatRefusesIsRefused)
{
    for (const char *xml : {
             "<Legends dX=\"1\" dX=\"2\"/>",                // duplicate attribute
             "<Legends dX=\"1\">&nbsp;</Legends>",           // undefined entity in the text
             "<Legends dX=\"&nbsp;\"/>",                      // undefined entity in a value
             "<Legends dX=\"&#0;\"/>",                        // NUL is no XML character
             "<Legends dX=\"&#xD800;\"/>",                    // a surrogate is no XML character
             "<Legends dX=\"&#x110000;\"/>",                  // beyond U+10FFFF
             "<Legends dX=\"&#;\"/>",                         // no digits
             "<Legends dX=\"&amp\"/>",                        // no ';'
             "<Legends dX=\"1<2\"/>",                         // '<' in a value
             "<Legends dX=\"1\">a & b</Legends>",             // a bare '&'
             "<Legends dX=\"1\">a ]]> b</Legends>",           // "]]>" in the text
             "<Legends dX=\"1\"dY=\"2\"/>",                   // no blank between two attributes
             "<Legends dX/>",                                 // an attribute without a value
             "<Legends dX=1/>",                               // an unquoted value
             "<Legends 1=\"1\"/>",                            // an attribute name that starts with a digit
             "<1Legends/>",                                   // an element name that starts with a digit
             "<Legends dX=\"1\"></Legends x>",                // junk in the end tag
             "<Legends dX=\"1\"><Legend></Legends></Legend>", // crossed tags
             "<Legends dX=\"1\"/>trailing",                   // text after the root
             "<Legends dX=\"1\"/><!-- ok --><Legends/>",      // a second root
             "text<Legends dX=\"1\"/>",                       // text before the root
             "<!DOCTYPE Legends><Legends dX=\"1\"/>",         // a DTD (no entity definitions are supported)
             "<Legends dX=\"1\"><!-- a -- b --></Legends>",   // "--" inside a comment
             "<Legends dX=\"1\"><!-- unterminated </Legends>",
             "<Legends dX=\"1\"><?xml version=\"1.0\"?></Legends>", // the declaration is only valid at the start
             "<Legends dX=\"1\"><![CDATA[ unterminated </Legends>",
             "<Legends dX=\"1\"/>\x01",                       // a control character
             "<?xml version=\"1.0\"?",                        // an unterminated declaration
         })
    {
        SCOPED_CONTEXT(string("legend ") + xml);
        CHECK(refused(xml));
    }
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\"/>"), "<Legends dX=\"2\"/>"); // the control
}

// What expat accepts is rewritten: the five entities in the text and in a value, character references, a single-quoted value, blanks around '=' and before '>' and '/>', comments and
// processing instructions before and after the root, a CDATA section, a name with ':' / '-' / '.' / '_', a byte order mark, a declaration.
TEST(SetScale_LegendScanner, WhatExpatAcceptsIsRewritten)
{
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\">&lt;&gt;&amp;&quot;&apos;&#65;&#x42;</Legends>"), "<Legends dX=\"2\">&lt;&gt;&amp;&quot;&apos;&#65;&#x42;</Legends>");
    CHECK_STR_EQ(rewritten("<Legends dX = '1' dY\n=\t\"2\" ></Legends >"), "<Legends dX = '2' dY\n=\t\"4\" ></Legends >");
    CHECK_STR_EQ(rewritten("<!-- a --><?pi x?><Legends dX=\"1\"/><!-- b --><?pi y?>\n"), "<!-- a --><?pi x?><Legends dX=\"2\"/><!-- b --><?pi y?>\n");
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\"><![CDATA[a < b & c]]></Legends>"), "<Legends dX=\"2\"><![CDATA[a < b & c]]></Legends>");
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\"><a:b-c.d_e x:y=\"1\"/></Legends>"), "<Legends dX=\"2\"><a:b-c.d_e x:y=\"1\"/></Legends>");
    CHECK_STR_EQ(rewritten("\xEF\xBB\xBF<Legends dX=\"1\"/>"), "\xEF\xBB\xBF<Legends dX=\"2\"/>");
    CHECK_STR_EQ(rewritten("<?xml version=\"1.0\" standalone=\"yes\"?><Legends dX=\"1\"/>"), "<?xml version=\"1.0\" standalone=\"yes\"?><Legends dX=\"2\"/>");
    // a value written with references and blanks is read as its decoded text: "&#x31;&#x30;" is "10", " 4 " is 4
    CHECK_STR_EQ(rewritten("<Legends dX=\"&#x31;&#x30;\" dY=\" 4 \"/>"), "<Legends dX=\"20\" dY=\"8\"/>");
    // deep nesting and the Legends element inside a Legend
    CHECK_STR_EQ(rewritten("<Legends><Legend dX=\"1\"><Legends dY=\"2\"><Legend dX=\"3\"/></Legends></Legend></Legends>"),
                 "<Legends><Legend dX=\"2\"><Legends dY=\"4\"><Legend dX=\"6\"/></Legends></Legend></Legends>");
}

// Bytes outside ASCII are opaque (register SS-10): a legend a Windows build wrote as latin-1 (a lone 0xE9) is not valid UTF-8 and must still scale, because the rewrite replaces only the
// digits of dX / dY and copies every other byte. The same holds for valid UTF-8 in names, values, comments and text.
TEST(SetScale_LegendScanner, BytesOutsideAsciiAreCopiedAsTheyAre)
{
    const string latin1 = "caf\xE9";
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\" Name=\"" + latin1 + "\"><!-- " + latin1 + " --><Legend dX=\"2\">" + latin1 + "</Legend></Legends>"),
                 "<Legends dX=\"2\" Name=\"" + latin1 + "\"><!-- " + latin1 + " --><Legend dX=\"4\">" + latin1 + "</Legend></Legends>");
    const string utf8 = "caf\xC3\xA9 \xE2\x82\xAC";
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\" N\xC3\xA4me=\"" + utf8 + "\"/>"), "<Legends dX=\"2\" N\xC3\xA4me=\"" + utf8 + "\"/>");
    // a non-ASCII byte INSIDE a coordinate is not a number
    CHECK(refused("<Legends dX=\"1\xE9\"/>"));
}

// Only the values change, and every dX / dY value is rewritten even at a factor of 1, with the Windows three-decimal rule ("1.50" -> "1.5", " 7 " -> "7"), exactly like the Windows
// function; an attribute that is not dX / dY and all the text around the values are never touched.
TEST(SetScale_LegendScanner, AValueIsRewrittenEvenAtFactorOneWithTheThreeDecimalRule)
{
    CHECK_STR_EQ(rewritten("<Legends dX=\"1.50\" dY=\" 7 \" Other=\"1.50\"/>", 1.0), "<Legends dX=\"1.5\" dY=\"7\" Other=\"1.50\"/>");
    CHECK_STR_EQ(rewritten("<Legends/>", 1.0), "<Legends/>");
}
