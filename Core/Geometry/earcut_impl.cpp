#include "earcut_impl.hpp"
#include "earcut.hpp"

namespace ost_geometry
{

    std::vector<uint32_t> triangulate_polygon(
        const std::vector<std::vector<std::array<double, 2>>> &polygon)
    {
        return mapbox::earcut<uint32_t>(polygon);
    }

}
