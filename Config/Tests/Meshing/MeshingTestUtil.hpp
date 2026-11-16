// Shared helpers of the Meshing area tests (not a test_*.cpp file, so CMake does not compile it alone).
#pragma once

#include "TestHarness.hpp"

#include "Data/ProjectData.hpp"
#include "Domain/Condition.hpp"
#include "Domain/Takeoff.hpp"
#include "Geometry/mesh_data.hpp"
#include "Meshing/MeshFactory.hpp"
#include "Meshing/MeshGenerator.hpp"
#include "Meshing/TakeoffMeshBuilder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mesh_test
{
    using Mesh = ost_geometry::CppMeshData;
    using V3 = std::array<double, 3>;
    using ost_domain::Condition;
    using ost_domain::ConditionType;
    using ost_domain::ShapeId;
    using ost_domain::Takeoff;

    // Windows frame (X = -x_ost, Y = y_ost, Z up) -> iOS frame (X, Y up, Z): (X, Y, Z) -> (X, Z, -Y).
    inline V3 to_ios(const V3 &w) { return {w[0], w[2], -w[1]}; }

    // Relative tolerance 1e-9 of the larger magnitude (at least 1.0): where Windows is not bit-exact across libms.
    inline double rel_tol(double a, double b)
    {
        return 1e-9 * std::max({1.0, std::fabs(a), std::fabs(b)});
    }

    inline double signed_volume(const Mesh &m)
    {
        double total = 0.0;
        for (const auto &f : m.faces)
        {
            const V3 &a = m.vertices[f[0]];
            const V3 &b = m.vertices[f[1]];
            const V3 &c = m.vertices[f[2]];
            total += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
                      a[2] * (b[0] * c[1] - b[1] * c[0])) /
                     6.0;
        }
        return total;
    }

    struct Bounds
    {
        double lo[3], hi[3];
    };

    inline Bounds bounds_of(const Mesh &m)
    {
        Bounds b{{1e300, 1e300, 1e300}, {-1e300, -1e300, -1e300}};
        for (const auto &v : m.vertices)
            for (int i = 0; i < 3; ++i)
            {
                b.lo[i] = std::min(b.lo[i], v[i]);
                b.hi[i] = std::max(b.hi[i], v[i]);
            }
        return b;
    }

    // Windows bounds {xmin, xmax, ymin, ymax, zmin, zmax} (Windows frame) against an iOS mesh: iOS X is Windows X,
    // iOS Z is -Windows Y, iOS Y is Windows Z.
    inline void check_bounds_ios(const Mesh &m, const double (&w)[6], double tol_abs = 0.0)
    {
        Bounds b = bounds_of(m);
        auto tol = [&](double a, double e) { return tol_abs > 0.0 ? tol_abs : rel_tol(a, e); };
        CHECK_NEAR(b.lo[0], w[0], tol(b.lo[0], w[0]));
        CHECK_NEAR(b.hi[0], w[1], tol(b.hi[0], w[1]));
        CHECK_NEAR(b.lo[2], -w[3], tol(b.lo[2], -w[3]));
        CHECK_NEAR(b.hi[2], -w[2], tol(b.hi[2], -w[2]));
        CHECK_NEAR(b.lo[1], w[4], tol(b.lo[1], w[4]));
        CHECK_NEAR(b.hi[1], w[5], tol(b.hi[1], w[5]));
    }

    // Index-wise: iOS vertex i == to_ios(Windows vertex i).
    inline void check_vertices_index_wise(const Mesh &m, const std::vector<V3> &windows, double tol_abs = 0.0)
    {
        REQUIRE_EQ(m.vertices.size(), windows.size());
        for (size_t i = 0; i < windows.size(); ++i)
        {
            SCOPED_CONTEXT("vertex " + std::to_string(i));
            V3 e = to_ios(windows[i]);
            for (int k = 0; k < 3; ++k)
                CHECK_NEAR(m.vertices[i][k], e[k], tol_abs > 0.0 ? tol_abs : rel_tol(m.vertices[i][k], e[k]));
        }
    }

    // The two vertex sets are equal (greedy one-to-one matching within tolerance), whatever the order.
    inline bool vertex_sets_equal(const std::vector<V3> &a, const std::vector<V3> &b, double tol_abs = 1e-7)
    {
        if (a.size() != b.size())
            return false;
        std::vector<bool> used(b.size(), false);
        for (const auto &p : a)
        {
            bool found = false;
            for (size_t j = 0; j < b.size(); ++j)
            {
                if (used[j])
                    continue;
                if (std::fabs(p[0] - b[j][0]) <= tol_abs && std::fabs(p[1] - b[j][1]) <= tol_abs &&
                    std::fabs(p[2] - b[j][2]) <= tol_abs)
                {
                    used[j] = true;
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }
        return true;
    }

    inline std::vector<V3> to_ios_all(const std::vector<V3> &w)
    {
        std::vector<V3> out;
        out.reserve(w.size());
        for (const auto &v : w)
            out.push_back(to_ios(v));
        return out;
    }

    // Faces as a multiset of sorted index triples: equal exactly when the two meshes use the same triangles,
    // whatever their winding and their order.
    inline std::multiset<std::array<uint32_t, 3>> triangle_set(const std::vector<std::array<uint32_t, 3>> &faces)
    {
        std::multiset<std::array<uint32_t, 3>> s;
        for (auto f : faces)
        {
            std::sort(f.begin(), f.end());
            s.insert(f);
        }
        return s;
    }

    // Every directed edge occurs exactly once and its reverse exactly once: a closed, consistently oriented surface.
    inline bool closed_and_consistent(const Mesh &m)
    {
        std::map<std::pair<uint32_t, uint32_t>, int> directed;
        for (const auto &f : m.faces)
        {
            if (f[0] == f[1] || f[1] == f[2] || f[0] == f[2])
                return false;
            for (int i = 0; i < 3; ++i)
                directed[{f[i], f[(i + 1) % 3]}]++;
        }
        for (const auto &kv : directed)
        {
            if (kv.second != 1)
                return false;
            auto rev = directed.find({kv.first.second, kv.first.first});
            if (rev == directed.end() || rev->second != 1)
                return false;
        }
        return true;
    }

    inline double shoelace_area(const std::vector<std::pair<double, double>> &ring)
    {
        double s = 0.0;
        for (size_t i = 0; i < ring.size(); ++i)
        {
            const auto &a = ring[i];
            const auto &b = ring[(i + 1) % ring.size()];
            s += a.first * b.second - b.first * a.second;
        }
        return std::fabs(s) * 0.5;
    }

    // ---- model builders ----
    inline Condition make_condition(const std::string &uid, ConditionType type)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.condition_type = type;
        return c;
    }

    inline Takeoff make_takeoff(const std::string &uid, const std::string &condition_uid, const std::string &page_uid,
                                std::vector<double> position, const std::string &parent_uid = "0",
                                double rotation = 0.0)
    {
        Takeoff t;
        t.uid = uid;
        t.condition_uid = condition_uid;
        t.page_uid = page_uid;
        t.position = std::move(position);
        t.parent_uid = parent_uid;
        t.rotation = rotation;
        return t;
    }

    // A page whose Windows width_pts / height_pts are width_pts / height_pts (iOS Page.width / height are inches).
    inline ost_domain::Page make_page(const std::string &uid, double width_pts, double height_pts,
                                      double sf1 = 1.0, double sf2 = 1.0, int rotation = 0,
                                      bool flip_x = false, bool flip_y = false)
    {
        ost_domain::Page p;
        p.uid = uid;
        p.name = uid;
        p.width = width_pts / 72.0;
        p.height = height_pts / 72.0;
        p.scale_factor1 = sf1;
        p.scale_factor2 = sf2;
        p.rotation = rotation;
        p.flip_x = flip_x;
        p.flip_y = flip_y;
        return p;
    }

    inline void add(ost_data::ProjectData &p, const ost_domain::Page &page) { p.pages[page.uid] = page; }
    inline void add(ost_data::ProjectData &p, const Condition &c) { p.conditions[c.uid] = c; }
    inline void add(ost_data::ProjectData &p, const Takeoff &t) { p.takeoffs.push_back(t); }

    inline std::vector<ost_meshing::TakeoffMesh> build(const ost_data::ProjectData &p,
                                                       const std::set<std::string> &pages = {},
                                                       bool page_transform = true)
    {
        return ost_meshing::generate_takeoff_meshes(p, pages, page_transform);
    }

    inline const ost_meshing::TakeoffMesh *find_mesh(const std::vector<ost_meshing::TakeoffMesh> &v,
                                                     const std::string &takeoff_uid)
    {
        for (const auto &m : v)
            if (m.takeoff->uid == takeoff_uid)
                return &m;
        return nullptr;
    }

    inline std::set<std::string> uids(const std::vector<ost_meshing::TakeoffMesh> &v)
    {
        std::set<std::string> s;
        for (const auto &m : v)
            s.insert(m.takeoff->uid);
        return s;
    }

    inline std::vector<std::string> uid_list(const std::vector<const Takeoff *> &v)
    {
        std::vector<std::string> out;
        for (const auto *t : v)
            out.push_back(t->uid);
        return out;
    }

    template <class F>
    bool throws_invalid_argument(F &&fn, std::string *what = nullptr)
    {
        try
        {
            fn();
        }
        catch (const std::invalid_argument &e)
        {
            if (what)
                *what = e.what();
            return true;
        }
        catch (...)
        {
            return false;
        }
        return false;
    }

} // namespace mesh_test
