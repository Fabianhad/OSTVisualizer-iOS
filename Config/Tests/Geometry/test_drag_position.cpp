// RPV-07, RPV-08, RPV-09, RPV-M01, RPV-M03, UND-11, GEO-M02 (aliases SNP-03/04, GEO-10/11/12, SNP-M01): where a dragged shape
// ends up and when the drag is rejected, extracted from DragController.mm into Core/Geometry/drag_rules.hpp.
//
// Windows oracle: presentation/components/plan_view/components/drag_handler.py (compute_new_position,
// _constrained_edge_resize, _polygon_edit_geometry_valid, _positions_meaningfully_different, update_drag_handle_positions) and
// tests/presentation/components/plan_view/components/test_drag_handler.py (ComputeNewPositionTests,
// ComputeNewPositionGapTests, DragHandlerPathAndValidityTests, DragHandlerSnapAndDeltaTests, AttachmentMovementTests).
// The compute_new_position table was produced by running the Windows function (read-only, python -B) with the real
// _snap_angle (angle 0 = Shift held or step 0, 15 = the default); the Windows tests mock _snap_angle, so their snap-angle
// calls are covered by rows with a real angle instead. Each row: {name, original, dx, dy, handle, corner_count, increment,
// angle, move_only_first_pair, free_mode, Windows result}. Results are compared with 1e-9 (Windows adds the snapped delta,
// one ulp from an absolute snap) and `libm` sin/cos differences.
#include "TestHarness.hpp"

#include "HierarchyTestSupport.hpp"

