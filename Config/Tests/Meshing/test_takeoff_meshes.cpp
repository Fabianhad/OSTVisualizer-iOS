// Takeoff -> mesh orchestration shared by the 3D view and the exports:
//   MSH-01 (raw OST units, the page scale ratio never scales a mesh), MSH-02 (page rotation / flip applied to the 3D
//   view meshes), MSH-14 (hole classification), EXP-03 (holes use the same transform as the outline; render and export
//   agree).
// Windows oracle: infrastructure/visualization_provider._MeshGeneratorAdapter.generate_meshes ->
// meshing/mesh_builder.process_takeoffs_to_meshes (+ _apply_page_transform), fed like
// VisualizationService.refresh_mesh_view does (visible takeoffs of the selected pages); numbers in WindowsOracles.hpp.
// iOS frame of a Windows vertex (X, Y, Z): (X, Z, -Y).
#include "MeshingTestUtil.hpp"
#include "WindowsOracles.hpp"

#include <cmath>
#include <limits>
#include <string>

using namespace mesh_test;

namespace
{
    ost_data::ProjectData project_with(const ost_domain::Page &page, const std::vector<Condition> &conds,
                                       const std::vector<Takeoff> &takeoffs)
    {
        ost_data::ProjectData p;
        add(p, page);
        for (const auto &c : conds)
            add(p, c);
        for (const auto &t : takeoffs)
            add(p, t);
        return p;
    }

    Condition area_cond(double thickness = 1.0, double z = 0.0)
    {
        Condition c = make_condition("area", ConditionType::Area);
        c.thickness = thickness;
        c.z_value = z;
        return c;
    }

    Condition linear_cond(double height = 96.0, double thickness = 6.0)
    {
        Condition c = make_condition("lin", ConditionType::Linear);
        c.height = height;
        c.thickness = thickness;
        return c;
    }

    const std::vector<double> kArea{4.0, 4.0, 10.0, 4.0, 8.0, 8.0, 4.0, 8.0};
    const std::vector<double> kAsymArea{1.0, 1.0, 2.5, 1.0, 2.0, 2.0, 1.0, 2.0};

    // The single mesh of a one-takeoff project.
    Mesh single_mesh(const ost_data::ProjectData &p, bool transform = true)
    {
        auto v = build(p, {}, transform);
        REQUIRE_EQ(v.size(), static_cast<size_t>(1));
        return v[0].mesh;
    }
} // namespace

// ---------------------------------------------------------------- MSH-01
TEST(MeshScale, ScalePortOfWindowsModelScaleTest)
{
    // Port of tests/integration/rendering/test_page_flip_mesh.py::PageFlip3DMeshTests::
    //   test_mesh_pipeline_preserves_model_scale_when_flipping
    // page sf1 = 1, sf2 = 4 (ratio 4), 720 x 360 pt, area thickness 1, takeoff [4,4,10,4,8,8,4,8].
    // Windows XY bounds (-10,-4,4,8) (the ratio is ignored); flip_x (-36,-30,4,8).
    auto base = project_with(make_page("p", 720, 360, 1.0, 4.0), {area_cond()}, {make_takeoff("t", "area", "p", kArea)});
    Mesh m = single_mesh(base);
    Bounds b = bounds_of(m);
    CHECK_NEAR(b.lo[0], -10.0, 1e-9);
    CHECK_NEAR(b.hi[0], -4.0, 1e-9);
    CHECK_NEAR(b.lo[2], -8.0, 1e-9); // iOS Z = -Y_windows
    CHECK_NEAR(b.hi[2], -4.0, 1e-9);
    CHECK_NEAR(b.lo[1], 0.0, 1e-12);
    CHECK_NEAR(b.hi[1], 1.0, 1e-12);
    auto flipped = project_with(make_page("p", 720, 360, 1.0, 4.0, 0, true, false), {area_cond()},
                                {make_takeoff("t", "area", "p", kArea)});
    Bounds f = bounds_of(single_mesh(flipped));
    CHECK_NEAR(f.lo[0], -36.0, 1e-9);
    CHECK_NEAR(f.hi[0], -30.0, 1e-9);
    CHECK_NEAR(f.lo[2], -8.0, 1e-9);
    CHECK_NEAR(f.hi[2], -4.0, 1e-9);
}

