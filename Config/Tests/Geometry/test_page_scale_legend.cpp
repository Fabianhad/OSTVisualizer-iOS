// Set Scale, the legend XML (BidLegends.Position) (plan 5.4, tests-first). Windows source of truth: legend_position.py:6-26 rescale_legend_position and
// tests/infrastructure/mdb/components/test_legend_position.py. iOS rewrites dX / dY IN PLACE (register SS-10): the bytes outside the values are kept, UTF-8, no
// xml.etree normalisation. So the SEVEN Windows tests whose expected text is the xml.etree byte form (`<Legend ... />` with a space, an empty element written as a
// self-closing tag) are ADAPTED: same inputs, same expected dX / dY values (the three-decimal trim table, the scaling, independent X and Y, the attribute order, the
// zero factor), the expected text being the INPUT with only the values replaced. They carry "_Adapted" in their name (:53, :71, :83, :92, :96, :119, :169).
// The other tests port by intent: the root rule, malformed XML, comments and processing instructions kept, text and tail kept, non-finite and non-numeric refused.
// Not ported (Python type tests): bytes / bytearray / str input (:9), latin-1 decode / encode (:22), a bytearray source left unmodified (:173).
#include "TestHarness.hpp"

#include "Domain/PageScaleTransform.hpp"

#include "PageScaleTransformOracleData.hpp"

#include <limits>
#include <string>

using namespace ost_domain;
using std::string;

namespace
{
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    constexpr double kNotANumber = std::numeric_limits<double>::quiet_NaN();

    // The rewritten text; FAIL_TEST when the rewrite is refused.
    string rewritten(const string &xml, double factor)
    {
        string out = "<untouched>";
        string error;
        if (!rescale_legend_xml(xml, factor, out, &error))
            FAIL_TEST("legend refused: " + xml + " (" + error + ")");
        return out;
    }

    // true when refused; `out` must be left exactly as it was and `error` must say why.
    bool refused(const string &xml, double factor, string *error_out = nullptr)
    {
        string out = "<untouched>";
        string error;
        const bool ok = rescale_legend_xml(xml, factor, out, &error);
        CHECK_STR_EQ(out, "<untouched>"); // nothing is written on a refusal
        if (!ok)
            CHECK(!error.empty());
        if (error_out)
            *error_out = error;
        return !ok;
    }
}

// ---- input rules ------------------------------------------------------------------------------------------------------------------------

// test_legend_position.py:33 test_non_legends_root_is_rejected_before_any_scaling: Legend, lower-case legends and a Legends nested in another root are refused.
TEST(SetScale_Legend, NonLegendsRootIsRejectedBeforeAnyScaling)
{
    for (const char *source : {"<Legend dX=\"1\" dY=\"2\"/>", "<legends dX=\"1\"/>", "<Position dX=\"1\"><Legends dX=\"1\"/></Position>"})
    {
        SCOPED_CONTEXT(string("legend ") + source);
        string error;
        CHECK(refused(source, 2.0, &error));
        CHECK_STR_EQ(error, "Legend Position requires a Legends XML root");
    }
}

// test_legend_position.py:45 test_malformed_xml_raises_parse_error: "", "not xml" and an unclosed element are refused (iOS: false, `out` untouched).
TEST(SetScale_Legend, MalformedXmlIsRefused)
{
    for (const char *source : {"", "not xml", "<Legends dX=\"1\">", "<Legends", "<Legends dX=\"1\"></Legend>", "<Legends dX=1/>", "<Legends/><Legends/>"})
    {
        SCOPED_CONTEXT(string("'") + source + "'");
        CHECK(refused(source, 2.0));
    }
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\"/>", 2.0), "<Legends dX=\"2\"/>"); // the control: a well-formed legend is rewritten
}

// ---- scaling (ADAPTED where the Windows expectation is the xml.etree byte form) -----------------------------------------------------------

// test_legend_position.py:53 test_only_legends_and_legend_elements_are_scaled. ADAPTED (xml.etree form `<Legend ... />`): the same inputs and values
// (5, 10, 15, 20; Extension and lower-case legend untouched), expected = the input with only the values replaced.
TEST(SetScale_Legend, OnlyLegendsAndLegendElementsAreScaled_Adapted)
{
    CHECK_STR_EQ(rewritten("<Legends dX=\"10\" dY=\"20\" Custom=\"7\">"
                           "<Legend dX=\"30\" dY=\"40\" FontSize=\"12\"/>"
                           "<Extension dX=\"90\" dY=\"91\"/>"
                           "<legend dX=\"5\" dY=\"6\"/>"
                           "</Legends>",
                           0.5),
                 "<Legends dX=\"5\" dY=\"10\" Custom=\"7\">"
                 "<Legend dX=\"15\" dY=\"20\" FontSize=\"12\"/>"
                 "<Extension dX=\"90\" dY=\"91\"/>"
                 "<legend dX=\"5\" dY=\"6\"/>"
                 "</Legends>");
}

