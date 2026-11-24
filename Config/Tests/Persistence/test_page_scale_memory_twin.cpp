// Set Scale, the MEMORY twin of the store writer (plan 5.4, 4.11): ost_domain::apply_page_scales updates the pages and the takeoffs the app holds in memory after a successful save,
// so that the success path does not reload (the ProjectData member apply_page_scales is a one-line wrapper of it). It is written like the STORE, not like Windows
// project_data_service.py:358-418 (register SS-4): takeoff positions go through the three-decimal text round trip, the overlay rectangle text and BOTH offsets are taken from the scaled
// rectangle, a change the store would refuse is refused. The proof is the AGREEMENT test: memory after the twin == a reload of the database after the store write.
// Ported Windows tests (group G, tests/domain/services/test_project_data_service.py): 7 ported 1:1 or adapted by the Windows model (no annotations on iOS in M1, no bid refs, no
// page object identity), 2 adapted for the deliberate differences (:2298 offsets, :2380 negative extent). iOS has no annotations in memory in M1: the annotation halves are M4.
#include "PageScaleTestSupport.hpp"

#include "Domain/PageScaleTransform.hpp"
#include "Domain/PositionParser.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

using ost_domain::Page;
using ost_domain::Takeoff;
using ost_ptest::TempDb;
using std::string;
using std::unordered_map;
using std::vector;

namespace
{
    struct World
    {
        unordered_map<string, Page> pages;
        vector<Takeoff> takeoffs;
    };

    Page make_page(const string &uid, double sf1, double sf2, const string &rect = "", double offset_x = 0.0, double offset_y = 0.0)
    {
        Page page;
        page.uid = uid;
        page.name = "Page " + uid;
        page.scale_factor1 = sf1;
        page.scale_factor2 = sf2;
        page.overlay_rect = rect;
        page.overlay_offset_x = offset_x;
        page.overlay_offset_y = offset_y;
        return page;
    }

    Takeoff make_takeoff(const string &uid, const string &page_uid, const vector<double> &position)
    {
        Takeoff takeoff;
        takeoff.uid = uid;
        takeoff.condition_uid = "c";
        takeoff.page_uid = page_uid;
        takeoff.position = position;
        return takeoff;
    }

    // ProjectDataServicePageMetadataTests.setUp (test_project_data_service.py:2231-2297): p1 is (1, 96) with the overlay (10, 20, 30, 40) and the offsets (4, 6), p2 is (1, 96);
    // t1 (p1) [96, 192, 288, 384], t2 (p2) [96, 96], t3 (a page that is not in the project) [96, 96].
    World windows_world()
    {
        World world;
        world.pages["p1"] = make_page("p1", 1.0, 96.0, "10,20,30,40", 4.0, 6.0);
        world.pages["p2"] = make_page("p2", 1.0, 96.0);
        world.takeoffs = {make_takeoff("t1", "p1", {96.0, 192.0, 288.0, 384.0}), make_takeoff("t2", "p2", {96.0, 96.0}), make_takeoff("t3", "other", {96.0, 96.0})};
        return world;
    }

    vector<string> apply(World &world, const vector<string> &uids, double sf1, double sf2)
    {
        return ost_domain::apply_page_scales(world.pages, world.takeoffs, uids, sf1, sf2);
    }

    // The whole memory of a world as text, to prove "nothing changed" exactly (every field the twin may touch).
    string describe(const World &world)
    {
        vector<string> uids;
        for (const auto &entry : world.pages)
            uids.push_back(entry.first);
        std::sort(uids.begin(), uids.end());
        string out;
        for (const string &uid : uids)
        {
            const Page &page = world.pages.at(uid);
            out += "page " + uid + " " + ost_test::detail::exact_float(page.scale_factor1) + " " + ost_test::detail::exact_float(page.scale_factor2) + " '" + page.overlay_rect + "' " +
                   ost_test::detail::exact_float(page.overlay_offset_x) + " " + ost_test::detail::exact_float(page.overlay_offset_y) + "\n";
        }
        for (const Takeoff &takeoff : world.takeoffs)
        {
            out += "takeoff " + takeoff.uid + " " + takeoff.page_uid;
            for (const double value : takeoff.position)
                out += " " + ost_test::detail::exact_float(value);
            out += "\n";
        }
        return out;
    }
}

