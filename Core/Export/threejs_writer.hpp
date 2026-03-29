#pragma once
#include "../Geometry/mesh_data.hpp"
#include <string>
#include <vector>
#include <array>

namespace ost_geometry
{

    class ThreeJSWriter
    {
    public:
        ThreeJSWriter();
        ~ThreeJSWriter();

        void set_title(const std::string &title);
        void add_mesh(const CppMeshData &mesh,
                      const std::string &name,
                      float r, float g, float b, float opacity = 1.0f);
        bool save(const std::string &path);

        void clear();

    private:
        struct MeshEntry
        {
            CppMeshData mesh;
            std::string name;
            float r, g, b, opacity;
        };

        std::string title_;
        std::vector<MeshEntry> meshes_;

        std::string build_scene_json() const;
        static std::string html_template();
    };

}
