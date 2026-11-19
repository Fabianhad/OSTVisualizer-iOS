// Text helpers of the exporters (Core/Export/export_text.*) against the Windows / Python routines they reproduce.
// The expected strings come from running the Windows code read-only (ExportTextOracleData.hpp).
#include "ExportTestSupport.hpp"
#include "ExportTextOracleData.hpp"
#include "Export/export_text.hpp"

#include <cstring>
#include <limits>

using namespace ost_geometry::export_text;

// ---------------------------------------------------------------- EXP-12 name sanitiser
TEST(ExportText, SanitizeMaterialNameMatchesTheWindowsRules)
{
    // Port of exporters/base_exporter.py BaseExporter._sanitize_material_name (the 8 cases of the EXP-12 plan, every
    // printable ASCII character alone / leading / inside / trailing, control characters, Unicode digits, random mixes).
    // Windows replaces ' ' / \ : ; * ? < > | . ( ) [ ] { } by '_', removes " ' , and keeps everything else (also non-ASCII,
    // # & = % + - @). A leading str.isdigit() character gets "Mat_"; an empty result is "Unknown".
    size_t checked = 0;
    for (const auto &c : export_oracle::kSanitizeCases)
    {
        SCOPED_CONTEXT(std::string("input ") + ost_test::detail::quote(c.input));
        CHECK_STR_EQ(sanitize_material_name(c.input), c.expected);
        ++checked;
    }
    CHECK(checked > 300);
}

TEST(ExportText, SanitizeMaterialNameDropsControlCharactersUnlikeWindows)
{
    // DELIBERATE DIFFERENCE from Windows, kept by user decision 2026-11-19 (EXP-12, see PORTING_BLOCKERS.md): Windows keeps
    // control characters in the name, so a line break or tab splits the 'newmtl' / 'g' line of the OBJ / MTL; iOS has always
    // dropped them from these names and keeps doing so. Everything else is the Windows rule (the oracle test above).
    CHECK_STR_EQ(sanitize_material_name("a\nb"), "ab");
    CHECK_STR_EQ(sanitize_material_name("a\tb\r\x01\x7f"), "ab");
    CHECK_STR_EQ(sanitize_material_name("\n1x"), "Mat_1x"); // the digit test sees the first character that is kept
    CHECK_STR_EQ(sanitize_material_name("\n"), "Unknown");
}

TEST(ExportText, SanitizeMaterialNameExamplesOfThePlan)
{
    CHECK_STR_EQ(sanitize_material_name("Concrete_Wall 1/2_12345678"), "Concrete_Wall_1_2_12345678");
    CHECK_STR_EQ(sanitize_material_name("5\" Slab, (Top).v2_abcdefgh"), "Mat_5_Slab__Top__v2_abcdefgh");
    CHECK_STR_EQ(sanitize_material_name("Walls_A:B;C*D?E<F>G|H\\I_aaaaaaaa"), "Walls_A_B_C_D_E_F_G_H_I_aaaaaaaa");
    CHECK_STR_EQ(sanitize_material_name(""), "Unknown");
    CHECK_STR_EQ(sanitize_material_name("O'Brien's"), "OBriens");
    CHECK_STR_EQ(sanitize_material_name("{x}[y]"), "_x__y_");
    CHECK_STR_EQ(sanitize_material_name("1st Floor"), "Mat_1st_Floor");
    // only quotes and commas: removed, nothing left
    CHECK_STR_EQ(sanitize_material_name("\"',"), "Unknown");
    // the digit test looks at the sanitised text: a leading space becomes '_' (no prefix), a leading quote is removed
    CHECK_STR_EQ(sanitize_material_name(" 1"), "_1");
    CHECK_STR_EQ(sanitize_material_name("\"1"), "Mat_1");
}

// ---------------------------------------------------------------- EXP-20 number format
TEST(ExportText, PythonFloatReprMatchesWindowsJsonDumps)
{
    // Port of float.__repr__ (what json.dumps writes for every number of the Windows scene): shortest round-trip digits,
    // exponent form outside decimal exponents [-4, 16), "1.0" / "-0.0" for whole numbers.
    size_t checked = 0;
    for (const auto &c : export_oracle::kReprCases)
    {
        double v = 0.0;
        std::memcpy(&v, &c.bits, sizeof v);
        SCOPED_CONTEXT(std::string("expected ") + c.expected);
        const std::string got = python_float_repr(v);
        CHECK_STR_EQ(got, c.expected);
        CHECK_EQ(std::strtod(got.c_str(), nullptr), v); // and it reads back as the same double
        ++checked;
    }
    CHECK(checked > 250);
}

