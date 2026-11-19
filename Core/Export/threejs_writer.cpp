#include "../Geometry/strict_fp.hpp"
#include "threejs_writer.hpp"
#include "export_text.hpp"
#include <fstream>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace ost_geometry
{

    namespace
    {
        // Windows threejs_renderer._generate_html: ONE regex pass over the template replaces {{TITLE}} and {{SCENE_DATA}}
        // (and {{SUBTITLE}}, which this template does not have), so placeholders inside the title or in the scene text are
        // never expanded. The title is html.escape(title, quote=False).
        std::string fill_template(const std::string &tpl, const std::string &title, const std::string &scene_json)
        {
            static const std::string kTitle = "{{TITLE}}";
            static const std::string kScene = "{{SCENE_DATA}}";
            const std::string escaped_title = export_text::html_escape(title);
            std::string out;
            out.reserve(tpl.size() + escaped_title.size() + scene_json.size());
            size_t pos = 0;
            while (pos < tpl.size())
            {
                const size_t open = tpl.find("{{", pos);
                if (open == std::string::npos)
                {
                    out.append(tpl, pos, std::string::npos);
                    break;
                }
                out.append(tpl, pos, open - pos);
                if (tpl.compare(open, kTitle.size(), kTitle) == 0)
                {
                    out += escaped_title;
                    pos = open + kTitle.size();
                }
                else if (tpl.compare(open, kScene.size(), kScene) == 0)
                {
                    out += scene_json;
                    pos = open + kScene.size();
                }
                else
                {
                    out += '{';
                    pos = open + 1;
                }
            }
            return out;
        }

        void append_numbers(std::string &json, const double *values, size_t count)
        {
            json += '[';
            for (size_t i = 0; i < count; ++i)
            {
                if (i > 0)
                    json += ',';
                json += export_text::python_float_repr(values[i]);
            }
            json += ']';
        }
    } // namespace

    ThreeJSWriter::ThreeJSWriter() : title_("OST Visualizer Export") {}
    ThreeJSWriter::~ThreeJSWriter() = default;

    void ThreeJSWriter::set_title(const std::string &title) { title_ = title; }

    void ThreeJSWriter::add_mesh(const CppMeshData &mesh,
                                 const std::string &name,
                                 float r, float g, float b, float opacity)
    {
        if (!mesh.is_valid())
            return;
        meshes_.push_back({mesh, name, r, g, b, opacity});
    }

    void ThreeJSWriter::clear() { meshes_.clear(); }

    bool ThreeJSWriter::save(const std::string &path)
    {
        if (meshes_.empty())
            return false;

        std::string scene_json = build_scene_json();
        std::string html = fill_template(html_template(), title_, scene_json);

        std::ofstream file(path);
        if (!file.is_open())
            return false;
        file << html;
        return file.good();
    }

    // The scene the viewer reads, written the way Windows writes it: json.dumps(scene, separators=(",", ":")) with every
    // number a full-precision Python float and '<' escaped (threejs_renderer._generate_html). The meshes are already in
    // the viewer frame (Y up): iOS (X, Y, Z) is the Windows source point (x, y, z) after its (x, z, -y) swap, so vertices
    // and normals are written as they are. The camera is framed like threejs_mesh_adapter.build_scene_data from the
    // Windows-frame bounds (x, -Z, Y) of the vertices (the same operations in the same order, so zeros keep their signs).
    std::string ThreeJSWriter::build_scene_json() const
    {
        // Windows bounds (xmin, xmax, ymin, ymax, zmin, zmax) over the source (Z-up) vertices (x, -Z_ios, Y_ios)
        double bounds[6] = {0, 0, 0, 0, 0, 0};
        bool first = true;
        for (const auto &entry : meshes_)
        {
            for (const auto &v : entry.mesh.vertices)
            {
                const double x = v[0], y = -v[2], z = v[1];
                if (first)
                {
                    bounds[0] = bounds[1] = x;
                    bounds[2] = bounds[3] = y;
                    bounds[4] = bounds[5] = z;
                    first = false;
                    continue;
                }
                bounds[0] = std::min(bounds[0], x);
                bounds[1] = std::max(bounds[1], x);
                bounds[2] = std::min(bounds[2], y);
                bounds[3] = std::max(bounds[3], y);
                bounds[4] = std::min(bounds[4], z);
                bounds[5] = std::max(bounds[5], z);
            }
        }

        const double center_x = (bounds[0] + bounds[1]) / 2;
        const double center_y = (bounds[2] + bounds[3]) / 2;
        const double center_z = (bounds[4] + bounds[5]) / 2;
        const double size = std::max({bounds[1] - bounds[0], bounds[3] - bounds[2], bounds[5] - bounds[4], 1.0});
        const double camera_position[3] = {center_x, center_z + size * 1.5, -center_y - size};
        const double camera_target[3] = {center_x, center_z, -center_y};

        std::string json = "{\"geometries\":[";
        for (size_t mi = 0; mi < meshes_.size(); mi++)
        {
            const auto &entry = meshes_[mi];
            const auto &mesh = entry.mesh;
            if (mi > 0)
                json += ',';

            json += "{\"vertices\":[";
            for (size_t i = 0; i < mesh.vertices.size(); i++)
            {
                if (i > 0)
                    json += ',';
                const auto &v = mesh.vertices[i];
                for (int k = 0; k < 3; k++)
                {
                    if (k > 0)
                        json += ',';
                    json += export_text::python_float_repr(v[static_cast<size_t>(k)]);
                }
            }
            json += "],\"normals\":[";

            // area-weighted vertex normals of the face winding (EXP-20: not split per crease)
            std::vector<std::array<double, 3>> normals(mesh.vertices.size(), {0, 0, 0});
            for (const auto &f : mesh.faces)
            {
                const auto &v0 = mesh.vertices[f[0]];
                const auto &v1 = mesh.vertices[f[1]];
                const auto &v2 = mesh.vertices[f[2]];

                double ax = v1[0] - v0[0], ay = v1[1] - v0[1], az = v1[2] - v0[2];
                double bx = v2[0] - v0[0], by = v2[1] - v0[1], bz = v2[2] - v0[2];
                double nx = ay * bz - az * by;
                double ny = az * bx - ax * bz;
                double nz = ax * by - ay * bx;
                for (int k = 0; k < 3; k++)
                {
                    normals[f[k]][0] += nx;
                    normals[f[k]][1] += ny;
                    normals[f[k]][2] += nz;
                }
            }
            for (size_t i = 0; i < normals.size(); i++)
            {
                if (i > 0)
                    json += ',';
                auto &n = normals[i];
                double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (len > 1e-10)
                {
                    n[0] /= len;
                    n[1] /= len;
                    n[2] /= len;
                }
                for (size_t k = 0; k < 3; k++)
                {
                    if (k > 0)
                        json += ',';
                    json += export_text::python_float_repr(n[k]);
                }
            }
            json += "],\"indices\":[";
            for (size_t i = 0; i < mesh.faces.size(); i++)
            {
                if (i > 0)
                    json += ',';
                json += std::to_string(mesh.faces[i][0]) + ',' + std::to_string(mesh.faces[i][1]) + ',' +
                        std::to_string(mesh.faces[i][2]);
            }
            // colour as the Windows float of the colour byte, opacity as the decimal it was typed as
            const double colour[3] = {export_text::colour_channel(entry.r), export_text::colour_channel(entry.g),
                                      export_text::colour_channel(entry.b)};
            json += "],\"color\":";
            append_numbers(json, colour, 3);
            json += ",\"opacity\":" + export_text::python_float_repr(export_text::shortest_double_of(entry.opacity));
            json += ",\"name\":" + export_text::json_string(entry.name) + '}';
        }
        json += "],\"camera\":{\"position\":";
        append_numbers(json, camera_position, 3);
        json += ",\"target\":";
        append_numbers(json, camera_target, 3);
        json += "}}";
        return json;
    }

    std::string ThreeJSWriter::html_template()
    {
        return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{{TITLE}}</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { overflow: hidden; background: #1a1a1a; }
        #canvas { width: 100vw; height: 100vh; display: block; }
    </style>
</head>
<body>
    <canvas id="canvas"></canvas>
    <script type="application/json" id="scene-data">
{{SCENE_DATA}}
    </script>
    <script type="importmap">
    {
        "imports": {
            "three": "https://unpkg.com/three@0.150.0/build/three.module.js",
            "three/addons/": "https://unpkg.com/three@0.150.0/examples/jsm/"
        }
    }
    </script>
    <script type="module">
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

// Shading and colour handling of the Windows viewer (viewer.html), which copies the native renderer: Lambert with an
// ambient term and three white directional lights, no specular, no tone mapping, colours used as given. The directions
// are in the Y-up frame of the scene, like the native vectors mapped as (x, z, -y).
const NATIVE_AMBIENT_INTENSITY = 0.55;
const NATIVE_KEY_LIGHT = { dir: [500.0, 500.0, 500.0], intensity: 0.45 };
const NATIVE_FILL_LIGHT = { dir: [-500.0, 500.0, -500.0], intensity: 0.35 };
const NATIVE_RIM_LIGHT = { dir: [0.0, -500.0, -500.0], intensity: 0.2 };

const sceneData = JSON.parse(document.getElementById('scene-data').textContent);
const canvas = document.getElementById('canvas');
// Colours are used as given (no conversion to linear), like the native renderer.
if (THREE.ColorManagement) THREE.ColorManagement.enabled = false;
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false });
renderer.setPixelRatio(window.devicePixelRatio);
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setClearColor(0x1a1a1a);
renderer.outputEncoding = THREE.LinearEncoding;
renderer.toneMapping = THREE.NoToneMapping;

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 100000);
camera.position.set(...sceneData.camera.position);

