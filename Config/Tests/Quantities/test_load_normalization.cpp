// load_project normalizes the units of every loaded condition (QTY-06: wiring of the Core normalization).
//
// Windows oracle: ost_visualizer/infrastructure/mdb/components/bid_data_reader.py (every BidConditions row goes
// through normalize_condition_uoms_for_system(condition, metric) with metric = Bids.MeasureBase == 1) and
// tests/integration/quantities/test_condition_uom_parity.py.  The expected units are the values of the Windows
// normalize_uom_for_calculation (see test_uom_normalization.cpp, which pins that function itself).
#include "QtyHelpers.hpp"

#include "../Persistence/PersistenceTestSupport.hpp"

#include <string>

using namespace qty;

namespace
{
    // One Bids row (UID 10) of the given measurement base with the given conditions
    // {UID, Type, Quantity1, UOM1, Quantity2, UOM2, Quantity3, UOM3}.
    struct Row
    {
        int uid, type, q1, u1, q2, u2, q3, u3;
    };

    void seed(const ost_ptest::TempDb &db, int measure_base, const std::vector<Row> &rows)
    {
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (10, 1, 'Job', 10, " +
                std::to_string(measure_base) + ")");
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (100, 10, 'P1')");
        int ref_no = 1;
        for (const Row &r : rows)
        {
            db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, Quantity1, UOM1, Quantity2, UOM2, "
                    "Quantity3, UOM3) VALUES (" +
                    std::to_string(r.uid) + ", 10, " + std::to_string(ref_no++) + ", 'C" + std::to_string(r.uid) +
                    "', " + std::to_string(r.type) + ", " + std::to_string(r.q1) + ", " + std::to_string(r.u1) +
                    ", " + std::to_string(r.q2) + ", " + std::to_string(r.u2) + ", " + std::to_string(r.q3) + ", " +
                    std::to_string(r.u3) + ")");
        }
    }

    const Condition &cond(const ost_data::ProjectData &p, const std::string &uid)
    {
        auto it = p.conditions.find(uid);
        if (it == p.conditions.end())
            FAIL_TEST("condition " + uid + " was not loaded");
        return it->second;
    }
} // namespace

// Windows probe (UNITS-M01 / DBP-06): a loaded BidConditions row is normalized with metric = (MeasureBase == 1).
// MeasureBase 1: (Quantity1=1, UOM1=2) -> 13; (20, 8) -> 16; (11, 5) -> 14; (23, 0) -> 0; (3, 2) -> 14.
TEST(Qty06_LoadNormalization, ConditionsOfAMetricBidAreLoadedWithMetricUnits)
{
    ost_ptest::TempDb db("qty_load_metric");
    seed(db, 1,
         {
             {30, 0, 1, 2, 4, 5, 20, 8},   // linear: LF, SF, CF -> m, m2, m3
             {31, 1, 11, 5, 0, 7, 21, 9},  // area: SF -> m2, (calc 0: only the system) 7 -> 14, CY -> m3
             {32, 2, 23, 2, 0, 0, 0, 0},   // count with a stored LF -> EA
             {33, 0, 1, 13, 4, 14, 20, 16}, // already metric: kept
         });
    ost_data::ProjectData project = ost_store::load_project(db.path(), "10");
    REQUIRE_EQ(project.conditions.size(), static_cast<size_t>(4));
    REQUIRE_EQ(project.bid.measure_base, 1);

    CHECK_EQ(cond(project, "30").uom1, 13);
    CHECK_EQ(cond(project, "30").uom2, 14);
    CHECK_EQ(cond(project, "30").uom3, 16);
    CHECK_EQ(cond(project, "31").uom1, 14);
    CHECK_EQ(cond(project, "31").uom2, 14);
    CHECK_EQ(cond(project, "31").uom3, 16);
    CHECK_EQ(cond(project, "32").uom1, 0);
    CHECK_EQ(cond(project, "33").uom1, 13);
    CHECK_EQ(cond(project, "33").uom2, 14);
    CHECK_EQ(cond(project, "33").uom3, 16);
    // The calculation codes themselves are never touched.
    CHECK_EQ(cond(project, "30").calc_type1, 1);
    CHECK_EQ(cond(project, "30").calc_type2, 4);
    CHECK_EQ(cond(project, "30").calc_type3, 20);
}

// MeasureBase 0: (1, 13) -> 2; (11, 14) -> 5; (20, 16) -> 8; (23, 2) -> 0; (11, 2) -> 5 (the repair of an
// incompatible pair applies to imperial bids too).
TEST(Qty06_LoadNormalization, ConditionsOfAnImperialBidAreLoadedWithImperialUnitsAndCompatiblePairs)
{
    ost_ptest::TempDb db("qty_load_imperial");
    seed(db, 0,
         {
             {30, 0, 1, 13, 4, 14, 20, 16}, // metric codes -> LF, SF, CF
             {31, 1, 11, 2, 11, 6, 0, 3},   // area: LF -> SF, SY kept, calc 0 keeps 3
             {32, 2, 23, 2, 24, 0, 0, 0},   // count: LF -> EA; total height: EA -> LF
             {33, 1, 11, 7, 20, 5, 1, 4},   // roofing square kept; volume with SF -> CF; length with IN2 -> LF
         });
    ost_data::ProjectData project = ost_store::load_project(db.path(), "10");
    REQUIRE_EQ(project.conditions.size(), static_cast<size_t>(4));
    REQUIRE_EQ(project.bid.measure_base, 0);

    CHECK_EQ(cond(project, "30").uom1, 2);
    CHECK_EQ(cond(project, "30").uom2, 5);
    CHECK_EQ(cond(project, "30").uom3, 8);
    CHECK_EQ(cond(project, "31").uom1, 5);
    CHECK_EQ(cond(project, "31").uom2, 6);
    CHECK_EQ(cond(project, "31").uom3, 3);
    CHECK_EQ(cond(project, "32").uom1, 0);
    CHECK_EQ(cond(project, "32").uom2, 2);
    CHECK_EQ(cond(project, "33").uom1, 7);
    CHECK_EQ(cond(project, "33").uom2, 8);
    CHECK_EQ(cond(project, "33").uom3, 2);
}

// Port of tests/integration/quantities/test_condition_uom_parity.py::ConditionUomConsistencyTests::test_incompatible_persisted_count_uom_normalizes_to_each_before_calculation
// through the loader: a Count condition stored with UOM LF reports 1 (iOS reported 1/12 'LF').
// And a stored LF length in a metric bid measures metres: a 100 in takeoff is 2.54 m.
TEST(Qty06_LoadNormalization, LoadedQuantitiesUseTheNormalizedUnits)
{
    ost_ptest::TempDb db("qty_load_quantities");
    seed(db, 1,
         {
             {30, 0, 1, 2, 0, 0, 0, 0},
             {31, 2, 23, 2, 0, 0, 0, 0},
         });
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (500, 10, 30, 100, '0;0;100;0')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (501, 10, 31, 100, '5;5')");
    ost_data::ProjectData project = ost_store::load_project(db.path(), "10");
    REQUIRE_EQ(project.takeoffs.size(), static_cast<size_t>(2));
    auto quantities = compute_page_quantities(project.conditions, project.takeoffs);
    REQUIRE_EQ(quantities.size(), static_cast<size_t>(2));
    CHECK_NEAR(quantities.at("30").q1, 2.54, 1e-12);
    CHECK_EQ(quantities.at("31").q1, 1.0);
}
