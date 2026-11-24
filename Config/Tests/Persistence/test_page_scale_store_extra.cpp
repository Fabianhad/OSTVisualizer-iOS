// Set Scale, the store writer: cases added by the independent review of m1-set-scale for mutants that the earlier tests did not kill (found by editing the store and the Core
// rules in a scratch copy and running the Set Scale tests: a mutant that survives is a rule no test pins). Each test names the Windows rule and the mutant it kills.
// Windows source of truth: page_operations.py:57-82 (the factor), :154-217 (`if not raw_position: continue`, the 17 tables).
#include "PageScaleTestSupport.hpp"

#include "Domain/PageScaleTransform.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

using ost_ptest::TempDb;
using ost_store::PageScaleOutcome;
using ost_store::PageScaleRefusal;
using std::string;
using std::vector;

namespace
{
    bool save(const TempDb &db, const vector<string> &pages, double sf1, double sf2, PageScaleOutcome *outcome, const string &bid = "10")
    {
        return ost_store::save_page_scales(db.path(), bid, pages, sf1, sf2, outcome);
    }
}

// A factor (new ratio / stored ratio) that overflows to infinity or underflows to 0 is refused BY THE STORE on its own, also when the page has no content that would refuse it
// later (Windows would write the new pair and rewrite nothing: there is nothing to rewrite; with content it writes "inf" text, SS-2). A page without rows and without an overlay
// rectangle is the case where only the store's own check of the factor stands between the new pair and the database. Kills: the removal of the factor check in save_one_page.
TEST(SetScale_StoreExtra, AFactorThatOverflowsOrUnderflowsIsRefusedOnAPageWithNoContent)
{
    for (const bool overflow : {true, false})
    {
        SCOPED_CONTEXT(overflow ? "factor 1e600 (overflow)" : "factor 1e-600 (underflow)");
        TempDb db("scale_extra_empty_factor");
        ost_ptest::seed_scale_world(db);
        // page 300 of bid 10: a usable stored scale at an extreme ratio and no row in any of the 17 tables, no overlay rectangle
        db.must(string("INSERT INTO BidPages (UID, BidUID, Name, ScaleFactor1, ScaleFactor2, Sequence) VALUES (300, 10, 'Empty', 1.0, ") + (overflow ? "1e-300" : "1e300") + ", 9)");
        for (const string &table : ost_ptest::scale_position_tables())
            CHECK_EQ(db.text("SELECT COUNT(*) FROM [" + table + "] WHERE BidPageUID = 300"), "0");
        const string before = db.snapshot_all();
        PageScaleOutcome outcome;
        CHECK_FALSE(save(db, {"300"}, 1.0, overflow ? 1e300 : 1e-300, &outcome));
        CHECK(outcome.refusal == PageScaleRefusal::BadNewScale);
        CHECK_STR_EQ(outcome.error, "Page 300 requires finite positive scale factors");
        CHECK_EQ(db.snapshot_all(), before);
        // the control: a finite factor on the same page writes the pair
        CHECK(save(db, {"300"}, 1.0, overflow ? 1e-290 : 1e290, nullptr));
        CHECK_EQ(ost_ptest::scale_values(db, 300), (std::pair<double, double>(1.0, overflow ? 1e-290 : 1e290)));
    }
}

