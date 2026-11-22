// TXL-01 .. TXL-07: the shared text-limit helpers of the M1 writers (plan 4.2 "Text limits", N-M1-13, S0.b): ost_domain::utf16_length,
// contains_nul, has_control_character, has_line_break (Core/Domain/TextLimits.hpp).
//
// Windows has no such check (register NC-4, CF-6, LY-8, LY-14, RP-2: no length limit, any character accepted), so there is no Windows
// test to port. The sources of the expected values are:
//  * the column sizes of the Windows schema, which are the limits the helper serves: ost_visualizer/infrastructure/mdb/database_creator.py
//    :142 (folder Name VARCHAR(50)), :166 (condition Name VARCHAR(255)), :310 (layer Name VARCHAR(50)), :373 (page Name VARCHAR(255));
//  * CPython 3.11.4 (Unicode 14.0.0), stdlib only, executed in memory with `python -B -I`:
//      sum(1 for c in range(0x110000) if unicodedata.category(chr(c)) == 'Cc')              -> 65, the ranges U+0000-U+001F, U+007F-U+009F
//      [c for c in range(0x110000) if len(("a" + chr(c) + "b").splitlines()) > 1]            -> U+000A U+000B U+000C U+000D U+001C U+001D
//                                                                                              U+001E U+0085 U+2028 U+2029 (10 code points)
//      bytes(...).decode("utf-8") for every byte string of the invalid-UTF-8 table below      -> UnicodeDecodeError (and valid for every
//                                                                                              byte string of the valid table)
//      len(s.encode("utf-16-le")) // 2                                                        -> the UTF-16 units of the valid table.
//  Python's len(str) counts a code point outside the BMP once; the helper counts the two UTF-16 units Access counts (see the header).
//
// Non-ASCII test text is written as UTF-8 byte escapes inside adjacent string literals, never a raw character.
#include "ParsersTestSupport.hpp"