#include "Geometry/drag_rules.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace
{
    using geo_test::Family;
    using geo_test::make_condition;
    using geo_test::make_takeoff;
    using ost_domain::ConditionType;
    using ost_domain::ShapeId;

    struct Row
    {
        const char *name;
        std::vector<double> original;
        double dx, dy;
        int handle, corners;
        double increment, angle;
        bool first_pair_only, free_mode;
        std::vector<double> expected;
    };

    const std::vector<Row> &windows_rows()
    {
        static const std::vector<Row> rows = {
        {"body keeps trailing values", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0}, 3.0, 4.0, -1, 0, 0.0, 0.0, false, false, {3.0, 4.0, 13.0, 4.0, 13.0, 14.0}},
        {"body trailing 7", {0.0, 0.0, 10.0, 0.0, 5.0, 3.0, 0.5}, 3.0, 4.0, -1, 0, 0.0, 0.0, false, false, {3.0, 4.0, 13.0, 4.0, 8.0, 7.0, 0.5}},
        {"body first pair only", {0.0, 0.0, 10.0, 0.0}, 3.0, 4.0, -1, 0, 0.0, 0.0, true, false, {3.0, 4.0, 10.0, 0.0}},
        {"body inc5 first point", {1.0, 2.0, 11.0, 2.0}, 3.0, 4.0, -1, 0, 5.0, 0.0, false, false, {5.0, 5.0, 15.0, 5.0}},
        {"body inc5 each axis", {1.0, 8.0, 11.0, 8.0}, 3.0, 4.0, -1, 0, 5.0, 0.0, false, false, {5.0, 10.0, 15.0, 10.0}},
        {"body inc1 off grid", {900.25, 900.25, 1100.75, 900.25, 1100.75, 1100.75}, 0.4, 0.4, -1, 0, 1.0, 0.0, false, false, {901.0, 901.0, 1101.5, 901.0, 1101.5, 1101.5}},
        {"body inc0", {0.0, 0.0, 10.0, 0.0, 5.0, 3.0, 0.5}, 3.0, 4.0, -1, 0, 0.0, 0.0, false, false, {3.0, 4.0, 13.0, 4.0, 8.0, 7.0, 0.5}},
        {"body inc1 GEO-11", {0.4, 0.4, 2.6, 0.4}, 0.3, 0.3, -1, 0, 1.0, 0.0, false, false, {1.0, 1.0, 3.2, 1.0}},
        {"body square inc1 0.4", {0.0, 0.0, 4.0, 0.0, 4.0, 4.0, 0.0, 4.0}, 0.4, 0.4, -1, 4, 1.0, 0.0, false, false, {0.0, 0.0, 4.0, 0.0, 4.0, 4.0, 0.0, 4.0}},
        {"body area inc1 GEO-M01", {0.4, 0.4, 10.4, 0.4, 10.4, 10.4, 0.4, 10.4}, 0.3, 0.3, -1, 4, 1.0, 0.0, false, false, {1.0, 1.0, 11.0, 1.0, 11.0, 11.0, 1.0, 11.0}},
        {"body tie inc1", {0.0, 0.0, 5.0, 0.0, 5.0, 5.0}, 2.5, 0.5, -1, 0, 1.0, 0.0, false, false, {2.0, 0.0, 7.0, 0.0, 7.0, 5.0}},
        {"trap edge4 constrained", {0.0, 0.0, 10.0, 0.0, 8.0, 10.0, 2.0, 10.0}, 5.0, 2.0, 4, 4, 0.0, 0.0, false, false, {0.3999999999999999, 2.0, 9.6, 2.0, 8.0, 10.0, 2.0, 10.0}},
        {"trap edge4 inc0.5", {0.0, 0.0, 10.0, 0.0, 8.0, 10.0, 2.0, 10.0}, 5.0, 2.0, 4, 4, 0.5, 0.0, false, false, {0.5, 2.0, 9.5, 2.0, 8.0, 10.0, 2.0, 10.0}},
        {"trap edge5 constrained", {0.0, 0.0, 10.0, 0.0, 8.0, 10.0, 2.0, 10.0}, 3.0, 1.0, 5, 4, 0.0, 0.0, false, false, {0.0, 0.0, 13.200000000000001, 0.0, 11.200000000000001, 10.0, 2.0, 10.0}},
        {"tri edge3 constrained", {0.0, 0.0, 10.0, 0.0, 5.0, 10.0}, 0.0, 2.0, 3, 3, 0.0, 0.0, false, false, {1.0, 2.0, 9.0, 2.0, 5.0, 10.0}},
        {"tri edge3 free", {0.0, 0.0, 10.0, 0.0, 5.0, 10.0}, 0.0, 2.0, 3, 3, 0.0, 0.0, false, true, {0.0, 2.0, 10.0, 2.0, 5.0, 10.0}},
        {"square edge4 constrained", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 3.0, 4.0, 4, 4, 0.0, 0.0, false, false, {0.0, 4.0, 10.0, 4.0, 10.0, 10.0, 0.0, 10.0}},
        {"square 4 edge5 d(1,0.3)", {0.0, 0.0, 4.0, 0.0, 4.0, 4.0, 0.0, 4.0}, 1.0, 0.3, 5, 4, 0.0, 0.0, false, false, {0.0, 0.0, 5.0, 0.0, 5.0, 4.0, 0.0, 4.0}},
        {"square 4 edge5 free", {0.0, 0.0, 4.0, 0.0, 4.0, 4.0, 0.0, 4.0}, 1.0, 0.3, 5, 4, 0.0, 0.0, false, true, {0.0, 0.0, 5.0, 0.3, 5.0, 4.3, 0.0, 4.0}},
        {"square edge5 free d(3,4)", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 3.0, 4.0, 5, 4, 0.0, 0.0, false, true, {0.0, 0.0, 13.0, 4.0, 13.0, 14.0, 0.0, 10.0}},
        {"square edge6 free d(3,4)", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 3.0, 4.0, 6, 4, 0.0, 0.0, false, true, {0.0, 0.0, 10.0, 0.0, 13.0, 14.0, 3.0, 14.0}},
        {"square edge7 free", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 3.0, 4.0, 7, 4, 0.0, 0.0, false, true, {3.0, 4.0, 10.0, 0.0, 10.0, 10.0, 3.0, 14.0}},
        {"two corner edge free", {0.0, 0.0, 10.0, 0.0}, 1.2, 1.3, 2, 2, 0.5, 0.0, false, false, {1.0, 1.5, 11.0, 1.5}},
        {"parallel prev", {0.0, 0.0, 5.0, 0.0, 10.0, 0.0, 10.0, 10.0}, 0.0, 2.0, 5, 4, 0.0, 0.0, false, false, {0.0, 0.0, 5.0, 0.0, 10.0, 2.0, 10.0, 10.0}},
        {"parallel next", {0.0, 10.0, 10.0, 10.0, 15.0, 10.0, 15.0, 0.0}, 0.0, -2.0, 4, 4, 0.0, 0.0, false, false, {3.0, 8.0, 10.0, 10.0, 15.0, 10.0, 15.0, 0.0}},
        {"zero length edge", {0.0, 0.0, 10.0, 0.0, 10.0, 0.0, 0.0, 10.0}, 3.0, 4.0, 5, 4, 0.0, 0.0, false, false, {0.0, 0.0, 10.0, 0.0, 10.0, 0.0, 0.0, 10.0}},
        {"skewed edge5", {0.0, 0.0, 10.0, 1.0, 9.0, 10.0, 1.0, 9.0}, 2.0, 0.0, 5, 4, 0.0, 0.0, false, false, {0.0, 0.0, 11.978021978021975, 1.1978021978021975, 10.972602739726026, 10.246575342465754, 1.0, 9.0}},
        {"nano edge at threshold", {0.0, 0.0, 1e-09, 0.0, 1e-09, 10.0, 0.0, 10.0}, 0.0, 2.0, 4, 4, 0.0, 0.0, false, false, {0.0, 2.0, 1e-09, 2.0, 1e-09, 10.0, 0.0, 10.0}},
        {"nano edge below", {0.0, 0.0, 1e-10, 0.0, 1e-10, 10.0, 0.0, 10.0}, 0.0, 2.0, 4, 4, 0.0, 0.0, false, false, {0.0, 0.0, 1e-10, 0.0, 1e-10, 10.0, 0.0, 10.0}},
        {"nano edge above", {0.0, 0.0, 1.5e-09, 0.0, 1.5e-09, 10.0, 0.0, 10.0}, 0.0, 2.0, 4, 4, 0.0, 0.0, false, false, {0.0, 2.0, 1.5e-09, 2.0, 1.5e-09, 10.0, 0.0, 10.0}},
        {"RPV-M01 edge5 -15", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, -15.0, 0.0, 5, 4, 0.0, 0.0, false, false, {0.0, 0.0, -5.0, 0.0, -5.0, 10.0, 0.0, 10.0}},
        {"RPV-M01 edge4 +15 y", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 0.0, 15.0, 4, 4, 0.0, 0.0, false, false, {0.0, 15.0, 10.0, 15.0, 10.0, 10.0, 0.0, 10.0}},
        {"edge free angle15", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 3.0, 1.0, 4, 4, 0.0, 15.0, false, true, {3.0545256618536034, 0.8184576843538136, 13.054525661853603, 0.8184576843538136, 10.0, 10.0, 0.0, 10.0}},
        {"edge constrained angle15 ignored", {0.0, 0.0, 10.0, 0.0, 8.0, 10.0, 2.0, 10.0}, 5.0, 2.0, 4, 4, 0.0, 15.0, false, false, {0.3999999999999999, 2.0, 9.6, 2.0, 8.0, 10.0, 2.0, 10.0}},
        {"corner 2 angle15", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 1.0, 2.0, 2, 4, 0.0, 15.0, false, false, {0.0, 0.0, 10.0, 0.0, 10.0, 12.041594578792296, 0.0, 10.0}},
        {"corner 0 angle15", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 1.0, 2.0, 0, 4, 0.0, 15.0, false, false, {4.936709072677383e-16, 1.9377422517014509, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}},
        {"corner 2 inc5 angle0", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 1.0, 2.0, 2, 4, 5.0, 0.0, false, false, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}},
        {"corner 2 inc5 angle15", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 1.0, 2.0, 2, 4, 5.0, 15.0, false, false, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}},
        {"pentagonish corner2 angle15", {1.0, 2.0, 11.0, 3.0, 12.0, 13.0, 2.0, 14.0}, 1.0, 1.0, 2, 4, 0.0, 15.0, false, false, {1.0, 2.0, 11.0, 3.0, 13.893684893604105, 13.79937904402505, 2.0, 14.0}},
        {"pentagonish corner0 angle15", {1.0, 2.0, 11.0, 3.0, 12.0, 13.0, 2.0, 14.0}, 1.0, 1.0, 0, 4, 0.0, 15.0, false, false, {2.000000000000001, 3.0, 11.0, 3.0, 12.0, 13.0, 2.0, 14.0}},
        {"later point no angle linear 4pts", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}, 1.0, 2.0, 3, 0, 0.0, 15.0, false, false, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 1.0, 12.0}},
        {"line end1", {0.0, 0.0, 10.0, 0.0}, 2.0, 3.0, 1, 0, 0.0, 0.0, false, false, {0.0, 0.0, 12.0, 3.0}},
        {"line end0", {0.0, 0.0, 10.0, 0.0}, 2.0, 3.0, 0, 0, 0.0, 0.0, false, false, {2.0, 3.0, 10.0, 0.0}},
        {"line end1 inc5", {0.0, 0.0, 10.0, 0.0}, 2.0, 3.0, 1, 0, 5.0, 0.0, false, false, {0.0, 0.0, 10.0, 5.0}},
        {"line end1 angle15", {0.0, 0.0, 10.0, 0.0}, 2.0, 3.0, 1, 0, 0.0, 15.0, false, false, {0.0, 0.0, 11.947842624905533, 3.2014147826375825}},
        {"line end0 angle15", {0.0, 0.0, 10.0, 0.0}, 2.0, 3.0, 0, 0, 0.0, 15.0, false, false, {1.7471261224872716, 2.211350890715446, 10.0, 0.0}},
        {"slanted end0 angle15", {0.0, 2.0, 10.0, 5.0}, 1.0, 1.0, 0, 0, 0.0, 15.0, false, false, {1.0946039020805696, 2.6138063072832147, 10.0, 5.0}},
        {"slanted end1 angle15", {0.0, 2.0, 10.0, 5.0}, 1.0, 1.0, 1, 0, 0.0, 15.0, false, false, {0.0, 2.0, 11.305871932727438, 5.029399254104013}},
        {"count point inc5", {5.0, 5.0}, 2.0, 3.0, 0, 0, 5.0, 0.0, false, false, {5.0, 10.0}},
        {"curved end1 d(2,0)", {0.0, 0.0, 10.0, 0.0, 5.0, 2.0}, 2.0, 0.0, 1, 0, 0.0, 0.0, false, false, {0.0, 0.0, 12.0, 0.0, 6.0, 2.0}},
        {"curved end0 d(0,3)", {0.0, 0.0, 10.0, 0.0, 5.0, 2.0}, 0.0, 3.0, 0, 0, 0.0, 0.0, false, false, {0.0, 3.0, 10.0, 0.0, 5.57469577113269, 3.415652570442303}},
        {"curved end1 d(0,6)", {0.0, 0.0, 10.0, 0.0, 5.0, 2.0}, 0.0, 6.0, 1, 0, 0.0, 0.0, false, false, {0.0, 0.0, 10.0, 6.0, 3.971008489144947, 4.714985851425088}},
        {"curved end0 angle15 d(0,3)", {0.0, 0.0, 10.0, 0.0, 5.0, 2.0}, 0.0, 3.0, 0, 0, 0.0, 15.0, false, false, {-0.0845616913305598, 2.7021501612138636, 10.0, 0.0, 5.4753572445397625, 3.2829267331850684}},
        {"curved end1 d(0,10) cp(5,3)", {0.0, 0.0, 10.0, 0.0, 5.0, 3.0}, 0.0, 10.0, 1, 0, 0.0, 0.0, false, false, {0.0, 0.0, 10.0, 10.0, 2.8786796564403576, 7.121320343559642}},
        {"curved end0 d(0,-4) cp(5,3)", {0.0, 0.0, 10.0, 0.0, 5.0, 3.0}, 0.0, -4.0, 0, 0, 0.0, 0.0, false, false, {0.0, -4.0, 10.0, 0.0, 3.885827970937689, 0.7854300726557781}},
        {"curved end1 onto other", {0.0, 0.0, 10.0, 0.0, 5.0, 3.0}, -10.0, 0.0, 1, 0, 0.0, 0.0, false, false, {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        {"curved degenerate chord", {4.0, 4.0, 4.0, 4.0, 7.0, 9.0}, 6.0, 0.0, 1, 0, 0.0, 0.0, false, false, {4.0, 4.0, 10.0, 4.0, 7.0, 4.0}},
        {"five value end1", {0.0, 0.0, 10.0, 0.0, 7.0}, 2.0, 3.0, 1, 0, 0.0, 0.0, false, false, {0.0, 0.0, 12.0, 3.0, 7.0}},
        {"curved 7 value end1", {0.0, 0.0, 10.0, 0.0, 5.0, 3.0, 0.5}, 0.0, 10.0, 1, 0, 0.0, 0.0, false, false, {0.0, 0.0, 10.0, 10.0, 2.8786796564403576, 7.121320343559642, 0.5}},
        {"curved slanted end1", {1.0, 2.0, 7.0, 10.0, 5.0, 3.0}, -3.0, 4.0, 1, 0, 0.0, 0.0, false, false, {1.0, 2.0, 4.0, 14.0, 5.022370500377862, 7.369407374905534}},
        {"curved slanted end0", {1.0, 2.0, 7.0, 10.0, 5.0, 3.0}, -3.0, 4.0, 0, 0, 0.0, 0.0, false, false, {-2.0, 6.0, 7.0, 10.0, 3.555960011738964, 5.624089973587331}},
        {"curved chord threshold old", {0.0, 0.0, 1e-09, 0.0, 5.0, 3.0}, 5.0, 0.0, 1, 0, 0.0, 0.0, false, false, {0.0, 0.0, 5.000000001, 0.0, 2.5000000005, 0.0}},
        {"curved chord threshold new", {-5.0, 0.0, 1e-09, 0.0, 5.0, 3.0}, 5.0, 0.0, 0, 0, 0.0, 0.0, false, false, {0.0, 0.0, 1e-09, 0.0, 5e-10, 0.0}},
        {"curved chord at threshold d0", {0.0, 0.0, 1e-09, 0.0, 5.0, 3.0}, 0.0, 0.0, 0, 0, 0.0, 0.0, false, false, {0.0, 0.0, 1e-09, 0.0, 5e-10, 0.0}},
        {"curved chord above d0", {0.0, 0.0, 1.5e-09, 0.0, 5.0, 3.0}, 0.0, 0.0, 0, 0, 0.0, 0.0, false, false, {0.0, 0.0, 1.5e-09, 0.0, 7.5e-10, 3.0}},
        {"curved end1 inc1 angle15", {0.0, 0.0, 10.0, 0.0, 5.0, 3.0}, 2.3, 4.4, 1, 0, 1.0, 15.0, false, false, {0.0, 0.0, 12.218102647414414, 3.2738307374152544, 5.332594188399645, 4.534692847574832}},
        {"out of range handle 3", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0}, 1.0, 2.0, 3, 0, 0.0, 0.0, false, false, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0}},
        {"out of range handle -2", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0}, 1.0, 2.0, -2, 0, 0.0, 0.0, false, false, {0.0, 0.0, 10.0, 0.0, 10.0, 10.0}},
        };
        return rows;
    }

    std::vector<double> drag(const std::vector<double> &pos, double dx, double dy, int handle, int corners,
                             double inc = 0.0, double angle = 0.0, bool first = false, bool free_mode = false)
    {
        return ost_drag::compute_new_position(pos, dx, dy, handle, corners, inc, angle, first, free_mode);
    }

    const std::vector<double> kSquare = {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0};
}

