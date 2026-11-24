// Set Scale, the text forms of a stored position (plan 4.6 and 5.4, tests-first): the 17-table rewrite of ONE Position text, the three-decimal and the
// exact-repr forms, the rotation slots, the rows that are skipped and the rows the hardening refuses. Windows oracle: page_operations._rescale_page_positions
// (page_operations.py:154-217) with serialization.py:18-51, run read-only on every row of PageScaleTransformOracleData.hpp (kPositionRows).
//
// iOS stores memo / long-binary columns as TEXT (plan 4.5), so a Windows BLOB payload is a TEXT here and both forms are asserted as strings.
// Where iOS differs from Windows on purpose the test says so: a coordinate that is not finite or that underflows to 0 refuses the change (register SS-2).
#include "TestHarness.hpp"

#include "Domain/OverlayRect.hpp"
#include "Domain/PageScaleTransform.hpp"
#include "Domain/PositionParser.hpp"

#include "PageScaleTransformOracleData.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace ost_domain;
using std::string;
using std::vector;

namespace
{
    const char *kind_name(PositionRescaleKind kind)
    {
        switch (kind)
        {
        case PositionRescaleKind::Empty: return "Empty";
        case PositionRescaleKind::NotNumeric: return "NotNumeric";
        case PositionRescaleKind::Rescaled: return "Rescaled";
        case PositionRescaleKind::NotFinite: return "NotFinite";
        case PositionRescaleKind::Underflow: return "Underflow";
        case PositionRescaleKind::BadFactor: return "BadFactor";
        }
        return "?";
    }

    // Prints a kind in a failure message without an operator<<.
    string describe(const PositionRescale &result)
    {
        return string(kind_name(result.kind)) + " '" + result.text + "'";
    }

    const char *const kAnnotationTables[13] = {"BidDimensions", "BidALines",           "BidArrows",          "BidHighlights",       "BidTexts",
                                               "BidCallOuts",   "BidAnnotationRects",  "BidAnnotationOvals", "BidAnnotationPolygons", "BidAnnotationClouds",
                                               "BidAnnoInk",    "BidNamedViews",       "BidHotLinks"};

    // The rescaled text of a row that must be rescaled (REQUIRE: the row was accepted).
    string rescaled(const string &table, const string &stored, double factor)
    {
        const PositionRescale result = rescale_position_text(table, stored, factor);
        if (result.kind != PositionRescaleKind::Rescaled)
            FAIL_TEST("expected Rescaled for " + table + " '" + stored + "', got " + describe(result));
        return result.text;
    }
}

// ---- the oracle: every row of kPositionRows is the real Windows result --------------------------------------------------------------------

// page_operations.py:154-217 on every row (Empty = no update and no warning, NotNumeric = no update and one warning, Rescaled = the text Windows writes).
// The rows cover the three three-decimal tables, the 13 annotation tables, ties (0.0625, 0.1875), -0, big values, entities and blanks inside the text.
TEST(SetScale_PositionText, EveryOracleRowGivesTheWindowsText)
{
    int rows = 0;
    for (const scale_oracle::PositionRow &row : scale_oracle::kPositionRows)
    {
        SCOPED_CONTEXT(string(row.table) + " '" + row.stored + "' x " + ost_test::detail::exact_float(row.factor));
        const PositionRescale got = rescale_position_text(row.table, row.stored, row.factor);
        if (row.kind == 0)
            CHECK_STR_EQ(kind_name(got.kind), "Empty");
        else if (row.kind == 1)
            CHECK_STR_EQ(kind_name(got.kind), "NotNumeric");
        else
        {
            CHECK_STR_EQ(kind_name(got.kind), "Rescaled");
            CHECK_STR_EQ(got.text, row.expected);
        }
        ++rows;
    }
    // The table is not silently empty or shortened: 3 three-decimal tables x 33 cases + 13 annotation tables x 20 cases = 359 rows. (Review fix: the guard was 600, a miscount
    // of the 639 LINES of the generated header; every row passed, the guard itself could never be met.)
    CHECK(rows >= 359);
}

