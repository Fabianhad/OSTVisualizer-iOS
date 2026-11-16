// Drop-stub union (MSH-19): the union of a wall and its drop stub passes remove_degenerate_faces before use.
// Windows: meshing/mesh_factory.py (boolean_union(mesh, stub, ...)) -> cpp_extensions/src/module_geometry.cpp
// boolean_union_py: `result = boolean_union(m1, m2); if (result) { remove_degenerate_faces(*result); ... }`.
// Needs the real boolean (Manifold): configure with -DOST_TEST_BOOLEAN=ON (run_tests.ps1 -Boolean). Without it the
// file compiles to nothing.
#include "MeshingTestUtil.hpp"

#if OST_HAVE_BOOLEAN
#include "Geometry/boolean_ops.hpp"
#include "Geometry/mesh_repair.hpp"

#include <cmath>
#include <string>

using namespace mesh_test;

namespace
{
    Condition drop_condition(double drop_value, double rise, double run)
    {
        Condition c = make_condition("c", ConditionType::Linear);
        c.height = 96.0;
        c.thickness = 6.0;
        c.rise = rise;
        c.run = run;
        c.drop_run = true;
        c.drop_value = drop_value;
        return c;
    }

    size_t degenerate_faces(const Mesh &m)
    {
        Mesh copy = m;
        size_t before = copy.faces.size();
        ost_geometry::remove_degenerate_faces(copy);
        return before - copy.faces.size();
    }
} // namespace

TEST(DropStubUnion, FactoryUnionHasNoDegenerateFaces)
{
    // Walls at an angle (0.7 rad), with and without slope, drop up and down: the raw Manifold union of wall and stub
    // contains zero-area faces (3 to 8 of 27 to 32 on these inputs); the factory must return the repaired union.
    int with_raw_degenerate = 0;
    for (double rise : {0.0, 1.0, -2.0})
        for (double run : {0.0, 4.0})
            for (double drop : {12.0, -12.0})
                for (double len : {120.0, 30.0})
                {
                    SCOPED_CONTEXT("rise " + std::to_string(rise) + " run " + std::to_string(run) + " drop " +
                                   std::to_string(drop) + " len " + std::to_string(len));
                    const double ang = 0.7;
                    const std::vector<double> pos{0, 0, len * std::cos(ang), len * std::sin(ang)};
                    Takeoff t = make_takeoff("t", "c", "p", pos);
                    Condition with_stub = drop_condition(drop, rise, run);
                    auto r = ost_meshing::create_mesh_for_takeoff(t, with_stub, 1.0);
                    REQUIRE(r.valid);
                    CHECK_EQ(degenerate_faces(r.mesh), static_cast<size_t>(0));

                    // The same union by hand: factory == repaired raw union.
                    Condition plain_cond = with_stub;
                    plain_cond.drop_run = false;
                    auto plain = ost_meshing::create_mesh_for_takeoff(t, plain_cond, 1.0);
                    Mesh stub = ost_meshing::generate_drop_stub_mesh(-pos[0], -pos[1], -pos[2], -pos[3], 6.0, drop, 0.0, 96.0,
                                                                     rise, run);
                    auto raw = ost_geometry::boolean_union(plain.mesh, stub);
                    REQUIRE(raw.has_value());
                    if (degenerate_faces(*raw) > 0)
                        ++with_raw_degenerate;
                    Mesh repaired = *raw;
                    ost_geometry::remove_degenerate_faces(repaired);
                    CHECK_EQ(r.mesh.faces.size(), repaired.faces.size());
                    CHECK(r.mesh.vertices == repaired.vertices);
                    CHECK(r.mesh.faces == repaired.faces);
                }
    CHECK(with_raw_degenerate > 0); // the inputs really exercise the repair
}

TEST(DropStubUnion, AxisAlignedWallUnionIsUnchangedAndValid)
{
    Takeoff t = make_takeoff("t", "c", "p", {0, 0, 120, 0});
    auto r = ost_meshing::create_mesh_for_takeoff(t, drop_condition(12.0, 0.0, 0.0), 1.0);
    REQUIRE(r.valid);
    CHECK_EQ(degenerate_faces(r.mesh), static_cast<size_t>(0));
    // wall 120 x 6 x 96 plus the stub volume above it
    CHECK(std::fabs(signed_volume(r.mesh)) > 120.0 * 6.0 * 96.0);
}

TEST(DropStubUnion, CurvedWallUnionHasNoDegenerateFaces)
{
    Takeoff t = make_takeoff("t", "c", "p", {0, 0, 120, 30, 60, 50});
    t.curve = 0;
    auto r = ost_meshing::create_mesh_for_takeoff(t, drop_condition(12.0, 1.0, 4.0), 1.0);
    REQUIRE(r.valid);
    CHECK_EQ(degenerate_faces(r.mesh), static_cast<size_t>(0));
}
#endif