// Every Windows result of the table (ComputeNewPositionTests / ComputeNewPositionGapTests and the item plans of RPV-08, RPV-09,
// RPV-M03, GEO-10, GEO-11, SNP-04, SNP-M01): same inputs, same output within 1e-9.
TEST(DragComputeNewPosition, MatchesTheWindowsFunction)
{
    for (const Row &row : windows_rows())
    {
        SCOPED_CONTEXT(std::string("row ") + row.name);
        const std::vector<double> input = row.original;
        const auto result = ost_drag::compute_new_position(input, row.dx, row.dy, row.handle, row.corners,
                                                           row.increment, row.angle, row.first_pair_only, row.free_mode);
        CHECK_EQ(input, row.original); // Port of test_body_drag_does_not_modify_its_input
        REQUIRE_EQ(result.size(), row.expected.size());
        for (size_t i = 0; i < result.size(); ++i)
            CHECK_NEAR(result[i], row.expected[i], 1e-9);
    }
}

// RPV-08 plan: a whole-shape drag keeps the size of an off-grid shape (iOS rounded every vertex: [5,5,15,5] for inc 5, pos
// [1,2,12,2], delta (3,4); Windows [5,5,16,5]).
TEST(DragComputeNewPosition, BodyDragSnapsTheFirstPointAndKeepsTheShapeSize)
{
    const auto r = drag({1.0, 2.0, 12.0, 2.0}, 3.0, 4.0, -1, 0, 5.0);
    CHECK_NEAR(r[0], 5.0, 1e-9);
    CHECK_NEAR(r[1], 5.0, 1e-9);
    CHECK_NEAR(r[2], 16.0, 1e-9);
    CHECK_NEAR(r[3], 5.0, 1e-9);
    // inc 0, pos [0,0,10,0,5,3,0.5], delta (3,4) -> [3,4,13,4,8,7,0.5]
    const auto raw = drag({0.0, 0.0, 10.0, 0.0, 5.0, 3.0, 0.5}, 3.0, 4.0, -1, 0);
    CHECK_EQ(raw, (std::vector<double>{3.0, 4.0, 13.0, 4.0, 8.0, 7.0, 0.5}));
    // edge lengths are unchanged for any delta: width and height of the dragged off-grid rectangle
    for (double d : {0.1, 0.4, 0.5, 0.9, 1.3})
    {
        const auto moved = drag({0.4, 0.4, 2.6, 0.4, 2.6, 3.9, 0.4, 3.9}, d, d, -1, 4, 1.0);
        CHECK_NEAR(moved[2] - moved[0], 2.2, 1e-9);
        CHECK_NEAR(moved[5] - moved[1], 3.5, 1e-9);
    }
}

