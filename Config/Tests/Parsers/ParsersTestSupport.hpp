// ParsersTestSupport.hpp - shared helpers for the Parsers host tests (elevation text, position text, colour fill).
//
// Non-ASCII test text is written with the OST_U_* UTF-8 escape macros inside adjacent string literals
// ("6" OST_U_NBSP "1/2", never a raw character), so the source shows exactly what is being tested.
#pragma once

#include "TestHarness.hpp"

#include <string>
#include <utility>
#include <vector>

// UTF-8 spellings of the characters the Windows tests name with Unicode escapes.
#define OST_U_NBSP "\xC2\xA0"        // U+00A0 no-break space
#define OST_U_NNBSP "\xE2\x80\xAF"   // U+202F narrow no-break space
#define OST_U_THIN "\xE2\x80\x89"    // U+2009 thin space
#define OST_U_FIGURE "\xE2\x80\x87"  // U+2007 figure space
#define OST_U_EM "\xE2\x80\x83"      // U+2003 em space
#define OST_U_IDEO "\xE3\x80\x80"    // U+3000 ideographic space
#define OST_U_NEL "\xC2\x85"         // U+0085 next line
#define OST_U_ZWSP "\xE2\x80\x8B"    // U+200B zero width space (NOT whitespace)
#define OST_U_PRIME "\xE2\x80\xB2"   // U+2032 prime
#define OST_U_DPRIME "\xE2\x80\xB3"  // U+2033 double prime
#define OST_U_ENDASH "\xE2\x80\x93"  // U+2013 en dash
#define OST_U_EURO "\xE2\x82\xAC"    // U+20AC euro sign

namespace ost_parsers_test
{
    struct Separator
    {
        const char *label;
        std::string text;
    };

    // Port of tests/infrastructure/parsers/test_position_parser.py::_UNICODE_SEPARATORS (every character
    // str.split() and the regex \s treat as whitespace; U+200B is deliberately not in it).
    inline const std::vector<Separator> &unicode_separators()
    {
        static const std::vector<Separator> k = {
            {"ascii space", " "},
            {"no-break space", OST_U_NBSP},
            {"narrow no-break space", OST_U_NNBSP},
            {"thin space", OST_U_THIN},
            {"figure space", OST_U_FIGURE},
            {"em space", OST_U_EM},
            {"ideographic space", OST_U_IDEO},
            {"tab", "\t"},
            {"vertical tab", "\x0b"},
            {"line feed", "\n"},
            {"next line", OST_U_NEL},
            {"two mixed", OST_U_NBSP OST_U_NNBSP},
        };
        return k;
    }

    // Windows' assertAlmostEqual(a, b, places=9): round(a - b, 9) == 0, i.e. |a - b| < 5e-10.
    constexpr double kAlmost9 = 5e-10;
}
