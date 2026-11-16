// RPV-02 (alias GEO-08): canvas -> OST delta with flips and rotation; plus the page-point maps extracted from
// PlanView+Placement.mm (_canvasToOst / _ostToCanvas, RPV-03 / GEO-08) so that GEO-06, RPV-02 and LP-16 share ONE
// display model (the Windows one, zoom_handler.py _get_page_transform):
//   the displayed page is the unrotated page flipped (flip_x / flip_y about the page centre) and then rotated
//   counter-clockwise by `rotation`; the canvas is the OUTPUT page (W and H swapped for 90/270), top-left origin, Y down.
//
// Windows oracle: drag_handler.scene_to_ost_delta (inverse of the page QTransform, linear part, times
// scale_ratio / (72 * view_scale)), selection_manager._scene_pos_to_ost and view._ost_to_scene_pos. The tables were
// produced by running those Windows functions on PySide6 QTransform (verbatim copy of _get_page_transform, read-only);
// canvas coordinates are the scene coordinates minus the top-left of the mapped page rectangle.
// Rows: delta {rotation, flip_x, flip_y, dx, dy, ost_dx, ost_dy}; points {rotation, flip_x, flip_y, in_x, in_y, out_x, out_y}.
#include "GeometryTestUtil.hpp"

#include <string>
#include <vector>

using geo_test::case_name;
using geo_test::check_vec2;
using geo_test::pdf;
using geo_test::rel_tol;
using geo_test::throws_invalid_argument;
using ost_types::Vec2;

namespace
{
    struct Row
    {
        int rot;
        bool fx, fy;
        double a, b, ea, eb;
    };

