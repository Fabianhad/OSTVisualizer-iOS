// python_repr (Core/Import/CabMemberName.hpp): repr(str) of CPython 3.11.4 (Unicode 14), the quoting of the Windows messages that print a
// member name with {name!r} (osp_importer._inspect_package, _image_destination_relative_path). Every expected text below was produced by
// repr() of the Windows interpreter (python -B) for the same code points: the characters str.isprintable() rejects (U+200B, U+3000, U+FEFF,
// U+2028, private use, tags, unassigned) are written as backslash escapes (x, u or U form), the printable ones stay as they are.
#include "TestHarness.hpp"

#include "Import/CabMemberName.hpp"

#include <string>

TEST(PythonRepr, NonPrintableCharactersAreEscapedLikeRepr)
{
    CHECK_STR_EQ(ost_import::python_repr("a" "\xE2" "\x80" "\x8B" "b"), "'a\\u200bb'");
    CHECK_STR_EQ(ost_import::python_repr("x" "\xC2" "\xA0" "y"), "'x\\xa0y'");
    CHECK_STR_EQ(ost_import::python_repr("\xE3" "\x80" "\x80"), "'\\u3000'");
    CHECK_STR_EQ(ost_import::python_repr("\xEF" "\xBB" "\xBF"), "'\\ufeff'");
    CHECK_STR_EQ(ost_import::python_repr("\xE2" "\x80" "\xA8"), "'\\u2028'");
    CHECK_STR_EQ(ost_import::python_repr("\xE2" "\x80" "\xA9"), "'\\u2029'");
    CHECK_STR_EQ(ost_import::python_repr("\xEE" "\x80" "\x80"), "'\\ue000'");
    CHECK_STR_EQ(ost_import::python_repr("\xF0" "\x9F" "\x98" "\x80"), "'" "\xF0" "\x9F" "\x98" "\x80" "'");
    CHECK_STR_EQ(ost_import::python_repr("\xF3" "\xA0" "\x80" "\x81"), "'\\U000e0001'");
    CHECK_STR_EQ(ost_import::python_repr("\xCD" "\xB8"), "'\\u0378'");
    CHECK_STR_EQ(ost_import::python_repr("\xF4" "\x8F" "\xBF" "\xBF"), "'\\U0010ffff'");
    CHECK_STR_EQ(ost_import::python_repr("caf" "\xC3" "\xA9"), "'caf" "\xC3" "\xA9" "'");
    CHECK_STR_EQ(ost_import::python_repr("\xD7" "\x90" "\xD7" "\x91"), "'" "\xD7" "\x90" "\xD7" "\x91" "'");
    CHECK_STR_EQ(ost_import::python_repr("it's" "\xE2" "\x80" "\x8B"), "\"it's\\u200b\"");
    CHECK_STR_EQ(ost_import::python_repr("say \"hi\"" "\xE2" "\x80" "\xAE"), "'say \"hi\"\\u202e'");
    CHECK_STR_EQ(ost_import::python_repr("it's \"both\"" "\xE2" "\x80" "\x8B"), "'it\\'s \"both\"\\u200b'");
    CHECK_STR_EQ(ost_import::python_repr("\xC2" "\xAD"), "'\\xad'");
    CHECK_STR_EQ(ost_import::python_repr("\xC2" "\x85"), "'\\x85'");
    CHECK_STR_EQ(ost_import::python_repr("\xE2" "\x80" "\x83"), "'\\u2003'");
    CHECK_STR_EQ(ost_import::python_repr("\xE2" "\x80" "\x8E" "\xE2" "\x80" "\x8F"), "'\\u200e\\u200f'");
    CHECK_STR_EQ(ost_import::python_repr("a" "\xD8" "\x9C" "b"), "'a\\u061cb'");
    CHECK_STR_EQ(ost_import::python_repr("\xE1" "\x9A" "\x80"), "'\\u1680'");
    CHECK_STR_EQ(ost_import::python_repr("\xE1" "\xA0" "\x8E"), "'\\u180e'");
    CHECK_STR_EQ(ost_import::python_repr("\xE2" "\x81" "\xA0"), "'\\u2060'");
    CHECK_STR_EQ(ost_import::python_repr("\xED" "\x9F" "\xBB"), "'" "\xED" "\x9F" "\xBB" "'");
    CHECK_STR_EQ(ost_import::python_repr("\xEF" "\xA3" "\xBF"), "'\\uf8ff'");
    CHECK_STR_EQ(ost_import::python_repr("\xEF" "\xA4" "\x80"), "'" "\xEF" "\xA4" "\x80" "'");
    CHECK_STR_EQ(ost_import::python_repr("\xEF" "\xB7" "\x90"), "'\\ufdd0'");
    CHECK_STR_EQ(ost_import::python_repr("\xEF" "\xBF" "\xBF"), "'\\uffff'");
    CHECK_STR_EQ(ost_import::python_repr("\xEF" "\xBF" "\xBD"), "'" "\xEF" "\xBF" "\xBD" "'");
    CHECK_STR_EQ(ost_import::python_repr("\xF0" "\x9D" "\x9F" "\x8E"), "'" "\xF0" "\x9D" "\x9F" "\x8E" "'");
    CHECK_STR_EQ(ost_import::python_repr("\xF0" "\xA0" "\x80" "\x80"), "'" "\xF0" "\xA0" "\x80" "\x80" "'");
    CHECK_STR_EQ(ost_import::python_repr("\xF0" "\xB1" "\x8D" "\x8A"), "'" "\xF0" "\xB1" "\x8D" "\x8A" "'");
    CHECK_STR_EQ(ost_import::python_repr("\xF0" "\xB1" "\x8D" "\x8B"), "'\\U0003134b'");
    CHECK_STR_EQ(ost_import::python_repr("\xF3" "\xA0" "\x87" "\xAF"), "'" "\xF3" "\xA0" "\x87" "\xAF" "'");
    CHECK_STR_EQ(ost_import::python_repr("\xF3" "\xA0" "\x84" "\x80"), "'" "\xF3" "\xA0" "\x84" "\x80" "'");
    CHECK_STR_EQ(ost_import::python_repr("\xF3" "\xB0" "\x80" "\x80"), "'\\U000f0000'");
    CHECK_STR_EQ(ost_import::python_repr("\xF3" "\xBF" "\xBF" "\xBF"), "'\\U000fffff'");
    CHECK_STR_EQ(ost_import::python_repr("\xF4" "\x80" "\x80" "\x80"), "'\\U00100000'");
    CHECK_STR_EQ(ost_import::python_repr("\xD8" "\x80"), "'\\u0600'");
    CHECK_STR_EQ(ost_import::python_repr("\xDB" "\x9D"), "'\\u06dd'");
    CHECK_STR_EQ(ost_import::python_repr("\xE0" "\xA2" "\x90"), "'\\u0890'");
    CHECK_STR_EQ(ost_import::python_repr("\xE1" "\xB4" "\xAC"), "'" "\xE1" "\xB4" "\xAC" "'");
    CHECK_STR_EQ(ost_import::python_repr("TempImages!.tmp\\" "\xE2" "\x80" "\x8B" "x.pdf"), "'TempImages!.tmp\\\\\\u200bx.pdf'");
}

// Bytes that are not well-formed UTF-8 are not characters: they stay as they are (member names are valid UTF-8, this is only the guard).
TEST(PythonRepr, MalformedUtf8StaysAsItIs)
{
    CHECK_STR_EQ(ost_import::python_repr("a" "\xFF" "b"), "'a" "\xFF" "b'");
    CHECK_STR_EQ(ost_import::python_repr("\xE2" "\x80"), "'" "\xE2" "\x80" "'");
}
