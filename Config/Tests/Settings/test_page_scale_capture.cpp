// Set Scale and the undo history, the capture (plan 4.7, 5.4, N-M1-06; tests first): which page and which scale pair an undo / redo entry stores for a takeoff, and what a replay
// does with the stored positions when nothing was captured or the captured page is gone. The Objective-C part (UndoRedoController and the two optional delegate methods
// undoRedoPageScaleForTakeoff:pageUID: / undoRedoPageScaleForPage:, implemented in ViewController+PageScale.mm) is device-only (check M1-17); the host proves the lookup and the rules
// here, and the arithmetic in test_page_scale_undo.cpp.
// Windows source of truth: plan_view_action_handler.py:1667-1673 _page_scale_for_page_uid -> (float(sf1 or 1.0), float(sf2 or 1.0)); :1706-1725 _capture_item_page_scales (keyed by the page
// uid of the takeoff); :1788 the lookup by page uid at replay; the pinned scenario tests/presentation/handlers/test_plan_view_action_handler.py:3849 (a takeoff [0, 0, 96, 0] deleted on a
// page at (0.125, 12), the scale becomes (0.1875, 12), the undo re-inserts [0, 0, 64, 0]).
// Where iOS differs on purpose (pinned below): a page the project does not have is UNKNOWN, not the pair (1.0, 1.0) that Windows answers; unknown at creation = nothing captured,
// unknown at replay = the undo / redo is refused.
#include "TestHarness.hpp"

#include "Data/ProjectData.hpp"
#include "Domain/PageScaleCapture.hpp"
#include "Domain/PageScaleTransform.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace ost_domain;
using std::optional;
using std::string;
using std::vector;

namespace
{
    constexpr double kNotANumber = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInfinity = std::numeric_limits<double>::infinity();

    optional<PageScale> pair(double sf1, double sf2)
    {
        return PageScale{sf1, sf2};
    }

    void add_page(ost_data::ProjectData &project, const string &uid, double sf1, double sf2)
    {
        Page page;
        page.uid = uid;
        page.name = "Page " + uid;
        page.scale_factor1 = sf1;
        page.scale_factor2 = sf2;
        project.pages[uid] = page;
        project.page_order.push_back(uid);
    }

    void add_takeoff(ost_data::ProjectData &project, const string &uid, const string &page_uid, const vector<double> &position)
    {
        Takeoff takeoff;
        takeoff.uid = uid;
        takeoff.page_uid = page_uid;
        takeoff.condition_uid = "1";
        takeoff.position = position;
        project.takeoffs.push_back(takeoff);
    }

    ost_data::ProjectData project_with_two_pages()
    {
        ost_data::ProjectData project;
        project.bid.uid = "7";
        add_page(project, "10", 0.125, 12.0);
        add_page(project, "11", 1.0, 100.0);
        add_takeoff(project, "100", "10", {0.0, 0.0, 96.0, 0.0});
        add_takeoff(project, "101", "11", {5.0, 5.0});
        add_takeoff(project, "102", "10", {1.0, 2.0});
        return project;
    }
}

// ---- the pair of a page (Windows _page_scale_for_page_uid) ------------------------------------------------------------------------------------------------------------------------

// plan_view_action_handler.py:1667-1673: the pair of the page as stored.
TEST(SetScale_Capture, ThePairOfAPageIsItsStoredPair)
{
    const auto project = project_with_two_pages();
    const optional<PageScale> ten = page_scale_of_page(project, "10");
    REQUIRE(ten.has_value());
    CHECK_EQ(ten->sf1, 0.125);
    CHECK_EQ(ten->sf2, 12.0);
    const optional<PageScale> eleven = page_scale_of_page(project, "11");
    REQUIRE(eleven.has_value());
    CHECK_EQ(eleven->sf1, 1.0);
    CHECK_EQ(eleven->sf2, 100.0);
}

