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
    // printable ASCII character alone / leading / inside / trailing, Unicode digits, random mixes; the table has no
    // control character, those are the Exp12_ tests below).
    // Windows replaces ' ' / \ : ; * ? < > | . ( ) [ ] { } by '_', removes " ' , and keeps everything else (also non-ASCII,
    // # & = % + - @). A leading str.isdigit() character gets "Mat_"; an empty result is "Unknown". U+0000..U+001F and
    // U+007F become '_' since Windows b9902e6 (the Exp12_ tests).
    size_t checked = 0;
    for (const auto &c : export_oracle::kSanitizeCases)
    {
        SCOPED_CONTEXT(std::string("input ") + ost_test::detail::quote(c.input));
        CHECK_STR_EQ(sanitize_material_name(c.input), c.expected);
        ++checked;
    }
    CHECK(checked > 300);
}

// ---------------------------------------------------------------- EXP-12 control characters (Windows b9902e6)
// Windows base_exporter.py: _CONTROL_CHARACTERS = {code: "_" for code in (*range(0x20), 0x7F)} and
// _sanitize_material_name(...).translate(_CONTROL_CHARACTERS): every U+0000..U+001F and U+007F becomes ONE '_' (it is not
// removed), so a name can no longer split the 'newmtl' / 'usemtl' / 'g' line of the OBJ / MTL or the DXF layer name. The
// digit rule and the empty rule look at the text after that replacement. Nothing else changed (U+0085, U+00A0, U+2028 ...
// stay as they are).
TEST(ExportText, Exp12_ControlCharactersBecomeUnderscoresLikeWindows)
{
    // Port of test_base_exporter.py MaterialNameControlCharacterTests::test_control_characters_become_underscores. The
    // controls are built with a loop: in a C++ literal "\x7fB" would be one (invalid) hex escape.
    std::string controls;
    for (int code = 0; code < 0x20; ++code)
        controls += static_cast<char>(code); // the NUL is a real character of the std::string
    controls += static_cast<char>(0x7F);
    REQUIRE_EQ(controls.size(), static_cast<size_t>(33));
    CHECK_STR_EQ(sanitize_material_name("A" + controls + "B"), "A" + std::string(33, '_') + "B");
    // and each one alone gives exactly one '_' (in the middle of a name, so the digit / empty rules do not interfere)
    for (int code = 0; code <= 0x7F; ++code)
    {
        if (code >= 0x20 && code < 0x7F)
            continue; // printable ASCII: not a control character
        SCOPED_CONTEXT("control character " + std::to_string(code));
        std::string name = "a";
        name += static_cast<char>(code);
        name += "b";
        CHECK_STR_EQ(sanitize_material_name(name), "a_b");
    }
}

TEST(ExportText, Exp12_InjectedLinesCannotSurviveInAMaterialName)
{
    // Port of ...::test_injected_lines_cannot_survive_in_a_material_name: the name of Windows
    // 'Slab\r\nnewmtl evil\rKd 1 0 0\nv 1 2 3\x00' (CR LF and CR and LF and a closing NUL). The NUL needs an explicit-length
    // std::string: a const char * would end at it.
    std::string injected = "Slab\r\nnewmtl evil\rKd 1 0 0\nv 1 2 3";
    injected.push_back('\0');
    REQUIRE_EQ(injected.size(), static_cast<size_t>(35));
    const std::string sanitized = sanitize_material_name(injected);
    CHECK_STR_EQ(sanitized, "Slab__newmtl_evil_Kd_1_0_0_v_1_2_3_");
    for (unsigned char c : sanitized)
        CHECK(c >= 0x20 && c != 0x7F);
}

TEST(ExportText, Exp12_PrintableAndNonAsciiTextIsUnchangedByTheControlCharacterRule)
{
    // Port of ...::test_printable_text_is_unchanged_by_the_control_character_rule: only U+0000..U+001F and U+007F are
    // replaced. 'Área_Ω' is C3 81 'rea_' CE A9; U+0085 (C2 85) and U+00A0 (C2 A0) are not controls of this rule (the bytes
    // are >= 0x80 and stay), and neither are U+2028 / U+2029 (E2 80 A8 / E2 80 A9).
    CHECK_STR_EQ(sanitize_material_name("\xC3\x81rea_\xCE\xA9"), "\xC3\x81rea_\xCE\xA9");
    CHECK_STR_EQ(sanitize_material_name("\xC2\x85\xC2\xA0"), "\xC2\x85\xC2\xA0");
    CHECK_STR_EQ(sanitize_material_name("a\xE2\x80\xA8" "b\xE2\x80\xA9"), "a\xE2\x80\xA8" "b\xE2\x80\xA9");
    // the printable ASCII characters around the control range keep the old rule: ' ' (0x20) is '_', '!' (0x21) and '~' (0x7E)
    // stay
    CHECK_STR_EQ(sanitize_material_name("a b~!"), "a_b~!");
}

TEST(ExportText, Exp12_TheDigitRuleAndTheEmptyRuleSeeTheReplacedText)
{
    // Derived from reading _sanitize_material_name (not a Windows test, not run): the control characters are replaced
    // before the digit test and the empty test, like the ' ' of the existing rule (" 1" gives "_1", no prefix).
    CHECK_STR_EQ(sanitize_material_name("\n1x"), "_1x");               // the leading control character is a '_', not a digit
    CHECK_STR_EQ(sanitize_material_name("\x01" "1abc"), "_1abc");      // no 'Mat_' prefix
    CHECK_STR_EQ(sanitize_material_name("1\x01"), "Mat_1_");           // a digit first still gets the prefix
    CHECK_STR_EQ(sanitize_material_name("\n"), "_");                   // only controls: not 'Unknown'
    CHECK_STR_EQ(sanitize_material_name("\x01\x02"), "__");
    CHECK_STR_EQ(sanitize_material_name(std::string(1, '\0')), "_");   // a lone NUL (explicit length)
    CHECK_STR_EQ(sanitize_material_name("\"\x01"), "_");               // a removed quote leaves the control character
    CHECK_STR_EQ(sanitize_material_name("\x7F"), "_");
    CHECK_STR_EQ(sanitize_material_name("\"',"), "Unknown");           // quotes and commas alone are still removed
    CHECK_STR_EQ(sanitize_material_name(""), "Unknown");
    // the old behaviour of the cases of the former test
    CHECK_STR_EQ(sanitize_material_name("a\nb"), "a_b");
    CHECK_STR_EQ(sanitize_material_name("a\tb\r\x01\x7f"), "a_b___");
    // sanitising twice changes nothing more (the OBJ writer sanitises a name that ExportMeshSet already sanitised)
    for (const char *raw : {"a\nb", "\n1x", "1\x01", "\x01\x02"})
    {
        const std::string once = sanitize_material_name(raw);
        CHECK_STR_EQ(sanitize_material_name(once), once);
    }
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