// test_legend_position.py:71 test_legend_elements_nested_below_other_elements_are_scaled. ADAPTED (xml.etree form): Legend and Legends at any depth are scaled,
// the Group element is not (its dX stays 1).
TEST(SetScale_Legend, LegendElementsNestedBelowOtherElementsAreScaled_Adapted)
{
    CHECK_STR_EQ(rewritten("<Legends><Group dX=\"1\"><Legend dX=\"2\" dY=\"3\"/><Legends dX=\"4\"/></Group></Legends>", 10.0),
                 "<Legends><Group dX=\"1\"><Legend dX=\"20\" dY=\"30\"/><Legends dX=\"40\"/></Group></Legends>");
}

// test_legend_position.py:83 test_missing_coordinates_are_not_invented. ADAPTED (xml.etree form): a missing dX / dY is not added; an empty root stays as it is.
TEST(SetScale_Legend, MissingCoordinatesAreNotInvented_Adapted)
{
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\"><Legend dY=\"2\"/><Legend/></Legends>", 4.0), "<Legends dX=\"4\"><Legend dY=\"8\"/><Legend/></Legends>");
    CHECK_STR_EQ(rewritten("<Legends/>", 4.0), "<Legends/>");
}

// test_legend_position.py:92 test_x_and_y_are_scaled_independently_by_the_same_factor. ADAPTED (xml.etree form).
TEST(SetScale_Legend, XAndYAreScaledIndependentlyByTheSameFactor_Adapted)
{
    CHECK_STR_EQ(rewritten("<Legends dX=\"7\" dY=\"100\"/>", 3.0), "<Legends dX=\"21\" dY=\"300\"/>");
}

// test_legend_position.py:96 test_scaled_values_use_three_decimals_without_trailing_zero_noise. ADAPTED (xml.etree form): the 13 Windows rows (100, 1.50, 10 x 10,
// 1.23456 -> 1.235, 1.2344 -> 1.234, 0.0004 -> 0, 2.5 x 0.4 -> 1, 1234.567 x 0.3 -> 370.37, 2345.678 x 0.3 -> 703.703, -3 x 2, 4 x -0.5, 0 x 5, 1e2) and the extra
// rows (ties, -0, huge values, whitespace and underscores, a leading '+', '.5', '5.') whose expected text was produced by the real Windows function.
TEST(SetScale_Legend, ScaledValuesUseThreeDecimalsWithoutTrailingZeroNoise_Adapted)
{
    int rows = 0;
    for (const scale_oracle::LegendValueRow &row : scale_oracle::kLegendValueRows)
    {
        SCOPED_CONTEXT(string("dX=\"") + row.value + "\" x " + ost_test::detail::exact_float(row.factor));
        CHECK_STR_EQ(rewritten(string("<Legends dX=\"") + row.value + "\"/>", row.factor), string("<Legends dX=\"") + row.expected + "\"/>");
        ++rows;
    }
    CHECK(rows >= 30); // the table is not silently empty
}

// test_legend_position.py:119 test_attribute_order_and_unrelated_attributes_are_preserved. ADAPTED (xml.etree form).
TEST(SetScale_Legend, AttributeOrderAndUnrelatedAttributesArePreserved_Adapted)
{
    CHECK_STR_EQ(rewritten("<Legends Z=\"1\" dY=\"2\" A=\"x\" dX=\"3\"/>", 2.0), "<Legends Z=\"1\" dY=\"4\" A=\"x\" dX=\"6\"/>");
}

// test_legend_position.py:123 test_comments_and_processing_instructions_survive_the_rewrite (a literal port: ElementTree writes this non-empty form unchanged).
TEST(SetScale_Legend, CommentsAndProcessingInstructionsSurviveTheRewrite)
{
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\"><!--keep me--><?hint data?><Legend dX=\"2\"><!--inner--></Legend></Legends>", 2.0),
                 "<Legends dX=\"2\"><!--keep me--><?hint data?><Legend dX=\"4\"><!--inner--></Legend></Legends>");
}