// ---- ported Windows tests (tests/infrastructure/mdb/components/test_page_operations.py) ------------------------------------------------

// :974 test_page_scale_rescale_writes_text_payload_for_text_position_tables and :985 test_page_scale_rescale_writes_binary_payload_for_binary_position_tables.
// On iOS both are TEXT: BidTexts keeps the exact repr form, BidTakeoffs the three-decimal form.
TEST(SetScale_PositionText, TextAndBinaryTablesWriteTheirForms)
{
    const string stored = ost_domain::encode_position({1.0, 2.0, 3.0, 4.0}); // "1.0;2.0;3.0;4.0\n", what the Windows tests store
    CHECK_STR_EQ(rescaled("BidTexts", stored, 0.5), "0.5;1.0;1.5;2.0\n");
    CHECK_STR_EQ(rescaled("BidTakeoffs", stored, 0.5), "0.5;1;1.5;2\n");
}

// :1058 test_scale_preserves_annotation_rotation: BidTexts / BidAnnotationRects / BidAnnotationOvals with [1, 2, 3, 4, 0.5] x 2 -> [2, 4, 6, 8, 0.5].
TEST(SetScale_PositionText, ScalePreservesAnnotationRotation)
{
    for (const char *table : {"BidTexts", "BidAnnotationRects", "BidAnnotationOvals"})
    {
        SCOPED_CONTEXT(string("table ") + table);
        const string text = rescaled(table, ost_domain::encode_position({1, 2, 3, 4, 0.5}), 2.0);
        CHECK_EQ(parse_position(text), vector<double>({2, 4, 6, 8, 0.5}));
        CHECK_STR_EQ(text, "2.0;4.0;6.0;8.0;0.5\n");
    }
}

// :1080 test_all_annotation_rotation_fields_survive_scale: every annotation table; ink [0.123456789, 100, 200, 300, 400], the others [100, 200, 300, 400, 0.123456789],
// x 2: every value is doubled except the rotation slot (index 0 for ink, index 4 otherwise), exactly.
TEST(SetScale_PositionText, AllAnnotationRotationFieldsSurviveScale)
{
    for (const char *table : kAnnotationTables)
    {
        SCOPED_CONTEXT(string("table ") + table);
        const bool ink = string(table) == "BidAnnoInk";
        const vector<double> position = ink ? vector<double>({0.123456789, 100, 200, 300, 400}) : vector<double>({100, 200, 300, 400, 0.123456789});
        string payload;
        for (size_t i = 0; i < position.size(); ++i)
            payload += (i ? ";" : "") + python_float_repr(position[i]);
        payload += "\n";
        const size_t rotation = ink ? 0 : 4;
        vector<double> expected = position;
        for (size_t i = 0; i < expected.size(); ++i)
            if (i != rotation)
                expected[i] *= 2;
        CHECK_EQ(parse_position(rescaled(table, payload, 2.0)), expected);
    }
}

// :1114 test_large_coordinate_round_trip_retains_three_decimal_precision: ten pairs of x2 then x0.5 keep "123456.789;987654.321" exactly.
TEST(SetScale_PositionText, LargeCoordinateRoundTripRetainsThreeDecimalPrecision)
{
    string payload = "123456.789;987654.321\n";
    for (int i = 0; i < 10; ++i)
        for (const double factor : {2.0, 0.5})
            payload = rescaled("BidTakeoffs", payload, factor);
    CHECK_EQ(parse_position(payload), vector<double>({123456.789, 987654.321}));
    CHECK_STR_EQ(payload, "123456.789;987654.321\n");
}

// :1127 test_curved_takeoff_offset_is_a_length_not_a_trailing_angle: a seven-value takeoff has no rotation slot, every value is doubled.
TEST(SetScale_PositionText, CurvedTakeoffOffsetIsALengthNotATrailingAngle)
{
    CHECK_EQ(parse_position(rescaled("BidTakeoffs", "1;2;3;4;5;6;7\n", 2.0)), vector<double>({2, 4, 6, 8, 10, 12, 14}));
}