TEST(MeshScale, LinearIsMeshedInRawOstUnitsOnAnyPageRatio)
{
    // MSH-01 plan: linear [0,0,120,0], height 96, thickness 6 on a page with sf2/sf1 = 4: length 120, height 96,
    // thickness 6 (iOS used to give 30 / 24 / 1.5). EXP-04: sf2 = 48, [0,0,100,0]: x -100..0, y 0..96, z -3..3.
    for (const auto &c : ost_oracle::linear_misc_cases())
    {
        if (c.rotation != 0)
            continue;
        SCOPED_CONTEXT(c.name);
        auto p = project_with(make_page("p", 720, 360, 1.0, c.sf2), {linear_cond()},
                              {make_takeoff("t", "lin", "p", {c.x1, c.y1, c.x2, c.y2})});
        Mesh m = single_mesh(p);
        check_bounds_ios(m, c.bounds, 1e-9);
        CHECK(vertex_sets_equal(m.vertices, to_ios_all(c.vertices), 1e-9));
        CHECK_NEAR(std::fabs(signed_volume(m)), std::fabs(c.x2 - c.x1) * 96.0 * 6.0, 1e-6);
    }
}

TEST(MeshScale, LinearOnARatio96PageMatchesWindows)
{
    // LP-01 plan: linear [960,480,1920,960], height 96, thickness 6, page 612 x 792 pt, sf1 = 0.125, sf2 = 12 (ratio 96):
    // Windows x[-1921.3416,-958.6584] y[477.3167,962.6833] z[0,96]; the same bounds with sf1 = sf2 = 1.
    for (const auto &c : ost_oracle::linear_ratio96_cases())
    {
        if (c.rotation != 0 || c.flip_x || c.flip_y)
            continue;
        SCOPED_CONTEXT(c.name);
        auto p = project_with(make_page("p", 612, 792, c.sf1, c.sf2), {linear_cond()},
                              {make_takeoff("t", "lin", "p", {960.0, 480.0, 1920.0, 960.0})});
        Mesh m = single_mesh(p);
        check_bounds_ios(m, c.bounds, 1e-6);
        CHECK(vertex_sets_equal(m.vertices, to_ios_all(c.vertices), 1e-6));
    }
}

TEST(MeshScale, TwoPagesWithDifferentRatiosDoNotRescaleRelativeToEachOther)
{
    // EXP-04: pages with different sf2 / sf1 must not rescale relative to each other.
    ost_data::ProjectData p;
    add(p, make_page("a", 720, 360, 1.0, 1.0));
    add(p, make_page("b", 720, 360, 1.0, 48.0));
    add(p, linear_cond());
    add(p, make_takeoff("ta", "lin", "a", {0, 0, 100, 0}));
    add(p, make_takeoff("tb", "lin", "b", {0, 0, 100, 0}));
    auto v = build(p);
    REQUIRE_EQ(v.size(), static_cast<size_t>(2));
    const Mesh &ma = find_mesh(v, "ta")->mesh;
    const Mesh &mb = find_mesh(v, "tb")->mesh;
    REQUIRE_EQ(ma.vertices.size(), mb.vertices.size());
    for (size_t i = 0; i < ma.vertices.size(); ++i)
        for (int k = 0; k < 3; ++k)
            CHECK_EQ(ma.vertices[i][k], mb.vertices[i][k]);
    Bounds b = bounds_of(ma);
    CHECK_NEAR(b.lo[0], -100.0, 1e-9);
    CHECK_NEAR(b.hi[0], 0.0, 1e-9);
    CHECK_NEAR(b.lo[1], 0.0, 1e-12);
    CHECK_NEAR(b.hi[1], 96.0, 1e-12);
    CHECK_NEAR(b.lo[2], -3.0, 1e-9);
    CHECK_NEAR(b.hi[2], 3.0, 1e-9);
}

TEST(MeshScale, ZeroScaleFactorFallsBackLikeWindows)
{
    // Windows _snapshot_page_render_info: float(page.scale_factor2 or 1.0): a 0 factor is 1.0, never an infinite
    // scale. Neither the meshes (raw units) nor the page transform may produce a non-finite coordinate.
    auto p = project_with(make_page("p", 720, 360, 1.0, 0.0, 90), {area_cond()}, {make_takeoff("t", "area", "p", kArea)});
    Mesh m = single_mesh(p);
    for (const auto &v : m.vertices)
        for (int k = 0; k < 3; ++k)
            CHECK(std::isfinite(v[k]));
    auto q = project_with(make_page("p", 720, 360, 1.0, 1.0, 90), {area_cond()}, {make_takeoff("t", "area", "p", kArea)});
    Mesh n = single_mesh(q);
    REQUIRE_EQ(m.vertices.size(), n.vertices.size());
    for (size_t i = 0; i < m.vertices.size(); ++i)
        for (int k = 0; k < 3; ++k)
            CHECK_EQ(m.vertices[i][k], n.vertices[i][k]);
}