// `float(sf1 or 1.0)`: a stored 0 (and -0) reads 1.0 for each factor on its own; a NaN, an infinity and a negative value stay (the replay arithmetic refuses what it cannot use).
TEST(SetScale_Capture, AZeroFactorReadsOneAndNothingElseIsCoalesced)
{
    ost_data::ProjectData project;
    add_page(project, "1", 0.0, 12.0);
    add_page(project, "2", 0.25, 0.0);
    add_page(project, "3", -0.0, -0.0);
    add_page(project, "4", kNotANumber, 12.0);
    add_page(project, "5", -0.125, 12.0);
    add_page(project, "6", 0.125, kInfinity);

    for (const char *uid : {"1", "2", "3", "4", "5", "6"})
        REQUIRE(page_scale_of_page(project, uid).has_value());
    const PageScale p1 = *page_scale_of_page(project, "1");
    const PageScale p2 = *page_scale_of_page(project, "2");
    const PageScale p3 = *page_scale_of_page(project, "3");
    const PageScale p4 = *page_scale_of_page(project, "4");
    const PageScale p5 = *page_scale_of_page(project, "5");
    const PageScale p6 = *page_scale_of_page(project, "6");
    CHECK_EQ(p1.sf1, 1.0);
    CHECK_EQ(p1.sf2, 12.0);
    CHECK_EQ(p2.sf1, 0.25);
    CHECK_EQ(p2.sf2, 1.0);
    CHECK_EQ(p3.sf1, 1.0);
    CHECK_EQ(p3.sf2, 1.0);
    CHECK(std::isnan(p4.sf1));
    CHECK_EQ(p4.sf2, 12.0);
    CHECK_EQ(p5.sf1, -0.125);
    CHECK(std::isinf(p6.sf2));
}

// Deliberate difference from Windows (which answers (1.0, 1.0) for an unknown page): a page that is not in the project, and an empty uid, are unknown.
TEST(SetScale_Capture, AnUnknownPageIsUnknownNotTheIdentityPair_DeliberateDifferenceFromWindows)
{
    const auto project = project_with_two_pages();
    CHECK_FALSE(page_scale_of_page(project, "99").has_value());
    CHECK_FALSE(page_scale_of_page(project, "").has_value());
    CHECK_FALSE(page_scale_of_page(ost_data::ProjectData{}, "10").has_value());
}

// ---- the capture of a takeoff (the creation-time delegate method) -----------------------------------------------------------------------------------------------------------------

// The takeoff gives its page uid, the page gives the pair: _capture_item_page_scales keys the capture by the page of the takeoff.
TEST(SetScale_Capture, ATakeoffCapturesItsPageUidAndThePairOfThatPage)
{
    const auto project = project_with_two_pages();
    const auto first = page_scale_of_takeoff(project, "100");
    REQUIRE(first.has_value());
    CHECK_STR_EQ(first->page_uid, "10");
    CHECK_EQ(first->scale.sf1, 0.125);
    CHECK_EQ(first->scale.sf2, 12.0);
    const auto other = page_scale_of_takeoff(project, "101");
    REQUIRE(other.has_value());
    CHECK_STR_EQ(other->page_uid, "11");
    CHECK_EQ(other->scale.sf2, 100.0);
    // two takeoffs of one page capture the same page
    const auto same_page = page_scale_of_takeoff(project, "102");
    REQUIRE(same_page.has_value());
    CHECK_STR_EQ(same_page->page_uid, "10");
}

// A takeoff that is not in the project, one without a page uid, one whose page is gone: nothing is captured (the replay factor of that takeoff is 1).
TEST(SetScale_Capture, ATakeoffWithoutAKnownPageCapturesNothing)
{
    auto project = project_with_two_pages();
    add_takeoff(project, "200", "", {1.0, 1.0});
    add_takeoff(project, "201", "99", {1.0, 1.0});
    CHECK_FALSE(page_scale_of_takeoff(project, "999").has_value());
    CHECK_FALSE(page_scale_of_takeoff(project, "").has_value());
    CHECK_FALSE(page_scale_of_takeoff(project, "200").has_value());
    CHECK_FALSE(page_scale_of_takeoff(project, "201").has_value());
}

