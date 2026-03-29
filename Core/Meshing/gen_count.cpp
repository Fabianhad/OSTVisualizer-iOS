#include "MeshGenerator.hpp"
#include "mesh_helpers.hpp"

namespace ost_meshing
{

    ost_geometry::CppMeshData generate_count_mesh(
        double x, double z,
        double width, double depth, double height,
        double y_offset, double rotation)
    {
        ost_geometry::CppMeshData mesh;

        double hw = width / 2.0, hd = depth / 2.0;
        double corners[4][2] = {
            {-hw, -hd}, {hw, -hd}, {hw, hd}, {-hw, hd}};
        if (std::abs(rotation) > 1e-9)
        {
            double cos_r = std::cos(rotation), sin_r = std::sin(rotation);
            for (int ci = 0; ci < 4; ci++)
            {
                double cx = corners[ci][0], cz = corners[ci][1];
                corners[ci][0] = cx * cos_r - cz * sin_r;
                corners[ci][1] = cx * sin_r + cz * cos_r;
            }
        }

        double y_b = y_offset, y_t = y_offset + height;
        mesh.vertices.resize(8);
        for (int i = 0; i < 4; i++)
        {
            double tx = x + corners[i][0], tz = z + corners[i][1];
            mesh.vertices[i] = {tx, y_b, tz};
            mesh.vertices[i + 4] = {tx, y_t, tz};
        }

        add_box_faces(mesh);
        return mesh;
    }

    ost_geometry::CppMeshData generate_cylinder_mesh(
        double x, double z,
        double diameter, double height,
        double y_offset, double rotation,
        int segments)
    {
        ost_geometry::CppMeshData mesh;
        if (segments < 3)
            return mesh;

        double rx = diameter / 2.0;
        double y_b = y_offset, y_t = y_offset + height;

        mesh.vertices.resize(static_cast<size_t>(segments * 2 + 2));

        for (int i = 0; i < segments; i++)
        {
            double a = 2.0 * M_PI * i / segments;
            double px = rx * std::cos(a);
            double pz = rx * std::sin(a);

            if (std::abs(rotation) > 1e-9)
            {
                double cos_r = std::cos(rotation), sin_r = std::sin(rotation);
                double rpx = px * cos_r - pz * sin_r;
                double rpz = px * sin_r + pz * cos_r;
                px = rpx;
                pz = rpz;
            }

            double tx = x + px, tz = z + pz;
            mesh.vertices[i] = {tx, y_b, tz};
            mesh.vertices[i + segments] = {tx, y_t, tz};
        }

        mesh.vertices[segments * 2] = {x, y_b, z};
        mesh.vertices[segments * 2 + 1] = {x, y_t, z};
        int n = segments;
        int bc = n * 2;
        int tc = n * 2 + 1;

        mesh.faces.reserve(static_cast<size_t>(n * 4));

        for (int i = 0; i < n; i++)
        {
            int j = (i + 1) % n;
            uint32_t ui = static_cast<uint32_t>(i);
            uint32_t uj = static_cast<uint32_t>(j);
            uint32_t uin = static_cast<uint32_t>(i + n);
            uint32_t ujn = static_cast<uint32_t>(j + n);
            uint32_t ubc = static_cast<uint32_t>(bc);
            uint32_t utc = static_cast<uint32_t>(tc);
            mesh.faces.push_back({ubc, uj, ui});
            mesh.faces.push_back({utc, uin, ujn});
            mesh.faces.push_back({ui, uj, ujn});
            mesh.faces.push_back({ui, ujn, uin});
        }

        return mesh;
    }

}