// ---------------------------------------------------------------- MSH-02
TEST(PageTransform3D, EveryRotationAndFlipComboMatchesWindows)
{
    // GEO-03 plan: page 720 x 360 pt, ratio 1 and ratio 4 (sf1 = 1, sf2 = 4: page box 40 x 20), area
    // [4,4,10,4,8,8,4,8], thickness 1: Windows vertices of all 32 rotation x flip combinations, index by index
    // (rot0: none (-10,-4,4,8), flipX (-6,0,4,8), rot90 none (-8,-4,0,6), rot270 flipY (-8,-4,4,10), ...).
    for (const auto &c : ost_oracle::page_combos())
    {
        SCOPED_CONTEXT(c.name);
        auto p = project_with(make_page("p", 720, 360, 1.0, c.sf2, c.rotation, c.flip_x, c.flip_y), {area_cond()},
                              {make_takeoff("t", "area", "p", kArea)});
        Mesh m = single_mesh(p);
        check_vertices_index_wise(m, c.vertices, 1e-9);
        check_bounds_ios(m, c.bounds, 1e-9);
        // Same triangles as Windows (faces are reversed exactly when one flip is set, which keeps the winding).
        CHECK(triangle_set(m.faces) == triangle_set(c.faces));
        CHECK(closed_and_consistent(m));
        CHECK(signed_volume(m) > 0.0); // never turned inside out
        CHECK_NEAR(signed_volume(m), 20.0, 1e-9);
    }
}

TEST(PageTransform3D, AsymmetricAreaFlipsAndRotationsPortOfWindowsTests)
{
    // Port of tests/presentation/visualization/meshing/test_mesh_factory.py::MeshFactoryPageFlipTests and
    // tests/integration/rendering/test_page_flip_mesh.py: page 720 x 360 pt, area [1,1,2.5,1,2,2,1,2], thickness 1:
    // none (-2.5,-1,1,2); flip_x (-9,-7.5,1,2); flip_y (-2.5,-1,3,4); both (-9,-7.5,3,4);
    // rot90 (-2,-1,7.5,9); 180 (-9,-7.5,3,4); 270 (-4,-3,1,2.5).
    for (const auto &c : ost_oracle::asym_cases())
    {
        SCOPED_CONTEXT(c.name);
        auto p = project_with(make_page("p", 720, 360, 1.0, 1.0, c.rotation, c.flip_x, c.flip_y), {area_cond()},
                              {make_takeoff("t", "area", "p", kAsymArea)});
        Mesh m = single_mesh(p);
        check_vertices_index_wise(m, c.vertices, 1e-9);
        check_bounds_ios(m, c.bounds, 1e-9);
        CHECK(triangle_set(m.faces) == triangle_set(c.faces));
        CHECK_NEAR(signed_volume(m), 1.25, 1e-9); // Windows asserts |volume| 1.25 for every flip
    }
}

TEST(PageTransform3D, FlipKeepsTheShellOrientationOfTheUnflippedMesh)
{
    // Port of ...::test_flip_keeps_top_face_winding_direction_of_unflipped_mesh: a mirrored mesh needs its faces
    // reversed to keep facing the same way; a double flip is a rotation and must not be reversed. The top-face
    // normal (Y component in iOS axes) keeps the sign of the unflipped mesh for all four flip combinations.
    auto top_normals = [&](bool fx, bool fy)
    {
        auto p = project_with(make_page("p", 720, 360, 1.0, 1.0, 0, fx, fy), {area_cond()},
                              {make_takeoff("t", "area", "p", kAsymArea)});
        Mesh m = single_mesh(p);
        double top_y = bounds_of(m).hi[1];
        std::vector<double> normals;
        for (const auto &f : m.faces)
        {
            const V3 &a = m.vertices[f[0]];
            const V3 &b = m.vertices[f[1]];
            const V3 &c = m.vertices[f[2]];
            if (a[1] == top_y && b[1] == top_y && c[1] == top_y)
                normals.push_back((b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2])); // Y of (b-a) x (c-a)
        }
        return normals;
    };
    auto baseline = top_normals(false, false);
    REQUIRE(!baseline.empty());
    const double outward = baseline[0] > 0.0 ? 1.0 : -1.0;
    for (double n : baseline)
        CHECK(n * outward > 0.0);
    for (auto fl : {std::make_pair(true, false), std::make_pair(false, true), std::make_pair(true, true)})
    {
        SCOPED_CONTEXT(std::string("flip_x ") + (fl.first ? "1" : "0") + " flip_y " + (fl.second ? "1" : "0"));
        auto normals = top_normals(fl.first, fl.second);
        CHECK_EQ(normals.size(), baseline.size());
        for (double n : normals)
            CHECK(n * outward > 0.0);
    }
}

