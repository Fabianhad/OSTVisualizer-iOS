// Set Scale and the undo history (plan 4.7, N-M1-06, 5.4, tests-first): an undo / redo entry stores the positions it holds in the units of the page scale it was captured under,
// and a replay rescales them to the scale that is current NOW. Without this a Delete undone after a scale change would write old-unit positions into a rescaled page (data
// corruption). The Objective-C part (UndoRedoController, the two optional delegate methods, the page lookup) is device-only (check M1-17); what the host proves here is the Core
// arithmetic, ost_domain::rescale_entry_positions, and its use through the real UndoHistory.
// Windows source of truth: plan_view_action_handler.py:1667-1828 (_page_scale_for_page_uid, _capture_*_scales, _positions_for_current_takeoff_scales, _takeoff_specs_for_current_scales)
// and the pinned scenario tests/presentation/handlers/test_plan_view_action_handler.py:3849. Where iOS differs on purpose: a MISSING capture means factor 1 (nothing is rescaled), not the
// Windows "(1.0, 1.0) ratio 1", which would turn the positions into the current ratio (48 on a 1/4 inch sheet); a factor that is not finite or not > 0, and a result that is not finite
// or whose non-zero coordinate underflows to 0, REFUSE the replay (the entry stays where it is, UND-02).
#include "TestHarness.hpp"

#include "Domain/PageScaleTransform.hpp"
#include "Domain/UndoHistory.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace ost_domain;
using std::optional;
using std::vector;

namespace
{
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    constexpr double kNotANumber = std::numeric_limits<double>::quiet_NaN();

    optional<PageScale> pair(double sf1, double sf2)
    {
        return PageScale{sf1, sf2};
    }
}

// tests/presentation/handlers/test_plan_view_action_handler.py:3849 test_takeoff_delete_undo_after_page_scale_change_uses_current_scale: the takeoff [0, 0, 96, 0] on a page at
// (0.125, 12) is deleted (the entry captures (0.125, 12)), the page becomes (0.1875, 12), and the undo re-inserts it as [0, 0, 64, 0] exactly (96 x 64 / 96).
TEST(SetScale_Undo, ADeleteUndoneAfterAScaleChangeUsesTheCurrentScale)
{
    const optional<vector<double>> replayed = rescale_entry_positions({0.0, 0.0, 96.0, 0.0}, pair(0.125, 12.0), PageScale{0.1875, 12.0});
    REQUIRE(replayed.has_value());
    CHECK_EQ(*replayed, vector<double>({0.0, 0.0, 64.0, 0.0}));
}

// S17: Place, Delete, Move and MoveGroup entries all hold positions captured at (0.125, 12); after the page goes to (0.1875, 12) the replay ends at x 2/3 of them. The expected
// values are the real Windows rescale_position_between_page_scales (python -B -I, page_scale_transform.py:43-51), exact doubles.
TEST(SetScale_Undo, EveryKindOfEntryReplaysAtTwoThirdsAfterTheScaleChange)
{
    const struct
    {
        vector<double> positions;
        double captured_sf1, captured_sf2, current_sf1, current_sf2;
        vector<double> expected;
    } rows[] = {
        {{0.0, 0.0, 96.0, 0.0}, 0.125, 12.0, 0.1875, 12.0, {0.0, 0.0, 64.0, 0.0}},
        {{96.0, 192.0, 288.0, 384.0}, 0.125, 12.0, 0.1875, 12.0, {64.0, 128.0, 192.0, 256.0}},
        {{10.5, 20.25, 0.1, 7.0}, 0.125, 12.0, 0.1875, 12.0, {7.0, 13.5, 0.06666666666666667, 4.666666666666666}},
        {{1.0, 2.0, 3.0, 4.0}, 0.25, 12.0, 0.5, 12.0, {0.5, 1.0, 1.5, 2.0}},
        {{1.0, 2.0}, 1.0, 100.0, 1.0, 30.0, {0.3, 0.6}},
        {{5.0, 6.0, 7.0}, 1.0, 1.0, 1.0, 3.0, {15.0, 18.0, 21.0}},
        {{123456.789, 987654.321}, 1.0, 100.0, 1.0, 30.0, {37037.0367, 296296.2963}},
        {{0.0, 0.0, 64.0, 0.0}, 0.1875, 12.0, 0.125, 12.0, {0.0, 0.0, 96.0, 0.0}},
    };
    for (const auto &row : rows)
    {
        SCOPED_CONTEXT("captured (" + ost_test::detail::exact_float(row.captured_sf1) + ", " + ost_test::detail::exact_float(row.captured_sf2) + ") current (" +
                       ost_test::detail::exact_float(row.current_sf1) + ", " + ost_test::detail::exact_float(row.current_sf2) + ")");
        const optional<vector<double>> got = rescale_entry_positions(row.positions, pair(row.captured_sf1, row.captured_sf2), PageScale{row.current_sf1, row.current_sf2});
        REQUIRE(got.has_value());
        REQUIRE_EQ(got->size(), row.expected.size());
        for (size_t i = 0; i < got->size(); ++i)
            CHECK_EQ((*got)[i], row.expected[i]); // exact: the replay does not round (no three-decimal re-quantisation)
    }
}

