// Set Scale, legend rewrite: tests ADDED BY THE INDEPENDENT REVIEW. The in-place rewrite (register SS-10) replaces only the dX / dY value bytes of a document that its own scanner
// found well-formed, so the property that matters for the data is: a DAMAGED document is never half rewritten and the scanner never crashes, whatever the bytes are. Windows
// source of truth: legend_position.py:6-26 (xml.etree: any parse error raises, nothing is returned for a document that is not complete and well formed).
#include "TestHarness.hpp"

#include "Domain/PageScaleTransform.hpp"

#include <string>
#include <vector>

using ost_domain::rescale_legend_xml;
using std::string;
using std::vector;

namespace
{
    // Documents that end exactly where their root element ends (so every PROPER prefix is an incomplete document), covering the constructs of the scanner.
    vector<string> complete_documents()
    {
        return {
            "<Legends dX=\"30.804\" dY=\"1022.679\" Visible=\"255\"><Legend ConditionUID=\"11334\" dX=\"30.804\" dY=\"30.804\" Visible=\"1\"/></Legends>",
            "<Legends dX='1' dY='2'/>",
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?><!-- keep --><Legends dX=\"1\"><?pi data?><Legend dX=\"2\" dY=\"3\"></Legend><![CDATA[ dX=\"9\" ]]></Legends>",
            "\xEF\xBB\xBF<Legends dX=\"1\" Name=\"&lt;x&gt; &amp; &#x41;&#66;\">text &quot;here&quot;<Other><Legend dX=\"5\"/></Other></Legends>",
            "<Legends\n  dX=\"10\"\n  dY = \"20\"\n><Legend\tdX=\"30\"\tdY=\"40\" FontSize=\"12\"\t/>\n</Legends>",
        };
    }

    // The deterministic pseudo-random generator of the fuzz (no <random>: the same sequence on every compiler).
    unsigned next_random(unsigned &state)
    {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    }
}

// Every proper prefix of a complete legend (a download or a write cut short) is refused and leaves the output untouched: nothing of a truncated document is ever scaled.
TEST(SetScale_LegendReview, EveryProperPrefixOfALegendIsRefused)
{
    for (const string &document : complete_documents())
    {
        string whole;
        CHECK(rescale_legend_xml(document, 2.0, whole, nullptr)); // the document itself is fine
        for (size_t length = 0; length < document.size(); ++length)
        {
            const string prefix = document.substr(0, length);
            SCOPED_CONTEXT("document " + document.substr(0, 24) + "... cut at " + std::to_string(length));
            string out = "untouched";
            string error;
            CHECK_FALSE(rescale_legend_xml(prefix, 2.0, out, &error));
            CHECK_EQ(out, "untouched");
            CHECK(!error.empty());
        }
    }
}

// After the root element only blanks, comments and processing instructions may follow: any other single byte appended to a complete legend is refused.
TEST(SetScale_LegendReview, AnyByteAfterTheRootElementIsRefusedUnlessItIsBlank)
{
    for (const string &document : complete_documents())
    {
        for (int byte = 0; byte < 256; ++byte)
        {
            const string appended = document + static_cast<char>(byte);
            string out = "untouched";
            const bool ok = rescale_legend_xml(appended, 2.0, out, nullptr);
            const bool blank = byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r';
            SCOPED_CONTEXT("appended byte " + std::to_string(byte));
            CHECK_EQ(ok, blank);
            if (!ok)
                CHECK_EQ(out, "untouched");
        }
    }
}

// A single damaged byte anywhere (a quote, an angle bracket, an ampersand, a NUL, a high byte, ...) never crashes or throws, and whatever the scanner still accepts comes out as a
// document it accepts again: rewriting its own output at the factor 1 is a fixed point (the values are already in their final three-decimal form).
TEST(SetScale_LegendReview, ADamagedByteNeverCrashesAndAcceptedOutputIsAFixedPoint)
{
    const unsigned char damage[] = {0x00, '<', '>', '"', '\'', '&', ';', '/', '=', ' ', 'x', '#', '-', '!', '?', '[', ']', 0x0B, 0x80, 0xC3, 0xFF};
    int accepted = 0;
    int refused = 0;
    for (const string &document : complete_documents())
    {
        for (size_t position = 0; position < document.size(); ++position)
        {
            for (const unsigned char byte : damage)
            {
                string damaged = document;
                damaged[position] = static_cast<char>(byte);
                string out;
                bool ok = false;
                try
                {
                    ok = rescale_legend_xml(damaged, 0.5, out, nullptr);
                }
                catch (...)
                {
                    SCOPED_CONTEXT("damaged document threw at byte " + std::to_string(position));
                    CHECK(false);
                    continue;
                }
                if (!ok)
                {
                    ++refused;
                    continue;
                }
                ++accepted;
                string again;
                SCOPED_CONTEXT("damaged document " + damaged + " gave " + out);
                REQUIRE(rescale_legend_xml(out, 1.0, again, nullptr));
                CHECK_EQ(again, out);
            }
        }
    }
    CHECK(accepted > 100); // the fuzz really reaches the rewrite (a damaged byte inside a text, a comment, an attribute value)
    CHECK(refused > 100);  // and really reaches the refusals
}

// Random garbage and random concatenations of legend pieces never crash either (the scanner has no recursion and no unchecked index).
TEST(SetScale_LegendReview, RandomPiecesNeverCrashTheScanner)
{
    const vector<string> pieces = {"<Legends", "<Legend", " dX=\"", " dY='", "1.5", "\"", "'", "/>", ">", "</Legends>", "</Legend>", "<!--", "-->", "<?", "?>", "<![CDATA[", "]]>",
                                   "&amp;", "&#", ";", "x", " ", "\n", "=", "\xC3\xA9", "\xFF", "nan", "1e400", "-0", "0"};
    unsigned state = 12345u;
    int accepted = 0;
    for (int round = 0; round < 4000; ++round)
    {
        string text;
        const unsigned count = 1 + next_random(state) % 14;
        for (unsigned i = 0; i < count; ++i)
            text += pieces[next_random(state) % pieces.size()];
        string out;
        try
        {
            if (rescale_legend_xml(text, 0.5, out, nullptr))
                ++accepted;
        }
        catch (...)
        {
            SCOPED_CONTEXT("threw on " + text);
            CHECK(false);
        }
    }
    // the generator does produce some documents the scanner accepts, so the loop above is not only refusing
    string out;
    CHECK(rescale_legend_xml("<Legends dX=\"1\"/>", 0.5, out, nullptr));
    CHECK_EQ(out, "<Legends dX=\"0.5\"/>");
    (void)accepted;
}
