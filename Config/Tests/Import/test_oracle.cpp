// Differential oracle for IMP-06 / IMP-03 / IMP-04 / IMP-07 (UID remap + page-area canonicalisation) and for the Python text
// rules behind IMP-01 / IMP-05 / IMP-10 / IMP-11.
//
// Fixtures/import_transform_oracle.txt and Fixtures/import_text_oracle.txt were produced by running the REAL Windows code
// (ost_importer.OstImporter._transform, which runs _build_uid_map, _build_page_uid_map, _remap_row and
// canonicalize_page_area_settings; and str.strip/lower/casefold, int(), float()) on seeded random inputs
// (scratchpad/import/gen_oracles.py, CPython 3.11.4, read-only, python -B). Inputs include blank/zero/absent/duplicate UIDs,
// padded page references, non-numeric BidAreaSelected, every reference field family, random table subsets and orders.
#include "ImportTestSupport.hpp"

#include "Import/OstImportTransform.hpp"
#include "Import/TextRules.hpp"

#include <cstdlib>
#include <string>
#include <vector>

using ost_import::OstRawData;
using ost_import::Row;
using ost_import::RowList;
using ost_import::UidMap;

namespace
{
    std::vector<std::string> split(const std::string &text, char sep)
    {
        std::vector<std::string> parts;
        size_t start = 0;
        for (;;)
        {
            const size_t pos = text.find(sep, start);
            if (pos == std::string::npos)
            {
                parts.push_back(text.substr(start));
                return parts;
            }
            parts.push_back(text.substr(start, pos - start));
            start = pos + 1;
        }
    }

    std::vector<std::string> lines_of(const std::string &content)
    {
        std::vector<std::string> lines = split(content, '\n');
        for (std::string &line : lines)
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
        return lines;
    }

    Row parse_row(const std::string &payload)
    {
        Row row;
        if (payload.empty())
            return row;
        for (const std::string &entry : split(payload, '\t'))
        {
            const size_t eq = entry.find('=');
            row[entry.substr(0, eq)] = entry.substr(eq + 1);
        }
        return row;
    }

    UidMap parse_map(const std::string &payload)
    {
        return parse_row(payload);
    }

    std::string unhex(const std::string &hex)
    {
        std::string out;
        for (size_t i = 0; i + 1 < hex.size(); i += 2)
            out += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
        return out;
    }

    struct OracleCase
    {
        int number = 0;
        long long max_uid = 0;
        UidMap cdn, job, emp, pay;
        OstRawData input, expected;
    };

    std::vector<OracleCase> load_transform_cases(const std::string &path)
    {
        std::string content;
        if (!ost_test::read_file(path, content))
            FAIL_TEST("cannot read " + path);
        std::vector<OracleCase> cases;
        OstRawData *target = nullptr;
        ost_import::TableMap *table_map = nullptr;
        std::string current_table;
        for (const std::string &line : lines_of(content))
        {
            if (line.empty())
                continue;
            if (line.rfind("CASE ", 0) == 0)
            {
                cases.emplace_back();
                cases.back().number = std::stoi(line.substr(5));
                target = &cases.back().input;
                table_map = nullptr;
            }
            else if (line.rfind("MAXUID ", 0) == 0)
                cases.back().max_uid = std::stoll(line.substr(7));
            else if (line.rfind("MAP cdn", 0) == 0)
                cases.back().cdn = parse_map(line.size() > 8 ? line.substr(8) : "");
            else if (line.rfind("MAP job", 0) == 0)
                cases.back().job = parse_map(line.size() > 8 ? line.substr(8) : "");
            else if (line.rfind("MAP emp", 0) == 0)
                cases.back().emp = parse_map(line.size() > 8 ? line.substr(8) : "");
            else if (line.rfind("MAP pay", 0) == 0)
                cases.back().pay = parse_map(line.size() > 8 ? line.substr(8) : "");
            else if (line == "EXPECT")
            {
                target = &cases.back().expected;
                table_map = nullptr;
            }
            else if (line == "END")
                target = nullptr;
            else if (line.rfind("BID", 0) == 0 && (line.size() == 3 || line[3] == ' '))
                target->bid_row = parse_row(line.size() > 4 ? line.substr(4) : "");
            else if (line.rfind("BTABLE ", 0) == 0)
            {
                table_map = &target->bid_tables;
                current_table = line.substr(7);
                (*table_map)[current_table];
            }
            else if (line.rfind("PTABLE ", 0) == 0)
            {
                table_map = &target->page_tables;
                current_table = line.substr(7);
                (*table_map)[current_table];
            }
            else if (line.rfind("ROW", 0) == 0)
                (*table_map)[current_table].push_back(parse_row(line.size() > 4 ? line.substr(4) : ""));
        }
        return cases;
    }

