#pragma once
#include "../Geometry/mesh_data.hpp"
#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace ost_geometry
{

    struct OBJMaterial
    {
        std::string name;
        float r, g, b;
        OBJMaterial() : name("default"), r(0.8f), g(0.8f), b(0.8f) {}
        OBJMaterial(const std::string &n, float r_, float g_, float b_)
            : name(n), r(r_), g(g_), b(b_) {}
    };

    class OBJWriter
    {
    public:
        OBJWriter();
        ~OBJWriter();

        // One OBJ group + MTL material per call. `group_name` is the material name before sanitising (the Windows
        // exporter builds '<cdn type>_<condition name>_<uid[:8]>'); `original_name` is the text of the '# <name>' comment
        // lines (the Windows condition name). The overload without it uses the group name.
        void add_mesh(const CppMeshData &mesh,
                      const std::string &group_name,
                      float r, float g, float b);
        void add_mesh(const CppMeshData &mesh,
                      const std::string &group_name,
                      const std::string &original_name,
                      float r, float g, float b);
        // One OBJ group + MTL material for ALL the meshes of one condition (Windows OBJExporter._write_obj_file: '# name',
        // 'g', 'usemtl', then the meshes of the condition one after the other with continuous indices, then a blank line).
        // A mesh without vertices or faces writes nothing; a group without any mesh is still written, with its header and
        // its MTL entry (RB-03: a condition whose positives were all consumed keeps an empty group and material).
        // `group_name` / `original_name` as in add_mesh.
        void add_group(const std::string &group_name,
                       const std::string &original_name,
                       float r, float g, float b,
                       const std::vector<const CppMeshData *> &meshes);
        // The display mode written to the MTL header ('# Display Mode: <mode>'); default RenderConfig::DEFAULT_MODE.
        void set_display_mode(const std::string &mode);
        void add_layered_meshes(const std::vector<LayeredMesh> &meshes,
                                const std::vector<std::array<float, 3>> &colors);

        bool save(const std::string &obj_path);

        void clear();

        size_t get_total_vertices() const;
        size_t get_total_faces() const;

    private:
        struct MeshGroup
        {
            std::string name;
            std::string original_name;
            CppMeshData mesh;
            OBJMaterial material;
        };

        std::vector<MeshGroup> groups_;
        std::string display_mode_;

        static std::string sanitize_name(const std::string &name);
        static std::string single_line(const std::string &text);
        static std::string replace_extension(const std::string &path,
                                             const std::string &ext);
        bool write_obj(const std::string &obj_path, const std::string &mtl_filename);
        bool write_mtl(const std::string &mtl_path);
    };

}