#include "Domain/TextLimits.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace
{
    using ost_domain::contains_nul;
    using ost_domain::has_control_character;
    using ost_domain::has_line_break;
    using ost_domain::utf16_length;

    // One emoji outside the BMP: U+1F600 GRINNING FACE, F0 9F 98 80.
    const std::string kEmoji = "\xF0\x9F\x98\x80";

    // "U+00E9" style label of a code point for the failure text.
    std::string u_plus(std::uint32_t cp)
    {
        static const char digits[] = "0123456789ABCDEF";
        std::string out;
        for (int shift = 20; shift >= 0; shift -= 4)
        {
            const unsigned nibble = (cp >> shift) & 0xF;
            if (!out.empty() || nibble != 0 || shift <= 12)
                out += digits[nibble];
        }
        return "U+" + out;
    }

    // utf16_length as a plain number: kInvalid for std::nullopt, so a wrong answer is a failed check and never a dereference of an empty
    // optional.
    const std::size_t kInvalid = static_cast<std::size_t>(-1);
    std::size_t units_of(const std::string &text)
    {
        const std::optional<std::size_t> units = utf16_length(text);
        return units ? *units : kInvalid;
    }

    std::string repeat(const std::string &unit, std::size_t times)
    {
        std::string out;
        for (std::size_t i = 0; i < times; ++i)
            out += unit;
        return out;
    }

    // The UTF-8 form of one scalar value (the test's own encoder; the caller never passes a surrogate unless it wants the invalid
    // 3-byte form of one).
    std::string encode_utf8(std::uint32_t cp)
    {
        std::string out;
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
        return out;
    }

    // An independent strict decoder, written differently from the Core one (it computes the code point from the payload bits and then
    // checks minimal length, the surrogate range and the maximum, where the Core one tests byte ranges per lead byte): the units of
    // the text, or nullopt when it is not valid UTF-8.
    std::optional<std::size_t> reference_units(const std::string &s)
    {
        std::size_t units = 0;
        std::size_t i = 0;
        while (i < s.size())
        {
            const unsigned lead = static_cast<unsigned char>(s[i]);
            std::size_t length = 0;
            std::uint32_t cp = 0;
            std::uint32_t minimum = 0;
            if (lead < 0x80)
            {
                length = 1;
                cp = lead;
            }
            else if ((lead & 0xE0) == 0xC0)
            {
                length = 2;
                cp = lead & 0x1F;
                minimum = 0x80;
            }
            else if ((lead & 0xF0) == 0xE0)
            {
                length = 3;
                cp = lead & 0x0F;
                minimum = 0x800;
            }
            else if ((lead & 0xF8) == 0xF0)
            {
                length = 4;
                cp = lead & 0x07;
                minimum = 0x10000;
            }
            else
            {
                return std::nullopt; // a continuation byte or F8..FF as a lead
            }
            if (i + length > s.size())
                return std::nullopt; // truncated
            for (std::size_t k = 1; k < length; ++k)
            {
                const unsigned next = static_cast<unsigned char>(s[i + k]);
                if ((next & 0xC0) != 0x80)
                    return std::nullopt;
                cp = (cp << 6) | (next & 0x3F);
            }
            if (cp < minimum)
                return std::nullopt; // overlong
            if (cp >= 0xD800 && cp <= 0xDFFF)
                return std::nullopt; // a surrogate
            if (cp > 0x10FFFF)
                return std::nullopt;
            units += cp >= 0x10000 ? 2 : 1;
            i += length;
        }
        return units;
    }

    struct Bytes
    {
        const char *label;
        std::string text;
    };

    // Every byte string here is invalid UTF-8 (Python: UnicodeDecodeError) and holds no 0x00 byte.
    const std::vector<Bytes> &invalid_texts()
    {
        static const std::vector<Bytes> k = {
            {"lone high surrogate U+D800", "\xED\xA0\x80"},
            {"lone high surrogate U+DBFF", "\xED\xAF\xBF"},
            {"lone low surrogate U+DC00", "\xED\xB0\x80"},
            {"lone low surrogate U+DFFF", "\xED\xBF\xBF"},
            {"a surrogate pair as two 3-byte sequences (CESU-8 of U+1F600)", "\xED\xA0\xBD\xED\xB8\x80"},
            {"overlong 2-byte slash", "\xC0\xAF"},
            {"overlong 2-byte NUL (modified UTF-8)", "\xC0\x80"},
            {"overlong 2-byte U+007F", "\xC1\xBF"},
            {"overlong 3-byte slash", "\xE0\x80\xAF"},
            {"overlong 3-byte U+07FF", "\xE0\x9F\xBF"},
            {"overlong 4-byte slash", "\xF0\x80\x80\xAF"},
            {"overlong 4-byte U+FFFF", "\xF0\x8F\xBF\xBF"},
            {"truncated 2-byte", "\xC3"},
            {"truncated 3-byte, one byte", "\xE2"},
            {"truncated 3-byte, two bytes", "\xE2\x82"},
            {"truncated 4-byte, one byte", "\xF0"},
            {"truncated 4-byte, two bytes", "\xF0\x9F"},
            {"truncated 4-byte, three bytes", "\xF0\x9F\x98"},
            {"stray continuation 80", "\x80"},
            {"stray continuation BF", "\xBF"},
            {"above U+10FFFF (F4 90 80 80)", "\xF4\x90\x80\x80"},
            {"lead F5", "\xF5\x80\x80\x80"},
            {"lead F8", "\xF8\x88\x80\x80\x80"},
            {"byte FE", "\xFE"},
            {"byte FF", "\xFF"},
            {"lead followed by a non-continuation", "\xC3\x28"},
            {"3-byte lead, bad third byte", "\xE2\x82\x28"},
            {"4-byte lead, bad fourth byte", "\xF0\x9F\x98\x28"},
            {"valid text then a truncated sequence", "abc\xC3"},
            {"valid text, an invalid byte, valid text", "a\xFF" "b"},
            {"an emoji then a lone surrogate", "\xF0\x9F\x98\x80\xED\xA0\x80"},
        };
        return k;
    }
}

// ---------------------------------------------------------------------------------------------------------
// TXL-01 utf16_length: valid text
// ---------------------------------------------------------------------------------------------------------
TEST(TextLimits, Utf16Length_EmptyAndAscii)
{
    CHECK(utf16_length("").has_value());
    CHECK_EQ(units_of(""), std::size_t(0));
    CHECK(utf16_length("a").has_value());
    CHECK_EQ(units_of("a"), std::size_t(1));
    CHECK_EQ(units_of("Layer 1"), std::size_t(7));
    CHECK_EQ(units_of(repeat("a", 255)), std::size_t(255));
    CHECK_EQ(units_of(repeat("a", 256)), std::size_t(256));
    CHECK_EQ(units_of(" "), std::size_t(1)); // a space counts like any other character: the limit is on the stored text
}