// An entry that captured nothing (a delegate without the methods, or a page unknown at creation time) replays with factor 1: the positions come back as they are, whatever the current
// scale is. NOT the captured pair (1, 1): the Windows rule would multiply by the CURRENT ratio (48 on a 1/4 inch sheet), writing positions 48 times too large.
TEST(SetScale_Undo, ANonCapturedEntryReplaysUnchangedNotAtTheCurrentRatio_Deliberate)
{
    const vector<double> positions = {1.0, 2.0, 3.0, 4.0};
    const optional<vector<double>> none = rescale_entry_positions(positions, std::nullopt, PageScale{0.25, 12.0});
    REQUIRE(none.has_value());
    CHECK_EQ(*none, positions);
    // the Windows lenient rule with the pair (1, 1) as "captured" would give 48: the difference this decision avoids
    CHECK_EQ(position_rescale_factor_between_page_scales(pair(1.0, 1.0), pair(0.25, 12.0)), 48.0);
    const optional<vector<double>> as_one_one = rescale_entry_positions(positions, pair(1.0, 1.0), PageScale{0.25, 12.0});
    REQUIRE(as_one_one.has_value());
    CHECK_EQ(*as_one_one, vector<double>({48.0, 96.0, 144.0, 192.0})); // a captured (1, 1) IS a capture: it rescales (only a missing capture does not)
}

// An unchanged scale (factor 1, or within 1e-9 of it) returns the positions untouched, an empty position stays empty.
TEST(SetScale_Undo, AnUnchangedScaleReturnsThePositionsUntouched)
{
    const vector<double> positions = {0.123456789012345, -7.0, 1e22};
    for (const double current_sf2 : {12.0, 12.0 * (1 + 1e-12)})
    {
        const optional<vector<double>> got = rescale_entry_positions(positions, pair(0.125, 12.0), PageScale{0.125, current_sf2});
        REQUIRE(got.has_value());
        CHECK_EQ(*got, positions);
    }
    const optional<vector<double>> empty = rescale_entry_positions({}, pair(0.125, 12.0), PageScale{0.1875, 12.0});
    REQUIRE(empty.has_value());
    CHECK(empty->empty());
}

// The entry keeps its positions in the captured units, so SEVERAL scale changes between the capture and the replay need no history: A -> B -> C replays at once from A to C, and the
// result equals the step by step result.
TEST(SetScale_Undo, SeveralScaleChangesBetweenCaptureAndReplayGoStraightToTheCurrentScale)
{
    const PageScale a{0.125, 12.0};
    const PageScale b{0.1875, 12.0};
    const PageScale c{0.25, 12.0};
    const vector<double> positions = {96.0, 192.0};
    const optional<vector<double>> direct = rescale_entry_positions(positions, a, c);
    const optional<vector<double>> step_one = rescale_entry_positions(positions, a, b);
    REQUIRE(direct.has_value());
    REQUIRE(step_one.has_value());
    const optional<vector<double>> step_two = rescale_entry_positions(*step_one, b, c);
    REQUIRE(step_two.has_value());
    CHECK_EQ(*direct, vector<double>({48.0, 96.0}));
    CHECK_EQ(*step_two, *direct);
}

