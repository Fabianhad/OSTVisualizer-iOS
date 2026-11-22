#include "obj_writer.hpp"
#include "export_text.hpp"
#include "../Domain/RenderConfig.hpp"
#include <fstream>
#include <cstdio>

namespace ost_geometry
{

    OBJWriter::OBJWriter() : display_mode_(ost_domain::RenderConfig::DEFAULT_MODE) {}
    OBJWriter::~OBJWriter() = default;

    void OBJWriter::add_mesh(const CppMeshData &mesh,
                             const std::string &group_name,
                             float r, float g, float b)
    {
        add_mesh(mesh, group_name, group_name, r, g, b);
    }

    void OBJWriter::add_mesh(const CppMeshData &mesh,
                             const std::string &group_name,
                             const std::string &original_name,
                             float r, float g, float b)
    {
        if (!mesh.is_valid())
            return;

        MeshGroup grp;
        grp.name = sanitize_name(group_name);
        grp.original_name = original_name;
        grp.mesh = mesh;
        grp.material = OBJMaterial(grp.name, r, g, b);
        groups_.push_back(std::move(grp));
    }

    void OBJWriter::add_group(const std::string &group_name,
                              const std::string &original_name,
                              float r, float g, float b,
                              const std::vector<const CppMeshData *> &meshes)
    {
        MeshGroup grp;
        grp.name = sanitize_name(group_name);
        grp.original_name = original_name;
        grp.material = OBJMaterial(grp.name, r, g, b);
        // OBJMeshWriter.write_mesh per mesh: the vertices of each mesh, then its faces shifted by the vertices written
        // before; `if not mesh_data or not mesh_data.vertices` writes nothing.
        for (const CppMeshData *mesh : meshes)
        {
            if (mesh == nullptr || !mesh->is_valid())
                continue;
            const uint32_t offset = static_cast<uint32_t>(grp.mesh.vertices.size());
            grp.mesh.vertices.insert(grp.mesh.vertices.end(), mesh->vertices.begin(), mesh->vertices.end());
            for (const auto &f : mesh->faces)
                grp.mesh.faces.push_back({f[0] + offset, f[1] + offset, f[2] + offset});
        }
        groups_.push_back(std::move(grp));
    }

    void OBJWriter::set_display_mode(const std::string &mode)
    {
        display_mode_ = mode;
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
        return export_text::sanitize_material_name(name);
    }

    // The text of a '# ...' comment line (EXP-16, Windows b9902e6 obj_exporter.py _comment_text): every CR and every LF is
    // one space ('A\r\nB' gives 'A  B', no collapsing), so a name cannot end the comment and turn the rest of it into OBJ / MTL
    // statements. Every other character stays as it is, TAB and the other control characters included, like Windows. (The
    // '# Display Mode:' header goes through it too; Windows writes that one raw, no difference for the fixed mode labels.)
    std::string OBJWriter::single_line(const std::string &text)
    {
        std::string out = text;
        for (char &c : out)
            if (c == '\n' || c == '\r')
                c = ' ';
        return out;
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

        // Windows OBJExporter._write_obj_file: header, mtllib, then per group '# <original name>', 'g', 'usemtl' and the
        // vertices / faces with indices continuing across groups, a blank line after the group.
        file << "# Created by OST Visualizer\n";
        file << "mtllib " << mtl_filename << "\n\n";
        uint32_t vertex_offset = 0;

        for (const auto &grp : groups_)
        {
            file << "# " << single_line(grp.original_name) << "\n";
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

        // Windows OBJExporter._write_mtl_file. Kd is the colour byte over 255 in double ({:.6f}).
        file << "# Created by OST Visualizer\n";
        file << "# Display Mode: " << single_line(display_mode_) << "\n";
        file << "# Materials: " << groups_.size() << "\n\n";

        for (const auto &grp : groups_)
        {
            const auto &m = grp.material;
            file << "newmtl " << m.name << "\n";
            file << "# " << single_line(grp.original_name) << "\n";
            char buf[128];
            std::snprintf(buf, sizeof(buf), "Kd %.6f %.6f %.6f\n",
                          export_text::colour_channel(m.r), export_text::colour_channel(m.g),
                          export_text::colour_channel(m.b));
            file << buf;
            file << "Ka 0.200000 0.200000 0.200000\n";
            file << "Ks 0.500000 0.500000 0.500000\n";
            file << "Ns 20.000000\n";
            file << "Ni 1.000000\n";
            file << "d 1.000000\n";
            file << "illum 2\n\n";
        }

        return file.good();
    }

}
