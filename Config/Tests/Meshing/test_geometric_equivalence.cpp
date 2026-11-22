// Geometric equivalence with the Windows meshes (EXP-17, user decision (a) of 2026-11-16): geometric equivalence is
// enough, no byte-identical vertex / face order. The iOS mesh of a count shape, a wall (plain, sloped, curved), a drop stub
// or an area has
//   - the SAME VERTEX SET as the Windows mesh (whatever the vertex order),
//   - the same bounding box and the same absolute signed volume,
//   - an outward-facing (positive) signed volume (GEO-15: iOS keeps its outward meshes; the cylinder and the curved walls
//     were inside-out until EXP-13 turned them outward, so the sign is asserted for every shape),
//   - a closed, consistently oriented shell,
//   - the same planar faces: the outline of every flat face (the edges that are not the diagonal of a coplanar pair of
//     triangles) is the same set of edges as in Windows, and
//   - the SAME FACE SET: the same triangles whatever the order of the faces and the winding of each face (areas: GEO-14;
//     every other shape: the triangulation of the quads follows the Windows diagonals; on the quads of a sloped curved
//     wall, which are not planar, that is the geometry itself).
// Vertex order, face order and winding are deliberately not compared. Windows oracle: the exact vertices / faces of
// MeshFactory.create_mesh_for_takeoff (count_cases() in WindowsOracles.hpp, equiv_cases() in EquivalenceOracles.hpp, Windows
// frame), of MeshGenerator.generate_drop_stub_mesh (wall_part_cases() in EquivalenceWallPartOracles.hpp) and of
// core/geometry/area.py calc_area_faces (face_cases()); the box face table is utils/mesh.py get_box_faces.
#include "MeshingTestUtil.hpp"
#include "WindowsOracles.hpp"
#include "EquivalenceOracles.hpp"
#include "EquivalenceWallPartOracles.hpp"

#include <cmath>
#include <map>
#include <string>

using namespace mesh_test;

namespace
{
    using Faces = std::vector<std::array<uint32_t, 3>>;
    using Edge = std::pair<uint32_t, uint32_t>;

    // utils/mesh.py get_box_faces() (Windows vertex order: bottom corners 0..3, top corners 4..7).
    const Faces &windows_box_faces()
    {
        static const Faces f = {{0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6}, {0, 3, 7}, {0, 7, 4},
                                {1, 0, 4}, {1, 4, 5}, {2, 1, 5}, {2, 5, 6}, {3, 2, 6}, {3, 6, 7}};
        return f;
    }

    const Faces &windows_count_faces(const std::string &table)
    {
        static const Faces none;
        if (table == "box")
            return windows_box_faces();
        if (table == "triangle")
            return ost_oracle::faces_triangle();
        if (table == "rhombus")
            return ost_oracle::faces_rhombus();
        if (table == "radial6")
            return ost_oracle::faces_radial6();
        if (table == "radial8")
            return ost_oracle::faces_radial8();
        if (table == "radial24")
            return ost_oracle::faces_radial24();
        return none;
    }

    // perm[i] = the index in `reference` of iOS vertex i (greedy one-to-one match within tol_abs); empty when the two
    // vertex sets differ.
    std::vector<uint32_t> match_vertices(const std::vector<V3> &ios, const std::vector<V3> &reference, double tol_abs)
    {
        std::vector<uint32_t> perm;
        if (ios.size() != reference.size())
            return perm;
        std::vector<bool> used(reference.size(), false);
        for (const auto &p : ios)
        {
            bool found = false;
            for (size_t j = 0; j < reference.size() && !found; ++j)
            {
                if (used[j])
                    continue;
                if (std::fabs(p[0] - reference[j][0]) <= tol_abs && std::fabs(p[1] - reference[j][1]) <= tol_abs &&
                    std::fabs(p[2] - reference[j][2]) <= tol_abs)
                {
                    used[j] = true;
                    perm.push_back(static_cast<uint32_t>(j));
                    found = true;
                }
            }
            if (!found)
                return {};
        }
        return perm;
    }

    // Faces of `m` expressed in the vertex numbering of the reference (Windows) mesh.
    Faces faces_in_reference_numbering(const Mesh &m, const std::vector<uint32_t> &perm)
    {
        Faces out;
        out.reserve(m.faces.size());
        for (const auto &f : m.faces)
            out.push_back({perm[f[0]], perm[f[1]], perm[f[2]]});
        return out;
    }

