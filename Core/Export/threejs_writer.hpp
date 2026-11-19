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

        // The title is HTML-escaped into the <title> of the page; placeholders in it are not expanded.
        void set_title(const std::string &title);
        // `mesh` is in the iOS mesh frame (Y up), which is the frame of the viewer: vertices and normals are written as
        // they are. `name` is written as a JSON string (escaped). The colour is the float of a colour byte (byte / 255.0f)
        // and is written as the Windows double byte / 255.0; `opacity` is written as the decimal it was typed as.
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