// After a Set Scale the lookup reads the NEW pair of the page (it reads the project, not a copy taken earlier).
TEST(SetScale_Capture, TheCaptureFollowsTheTakeoffsPageAfterASetScale)
{
    auto project = project_with_two_pages();
    // the memory twin of a save: page 10 becomes (0.1875, 12), the takeoffs of page 10 are rescaled
    const vector<string> changed = apply_page_scales(project.pages, project.takeoffs, {"10"}, 0.1875, 12.0);
    REQUIRE_EQ(changed.size(), static_cast<std::size_t>(1));
    const auto after = page_scale_of_takeoff(project, "100");
    REQUIRE(after.has_value());
    CHECK_EQ(after->scale.sf1, 0.1875);
    CHECK_EQ(after->scale.sf2, 12.0);
    // page 11 stays as it was
    const auto untouched = page_scale_of_takeoff(project, "101");
    REQUIRE(untouched.has_value());
    CHECK_EQ(untouched->scale.sf2, 100.0);
}

// ---- the replay rules --------------------------------------------------------------------------------------------------------------------------------------------------------------

// Nothing captured: the positions come back unchanged, whatever the current pair is, even when the page is unknown (the factor of an entry without a capture is 1).
TEST(SetScale_Replay, NothingCapturedReplaysTheStoredPositionsUnchanged)
{
    const vector<double> stored = {0.0, 0.0, 96.0, 0.0};
    for (const optional<PageScale> &current : {pair(0.1875, 12.0), pair(1.0, 1.0), optional<PageScale>()})
    {
        const auto result = replay_entry_positions(stored, std::nullopt, current);
        REQUIRE(result.has_value());
        CHECK(*result == stored);
    }
    // not the pair (1, 1) that Windows would read for a missing capture: on a 1/4 inch sheet that would turn 96 into 96 * 48
    const auto result = replay_entry_positions(stored, std::nullopt, pair(0.25, 12.0));
    REQUIRE(result.has_value());
    CHECK_EQ((*result)[2], 96.0);
}

// A captured entry whose page is gone at replay: the undo / redo is refused (deliberate difference from Windows, which would use the pair (1.0, 1.0)).
TEST(SetScale_Replay, ACapturedPageThatIsGoneRefusesTheReplay_DeliberateDifferenceFromWindows)
{
    const vector<double> stored = {0.0, 0.0, 96.0, 0.0};
    CHECK_FALSE(replay_entry_positions(stored, pair(0.125, 12.0), std::nullopt).has_value());
    CHECK_FALSE(replay_entry_positions({}, pair(0.125, 12.0), std::nullopt).has_value());
}

// The pinned Windows scenario, test_plan_view_action_handler.py:3849: [0, 0, 96, 0] captured at (0.125, 12), the page is now at (0.1875, 12): the replay gives [0, 0, 64, 0].
TEST(SetScale_Replay, TheWindowsDeleteUndoScenarioGivesSixtyFour)
{
    const auto result = replay_entry_positions({0.0, 0.0, 96.0, 0.0}, pair(0.125, 12.0), pair(0.1875, 12.0));
    REQUIRE(result.has_value());
    REQUIRE_EQ(result->size(), static_cast<std::size_t>(4));
    CHECK_NEAR((*result)[0], 0.0, 1e-12);
    CHECK_NEAR((*result)[1], 0.0, 1e-12);
    CHECK_NEAR((*result)[2], 64.0, 1e-9);
    CHECK_NEAR((*result)[3], 0.0, 1e-12);
}

// The same scale: unchanged. A scale that cannot be used (a factor that is not finite and > 0) refuses: the arithmetic of rescale_entry_positions is not bypassed.
TEST(SetScale_Replay, TheArithmeticRefusalsStandThroughTheRule)
{
    const vector<double> stored = {10.0, 20.0};
    const auto same = replay_entry_positions(stored, pair(0.125, 12.0), pair(0.125, 12.0));
    REQUIRE(same.has_value());
    CHECK(*same == stored);
    // the current pair has ratio 0 / infinity / negative: refused
    CHECK_FALSE(replay_entry_positions(stored, pair(0.125, 12.0), pair(0.125, 0.0)).has_value());
    CHECK_FALSE(replay_entry_positions(stored, pair(0.125, 12.0), pair(0.125, -12.0)).has_value());
    CHECK_FALSE(replay_entry_positions(stored, pair(0.125, 12.0), pair(0.125, kInfinity)).has_value());
    CHECK_FALSE(replay_entry_positions(stored, pair(0.125, 12.0), pair(kNotANumber, 12.0)).has_value());
}