// ---- group G: tests/domain/services/test_project_data_service.py -------------------------------------------------------------------------------------

// :1211 test_committed_scale_projects_only_scale_dependent_model_state (the takeoff and page half): page 42 (1, 10), takeoff [20, 40], overlay (2, 4, 6, 8) with the offsets (2, 4),
// -> (1, 20): the takeoff [40, 80], the rectangle (4, 8, 12, 16) as the stored text, the offsets (4, 8) (the scaled rectangle x and y).
TEST(SetScale_MemoryTwin, CommittedScaleProjectsOnlyScaleDependentModelState)
{
    World world;
    world.pages["42"] = make_page("42", 1.0, 10.0, "2,4,6,8", 2.0, 4.0);
    world.takeoffs = {make_takeoff("10", "42", {20.0, 40.0})};
    const vector<string> changed = apply(world, {"42"}, 1.0, 20.0);
    CHECK_EQ(changed, vector<string>({"42"}));
    CHECK_EQ(world.pages["42"].scale_factor1, 1.0);
    CHECK_EQ(world.pages["42"].scale_factor2, 20.0);
    CHECK_EQ(world.takeoffs[0].position, vector<double>({40.0, 80.0}));
    CHECK_STR_EQ(world.pages["42"].overlay_rect, "4.000000,8.000000,12.000000,16.000000");
    CHECK_EQ(world.pages["42"].overlay_offset_x, 4.0);
    CHECK_EQ(world.pages["42"].overlay_offset_y, 8.0);
}

// :1221 test_scale_projection_rejects_another_bid_without_mutation and :1231 test_scale_batch_rejects_replaced_page_before_mutating_valid_sibling. ADAPTED: iOS holds ONE bid
// and its page map has no object identity, so "another bid" and "a replaced page" are "a uid that is not a page of the project": the call is refused ({}) and NOTHING changes, not
// even the valid page next to the unknown one.
TEST(SetScale_MemoryTwin, AnUnknownPageRefusesTheWholeCallWithoutMutation_Adapted)
{
    World world;
    world.pages["42"] = make_page("42", 1.0, 10.0, "2,4,6,8", 2.0, 4.0);
    world.takeoffs = {make_takeoff("10", "42", {20.0, 40.0})};
    const string before = describe(world);
    CHECK_EQ(apply(world, {"43"}, 1.0, 20.0), vector<string>());
    CHECK_EQ(apply(world, {"42", "43"}, 1.0, 20.0), vector<string>());
    CHECK_EQ(apply(world, {"43", "42"}, 1.0, 20.0), vector<string>());
    CHECK_EQ(describe(world), before);
    CHECK_EQ(apply(world, {"42"}, 1.0, 20.0), vector<string>({"42"})); // the control
}

// :2298 test_scales_rescale_geometry_overlay_and_metadata_for_exactly_the_selected_pages. ADAPTED (offsets, SS-4): p1 (1, 96) -> (1, 48): t1 [96, 192, 288, 384] -> [48, 96, 144, 192],
// the takeoffs of p2 and of the unknown page are unchanged, p2 keeps (1, 96), the overlay (10, 20, 30, 40) -> (5, 10, 15, 20) as text. Windows expects the offsets (2, 3) (each offset
// x 0.5, memory only); the store writes the offsets of the SCALED RECTANGLE, (5, 10), and the twin must agree with a reload (see the agreement test): offsets (5, 10).
TEST(SetScale_MemoryTwin, ScalesRescaleGeometryOverlayAndMetadataForExactlyTheSelectedPages_Adapted)
{
    World world = windows_world();
    CHECK_EQ(apply(world, {"p1"}, 1.0, 48.0), vector<string>({"p1"}));
    CHECK_EQ(world.pages["p1"].scale_factor1, 1.0);
    CHECK_EQ(world.pages["p1"].scale_factor2, 48.0);
    CHECK_EQ(world.pages["p2"].scale_factor1, 1.0);
    CHECK_EQ(world.pages["p2"].scale_factor2, 96.0);
    CHECK_EQ(world.takeoffs[0].position, vector<double>({48.0, 96.0, 144.0, 192.0}));
    CHECK_EQ(world.takeoffs[1].position, vector<double>({96.0, 96.0}));
    CHECK_EQ(world.takeoffs[2].position, vector<double>({96.0, 96.0}));
    CHECK_STR_EQ(world.pages["p1"].overlay_rect, "5.000000,10.000000,15.000000,20.000000");
    CHECK_EQ(world.pages["p1"].overlay_offset_x, 5.0); // Windows: 2.0
    CHECK_EQ(world.pages["p1"].overlay_offset_y, 10.0); // Windows: 3.0
}