    // Windows scene_to_ost_delta on QTransform pages (612x792 scene; the translation of the transform cancels),
    // scale_ratio 48 (sf1 = 1, sf2 = 48): dx, dy -> ost delta. Quarter turns incl. -90, 450, 360, -180, -270.
    const Row kDeltaQuarter[] = {
        {0, false, false, 10.0, 0.0, 6.666666666666666, 0.0},
        {0, false, true, 10.0, 0.0, 6.666666666666666, 0.0},
        {0, true, false, 10.0, 0.0, -6.666666666666666, 0.0},
        {0, true, true, 10.0, 0.0, -6.666666666666666, 0.0},
        {90, false, false, 10.0, 0.0, 0.0, 6.666666666666666},
        {90, false, true, 10.0, 0.0, 0.0, -6.666666666666666},
        {90, true, false, 10.0, 0.0, 0.0, 6.666666666666666},
        {90, true, true, 10.0, 0.0, 0.0, -6.666666666666666},
        {180, false, false, 10.0, 0.0, -6.666666666666666, 0.0},
        {180, false, true, 10.0, 0.0, -6.666666666666666, 0.0},
        {180, true, false, 10.0, 0.0, 6.666666666666666, 0.0},
        {180, true, true, 10.0, 0.0, 6.666666666666666, 0.0},
        {270, false, false, 10.0, 0.0, 0.0, -6.666666666666666},
        {270, false, true, 10.0, 0.0, 0.0, 6.666666666666666},
        {270, true, false, 10.0, 0.0, 0.0, -6.666666666666666},
        {270, true, true, 10.0, 0.0, 0.0, 6.666666666666666},
        {-90, false, false, 10.0, 0.0, 0.0, -6.666666666666666},
        {-90, false, true, 10.0, 0.0, 0.0, 6.666666666666666},
        {-90, true, false, 10.0, 0.0, 0.0, -6.666666666666666},
        {-90, true, true, 10.0, 0.0, 0.0, 6.666666666666666},
        {450, false, false, 10.0, 0.0, 0.0, 6.666666666666666},
        {450, false, true, 10.0, 0.0, 0.0, -6.666666666666666},
        {450, true, false, 10.0, 0.0, 0.0, 6.666666666666666},
        {450, true, true, 10.0, 0.0, 0.0, -6.666666666666666},
        {360, false, false, 10.0, 0.0, 6.666666666666666, 0.0},
        {360, false, true, 10.0, 0.0, 6.666666666666666, 0.0},
        {360, true, false, 10.0, 0.0, -6.666666666666666, 0.0},
        {360, true, true, 10.0, 0.0, -6.666666666666666, 0.0},
        {-180, false, false, 10.0, 0.0, -6.666666666666666, 0.0},
        {-180, false, true, 10.0, 0.0, -6.666666666666666, 0.0},
        {-180, true, false, 10.0, 0.0, 6.666666666666666, 0.0},
        {-180, true, true, 10.0, 0.0, 6.666666666666666, 0.0},
        {-270, false, false, 10.0, 0.0, 0.0, 6.666666666666666},
        {-270, false, true, 10.0, 0.0, 0.0, -6.666666666666666},
        {-270, true, false, 10.0, 0.0, 0.0, 6.666666666666666},
        {-270, true, true, 10.0, 0.0, 0.0, -6.666666666666666},
        {0, false, false, 0.0, 10.0, 0.0, 6.666666666666666},
        {0, false, true, 0.0, 10.0, 0.0, -6.666666666666666},
        {0, true, false, 0.0, 10.0, 0.0, 6.666666666666666},
        {0, true, true, 0.0, 10.0, 0.0, -6.666666666666666},
        {90, false, false, 0.0, 10.0, -6.666666666666666, 0.0},
        {90, false, true, 0.0, 10.0, -6.666666666666666, 0.0},
        {90, true, false, 0.0, 10.0, 6.666666666666666, 0.0},
        {90, true, true, 0.0, 10.0, 6.666666666666666, 0.0},
        {180, false, false, 0.0, 10.0, 0.0, -6.666666666666666},
        {180, false, true, 0.0, 10.0, 0.0, 6.666666666666666},
        {180, true, false, 0.0, 10.0, 0.0, -6.666666666666666},
        {180, true, true, 0.0, 10.0, 0.0, 6.666666666666666},
        {270, false, false, 0.0, 10.0, 6.666666666666666, 0.0},
        {270, false, true, 0.0, 10.0, 6.666666666666666, 0.0},
        {270, true, false, 0.0, 10.0, -6.666666666666666, 0.0},
        {270, true, true, 0.0, 10.0, -6.666666666666666, 0.0},
        {-90, false, false, 0.0, 10.0, 6.666666666666666, 0.0},
        {-90, false, true, 0.0, 10.0, 6.666666666666666, 0.0},
        {-90, true, false, 0.0, 10.0, -6.666666666666666, 0.0},
        {-90, true, true, 0.0, 10.0, -6.666666666666666, 0.0},
        {450, false, false, 0.0, 10.0, -6.666666666666666, 0.0},
        {450, false, true, 0.0, 10.0, -6.666666666666666, 0.0},
        {450, true, false, 0.0, 10.0, 6.666666666666666, 0.0},
        {450, true, true, 0.0, 10.0, 6.666666666666666, 0.0},
        {360, false, false, 0.0, 10.0, 0.0, 6.666666666666666},
        {360, false, true, 0.0, 10.0, 0.0, -6.666666666666666},
        {360, true, false, 0.0, 10.0, 0.0, 6.666666666666666},
        {360, true, true, 0.0, 10.0, 0.0, -6.666666666666666},
        {-180, false, false, 0.0, 10.0, 0.0, -6.666666666666666},
        {-180, false, true, 0.0, 10.0, 0.0, 6.666666666666666},
        {-180, true, false, 0.0, 10.0, 0.0, -6.666666666666666},
        {-180, true, true, 0.0, 10.0, 0.0, 6.666666666666666},
        {-270, false, false, 0.0, 10.0, -6.666666666666666, 0.0},
        {-270, false, true, 0.0, 10.0, -6.666666666666666, 0.0},
        {-270, true, false, 0.0, 10.0, 6.666666666666666, 0.0},
        {-270, true, true, 0.0, 10.0, 6.666666666666666, 0.0},
        {0, false, false, 10.0, 20.0, 6.666666666666666, 13.333333333333332},
        {0, false, true, 10.0, 20.0, 6.666666666666666, -13.333333333333332},
        {0, true, false, 10.0, 20.0, -6.666666666666666, 13.333333333333332},
        {0, true, true, 10.0, 20.0, -6.666666666666666, -13.333333333333332},
        {90, false, false, 10.0, 20.0, -13.333333333333332, 6.666666666666666},
        {90, false, true, 10.0, 20.0, -13.333333333333332, -6.666666666666666},
        {90, true, false, 10.0, 20.0, 13.333333333333332, 6.666666666666666},
        {90, true, true, 10.0, 20.0, 13.333333333333332, -6.666666666666666},
        {180, false, false, 10.0, 20.0, -6.666666666666666, -13.333333333333332},
        {180, false, true, 10.0, 20.0, -6.666666666666666, 13.333333333333332},
        {180, true, false, 10.0, 20.0, 6.666666666666666, -13.333333333333332},
        {180, true, true, 10.0, 20.0, 6.666666666666666, 13.333333333333332},
        {270, false, false, 10.0, 20.0, 13.333333333333332, -6.666666666666666},
        {270, false, true, 10.0, 20.0, 13.333333333333332, 6.666666666666666},
        {270, true, false, 10.0, 20.0, -13.333333333333332, -6.666666666666666},
        {270, true, true, 10.0, 20.0, -13.333333333333332, 6.666666666666666},
        {-90, false, false, 10.0, 20.0, 13.333333333333332, -6.666666666666666},
        {-90, false, true, 10.0, 20.0, 13.333333333333332, 6.666666666666666},
        {-90, true, false, 10.0, 20.0, -13.333333333333332, -6.666666666666666},
        {-90, true, true, 10.0, 20.0, -13.333333333333332, 6.666666666666666},
        {450, false, false, 10.0, 20.0, -13.333333333333332, 6.666666666666666},
        {450, false, true, 10.0, 20.0, -13.333333333333332, -6.666666666666666},
        {450, true, false, 10.0, 20.0, 13.333333333333332, 6.666666666666666},
        {450, true, true, 10.0, 20.0, 13.333333333333332, -6.666666666666666},
        {360, false, false, 10.0, 20.0, 6.666666666666666, 13.333333333333332},
        {360, false, true, 10.0, 20.0, 6.666666666666666, -13.333333333333332},
        {360, true, false, 10.0, 20.0, -6.666666666666666, 13.333333333333332},
        {360, true, true, 10.0, 20.0, -6.666666666666666, -13.333333333333332},
        {-180, false, false, 10.0, 20.0, -6.666666666666666, -13.333333333333332},
        {-180, false, true, 10.0, 20.0, -6.666666666666666, 13.333333333333332},
        {-180, true, false, 10.0, 20.0, 6.666666666666666, -13.333333333333332},
        {-180, true, true, 10.0, 20.0, 6.666666666666666, 13.333333333333332},
        {-270, false, false, 10.0, 20.0, -13.333333333333332, 6.666666666666666},
        {-270, false, true, 10.0, 20.0, -13.333333333333332, -6.666666666666666},
        {-270, true, false, 10.0, 20.0, 13.333333333333332, 6.666666666666666},
        {-270, true, true, 10.0, 20.0, 13.333333333333332, -6.666666666666666},
        {0, false, false, -7.5, 3.25, -5.0, 2.1666666666666665},
        {0, false, true, -7.5, 3.25, -5.0, -2.1666666666666665},
        {0, true, false, -7.5, 3.25, 5.0, 2.1666666666666665},
        {0, true, true, -7.5, 3.25, 5.0, -2.1666666666666665},
        {90, false, false, -7.5, 3.25, -2.1666666666666665, -5.0},
        {90, false, true, -7.5, 3.25, -2.1666666666666665, 5.0},
        {90, true, false, -7.5, 3.25, 2.1666666666666665, -5.0},
        {90, true, true, -7.5, 3.25, 2.1666666666666665, 5.0},
        {180, false, false, -7.5, 3.25, 5.0, -2.1666666666666665},
        {180, false, true, -7.5, 3.25, 5.0, 2.1666666666666665},
        {180, true, false, -7.5, 3.25, -5.0, -2.1666666666666665},
        {180, true, true, -7.5, 3.25, -5.0, 2.1666666666666665},
        {270, false, false, -7.5, 3.25, 2.1666666666666665, 5.0},
        {270, false, true, -7.5, 3.25, 2.1666666666666665, -5.0},
        {270, true, false, -7.5, 3.25, -2.1666666666666665, 5.0},
        {270, true, true, -7.5, 3.25, -2.1666666666666665, -5.0},
        {-90, false, false, -7.5, 3.25, 2.1666666666666665, 5.0},
        {-90, false, true, -7.5, 3.25, 2.1666666666666665, -5.0},
        {-90, true, false, -7.5, 3.25, -2.1666666666666665, 5.0},
        {-90, true, true, -7.5, 3.25, -2.1666666666666665, -5.0},
        {450, false, false, -7.5, 3.25, -2.1666666666666665, -5.0},
        {450, false, true, -7.5, 3.25, -2.1666666666666665, 5.0},
        {450, true, false, -7.5, 3.25, 2.1666666666666665, -5.0},
        {450, true, true, -7.5, 3.25, 2.1666666666666665, 5.0},
        {360, false, false, -7.5, 3.25, -5.0, 2.1666666666666665},
        {360, false, true, -7.5, 3.25, -5.0, -2.1666666666666665},
        {360, true, false, -7.5, 3.25, 5.0, 2.1666666666666665},
        {360, true, true, -7.5, 3.25, 5.0, -2.1666666666666665},
        {-180, false, false, -7.5, 3.25, 5.0, -2.1666666666666665},
        {-180, false, true, -7.5, 3.25, 5.0, 2.1666666666666665},
        {-180, true, false, -7.5, 3.25, -5.0, -2.1666666666666665},
        {-180, true, true, -7.5, 3.25, -5.0, 2.1666666666666665},
        {-270, false, false, -7.5, 3.25, -2.1666666666666665, -5.0},
        {-270, false, true, -7.5, 3.25, -2.1666666666666665, 5.0},
        {-270, true, false, -7.5, 3.25, 2.1666666666666665, -5.0},
        {-270, true, true, -7.5, 3.25, 2.1666666666666665, 5.0},
    };