const controls = new OrbitControls(camera, canvas);
controls.target.set(...sceneData.camera.target);
controls.enableDamping = true;
controls.dampingFactor = 0.05;
controls.update();

function lightUniform(light) {
    return { dir: new THREE.Vector3(light.dir[0], light.dir[1], light.dir[2]).normalize(), intensity: light.intensity };
}

const NATIVE_VERTEX_SHADER = `
varying vec3 vWorldPosition;
void main() {
    vec4 worldPosition = modelMatrix * vec4(position, 1.0);
    vWorldPosition = worldPosition.xyz;
    gl_Position = projectionMatrix * viewMatrix * worldPosition;
}`;

// The lighting of the Windows shader. The face normal comes from the screen-space derivatives (flat shading, always facing
// the viewer) because the exported meshes share their corner vertices, so smooth vertex normals would blur box edges.
const NATIVE_FRAGMENT_SHADER = `
uniform vec3 uColor;
uniform float uAlpha;
uniform float uAmbient;
uniform vec3 uKeyDir;
uniform float uKeyIntensity;
uniform vec3 uFillDir;
uniform float uFillIntensity;
uniform vec3 uRimDir;
uniform float uRimIntensity;
varying vec3 vWorldPosition;
vec3 nativeLitColor() {
    vec3 norm = normalize(cross(dFdx(vWorldPosition), dFdy(vWorldPosition)));
    float keyDiff = max(dot(norm, uKeyDir), 0.0);
    float fillDiff = max(dot(norm, uFillDir), 0.0);
    float rimDiff = max(dot(norm, uRimDir), 0.0);
    vec3 lighting = vec3(uAmbient) + uKeyIntensity * keyDiff * vec3(1.0)
        + uFillIntensity * fillDiff * vec3(1.0) + uRimIntensity * rimDiff * vec3(1.0);
    return lighting * uColor;
}
void main() {
    gl_FragColor = vec4(nativeLitColor(), uAlpha);
}`;