// :2327 test_ordered_uids_follow_the_input_pages_and_overlay_is_only_scaled_with_a_real_rectangle: [p2, p1] -> (2, 96) returns ("p2", "p1"); the empty overlay of p2 stays; p1 goes
// from 96 to 48 units per inch, so its rectangle is halved; a rectangle with a ZERO width or height is not a real rectangle and stays (with its offset); a tiny (1, 1, 0.5, 0.5)
// becomes (0.5, 0.5, 0.25, 0.25). ADAPTED: the offsets of a real rectangle follow the scaled rectangle (5, 10), not Windows (2, 3).
TEST(SetScale_MemoryTwin, OrderedUidsFollowTheInputAndOverlayIsOnlyScaledWithARealRectangle_Adapted)
{
    World world = windows_world();
    CHECK_EQ(apply(world, {"p2", "p1"}, 2.0, 96.0), vector<string>({"p2", "p1"}));
    CHECK_STR_EQ(world.pages["p2"].overlay_rect, "");
    CHECK_EQ(world.pages["p2"].overlay_offset_x, 0.0);
    CHECK_STR_EQ(world.pages["p1"].overlay_rect, "5.000000,10.000000,15.000000,20.000000");
    CHECK_EQ(world.pages["p1"].overlay_offset_x, 5.0);
    CHECK_EQ(world.pages["p1"].overlay_offset_y, 10.0);
    // a flat rectangle (width 0) and a tall one (height 0) are not real rectangles: they and their offsets stay
    for (const char *rect : {"10,20,0,40", "10,20,30,0"})
    {
        SCOPED_CONTEXT(string("rect ") + rect);
        World flat;
        flat.pages["p1"] = make_page("p1", 1.0, 96.0, rect, 4.0, 6.0);
        CHECK_EQ(apply(flat, {"p1"}, 1.0, 48.0), vector<string>({"p1"}));
        CHECK_STR_EQ(flat.pages["p1"].overlay_rect, rect);
        CHECK_EQ(flat.pages["p1"].overlay_offset_x, 4.0);
        CHECK_EQ(flat.pages["p1"].overlay_offset_y, 6.0);
        CHECK_EQ(flat.pages["p1"].scale_factor2, 48.0); // the scale itself changes
    }
    World tiny;
    tiny.pages["p1"] = make_page("p1", 1.0, 96.0, "1,1,0.5,0.5");
    CHECK_EQ(apply(tiny, {"p1"}, 1.0, 48.0), vector<string>({"p1"}));
    CHECK_STR_EQ(tiny.pages["p1"].overlay_rect, "0.500000,0.500000,0.250000,0.250000");
}

// :2380 test_an_overlay_with_a_negative_extent_is_not_a_real_rectangle_and_stays_unscaled (memory only, register SS-4). ADAPTED: Windows skips the rectangle in memory but the DATABASE
// refuses it (test_page_operations.py:293), so memory and a reload could disagree. iOS: a negative width or height is a malformed rectangle: the twin refuses the whole change, {},
// and nothing changes (the scale included), exactly like the store.
TEST(SetScale_MemoryTwin, ANegativeExtentIsRefusedAndNothingChanges_Adapted)
{
    World control; // the control: the same page with a rectangle of positive size applies
    control.pages["p1"] = make_page("p1", 1.0, 96.0, "10,20,30,40", 4.0, 6.0);
    CHECK_EQ(apply(control, {"p1"}, 1.0, 48.0), vector<string>({"p1"}));
    for (const char *rect : {"10,20,-30,40", "10,20,30,-40", "10,20,-30,-40"})
    {
        SCOPED_CONTEXT(string("rect ") + rect);
        World world;
        world.pages["p1"] = make_page("p1", 1.0, 96.0, rect, 4.0, 6.0);
        world.takeoffs = {make_takeoff("t1", "p1", {96.0, 192.0})};
        const string before = describe(world);
        CHECK_EQ(apply(world, {"p1"}, 1.0, 48.0), vector<string>());
        CHECK_EQ(describe(world), before);
    }
}