    // Same, general (non quarter-turn) angles: QTransform.rotate(-rotation) is arbitrary in Windows.
    const Row kDeltaGeneral[] = {
        {45, false, false, 10.0, 20.0, -4.714045207910317, 14.142135623730951},
        {45, false, true, 10.0, 20.0, -4.714045207910317, -14.142135623730912},
        {45, true, false, 10.0, 20.0, 4.714045207910317, 14.142135623730951},
        {45, true, true, 10.0, 20.0, 4.714045207910317, -14.142135623730912},
        {30, false, false, 10.0, 20.0, -0.8931639747704025, 14.880338717125852},
        {30, false, true, 10.0, 20.0, -0.8931639747704025, -14.880338717125824},
        {30, true, false, 10.0, 20.0, 0.8931639747704214, 14.880338717125852},
        {30, true, true, 10.0, 20.0, 0.8931639747704214, -14.880338717125824},
        {-45, false, false, 10.0, 20.0, 14.142135623730951, 4.714045207910317},
        {-45, false, true, 10.0, 20.0, 14.142135623730951, -4.714045207910317},
        {-45, true, false, 10.0, 20.0, -14.142135623730912, 4.714045207910317},
        {-45, true, true, 10.0, 20.0, -14.142135623730912, -4.714045207910317},
        {135, false, false, 10.0, 20.0, -14.142135623730988, -4.714045207910317},
        {135, false, true, 10.0, 20.0, -14.142135623730912, 4.714045207910317},
        {135, true, false, 10.0, 20.0, 14.142135623730951, -4.714045207910317},
        {135, true, true, 10.0, 20.0, 14.142135623730951, 4.714045207910317},
        {45, false, false, -7.5, 3.25, -5.067598598503574, -2.003469213361882},
        {45, false, true, -7.5, 3.25, -5.067598598503574, 2.0034692133619196},
        {45, true, false, -7.5, 3.25, 5.067598598503594, -2.003469213361882},
        {45, true, true, -7.5, 3.25, 5.067598598503594, 2.0034692133619196},
        {30, false, false, -7.5, 3.25, -5.413460352255527, -0.6236116251337194},
        {30, false, true, -7.5, 3.25, -5.413460352255527, 0.6236116251337382},
        {30, true, false, -7.5, 3.25, 5.413460352255545, -0.6236116251337194},
        {30, true, true, -7.5, 3.25, 5.413460352255507, 0.6236116251337382},
        {-45, false, false, -7.5, 3.25, -2.003469213361882, 5.067598598503574},
        {-45, false, true, -7.5, 3.25, -2.003469213361882, -5.067598598503574},
        {-45, true, false, -7.5, 3.25, 2.0034692133619196, 5.067598598503574},
        {-45, true, true, -7.5, 3.25, 2.0034692133619196, -5.067598598503574},
        {135, false, false, -7.5, 3.25, 2.0034692133619196, -5.067598598503574},
        {135, false, true, -7.5, 3.25, 2.0034692133619196, 5.067598598503574},
        {135, true, false, -7.5, 3.25, -2.003469213361882, -5.067598598503574},
        {135, true, true, -7.5, 3.25, -2.003469213361882, 5.067598598503574},
    };