// RPV-09 plan: the default edge drag of an area is the constrained one; the free one (Shift held on Windows; iOS: angle
// snapping off) moves both corners by the angle-snapped delta; a 2-corner shape is always free. iOS returned [5,2,15,2,8,10,2,10]
// for the trapezoid.
TEST(DragComputeNewPosition, AreaEdgeDragIsConstrainedUnlessFree)
{
    const std::vector<double> trapezoid = {0.0, 0.0, 10.0, 0.0, 8.0, 10.0, 2.0, 10.0};
    const auto constrained = drag(trapezoid, 5.0, 2.0, 4, 4);
    CHECK_NEAR(constrained[0], 0.4, 1e-9);
    CHECK_NEAR(constrained[2], 9.6, 1e-9);
    CHECK_NEAR(constrained[1], 2.0, 1e-9);
    const auto free_move = drag(trapezoid, 5.0, 2.0, 4, 4, 0.0, 0.0, false, true);
    CHECK_EQ(free_move, (std::vector<double>{5.0, 2.0, 15.0, 2.0, 8.0, 10.0, 2.0, 10.0}));
    const auto line_like = drag({0.0, 0.0, 10.0, 0.0}, 1.2, 1.3, 2, 2, 0.5);
    CHECK_EQ(line_like, (std::vector<double>{1.0, 1.5, 11.0, 1.5}));
}