// A replay is refused (nullopt) when the factor is not finite or not > 0: the current page ratio is 0 (a missing sf2), negative, NaN or infinite, or the ratio of ratios overflows or
// underflows. Windows would multiply by it (0 collapses every coordinate, NaN / inf write garbage); iOS leaves the entry where it is (UND-02).
TEST(SetScale_Undo, AFactorThatIsNotFiniteAndPositiveRefusesTheReplay)
{
    const vector<double> positions = {1.0, 2.0};
    const struct
    {
        PageScale captured;
        PageScale current;
    } bad[] = {
        {{0.125, 12.0}, {1.0, 0.0}},           // the current ratio is 0: the factor 0
        {{0.125, 12.0}, {1.0, -12.0}},         // negative
        {{0.125, 12.0}, {kNotANumber, 12.0}},  // NaN
        {{0.125, 12.0}, {1.0, kInfinity}},     // infinite
        {{1.0, 1e-300}, {1.0, 1e300}},         // the factor overflows to infinity
        {{1.0, 1e300}, {1.0, 1e-300}},         // the factor underflows to 0
        {{1.0, -12.0}, {1.0, 12.0}},           // a negative captured ratio: the factor is -1
    };
    for (const auto &row : bad)
    {
        SCOPED_CONTEXT("captured (" + ost_test::detail::exact_float(row.captured.sf1) + ", " + ost_test::detail::exact_float(row.captured.sf2) + ") current (" +
                       ost_test::detail::exact_float(row.current.sf1) + ", " + ost_test::detail::exact_float(row.current.sf2) + ")");
        CHECK_FALSE(rescale_entry_positions(positions, row.captured, row.current).has_value());
    }
    // the lenient rule's identity cases are NOT refusals: a captured sf1 of 0 has ratio 1, a captured sf2 of 0 (ratio 0) gives the factor 1
    const optional<vector<double>> zero_sf1 = rescale_entry_positions(positions, pair(0.0, 12.0), PageScale{0.25, 12.0});
    REQUIRE(zero_sf1.has_value());
    CHECK_EQ(*zero_sf1, vector<double>({48.0, 96.0})); // Windows (0, 12) -> (0.25, 12) = 48 (the pinned oracle)
    const optional<vector<double>> zero_sf2 = rescale_entry_positions(positions, pair(1.0, 0.0), PageScale{0.25, 12.0});
    REQUIRE(zero_sf2.has_value());
    CHECK_EQ(*zero_sf2, positions);
}

// A result that is not finite (a coordinate that overflows) or a non-zero coordinate that underflows to 0 refuses the replay (hardening, same rule as the store): the entry stays and
// nothing is written. A zero coordinate stays zero.
TEST(SetScale_Undo, AResultThatOverflowsOrUnderflowsRefusesTheReplay_Hardening)
{
    CHECK_FALSE(rescale_entry_positions({1e308, 1.0}, pair(1.0, 1.0), PageScale{1.0, 10.0}).has_value());     // x 10 overflows
    CHECK_FALSE(rescale_entry_positions({kInfinity, 1.0}, pair(1.0, 1.0), PageScale{1.0, 10.0}).has_value());  // a stored infinity
    CHECK_FALSE(rescale_entry_positions({kNotANumber, 1.0}, pair(1.0, 1.0), PageScale{1.0, 10.0}).has_value());
    CHECK_FALSE(rescale_entry_positions({5e-324, 1.0}, pair(1.0, 2.0), PageScale{1.0, 1.0}).has_value());       // x 0.5 underflows to 0
    const optional<vector<double>> zeros = rescale_entry_positions({0.0, -0.0, 4.0}, pair(1.0, 2.0), PageScale{1.0, 1.0});
    REQUIRE(zeros.has_value());
    CHECK_EQ(*zeros, vector<double>({0.0, 0.0, 2.0}));
    const optional<vector<double>> big = rescale_entry_positions({1e307, 1.0}, pair(1.0, 1.0), PageScale{1.0, 10.0}); // x 10 is still finite
    REQUIRE(big.has_value());
    CHECK_EQ((*big)[0], 1e307 * 10.0);
}