TEST(PageTransform3D, FlipTransformsEveryPointMeshVertex)
{
    // Port of ...::test_flip_transforms_every_point_mesh_vertex_not_only_its_anchor: triangle (width 2, depth 3,
    // height 1, rotation 0.35) at [2,1] on a 720 x 360 page: with flip_x each vertex (x, y, z) becomes (-10 - x, y, z).
    Condition tri = make_condition("tri", ConditionType::Count);
    tri.shape = ShapeId::Triangle;
    tri.width = 2.0;
    tri.depth = 3.0;
    tri.height = 1.0;
    tri.display_size = 100.0;
    for (const auto &c : ost_oracle::triangle_page_cases())
    {
        SCOPED_CONTEXT(c.name);
        auto p = project_with(make_page("p", 720, 360, 1.0, 1.0, c.rotation, c.flip_x, c.flip_y), {tri},
                              {make_takeoff("t", "tri", "p", {2.0, 1.0}, "0", 0.35)});
        Mesh m = single_mesh(p);
        check_vertices_index_wise(m, c.vertices, 1e-9);
        check_bounds_ios(m, c.bounds, 1e-9);
        CHECK(triangle_set(m.faces) == triangle_set(c.faces));
        CHECK(signed_volume(m) > 0.0);
    }
    // the plain statement of the Windows test
    auto base = single_mesh(project_with(make_page("p", 720, 360), {tri}, {make_takeoff("t", "tri", "p", {2.0, 1.0}, "0", 0.35)}));
    auto flip = single_mesh(project_with(make_page("p", 720, 360, 1.0, 1.0, 0, true, false), {tri},
                                         {make_takeoff("t", "tri", "p", {2.0, 1.0}, "0", 0.35)}));
    REQUIRE_EQ(base.vertices.size(), flip.vertices.size());
    for (size_t i = 0; i < base.vertices.size(); ++i)
    {
        CHECK_NEAR(flip.vertices[i][0], -10.0 - base.vertices[i][0], 1e-9);
        CHECK_NEAR(flip.vertices[i][1], base.vertices[i][1], 1e-12);
        CHECK_NEAR(flip.vertices[i][2], base.vertices[i][2], 1e-9);
    }
}

TEST(PageTransform3D, LinearOnARatio96PageWithRotationAndFlips)
{
    // LP-02 plan (Windows run): linear [960,480,1920,960], height 96, thickness 6, page 612 x 792 pt, sf 0.125 / 12:
    // rotation 90 x[-962.6833,-477.3167] y[-1105.3416,-142.6584]; rotation 180 x[142.6584,1105.3416]
    // y[93.3167,578.6833]; flip_x x[142.6584,1105.3416] y[477.3167,962.6833]; plus 270 / flip_y / 90 + flip_x.
    for (const auto &c : ost_oracle::linear_ratio96_cases())
    {
        SCOPED_CONTEXT(c.name);
        auto p = project_with(make_page("p", 612, 792, c.sf1, c.sf2, c.rotation, c.flip_x, c.flip_y), {linear_cond()},
                              {make_takeoff("t", "lin", "p", {960.0, 480.0, 1920.0, 960.0})});
        Mesh m = single_mesh(p);
        check_bounds_ios(m, c.bounds, 1e-6);
        CHECK(vertex_sets_equal(m.vertices, to_ios_all(c.vertices), 1e-6));
        CHECK(closed_and_consistent(m));
        CHECK(signed_volume(m) > 0.0);
    }
}

TEST(PageTransform3D, EachPageUsesItsOwnTransform)
{
    // Port of ...::test_mesh_pipeline_selects_independent_transform_for_each_page: the same area on a flip_x page and
    // on a flip_y page: (-9,-7.5,1,2) and (-2.5,-1,3,4).
    ost_data::ProjectData p;
    add(p, make_page("page-horizontal", 720, 360, 1.0, 1.0, 0, true, false));
    add(p, make_page("page-vertical", 720, 360, 1.0, 1.0, 0, false, true));
    add(p, area_cond());
    add(p, make_takeoff("first", "area", "page-horizontal", kAsymArea));
    add(p, make_takeoff("second", "area", "page-vertical", kAsymArea));
    auto v = build(p);
    REQUIRE_EQ(v.size(), static_cast<size_t>(2));
    const double first[6] = {-9.0, -7.5, 1.0, 2.0, 0.0, 1.0};
    const double second[6] = {-2.5, -1.0, 3.0, 4.0, 0.0, 1.0};
    check_bounds_ios(find_mesh(v, "first")->mesh, first, 1e-9);
    check_bounds_ios(find_mesh(v, "second")->mesh, second, 1e-9);
}