// :2406 test_scale_changes_are_refused_without_touching_anything_for_other_bids_empty_or_foreign_pages (the portable half): an empty list and a page that is not in the project
// ("zz", or one valid page with one unknown page) are refused ({}); nothing changes. (Another bid and a "stranger" object with the same uid are not iOS notions.)
TEST(SetScale_MemoryTwin, EmptyAndUnknownPagesAreRefusedWithoutTouchingAnything)
{
    World control = windows_world(); // the control: the same call with a page that exists applies
    CHECK_EQ(apply(control, {"p1"}, 1.0, 48.0), vector<string>({"p1"}));
    World world = windows_world();
    const string before = describe(world);
    CHECK_EQ(apply(world, {}, 1.0, 48.0), vector<string>());
    CHECK_EQ(apply(world, {"p1", "zz"}, 1.0, 48.0), vector<string>());
    CHECK_EQ(apply(world, {"zz"}, 1.0, 48.0), vector<string>());
    CHECK_EQ(apply(world, {""}, 1.0, 48.0), vector<string>());
    CHECK_EQ(describe(world), before);
    CHECK_EQ(world.pages["p1"].scale_factor2, 96.0);
    CHECK_EQ(world.takeoffs[0].position, vector<double>({96.0, 192.0, 288.0, 384.0}));
}

// :3802 test_a_mixed_scale_batch_with_one_foreign_page_changes_nothing: [p1, p2-that-is-not-here] refuses the whole call; p1, its overlay and its takeoff keep their values.
TEST(SetScale_MemoryTwin, AMixedBatchWithOneForeignPageChangesNothing)
{
    World control = windows_world(); // the control: with both pages present the same call applies
    CHECK_EQ(apply(control, {"p1", "p2"}, 1.0, 48.0), vector<string>({"p1", "p2"}));
    World world = windows_world();
    world.pages.erase("p2");
    const string before = describe(world);
    CHECK_EQ(apply(world, {"p1", "p2"}, 1.0, 48.0), vector<string>());
    CHECK_EQ(describe(world), before);
    CHECK_EQ(world.pages["p1"].scale_factor2, 96.0);
    CHECK_STR_EQ(world.pages["p1"].overlay_rect, "10,20,30,40");
    CHECK_EQ(world.takeoffs[0].position, vector<double>({96.0, 192.0, 288.0, 384.0}));
}

// :3955 test_an_empty_scale_batch_is_refused_before_the_scale_values_are_parsed: an empty list with a pair that would not even parse ((NaN, NaN): Windows passes (None, "x")) is
// refused first and changes nothing.
TEST(SetScale_MemoryTwin, AnEmptyBatchIsRefusedBeforeTheScaleValuesAreRead)
{
    World control = windows_world(); // the control: a batch with a page and a good pair applies
    CHECK_EQ(apply(control, {"p1"}, 1.0, 48.0), vector<string>({"p1"}));
    World world = windows_world();
    const string before = describe(world);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK_EQ(apply(world, {}, nan, nan), vector<string>());
    CHECK_EQ(describe(world), before);
}

// ---- iOS-only: the twin refuses what the store refuses, and rounds like the stored text -----------------------------------------------------------------