function createNativeMaterial(color, alpha) {
    const key = lightUniform(NATIVE_KEY_LIGHT);
    const fill = lightUniform(NATIVE_FILL_LIGHT);
    const rim = lightUniform(NATIVE_RIM_LIGHT);
    return new THREE.ShaderMaterial({
        uniforms: {
            uColor: { value: new THREE.Vector3(color[0], color[1], color[2]) },
            uAlpha: { value: alpha },
            uAmbient: { value: NATIVE_AMBIENT_INTENSITY },
            uKeyDir: { value: key.dir },
            uKeyIntensity: { value: key.intensity },
            uFillDir: { value: fill.dir },
            uFillIntensity: { value: fill.intensity },
            uRimDir: { value: rim.dir },
            uRimIntensity: { value: rim.intensity }
        },
        vertexShader: NATIVE_VERTEX_SHADER,
        fragmentShader: NATIVE_FRAGMENT_SHADER,
        extensions: { derivatives: true },
        transparent: alpha < 1.0,
        side: THREE.DoubleSide
    });
}

sceneData.geometries.forEach(g => {
    const geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(g.vertices), 3));
    if (g.normals && g.normals.length > 0)
        geom.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(g.normals), 3));
    geom.setIndex(new THREE.BufferAttribute(new Uint32Array(g.indices), 1));
    scene.add(new THREE.Mesh(geom, createNativeMaterial(g.color, g.opacity)));
});

function animate() { requestAnimationFrame(animate); controls.update(); renderer.render(scene, camera); }
window.addEventListener('resize', () => {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
});
animate();
    </script>
</body>
</html>
)HTML";
    }

}