// test_legend_position.py:135 test_text_and_tail_content_is_preserved (a literal port).
TEST(SetScale_Legend, TextAndTailContentIsPreserved)
{
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\">head<Legend dX=\"2\">body</Legend>tail</Legends>", 2.0),
                 "<Legends dX=\"2\">head<Legend dX=\"4\">body</Legend>tail</Legends>");
}

// ---- failures -----------------------------------------------------------------------------------------------------------------------------

// test_legend_position.py:145 test_non_finite_results_are_rejected_with_a_named_error: the eight rows (nan, inf, -inf, 1e308 x 10, factor inf, factor nan, a
// Legend dY x inf, inf x 0): refused with "Legend Position coordinates must be finite".
TEST(SetScale_Legend, NonFiniteResultsAreRejectedWithANamedError)
{
    struct Row
    {
        const char *xml;
        double factor;
    };
    const Row rows[] = {
        {"<Legends dX=\"nan\"/>", 2.0},
        {"<Legends dX=\"inf\"/>", 2.0},
        {"<Legends dY=\"-inf\"/>", 2.0},
        {"<Legends dX=\"1e308\"/>", 10.0},
        {"<Legends dX=\"1\"/>", kInfinity},
        {"<Legends dX=\"1\"/>", kNotANumber},
        {"<Legends><Legend dY=\"5\"/></Legends>", kInfinity},
        {"<Legends dX=\"inf\"/>", 0.0},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(string(row.xml) + " x " + ost_test::detail::exact_float(row.factor));
        string error;
        CHECK(refused(row.xml, row.factor, &error));
        CHECK_STR_EQ(error, "Legend Position coordinates must be finite");
    }
}

// test_legend_position.py:163 test_non_numeric_coordinates_raise_value_error: "abc" and an empty dY are refused (the Python message differs: iOS names the value).
TEST(SetScale_Legend, NonNumericCoordinatesAreRefused)
{
    CHECK(refused("<Legends dX=\"abc\"/>", 2.0));
    CHECK(refused("<Legends><Legend dY=\"\"/></Legends>", 2.0));
    for (const scale_oracle::LegendRefuseRow &row : scale_oracle::kLegendRefuseRows) // every oracle value Windows raises for ("0x10", "1,5", "1 2", nan, ...)
    {
        SCOPED_CONTEXT(string("dX=\"") + row.value + "\" x " + ost_test::detail::exact_float(row.factor));
        CHECK(refused(string("<Legends dX=\"") + row.value + "\"/>", row.factor));
    }
    CHECK_STR_EQ(rewritten("<Legends dX=\"2\"/>", 2.0), "<Legends dX=\"4\"/>"); // the control
}

// test_legend_position.py:169 test_zero_factor_collapses_coordinates_to_zero. ADAPTED (xml.etree form). (The store never passes a factor of 0: the caller refuses it.)
TEST(SetScale_Legend, ZeroFactorCollapsesCoordinatesToZero_Adapted)
{
    CHECK_STR_EQ(rewritten("<Legends dX=\"5\" dY=\"6\"/>", 0.0), "<Legends dX=\"0\" dY=\"0\"/>");
}

// ---- tests/integration/persistence/test_page_scale_geometry.py and test_page_operations.py (the legend halves) ----------------------------

// test_page_scale_geometry.py:21 test_legend_transform_keeps_extensions_and_null_position (the legend half). ADAPTED (the Windows test re-parses with ElementTree and
// compares attributes; here the expected text is the input with the values replaced): dX 1 -> 2, dY 2 -> 4, Custom 7 kept, Legend dX 3 -> 6 / dY 4 -> 8, FontSize 12 kept,
// Extension dX 9 kept, the comment kept. The NULL Position half is in the store tests. The positive control is the Windows one.
TEST(SetScale_Legend, LegendTransformKeepsExtensionsAndTheComment_Adapted)
{
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\" dY=\"2\" Custom=\"7\"><!--keep--><Legend dX=\"3\" dY=\"4\" FontSize=\"12\"/><Extension dX=\"9\"/></Legends>", 2.0),
                 "<Legends dX=\"2\" dY=\"4\" Custom=\"7\"><!--keep--><Legend dX=\"6\" dY=\"8\" FontSize=\"12\"/><Extension dX=\"9\"/></Legends>");
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\" dY=\"2\"><Legend dX=\"3\" dY=\"4\"/></Legends>", 2.0), "<Legends dX=\"2\" dY=\"4\"><Legend dX=\"6\" dY=\"8\"/></Legends>");
}