TEST(PageTransform3D, ExportsStayUntransformed)
{
    // MSH-02 note / LP-02: the transform belongs to the live 3D scene; OBJ / DXF / HTML exports stay untransformed.
    auto p = project_with(make_page("p", 720, 360, 1.0, 4.0, 90, true, false), {area_cond()},
                          {make_takeoff("t", "area", "p", kArea)});
    Mesh exported = single_mesh(p, /*transform=*/false);
    const double plain[6] = {-10.0, -4.0, 4.0, 8.0, 0.0, 1.0};
    check_bounds_ios(exported, plain, 1e-9);
    Mesh scene = single_mesh(p, true);
    Bounds a = bounds_of(exported), b = bounds_of(scene);
    CHECK(std::fabs(a.lo[0] - b.lo[0]) > 1.0);
}

TEST(PageTransform3D, PagesWithoutAUsableSizeOrRecordAreNotTransformed)
{
    // Windows _MeshGeneratorAdapter: a page whose size is non-finite or <= 0 (width_pts * ratio) is skipped, and a
    // takeoff of an unknown page has no coordinate system: both stay untransformed.
    const double plain[6] = {-10.0, -4.0, 4.0, 8.0, 0.0, 1.0};
    auto zero_width = project_with(make_page("p", 0, 360, 1.0, 4.0, 90, true, false), {area_cond()},
                                   {make_takeoff("t", "area", "p", kArea)});
    check_bounds_ios(single_mesh(zero_width), plain, 1e-9);
    auto negative_ratio = project_with(make_page("p", 720, 360, 1.0, -4.0, 90, true, false), {area_cond()},
                                       {make_takeoff("t", "area", "p", kArea)});
    check_bounds_ios(single_mesh(negative_ratio), plain, 1e-9);
    auto missing = project_with(make_page("p", 720, 360, 1.0, 1.0, 90), {area_cond()},
                                {make_takeoff("t", "area", "nopage", kArea)});
    check_bounds_ios(single_mesh(missing), plain, 1e-9);
}

TEST(PageTransform3D, RotationIsReducedModuloAFullTurn)
{
    // Windows OSTCoordinateSystem._validate_page_info: int(rotation) % 360 (360 -> none, -90 -> 270, 450 -> 90).
    struct C
    {
        int rotation;
        double b[6];
    };
    const C cases[] = {{360, {-10, -4, 4, 8, 0, 1}}, {-90, {-1, 3, 4, 10, 0, 1}}, {450, {-8, -4, 0, 6, 0, 1}}};
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT("rotation " + std::to_string(c.rotation));
        auto p = project_with(make_page("p", 720, 360, 1.0, 1.0, c.rotation), {area_cond()},
                              {make_takeoff("t", "area", "p", kArea)});
        check_bounds_ios(single_mesh(p), c.b, 1e-9);
    }
}

TEST(PageTransform3D, RotationThatIsNotAQuarterTurnFailsLikeWindows)
{
    // Windows: ValueError("Page rotation must be a multiple of 90 degrees") from ost_to_pdf_coordinates; the scene
    // generation fails (the caller shows an empty scene). The C++ counterpart is std::invalid_argument.
    auto p = project_with(make_page("p", 720, 360, 1.0, 1.0, 45), {area_cond()}, {make_takeoff("t", "area", "p", kArea)});
    std::string what;
    CHECK(throws_invalid_argument([&] { build(p); }, &what));
    CHECK_STR_EQ(what, "Page rotation must be a multiple of 90 degrees");
    // not transformed (exports) or without any mesh on that page: no failure, like Windows
    CHECK(!throws_invalid_argument([&] { build(p, {}, false); }));
    ost_data::ProjectData empty;
    add(empty, make_page("p", 720, 360, 1.0, 1.0, 45));
    add(empty, area_cond());
    CHECK(!throws_invalid_argument([&] { build(empty); }));
}

// ---------------------------------------------------------------- MSH-14 / EXP-03
namespace
{
    ost_data::ProjectData holes_project(const std::vector<Condition> &conds, const std::vector<Takeoff> &takeoffs)
    {
        return project_with(make_page("p", 720, 360), conds, takeoffs);
    }

    Condition slab_cond()
    {
        Condition c = make_condition("slab", ConditionType::Area);
        c.thickness = 6.0;
        c.z_value = 96.0;
        return c;
    }

    const std::vector<double> kBig{0, 0, 100, 0, 100, 100, 0, 100};
    const std::vector<double> kHole{20, 20, 40, 20, 40, 40, 20, 40};

