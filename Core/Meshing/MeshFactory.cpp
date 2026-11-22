#include "../Geometry/strict_fp.hpp"
#include "MeshFactory.hpp"
#include "MeshGenerator.hpp"
#include "mesh_helpers.hpp"
#include "../Geometry/boolean_ops.hpp"
#include "../Geometry/mesh_repair.hpp"
#include "../Domain/CoreLog.hpp"
#include <cmath>
#include <algorithm>
#include <exception>
#include <optional>
#include <utility>

namespace ost_meshing
{

    static MeshResult make_result(ost_geometry::CppMeshData mesh,
                                  const ost_domain::Condition &c)
    {
        MeshResult r;
        r.mesh = std::move(mesh);
        c.get_rgb(r.color_r, r.color_g, r.color_b);
        r.valid = !r.mesh.vertices.empty();
        return r;
    }

    // module_geometry.cpp boolean_union_py: the native union, then remove_degenerate_faces on its result.
    static std::optional<ost_geometry::CppMeshData> native_drop_stub_union(const ost_geometry::CppMeshData &wall,
                                                                           const ost_geometry::CppMeshData &stub)
    {
        auto combined = ost_geometry::boolean_union(wall, stub);
        if (combined.has_value())
            ost_geometry::remove_degenerate_faces(*combined);
        return combined;
    }

    ost_geometry::CppMeshData union_with_drop_stub(const ost_domain::Takeoff &takeoff,
                                                   ost_geometry::CppMeshData wall,
                                                   const ost_geometry::CppMeshData &stub,
                                                   const DropStubUnionFn &union_fn)
    {
        // Windows _union_with_drop_stub: `except Exception` keeps the wall and logs "raised" with the exception; a
        // missing or face-less union keeps the wall and logs "failed". Exactly one warning, naming the takeoff uid.
        std::optional<ost_geometry::CppMeshData> combined;
        try
        {
            combined = union_fn ? union_fn(wall, stub) : native_drop_stub_union(wall, stub);
        }
        catch (const std::exception &e)
        {
            ost_log::warning("Drop stub union raised for takeoff " + takeoff.uid + "; keeping the wall mesh", e.what());
            return wall;
        }
        catch (...)
        {
            ost_log::warning("Drop stub union raised for takeoff " + takeoff.uid + "; keeping the wall mesh",
                             "unknown exception");
            return wall;
        }
        if (!combined.has_value() || combined->faces.empty())
        {
            ost_log::warning("Drop stub union failed for takeoff " + takeoff.uid + "; keeping the wall mesh");
            return wall;
        }
        return std::move(*combined);
    }