    // scale_ratio 2 (sf1 = 1, sf2 = 2), dx = 10, dy = 20.
    const Row kDelta1over2[] = {
        {0, false, false, 10.0, 20.0, 0.2777777777777778, 0.5555555555555556},
        {0, false, true, 10.0, 20.0, 0.2777777777777778, -0.5555555555555556},
        {0, true, false, 10.0, 20.0, -0.2777777777777778, 0.5555555555555556},
        {0, true, true, 10.0, 20.0, -0.2777777777777778, -0.5555555555555556},
        {90, false, false, 10.0, 20.0, -0.5555555555555556, 0.2777777777777778},
        {90, false, true, 10.0, 20.0, -0.5555555555555556, -0.2777777777777778},
        {90, true, false, 10.0, 20.0, 0.5555555555555556, 0.2777777777777778},
        {90, true, true, 10.0, 20.0, 0.5555555555555556, -0.2777777777777778},
        {180, false, false, 10.0, 20.0, -0.2777777777777778, -0.5555555555555556},
        {180, false, true, 10.0, 20.0, -0.2777777777777778, 0.5555555555555556},
        {180, true, false, 10.0, 20.0, 0.2777777777777778, -0.5555555555555556},
        {180, true, true, 10.0, 20.0, 0.2777777777777778, 0.5555555555555556},
        {270, false, false, 10.0, 20.0, 0.5555555555555556, -0.2777777777777778},
        {270, false, true, 10.0, 20.0, 0.5555555555555556, 0.2777777777777778},
        {270, true, false, 10.0, 20.0, -0.5555555555555556, -0.2777777777777778},
        {270, true, true, 10.0, 20.0, -0.5555555555555556, 0.2777777777777778},
    };

    // Windows _ost_to_scene_pos (72 * view_scale / scale_ratio, then the page transform) minus the top-left of the
    // transformed page rect. Page 612x792, sf1 = 1, sf2 = 48: ost (in) -> canvas (out).
    const Row kPointFwd[] = {
        {0, false, false, 100.0, 200.0, 150.0, 300.0},
        {0, false, false, 0.0, 0.0, 0.0, 0.0},
        {0, false, false, -5.5, 7.25, -8.25, 10.875},
        {0, false, false, 1234.5, 10.0, 1851.75, 15.0},
        {0, false, false, 12.0, 16.5, 18.0, 24.75},
        {0, false, true, 100.0, 200.0, 150.0, 492.0},
        {0, false, true, 0.0, 0.0, 0.0, 792.0},
        {0, false, true, -5.5, 7.25, -8.25, 781.125},
        {0, false, true, 1234.5, 10.0, 1851.75, 777.0},
        {0, false, true, 12.0, 16.5, 18.0, 767.25},
        {0, true, false, 100.0, 200.0, 462.0, 300.0},
        {0, true, false, 0.0, 0.0, 612.0, 0.0},
        {0, true, false, -5.5, 7.25, 620.25, 10.875},
        {0, true, false, 1234.5, 10.0, -1239.75, 15.0},
        {0, true, false, 12.0, 16.5, 594.0, 24.75},
        {0, true, true, 100.0, 200.0, 462.0, 492.0},
        {0, true, true, 0.0, 0.0, 612.0, 792.0},
        {0, true, true, -5.5, 7.25, 620.25, 781.125},
        {0, true, true, 1234.5, 10.0, -1239.75, 777.0},
        {0, true, true, 12.0, 16.5, 594.0, 767.25},
        {90, false, false, 100.0, 200.0, 300.0, 462.0},
        {90, false, false, 0.0, 0.0, 0.0, 612.0},
        {90, false, false, -5.5, 7.25, 10.875, 620.25},
        {90, false, false, 1234.5, 10.0, 15.0, -1239.75},
        {90, false, false, 12.0, 16.5, 24.75, 594.0},
        {90, false, true, 100.0, 200.0, 492.0, 462.0},
        {90, false, true, 0.0, 0.0, 792.0, 612.0},
        {90, false, true, -5.5, 7.25, 781.125, 620.25},
        {90, false, true, 1234.5, 10.0, 777.0, -1239.75},
        {90, false, true, 12.0, 16.5, 767.25, 594.0},
        {90, true, false, 100.0, 200.0, 300.0, 150.0},
        {90, true, false, 0.0, 0.0, 0.0, 0.0},
        {90, true, false, -5.5, 7.25, 10.875, -8.25},
        {90, true, false, 1234.5, 10.0, 15.0, 1851.75},
        {90, true, false, 12.0, 16.5, 24.75, 18.0},
        {90, true, true, 100.0, 200.0, 492.0, 150.0},
        {90, true, true, 0.0, 0.0, 792.0, 0.0},
        {90, true, true, -5.5, 7.25, 781.125, -8.25},
        {90, true, true, 1234.5, 10.0, 777.0, 1851.75},
        {90, true, true, 12.0, 16.5, 767.25, 18.0},
        {180, false, false, 100.0, 200.0, 462.0, 492.0},
        {180, false, false, 0.0, 0.0, 612.0, 792.0},
        {180, false, false, -5.5, 7.25, 620.25, 781.125},
        {180, false, false, 1234.5, 10.0, -1239.75, 777.0},
        {180, false, false, 12.0, 16.5, 594.0, 767.25},
        {180, false, true, 100.0, 200.0, 462.0, 300.0},
        {180, false, true, 0.0, 0.0, 612.0, 0.0},
        {180, false, true, -5.5, 7.25, 620.25, 10.875},
        {180, false, true, 1234.5, 10.0, -1239.75, 15.0},
        {180, false, true, 12.0, 16.5, 594.0, 24.75},
        {180, true, false, 100.0, 200.0, 150.0, 492.0},
        {180, true, false, 0.0, 0.0, 0.0, 792.0},
        {180, true, false, -5.5, 7.25, -8.25, 781.125},
        {180, true, false, 1234.5, 10.0, 1851.75, 777.0},
        {180, true, false, 12.0, 16.5, 18.0, 767.25},
        {180, true, true, 100.0, 200.0, 150.0, 300.0},
        {180, true, true, 0.0, 0.0, 0.0, 0.0},
        {180, true, true, -5.5, 7.25, -8.25, 10.875},
        {180, true, true, 1234.5, 10.0, 1851.75, 15.0},
        {180, true, true, 12.0, 16.5, 18.0, 24.75},
        {270, false, false, 100.0, 200.0, 492.0, 150.0},
        {270, false, false, 0.0, 0.0, 792.0, 0.0},
        {270, false, false, -5.5, 7.25, 781.125, -8.25},
        {270, false, false, 1234.5, 10.0, 777.0, 1851.75},
        {270, false, false, 12.0, 16.5, 767.25, 18.0},
        {270, false, true, 100.0, 200.0, 300.0, 150.0},
        {270, false, true, 0.0, 0.0, 0.0, 0.0},
        {270, false, true, -5.5, 7.25, 10.875, -8.25},
        {270, false, true, 1234.5, 10.0, 15.0, 1851.75},
        {270, false, true, 12.0, 16.5, 24.75, 18.0},
        {270, true, false, 100.0, 200.0, 492.0, 462.0},
        {270, true, false, 0.0, 0.0, 792.0, 612.0},
        {270, true, false, -5.5, 7.25, 781.125, 620.25},
        {270, true, false, 1234.5, 10.0, 777.0, -1239.75},
        {270, true, false, 12.0, 16.5, 767.25, 594.0},
        {270, true, true, 100.0, 200.0, 300.0, 462.0},
        {270, true, true, 0.0, 0.0, 0.0, 612.0},
        {270, true, true, -5.5, 7.25, 10.875, 620.25},
        {270, true, true, 1234.5, 10.0, 15.0, -1239.75},
        {270, true, true, 12.0, 16.5, 24.75, 594.0},
    };