// test_page_operations.py:999 test_legend_xml_coordinates_rescale_without_changing_metadata: the Windows payload (newlines between the elements), factor 2: dX 61.608,
// dY 2045.358, the Legend's dX / dY 61.608; every other attribute and the whitespace are untouched.
TEST(SetScale_Legend, LegendXmlCoordinatesRescaleWithoutChangingMetadata)
{
    const string payload = "<Legends dX=\"30.804\" dY=\"1022.679\" Visible=\"255\">\n"
                           "<Legend ConditionUID=\"11334\" dX=\"30.804\" dY=\"30.804\" "
                           "Visible=\"1\" FontSize=\"12\" FontName=\"Arial\" FontColor=\"0\" FontStyle=\"0\"/>\n"
                           "</Legends>\n";
    CHECK_STR_EQ(rewritten(payload, 2.0), "<Legends dX=\"61.608\" dY=\"2045.358\" Visible=\"255\">\n"
                                          "<Legend ConditionUID=\"11334\" dX=\"61.608\" dY=\"61.608\" "
                                          "Visible=\"1\" FontSize=\"12\" FontName=\"Arial\" FontColor=\"0\" FontStyle=\"0\"/>\n"
                                          "</Legends>\n");
}

// test_page_operations.py:1040 test_invalid_legend_xml_rejects_scale_instead_of_partially_rewriting_it: "<Legends", a non-Legends root and dX="NaN" are refused.
TEST(SetScale_Legend, InvalidLegendXmlIsRefusedInsteadOfPartiallyRewritten)
{
    for (const char *source : {"<Legends", "<Other dX=\"1\"/>", "<Legends dX=\"NaN\"/>"})
    {
        SCOPED_CONTEXT(string("legend ") + source);
        CHECK(refused(source, 2.0));
    }
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\"/>", 2.0), "<Legends dX=\"2\"/>"); // the control: a valid legend is rewritten
}

// ---- iOS-only: the in-place rewrite must not touch what only looks like a coordinate (SS-10) -------------------------------------------------------

// Windows fails the whole page change on a non-latin-1 character (UnicodeEncodeError, register SS-10). iOS keeps UTF-8 byte for byte: a name with letters outside
// latin-1 (U+0142 l with stroke, U+20AC euro sign, U+4E2D a CJK character, U+1F600 an emoji) is rescaled without failing.
TEST(SetScale_LegendSafety, Utf8TextOutsideLatin1IsKeptByteForByte)
{
    const string name = "\xC5\x82\xC3\xA9 \xE2\x82\xAC \xE4\xB8\xAD \xF0\x9F\x98\x80";
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\" Name=\"" + name + "\"><Legend dX=\"2\">" + name + "</Legend></Legends>", 3.0),
                 "<Legends dX=\"3\" Name=\"" + name + "\"><Legend dX=\"6\">" + name + "</Legend></Legends>");
}

// Nothing inside a comment, a CDATA section or an attribute value is a coordinate: a regex that scales every `dX="..."` it finds would corrupt these.
TEST(SetScale_LegendSafety, NothingInsideCommentsCdataOrAttributeValuesIsScaled)
{
    const string source = "<Legends dX=\"1\"><!-- <Legend dX=\"9\" dY=\"9\"/> --><![CDATA[<Legend dX=\"8\"/>]]>"
                          "<Legend Name=\"&lt;Legend dX=&quot;7&quot;&gt; &gt; dY='5'\" dX=\"2\"/><?pi <Legend dX=\"6\"/> ?></Legends>";
    CHECK_STR_EQ(rewritten(source, 2.0), "<Legends dX=\"2\"><!-- <Legend dX=\"9\" dY=\"9\"/> --><![CDATA[<Legend dX=\"8\"/>]]>"
                                         "<Legend Name=\"&lt;Legend dX=&quot;7&quot;&gt; &gt; dY='5'\" dX=\"4\"/><?pi <Legend dX=\"6\"/> ?></Legends>");
}

