// DBP-07 (serialize_position keeps full precision) and DBP-08 (parse_position rejects partly numeric tokens).
// Windows oracle: domain/utils/position.py::parse_position and
// infrastructure/mdb/components/serialization.py::encode_position(position, preserve_indices=all indices),
// the form every takeoff write path uses (takeoff_operations.py:256-262, 508-511).
#include "ParsersTestSupport.hpp"
#include "PositionOracleData.hpp"

#include "Persistence/ProjectStore/ProjectStore_Internal.hpp"

#include <cmath>
#include <limits>

namespace
{
    using Vec = std::vector<double>;

    Vec parse(const std::string &text) { return ost_store::parse_position(text); }

    // exact equality of every element, including the sign of a zero (Windows -0.0 vs 0.0)
    void expect_positions_equal(const Vec &got, const Vec &want)
    {
        REQUIRE_EQ(got.size(), want.size());
        for (size_t i = 0; i < got.size(); ++i)
        {
            SCOPED_CONTEXT("index " + std::to_string(i));
            if (std::isnan(want[i]))
            {
                CHECK(std::isnan(got[i]));
                continue;
            }
            CHECK_EQ(got[i], want[i]);
            CHECK_EQ(std::signbit(got[i]), std::signbit(want[i]));
        }
    }

    void expect_parse(const std::string &text, const Vec &want)
    {
        SCOPED_CONTEXT("text=" + text);
        expect_positions_equal(parse(text), want);
    }
}

// ---------------------------------------------------------------------------------------------------------
// DBP-08 parse_position
// ---------------------------------------------------------------------------------------------------------
TEST(PositionParse, PlainSequences)
{
    // Port of tests/infrastructure/mdb/components/test_serialization.py::test_position_parser_reads_text_and_binary_storage_values
    expect_parse("1;2;3;4", {1.0, 2.0, 3.0, 4.0});
    expect_parse("1;2.5;-3\n", {1.0, 2.5, -3.0});
    expect_parse("1;2;3;4;", {1.0, 2.0, 3.0, 4.0});
    expect_parse(" 1 ; 2 ; 3 ", {1.0, 2.0, 3.0});
    expect_parse("1.5;2.5; 3", {1.5, 2.5, 3.0});
}

TEST(PositionParse, EmptyOrNonNumericStorageHasNoGeometry)
{
    // Port of ...::test_position_parser_returns_no_geometry_for_empty_or_non_numeric_storage
    expect_parse("", {});
    expect_parse("   ", {});
    expect_parse("not-a-position", {});
    expect_parse("1;x;3", {});
    expect_parse("1;oops", {});
    expect_parse("abc;2", {});
    expect_parse("1;bad;3", {});
}

TEST(PositionParse, XmlNewlineEntitiesAreStripped)
{
    // Port of tests/integration/persistence/test_position_contracts.py::test_position_parsing_preserves_previous_text_edge_cases
    // (entity spellings handled by text_cleanup.strip_xml_newline_entities, case-insensitive)
    expect_parse("1; 2;&#xA;3;4", {1.0, 2.0, 3.0, 4.0});
    expect_parse("1;&#10;2;&#x0A;3;&#xA;4", {1.0, 2.0, 3.0, 4.0});
    expect_parse("1;2&#xa;;3", {1.0, 2.0, 3.0});
    expect_parse("&#xA;", {});
    expect_parse("&#x0a;", {});
    expect_parse("&#x0A;", {});
    expect_parse("&#10;", {});
    expect_parse("1;;  2 ;\n", {1.0, 2.0});
    expect_parse("1&#X0a;;2", {1.0, 2.0});
    expect_parse("1&#x0A;2", {12.0});
    // &#13; is not one of the stripped entities: the token stays unreadable
    expect_parse("1&#13;;2", {});
}