// A change the store would refuse is refused by the twin and leaves the memory untouched: an unusable page scale (0, NaN, negative, infinite) or new pair, an overflowing or
// underflowing factor, a coordinate that overflows or underflows, a rectangle that is not four numbers, a duplicate request is allowed (once).
TEST(SetScale_MemoryTwin, AFailureLeavesProjectDataUntouched)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    {
        // unusable new pairs
        World world = windows_world();
        const string before = describe(world);
        const struct
        {
            double sf1, sf2;
        } bad[] = {{0.0, 48.0}, {1.0, 0.0}, {-1.0, 48.0}, {1.0, -48.0}, {nan, 48.0}, {1.0, nan}, {inf, 48.0}, {1.0, inf}, {1e-320, 1e300}, {1e300, 1e-320}};
        for (const auto &pair : bad)
        {
            SCOPED_CONTEXT("new (" + ost_test::detail::exact_float(pair.sf1) + ", " + ost_test::detail::exact_float(pair.sf2) + ")");
            CHECK_EQ(apply(world, {"p1"}, pair.sf1, pair.sf2), vector<string>());
            CHECK_EQ(describe(world), before);
        }
        CHECK_EQ(apply(world, {"p1"}, 1.0, 48.0), vector<string>({"p1"})); // the control
    }
    {
        // an unusable stored scale of the page (memory can hold what the loader would have coalesced: NaN, a negative value, an infinity)
        for (const double stored : {nan, -1.0, inf, 0.0})
        {
            SCOPED_CONTEXT("stored sf2 " + ost_test::detail::exact_float(stored));
            World world = windows_world();
            world.pages["p1"].scale_factor2 = stored;
            const string before = describe(world);
            CHECK_EQ(apply(world, {"p1"}, 1.0, 48.0), vector<string>());
            CHECK_EQ(describe(world), before);
        }
    }
    {
        // coordinates and rectangles
        const struct
        {
            vector<double> position;
            const char *rect;
        } faults[] = {
            {{1e300, 1.0}, "10,20,30,40"},     // overflows at x 1e10
            {{5e-324, 1.0}, "10,20,30,40"},    // underflows at x 0.5
            {{inf, 1.0}, "10,20,30,40"},
            {{nan, 1.0}, "10,20,30,40"},
            {{1.0, 2.0}, "10,20,abc,40"},
            {{1.0, 2.0}, "10,20,30"},
            {{1.0, 2.0}, "nan,20,30,40"},
            {{1.0, 2.0}, "5e-324,20,30,40"},
        };
        for (const auto &fault : faults)
        {
            SCOPED_CONTEXT(string("rect '") + fault.rect + "' position " + (fault.position.empty() ? "" : ost_test::detail::exact_float(fault.position[0])));
            World world;
            world.pages["p1"] = make_page("p1", 1.0, 96.0, fault.rect, 4.0, 6.0);
            world.takeoffs = {make_takeoff("t1", "p1", fault.position)};
            const string before = describe(world);
            const double sf2 = fault.position[0] == 1e300 ? 9.6e11 : 48.0; // x 1e10 for the overflow row, x 0.5 otherwise
            CHECK_EQ(apply(world, {"p1"}, 1.0, sf2), vector<string>());
            CHECK_EQ(describe(world), before);
        }
    }
}

// A repeated uid counts once (a page is never scaled twice) and the order of the input is the order of the result.
TEST(SetScale_MemoryTwin, RepeatedUidsCountOnceAndTheInputOrderIsKept)
{
    World world = windows_world();
    CHECK_EQ(apply(world, {"p2", "p1", "p2", "p1"}, 1.0, 48.0), vector<string>({"p2", "p1"}));
    CHECK_EQ(world.takeoffs[0].position, vector<double>({48.0, 96.0, 144.0, 192.0})); // x 0.5 once
    CHECK_EQ(world.takeoffs[1].position, vector<double>({48.0, 48.0}));
}

// Each page of a batch is rescaled by ITS OWN scale; a change of less than 1e-9 in the factor writes the new pair and touches no content (the store does the same).
TEST(SetScale_MemoryTwin, EachPageUsesItsOwnScaleAndATinyChangeTouchesNoContent)
{
    World world = windows_world();
    world.pages["p2"].scale_factor2 = 48.0; // p2 is (1, 48): 48 units per inch
    CHECK_EQ(apply(world, {"p1", "p2"}, 1.0, 192.0), vector<string>({"p1", "p2"}));
    CHECK_EQ(world.takeoffs[0].position, vector<double>({192.0, 384.0, 576.0, 768.0})); // p1: x 2
    CHECK_EQ(world.takeoffs[1].position, vector<double>({384.0, 384.0}));               // p2: x 4
    World tiny = windows_world();
    CHECK_EQ(apply(tiny, {"p1"}, 1.0, 96.0 * (1 + 1e-12)), vector<string>({"p1"}));
    CHECK_EQ(tiny.pages["p1"].scale_factor2, 96.0 * (1 + 1e-12));
    CHECK_EQ(tiny.takeoffs[0].position, vector<double>({96.0, 192.0, 288.0, 384.0}));
    CHECK_STR_EQ(tiny.pages["p1"].overlay_rect, "10,20,30,40");
    CHECK_EQ(tiny.pages["p1"].overlay_offset_x, 4.0);
}

