#pragma once
#include <vector>
#include <cstdint>
#include <array>

namespace ost_geometry
{

    std::vector<uint32_t> triangulate_polygon(
        const std::vector<std::vector<std::array<double, 2>>> &polygon);

}