TEST(PositionParse, PartlyNumericTokensGiveNoGeometry)
{
    // DBP-08 / POS-01 / GEO-M03 test plans (Windows float() refuses every token below)
    expect_parse("1.5abc;2", {});
    expect_parse("1,5;2", {});
    expect_parse("1.5 2.5;3", {});
    expect_parse("1 2;3", {});
    expect_parse("1.2.3;4", {});
    expect_parse("1-2;3", {});
    expect_parse("1e;2", {});
    expect_parse("0x10;2", {});
    expect_parse("0x10;1", {});
    expect_parse("1.5;2x;3", {});
    expect_parse("1;2 3", {});
    expect_parse("-;1", {});
    expect_parse("+;1", {});
    expect_parse(".;1", {});
    expect_parse("e5;1", {});
    expect_parse("1e+;1", {});
}

TEST(PositionParse, FloatSpellingsPythonAccepts)
{
    expect_parse("1e5;2", {100000.0, 2.0});
    expect_parse("+3;4", {3.0, 4.0});
    expect_parse(".5;5.", {0.5, 5.0});
    expect_parse(".5;5.;+3;-2e2", {0.5, 5.0, 3.0, -200.0});
    expect_parse("1E5;1", {100000.0, 1.0});
    expect_parse("1.e5;1", {100000.0, 1.0});
    expect_parse("00012;1", {12.0, 1.0});
    expect_parse("-0;1", {-0.0, 1.0});
    expect_parse("1_0;2", {10.0, 2.0});
    expect_parse("1_0.5_5;3", {10.55, 3.0});
    expect_parse("1__0;2", {});
    expect_parse("_1;2", {});
    expect_parse("1_;2", {});
}

TEST(PositionParse, OutOfRangeValuesAreInfinityOrZeroNotAnError)
{
    // 1e400 -> inf and 1e-400 -> 0.0 in Windows float(); the old std::stod threw out_of_range and dropped the position
    expect_parse("1e400;1", {std::numeric_limits<double>::infinity(), 1.0});
    expect_parse("-1e400;1", {-std::numeric_limits<double>::infinity(), 1.0});
    expect_parse("1e-400;1", {0.0, 1.0});
    expect_parse("1e308;1e309", {1e308, std::numeric_limits<double>::infinity()});
}

TEST(PositionParse, InfAndNanAreAcceptedLikePythonFloat)
{
    const double inf = std::numeric_limits<double>::infinity();
    expect_parse("inf;2", {inf, 2.0});
    expect_parse("-inf;2", {-inf, 2.0});
    expect_parse("+Infinity;2", {inf, 2.0});
    expect_parse("INF;1", {inf, 1.0});
    expect_parse("infx;1", {});
    expect_parse("in f;1", {});
    const Vec nan_result = parse("nan;1");
    REQUIRE_EQ(nan_result.size(), size_t(2));
    CHECK(std::isnan(nan_result[0]));
    CHECK_EQ(nan_result[1], 1.0);
}

TEST(PositionParse, UnicodeWhitespaceAroundTokensIsStripped)
{
    // Python str.strip()/isspace: NBSP, EM SPACE, NEL, FS (0x1c) count as whitespace; U+200B does not
    expect_parse(OST_U_NBSP "1;2", {1.0, 2.0});
    expect_parse("1;2;3;" OST_U_NBSP, {1.0, 2.0, 3.0});
    expect_parse("1;" OST_U_EM " 2 " OST_U_EM ";3", {1.0, 2.0, 3.0});
    expect_parse("1;" OST_U_NEL " 2;3", {1.0, 2.0, 3.0});
    expect_parse("1;\x1c" "2;3", {1.0, 2.0, 3.0});
    expect_parse("1;\x0b" "2\x0c;3", {1.0, 2.0, 3.0});
    expect_parse("1\r\n;2\r\n", {1.0, 2.0});
    expect_parse("1" OST_U_ZWSP ";2", {});
    expect_parse("1" OST_U_NBSP "2;3", {});
}