// page_operations.py:177-178 `if not raw_position: continue`: an EMPTY Position (an empty text and an empty BLOB, as well as NULL) is skipped in EVERY one of the 17 tables, the
// legends included (the legend rewrite is not called on it, so an empty legend is not "malformed XML"). The rows stay byte for byte as they were, the rest of the page is rewritten
// and the change succeeds. Kills: the removal of the empty-text skip in rescale_table (an empty legend would be refused as BadLegend).
TEST(SetScale_StoreExtra, AnEmptyPositionIsSkippedInEveryTableLegendsIncluded)
{
    TempDb db("scale_extra_empty_position");
    ost_ptest::seed_scale_world(db);
    long long uid = 900000;
    struct Empty
    {
        long long text_row;
        long long blob_row;
        long long null_row;
    };
    vector<std::pair<string, Empty>> rows;
    for (const string &table : ost_ptest::scale_position_tables())
    {
        Empty empty{uid, uid + 1, uid + 2};
        uid += 3;
        db.must("INSERT INTO [" + table + "] (UID, BidUID, BidPageUID, Position) VALUES (" + std::to_string(empty.text_row) + ", 10, 100, '')");
        db.must("INSERT INTO [" + table + "] (UID, BidUID, BidPageUID, Position) VALUES (" + std::to_string(empty.blob_row) + ", 10, 100, X'')");
        db.must("INSERT INTO [" + table + "] (UID, BidUID, BidPageUID, Position) VALUES (" + std::to_string(empty.null_row) + ", 10, 100, NULL)");
        rows.emplace_back(table, empty);
    }
    PageScaleOutcome outcome;
    REQUIRE(save(db, {"100"}, 0.25, 12.0, &outcome));
    CHECK(outcome.refusal == PageScaleRefusal::None);
    for (const auto &entry : rows)
    {
        SCOPED_CONTEXT("table " + entry.first);
        CHECK_EQ(ost_ptest::type_of(db, entry.first, "Position", entry.second.text_row), "text");
        CHECK_EQ(ost_ptest::position_of(db, entry.first, entry.second.text_row), "");
        CHECK_EQ(ost_ptest::type_of(db, entry.first, "Position", entry.second.blob_row), "blob"); // not rewritten: still the empty BLOB
        CHECK_EQ(ost_ptest::type_of(db, entry.first, "Position", entry.second.null_row), "null");
        // the world row of the same table WAS rewritten (the skip does not end the table)
        CHECK(ost_ptest::position_of(db, entry.first, ost_ptest::world_row_uid(entry.first, 0)) != ost_ptest::world_position(entry.first));
    }
    CHECK_EQ(ost_ptest::scale_values(db, 100), (std::pair<double, double>(0.25, 12.0)));
}

// The memory twin (apply_page_scales) refuses a page whose stored FIRST scale value is not usable (0, negative, NaN, infinite) exactly like the store does (the store reads the cell
// and refuses BadStoredScale; memory can hold what the loader coalesced, e.g. a NaN or a negative value). The earlier twin test varies only the stored SECOND value. A twin that
// repairs a bad first value (reads it as 1) would rescale a page by a factor that the store never accepts. Kills: a twin that replaces an unusable stored sf1 by 1.
TEST(SetScale_StoreExtra, TheMemoryTwinRefusesAnUnusableStoredFirstScale)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double stored : {0.0, -0.0, -1.0, -0.125, nan, inf, -inf})
    {
        SCOPED_CONTEXT("stored sf1 " + ost_test::detail::exact_float(stored));
        std::unordered_map<string, ost_domain::Page> pages;
        ost_domain::Page page;
        page.uid = "p1";
        page.scale_factor1 = stored;
        page.scale_factor2 = 96.0;
        page.overlay_rect = "10,20,30,40";
        page.overlay_offset_x = 4.0;
        page.overlay_offset_y = 6.0;
        pages["p1"] = page;
        ost_domain::Takeoff takeoff;
        takeoff.uid = "t1";
        takeoff.page_uid = "p1";
        takeoff.position = {96.0, 192.0};
        std::vector<ost_domain::Takeoff> takeoffs = {takeoff};
        CHECK(ost_domain::apply_page_scales(pages, takeoffs, {"p1"}, 1.0, 48.0).empty());
        // nothing changed: not the scale, not the rectangle, not the offsets, not the takeoff
        CHECK(std::isnan(stored) ? std::isnan(pages["p1"].scale_factor1) : pages["p1"].scale_factor1 == stored);
        CHECK_EQ(pages["p1"].scale_factor2, 96.0);
        CHECK_STR_EQ(pages["p1"].overlay_rect, "10,20,30,40");
        CHECK_EQ(pages["p1"].overlay_offset_x, 4.0);
        CHECK_EQ(pages["p1"].overlay_offset_y, 6.0);
        CHECK_EQ(takeoffs[0].position, std::vector<double>({96.0, 192.0}));
    }
    // the control: a usable first value is rescaled by the factor 0.5
    std::unordered_map<string, ost_domain::Page> pages;
    ost_domain::Page page;
    page.uid = "p1";
    page.scale_factor1 = 1.0;
    page.scale_factor2 = 96.0;
    pages["p1"] = page;
    std::vector<ost_domain::Takeoff> takeoffs;
    CHECK_EQ(ost_domain::apply_page_scales(pages, takeoffs, {"p1"}, 1.0, 48.0), std::vector<string>({"p1"}));
}