    // Windows _scene_pos_to_ost (inverse page transform, times scale_ratio / (72 * view_scale)). Page 612x792,
    // sf1 = 1, sf2 = 48: canvas (in) -> ost (out).
    const Row kPointInv[] = {
        {0, false, false, 100.25, 200.5, 66.83333333333333, 133.66666666666666},
        {0, false, false, 0.0, 0.0, 0.0, 0.0},
        {0, false, false, 300.0, 400.0, 200.0, 266.66666666666663},
        {0, false, false, 792.0, 612.0, 528.0, 408.0},
        {0, false, false, 33.0, 700.125, 22.0, 466.75},
        {0, false, true, 100.25, 200.5, 66.83333333333333, 394.3333333333333},
        {0, false, true, 0.0, 0.0, 0.0, 528.0},
        {0, false, true, 300.0, 400.0, 200.0, 261.3333333333333},
        {0, false, true, 792.0, 612.0, 528.0, 120.0},
        {0, false, true, 33.0, 700.125, 22.0, 61.25},
        {0, true, false, 100.25, 200.5, 341.16666666666663, 133.66666666666666},
        {0, true, false, 0.0, 0.0, 408.0, 0.0},
        {0, true, false, 300.0, 400.0, 208.0, 266.66666666666663},
        {0, true, false, 792.0, 612.0, -120.0, 408.0},
        {0, true, false, 33.0, 700.125, 386.0, 466.75},
        {0, true, true, 100.25, 200.5, 341.16666666666663, 394.3333333333333},
        {0, true, true, 0.0, 0.0, 408.0, 528.0},
        {0, true, true, 300.0, 400.0, 208.0, 261.3333333333333},
        {0, true, true, 792.0, 612.0, -120.0, 120.0},
        {0, true, true, 33.0, 700.125, 386.0, 61.25},
        {90, false, false, 100.25, 200.5, 274.3333333333333, 66.83333333333333},
        {90, false, false, 0.0, 0.0, 408.0, 0.0},
        {90, false, false, 300.0, 400.0, 141.33333333333331, 200.0},
        {90, false, false, 792.0, 612.0, 0.0, 528.0},
        {90, false, false, 33.0, 700.125, -58.75, 22.0},
        {90, false, true, 100.25, 200.5, 274.3333333333333, 461.16666666666663},
        {90, false, true, 0.0, 0.0, 408.0, 528.0},
        {90, false, true, 300.0, 400.0, 141.33333333333331, 328.0},
        {90, false, true, 792.0, 612.0, 0.0, 0.0},
        {90, false, true, 33.0, 700.125, -58.75, 506.0},
        {90, true, false, 100.25, 200.5, 133.66666666666666, 66.83333333333333},
        {90, true, false, 0.0, 0.0, 0.0, 0.0},
        {90, true, false, 300.0, 400.0, 266.66666666666663, 200.0},
        {90, true, false, 792.0, 612.0, 408.0, 528.0},
        {90, true, false, 33.0, 700.125, 466.75, 22.0},
        {90, true, true, 100.25, 200.5, 133.66666666666666, 461.16666666666663},
        {90, true, true, 0.0, 0.0, 0.0, 528.0},
        {90, true, true, 300.0, 400.0, 266.66666666666663, 328.0},
        {90, true, true, 792.0, 612.0, 408.0, 0.0},
        {90, true, true, 33.0, 700.125, 466.75, 506.0},
        {180, false, false, 100.25, 200.5, 341.16666666666663, 394.3333333333333},
        {180, false, false, 0.0, 0.0, 408.0, 528.0},
        {180, false, false, 300.0, 400.0, 208.0, 261.3333333333333},
        {180, false, false, 792.0, 612.0, -120.0, 120.0},
        {180, false, false, 33.0, 700.125, 386.0, 61.25},
        {180, false, true, 100.25, 200.5, 341.16666666666663, 133.66666666666666},
        {180, false, true, 0.0, 0.0, 408.0, 0.0},
        {180, false, true, 300.0, 400.0, 208.0, 266.66666666666663},
        {180, false, true, 792.0, 612.0, -120.0, 408.0},
        {180, false, true, 33.0, 700.125, 386.0, 466.75},
        {180, true, false, 100.25, 200.5, 66.83333333333333, 394.3333333333333},
        {180, true, false, 0.0, 0.0, 0.0, 528.0},
        {180, true, false, 300.0, 400.0, 200.0, 261.3333333333333},
        {180, true, false, 792.0, 612.0, 528.0, 120.0},
        {180, true, false, 33.0, 700.125, 22.0, 61.25},
        {180, true, true, 100.25, 200.5, 66.83333333333333, 133.66666666666666},
        {180, true, true, 0.0, 0.0, 0.0, 0.0},
        {180, true, true, 300.0, 400.0, 200.0, 266.66666666666663},
        {180, true, true, 792.0, 612.0, 528.0, 408.0},
        {180, true, true, 33.0, 700.125, 22.0, 466.75},
        {270, false, false, 100.25, 200.5, 133.66666666666666, 461.16666666666663},
        {270, false, false, 0.0, 0.0, 0.0, 528.0},
        {270, false, false, 300.0, 400.0, 266.66666666666663, 328.0},
        {270, false, false, 792.0, 612.0, 408.0, 0.0},
        {270, false, false, 33.0, 700.125, 466.75, 506.0},
        {270, false, true, 100.25, 200.5, 133.66666666666666, 66.83333333333333},
        {270, false, true, 0.0, 0.0, 0.0, 0.0},
        {270, false, true, 300.0, 400.0, 266.66666666666663, 200.0},
        {270, false, true, 792.0, 612.0, 408.0, 528.0},
        {270, false, true, 33.0, 700.125, 466.75, 22.0},
        {270, true, false, 100.25, 200.5, 274.3333333333333, 461.16666666666663},
        {270, true, false, 0.0, 0.0, 408.0, 528.0},
        {270, true, false, 300.0, 400.0, 141.33333333333331, 328.0},
        {270, true, false, 792.0, 612.0, 0.0, 0.0},
        {270, true, false, 33.0, 700.125, -58.75, 506.0},
        {270, true, true, 100.25, 200.5, 274.3333333333333, 66.83333333333333},
        {270, true, true, 0.0, 0.0, 408.0, 0.0},
        {270, true, true, 300.0, 400.0, 141.33333333333331, 200.0},
        {270, true, true, 792.0, 612.0, 0.0, 528.0},
        {270, true, true, 33.0, 700.125, -58.75, 22.0},
    };