TEST(PositionParse, UnicodeDecimalDigitsAreDigitsToPythonFloat)
{
    // Python float() converts any Unicode decimal digit (Nd): ARABIC-INDIC 1 2 and FULLWIDTH 1
    expect_parse("\xD9\xA1\xD9\xA2;3", {12.0, 3.0});
    expect_parse("\xEF\xBC\x91;2", {1.0, 2.0});
}

TEST(PositionParse, ParseMatchesTheWindowsOracle)
{
    // Every row was produced by running Windows parse_position (see PositionOracleData.hpp)
    size_t rows = 0;
    for (const auto &row : ost_parsers_test::kPositionParseOracle)
    {
        SCOPED_CONTEXT(std::string("text=") + row.text);
        expect_positions_equal(parse(row.text), row.expected);
        ++rows;
    }
    CHECK(rows > 800);
}

// ---------------------------------------------------------------------------------------------------------
// DBP-07 serialize_position
// ---------------------------------------------------------------------------------------------------------
TEST(PositionSerialize, WindowsTextForm)
{
    // Port of tests/infrastructure/mdb/components/test_serialization.py::test_position_serializer_returns_bytes_for_binary_position_tables
    // (BidDimensions column: preserve all indices) and test_takeoff_operations.py (insert keeps unrounded positions)
    CHECK_STR_EQ(ost_store::serialize_position({1.5, 2.25, 3.0, 4.125}), "1.5;2.25;3.0;4.125\n");
    CHECK_STR_EQ(ost_store::serialize_position({10.0, 20.0, 30.0, 40.5}), "10.0;20.0;30.0;40.5\n");
    CHECK_STR_EQ(ost_store::serialize_position({0.1234567, 100.0005, 1234.567890123456, 5.0}),
                 "0.1234567;100.0005;1234.567890123456;5.0\n");
    CHECK_STR_EQ(ost_store::serialize_position({4.0, 12345.678901234, 0.1 + 0.2}), "4.0;12345.678901234;0.30000000000000004\n");
    CHECK_STR_EQ(ost_store::serialize_position({3.0}), "3.0\n");
    CHECK_STR_EQ(ost_store::serialize_position({1e-05}), "1e-05\n");
    CHECK_STR_EQ(ost_store::serialize_position({1e16}), "1e+16\n");
    CHECK_STR_EQ(ost_store::serialize_position({}), "\n");
}

TEST(PositionSerialize, FullPrecisionRoundTripsBitExactly)
{
    // Port of tests/infrastructure/mdb/components/test_serialization.py::DimensionLifecycleTests and
    // ::test_text_geometry_roundtrip_preserves_fractional_dimensions_and_rotation (assertEqual on floats)
    const Vec sets[] = {
        {1.0 / 64, -1.0 / 25.4, 31.0 / 64, 2.0 / 25.4},
        {10.1234567, -100.7654321, 10000.1234567, -4.0123456},
        {1.0 / 64, -1.0 / 25.4, 80.123456, 24.654321, 3.141592653589793 / 7},
        {0.1, 0.2, 0.30000000000000004},
        {12345.678901234, 1e-7, 123456789.123456789},
    };
    for (const Vec &position : sets)
    {
        const std::string stored = ost_store::serialize_position(position);
        SCOPED_CONTEXT("stored=" + stored);
        expect_positions_equal(parse(stored), position);
    }
}

TEST(PositionSerialize, MatchesTheWindowsOracleAndRoundTrips)
{
    // Every row was produced by running Windows encode_position(preserve_indices=all) (see PositionOracleData.hpp)
    size_t rows = 0;
    for (const auto &row : ost_parsers_test::kPositionEncodeOracle)
    {
        const std::string got = ost_store::serialize_position(row.values);
        SCOPED_CONTEXT("expected=" + std::string(row.expected));
        CHECK_STR_EQ(got, row.expected);
        // every finite value survives serialize -> parse bit-exactly
        bool finite = true;
        for (double v : row.values)
            finite = finite && std::isfinite(v);
        if (finite)
            expect_positions_equal(parse(got), row.values);
        ++rows;
    }
    CHECK(rows > 400);
}
