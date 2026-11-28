// Layer: Domain (pure C++)
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The ONE helper set for the text limits of the M1 writers (plan 4.2 "Text limits", N-M1-13, S0.b): condition name 255, folder name 50,
// layer name 50, page name 255, and the character rules of layer names (control characters, line breaks) and page names (NUL). No Core
// function counted UTF-16 units before; every writer calls these and none writes a copy (inline functions with one name and different
// bodies in ost_domain would be a silent ODR violation).
//
// Why UTF-16 units: the Access VARCHAR(n) columns of the Windows schema hold n UTF-16 code units (database_creator.py:142 folder
// Name(50), :166 condition Name(255), :310 layer Name(50), :373 page Name(255)); a character outside the Basic Multilingual Plane (an
// emoji) takes two. Python's len() of a str counts code points, so for such a character it counts one: the units are the stricter and
// the Access-correct count, and Windows itself checks no length at all (register NC-4, CF-6, LY-8, RP-2), so these limits are the plan's
// safe behaviour, not a Windows rule.
//
// All functions take UTF-8 (what std::string holds everywhere in Core) and read it STRICTLY (RFC 3629, Unicode Table 3-7, the same
// strictness as Python's bytes.decode("utf-8")): a sequence is invalid when it is an overlong form (C0 80, E0 80 AF), a UTF-16 surrogate
// encoded in UTF-8 (ED A0 80 ... ED BF BF), above U+10FFFF (F4 90 80 80, F5 and up), truncated, or a stray continuation byte. No function
// here repairs or replaces anything.
#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace ost_domain
{

    // The number of UTF-16 code units of `utf8`: 1 for a code point up to U+FFFF, 2 for a code point from U+10000 (a surrogate pair);
    // 0 for the empty string; NUL and every other control character count 1 each. std::nullopt when `utf8` is not valid UTF-8 (see the
    // file comment): a writer refuses an invalid name, it never truncates or repairs it. The noncharacters (U+FFFE, U+FFFF) and the byte
    // order mark U+FEFF are valid and count 1.
    std::optional<std::size_t> utf16_length(const std::string &utf8);

    // true when `utf8` has a NUL, i.e. a 0x00 byte (the only valid encoding of U+0000; the overlong C0 80 is invalid UTF-8, not NUL).
    // A plain byte search, exact whether or not the rest is valid UTF-8, because a NUL cuts a C string (sqlite3_bind_text of c_str(),
    // the Windows driver) whatever follows. This is the whole character rule of a PAGE name (N-M1-13: "containing NUL").
    bool contains_nul(const std::string &utf8);

    // true when `utf8` has a control character: a code point of the Unicode category Cc, U+0000-U+001F (C0, which includes NUL, tab and
    // the line breaks LF and CR), U+007F (DEL) and U+0080-U+009F (C1, which includes U+0085 NEL). U+2028 LINE SEPARATOR and U+2029
    // PARAGRAPH SEPARATOR are NOT control characters (categories Zl and Zp): has_line_break names them. A tab is a control character
    // (a layer name, LY-14, may not hold one). Invalid UTF-8 ALSO answers true: text that cannot be decoded is never an acceptable
    // name, so a writer that only calls the predicates fails closed (plan 6.2 S0.b).
    bool has_control_character(const std::string &utf8);

    // true when `utf8` has a line break: the code points Python's str.splitlines() splits on, the Windows notion of a line break
    // (a pasted line break, LY-14): U+000A LF, U+000B VT, U+000C FF, U+000D CR, U+001C, U+001D, U+001E, U+0085 NEL, U+2028 and U+2029.
    // All but U+2028 and U+2029 are also control characters; a layer name is refused by has_control_character OR has_line_break (the
    // two together are "control characters / line breaks" of N-M1-13). Invalid UTF-8 answers true, like has_control_character.
    bool has_line_break(const std::string &utf8);

}