TEST(TextLimits, Utf16Length_BmpCharactersAreOneUnitWhateverTheirUtf8Length)
{
    struct Case
    {
        const char *label;
        std::string text;
    };
    const std::vector<Case> cases = {
        {"U+00E9 e acute (2 bytes)", "\xC3\xA9"},
        {"U+0080 first 2-byte", "\xC2\x80"},
        {"U+07FF last 2-byte", "\xDF\xBF"},
        {"U+0800 first 3-byte", "\xE0\xA0\x80"},
        {"U+20AC euro (3 bytes)", OST_U_EURO},
        {"U+4E2D a CJK ideograph", "\xE4\xB8\xAD"},
        {"U+D7FF last before the surrogates", "\xED\x9F\xBF"},
        {"U+E000 first after the surrogates", "\xEE\x80\x80"},
        {"U+FEFF byte order mark", "\xEF\xBB\xBF"},
        {"U+FFFE noncharacter", "\xEF\xBF\xBE"},
        {"U+FFFF noncharacter, last BMP", "\xEF\xBF\xBF"},
        {"U+007F DEL", "\x7F"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.label);
        const std::optional<std::size_t> units = utf16_length(c.text);
        REQUIRE(units.has_value());
        CHECK_EQ(*units, std::size_t(1));
    }
}

TEST(TextLimits, Utf16Length_CharactersOutsideTheBmpAreTwoUnits)
{
    // U+10000 first outside the BMP, U+1F600 an emoji, U+10FFFF the last scalar value.
    CHECK_EQ(units_of("\xF0\x90\x80\x80"), std::size_t(2));
    CHECK_EQ(units_of(kEmoji), std::size_t(2));
    CHECK_EQ(units_of("\xF4\x8F\xBF\xBF"), std::size_t(2));
    // The count is per character, in a mixed text: "a" + emoji + "b" is 1 + 2 + 1.
    CHECK_EQ(units_of("a" + kEmoji + "b"), std::size_t(4));
    // A 2-unit character in the middle of a name must not be counted twice or once: 3 emoji = 6 units, 3 code points.
    CHECK_EQ(units_of(repeat(kEmoji, 3)), std::size_t(6));
}

TEST(TextLimits, Utf16Length_TheBoundariesOfTheNameLimits)
{
    // The 255-unit limit of a condition name and a page name: 127 emoji are 254 units (allowed), 128 are 256 (refused). Python len()
    // would give 127 and 128 code points, both under the limit: the helper counts what Access counts.
    CHECK_EQ(units_of(repeat(kEmoji, 127)), std::size_t(254));
    CHECK_EQ(units_of(repeat(kEmoji, 128)), std::size_t(256));
    CHECK_EQ(units_of(repeat("a", 253) + kEmoji), std::size_t(255)); // exactly at the limit
    CHECK_EQ(units_of(repeat("a", 254) + kEmoji), std::size_t(256)); // one over, by the second unit of the last character
    // The 50-unit limit of a folder name and a layer name.
    CHECK_EQ(units_of(repeat(kEmoji, 25)), std::size_t(50));
    CHECK_EQ(units_of(repeat(kEmoji, 26)), std::size_t(52));
    CHECK_EQ(units_of(repeat("a", 50)), std::size_t(50));
    CHECK_EQ(units_of(repeat("a", 51)), std::size_t(51));
    // Multi-byte BMP text: 255 euro signs are 255 units although they are 765 bytes.
    CHECK_EQ(units_of(repeat(OST_U_EURO, 255)), std::size_t(255));
}

TEST(TextLimits, Utf16Length_ControlCharactersAndNulCountOneUnitEach)
{
    CHECK_EQ(units_of(std::string("a\0b", 3)), std::size_t(3)); // an embedded NUL is counted, not a terminator
    CHECK_EQ(units_of(std::string(1, '\0')), std::size_t(1));
    CHECK_EQ(units_of("\t\n\r"), std::size_t(3));
    CHECK_EQ(units_of("\xE2\x80\xA8"), std::size_t(1)); // U+2028 LINE SEPARATOR
}

