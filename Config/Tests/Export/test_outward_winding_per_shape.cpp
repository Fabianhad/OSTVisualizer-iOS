// Outward winding of every 3D shape (Windows d1c430a "outward faces"): the pin tests of the Windows
// tests/integration/geometry/test_outward_winding_per_shape.py (14 shapes x 3 scenes through the 3D view, the HTML scene and
// the OBJ export) and test_uncut_winding_with_negatives.py (the uncut wall and rhombus with a negative elsewhere).
// iOS already stores every mesh outward (positive signed volume): these are pins of existing behaviour, not a change. The
// Windows FBX leg has no iOS counterpart (iOS has no FBX export).
//
// Entry points: Core/Meshing/ExportMeshSet.hpp (generate_view_meshes, generate_html_export, generate_condition_export); the
// OBJ leg writes the groups of generate_condition_export through OBJWriter::add_group exactly as Services/ExportService.mm
// does and reads the file back. The cases that need the real boolean difference (the sloped/straight drop wall union, the cut
// slab) run in the '#if OST_HAVE_BOOLEAN' twin (-Boolean); the stand-in difference keeps the positive as it is.
// iOS mesh frame: X = -x_ost, Y up, Z = -y_ost (a rotation of the Windows frame, so signed volumes keep their sign).
#include "ExportRuleTestSupport.hpp"

#include "Export/obj_writer.hpp"
#include "Meshing/MeshFactory.hpp"

#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

using namespace rule_test;
using export_test::closed_and_consistently_oriented;
using export_test::parse_obj;
using export_test::read_text_lf;
using export_test::ScratchDir;
using export_test::signed_volume;
using ost_domain::ShapeId;
using ost_meshing::BooleanDifferenceFn;
using ost_meshing::ExportStatus;

namespace
{
    ost_domain::RenderConfig solid_config()
    {
        ost_domain::RenderConfig c;
        c.color_mode = ost_domain::RenderConfig::MODE_SOLID;
        c.grayscale_enabled = false;
        return c;
    }

    const std::set<std::string> kAllPages;

    // The stand-in of the host build without Manifold: the negative of the scene cuts nothing the test looks at, so the
    // positive goes through the pass unchanged (the pass itself is pinned in test_boolean_rule.cpp).
    BooleanDifferenceFn keep_positive_fn()
    {
        return [](const Mesh &p, const std::vector<Mesh> &) -> std::optional<Mesh> { return p; };
    }