    template <size_t N>
    void check_delta_rows(const Row (&rows)[N], double sf1, double sf2)
    {
        for (const Row &r : rows)
        {
            SCOPED_CONTEXT(case_name(r.rot, r.fx, r.fy) + " dx=" + std::to_string(r.a) + " dy=" + std::to_string(r.b));
            Vec2 got = ost_coord::canvas_delta_to_ost(r.a, r.b, sf1, sf2, r.rot, r.fx, r.fy);
            check_vec2(got, r.ea, r.eb, false);
        }
    }

    const double kW = 612.0, kH = 792.0, kSf1 = 1.0, kSf2 = 48.0;
} // namespace

// ---- canvas_delta_to_ost -------------------------------------------------------------------------------------

// RPV-02 test plan (sf1 = 1, sf2 = 48: 0.66667 units per point) and 3 more deltas, all rotations, all flips.
TEST(CanvasDelta, MatchesTheWindowsInverseTransformOnQuarterTurns)
{
    check_delta_rows(kDeltaQuarter, kSf1, kSf2);
}

// Windows rotates by any angle (QTransform.rotate): the delta is the linear part of the inverse, no exception.
TEST(CanvasDelta, GeneralAnglesFollowTheQTransform)
{
    check_delta_rows(kDeltaGeneral, kSf1, kSf2);
}

// LP-03 note: canvas_delta_to_ost(10, 20, 1, 2, 90) was (-0.5556, -0.2778) on iOS; Windows (-0.5556, +0.2778).
TEST(CanvasDelta, ScaleOneToTwoTable)
{
    check_delta_rows(kDelta1over2, 1.0, 2.0);
    Vec2 v = ost_coord::canvas_delta_to_ost(10.0, 20.0, 1.0, 2.0, 90, false, false);
    CHECK_NEAR(v[0], -0.5555555555555556, 1e-12);
    CHECK_NEAR(v[1], 0.2777777777777778, 1e-12);
}

// Port of tests/presentation/components/plan_view/components/test_drag_handler.py
//   ::DragHandlerSnapAndDeltaTests::test_scene_to_ost_delta_scales_by_the_pdf_ratio_without_a_page_transform
// (scale_ratio 36 -> sf1 = 1, sf2 = 36; the canvas is in points, i.e. Windows view_scale = 1).
TEST(CanvasDelta, ScalesByThePdfRatioWithoutAPageTransform)
{
    Vec2 v = ost_coord::canvas_delta_to_ost(6.0, 8.0, 1.0, 36.0, 0, false, false);
    CHECK_EQ(v[0], 3.0);
    CHECK_EQ(v[1], 4.0);
}

// Port of ...::test_page_rotation_mixes_the_axes_in_both_directions: the Windows test uses
// QTransform().translate(10, 20).rotate(90.0), i.e. a clockwise quarter turn = page rotation 270 (-90);
// scene_to_ost_delta(-5, 3) with scale_ratio 72 is (3, 5).
TEST(CanvasDelta, PageRotationMixesTheAxes)
{
    Vec2 v = ost_coord::canvas_delta_to_ost(-5.0, 3.0, 1.0, 72.0, 270, false, false);
    CHECK_NEAR(v[0], 3.0, 1e-9);
    CHECK_NEAR(v[1], 5.0, 1e-9);
    // ... and rotation -90 is the same page.
    v = ost_coord::canvas_delta_to_ost(-5.0, 3.0, 1.0, 72.0, -90, false, false);
    CHECK_NEAR(v[0], 3.0, 1e-9);
    CHECK_NEAR(v[1], 5.0, 1e-9);
}

// LP-M01 (RPV-03 alias): letter page, ratio 1, flip_x. On a flip_x page +1 OST x moves the canvas by -72 points, so a
// canvas delta of -72 is +1 OST (iOS returned -1 because flips were not consulted).
TEST(CanvasDelta, FlipXPageDragsInTheOppositeCanvasDirection)
{
    Vec2 v = ost_coord::canvas_delta_to_ost(-72.0, 0.0, 1.0, 1.0, 0, true, false);
    CHECK_EQ(v[0], 1.0);
    CHECK_EQ(v[1], 0.0);
    v = ost_coord::canvas_delta_to_ost(0.0, -72.0, 1.0, 1.0, 0, false, true); // flip_y
    CHECK_EQ(v[0], 0.0);
    CHECK_EQ(v[1], 1.0);
}