// ---------------------------------------------------------------------------------------------------------
// TXL-02 utf16_length: invalid UTF-8 is reported as invalid
// ---------------------------------------------------------------------------------------------------------
TEST(TextLimits, Utf16Length_InvalidUtf8IsReportedAsInvalid)
{
    for (const Bytes &b : invalid_texts())
    {
        SCOPED_CONTEXT(b.label);
        CHECK_FALSE(utf16_length(b.text).has_value());
    }
}

TEST(TextLimits, Utf16Length_ALoneSurrogateIsInvalidButTheNeighboursAreValid)
{
    // U+D7FF (ED 9F BF) and U+E000 (EE 80 80) are valid, U+D800 (ED A0 80) .. U+DFFF (ED BF BF) are not: the range test of the lead ED.
    CHECK(utf16_length("\xED\x9F\xBF").has_value());
    CHECK(utf16_length("\xEE\x80\x80").has_value());
    for (std::uint32_t cp = 0xD800; cp <= 0xDFFF; cp += 0x80)
    {
        SCOPED_CONTEXT("surrogate " + u_plus(cp));
        CHECK_FALSE(utf16_length(encode_utf8(cp)).has_value());
    }
    CHECK_FALSE(utf16_length(encode_utf8(0xDFFF)).has_value());
}

TEST(TextLimits, Utf16Length_TheLastValidAndTheFirstInvalidScalar)
{
    CHECK(utf16_length("\xF4\x8F\xBF\xBF").has_value());  // U+10FFFF
    CHECK_FALSE(utf16_length("\xF4\x90\x80\x80").has_value()); // U+110000
    CHECK(utf16_length("\xF0\x90\x80\x80").has_value());  // U+10000, the shortest valid 4-byte form
    CHECK_FALSE(utf16_length("\xF0\x8F\xBF\xBF").has_value()); // U+FFFF as 4 bytes: overlong
}

// ---------------------------------------------------------------------------------------------------------
// TXL-03 utf16_length against an independent decoder, over whole byte spaces
// ---------------------------------------------------------------------------------------------------------
TEST(TextLimits, Utf16Length_EveryScalarValueHasItsUnitsAndEverySurrogateIsInvalid)
{
    std::size_t valid = 0;
    for (std::uint32_t cp = 0; cp <= 0x10FFFF; ++cp)
    {
        const std::string text = encode_utf8(cp);
        const std::optional<std::size_t> units = utf16_length(text);
        if (cp >= 0xD800 && cp <= 0xDFFF)
        {
            if (units.has_value())
                FAIL_TEST("surrogate " + u_plus(cp) + " was accepted");
            continue;
        }
        if (!units.has_value() || *units != (cp >= 0x10000 ? 2u : 1u))
            FAIL_TEST("wrong units for " + u_plus(cp));
        ++valid;
    }
    CHECK_EQ(valid, std::size_t(0x110000 - 0x800)); // every scalar value (the 2048 surrogates are not scalar values)
}

TEST(TextLimits, Utf16Length_AgreesWithAnIndependentStrictDecoderOnShortByteStrings)
{
    // Every 1-byte and every 2-byte string.
    for (unsigned a = 0; a < 256; ++a)
    {
        const std::string one(1, static_cast<char>(a));
        if (utf16_length(one) != reference_units(one))
            FAIL_TEST("1-byte string " + std::to_string(a));
        for (unsigned b = 0; b < 256; ++b)
        {
            std::string two = one;
            two += static_cast<char>(b);
            if (utf16_length(two) != reference_units(two))
                FAIL_TEST("2-byte string " + std::to_string(a) + " " + std::to_string(b));
        }
    }
}

TEST(TextLimits, Utf16Length_AgreesWithAnIndependentStrictDecoderOnEveryThreeByteString)
{
    // Every lead E0..EF with every second and third byte, and every lead C2..DF / E0..EF with every second byte (a truncated 3-byte form).
    for (unsigned a = 0xE0; a <= 0xEF; ++a)
    {
        for (unsigned b = 0; b < 256; ++b)
        {
            std::string two;
            two += static_cast<char>(a);
            two += static_cast<char>(b);
            if (utf16_length(two) != reference_units(two))
                FAIL_TEST("truncated 3-byte " + std::to_string(a) + " " + std::to_string(b));
            for (unsigned c = 0; c < 256; ++c)
            {
                std::string three = two;
                three += static_cast<char>(c);
                if (utf16_length(three) != reference_units(three))
                    FAIL_TEST("3-byte string " + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c));
            }
        }
    }
}

