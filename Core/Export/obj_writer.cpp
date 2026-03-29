#include "obj_writer.hpp"
#include <fstream>
#include <cctype>
#include <cstdio>

namespace ost_geometry
{

    OBJWriter::OBJWriter() = default;
    OBJWriter::~OBJWriter() = default;

    void OBJWriter::add_mesh(const CppMeshData &mesh,
                             const std::string &group_name,
                             float r, float g, float b)
    {
        if (!mesh.is_valid())
            return;

        MeshGroup grp;
        grp.name = sanitize_name(group_name);
        grp.mesh = mesh;
        grp.material = OBJMaterial(grp.name, r, g, b);
        groups_.push_back(std::move(grp));
    }

    void OBJWriter::add_layered_meshes(const std::vector<LayeredMesh> &meshes,
                                       const std::vector<std::array<float, 3>> &colors)
    {
        for (size_t i = 0; i < meshes.size(); ++i)
        {
            float r = 0.8f, g = 0.8f, b = 0.8f;
            if (i < colors.size())
            {
                r = colors[i][0];
                g = colors[i][1];
                b = colors[i][2];
            }
            add_mesh(meshes[i].mesh, meshes[i].layer_name, r, g, b);
        }
    }

    bool OBJWriter::save(const std::string &obj_path)
    {
        if (groups_.empty())
            return false;

        std::string mtl_path = replace_extension(obj_path, ".mtl");
        std::string mtl_filename = mtl_path;
        auto sep = mtl_filename.find_last_of("/\\");
        if (sep != std::string::npos)
        {
            mtl_filename = mtl_filename.substr(sep + 1);
        }

        if (!write_mtl(mtl_path))
            return false;
        if (!write_obj(obj_path, mtl_filename))
            return false;

        return true;
    }

    void OBJWriter::clear()
    {
        groups_.clear();
    }

    size_t OBJWriter::get_total_vertices() const
    {
        size_t total = 0;
        for (const auto &g : groups_)
            total += g.mesh.vertices.size();
        return total;
    }

    size_t OBJWriter::get_total_faces() const
    {
        size_t total = 0;
        for (const auto &g : groups_)
            total += g.mesh.faces.size();
        return total;
    }

    std::string OBJWriter::sanitize_name(const std::string &name)
    {
        std::string result;
        result.reserve(name.size());
        for (char c : name)
        {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
            {
                result += c;
            }
            else if (c == ' ')
            {
                result += '_';
            }
        }
        if (result.empty())
            result = "mesh";
        return result;
    }

    std::string OBJWriter::replace_extension(const std::string &path,
                                             const std::string &ext)
    {
        auto dot = path.find_last_of('.');
        auto sep = path.find_last_of("/\\");

        if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
        {
            return path.substr(0, dot) + ext;
        }
        return path + ext;
    }

    bool OBJWriter::write_obj(const std::string &obj_path,
                              const std::string &mtl_filename)
    {
        std::ofstream file(obj_path);
        if (!file.is_open())
            return false;

        file << "# OBJ exported by OST Visualizer (iOS)\n";
        file << "mtllib " << mtl_filename << "\n\n";
        uint32_t vertex_offset = 0;

        for (const auto &grp : groups_)
        {
            file << "g " << grp.name << "\n";
            file << "usemtl " << grp.material.name << "\n";
            for (const auto &v : grp.mesh.vertices)
            {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "v %.6f %.6f %.6f\n", v[0], v[1], v[2]);
                file << buf;
            }
            for (const auto &f : grp.mesh.faces)
            {
                file << "f " << (f[0] + 1 + vertex_offset)
                     << " " << (f[1] + 1 + vertex_offset)
                     << " " << (f[2] + 1 + vertex_offset) << "\n";
            }

            vertex_offset += static_cast<uint32_t>(grp.mesh.vertices.size());
            file << "\n";
        }

        return file.good();
    }

    bool OBJWriter::write_mtl(const std::string &mtl_path)
    {
        std::ofstream file(mtl_path);
        if (!file.is_open())
            return false;

        file << "# MTL exported by OST Visualizer (iOS)\n\n";

        for (const auto &grp : groups_)
        {
            const auto &m = grp.material;
            file << "newmtl " << m.name << "\n";
            char buf[128];
            std::snprintf(buf, sizeof(buf), "Kd %.4f %.4f %.4f\n", m.r, m.g, m.b);
            file << buf;
            file << "Ka 0.1000 0.1000 0.1000\n";
            file << "Ks 0.0000 0.0000 0.0000\n";
            file << "d 1.0000\n";
            file << "illum 1\n\n";
        }

        return file.good();
    }

}
