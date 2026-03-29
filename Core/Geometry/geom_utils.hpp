#pragma once
#include "vec_types.hpp"
#include <vector>

namespace ost_geom
{
    using ost_types::Vec2;

    bool segments_intersect(
        const Vec2 &p1, const Vec2 &p2,
        const Vec2 &p3, const Vec2 &p4);

    bool polygon_is_valid(const std::vector<Vec2> &pts);

    bool polyline_self_intersects(const std::vector<Vec2> &pts);

    double signed_area(const std::vector<double> &pos);

}