TEST(TextLimits, Utf16Length_AgreesWithAnIndependentStrictDecoderOnFourByteStrings)
{
    // Every lead F0..F7 and F8, every second byte, third and fourth byte from the boundary set (the edges of the continuation range,
    // of the overlong limits and of the U+10FFFF limit), and every prefix of them.
    const std::vector<unsigned> edges = {0x00, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xFF};
    for (unsigned a = 0xF0; a <= 0xF8; ++a)
    {
        for (unsigned b = 0; b < 256; ++b)
        {
            std::string two;
            two += static_cast<char>(a);
            two += static_cast<char>(b);
            if (utf16_length(two) != reference_units(two))
                FAIL_TEST("truncated 4-byte " + std::to_string(a) + " " + std::to_string(b));
            for (unsigned c : edges)
            {
                std::string three = two;
                three += static_cast<char>(c);
                if (utf16_length(three) != reference_units(three))
                    FAIL_TEST("truncated 4-byte " + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c));
                for (unsigned d : edges)
                {
                    std::string four = three;
                    four += static_cast<char>(d);
                    if (utf16_length(four) != reference_units(four))
                        FAIL_TEST("4-byte string " + std::to_string(a) + " " + std::to_string(b) + " " + std::to_string(c) + " " +
                                  std::to_string(d));
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------
// TXL-04 contains_nul
// ---------------------------------------------------------------------------------------------------------
TEST(TextLimits, ContainsNul_FindsTheNulByteAnywhere)
{
    CHECK(contains_nul(std::string(1, '\0')));
    CHECK(contains_nul(std::string("a\0b", 3)));
    CHECK(contains_nul(std::string("abc\0", 4)));
    CHECK(contains_nul(std::string("\0abc", 4)));
    CHECK(contains_nul(std::string("\xF0\x9F\x98\x80\0", 5)));
}

TEST(TextLimits, ContainsNul_OtherCharactersAreNotNul)
{
    CHECK_FALSE(contains_nul(""));
    CHECK_FALSE(contains_nul("abc"));
    CHECK_FALSE(contains_nul("\x01\x1F\x7F")); // other control characters are not NUL
    CHECK_FALSE(contains_nul("0"));            // the digit
    CHECK_FALSE(contains_nul("\\0"));          // a backslash and a digit
    CHECK_FALSE(contains_nul(kEmoji));
    CHECK_FALSE(contains_nul("\xC0\x80")); // the overlong form of U+0000 has no 0x00 byte (it is invalid UTF-8: utf16_length says so)
    CHECK_FALSE(utf16_length("\xC0\x80").has_value());
}

TEST(TextLimits, ContainsNul_IsExactEvenWhenTheRestIsNotUtf8)
{
    // A NUL cuts a C string whatever else is wrong with the text, so the byte search does not depend on the validity.
    CHECK(contains_nul(std::string("\xFF\0", 2)));
    CHECK(contains_nul(std::string("\xED\xA0\x80\0", 4)));
    CHECK_FALSE(contains_nul("\xFF"));
}

// ---------------------------------------------------------------------------------------------------------
// TXL-05 has_control_character
// ---------------------------------------------------------------------------------------------------------
TEST(TextLimits, HasControlCharacter_TheC0RangeAndDel)
{
    for (std::uint32_t cp = 0; cp <= 0x1F; ++cp)
    {
        SCOPED_CONTEXT(u_plus(cp));
        CHECK(has_control_character(encode_utf8(cp)));
    }
    CHECK(has_control_character("\x7F"));
    CHECK(has_control_character("\t")); // a tab is a control character
    CHECK(has_control_character("\n"));
    CHECK(has_control_character("\r"));
    CHECK(has_control_character(std::string(1, '\0'))); // NUL is one of them
    for (std::uint32_t cp = 0x20; cp <= 0x7E; ++cp)
    {
        SCOPED_CONTEXT("printable ASCII " + u_plus(cp));
        CHECK_FALSE(has_control_character(encode_utf8(cp)));
    }
}

TEST(TextLimits, HasControlCharacter_TheC1RangeCounts)
{
    // Decision of the helper: the C1 controls U+0080-U+009F are control characters (Unicode category Cc, like C0 and DEL); a layer name
    // never needs one.
    for (std::uint32_t cp = 0x80; cp <= 0x9F; ++cp)
    {
        SCOPED_CONTEXT("C1 " + u_plus(cp));
        CHECK(has_control_character(encode_utf8(cp)));
    }
    CHECK(has_control_character(OST_U_NEL)); // U+0085, a control character AND a line break
    // The neighbours are not: U+00A0 no-break space, U+00A1.
    CHECK_FALSE(has_control_character(OST_U_NBSP));
    CHECK_FALSE(has_control_character("\xC2\xA1"));
}

TEST(TextLimits, HasControlCharacter_TheSeparatorsAndFormatCharactersAreNot)
{
    CHECK_FALSE(has_control_character("\xE2\x80\xA8")); // U+2028 LINE SEPARATOR (category Zl): has_line_break names it
    CHECK_FALSE(has_control_character("\xE2\x80\xA9")); // U+2029 PARAGRAPH SEPARATOR (Zp)
    CHECK_FALSE(has_control_character(OST_U_ZWSP));     // U+200B zero width space (Cf)
    CHECK_FALSE(has_control_character("\xEF\xBB\xBF")); // U+FEFF byte order mark (Cf)
    CHECK_FALSE(has_control_character(OST_U_IDEO));     // U+3000 ideographic space (Zs)
    CHECK_FALSE(has_control_character(kEmoji));
    CHECK_FALSE(has_control_character(OST_U_EURO));
    CHECK_FALSE(has_control_character("Layer 1"));
    CHECK_FALSE(has_control_character(" "));
    CHECK_FALSE(has_control_character(""));
}

TEST(TextLimits, HasControlCharacter_FindsOneAnywhereInTheText)
{
    CHECK(has_control_character("ab\x01" "cd"));
    CHECK(has_control_character("\x01" "abc"));
    CHECK(has_control_character("abc\x01"));
    CHECK(has_control_character(kEmoji + "\n"));
    CHECK(has_control_character("a" OST_U_NEL "b"));
    CHECK(has_control_character(std::string("a\0b", 3)));
}

TEST(TextLimits, HasControlCharacter_ExactlyTheUnicodeCcCodePoints)
{
    // Oracle (Python 3.11.4, Unicode 14.0.0): unicodedata.category(chr(c)) == 'Cc' for exactly 65 code points, U+0000-U+001F and
    // U+007F-U+009F. Every scalar value is tried.
    std::size_t controls = 0;
    for (std::uint32_t cp = 0; cp <= 0x10FFFF; ++cp)
    {
        if (cp >= 0xD800 && cp <= 0xDFFF)
            continue;
        const bool expected = cp <= 0x1F || (cp >= 0x7F && cp <= 0x9F);
        if (has_control_character(encode_utf8(cp)) != expected)
            FAIL_TEST(u_plus(cp));
        if (expected)
            ++controls;
    }
    CHECK_EQ(controls, std::size_t(65));
}

TEST(TextLimits, HasControlCharacter_InvalidUtf8AnswersTrue)
{
    // Plan 6.2 S0.b: fail closed, text that cannot be decoded is never an acceptable name.
    for (const Bytes &b : invalid_texts())
    {
        SCOPED_CONTEXT(b.label);
        CHECK(has_control_character(b.text));
    }
}

// ---------------------------------------------------------------------------------------------------------
// TXL-06 has_line_break
// ---------------------------------------------------------------------------------------------------------
TEST(TextLimits, HasLineBreak_ExactlyTheSplitlinesCodePoints)
{
    // Oracle (Python 3.11.4): the code points c with len(("a" + chr(c) + "b").splitlines()) > 1 are U+000A U+000B U+000C U+000D U+001C
    // U+001D U+001E U+0085 U+2028 U+2029. Every scalar value is tried.
    const std::vector<std::uint32_t> breaks = {0x0A, 0x0B, 0x0C, 0x0D, 0x1C, 0x1D, 0x1E, 0x85, 0x2028, 0x2029};
    std::size_t found = 0;
    for (std::uint32_t cp = 0; cp <= 0x10FFFF; ++cp)
    {
        if (cp >= 0xD800 && cp <= 0xDFFF)
            continue;
        bool expected = false;
        for (std::uint32_t b : breaks)
            expected = expected || b == cp;
        if (has_line_break(encode_utf8(cp)) != expected)
            FAIL_TEST(u_plus(cp));
        if (expected)
            ++found;
    }
    CHECK_EQ(found, breaks.size());
}

TEST(TextLimits, HasLineBreak_TheCommonCases)
{
    CHECK(has_line_break("a\nb"));
    CHECK(has_line_break("a\r\nb"));
    CHECK(has_line_break("a\rb"));
    CHECK(has_line_break("line\n"));
    CHECK(has_line_break("\nline"));
    CHECK(has_line_break("a\xE2\x80\xA8" "b")); // U+2028
    CHECK(has_line_break("a\xE2\x80\xA9" "b")); // U+2029
    CHECK(has_line_break("a" OST_U_NEL "b"));   // U+0085
    CHECK(has_line_break("a\x0B" "b"));         // VT
    CHECK(has_line_break("a\x0C" "b"));         // FF
}

TEST(TextLimits, HasLineBreak_OtherTextIsNot)
{
    CHECK_FALSE(has_line_break(""));
    CHECK_FALSE(has_line_break("Walls"));
    CHECK_FALSE(has_line_break("a b"));
    CHECK_FALSE(has_line_break("a\tb")); // a tab is a control character, not a line break
    CHECK_FALSE(has_line_break(std::string("a\0b", 3)));
    CHECK_FALSE(has_line_break(OST_U_NBSP));
    CHECK_FALSE(has_line_break(OST_U_ZWSP));
    CHECK_FALSE(has_line_break(OST_U_IDEO));
    CHECK_FALSE(has_line_break(kEmoji));
    CHECK_FALSE(has_line_break("\x7F"));
}

TEST(TextLimits, HasLineBreak_InvalidUtf8AnswersTrue)
{
    // The same fail-closed rule as has_control_character (see the header).
    for (const Bytes &b : invalid_texts())
    {
        SCOPED_CONTEXT(b.label);
        CHECK(has_line_break(b.text));
    }
}

// ---------------------------------------------------------------------------------------------------------
// TXL-07 the rule each writer builds from the blocks (plan 4.2, N-M1-13)
// ---------------------------------------------------------------------------------------------------------
TEST(TextLimits, LayerNameRule_ControlCharactersOrLineBreaks)
{
    // LY-14: a layer name is refused by has_control_character OR has_line_break; the two together catch every line break, U+2028 and
    // U+2029 included, which are not control characters.
    const auto refused = [](const std::string &name) { return has_control_character(name) || has_line_break(name); };
    CHECK_FALSE(refused("Walls"));
    CHECK_FALSE(refused("Walls 2 " OST_U_EURO));
    CHECK_FALSE(refused("A" OST_U_NBSP "B")); // a no-break space is accepted
    CHECK_FALSE(refused(kEmoji));
    CHECK(refused("A\nB"));
    CHECK(refused("A\r\nB"));
    CHECK(refused("A\tB"));
    CHECK(refused("A\x7F" "B"));
    CHECK(refused("A" OST_U_NEL "B"));
    CHECK(refused("A\xE2\x80\xA8" "B")); // caught by has_line_break only
    CHECK(refused("A\xE2\x80\xA9" "B"));
    CHECK(refused(std::string("A\0B", 3)));
    CHECK(refused("A\xFF" "B")); // not UTF-8
}

TEST(TextLimits, PageNameRule_OnlyNulIsRefusedByTheCharacterRule)
{
    // N-M1-13: a page name is refused when it contains NUL (and over 255 units); a tab or a line break is accepted by this rule.
    CHECK(contains_nul(std::string("Page\0 1", 7)));
    CHECK_FALSE(contains_nul("Page\t1"));
    CHECK_FALSE(contains_nul("Page\n1"));
    CHECK_FALSE(contains_nul("Page 1 " + kEmoji));
}