// ---- ost_to_canvas_point / canvas_to_ost_point ------------------------------------------------------------------

TEST(CanvasPoint, OstToCanvasMatchesTheWindowsPageTransform)
{
    for (const Row &r : kPointFwd)
    {
        SCOPED_CONTEXT(case_name(r.rot, r.fx, r.fy) + " ost=" + std::to_string(r.a) + "," + std::to_string(r.b));
        Vec2 got = ost_coord::ost_to_canvas_point(r.a, r.b, kW, kH, kSf1, kSf2, r.rot, r.fx, r.fy);
        check_vec2(got, r.ea, r.eb, false);
    }
}

TEST(CanvasPoint, CanvasToOstMatchesTheWindowsInverseTransform)
{
    for (const Row &r : kPointInv)
    {
        SCOPED_CONTEXT(case_name(r.rot, r.fx, r.fy) + " canvas=" + std::to_string(r.a) + "," + std::to_string(r.b));
        Vec2 got = ost_coord::canvas_to_ost_point(r.a, r.b, kW, kH, kSf1, kSf2, r.rot, r.fx, r.fy);
        check_vec2(got, r.ea, r.eb, false);
    }
}

// GEO-08 alias plan: OST (10, 20) on a 600x800 page at sf 1:2. The canvas is the OUTPUT page (W and H swap for 90 and
// 270), top-left origin: unrotated (360, 720); with rotation 90 it is (720, 240) (= the Y-up PDF point (720, 360) of the
// RPV-01 table, 600 - 240 = 360). Values from QTransform (rows: none, flip_y, flip_x, both).
TEST(CanvasPoint, OstTenTwentyOn600x800Page)
{
    const double expected[4][4][2] = {
        {{360.0, 720.0}, {360.0, 80.0}, {240.0, 720.0}, {240.0, 80.0}},   // rotation 0
        {{720.0, 240.0}, {80.0, 240.0}, {720.0, 360.0}, {80.0, 360.0}},   // rotation 90
        {{240.0, 80.0}, {240.0, 720.0}, {360.0, 80.0}, {360.0, 720.0}},   // rotation 180
        {{80.0, 360.0}, {720.0, 360.0}, {80.0, 240.0}, {720.0, 240.0}}};  // rotation 270
    for (int q = 0; q < 4; ++q)
    {
        for (int f = 0; f < 4; ++f)
        {
            const int rot = q * 90;
            const geo_test::Flips fl = geo_test::all_flips()[f];
            SCOPED_CONTEXT(case_name(rot, fl.x, fl.y));
            Vec2 c = ost_coord::ost_to_canvas_point(10.0, 20.0, 600.0, 800.0, 1.0, 2.0, rot, fl.x, fl.y);
            CHECK_EQ(c[0], expected[q][f][0]);
            CHECK_EQ(c[1], expected[q][f][1]);
            Vec2 back = ost_coord::canvas_to_ost_point(c[0], c[1], 600.0, 800.0, 1.0, 2.0, rot, fl.x, fl.y);
            CHECK_NEAR(back[0], 10.0, 1e-9);
            CHECK_NEAR(back[1], 20.0, 1e-9);
        }
    }
}

// LP-M01: letter page, sf 1/1, flip_x: Core ost_to_pdf_coordinates of OST (1, 2) is (540, 648) Y-up, i.e. the canvas
// (Y-down) point (540, 144); the inverse of that canvas point must be (1, 2) (iOS _canvasToOst gave (7.5, 2.0)).
TEST(CanvasPoint, LetterFlipXInverse)
{
    Vec2 c = ost_coord::ost_to_canvas_point(1.0, 2.0, 612.0, 792.0, 1.0, 1.0, 0, true, false);
    CHECK_EQ(c[0], 540.0);
    CHECK_EQ(c[1], 144.0);
    Vec2 o = ost_coord::canvas_to_ost_point(540.0, 144.0, 612.0, 792.0, 1.0, 1.0, 0, true, false);
    CHECK_NEAR(o[0], 1.0, 1e-12);
    CHECK_NEAR(o[1], 2.0, 1e-12);
}

// RPV-03 plan: canvas -> ost -> canvas and ost -> canvas -> ost are the identity for every rotation x flip.
TEST(CanvasPoint, RoundTripsOnAllSixteenCombinations)
{
    const double points[][2] = {{10.0, 20.0}, {0.0, 0.0}, {-5.5, 7.25}, {123.456, 98.7}, {1234.5, 10.0}};
    for (int q = 0; q < 4; ++q)
    {
        for (const geo_test::Flips &fl : geo_test::all_flips())
        {
            const int rot = q * 90;
            for (const auto &p : points)
            {
                SCOPED_CONTEXT(case_name(rot, fl.x, fl.y) + " p=" + std::to_string(p[0]) + "," + std::to_string(p[1]));
                Vec2 c = ost_coord::ost_to_canvas_point(p[0], p[1], kW, kH, kSf1, kSf2, rot, fl.x, fl.y);
                Vec2 o = ost_coord::canvas_to_ost_point(c[0], c[1], kW, kH, kSf1, kSf2, rot, fl.x, fl.y);
                CHECK_NEAR(o[0], p[0], rel_tol(o[0], p[0]));
                CHECK_NEAR(o[1], p[1], rel_tol(o[1], p[1]));
                Vec2 c2 = ost_coord::ost_to_canvas_point(o[0], o[1], kW, kH, kSf1, kSf2, rot, fl.x, fl.y);
                CHECK_NEAR(c2[0], c[0], rel_tol(c2[0], c[0]));
                CHECK_NEAR(c2[1], c[1], rel_tol(c2[1], c[1]));
            }
            // and from canvas points
            const double canvas[][2] = {{100.25, 200.5}, {0.0, 0.0}, {300.0, 400.0}, {33.0, 700.125}};
            for (const auto &c : canvas)
            {
                SCOPED_CONTEXT(case_name(rot, fl.x, fl.y) + " canvas=" + std::to_string(c[0]) + "," + std::to_string(c[1]));
                Vec2 o = ost_coord::canvas_to_ost_point(c[0], c[1], kW, kH, kSf1, kSf2, rot, fl.x, fl.y);
                Vec2 c2 = ost_coord::ost_to_canvas_point(o[0], o[1], kW, kH, kSf1, kSf2, rot, fl.x, fl.y);
                CHECK_NEAR(c2[0], c[0], rel_tol(c2[0], c[0]));
                CHECK_NEAR(c2[1], c[1], rel_tol(c2[1], c[1]));
            }
        }
    }
}