    V3 face_normal(const Mesh &m, const std::array<uint32_t, 3> &f)
    {
        const V3 &a = m.vertices[f[0]];
        const V3 &b = m.vertices[f[1]];
        const V3 &c = m.vertices[f[2]];
        const V3 u{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const V3 v{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        V3 n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len > 0.0)
            for (double &x : n)
                x /= len;
        return n;
    }

    // The outline of the flat faces of a mesh, in reference numbering: every edge with one adjacent triangle, with more
    // than two, or whose two adjacent triangles are not coplanar. The diagonal that splits a flat quad into two
    // triangles has two coplanar neighbours and is left out, so two triangulations of the same flat faces have the same
    // outline. `faces` are the faces of `m` in reference numbering; `m` supplies the positions for the normals.
    std::set<Edge> flat_face_outline(const Mesh &m, const Faces &faces)
    {
        std::map<Edge, std::vector<size_t>> adjacent;
        for (size_t i = 0; i < faces.size(); ++i)
            for (int k = 0; k < 3; ++k)
            {
                uint32_t a = faces[i][k], b = faces[i][(k + 1) % 3];
                adjacent[{std::min(a, b), std::max(a, b)}].push_back(i);
            }
        std::set<Edge> outline;
        for (const auto &kv : adjacent)
        {
            const auto &tris = kv.second;
            bool coplanar_pair = false;
            if (tris.size() == 2)
            {
                const V3 n0 = face_normal(m, m.faces[tris[0]]);
                const V3 n1 = face_normal(m, m.faces[tris[1]]);
                const V3 cross{n0[1] * n1[2] - n0[2] * n1[1], n0[2] * n1[0] - n0[0] * n1[2], n0[0] * n1[1] - n0[1] * n1[0]};
                coplanar_pair = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]) < 1e-9;
            }
            if (!coplanar_pair)
                outline.insert(kv.first);
        }
        return outline;
    }

    struct Expect
    {
        bool outward = true; // positive signed volume
    };

    // The EXP-17 acceptance for one mesh against its Windows reference (vertices already in the iOS frame, Windows faces
    // in the Windows numbering).
    void check_geometrically_equivalent(const Mesh &ios, const std::vector<V3> &windows_vertices_ios_frame,
                                        const Faces &windows_faces, double windows_abs_volume, const Expect &expect)
    {
        auto perm = match_vertices(ios.vertices, windows_vertices_ios_frame, 1e-7);
        REQUIRE_EQ(perm.size(), ios.vertices.size()); // vertex SET equal
        REQUIRE_EQ(ios.faces.size(), windows_faces.size());
        const Faces ios_faces = faces_in_reference_numbering(ios, perm);

        Mesh reference;
        reference.vertices = windows_vertices_ios_frame;
        reference.faces = windows_faces;
        Bounds a = bounds_of(ios), b = bounds_of(reference);
        for (int k = 0; k < 3; ++k)
        {
            CHECK_NEAR(a.lo[k], b.lo[k], 1e-7);
            CHECK_NEAR(a.hi[k], b.hi[k], 1e-7);
        }
        CHECK_NEAR(std::fabs(signed_volume(ios)), windows_abs_volume, 1e-9 * std::max(1.0, windows_abs_volume));
        if (expect.outward)
            CHECK(signed_volume(ios) > 0.0);
        CHECK(closed_and_consistent(ios));
        CHECK(flat_face_outline(ios, ios_faces) == flat_face_outline(reference, windows_faces)); // same flat faces
        CHECK(triangle_set(ios_faces) == triangle_set(windows_faces));                           // same face SET
    }

    Condition count_condition(int shape, int condition_type, double width, double depth, double height,
                              double display_size, double z_value, bool is_top)
    {
        Condition cond = make_condition("c", condition_type == 3 ? ConditionType::Attachment : ConditionType::Count);
        cond.shape = static_cast<ShapeId>(shape);
        cond.width = width;
        cond.depth = depth;
        cond.height = height;
        cond.display_size = display_size;
        cond.z_value = z_value;
        cond.is_top = is_top;
        return cond;
    }
} // namespace

TEST(GeometricEquivalence, EveryCountShapeMatchesTheWindowsMesh)
{
    // All Windows MeshFactory count / attachment cases (box with every depth rule and a rotation, circle, ellipse,
    // triangle, rhombus, hexagon, octagon; rotated, display-scaled, top-anchored, z-offset): the same vertex set, flat
    // faces and triangles as Windows, outward-facing (the circle cylinder too since EXP-13).
    size_t checked = 0;
    for (const auto &c : ost_oracle::count_cases())
    {
        SCOPED_CONTEXT(c.name);
        Takeoff t = make_takeoff("t", "c", "p", {c.x, c.y}, "0", c.rotation);
        auto r = ost_meshing::create_mesh_for_takeoff(
            t, count_condition(c.shape, c.condition_type, c.width, c.depth, c.height, c.display_size, c.z_value, c.is_top),
            1.0);
        REQUIRE(r.valid);
        const std::string table = c.table;
        const Faces &wf = windows_count_faces(table);
        REQUIRE_EQ(wf.size(), static_cast<size_t>(c.face_count));
        Expect expect; // outward (EXP-13: the circle cylinder is outward-facing now too)
        check_geometrically_equivalent(r.mesh, to_ios_all(c.vertices), wf, std::fabs(c.signed_volume), expect);
        ++checked;
    }
    CHECK_EQ(checked, ost_oracle::count_cases().size());
}