// RPV-M03 plan: pos [0,0,10,0,5,2], inc 0, no angle snap. Handle 1, delta (2,0): Windows [0,0,12,0,6,2] (iOS [0,0,12,0,5,2]);
// handle 0, delta (0,3): [0,3,10,0,5.5747,3.4157]; handle 1, delta (0,6): [0,0,10,6,3.9710,4.7150] (iOS rejected it:
// winding flips). With angle snap ON the end snaps against the OTHER END, not against the control point.
TEST(DragComputeNewPosition, CurvedLinearEndHandlesKeepTheControlPointOffset)
{
    const std::vector<double> curved = {0.0, 0.0, 10.0, 0.0, 5.0, 2.0};
    CHECK_EQ(drag(curved, 2.0, 0.0, 1, 0), (std::vector<double>{0.0, 0.0, 12.0, 0.0, 6.0, 2.0}));
    const auto b = drag(curved, 0.0, 3.0, 0, 0);
    CHECK_NEAR(b[1], 3.0, 1e-9);
    CHECK_NEAR(b[4], 5.5746957711326909, 1e-9);
    CHECK_NEAR(b[5], 3.4156525704423028, 1e-9);
    const auto c = drag(curved, 0.0, 6.0, 1, 0);
    CHECK_NEAR(c[3], 6.0, 1e-9);
    CHECK_NEAR(c[4], 3.971008489144947, 1e-9);
    CHECK_NEAR(c[5], 4.714985851425088, 1e-9);
    // the winding flip of the 3-point polygon reading no longer rejects it: the shape is not an area
    CHECK_EQ(c.size(), static_cast<size_t>(6));
}