    std::string show_tables(const ost_import::TableMap &tables)
    {
        std::string out;
        for (const auto &table : tables)
        {
            out += table.first + "[";
            for (const Row &row : table.second)
                out += "{" + ost_itest::describe(row) + "}";
            out += "]";
        }
        return out;
    }
}

// The Windows code is the oracle: the iOS transform must give the identical bid row, tables (names, ORDER, rows) and page
// tables for every random input.
TEST(ImportOracle, Imp06_TransformMatchesTheWindowsImporterOnRandomInputs)
{
    const std::vector<OracleCase> cases = load_transform_cases(ost_test::fixture_path("import_transform_oracle.txt"));
    REQUIRE_EQ(cases.size(), 40u);
    for (const OracleCase &c : cases)
    {
        SCOPED_CONTEXT("oracle case " + std::to_string(c.number));
        const OstRawData actual = ost_import::transform(c.input, c.max_uid, c.cdn, c.job, c.emp, c.pay);
        CHECK_EQ(ost_itest::describe(actual.bid_row), ost_itest::describe(c.expected.bid_row));
        CHECK_EQ(show_tables(actual.bid_tables), show_tables(c.expected.bid_tables));
        CHECK_EQ(show_tables(actual.page_tables), show_tables(c.expected.page_tables));
        CHECK(actual.global_tables.empty());
    }
}

// str.strip() / str.lower() / str.casefold() on random strings from a pool of interesting characters (accents, sharp s,
// dotted I, sigma in all positions, whitespace variants, titlecase digraphs, combining marks, astral letters).
TEST(ImportOracle, TextRulesMatchPythonOnRandomStrings)
{
    std::string content;
    REQUIRE(ost_test::read_file(ost_test::fixture_path("import_text_oracle.txt"), content));
    int checked = 0;
    bool in_text = false;
    for (const std::string &line : lines_of(content))
    {
        if (line.rfind("# text", 0) == 0)
        {
            in_text = true;
            continue;
        }
        if (line.rfind("# numbers", 0) == 0)
        {
            in_text = false;
            continue;
        }
        if (!in_text || line.empty())
            continue;
        const std::vector<std::string> cols = split(line, '\t');
        REQUIRE_EQ(cols.size(), 4u);
        const std::string input = unhex(cols[0]);
        SCOPED_CONTEXT("text case " + cols[0]);
        CHECK_EQ(ost_import::text_rules::py_strip(input), unhex(cols[1]));
        CHECK_EQ(ost_import::text_rules::py_lower(input), unhex(cols[2]));
        CHECK_EQ(ost_import::text_rules::py_casefold(input), unhex(cols[3]));
        ++checked;
    }
    CHECK_EQ(checked, 400);
}

// int(text) and float(text) (for texts with a '.') on random numeric-looking strings.
TEST(ImportOracle, NumberParsingMatchesPythonOnRandomStrings)
{
    std::string content;
    REQUIRE(ost_test::read_file(ost_test::fixture_path("import_text_oracle.txt"), content));
    int checked = 0;
    bool in_numbers = false;
    for (const std::string &line : lines_of(content))
    {
        if (line.rfind("# numbers", 0) == 0)
        {
            in_numbers = true;
            continue;
        }
        if (!in_numbers || line.empty())
            continue;
        const std::vector<std::string> cols = split(line, '\t');
        REQUIRE_EQ(cols.size(), 5u);
        const std::string input = unhex(cols[0]);
        SCOPED_CONTEXT("number case " + cols[0]);
        long long int_value = 0;
        const ost_import::text_rules::NumberParse status = ost_import::text_rules::py_parse_int(input, int_value);
        if (cols[1] == "ok")
        {
            CHECK(status == ost_import::text_rules::NumberParse::Ok);
            CHECK_EQ(int_value, std::stoll(cols[2]));
        }
        else if (cols[1] == "overflow")
        {
            CHECK(status == ost_import::text_rules::NumberParse::Overflow);
        }
        else
        {
            CHECK(status == ost_import::text_rules::NumberParse::Invalid);
        }
        if (cols[3] != "skip")
        {
            double float_value = 0.0;
            const bool ok = ost_import::text_rules::py_parse_float(input, float_value);
            CHECK_EQ(ok, cols[3] == "ok");
            if (ok && cols[3] == "ok")
                CHECK_EQ(float_value, std::strtod(cols[4].c_str(), nullptr));
        }
        ++checked;
    }
    CHECK_EQ(checked, 500);
}