TEST(ExportText, PythonFloatReprOfNonFiniteValuesIsWhatJsonDumpsWrites)
{
    CHECK_STR_EQ(python_float_repr(std::numeric_limits<double>::quiet_NaN()), "NaN");
    CHECK_STR_EQ(python_float_repr(std::numeric_limits<double>::infinity()), "Infinity");
    CHECK_STR_EQ(python_float_repr(-std::numeric_limits<double>::infinity()), "-Infinity");
}

TEST(ExportText, ColourChannelOfAByteIsTheByteOverTwoFiftyFiveInDouble)
{
    // Windows ColorService.hex_to_rgb: int(byte, 16) / 255.0 in double. The iOS colour is float(byte) / 255.0f
    // (Condition::rgb_from_color_fill); widened as it is, byte 128 would read 0.501960813999176 instead of
    // 0.5019607843137255.
    for (int k = 0; k < 256; ++k)
    {
        SCOPED_CONTEXT("byte " + std::to_string(k));
        const float f = static_cast<float>(k) / 255.0f;
        CHECK_EQ(colour_channel(f), static_cast<double>(k) / 255.0);
    }
    CHECK_STR_EQ(python_float_repr(colour_channel(128.0f / 255.0f)), "0.5019607843137255");
    // a component that is not a byte fraction is not snapped
    CHECK_EQ(colour_channel(0.3f), static_cast<double>(0.3f));
    CHECK_EQ(colour_channel(0.0f), 0.0);
    CHECK_EQ(colour_channel(1.0f), 1.0);
}

TEST(ExportText, ShortestDoubleOfAFloatIsTheDecimalItWasTypedAs)
{
    // An opacity such as 0.35f must come out as the Windows number 0.35, not 0.3499999940395355.
    CHECK_EQ(shortest_double_of(0.35f), 0.35);
    CHECK_EQ(shortest_double_of(0.5f), 0.5);
    CHECK_EQ(shortest_double_of(1.0f), 1.0);
    CHECK_EQ(shortest_double_of(0.1f), 0.1);
    CHECK_EQ(shortest_double_of(0.99f), 0.99);
    CHECK_EQ(shortest_double_of(123456.7f), 123456.7);
    CHECK_EQ(shortest_double_of(1e-5f), 1e-5);
    CHECK_EQ(shortest_double_of(0.0f), 0.0);
    CHECK_STR_EQ(python_float_repr(shortest_double_of(0.35f)), "0.35");
}

// ---------------------------------------------------------------- EXP-08 escapes
TEST(ExportText, JsonStringMatchesJsonDumpsWithTheLessThanSignEscaped)
{
    // Port of threejs_renderer._generate_html: json.dumps(scene, separators=(",", ":")) with every '<' replaced by its
    // JSON escape (a backslash followed by u003c).
    for (const auto &c : export_oracle::kEscapeCases)
    {
        SCOPED_CONTEXT(std::string("input ") + ost_test::detail::quote(c.input));
        CHECK_STR_EQ(json_string(c.input), c.json);
    }
}

TEST(ExportText, JsonStringReplacesInvalidUtf8ByTheReplacementCharacter)
{
    const std::string expected = std::string("\"a") + "\\" + "ufffd" + "b\"";
    CHECK_STR_EQ(json_string(std::string("a\xff") + "b"), expected);
    CHECK_STR_EQ(json_string(std::string("a\xc3") + "b"), expected); // truncated 2-byte sequence
}

TEST(ExportText, HtmlEscapeMatchesHtmlEscapeWithoutQuotes)
{
    // Port of html.escape(title, quote=False): & < > only, ampersand first.
    for (const auto &c : export_oracle::kEscapeCases)
    {
        SCOPED_CONTEXT(std::string("input ") + ost_test::detail::quote(c.input));
        CHECK_STR_EQ(html_escape(c.input), c.html);
    }
    CHECK_STR_EQ(html_escape("&lt;"), "&amp;lt;");
}
