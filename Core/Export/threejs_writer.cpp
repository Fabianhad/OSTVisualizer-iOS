#include "threejs_writer.hpp"
#include <fstream>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace ost_geometry
{

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
        std::string html = html_template();

        auto replace = [](std::string &s, const std::string &from, const std::string &to)
        {
            size_t pos = s.find(from);
            if (pos != std::string::npos)
                s.replace(pos, from.length(), to);
        };
        replace(html, "{{TITLE}}", title_);
        replace(html, "{{SCENE_DATA}}", scene_json);

        std::ofstream file(path);
        if (!file.is_open())
            return false;
        file << html;
        return file.good();
    }

    std::string ThreeJSWriter::build_scene_json() const
    {

        double bmin[3] = {1e30, 1e30, 1e30};
        double bmax[3] = {-1e30, -1e30, -1e30};

        for (const auto &entry : meshes_)
        {
            for (const auto &v : entry.mesh.vertices)
            {

                double tx = v[0], ty = v[2], tz = -v[1];
                bmin[0] = std::min(bmin[0], tx);
                bmin[1] = std::min(bmin[1], ty);
                bmin[2] = std::min(bmin[2], tz);
                bmax[0] = std::max(bmax[0], tx);
                bmax[1] = std::max(bmax[1], ty);
                bmax[2] = std::max(bmax[2], tz);
            }
        }

        double cx = (bmin[0] + bmax[0]) * 0.5;
        double cy = (bmin[1] + bmax[1]) * 0.5;
        double cz = (bmin[2] + bmax[2]) * 0.5;
        double size = std::max({bmax[0] - bmin[0], bmax[1] - bmin[1], bmax[2] - bmin[2], 1.0});

        std::string json = "{\n";

        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "  \"camera\": {\n"
                      "    \"position\": [%.4f, %.4f, %.4f],\n"
                      "    \"target\": [%.4f, %.4f, %.4f]\n"
                      "  },\n",
                      cx, cy + size * 1.5, cz - size,
                      cx, cy, cz);
        json += buf;

        json += "  \"geometries\": [\n";
        for (size_t mi = 0; mi < meshes_.size(); mi++)
        {
            const auto &entry = meshes_[mi];
            const auto &mesh = entry.mesh;

            json += "    {\n";

            json += "      \"name\": \"" + entry.name + "\",\n";

            json += "      \"vertices\": [";
            for (size_t i = 0; i < mesh.vertices.size(); i++)
            {
                if (i > 0)
                    json += ",";
                const auto &v = mesh.vertices[i];
                std::snprintf(buf, sizeof(buf), "%.4f,%.4f,%.4f", v[0], v[2], -v[1]);
                json += buf;
            }
            json += "],\n";

            json += "      \"normals\": [";

            std::vector<std::array<double, 3>> normals(mesh.vertices.size(), {0, 0, 0});
            for (const auto &f : mesh.faces)
            {
                const auto &v0 = mesh.vertices[f[0]];
                const auto &v1 = mesh.vertices[f[1]];
                const auto &v2 = mesh.vertices[f[2]];

                double ax = v1[0] - v0[0], ay = v1[2] - v0[2], az = -(v1[1] - v0[1]);
                double bx = v2[0] - v0[0], by = v2[2] - v0[2], bz = -(v2[1] - v0[1]);
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
                    json += ",";
                auto &n = normals[i];
                double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (len > 1e-10)
                {
                    n[0] /= len;
                    n[1] /= len;
                    n[2] /= len;
                }
                std::snprintf(buf, sizeof(buf), "%.4f,%.4f,%.4f", n[0], n[1], n[2]);
                json += buf;
            }
            json += "],\n";

            json += "      \"indices\": [";
            for (size_t i = 0; i < mesh.faces.size(); i++)
            {
                if (i > 0)
                    json += ",";
                std::snprintf(buf, sizeof(buf), "%u,%u,%u",
                              mesh.faces[i][0], mesh.faces[i][1], mesh.faces[i][2]);
                json += buf;
            }
            json += "],\n";

            std::snprintf(buf, sizeof(buf),
                          "      \"color\": [%.4f, %.4f, %.4f],\n"
                          "      \"opacity\": %.4f\n",
                          entry.r, entry.g, entry.b, entry.opacity);
            json += buf;

            json += mi + 1 < meshes_.size() ? "    },\n" : "    }\n";
        }
        json += "  ]\n}";
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
            "three": "https://unpkg.com/three@0.152.0/build/three.module.js",
            "three/addons/": "https://unpkg.com/three@0.152.0/examples/jsm/"
        }
    }
    </script>
    <script type="module">
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const sceneData = JSON.parse(document.getElementById('scene-data').textContent);
const canvas = document.getElementById('canvas');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: false });
renderer.setPixelRatio(window.devicePixelRatio);
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setClearColor(0x1a1a1a);
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 0.25;

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 100000);
camera.position.set(...sceneData.camera.position);

const controls = new OrbitControls(camera, canvas);
controls.target.set(...sceneData.camera.target);
controls.enableDamping = true;
controls.dampingFactor = 0.05;
controls.update();

scene.add(new THREE.AmbientLight(0xffffff, 0.35));
const kl = new THREE.DirectionalLight(0xffffff, 1.2); kl.position.set(500, 500, -500); scene.add(kl);
const fl = new THREE.DirectionalLight(0xffffff, 0.6); fl.position.set(-500, 500, 500); scene.add(fl);
const rl = new THREE.DirectionalLight(0xffffff, 0.4); rl.position.set(0, -500, -500); scene.add(rl);

sceneData.geometries.forEach(g => {
    const geom = new THREE.BufferGeometry();
    geom.setAttribute('position', new THREE.BufferAttribute(new Float32Array(g.vertices), 3));
    if (g.normals && g.normals.length > 0)
        geom.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(g.normals), 3));
    geom.setIndex(new THREE.BufferAttribute(new Uint32Array(g.indices), 1));
    const mat = new THREE.MeshPhongMaterial({
        color: new THREE.Color(g.color[0], g.color[1], g.color[2]),
        opacity: g.opacity, transparent: g.opacity < 1.0,
        side: THREE.DoubleSide, flatShading: true
    });
    scene.add(new THREE.Mesh(geom, mat));
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