    // Compare the iOS result of a Windows hole case: the same takeoffs, vertex / face counts, signed volume and
    // bounds (the iOS result order is the project order, so match by takeoff uid).
    void check_hole_case(const char *name, const ost_data::ProjectData &p)
    {
        const ost_oracle::HoleCase *wc = nullptr;
        for (const auto &c : ost_oracle::hole_cases())
            if (std::string(c.name) == name)
                wc = &c;
        REQUIRE(wc != nullptr);
        auto v = build(p);
        CHECK_EQ(v.size(), wc->meshes.size());
        for (const auto &w : wc->meshes)
        {
            SCOPED_CONTEXT(std::string(name) + " / " + w.takeoff_uid);
            const auto *m = find_mesh(v, w.takeoff_uid);
            REQUIRE(m != nullptr);
            CHECK_EQ(m->mesh.vertices.size(), static_cast<size_t>(w.vertex_count));
            CHECK_EQ(m->mesh.faces.size(), static_cast<size_t>(w.face_count));
            CHECK_NEAR(std::fabs(signed_volume(m->mesh)), std::fabs(w.signed_volume), 1e-6);
            check_bounds_ios(m->mesh, w.bounds, 1e-9);
        }
    }
} // namespace

TEST(HoleMeshes, SlabWithAHoleChildIsCutAndTheChildIsNotMeshedOnItsOwn)
{
    // EXP-03 plan: slab (area, thickness 6, z 96) OST [0,0,100,0,100,100,0,100] plus a hole takeoff (same condition,
    // parent = slab) [20,20,40,20,40,40,20,40]: Windows 16 vertices, 32 faces, bbox x[-100,0] y[96,102] z[-100,0],
    // |volume| 57600 (iOS export used to mirror the hole: z[-100,40], 20 faces).
    auto p = holes_project({slab_cond()}, {make_takeoff("slab1", "slab", "p", kBig),
                                           make_takeoff("hole1", "slab", "p", kHole, "slab1")});
    check_hole_case("slab_with_hole", p);
    Mesh m = single_mesh(p);
    check_vertices_index_wise(m, ost_oracle::slab_with_hole_vertices(), 1e-9);
    CHECK(triangle_set(m.faces) == triangle_set(ost_oracle::slab_with_hole_faces()));
    CHECK(closed_and_consistent(m));
    CHECK_NEAR(signed_volume(m), 57600.0, 1e-6);
    // every hole vertex lies inside the outline's footprint (hole centre (-30, -30) in iOS axes)
    for (size_t i = 8; i < 16; ++i)
    {
        CHECK(m.vertices[i][0] > -100.0 && m.vertices[i][0] < 0.0);
        CHECK(m.vertices[i][2] > -100.0 && m.vertices[i][2] < 0.0);
    }
}

TEST(HoleMeshes, RenderAndExportProduceTheSameMesh)
{
    // EXP-03 / MSH-12 / GEO-02 / LP-06: render path and export path share the hole map: for an unrotated page the
    // two meshes are identical, hole ring at (-4,-2) x (-4,-2) inside the outline for the small example.
    auto p = holes_project({[] { Condition c = make_condition("slab", ConditionType::Area); c.thickness = 1.0; return c; }()},
                           {make_takeoff("outer", "slab", "p", {0, 0, 10, 0, 10, 10, 0, 10}),
                            make_takeoff("h", "slab", "p", {2, 2, 4, 2, 4, 4, 2, 4}, "outer")});
    Mesh render = single_mesh(p, true);
    Mesh exported = single_mesh(p, false);
    REQUIRE_EQ(render.vertices.size(), static_cast<size_t>(16));
    REQUIRE_EQ(render.vertices.size(), exported.vertices.size());
    REQUIRE_EQ(render.faces.size(), exported.faces.size());
    for (size_t i = 0; i < render.vertices.size(); ++i)
        for (int k = 0; k < 3; ++k)
            CHECK_EQ(render.vertices[i][k], exported.vertices[i][k]);
    for (size_t i = 0; i < render.faces.size(); ++i)
        CHECK(render.faces[i] == exported.faces[i]);
    // hole vertices (8..11 bottom, 12..15 top): (x, z) = (-x_ost, -y_ost) in {-4,-2}^2
    for (size_t i = 8; i < 16; ++i)
    {
        CHECK(exported.vertices[i][0] >= -4.0 - 1e-12 && exported.vertices[i][0] <= -2.0 + 1e-12);
        CHECK(exported.vertices[i][2] >= -4.0 - 1e-12 && exported.vertices[i][2] <= -2.0 + 1e-12);
    }
    // MSH-12 plan: top-face area 1500 for a 40 x 40 slab with a 10 x 10 hole
    auto q = holes_project({[] { Condition c = make_condition("slab", ConditionType::Area); c.thickness = 1.0; return c; }()},
                           {make_takeoff("outer", "slab", "p", {0, 0, 40, 0, 40, 40, 0, 40}),
                            make_takeoff("h", "slab", "p", {10, 10, 20, 10, 20, 20, 10, 20}, "outer")});
    Mesh em = single_mesh(q, false);
    CHECK_EQ(em.vertices.size(), static_cast<size_t>(16));
    CHECK_EQ(em.faces.size(), static_cast<size_t>(32));
    double top_area = 0.0;
    for (const auto &f : em.faces)
    {
        const V3 &a = em.vertices[f[0]];
        const V3 &b = em.vertices[f[1]];
        const V3 &c = em.vertices[f[2]];
        if (a[1] == 1.0 && b[1] == 1.0 && c[1] == 1.0)
            top_area += 0.5 * std::fabs((b[0] - a[0]) * (c[2] - a[2]) - (c[0] - a[0]) * (b[2] - a[2]));
    }
    CHECK_NEAR(top_area, 1500.0, 1e-9);
}