    // ---- the scene of test_outward_winding_per_shape.py ----
    Condition count_condition(const std::string &uid, ShapeId shape)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.condition_type = ConditionType::Count;
        c.shape = shape;
        c.width = 20.0;
        c.depth = 12.0;
        c.height = 10.0;
        c.display_size = 100.0;
        return c;
    }

    Condition attachment_condition(const std::string &uid)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.condition_type = ConditionType::Attachment;
        c.shape = ShapeId::Square;
        c.width = 5.0;
        c.depth = 5.0;
        c.height = 5.0;
        c.display_size = 100.0;
        return c;
    }

    Condition wall_condition(const std::string &uid)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.condition_type = ConditionType::Linear;
        c.height = 10.0;
        c.thickness = 2.0;
        return c;
    }

    Condition area_condition(const std::string &uid)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.condition_type = ConditionType::Area;
        c.thickness = 4.0;
        return c;
    }

    const std::vector<std::string> kCountUids = {"square", "rectangle", "circle", "ellipse", "triangle",
                                                 "rhombus", "hexagon",   "octagon", "attachment"};

    std::vector<Condition> shape_conditions()
    {
        Condition drop = wall_condition("dropwall");
        drop.drop_run = true;
        drop.drop_value = 4.0;
        Condition sloped = area_condition("sloped");
        sloped.rise = 1.0;
        sloped.run = 12.0;
        return {wall_condition("wall"),
                wall_condition("curved"),
                drop,
                area_condition("slab"),
                sloped,
                count_condition("square", ShapeId::Square),
                count_condition("rectangle", ShapeId::Rectangle),
                count_condition("circle", ShapeId::Circle),
                count_condition("ellipse", ShapeId::Ellipse),
                count_condition("triangle", ShapeId::Triangle),
                count_condition("rhombus", ShapeId::Rhombus),
                count_condition("hexagon", ShapeId::Hexagon),
                count_condition("octagon", ShapeId::Octagon),
                attachment_condition("attachment"),
                area_condition("hole"),
                attachment_condition("other")};
    }

    std::vector<Takeoff> shape_takeoffs()
    {
        std::vector<Takeoff> v;
        v.push_back(makeoff("wall", "wall", "p", {0.0, 0.0, 40.0, 0.0}));
        Takeoff curved = makeoff("curved", "curved", "p", {0.0, 100.0, 40.0, 100.0, 20.0, 90.0});
        curved.curve = 0;
        v.push_back(curved);
        v.push_back(makeoff("dropwall", "dropwall", "p", {0.0, 200.0, 40.0, 200.0}));
        v.push_back(makeoff("slab", "slab", "p", {100.0, 0.0, 160.0, 0.0, 160.0, 60.0, 100.0, 60.0}));
        v.push_back(makeoff("sloped", "sloped", "p", {200.0, 0.0, 260.0, 0.0, 260.0, 60.0, 200.0, 60.0}));
        for (size_t i = 0; i < kCountUids.size(); ++i)
            v.push_back(makeoff(kCountUids[i], kCountUids[i], "p", {400.0 + 60.0 * static_cast<double>(i), 300.0}));
        return v;
    }

    std::vector<std::string> shape_uids()
    {
        std::vector<std::string> v;
        for (const auto &t : shape_takeoffs())
            v.push_back(t.uid);
        return v;
    }

    struct Scene
    {
        const char *name;
        std::vector<Takeoff> takeoffs;
    };

    std::vector<Scene> shape_scenes()
    {
        std::vector<Scene> scenes;
        scenes.push_back({"no negative", shape_takeoffs()});
        auto other = shape_takeoffs();
        other.push_back(makeoff("n", "other", "p", {900.0, 900.0}, true));
        scenes.push_back({"other-type negative apart", other});
        auto same = shape_takeoffs();
        same.push_back(makeoff("n", "hole", "p", {120.0, 20.0, 140.0, 20.0, 140.0, 40.0, 120.0, 40.0}, true));
        scenes.push_back({"same-type negative cutting the slab", same});
        return scenes;
    }

    // ---- readers ----
    // Port of _winding(vertices, triangles, up_axis): (signed volume rounded to 6 places, side faces whose normal points away
    // from the centroid of the used vertices, side faces = faces whose normal is not along the up axis).
    struct Winding
    {
        double volume = 0.0;
        int outward = 0;
        int sides = 0;
        bool operator==(const Winding &o) const { return volume == o.volume && outward == o.outward && sides == o.sides; }
    };

    Winding winding_of(const Mesh &mesh, int up_axis)
    {
        using V = std::array<double, 3>;
        Winding w;
        w.volume = std::round(signed_volume(mesh) * 1e6) / 1e6;
        std::vector<bool> used(mesh.vertices.size(), false);
        for (const auto &f : mesh.faces)
            for (int k = 0; k < 3; ++k)
                used[f[static_cast<size_t>(k)]] = true;
        V centre{0.0, 0.0, 0.0};
        size_t n = 0;
        for (size_t i = 0; i < used.size(); ++i)
            if (used[i])
            {
                for (size_t a = 0; a < 3; ++a)
                    centre[a] += mesh.vertices[i][a];
                ++n;
            }
        for (auto &c : centre)
            c /= static_cast<double>(n);
        for (const auto &f : mesh.faces)
        {
            const V &a = mesh.vertices[f[0]];
            const V &b = mesh.vertices[f[1]];
            const V &c = mesh.vertices[f[2]];
            const V e1{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
            const V e2{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            V normal{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
            const double length = std::sqrt(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
            for (auto &x : normal)
                x /= (length == 0.0 ? 1.0 : length);
            if (std::fabs(normal[static_cast<size_t>(up_axis)]) > 0.9)
                continue;
            ++w.sides;
            double dot = 0.0;
            for (size_t k = 0; k < 3; ++k)
                dot += normal[k] * ((a[k] + b[k] + c[k]) / 3.0 - centre[k]);
            if (dot > 0.0)
                ++w.outward;
        }
        return w;
    }

    // What an OBJ export of the groups holds: the file written by OBJWriter::add_group (as ExportService.mm does), read
    // back, one mesh per condition group (every shape has a condition of its own; empty groups give an empty mesh).
    std::map<std::string, Mesh> obj_meshes_of(const ost_meshing::GroupedExport &grouped, const std::string &scratch)
    {
        ost_geometry::OBJWriter writer;
        writer.set_display_mode(ost_domain::RenderConfig::MODE_SOLID);
        for (const auto &group : grouped.groups)
            writer.add_group(group.material_name, group.name, group.color.r, group.color.g, group.color.b,
                             group.mesh_pointers());
        ScratchDir dir(scratch);
        REQUIRE(writer.save(dir.file("w.obj")));
        const auto data = parse_obj(read_text_lf(dir.file("w.obj")));
        std::map<std::string, Mesh> out;
        size_t vertex_offset = 0, face_offset = 0;
        for (const auto &group : grouped.groups)
        {
            Mesh one;
            for (const auto &m : group.meshes)
            {
                for (size_t i = 0; i < m.mesh.vertices.size(); ++i)
                    one.vertices.push_back(data.vertices.at(vertex_offset + i));
                for (size_t i = 0; i < m.mesh.faces.size(); ++i)
                {
                    auto f = data.faces.at(face_offset + i);
                    for (auto &index : f)
                        index = static_cast<uint32_t>(index - vertex_offset);
                    one.faces.push_back(f);
                }
                vertex_offset += m.mesh.vertices.size();
                face_offset += m.mesh.faces.size();
            }
            out[group.condition_uid] = one;
        }
        REQUIRE_EQ(vertex_offset, data.vertices.size());
        REQUIRE_EQ(face_offset, data.faces.size());
        return out;
    }

    std::map<std::string, Mesh> by_uid(const std::vector<ost_meshing::BooleanMesh> &meshes)
    {
        std::map<std::string, Mesh> out;
        for (const auto &m : meshes)
            out[m.takeoff->uid] = m.mesh;
        return out;
    }

    void check_closed_outward(const std::map<std::string, Mesh> &meshes, const std::vector<std::string> &uids)
    {
        for (const auto &uid : uids)
        {
            SCOPED_CONTEXT("shape " + uid);
            const auto it = meshes.find(uid);
            REQUIRE(it != meshes.end());
            CHECK(closed_and_consistently_oriented(it->second.faces));
            CHECK(signed_volume(it->second) > 0.0); // outward-facing
        }
    }

    // test_every_shape_is_a_closed_outward_mesh_in_the_3d_view / _html_export / _obj_export and
    // test_the_signed_volume_does_not_depend_on_a_negative_elsewhere
    void check_every_shape(const BooleanDifferenceFn &difference)
    {
        const auto config = solid_config();
        const auto project_conditions = shape_conditions();
        const auto uids = shape_uids();
        REQUIRE_EQ(uids.size(), static_cast<size_t>(14));
        std::map<std::string, std::map<std::string, double>> volumes; // scene -> uid -> rounded 3 decimals
        for (const auto &scene : shape_scenes())
        {
            SCOPED_CONTEXT(std::string("scene: ") + scene.name);
            const auto project = project_of(project_conditions, scene.takeoffs);

            const auto view = ost_meshing::generate_view_meshes(project, kAllPages, config, difference);
            const auto view_meshes = by_uid(view);
            {
                SCOPED_CONTEXT("3D view");
                REQUIRE_EQ(view_meshes.size(), uids.size());
                check_closed_outward(view_meshes, uids);
            }

            const auto html = ost_meshing::generate_html_export(project, kAllPages, config, difference);
            REQUIRE(html.status == ExportStatus::Ok);
            {
                SCOPED_CONTEXT("HTML export");
                const auto html_meshes = by_uid(html.meshes);
                REQUIRE_EQ(html_meshes.size(), uids.size());
                check_closed_outward(html_meshes, uids);
            }

            const auto grouped = ost_meshing::generate_condition_export(project, kAllPages, config, difference);
            REQUIRE(grouped.status == ExportStatus::Ok);
            {
                SCOPED_CONTEXT("OBJ export (file read back)");
                const auto obj = obj_meshes_of(grouped, "winding_shapes");
                // 14 shapes (+ the empty groups of the negative-only conditions of the scene)
                for (const auto &uid : uids)
                {
                    SCOPED_CONTEXT("shape " + uid);
                    const auto it = obj.find(uid);
                    REQUIRE(it != obj.end());
                    CHECK(closed_and_consistently_oriented(it->second.faces));
                    CHECK(signed_volume(it->second) > 0.0);
                }
            }

            auto &round_volumes = volumes[scene.name];
            for (const auto &uid : uids)
                if (uid != "slab") // the cut slab is the one shape the negative changes
                    round_volumes[uid] = std::round(signed_volume(view_meshes.at(uid)) * 1e3) / 1e3;
        }
        // test_the_signed_volume_does_not_depend_on_a_negative_elsewhere
        CHECK(volumes["other-type negative apart"] == volumes["no negative"]);
        CHECK(volumes["same-type negative cutting the slab"] == volumes["no negative"]);
    }

    // ---- test_uncut_winding_with_negatives.py ----
    std::vector<Condition> uncut_conditions()
    {
        return {wall_condition("wall"), count_condition("rhombus", ShapeId::Rhombus), attachment_condition("block"),
                wall_condition("hole")};
    }

    // Windows (volume, outward faces, faces): view and html {'w': (800.0, 8, 8), 'r': (1150.0, 8, 8)}; obj (read by the
    // Windows frame, up axis Z of the OBJ frame) wall (800.0, 8, 8), rhombus (1150.0, 12, 12). The iOS mesh frame has Y up.
    void check_uncut_scene(const BooleanDifferenceFn &difference, const char *name, const std::vector<Takeoff> &takeoffs)
    {
        SCOPED_CONTEXT(std::string("scene: ") + name);
        const auto project = project_of(uncut_conditions(), takeoffs);
        const auto config = solid_config();
        const Winding wall{800.0, 8, 8}, rhombus{1150.0, 8, 8};

        const auto view = by_uid(ost_meshing::generate_view_meshes(project, kAllPages, config, difference));
        REQUIRE_EQ(view.size(), static_cast<size_t>(2));
        CHECK(winding_of(view.at("w"), 1) == wall);
        CHECK(winding_of(view.at("r"), 1) == rhombus);

        const auto html = ost_meshing::generate_html_export(project, kAllPages, config, difference);
        REQUIRE(html.status == ExportStatus::Ok);
        const auto html_meshes = by_uid(html.meshes);
        REQUIRE_EQ(html_meshes.size(), static_cast<size_t>(2));
        CHECK(winding_of(html_meshes.at("w"), 1) == wall);
        CHECK(winding_of(html_meshes.at("r"), 1) == rhombus);

        const auto grouped = ost_meshing::generate_condition_export(project, kAllPages, config, difference);
        REQUIRE(grouped.status == ExportStatus::Ok);
        const auto obj = obj_meshes_of(grouped, "winding_uncut");
        CHECK(winding_of(obj.at("wall"), 1) == wall);
        CHECK(winding_of(obj.at("rhombus"), 1) == rhombus);
        // the Windows reading of the OBJ (up axis = third axis of its frame): the Windows tuples (800.0, 8, 8) and (1150.0, 12, 12)
        CHECK(winding_of(obj.at("wall"), 2) == (Winding{800.0, 8, 8}));
        CHECK(winding_of(obj.at("rhombus"), 2) == (Winding{1150.0, 12, 12}));
    }

    void check_uncut_meshes(const BooleanDifferenceFn &difference)
    {
        const std::vector<Takeoff> base = {makeoff("w", "wall", "p", {0.0, 0.0, 40.0, 0.0}),
                                           makeoff("r", "rhombus", "p", {80.0, 0.0})};
        check_uncut_scene(difference, "no negative", base);
        auto other = base;
        other.push_back(makeoff("n", "block", "p", {500.0, 500.0}, true));
        check_uncut_scene(difference, "other-type negative", other);
        auto same = base;
        same.push_back(makeoff("n", "hole", "p", {500.0, 500.0, 540.0, 500.0}, true));
        check_uncut_scene(difference, "same-type negative apart", same);
    }
} // namespace

TEST(OutwardWindingPerShape, EveryShapeIsAClosedOutwardMeshInTheViewHtmlAndObjWithTheStandIn)
{
    // 14 shapes of the Windows test (wall, curved wall, drop wall, slab, sloped slab, square, rectangle, circle, ellipse,
    // triangle, rhombus, hexagon, octagon, attachment) x 3 scenes (no negative; an other-type negative apart; a same-type
    // negative over the slab). Without Manifold the drop stub union falls back to the wall (ost_log warning) and the
    // difference keeps the positive.
    check_every_shape(keep_positive_fn());
}

TEST(OutwardWindingPerShape, TheUncutWallAndRhombusAreOutwardWithANegativeElsewhereWithTheStandIn)
{
    check_uncut_meshes(keep_positive_fn());
}

#if OST_HAVE_BOOLEAN
TEST(OutwardWindingPerShapeManifold, EveryShapeIsAClosedOutwardMeshInTheViewHtmlAndObjWithManifold)
{
    // The same 14 shapes x 3 scenes with the real boolean difference and the real drop stub union (the 'dropwall' of the
    // scene is the union of the wall and its stub): every mesh, the cut slab and the union included, is outward-facing.
    check_every_shape({});
}

TEST(OutwardWindingPerShapeManifold, TheUncutWallAndRhombusAreOutwardWithANegativeElsewhereWithManifold)
{
    check_uncut_meshes({});
}

TEST(OutwardWindingPerShapeManifold, TheDropWallUnionIsOutwardForEveryAngleSlopeAndDropDirection)
{
    // The drop wall of the scene is the Manifold union of the wall and its stub (MeshFactory union_with_drop_stub, repaired by
    // remove_degenerate_faces). test_drop_stub_union.cpp compares |volume| only; here the sign: the union is outward-facing
    // for walls at an angle, with and without slope, a drop up and down, straight and curved (Windows d1c430a: every
    // 3D mesh is outward, the union included).
    for (double rise : {0.0, 1.0, -2.0})
        for (double run : {0.0, 4.0})
            for (double drop : {12.0, -12.0})
                for (double angle : {0.0, 0.7})
                {
                    SCOPED_CONTEXT("rise " + std::to_string(rise) + " run " + std::to_string(run) + " drop " +
                                   std::to_string(drop) + " angle " + std::to_string(angle));
                    Condition c = wall_condition("c");
                    c.height = 96.0;
                    c.thickness = 6.0;
                    c.rise = rise;
                    c.run = run;
                    c.drop_run = true;
                    c.drop_value = drop;
                    for (const bool curved : {false, true})
                    {
                        std::vector<double> position{0.0, 0.0, 120.0 * std::cos(angle), 120.0 * std::sin(angle)};
                        if (curved)
                        {
                            position.push_back(60.0);
                            position.push_back(50.0);
                        }
                        Takeoff t = makeoff("t", "c", "p", position);
                        if (curved)
                            t.curve = 0;
                        const auto r = ost_meshing::create_mesh_for_takeoff(t, c, 1.0);
                        REQUIRE(r.valid);
                        CHECK(signed_volume(r.mesh) > 0.0);
                    }
                }
}
#endif
