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

        void add_mesh(const CppMeshData &mesh,
                      const std::string &group_name,
                      float r, float g, float b);
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
            CppMeshData mesh;
            OBJMaterial material;
        };

        std::vector<MeshGroup> groups_;

        static std::string sanitize_name(const std::string &name);
        static std::string replace_extension(const std::string &path,
                                             const std::string &ext);
        bool write_obj(const std::string &obj_path, const std::string &mtl_filename);
        bool write_mtl(const std::string &mtl_path);
    };

}