TEST(HoleMeshes, HolesShareThePageTransformWithTheOutline)
{
    // The hole is part of the same mesh: rotation 90 + flip_x on a ratio 4 page (Windows run).
    auto p = project_with(make_page("p", 720, 360, 1.0, 4.0, 90, true, false), {slab_cond()},
                          {make_takeoff("slab1", "slab", "p", kBig), make_takeoff("hole1", "slab", "p", kHole, "slab1")});
    Mesh m = single_mesh(p);
    check_vertices_index_wise(m, ost_oracle::hole_page_vertices(), 1e-9);
    CHECK(triangle_set(m.faces) == triangle_set(ost_oracle::hole_page_faces()));
    CHECK(closed_and_consistent(m));
}

TEST(HoleMeshes, OrphanHoleIsMeshedAsAnOrdinaryArea)
{
    // GEO-04 / LP-14 / EXP-10: a child whose parent is not among the processed takeoffs is not a hole: it is an
    // ordinary area (Windows: 8 vertices, 12 faces, |volume| 2400, x[-40,-20] y[20,40]).
    auto p = holes_project({slab_cond()}, {make_takeoff("lone", "slab", "p", kHole, "gone")});
    check_hole_case("orphan_hole", p);
}

TEST(HoleMeshes, HoleUnderANonAreaParentStaysAnOrdinaryTakeoff)
{
    // GEO-04 second case: line1 (linear) and stray (area, parent = line1): both are meshed.
    Condition line = make_condition("line", ConditionType::Linear);
    line.height = 96.0;
    line.thickness = 6.0;
    auto p = holes_project({slab_cond(), line}, {make_takeoff("line1", "line", "p", {0, 0, 100, 0}),
                                                 make_takeoff("stray", "slab", "p", kHole, "line1")});
    check_hole_case("hole_under_line", p);
}

TEST(HoleMeshes, LinearCountAndAttachmentChildrenOfAnAreaAreMeshedAsTheirOwnElements)
{
    // MSH-14 plan / EXP-M03 / LP-14: parented Linear / Count / Attachment children of an area are meshed (iOS skipped
    // them) and are NOT holes: the slab stays a full 8-vertex area.
    Condition line = make_condition("line", ConditionType::Linear);
    line.height = 96.0;
    line.thickness = 6.0;
    Condition cnt = make_condition("cnt", ConditionType::Count);
    cnt.shape = ShapeId::Square;
    cnt.width = 10.0;
    cnt.height = 10.0;
    Condition att = make_condition("att", ConditionType::Attachment);
    att.shape = ShapeId::Square;
    att.width = 10.0;
    att.height = 10.0;
    auto p = holes_project({slab_cond(), line, cnt, att},
                           {make_takeoff("slab1", "slab", "p", kBig),
                            make_takeoff("l1", "line", "p", {0, 200, 100, 200}, "slab1"),
                            make_takeoff("c1", "cnt", "p", {50, 50}, "slab1"),
                            make_takeoff("a1", "att", "p", {5, 5}, "slab1")});
    check_hole_case("children_of_slab", p);
    // EXP-M03: slab + one linear child: 2 meshes, 16 vertices
    auto q = holes_project({slab_cond(), line}, {make_takeoff("parent", "slab", "p", kBig),
                                                 make_takeoff("lin1", "line", "p", {0, 200, 100, 200}, "parent")});
    check_hole_case("exp_m03", q);
    auto v = build(q, {}, false);
    size_t total = 0;
    for (const auto &m : v)
        total += m.mesh.vertices.size();
    CHECK_EQ(total, static_cast<size_t>(16));
}

