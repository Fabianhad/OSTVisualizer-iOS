// IMP-10 / IMP-11 / IMP-01 / IMP-05 support: the Python text rules the Windows importer relies on.
//
// Windows keys (import_operations.py / master_data_identity.py): CdnTypes.Name = str.strip().lower(); employee keys =
// str.strip().casefold(); Settings.NextBidNo and every UID floor read use int(value); typed columns use int()/float().
// The expected values below were computed with CPython 3.11.4 (the interpreter of the Windows app) from the listed
// inputs; non-ASCII text is written as octal UTF-8 escapes. No Windows test pins most of these (only the ones named in
// the comments), so they are the differential oracle for the C++ port.
#include "TestHarness.hpp"

#include "Import/TextRules.hpp"

#include <string>
#include <vector>

namespace tr = ost_import::text_rules;

namespace
{
    struct CaseRow
    {
        const char *input;
        const char *stripped;
        const char *lowered;
        const char *folded;
    };

    const CaseRow kCaseRows[] = {
        {"", "", "", ""},
        {"Concrete", "Concrete", "concrete", "concrete"},
        {" Concrete ", "Concrete", " concrete ", " concrete "},
        {"  concrete\011", "concrete", "  concrete\011", "  concrete\011"},
        {"CONCRETE", "CONCRETE", "concrete", "concrete"},
        {"Funda\303\247\303\243o", "Funda\303\247\303\243o", "funda\303\247\303\243o", "funda\303\247\303\243o"},
        {"FUNDA\303\207\303\203O", "FUNDA\303\207\303\203O", "funda\303\247\303\243o", "funda\303\247\303\243o"},
        {"  Alvenaria Estrutural  ", "Alvenaria Estrutural", "  alvenaria estrutural  ", "  alvenaria estrutural  "},
        {"Stra\303\237e", "Stra\303\237e", "stra\303\237e", "strasse"},
        {"STRASSE", "STRASSE", "strasse", "strasse"},
        {"\302\240Nbsp\302\240", "Nbsp", "\302\240nbsp\302\240", "\302\240nbsp\302\240"},
        {"\342\200\203em space\343\200\200", "em space", "\342\200\203em space\343\200\200", "\342\200\203em space\343\200\200"},
        {"\316\221\316\243", "\316\221\316\243", "\316\261\317\202", "\316\261\317\203"},
        {"\316\221\316\243 \316\222", "\316\221\316\243 \316\222", "\316\261\317\202 \316\262", "\316\261\317\203 \316\262"},
        {"\316\243\316\221\316\243", "\316\243\316\221\316\243", "\317\203\316\261\317\202", "\317\203\316\261\317\203"},
        {"\316\243", "\316\243", "\317\203", "\317\203"},
        {"\316\221\316\243.", "\316\221\316\243.", "\316\261\317\202.", "\316\261\317\203."},
        {"\316\221.\316\243", "\316\221.\316\243", "\316\261.\317\202", "\316\261.\317\203"},
        {"\304\260stanbul", "\304\260stanbul", "i\314\207stanbul", "i\314\207stanbul"},
        {"\307\205", "\307\205", "\307\206", "\307\206"},
        {"\307\210", "\307\210", "\307\211", "\307\211"},
        {"\307\213", "\307\213", "\307\214", "\307\214"},
        {"\316\214\316\243\316\237\316\243", "\316\214\316\243\316\237\316\243", "\317\214\317\203\316\277\317\202", "\317\214\317\203\316\277\317\203"},
        {"\304\260", "\304\260", "i\314\207", "i\314\207"},
        {"\303\237", "\303\237", "\303\237", "ss"},
        {"\357\254\201", "\357\254\201", "\357\254\201", "fi"},
        {"\305\211", "\305\211", "\305\211", "\312\274n"},
        {"\307\260", "\307\260", "\307\260", "j\314\214"},
        {"\316\220", "\316\220", "\316\220", "\316\271\314\210\314\201"},
        {"\302\265", "\302\265", "\302\265", "\316\274"},
        {"\317\202", "\317\202", "\317\202", "\317\203"},
        {"\341\275\210\316\264\317\205\317\203\317\203\316\265\317\215\317\202", "\341\275\210\316\264\317\205\317\203\317\203\316\265\317\215\317\202", "\341\275\200\316\264\317\205\317\203\317\203\316\265\317\215\317\202", "\341\275\200\316\264\317\205\317\203\317\203\316\265\317\215\317\203"},
        {"\307\205\307\206\307\207", "\307\205\307\206\307\207", "\307\206\307\206\307\211", "\307\206\307\206\307\211"},
        {"\342\205\247", "\342\205\247", "\342\205\267", "\342\205\267"},
        {"\342\223\220\342\222\266", "\342\223\220\342\222\266", "\342\223\220\342\223\220", "\342\223\220\342\223\220"},
        {"\360\220\220\200", "\360\220\220\200", "\360\220\220\250", "\360\220\220\250"},
        {"\034\035 x \036\037", "x", "\034\035 x \036\037", "\034\035 x \036\037"},
        {"a\302\205", "a", "a\302\205", "a\302\205"},
        {"\342\200\213zero width\342\200\213", "\342\200\213zero width\342\200\213", "\342\200\213zero width\342\200\213", "\342\200\213zero width\342\200\213"},
    };

