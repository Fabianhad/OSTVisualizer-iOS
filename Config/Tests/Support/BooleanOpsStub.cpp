// Host-test stand-in for Core/Geometry/boolean_ops.cpp when OST_TEST_BOOLEAN=OFF (Manifold not built).
// It exists only so that objects which reference the boolean API (Core/Meshing/MeshFactory.cpp) link.
// Every call throws, so a test that reaches it FAILS loudly instead of silently passing on wrong data.
// Tests that need real booleans must be wrapped in '#if OST_HAVE_BOOLEAN' (1 when OST_TEST_BOOLEAN=ON).
#include "Core/Geometry/boolean_ops.hpp"

#include <stdexcept>
#include <string>

namespace ost_geometry
{
    namespace
    {
        [[noreturn]] void unavailable(const char *fn)
        {
            throw std::runtime_error(std::string("ost_geometry::") + fn +
                                     " called but Manifold is not built (configure with -DOST_TEST_BOOLEAN=ON)");
        }
    } // namespace

    std::optional<CppMeshData> boolean_difference(const CppMeshData &, const CppMeshData &)
    {
        unavailable("boolean_difference");
    }
    std::optional<CppMeshData> boolean_difference_batch(const CppMeshData &, const std::vector<CppMeshData> &)
    {
        unavailable("boolean_difference_batch");
    }
    std::optional<CppMeshData> boolean_union(const CppMeshData &, const CppMeshData &)
    {
        unavailable("boolean_union");
    }
    std::optional<CppMeshData> boolean_intersection(const CppMeshData &, const CppMeshData &)
    {
        unavailable("boolean_intersection");
    }
    bool aabb_intersects(const CppMeshData &, const CppMeshData &)
    {
        unavailable("aabb_intersects");
    }
} // namespace ost_geometry