// :1138 test_unverified_position_tables_leave_empty_payloads_untouched: BidComments / BidTypGroupViews with an empty payload: no update, no warning.
// (A NULL cell and an empty blob are the empty text here.)
TEST(SetScale_PositionText, EmptyPayloadsAreUntouched)
{
    for (const char *table : {"BidComments", "BidTypGroupViews", "BidTakeoffs", "BidTexts", "BidAnnoInk"})
    {
        SCOPED_CONTEXT(string("table ") + table);
        const PositionRescale got = rescale_position_text(table, "", 2.0);
        CHECK_STR_EQ(kind_name(got.kind), "Empty");
        CHECK_STR_EQ(got.text, "");
    }
}

// :1152 test_unverified_position_tables_do_not_rewrite_unparseable_payloads ("not-a-numeric-position") and :1176 test_page_scale_rescale_skips_unparseable_position_payload
// (the three private-use characters U+A0E3 U+E2B8 U+B7B8, UTF-8 here): the row is skipped (the store logs the warning, left as it is).
TEST(SetScale_PositionText, UnparseablePayloadsAreSkipped)
{
    const struct
    {
        const char *table;
        const char *payload;
    } rows[] = {
        {"BidComments", "not-a-numeric-position"},
        {"BidTypGroupViews", "not-a-numeric-position"},
        {"BidTexts", "\xEA\x83\xA3\xEE\x8A\xB8\xEB\x9E\xB8"},
    };
    for (const auto &row : rows)
    {
        SCOPED_CONTEXT(string(row.table));
        const PositionRescale got = rescale_position_text(row.table, row.payload, 2.0);
        CHECK_STR_EQ(kind_name(got.kind), "NotNumeric");
        CHECK_STR_EQ(got.text, "");
    }
}

// tests/integration/persistence/test_page_scale_geometry.py:63 test_repeated_round_trips_obey_storage_precision: one step of x0.3 on "1234.567;2345.678" is
// "370.37;703.703\n"; 20 pairs of (x0.3, x1/0.3) on a takeoff, a legend dX and an overlay rectangle stay within 0.0022, 0.0022 and 0.0000022 of 1234.567.
TEST(SetScale_PositionText, RepeatedRoundTripsObeyStoragePrecision)
{
    CHECK_STR_EQ(rescaled("BidTakeoffs", "1234.567;2345.678\n", 0.3), "370.37;703.703\n");
    string numeric = "1234.567;2345.678\n";
    string legend = "<Legends dX=\"1234.567\" dY=\"2345.678\"/>";
    string overlay = "1234.567,2345.678,100.123456,200.123456";
    for (int i = 0; i < 20; ++i)
    {
        for (const double factor : {0.3, 1 / 0.3})
        {
            numeric = rescaled("BidTakeoffs", numeric, factor);
            string scaled_legend;
            REQUIRE(rescale_legend_xml(legend, factor, scaled_legend));
            legend = scaled_legend;
            const OverlayRescale rect = rescale_overlay_rect(overlay, factor);
            REQUIRE(rect.kind == OverlayRescaleKind::Rescaled);
            overlay = rect.rect_text;
        }
    }
    const vector<double> final_numeric = parse_position(numeric);
    REQUIRE_EQ(final_numeric.size(), static_cast<size_t>(2));
    CHECK_NEAR(final_numeric[0], 1234.567, 0.0022);
    const size_t start = legend.find("dX=\"") + 4;
    double legend_x = 0.0;
    REQUIRE(parse_python_float(legend.substr(start, legend.find('"', start) - start), legend_x));
    CHECK_NEAR(legend_x, 1234.567, 0.0022);
    OverlayRect parsed;
    REQUIRE(parse_rect_storage(overlay, parsed));
    CHECK_NEAR(parsed.x, 1234.567, 0.0000022);
}