TEST(GeometricEquivalence, RotatedAndRaisedBoxesAndCirclesMatchTheWindowsMesh)
{
    // Extra Windows MeshFactory runs: a rotated, display-scaled circle (rotation sense), a top-anchored attachment circle
    // at z 5, an attachment square at z 3 with a negative rotation. Same vertex set, flat faces and triangles as Windows.
    size_t checked = 0;
    for (const auto &c : ost_oracle::equiv_cases())
    {
        if (std::string(c.kind) != "count")
            continue;
        SCOPED_CONTEXT(c.name);
        Takeoff t = make_takeoff("t", "c", "p", c.position, "0", c.rotation);
        auto r = ost_meshing::create_mesh_for_takeoff(
            t, count_condition(c.shape, c.condition_type, c.width, c.depth, c.height, c.display_size, c.z_value, c.is_top),
            1.0);
        REQUIRE(r.valid);
        const bool circle = c.shape == static_cast<int>(ShapeId::Circle);
        Expect expect; // outward, circles included (EXP-13)
        check_geometrically_equivalent(r.mesh, to_ios_all(c.vertices),
                                       circle ? ost_oracle::faces_radial24() : windows_box_faces(), std::fabs(c.signed_volume),
                                       expect);
        ++checked;
    }
    CHECK_EQ(checked, static_cast<size_t>(3));
}

TEST(GeometricEquivalence, WallsMatchTheWindowsMesh)
{
    // The EXP-17 headline case: wall [0,0,100,0] height 96 thickness 6 (and an angled wall, and a curved wall with the
    // control [50,20]). Same vertex set, bounding box, |signed volume|, flat faces and triangles as Windows; the straight
    // and the curved wall are outward (EXP-13).
    size_t checked = 0;
    for (const auto &c : ost_oracle::equiv_cases())
    {
        if (std::string(c.kind) != "wall")
            continue;
        SCOPED_CONTEXT(c.name);
        Condition cond = make_condition("c", ConditionType::Linear);
        cond.height = c.height;
        cond.thickness = 6.0;
        Takeoff t = make_takeoff("t", "c", "p", c.position);
        t.curve = c.curve;
        auto r = ost_meshing::create_mesh_for_takeoff(t, cond, 1.0);
        REQUIRE(r.valid);
        // The Windows face list of the wall is the box table (straight) or the curved-wall table (curved): use the
        // oracle's own faces.
        Expect expect; // outward, curved walls included (EXP-13)
        check_geometrically_equivalent(r.mesh, to_ios_all(c.vertices), c.faces, std::fabs(c.signed_volume), expect);
        ++checked;
    }
    CHECK_EQ(checked, static_cast<size_t>(3));
}

TEST(GeometricEquivalence, SlopedWallsMatchTheWindowsMesh)
{
    // Walls with a slope (rise / run) and a z offset, straight and curved (Windows MeshFactory, no drop stub). The quads of
    // a sloped CURVED wall are not planar (the offset lines of two neighbouring curve points are not parallel while their
    // heights differ), so there the diagonal that splits a quad changes the surface: the triangles must be the Windows ones.
    size_t checked = 0;
    for (const auto &c : ost_oracle::wall_part_cases())
    {
        if (std::string(c.kind) != "wall")
            continue;
        SCOPED_CONTEXT(c.name);
        Condition cond = make_condition("c", ConditionType::Linear);
        cond.height = c.height;
        cond.thickness = c.thickness;
        cond.z_value = c.z_offset;
        cond.rise = c.rise;
        cond.run = c.run;
        Takeoff t = make_takeoff("t", "c", "p", c.position);
        t.curve = c.curve;
        auto r = ost_meshing::create_mesh_for_takeoff(t, cond, 1.0);
        REQUIRE(r.valid);
        Expect expect; // outward, curved walls included (EXP-13)
        check_geometrically_equivalent(r.mesh, to_ios_all(c.vertices), c.faces, std::fabs(c.signed_volume), expect);
        ++checked;
    }
    CHECK_EQ(checked, static_cast<size_t>(3));
}