    static MeshResult create_linear(const ost_domain::Takeoff &t,
                                    const ost_domain::Condition &c,
                                    double sr)
    {
        if (t.position.size() < 4)
            return {};
        double x1 = -t.position[0] * sr, z1 = -t.position[1] * sr;
        double x2 = -t.position[2] * sr, z2 = -t.position[3] * sr;

        double height = std::max(c.height > 0 ? c.height : 1.0, 1.0) * sr;
        double thickness = std::max(c.thickness > 0 ? c.thickness : 1.0, 1.0) * sr;
        double y_offset = c.z_value * sr;

        if (c.is_top)
            y_offset -= height;

        double rise = c.rise, run = c.run;
        if (t.curve >= 0 && t.position.size() >= 6)
        {
            double cx = -t.position[4] * sr;
            double cz = -t.position[5] * sr;

            if (t.position.size() >= 7)
            {
                // Windows proc_curved_pos: the control is rebuilt from the chord midpoint and the perpendicular
                // offset only when the chord has a length; otherwise the stored control (position[4], [5]) stays.
                double offset = -t.position[6] * sr;
                double mx = (x1 + x2) * 0.5, mz = (z1 + z2) * 0.5;
                double dx = x2 - x1, dz = z2 - z1;
                double len = ost_fp::hypot(dx, dz);
                if (len > 0)
                {
                    dx /= len;
                    dz /= len;
                    cx = mx - dz * offset;
                    cz = mz + dx * offset;
                }
            }
            auto mesh = generate_curved_linear_mesh(x1, z1, x2, z2, cx, cz,
                                                    height, thickness, y_offset,
                                                    rise, run);
            if (mesh.is_valid() && c.drop_run && c.drop_value != 0.0)
            {
                auto stub = generate_curved_drop_stub_mesh(x1, z1, x2, z2, cx, cz,
                                                           thickness, c.drop_value,
                                                           y_offset, height,
                                                           rise, run);
                if (stub.is_valid())
                    mesh = union_with_drop_stub(t, std::move(mesh), stub);
            }
            return make_result(std::move(mesh), c);
        }

        auto mesh = generate_linear_mesh(x1, z1, x2, z2,
                                         height, thickness, y_offset,
                                         rise, run);
        if (mesh.is_valid() && c.drop_run && c.drop_value != 0.0)
        {
            auto stub = generate_drop_stub_mesh(x1, z1, x2, z2,
                                                thickness, c.drop_value,
                                                y_offset, height,
                                                rise, run);
            if (stub.is_valid())
                mesh = union_with_drop_stub(t, std::move(mesh), stub);
        }
        return make_result(std::move(mesh), c);
    }
    static MeshResult create_count(const ost_domain::Takeoff &t,
                                   const ost_domain::Condition &c,
                                   double sr)
    {
        if (t.position.size() < 2)
            return {};
        double x = -t.position[0] * sr, z = -t.position[1] * sr;
        int shape_id = static_cast<int>(c.shape);

        double width = std::max(c.width > 0 ? c.width : 1.0, 1.0);
        double depth;
        if (shape_id == static_cast<int>(ost_domain::ShapeId::Square) ||
            shape_id == static_cast<int>(ost_domain::ShapeId::Circle))
        {
            depth = width;
        }
        else
        {
            depth = std::max(c.depth != 0.0 ? c.depth : width, 1.0);
        }

        double height = std::max(c.height > 0 ? c.height : 1.0, 1.0);
        if (c.is_count())
        {
            double scale = std::max(c.display_size, 0.1) / 100.0;
            width *= scale;
            depth *= scale;
            height *= scale;
        }
        width *= sr;
        depth *= sr;
        height *= sr;
        double y_offset = c.z_value * sr;

        if (c.is_top)
            y_offset -= height;

        double rotation = t.rotation;
        // Windows _generate_count_mesh_from_spec: circle, ellipse, rhombus (width only), triangle, polygon prisms
        // (hexagon (6, 0), octagon (8, pi / 8)), everything else a box of width x depth.
        switch (c.shape)
        {
        case ost_domain::ShapeId::Circle:
            return make_result(generate_cylinder_mesh(x, z, width, height, y_offset, rotation), c);
        case ost_domain::ShapeId::Ellipse:
            return make_result(generate_elliptical_cylinder_mesh(x, z, width, depth, height, y_offset, rotation), c);
        case ost_domain::ShapeId::Rhombus:
            return make_result(generate_rhombus_prism_mesh(x, z, width, height, y_offset, rotation), c);
        case ost_domain::ShapeId::Triangle:
            return make_result(generate_isosceles_triangle_prism_mesh(x, z, width, depth, height, y_offset, rotation), c);
        case ost_domain::ShapeId::Hexagon:
            return make_result(generate_polygon_prism_mesh(x, z, width, height, 6, y_offset, rotation, 0.0), c);
        case ost_domain::ShapeId::Octagon:
            return make_result(generate_polygon_prism_mesh(x, z, width, height, 8, y_offset, rotation, M_PI / 8), c);
        default:
            break;
        }
        auto mesh = generate_count_mesh(x, z, width, depth, height,
                                        y_offset, rotation);
        return make_result(std::move(mesh), c);
    }
    static MeshResult create_area(const ost_domain::Takeoff &t,
                                  const ost_domain::Condition &c,
                                  double sr,
                                  const std::vector<std::vector<std::pair<double, double>>> &holes)
    {
        if (t.position.size() < 6)
            return {};

        double thickness = std::max(c.thickness > 0 ? c.thickness : 1.0, 1.0) * sr;
        double y_offset = c.z_value * sr;

        if (c.is_top)
            y_offset -= thickness;

        std::vector<std::pair<double, double>> verts;
        for (size_t j = 0; j + 1 < t.position.size(); j += 2)
        {
            verts.emplace_back(-t.position[j] * sr, -t.position[j + 1] * sr);
        }
        if (verts.size() < 3)
            return {};

        auto mesh = generate_area_mesh(verts, thickness, y_offset,
                                       holes, c.rise, c.run, t.rotation);
        return make_result(std::move(mesh), c);
    }

    MeshResult create_mesh_for_takeoff(
        const ost_domain::Takeoff &takeoff,
        const ost_domain::Condition &condition,
        double scale_ratio,
        const std::vector<std::vector<std::pair<double, double>>> &holes)
    {
        if (takeoff.position.empty())
            return {};

        if (condition.is_linear())
            return create_linear(takeoff, condition, scale_ratio);
        if (condition.is_area())
            return create_area(takeoff, condition, scale_ratio, holes);
        if (condition.is_count() || condition.is_attachment())
            return create_count(takeoff, condition, scale_ratio);

        return {};
    }

}