// The canvas point of an OST point is the Windows Y-up PDF point of ost_to_pdf_coordinates flipped with the OUTPUT
// height (PlanView must use the output height, not the unrotated page height, for its Y flip: GEO-06 call sites).
TEST(CanvasPoint, AgreesWithOstToPdfCoordinates)
{
    const double points[][2] = {{10.0, 20.0}, {0.0, 0.0}, {-5.5, 7.25}, {123.456, 98.7}};
    for (int q = 0; q < 4; ++q)
    {
        for (const geo_test::Flips &fl : geo_test::all_flips())
        {
            const int rot = q * 90;
            const double out_h = (rot == 90 || rot == 270) ? kW : kH;
            for (const auto &p : points)
            {
                SCOPED_CONTEXT(case_name(rot, fl.x, fl.y) + " p=" + std::to_string(p[0]) + "," + std::to_string(p[1]));
                std::vector<Vec2> pd = pdf({p[0], p[1]}, kW, kH, kSf1, kSf2, rot, fl.x, fl.y);
                REQUIRE_EQ(pd.size(), 1u);
                Vec2 c = ost_coord::ost_to_canvas_point(p[0], p[1], kW, kH, kSf1, kSf2, rot, fl.x, fl.y);
                CHECK_NEAR(c[0], pd[0][0], rel_tol(c[0], pd[0][0]));
                CHECK_NEAR(c[1], out_h - pd[0][1], rel_tol(c[1], out_h - pd[0][1]));
            }
        }
    }
}

// canvas_delta_to_ost is the linear part of canvas_to_ost_point: c2 - c1 maps to the difference of the OST points.
TEST(CanvasPoint, DeltaIsTheLinearPartOfThePointMap)
{
    const double c1[2] = {123.5, 77.25}, c2[2] = {300.75, 410.125};
    for (int q = 0; q < 4; ++q)
    {
        for (const geo_test::Flips &fl : geo_test::all_flips())
        {
            const int rot = q * 90;
            SCOPED_CONTEXT(case_name(rot, fl.x, fl.y));
            Vec2 o1 = ost_coord::canvas_to_ost_point(c1[0], c1[1], kW, kH, kSf1, kSf2, rot, fl.x, fl.y);
            Vec2 o2 = ost_coord::canvas_to_ost_point(c2[0], c2[1], kW, kH, kSf1, kSf2, rot, fl.x, fl.y);
            Vec2 d = ost_coord::canvas_delta_to_ost(c2[0] - c1[0], c2[1] - c1[1], kSf1, kSf2, rot, fl.x, fl.y);
            CHECK_NEAR(d[0], o2[0] - o1[0], rel_tol(d[0], o2[0] - o1[0]));
            CHECK_NEAR(d[1], o2[1] - o1[1], rel_tol(d[1], o2[1] - o1[1]));
        }
    }
}

// The point maps use the page contract of Windows page_transform.hpp (same messages as ost_to_pdf_coordinates).
TEST(CanvasPoint, RejectInvalidPageContract)
{
    std::string what;
    CHECK(throws_invalid_argument([] { ost_coord::ost_to_canvas_point(1.0, 2.0, 0.0, 800.0, 1.0, 2.0, 0, false, false); }, &what));
    CHECK_STR_EQ(what, "Page dimensions must be finite and positive");
    CHECK(throws_invalid_argument([] { ost_coord::canvas_to_ost_point(1.0, 2.0, 600.0, 0.0, 1.0, 2.0, 0, false, false); }, &what));
    CHECK_STR_EQ(what, "Page dimensions must be finite and positive");
    CHECK(throws_invalid_argument([] { ost_coord::ost_to_canvas_point(1.0, 2.0, 600.0, 800.0, 0.0, 2.0, 0, false, false); }, &what));
    CHECK_STR_EQ(what, "Page scale factors must be finite and positive");
    CHECK(throws_invalid_argument([] { ost_coord::canvas_to_ost_point(1.0, 2.0, 600.0, 800.0, 1.0, -2.0, 0, false, false); }, &what));
    CHECK_STR_EQ(what, "Page scale factors must be finite and positive");
    CHECK(throws_invalid_argument([] { ost_coord::ost_to_canvas_point(1.0, 2.0, 600.0, 800.0, 1.0, 2.0, 45, false, false); }, &what));
    CHECK_STR_EQ(what, "Page rotation must be a multiple of 90 degrees");
    CHECK(throws_invalid_argument([] { ost_coord::canvas_to_ost_point(1.0, 2.0, 600.0, 800.0, 1.0, 2.0, 45, false, false); }, &what));
    CHECK_STR_EQ(what, "Page rotation must be a multiple of 90 degrees");
    CHECK(throws_invalid_argument([] {
        ost_coord::ost_to_canvas_point(std::numeric_limits<double>::quiet_NaN(), 2.0, 600.0, 800.0, 1.0, 2.0, 0, false, false);
    }, &what));
    CHECK_STR_EQ(what, "Page coordinates must be finite");
    CHECK(throws_invalid_argument([] {
        ost_coord::canvas_to_ost_point(1.0, std::numeric_limits<double>::infinity(), 600.0, 800.0, 1.0, 2.0, 0, false, false);
    }, &what));
    CHECK_STR_EQ(what, "Page coordinates must be finite");
}