// ---- the whole round, as the device check M1-17 does it, at the level of the Core -------------------------------------------------------------------------------------------------

// Delete a takeoff, change the scale, undo the delete: the takeoff is back at the SAME SHEET POSITION. The entry holds the positions in the units of the capture; the replay gives the
// value that the rescaled page would hold for that takeoff, which is the value a takeoff that stayed on the page got from the memory twin of the save.
TEST(SetScale_Replay, AnUndoneDeleteLandsWhereAStayingTakeoffWentAfterTheScaleChange)
{
    auto project = project_with_two_pages();

    // 1. the entry is created for takeoff 100: it stores the positions and the capture (page 10 at (0.125, 12))
    const vector<double> stored = project.takeoffs[0].position;
    const auto captured = page_scale_of_takeoff(project, "100");
    REQUIRE(captured.has_value());

    // 2. the takeoff is deleted, then the scale of page 10 changes to (0.1875, 12): the takeoff 102 of the page stays and is rescaled by the memory twin
    project.takeoffs.erase(project.takeoffs.begin());
    const auto changed = apply_page_scales(project.pages, project.takeoffs, {"10"}, 0.1875, 12.0);
    REQUIRE_EQ(changed.size(), static_cast<std::size_t>(1));
    const double staying_x = project.takeoffs[1].position[0]; // takeoff 102 [1, 2] -> [2/3, 4/3] (three decimals)
    CHECK_NEAR(staying_x, 0.667, 1e-9);

    // 3. the undo: the current pair of the captured page is asked, the stored positions are rescaled to it
    const optional<PageScale> current = page_scale_of_page(project, captured->page_uid);
    REQUIRE(current.has_value());
    const auto replayed = replay_entry_positions(stored, captured->scale, current);
    REQUIRE(replayed.has_value());
    CHECK_NEAR((*replayed)[2], 64.0, 1e-9);

    // the takeoff has the same sheet position as before: position / ratio is unchanged (ratio 96 before, 64 after)
    const double sheet_before = stored[2] / 96.0;
    const double sheet_after = (*replayed)[2] / *units_per_sheet_inch(0.1875, 12.0);
    CHECK_NEAR(sheet_after, sheet_before, 1e-12);
}

// Move a takeoff, change the scale, undo: both stored positions of the Move entry (old and new) are rescaled by the same captured pair.
TEST(SetScale_Replay, AMoveEntryRescalesTheOldAndTheNewPositionByTheSameCapture)
{
    auto project = project_with_two_pages();
    const auto captured = page_scale_of_takeoff(project, "102");
    REQUIRE(captured.has_value());
    const vector<double> old_position = {1.0, 2.0};
    const vector<double> new_position = {10.0, 20.0};

    apply_page_scales(project.pages, project.takeoffs, {"10"}, 0.25, 12.0); // ratio 96 -> 48: factor 0.5
    const optional<PageScale> current = page_scale_of_page(project, captured->page_uid);
    const auto undone = replay_entry_positions(old_position, captured->scale, current);
    const auto redone = replay_entry_positions(new_position, captured->scale, current);
    REQUIRE(undone.has_value());
    REQUIRE(redone.has_value());
    CHECK_NEAR((*undone)[0], 0.5, 1e-12);
    CHECK_NEAR((*undone)[1], 1.0, 1e-12);
    CHECK_NEAR((*redone)[0], 5.0, 1e-12);
    CHECK_NEAR((*redone)[1], 10.0, 1e-12);
    // the entry is not changed by a replay: a second replay at a later scale starts from the captured units again
    apply_page_scales(project.pages, project.takeoffs, {"10"}, 0.5, 12.0); // ratio 48 -> 24 from the capture's 96: factor 0.25
    const auto again = replay_entry_positions(old_position, captured->scale, page_scale_of_page(project, "10"));
    REQUIRE(again.has_value());
    CHECK_NEAR((*again)[0], 0.25, 1e-12);
    CHECK_NEAR((*again)[1], 0.5, 1e-12);
}