    struct IntRow
    {
        const char *input;
        int status; // 0 ok, 1 ValueError, 2 valid but beyond 64 bits
        long long value;
    };

    const IntRow kIntRows[] = {
        {"7", 0, 7LL},
        {"0", 0, 0LL},
        {"-3", 0, -3LL},
        {"+5", 0, 5LL},
        {" 12 ", 0, 12LL},
        {"1_000", 0, 1000LL},
        {"1__0", 1, 0LL},
        {"_1", 1, 0LL},
        {"1_", 1, 0LL},
        {"", 1, 0LL},
        {"abc", 1, 0LL},
        {"7.0", 1, 0LL},
        {"00012", 0, 12LL},
        {"-0", 0, 0LL},
        {"9223372036854775807", 0, 9223372036854775807LL},
        {"-9223372036854775808", 0, (-9223372036854775807LL - 1)},
        {"\302\2409\302\240", 0, 9LL},
        {"0x10", 1, 0LL},
        {"1e3", 1, 0LL},
        {"- 1", 1, 0LL},
        {"9223372036854775808", 2, 0LL}, // overflow
        {"-9223372036854775809", 2, 0LL}, // overflow
    };

    struct FloatRow
    {
        const char *input;
        int ok;
        double value;
    };

    const FloatRow kFloatRows[] = {
        {"1.5", 1, 1.5},
        {"4.0", 1, 4.0},
        {"-2.25", 1, -2.25},
        {"+.5", 1, 0.5},
        {"5.", 1, 5.0},
        {".", 0, 0.0},
        {"1.2.3", 0, 0.0},
        {" 1.5 ", 1, 1.5},
        {"1_0.5", 1, 10.5},
        {"1._5", 0, 0.0},
        {"1.5e3", 1, 1500.0},
        {"1.5E-2", 1, 0.015},
        {"1.5e", 0, 0.0},
        {"1e5.0", 0, 0.0},
        {"0.1", 1, 0.1},
        {"123456789.123456789", 1, 123456789.12345679},
        {"1.", 1, 1.0},
        {"-.5e1", 1, -5.0},
        {"1.5f", 0, 0.0},
        {"", 0, 0.0},
        {"inf.", 0, 0.0},
        {"1,5", 0, 0.0},
    };
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_employee_identity_keys_fold_case_with_casefold_not_lower
// (STRASSE vs Strasse sharp s) and ::test_access_import_uses_condition_type_name_contract (' Concrete ' vs 'concrete').
TEST(TextRules, StripMatchesPython)
{
    for (const CaseRow &row : kCaseRows)
    {
        SCOPED_CONTEXT(std::string("input ") + row.input);
        CHECK_STR_EQ(tr::py_strip(row.input), std::string(row.stripped));
    }
}

TEST(TextRules, LowerMatchesPython)
{
    for (const CaseRow &row : kCaseRows)
    {
        SCOPED_CONTEXT(std::string("input ") + row.input);
        CHECK_STR_EQ(tr::py_lower(row.input), std::string(row.lowered));
    }
}

TEST(TextRules, CasefoldMatchesPython)
{
    for (const CaseRow &row : kCaseRows)
    {
        SCOPED_CONTEXT(std::string("input ") + row.input);
        CHECK_STR_EQ(tr::py_casefold(row.input), std::string(row.folded));
    }
}

TEST(TextRules, PortugueseMixedCaseNamesFoldTogether)
{
    // The CdnTypes name contract is strip().lower(): accents and case must not make a second condition type.
    CHECK_STR_EQ(tr::py_lower(tr::py_strip("  FUNDA\303\207\303\203O ")), std::string("funda\303\247\303\243o"));
    CHECK_STR_EQ(tr::py_lower(tr::py_strip("Funda\303\247\303\243o")), std::string("funda\303\247\303\243o"));
}

TEST(TextRules, InvalidUtf8BytesPassThroughUnchanged)
{
    // A lone 0xE9 (windows-1252 e-acute) is neither whitespace nor cased: kept as is by all three.
    const std::string bytes = "  Caf\351  ";
    CHECK_STR_EQ(tr::py_strip(bytes), std::string("Caf\351"));
    CHECK_STR_EQ(tr::py_lower("CAF\351"), std::string("caf\351"));
    CHECK_STR_EQ(tr::py_casefold("CAF\351"), std::string("caf\351"));
}

TEST(TextRules, ParseIntMatchesPythonInt)
{
    for (const IntRow &row : kIntRows)
    {
        SCOPED_CONTEXT(std::string("input '") + row.input + "'");
        long long value = 12345;
        const tr::NumberParse status = tr::py_parse_int(row.input, value);
        if (row.status == 0)
        {
            CHECK(status == tr::NumberParse::Ok);
            CHECK_EQ(value, row.value);
        }
        else if (row.status == 1)
        {
            CHECK(status == tr::NumberParse::Invalid);
        }
        else
        {
            CHECK(status == tr::NumberParse::Overflow);
        }
    }
}

TEST(TextRules, ParseFloatMatchesPythonFloat)
{
    for (const FloatRow &row : kFloatRows)
    {
        SCOPED_CONTEXT(std::string("input '") + row.input + "'");
        double value = -1.0;
        const bool ok = tr::py_parse_float(row.input, value);
        CHECK_EQ(ok, row.ok != 0);
        if (row.ok)
            CHECK_EQ(value, row.value); // exact: both sides are correctly rounded decimal -> double
    }
}