// RPV-22 decision b: the centre (control) handle of a curved linear is not ported; a drag on it leaves the position unchanged
// instead of overwriting the control point as a plain vertex.
TEST(DragComputeNewPosition, CurvedLinearCentreHandleIsNotPorted)
{
    const std::vector<double> curved = {0.0, 0.0, 6.0, 8.0, 5.0, 3.0};
    CHECK_EQ(drag(curved, 5.0, 10.0, 2, 0), curved);
    const std::vector<double> bulge = {0.0, 0.0, 6.0, 8.0, 5.0, 3.0, 0.5};
    CHECK_EQ(drag(bulge, 5.0, 10.0, 2, 0), bulge);
}

// Degenerate input is defined explicitly (Windows raises IndexError): nothing moves.
TEST(DragComputeNewPosition, DegenerateInputsLeaveThePositionUnchanged)
{
    CHECK_EQ(drag({}, 1.0, 2.0, -1, 0), std::vector<double>{});
    CHECK_EQ(drag({7.0}, 1.0, 2.0, -1, 0), std::vector<double>{7.0});
    CHECK_EQ(drag(kSquare, 1.0, 2.0, 8, 4), kSquare);  // edge handle beyond the last edge
    CHECK_EQ(drag(kSquare, 1.0, 2.0, 99, 4), kSquare);
    CHECK_EQ(drag(kSquare, 1.0, 2.0, -2, 4), kSquare);
}

// Port of test_polygon_edit_requires_three_valid_points_and_stable_winding.
TEST(DragPolygonEdit, RequiresThreeValidPointsAndStableWinding)
{
    using ost_drag::polygon_edit_geometry_valid;
    const std::vector<double> square = {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0};
    const std::vector<double> reversed = {0.0, 10.0, 10.0, 10.0, 10.0, 0.0, 0.0, 0.0};
    const std::vector<double> bow_tie = {0.0, 0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 10.0};
    CHECK(polygon_edit_geometry_valid(square, {}));
    CHECK_FALSE(polygon_edit_geometry_valid({0.0, 0.0, 10.0, 0.0}, {}));
    CHECK_FALSE(polygon_edit_geometry_valid(bow_tie, {}));
    CHECK(polygon_edit_geometry_valid(square, square));
    CHECK_FALSE(polygon_edit_geometry_valid(reversed, square));
    CHECK(polygon_edit_geometry_valid(reversed, reversed));
    CHECK_FALSE(polygon_edit_geometry_valid(square, reversed));
}

// Port of test_polygon_edit_ignores_winding_when_the_previous_area_is_zero,
// test_polygon_edit_winding_check_uses_the_sign_of_each_area_not_a_threshold and
// test_polygon_edit_accepts_a_valid_triangle_and_reports_plain_booleans.
TEST(DragPolygonEdit, WindingUsesTheSignOfEachAreaNotAThreshold)
{
    using ost_drag::polygon_edit_geometry_valid;
    const std::vector<double> square = {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0};
    CHECK(polygon_edit_geometry_valid(square, {0.0, 0.0, 5.0, 0.0, 10.0, 0.0})); // previous area 0: no winding check
    const std::vector<double> tiny_ccw = {0.0, 0.0, 1.0, 0.0, 0.0, 1.0};
    const std::vector<double> tiny_cw = {0.0, 0.0, 0.0, 1.0, 1.0, 0.0};
    const std::vector<double> big_cw = {0.0, 0.0, 0.0, 10.0, 10.0, 10.0, 10.0, 0.0};
    CHECK_FALSE(polygon_edit_geometry_valid(tiny_cw, tiny_ccw));
    CHECK(polygon_edit_geometry_valid(tiny_ccw, tiny_ccw));
    CHECK_FALSE(polygon_edit_geometry_valid(square, big_cw));
    const std::vector<double> triangle = {0.0, 0.0, 10.0, 0.0, 5.0, 8.0};
    CHECK(polygon_edit_geometry_valid(triangle, {}));
    CHECK_FALSE(polygon_edit_geometry_valid(triangle, {0.0, 0.0, 5.0, 8.0, 10.0, 0.0}));
}

// Port of test_positions_meaningfully_different_uses_a_nano_unit_tolerance (UND-11).
TEST(DragMeaningfullyDifferent, UsesANanoUnitAbsoluteTolerance)
{
    using ost_drag::positions_meaningfully_different;
    CHECK_FALSE(positions_meaningfully_different({}, {}));
    CHECK_FALSE(positions_meaningfully_different({1.0, 2.0}, {1.0, 2.0}));
    CHECK_FALSE(positions_meaningfully_different({1.0, 2.0}, {1.0 + 5e-10, 2.0 - 5e-10}));
    CHECK(positions_meaningfully_different({1.0, 2.0}, {1.0, 2.0 + 1.5e-9}));
    CHECK(positions_meaningfully_different({1.0, 2.0}, {1.0, 3.0}));
    CHECK(positions_meaningfully_different({1.0, 2.0}, {0.0, 2.0}));
    CHECK(positions_meaningfully_different({1.0, 2.0}, {1.0, 2.0, 3.0}));
    CHECK(positions_meaningfully_different({1.0, 2.0, 3.0}, {1.0, 2.0}));
    CHECK(positions_meaningfully_different({1e9}, {1e9 + 1e-3})); // the tolerance is absolute, not relative
}