// ---- through the real UndoHistory (UND-01, UND-02) -------------------------------------------------------------------------------------------------------

namespace
{
    // The shape of the plan's file-local UndoEntry: the positions of one takeoff in the units of `captured`.
    struct Entry
    {
        vector<double> positions;
        optional<PageScale> captured;
    };

    using History = UndoHistory<Entry>;
}

// A history of two entries captured at (0.125, 12); the page is changed to (0.1875, 12) and then to (0.25, 12). Undo replays the newest entry at the CURRENT scale (0.25), redo
// replays it again at the then current scale, and the older entry (a Move, say) goes to the same current scale after the page changed once more: every replay starts from the
// captured units, never from the previous replay.
TEST(SetScale_Undo, UndoAndRedoReplayAtTheScaleThatIsCurrentAtEachMoment)
{
    History history;
    PageScale current{0.125, 12.0};
    history.push(Entry{{96.0, 192.0}, pair(0.125, 12.0)}); // an older action
    history.push(Entry{{0.0, 0.0, 96.0, 0.0}, pair(0.125, 12.0)}); // the newest: a delete of the takeoff
    current = PageScale{0.1875, 12.0};
    current = PageScale{0.25, 12.0};
    vector<double> applied;
    REQUIRE(history.undo([&](Entry &entry) {
        const optional<vector<double>> got = rescale_entry_positions(entry.positions, entry.captured, current);
        if (!got)
            return false;
        applied = *got;
        return true; // stored: the entry moves to the redo stack
    }));
    CHECK_EQ(applied, vector<double>({0.0, 0.0, 48.0, 0.0})); // x 0.5, not x 2/3 and not 96
    current = PageScale{0.375, 12.0};                           // the page changes again while the entry waits on the redo stack
    REQUIRE(history.redo([&](Entry &entry) {
        const optional<vector<double>> got = rescale_entry_positions(entry.positions, entry.captured, current);
        if (!got)
            return false;
        applied = *got;
        return true;
    }));
    CHECK_EQ(applied, vector<double>({0.0, 0.0, 32.0, 0.0})); // 96 x 32 / 96: from the captured units to 0.375
    current = PageScale{0.25, 12.0};
    REQUIRE(history.undo([&](Entry &entry) {
        const optional<vector<double>> got = rescale_entry_positions(entry.positions, entry.captured, current);
        applied = got ? *got : vector<double>();
        return got.has_value();
    }));
    CHECK_EQ(applied, vector<double>({0.0, 0.0, 48.0, 0.0}));
    REQUIRE(history.undo([&](Entry &entry) { // the older entry
        const optional<vector<double>> got = rescale_entry_positions(entry.positions, entry.captured, current);
        applied = got ? *got : vector<double>();
        return got.has_value();
    }));
    CHECK_EQ(applied, vector<double>({48.0, 96.0}));
}

// UND-02: a replay that is refused (the helper answers nullopt) does not advance the history: the entry stays on the undo stack and a later undo retries it (here after the page has a
// usable scale again).
TEST(SetScale_Undo, ARefusedReplayDoesNotAdvanceTheHistory)
{
    History history;
    history.push(Entry{{96.0, 192.0}, pair(0.125, 12.0)});
    PageScale current{1.0, 0.0}; // an unusable current scale
    vector<double> applied;
    auto replay = [&](Entry &entry) {
        const optional<vector<double>> got = rescale_entry_positions(entry.positions, entry.captured, current);
        if (!got)
            return false;
        applied = *got;
        return true;
    };
    CHECK_FALSE(history.undo(replay));
    CHECK_EQ(history.undo_count(), static_cast<size_t>(1));
    CHECK_EQ(history.redo_count(), static_cast<size_t>(0));
    CHECK(applied.empty());
    current = PageScale{0.25, 12.0}; // fixed: the retry runs
    CHECK(history.undo(replay));
    CHECK_EQ(history.undo_count(), static_cast<size_t>(0));
    CHECK_EQ(history.redo_count(), static_cast<size_t>(1));
    CHECK_EQ(applied, vector<double>({48.0, 96.0}));
}