TEST(GeometricEquivalence, DropStubsMatchTheWindowsMesh)
{
    // The drop stub that MeshFactory unions with the wall, before the union (Windows MeshGenerator.generate_drop_stub_mesh /
    // generate_curved_drop_stub_mesh): straight stubs (drop up and down, slope up and down, raised) and curved stubs.
    // Same vertex set, bounding box, |signed volume|, flat faces and triangles as Windows; the straight and the curved
    // stub are outward (EXP-13).
    size_t checked = 0;
    for (const auto &c : ost_oracle::wall_part_cases())
    {
        if (std::string(c.kind) != "stub")
            continue;
        SCOPED_CONTEXT(c.name);
        const auto &p = c.position;
        // MeshFactory frame: (x, z) = (-x_ost, -y_ost)
        Mesh stub = c.curve >= 0
                        ? ost_meshing::generate_curved_drop_stub_mesh(-p[0], -p[1], -p[2], -p[3], -p[4], -p[5], c.thickness,
                                                                      c.drop_value, c.z_offset, c.height, c.rise, c.run)
                        : ost_meshing::generate_drop_stub_mesh(-p[0], -p[1], -p[2], -p[3], c.thickness, c.drop_value,
                                                               c.z_offset, c.height, c.rise, c.run);
        Expect expect; // outward, curved stubs included (EXP-13)
        check_geometrically_equivalent(stub, to_ios_all(c.vertices), c.faces, std::fabs(c.signed_volume), expect);
        ++checked;
    }
    CHECK_EQ(checked, static_cast<size_t>(5));
}

TEST(GeometricEquivalence, EveryAreaOutlineHasTheWindowsVertexSetAndFaceSet)
{
    // The 13 Windows calc_area_faces outlines (CW / CCW, L-shape, concave, holes of both windings, two holes, pentagon with
    // a hole, triangle), thickness 1, flat: the same vertices and the same triangles (bottom cap, top cap, side walls),
    // GEO-14.
    for (const auto &c : ost_oracle::face_cases())
    {
        SCOPED_CONTEXT(c.name);
        std::vector<std::pair<double, double>> outer;
        std::vector<V3> windows; // Windows vertex order: outer bottom, outer top, then bottom / top of every hole
        for (double z : {0.0, 1.0})
            for (const auto &p : c.outer)
                windows.push_back({p[0], p[1], z});
        std::vector<std::vector<std::pair<double, double>>> holes;
        for (const auto &h : c.holes)
        {
            for (double z : {0.0, 1.0})
                for (const auto &p : h)
                    windows.push_back({p[0], p[1], z});
        }
        for (const auto &p : c.outer)
            outer.emplace_back(p[0], -p[1]);
        for (const auto &h : c.holes)
        {
            std::vector<std::pair<double, double>> ring;
            for (const auto &p : h)
                ring.emplace_back(p[0], -p[1]);
            holes.push_back(std::move(ring));
        }
        Mesh m = ost_meshing::generate_area_mesh(outer, 1.0, 0.0, holes, 0.0, 0.0, 0.0);
        double area = shoelace_area(outer);
        for (const auto &h : holes)
            area -= shoelace_area(h);
        check_geometrically_equivalent(m, to_ios_all(windows), c.faces, area, Expect{});
    }
}

TEST(GeometricEquivalence, FlatFaceOutlineIgnoresOnlyTheDiagonalOfAFlatQuad)
{
    // Guard of the helper the checks above rely on: re-splitting a flat quad along its other diagonal leaves the outline
    // unchanged, bending the quad (a vertex off the plane) or dropping a face changes it.
    Mesh box = ost_meshing::generate_count_mesh(0.0, 0.0, 4.0, 2.0, 3.0, 0.0, 0.0);
    REQUIRE_EQ(box.faces.size(), static_cast<size_t>(12));
    std::vector<uint32_t> identity;
    for (uint32_t i = 0; i < box.vertices.size(); ++i)
        identity.push_back(i);
    const auto outline = flat_face_outline(box, faces_in_reference_numbering(box, identity));
    CHECK_EQ(outline.size(), static_cast<size_t>(12)); // the 12 edges of a box, no diagonal

    Mesh split_02 = box; // bottom quad 0,1,2,3 split along 0-2 ...
    split_02.faces[0] = {0, 1, 2};
    split_02.faces[1] = {0, 2, 3};
    Mesh split_13 = box; // ... and along 1-3: two different triangle sets, the same flat face
    split_13.faces[0] = {0, 1, 3};
    split_13.faces[1] = {1, 2, 3};
    CHECK(triangle_set(split_02.faces) != triangle_set(split_13.faces));
    CHECK(flat_face_outline(split_02, faces_in_reference_numbering(split_02, identity)) == outline);
    CHECK(flat_face_outline(split_13, faces_in_reference_numbering(split_13, identity)) == outline);

    Mesh bent = box; // lift one top corner: the top quad is no longer flat, its diagonal becomes an outline edge
    bent.vertices[6][1] += 1.0;
    CHECK(flat_face_outline(bent, faces_in_reference_numbering(bent, identity)) != outline);

    Mesh open = box; // a missing face leaves boundary edges
    open.faces.pop_back();
    CHECK(flat_face_outline(open, faces_in_reference_numbering(open, identity)) != outline);
}