// ---- the three-decimal encoder ----------------------------------------------------------------------------------------------------------

// serialization.py:18-28: {0.5, 1, 1.5, 2} -> "0.5;1;1.5;2\n"; the trailing zeros and a trailing dot are removed; an exact tie rounds to the even digit;
// a negative zero keeps its sign; non-finite values are written as Python writes them (the store never writes those: see the refusal tests below).
TEST(SetScale_PositionText, ThreeDecimalEncoderTrimsLikeWindows)
{
    CHECK_STR_EQ(encode_position_3dp({0.5, 1.0, 1.5, 2.0}), "0.5;1;1.5;2\n");
    CHECK_STR_EQ(encode_position_3dp({}), "\n");
    CHECK_STR_EQ(encode_position_3dp({370.3701, 703.7034}), "370.37;703.703\n");
    CHECK_STR_EQ(encode_position_3dp({0.0625, 0.1875, 0.3125, 0.4375}), "0.062;0.188;0.312;0.438\n"); // exact ties go to the even digit
    CHECK_STR_EQ(encode_position_3dp({-0.0004, 0.0004}), "-0;0\n");
    CHECK_STR_EQ(encode_position_3dp({1e22}), "10000000000000000000000\n");
}

// ---- iOS-only data safety: hardening of SS-2 (Windows writes "inf" / "nan" text and has no overflow guard: serialization.py:18-28) --------

// A stored or a scaled coordinate that is not finite refuses the change; the loader refuses a non-finite takeoff (ProjectStore.hpp:66), so writing one would make
// the bid unloadable. Pinned for every kind of table.
TEST(SetScale_PositionTextSafety, ANonFiniteStoredOrScaledCoordinateIsRefused)
{
    for (const char *table : {"BidTakeoffs", "BidTexts", "BidAnnoInk", "BidComments"})
    {
        SCOPED_CONTEXT(string("table ") + table);
        CHECK_STR_EQ(kind_name(rescale_position_text(table, "1e308;1", 10.0).kind), "NotFinite"); // overflows to infinity
        CHECK_STR_EQ(kind_name(rescale_position_text(table, "inf;1", 2.0).kind), "NotFinite");    // stored infinity (Python float("inf") parses)
        CHECK_STR_EQ(kind_name(rescale_position_text(table, "1;-inf", 2.0).kind), "NotFinite");
        CHECK_STR_EQ(kind_name(rescale_position_text(table, "nan;1", 2.0).kind), "NotFinite");
        CHECK_STR_EQ(kind_name(rescale_position_text(table, "1e400;1", 2.0).kind), "NotFinite");  // 1e400 reads as infinity
    }
    // the rotation slot is kept, not multiplied, so a huge rotation value does not overflow: [1, 2, 3, 4, 1e308] x 10 on a text keeps 1e308
    CHECK_STR_EQ(kind_name(rescale_position_text("BidTexts", "1;2;3;4;1e308", 10.0).kind), "Rescaled");
    // but a stored rotation slot that is itself not finite is a stored non-finite value: refused
    CHECK_STR_EQ(kind_name(rescale_position_text("BidTexts", "1;2;3;4;nan", 2.0).kind), "NotFinite");
    // a control run: the same rows with a sane value are rescaled
    CHECK_STR_EQ(kind_name(rescale_position_text("BidTakeoffs", "1e307;1", 10.0).kind), "Rescaled");
}