// int(text) and float(text) on NON-ASCII input. CPython turns every Unicode whitespace into ' ' and every Unicode decimal digit
// into its ASCII digit before it parses (so "٣", "１２" and "7　" are numbers), keeps every character below
// 127 as it is (so \x1c..\x1f, which str.strip() removes, are NOT whitespace for int()), and rejects everything else.
// Fixtures/import_number_oracle.txt: scratchpad/import-pass1/gen_number_oracle.py on CPython 3.11.4 (Unicode 14.0.0),
// every one of the 650 Unicode decimal digits above ASCII plus whitespace/lookalike/ASCII mixes.
TEST(ImportOracle, NumberParsingMatchesPythonOnUnicodeInput)
{
    std::string content;
    REQUIRE(ost_test::read_file(ost_test::fixture_path("import_number_oracle.txt"), content));
    int checked = 0;
    for (const std::string &line : lines_of(content))
    {
        if (line.empty() || line[0] == '#')
            continue;
        const std::vector<std::string> cols = split(line, '\t');
        REQUIRE_EQ(cols.size(), 5u);
        const std::string input = unhex(cols[0]);
        SCOPED_CONTEXT("number case " + cols[0]);
        long long int_value = 0;
        const ost_import::text_rules::NumberParse status = ost_import::text_rules::py_parse_int(input, int_value);
        if (cols[1] == "ok")
        {
            CHECK(status == ost_import::text_rules::NumberParse::Ok);
            CHECK_EQ(int_value, std::stoll(cols[2]));
        }
        else if (cols[1] == "overflow")
        {
            CHECK(status == ost_import::text_rules::NumberParse::Overflow);
        }
        else
        {
            CHECK(status == ost_import::text_rules::NumberParse::Invalid);
        }
        if (cols[3] != "skip")
        {
            double float_value = 0.0;
            const bool ok = ost_import::text_rules::py_parse_float(input, float_value);
            CHECK_EQ(ok, cols[3] == "ok");
            if (ok && cols[3] == "ok")
                CHECK_EQ(float_value, std::strtod(cols[4].c_str(), nullptr));
        }
        ++checked;
    }
    CHECK_EQ(checked, 2087);
}

// CPython 3.11 refuses int(text) for a string of more than 4300 digits (sys.int_info.default_max_str_digits, the
// CVE-2020-10735 limit): a ValueError, which the Windows code reads as "unparsable" (import_operations._convert_access_value
// stores NULL, page_area_selection._integer_or_zero gives 0) instead of a huge integer that would fail the INSERT.
// Underscores do not count, leading zeros do. Expected values computed with the Windows interpreter (venv, CPython 3.11.4):
// int('9' * 4300) is a (too large for SQLite) integer, int('0' * 4300) == 0, while int('9' * 4301), int('0' * 4301),
// int('0' * 5000 + '5'), int('-' + '9' * 4301) and int('1_' * 4300 + '1') (4301 digits) raise ValueError;
// int('1_' * 4299 + '1') (4300 digits) is a large integer.
TEST(ImportOracle, IntegerTextWithMoreThan4300DigitsIsInvalidLikeInCPython)
{
    using ost_import::text_rules::NumberParse;
    using ost_import::text_rules::py_parse_int;
    long long value = -1;
    CHECK(py_parse_int(std::string(4300, '9'), value) == NumberParse::Overflow);
    CHECK(py_parse_int(std::string(4301, '9'), value) == NumberParse::Invalid);
    CHECK(py_parse_int("-" + std::string(4301, '9'), value) == NumberParse::Invalid);
    CHECK(py_parse_int(" " + std::string(4301, '9') + " ", value) == NumberParse::Invalid);
    value = -1;
    CHECK(py_parse_int(std::string(4300, '0'), value) == NumberParse::Ok);
    CHECK_EQ(value, 0LL);
    CHECK(py_parse_int(std::string(4301, '0'), value) == NumberParse::Invalid);
    CHECK(py_parse_int(std::string(5000, '0') + "5", value) == NumberParse::Invalid);
    std::string underscored;
    for (int i = 0; i < 4299; ++i)
        underscored += "1_";
    underscored += "1"; // 4300 digits
    CHECK(py_parse_int(underscored, value) == NumberParse::Overflow);
    underscored += "_1"; // 4301 digits
    CHECK(py_parse_int(underscored, value) == NumberParse::Invalid);
}