// The twin rounds a takeoff like the STORED text: 1234.567 and 2345.678 x 0.3 are 370.37 and 703.703 (the three-decimal text parsed back), not the unrounded 370.3701 /
// 703.7034 that Windows memory would hold (SS-6: the database re-quantises; memory must hold what a reload reads).
TEST(SetScale_MemoryTwin, TakeoffPositionsAreRoundedLikeTheStoredText)
{
    World world;
    world.pages["p1"] = make_page("p1", 1.0, 100.0);
    world.takeoffs = {make_takeoff("t1", "p1", {1234.567, 2345.678}), make_takeoff("t2", "p1", {0.0625, -0.0004, 1e22})};
    CHECK_EQ(apply(world, {"p1"}, 1.0, 30.0), vector<string>({"p1"}));
    CHECK_EQ(world.takeoffs[0].position, ost_domain::parse_position("370.37;703.703"));
    CHECK_EQ(world.takeoffs[0].position[0], 370.37);
    CHECK_EQ(world.takeoffs[0].position[1], 703.703);
    // 0.0625 x 0.3 = 0.01875 -> 0.019, -0.0004 x 0.3 -> "-0" (the sign survives), 1e22 x 0.3 = 3e21 in full digits: the Windows text of serialization.encode_position (python -B -I)
    CHECK_EQ(world.takeoffs[1].position, ost_domain::parse_position("0.019;-0;3000000000000000000000"));
}

// ---- the agreement test: memory == reload -----------------------------------------------------------------------------------------------------------

namespace
{
    // The fixture of the agreement test: bid 10 (seed_two_bids: conditions 30 and 31, pages 100 and 101) with takeoffs that need three-decimal rounding on both pages, a rectangle on
    // page 100, an empty rectangle on page 101, and bid 20's page 200 with a takeoff (it must not change).
    void seed_agreement_world(const TempDb &db)
    {
        ost_ptest::seed_two_bids(db);
        db.must("UPDATE BidPages SET ScaleFactor1 = 1.0, ScaleFactor2 = 100.0 WHERE UID IN (100, 101, 200)");
        db.must("UPDATE BidPages SET OverlayRect = '1234.567,2345.678,100.123456,200.123456', OverlayOffsetX = 9.0, OverlayOffsetY = 8.0 WHERE UID = 100");
        db.must("UPDATE BidPages SET OverlayRect = '', OverlayOffsetX = 3.0, OverlayOffsetY = 2.0 WHERE UID = 101");
        const struct
        {
            int uid;
            int condition;
            int page;
            int bid;
            const char *position;
        } rows[] = {{801, 30, 100, 10, "1234.567;2345.678\n"},
                    {802, 31, 100, 10, "0.0625;0.1875;0.3125;0.4375"},
                    {803, 30, 100, 10, "123456.789;987654.321;7;8;9;10;11"},
                    {804, 30, 101, 10, "3.14159;2.71828"},
                    {805, 31, 101, 10, "-0.0004;0.0004;1e5;1e-5"},
                    {806, 30, 100, 10, "not-numeric"},
                    {807, 30, 100, 10, ""}};
        for (const auto &row : rows)
            db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (" + std::to_string(row.uid) + ", " + std::to_string(row.bid) + ", " +
                    std::to_string(row.condition) + ", " + std::to_string(row.page) + ", '" + row.position + "')");
    }

    // The memory twin's view of a loaded project: only what it may touch.
    string describe_loaded(const ost_data::ProjectData &project)
    {
        World world;
        world.pages = project.pages;
        world.takeoffs = project.takeoffs;
        std::sort(world.takeoffs.begin(), world.takeoffs.end(), [](const Takeoff &a, const Takeoff &b) { return std::stoll(a.uid) < std::stoll(b.uid); });
        return describe(world);
    }
}