// Port of test_translate_position_moves_pairs_and_leaves_trailing_values.
TEST(DragTranslate, MovesPairsAndLeavesTrailingValues)
{
    const std::vector<double> in = {1.0, 2.0, 3.0, 4.0, 9.0};
    CHECK_EQ(ost_drag::translate_position(in, 10.0, 20.0), (std::vector<double>{11.0, 22.0, 13.0, 24.0, 9.0}));
    CHECK_EQ(in, (std::vector<double>{1.0, 2.0, 3.0, 4.0, 9.0}));
    CHECK_EQ(ost_drag::translate_position({1.0, 2.0}, 10.0, 20.0), (std::vector<double>{11.0, 22.0}));
}

// ---------------------------------------------------------------------------------------------------------------
// drag_candidate_valid (GEO-M02, RPV-M01): the validity chain of update_drag_handle_positions
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    Family parent_with_hole()
    {
        Family f;
        f.add_condition(make_condition("ca", ConditionType::Area));
        f.add_condition(make_condition("cl", ConditionType::Linear));
        f.add_condition(make_condition("cat", ConditionType::Attachment, ShapeId::Square, 2.0, 2.0));
        f.add(make_takeoff("parent", "ca", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}));
        f.add(make_takeoff("hole", "ca", {2.0, 2.0, 4.0, 2.0, 4.0, 4.0, 2.0, 4.0}, "parent"));
        return f;
    }

    bool valid(const Family &f, const std::string &uid, const std::vector<double> &candidate, int handle,
               const std::vector<double> &last)
    {
        const auto model = f.model();
        const ost_domain::Takeoff *t = model.find_takeoff(uid);
        REQUIRE(t != nullptr);
        const ost_domain::Condition *c = model.find_condition(t->condition_uid);
        REQUIRE(c != nullptr);
        return ost_drag::drag_candidate_valid(model, *t, *c, candidate, handle, last);
    }
}

// GEO-M02 plan: parent [0,0,10,0,10,10,0,10] + hole [2,2,4,2,4,4,2,4]: dragging parent corner 2 to (3,3) must be rejected (the hole
// would leave the parent), to (8,8) accepted.
TEST(DragCandidateValid, ParentCornerThatWouldExpelAHoleIsRejected)
{
    const Family f = parent_with_hole();
    const std::vector<double> last = f.takeoffs[0].position;
    CHECK_FALSE(valid(f, "parent", {0.0, 0.0, 10.0, 0.0, 3.0, 3.0, 0.0, 10.0}, 2, last));
    CHECK(valid(f, "parent", {0.0, 0.0, 10.0, 0.0, 8.0, 8.0, 0.0, 10.0}, 2, last));
    // an edge handle (index 4 + i) is a vertex edit too
    CHECK_FALSE(valid(f, "parent", {0.0, 0.0, 3.0, 0.0, 3.0, 3.0, 0.0, 10.0}, 5, last));
    // a whole-shape drag of the parent moves its children along: no containment check
    CHECK(valid(f, "parent", {100.0, 100.0, 110.0, 100.0, 110.0, 110.0, 100.0, 110.0}, -1, last));
}

// Port of AttachmentMovementTests::test_parent_resize_cannot_exclude_attachment (parent 10 x 10, attachment at (5,5)): a parent
// shrunk to 3 x 3 is rejected, to 8 x 8 and enlarged to 12 x 12 accepted.
TEST(DragCandidateValid, ParentResizeCannotExcludeAnAttachment)
{
    Family f;
    f.add_condition(make_condition("ca", ConditionType::Area));
    f.add_condition(make_condition("cat", ConditionType::Attachment, ShapeId::Square, 2.0, 2.0));
    f.add(make_takeoff("parent", "ca", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}));
    f.add(make_takeoff("att", "cat", {5.0, 5.0}, "parent"));
    const std::vector<double> last = f.takeoffs[0].position;
    CHECK_FALSE(valid(f, "parent", {0.0, 0.0, 3.0, 0.0, 3.0, 3.0, 0.0, 3.0}, 2, last));
    CHECK(valid(f, "parent", {0.0, 0.0, 8.0, 0.0, 8.0, 8.0, 0.0, 8.0}, 2, last));
    CHECK(valid(f, "parent", {0.0, 0.0, 12.0, 0.0, 12.0, 12.0, 0.0, 12.0}, 2, last));
}

// Port of AttachmentMovementTests::test_mouse_preview_collides_at_every_edge: an attachment dragged across any edge of its parent
// is rejected, inside accepted.
TEST(DragCandidateValid, AttachmentMustStayInsideItsParent)
{
    Family f;
    f.add_condition(make_condition("ca", ConditionType::Area));
    f.add_condition(make_condition("cat", ConditionType::Attachment, ShapeId::Square, 2.0, 2.0));
    f.add(make_takeoff("parent", "ca", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}));
    f.add(make_takeoff("att", "cat", {5.0, 5.0}, "parent"));
    for (const std::vector<double> &position :
         std::vector<std::vector<double>>{{-1.0, 5.0}, {11.0, 5.0}, {5.0, -1.0}, {5.0, 11.0}})
    {
        SCOPED_CONTEXT("candidate " + std::to_string(position[0]) + "," + std::to_string(position[1]));
        CHECK_FALSE(valid(f, "att", position, -1, {5.0, 5.0}));
    }
    CHECK(valid(f, "att", {6.0, 4.0}, -1, {5.0, 5.0}));
}