TEST(HoleMeshes, ParentedCurvedLinearIsNotAHole)
{
    // MSH-14 plan: a parented curved linear with 6 values must not create a hole.
    Condition line = make_condition("line", ConditionType::Linear);
    line.height = 96.0;
    line.thickness = 6.0;
    auto p = holes_project({slab_cond(), line}, {make_takeoff("slab1", "slab", "p", kBig),
                                                 [] {
                                                     Takeoff t = make_takeoff("cv", "line", "p", {20, 20, 60, 20, 40, 50}, "slab1");
                                                     t.curve = 0;
                                                     return t;
                                                 }()});
    check_hole_case("curved6_child", p);
}

TEST(HoleMeshes, HoleWhoseConditionLayerIsHiddenDoesNotCutItsParent)
{
    // MSH-14 plan: a hole whose condition layer is hidden is not cut (Windows pipes only visible takeoffs into the
    // hole grouping); the slab stays whole and the hole is not drawn.
    Condition hid = make_condition("hid", ConditionType::Area);
    hid.thickness = 6.0;
    hid.z_value = 96.0;
    hid.layer_uid = "L1";
    auto p = holes_project({slab_cond(), hid}, {make_takeoff("slab1", "slab", "p", kBig),
                                                make_takeoff("hole1", "hid", "p", kHole, "slab1")});
    ost_domain::Layer layer;
    layer.uid = "L1";
    layer.visible = false;
    p.layers["L1"] = layer;
    check_hole_case("hidden_layer_hole", p);
    // visible again: the hole cuts
    p.layers["L1"].visible = true;
    auto v = build(p);
    REQUIRE_EQ(v.size(), static_cast<size_t>(1));
    CHECK_EQ(v[0].mesh.vertices.size(), static_cast<size_t>(16));
}

TEST(HoleMeshes, SeveralHolesAndTooShortHoleRings)
{
    // Windows get_holes_for_takeoff: a ring needs 6 values and 3 points; [1,1,2,2] (4 values) is skipped:
    // 24 vertices, 52 faces for two valid holes.
    auto p = holes_project({slab_cond()},
                           {make_takeoff("slab1", "slab", "p", kBig), make_takeoff("h1", "slab", "p", kHole, "slab1"),
                            make_takeoff("h2", "slab", "p", {60, 60, 80, 60, 80, 90, 60, 90}, "slab1"),
                            make_takeoff("short", "slab", "p", {1, 1, 2, 2}, "slab1")});
    check_hole_case("two_holes", p);
}

TEST(HoleMeshes, SelectedPagesLimitTheHoleGrouping)
{
    // The grouping runs on the selected pages' takeoffs only: a hole on an unselected page neither cuts nor is drawn.
    ost_data::ProjectData p;
    add(p, make_page("p", 720, 360));
    add(p, make_page("q", 720, 360));
    add(p, slab_cond());
    add(p, make_takeoff("slab1", "slab", "p", kBig));
    add(p, make_takeoff("hole1", "slab", "q", kHole, "slab1"));
    auto only_p = build(p, {"p"});
    REQUIRE_EQ(only_p.size(), static_cast<size_t>(1));
    CHECK_EQ(only_p[0].mesh.vertices.size(), static_cast<size_t>(8));
    // an empty selection means every page (the export): the hole child is a hole of the slab
    auto all = build(p, {});
    REQUIRE_EQ(all.size(), static_cast<size_t>(1));
    CHECK_EQ(all[0].mesh.vertices.size(), static_cast<size_t>(16));
}

TEST(HoleMeshes, ConditionsMissingOrHiddenAreSkipped)
{
    ost_data::ProjectData p;
    add(p, make_page("p", 720, 360));
    add(p, slab_cond());
    add(p, make_takeoff("nocond", "ghost", "p", kBig));
    add(p, make_takeoff("ok", "slab", "p", kBig));
    auto v = build(p);
    CHECK_EQ(uids(v), (std::set<std::string>{"ok"}));
}

TEST(HoleMeshes, ResultCarriesTheConditionAndTheTakeoff)
{
    Condition c = slab_cond();
    c.color_fill = 0x0000FF; // BGR int: r = 255
    auto p = holes_project({c}, {make_takeoff("slab1", "slab", "p", kBig)});
    auto v = build(p);
    REQUIRE_EQ(v.size(), static_cast<size_t>(1));
    CHECK(v[0].takeoff == &p.takeoffs[0]);
    CHECK(v[0].condition == &p.conditions.at("slab"));
    CHECK_NEAR(v[0].color_r, 1.0f, 1e-6);
    CHECK_NEAR(v[0].color_g, 0.0f, 1e-6);
}