// Attribute and element names are matched exactly (case-sensitive, whole name): xdX, dXx, DX, dx are not dX; LegendX, xLegend, LEGEND are not Legend.
TEST(SetScale_LegendSafety, NamesAreMatchedExactly)
{
    CHECK_STR_EQ(rewritten("<Legends xdX=\"1\" dXx=\"2\" DX=\"3\" dx=\"4\" dY=\"6\"><LegendX dX=\"1\"/><xLegend dX=\"2\"/><LEGEND dX=\"3\"/><Legend dX=\"5\"/></Legends>", 2.0),
                 "<Legends xdX=\"1\" dXx=\"2\" DX=\"3\" dx=\"4\" dY=\"12\"><LegendX dX=\"1\"/><xLegend dX=\"2\"/><LEGEND dX=\"3\"/><Legend dX=\"10\"/></Legends>");
}

// The quote character and the spaces around '=' are kept; a single-quoted value is replaced in single quotes.
TEST(SetScale_LegendSafety, QuoteStyleAndSpacesAroundEqualsAreKept)
{
    CHECK_STR_EQ(rewritten("<Legends dX='1' dY = \"2\"\n   Visible='255'/>", 2.0), "<Legends dX='2' dY = \"4\"\n   Visible='255'/>");
}

// An XML declaration and a leading comment are kept (Windows drops the declaration), and a value written with a character reference is read as the decoded text:
// dX="&#49;" is "1".
TEST(SetScale_LegendSafety, DeclarationCommentAndCharacterReferencesAreHandled)
{
    CHECK_STR_EQ(rewritten("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!-- head -->\n<Legends dX=\"&#49;\" dY=\"&#x32;\"/>\n", 2.0),
                 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!-- head -->\n<Legends dX=\"2\" dY=\"4\"/>\n");
}

// SS-2 hardening: a non-zero coordinate that underflows to 0.0 by the multiplication refuses the legend (data destroyed) unless the factor itself is 0 (the pinned
// Windows collapse above). A zero coordinate stays zero.
TEST(SetScale_LegendSafety, AnUnderflowingCoordinateIsRefused)
{
    string error;
    CHECK(refused("<Legends dX=\"5e-324\"/>", 0.5, &error));
    CHECK_STR_EQ(error, "Legend Position coordinates underflow");
    CHECK(refused("<Legends><Legend dY=\"1e-300\"/></Legends>", 1e-300));
    CHECK_STR_EQ(rewritten("<Legends dX=\"0\" dY=\"-0\"/>", 0.5), "<Legends dX=\"0\" dY=\"-0\"/>"); // Python: f"{-0.0 * 0.5:.3f}" is "-0.000" -> "-0"
    CHECK_STR_EQ(rewritten("<Legends dX=\"5e-324\"/>", 0.0), "<Legends dX=\"0\"/>");
}

// One refusal anywhere refuses the whole legend: a good Legend first and a bad one later leaves `out` untouched (no partial text is ever returned).
TEST(SetScale_LegendSafety, OneBadValueAnywhereRefusesTheWholeLegend)
{
    CHECK(refused("<Legends dX=\"1\"><Legend dX=\"2\"/><Legend dX=\"abc\"/><Legend dX=\"4\"/></Legends>", 2.0));
    CHECK(refused("<Legends dX=\"1\"><Legend dX=\"2\"/><Legend dX=\"1e308\"/></Legends>", 10.0));
    CHECK(refused("<Legends dX=\"1\"><Legend dX=\"2\"/><Legend dY=\"x\"/></Legends>", 10.0));
    CHECK_STR_EQ(rewritten("<Legends dX=\"1\"><Legend dX=\"2\"/><Legend dY=\"3\"/></Legends>", 10.0), "<Legends dX=\"10\"><Legend dX=\"20\"/><Legend dY=\"30\"/></Legends>"); // the control
}

// A realistic legend of 200 entries is rewritten completely: every entry's values are scaled, nothing else changes.
TEST(SetScale_LegendSafety, ALargeLegendIsRewrittenCompletely)
{
    string source = "<Legends dX=\"10\" dY=\"20\">";
    string expected = "<Legends dX=\"20\" dY=\"40\">";
    for (int i = 0; i < 200; ++i)
    {
        const string uid = std::to_string(11000 + i);
        source += "<Legend ConditionUID=\"" + uid + "\" dX=\"" + std::to_string(i) + ".5\" dY=\"" + std::to_string(2 * i) + "\" Visible=\"1\"/>\n";
        expected += "<Legend ConditionUID=\"" + uid + "\" dX=\"" + std::to_string(2 * i + 1) + "\" dY=\"" + std::to_string(4 * i) + "\" Visible=\"1\"/>\n";
    }
    source += "</Legends>";
    expected += "</Legends>";
    CHECK_STR_EQ(rewritten(source, 2.0), expected);
}