// tests/integration/pages/test_set_scale_apply.py:277 test_scale_save_does_not_depend_on_database_reload: the success path updates memory WITHOUT reading the database again (Windows: a
// reload that fails does not matter, the page is updated in place and no reload happens). The twin has no database argument; here the proof is that after the store write the database is
// made unreadable (the pages table is dropped, so a reload fails) and the twin still gives memory exactly what the agreement test shows a reload would give.
TEST(SetScale_MemoryTwin, AScaleSaveDoesNotDependOnADatabaseReload)
{
    TempDb db("scale_twin_no_reload");
    seed_agreement_world(db);
    ost_data::ProjectData memory;
    std::string error;
    REQUIRE(ost_store::load_project(db.path(), "10", memory, &error));
    REQUIRE(ost_store::save_page_scale(db.path(), "10", "100", 1.0, 30.0, nullptr));
    ost_data::ProjectData reloaded;
    REQUIRE(ost_store::load_project(db.path(), "10", reloaded, &error)); // what a reload gives, read while the database is still readable
    db.must("DROP TABLE BidPages");
    ost_data::ProjectData failed;
    CHECK_FALSE(ost_store::load_project(db.path(), "10", failed, &error)); // a reload is impossible now
    CHECK_EQ(ost_domain::apply_page_scales(memory.pages, memory.takeoffs, {"100"}, 1.0, 30.0), vector<string>({"100"}));
    CHECK_EQ(describe_loaded(memory), describe_loaded(reloaded));
}

// Memory equals reload: after each of a series of changes (single page, both pages, back and forth, ratios that need rounding), the twin applied to the loaded project gives EXACTLY what
// a fresh load_project of the database gives after the store write: the page scale pairs, the overlay rectangle text, both offsets, and every takeoff position (bit for bit).
TEST(SetScale_MemoryTwin, MemoryEqualsAReloadOfTheDatabaseAfterEveryChange)
{
    TempDb db("scale_twin_agreement");
    seed_agreement_world(db);
    ost_data::ProjectData memory;
    std::string error;
    REQUIRE(ost_store::load_project(db.path(), "10", memory, &error));
    const struct
    {
        vector<string> pages;
        double sf1, sf2;
    } steps[] = {
        {{"100"}, 1.0, 30.0},           // factor 0.3
        {{"100"}, 1.0, 100.0},          // back: 1 / 0.3 (re-quantised)
        {{"100", "101"}, 0.25, 12.0},   // both pages, each by its own stored scale
        {{"101"}, 0.125, 12.0},
        {{"100", "101"}, 1.0, 3.0},
        {{"100"}, 0.5, 1.0},
        {{"101", "100"}, 10.0, 1.0},
        {{"100"}, 10.0, 1.0},           // the same scale again: factor 1, nothing rewritten
        {{"100", "101"}, 1.0, 97.0},
        {{"100", "101"}, 1.0, 1e5},
    };
    int index = 0;
    for (const auto &step : steps)
    {
        SCOPED_CONTEXT("step " + std::to_string(index++) + ": scale (" + ost_test::detail::exact_float(step.sf1) + ", " + ost_test::detail::exact_float(step.sf2) + ")");
        ost_store::PageScaleOutcome outcome;
        REQUIRE(ost_store::save_page_scales(db.path(), "10", step.pages, step.sf1, step.sf2, &outcome));
        const vector<string> changed = ost_domain::apply_page_scales(memory.pages, memory.takeoffs, step.pages, step.sf1, step.sf2);
        CHECK_EQ(changed, step.pages);
        ost_data::ProjectData reloaded;
        REQUIRE(ost_store::load_project(db.path(), "10", reloaded, &error));
        CHECK_EQ(describe_loaded(memory), describe_loaded(reloaded));
    }
    // bid 20 never changed
    ost_data::ProjectData other;
    REQUIRE(ost_store::load_project(db.path(), "20", other, &error));
    CHECK_EQ(other.pages["200"].scale_factor2, 100.0);
}