// Port of AttachmentMovementTests::test_nested_backout_vertex_edit_cannot_exclude_its_child: a backout whose vertex is dragged
// to (5,5) would leave its nested hole outside (rejected); (8.5, 8.5) still contains it.
TEST(DragCandidateValid, NestedBackoutCannotExcludeItsChild)
{
    Family f;
    f.add_condition(make_condition("ca", ConditionType::Area));
    f.add(make_takeoff("parent", "ca", {0.0, 0.0, 10.0, 0.0, 10.0, 10.0, 0.0, 10.0}));
    f.add(make_takeoff("backout", "ca", {1.0, 1.0, 9.0, 1.0, 9.0, 9.0, 1.0, 9.0}, "parent"));
    f.add(make_takeoff("nested", "ca", {7.0, 7.0, 8.0, 7.0, 8.0, 8.0, 7.0, 8.0}, "backout"));
    const std::vector<double> original = f.takeoffs[1].position;
    CHECK_FALSE(valid(f, "backout", {1.0, 1.0, 9.0, 1.0, 5.0, 5.0, 1.0, 9.0}, 2, original));
    CHECK(valid(f, "backout", {1.0, 1.0, 9.0, 1.0, 8.5, 8.5, 1.0, 9.0}, 2, original));
}

// Windows runs _validate_hole_position for every drag of a hole (also the whole-shape one): a hole stays inside its parent.
TEST(DragCandidateValid, HoleStaysInsideItsParentForEveryKindOfDrag)
{
    const Family f = parent_with_hole();
    const std::vector<double> last = f.takeoffs[1].position;
    CHECK(valid(f, "hole", {8.0, 8.0, 10.0, 8.0, 10.0, 10.0, 8.0, 10.0}, -1, last));
    CHECK_FALSE(valid(f, "hole", {9.0, 9.0, 11.0, 9.0, 11.0, 11.0, 9.0, 11.0}, -1, last));
    CHECK_FALSE(valid(f, "hole", {2.0, 2.0, 4.0, 2.0, 14.0, 14.0, 2.0, 4.0}, 2, last));
    CHECK(valid(f, "hole", {2.0, 2.0, 4.0, 2.0, 5.0, 5.0, 2.0, 4.0}, 2, last));
}

// RPV-M01 plan: square [0,0,10,0,10,10,0,10] (signed_area 200). Edge handle 5 (corners 1,2), delta (-15,0): the constrained
// result [0,0,-5,0,-5,10,0,10] has signed_area -100 and is rejected; edge handle 4 (corners 0,1), delta (0,15): the proposal
// [0,15,10,15,10,10,0,10] is rejected. A corner drag is validated the same way, and a linear never is.
TEST(DragCandidateValid, EdgeDragsAreValidatedLikeCornerDrags)
{
    Family f;
    f.add_condition(make_condition("ca", ConditionType::Area));
    f.add_condition(make_condition("cl", ConditionType::Linear));
    f.add(make_takeoff("sq", "ca", kSquare));
    f.add(make_takeoff("ln", "cl", {0.0, 0.0, 10.0, 0.0}));
    f.add(make_takeoff("tri", "ca", {0.0, 0.0, 10.0, 0.0, 5.0, 10.0}));
    CHECK_FALSE(valid(f, "sq", {0.0, 0.0, -5.0, 0.0, -5.0, 10.0, 0.0, 10.0}, 5, kSquare));
    CHECK_FALSE(valid(f, "sq", {0.0, 15.0, 10.0, 15.0, 10.0, 10.0, 0.0, 10.0}, 4, kSquare));
    CHECK(valid(f, "sq", {0.0, 4.0, 10.0, 4.0, 10.0, 10.0, 0.0, 10.0}, 4, kSquare));
    CHECK_FALSE(valid(f, "sq", {0.0, 0.0, 10.0, 10.0, 10.0, 0.0, 0.0, 10.0}, 1, kSquare)); // bow tie by a corner
    // a corner dragged through the opposite edge of a triangle gives a valid polygon with the other winding: rejected
    const std::vector<double> triangle = {0.0, 0.0, 10.0, 0.0, 5.0, 10.0};
    CHECK_FALSE(valid(f, "tri", {0.0, 0.0, 10.0, 0.0, 5.0, -10.0}, 2, triangle));
    CHECK(valid(f, "tri", {0.0, 0.0, 10.0, 0.0, 5.0, 5.0}, 2, triangle));
    CHECK(valid(f, "sq", {5.0, 5.0, 15.0, 5.0, 15.0, 15.0, 5.0, 15.0}, -1, kSquare));
    // a linear is never rejected, even when it is dragged onto itself
    CHECK(valid(f, "ln", {0.0, 0.0, 0.0, 0.0}, 1, {0.0, 0.0, 10.0, 0.0}));
}