// A non-zero coordinate that underflows to exactly 0.0 by the multiplication refuses the change (data destroyed); a zero coordinate stays zero; a value that only
// ROUNDS to 0 at three decimals is not an underflow (Windows writes "0" / "-0" for it: the oracle rows).
TEST(SetScale_PositionTextSafety, ANonZeroCoordinateThatUnderflowsToZeroIsRefused)
{
    for (const char *table : {"BidTakeoffs", "BidTexts", "BidAnnotationPolygons"})
    {
        SCOPED_CONTEXT(string("table ") + table);
        CHECK_STR_EQ(kind_name(rescale_position_text(table, "5e-324;1", 0.5).kind), "Underflow");
        CHECK_STR_EQ(kind_name(rescale_position_text(table, "1;1e-300", 1e-300).kind), "Underflow");
        CHECK_STR_EQ(kind_name(rescale_position_text(table, "0;0", 0.5).kind), "Rescaled");
        CHECK_STR_EQ(kind_name(rescale_position_text(table, "0.0004;5", 1.0).kind), "Rescaled"); // rounds to 0 at three decimals: written, not refused
    }
    CHECK_STR_EQ(rescaled("BidTakeoffs", "0.0004;5", 1.0), "0;5\n");
}

// The factor must be finite and > 0: 0, a negative value, NaN and an infinity are BadFactor, and nothing is parsed (a non-numeric row is BadFactor too, not skipped).
TEST(SetScale_PositionTextSafety, ABadFactorIsRefusedBeforeAnythingIsRead)
{
    const double bad[] = {0.0, -0.0, -1.0, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()};
    for (const double factor : bad)
    {
        SCOPED_CONTEXT("factor " + ost_test::detail::exact_float(factor));
        CHECK_STR_EQ(kind_name(rescale_position_text("BidTakeoffs", "1;2;3;4", factor).kind), "BadFactor");
        CHECK_STR_EQ(kind_name(rescale_position_text("BidTexts", "1;2;3;4;0.5", factor).kind), "BadFactor");
        CHECK_STR_EQ(kind_name(rescale_position_text("BidTakeoffs", "not-numeric", factor).kind), "BadFactor");
    }
    CHECK_STR_EQ(kind_name(rescale_position_text("BidTakeoffs", "1;2;3;4", 1.0).kind), "Rescaled"); // the control
}

// The text is never partly rewritten: a refusal returns an empty text, and a skipped row returns an empty text (the caller leaves the stored text alone).
TEST(SetScale_PositionTextSafety, ARefusedOrSkippedRowCarriesNoText)
{
    CHECK_STR_EQ(rescale_position_text("BidTakeoffs", "1e308;1", 10.0).text, "");
    CHECK_STR_EQ(rescale_position_text("BidTakeoffs", "5e-324;1", 0.5).text, "");
    CHECK_STR_EQ(rescale_position_text("BidTakeoffs", "x;1", 2.0).text, "");
    CHECK_STR_EQ(rescale_position_text("BidTakeoffs", "", 2.0).text, "");
    CHECK_STR_EQ(rescaled("BidTakeoffs", "1;2", 2.0), "2;4\n"); // the control: a good row does carry its text
}

// SS-13 (parity, register): a row whose Position is not numeric is skipped while the scale changes anyway; ANY bad token makes the whole row unreadable (Python
// parse_position), including a decimal comma, a hex number and a blank-only text. The numbers inside a good row are read like Python float().
TEST(SetScale_PositionText, AnyBadTokenMakesTheRowUnreadableAndGoodTokensReadLikePython)
{
    for (const char *stored : {"abc;1", "1;2;x", "1,5;2", "0x10;2", "1 2;3", ";", "  ", "1;2;3;"})
    {
        SCOPED_CONTEXT(string("stored '") + stored + "'");
        const PositionRescale got = rescale_position_text("BidTakeoffs", stored, 2.0);
        if (string(stored) == "1;2;3;")
            CHECK_STR_EQ(kind_name(got.kind), "Rescaled"); // a trailing ';' only leaves a blank token, which parse_position drops
        else
            CHECK_STR_EQ(kind_name(got.kind), "NotNumeric");
    }
    CHECK_STR_EQ(rescaled("BidTakeoffs", " 1 ; 2 ", 2.0), "2;4\n");
    CHECK_STR_EQ(rescaled("BidTakeoffs", "1_0;2", 2.0), "20;4\n");
    CHECK_STR_EQ(rescaled("BidTakeoffs", "1;;2", 2.0), "2;4\n");
}
