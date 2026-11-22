// Shared helpers of the export-rules tests (boolean rule, mesh set, display colour): the scenario of the Windows tests
// tests/presentation/visualization/exporters/boolean_rule_support.py built as an iOS project, a box difference that
// stands in for Manifold where the scenario only needs axis-aligned cuts, and small mesh readers.
// Not a test_*.cpp file, so CMake does not compile it alone.
#pragma once

#include "ExportTestSupport.hpp"

#include "Data/ProjectData.hpp"
#include "Domain/Condition.hpp"
#include "Domain/Takeoff.hpp"
#include "Meshing/BooleanPass.hpp"
#include "Meshing/ExportMeshSet.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace rule_test
{
    using export_test::Mesh;
    using export_test::V3;
    using ost_domain::Condition;
    using ost_domain::ConditionType;
    using ost_domain::Takeoff;

    // boolean_rule_support._box_condition: a Square box, width = depth = size, display_size 100, name = uid.
    inline Condition box_condition(const std::string &uid, ConditionType type, double size = 20.0,
                                   double height = 10.0, double z_value = 0.0)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.condition_type = type;
        c.shape = ost_domain::ShapeId::Square;
        c.width = size;
        c.depth = size;
        c.height = height;
        c.display_size = 100.0;
        c.z_value = z_value;
        return c;
    }

    inline Takeoff makeoff(const std::string &uid, const std::string &condition_uid, const std::string &page_uid,
                           std::vector<double> position, bool is_negative = false, const std::string &parent_uid = "0")
    {
        Takeoff t;
        t.uid = uid;
        t.condition_uid = condition_uid;
        t.page_uid = page_uid;
        t.position = std::move(position);
        t.is_negative = is_negative;
        t.parent_uid = parent_uid;
        return t;
    }

    // boolean_rule_support._takeoff: position [x, 0.0].
    inline Takeoff box_takeoff(const std::string &uid, const std::string &condition_uid, double x,
                               bool is_negative = false)
    {
        return makeoff(uid, condition_uid, "p", {x, 0.0}, is_negative);
    }

    inline ost_domain::Page make_page(const std::string &uid)
    {
        ost_domain::Page p;
        p.uid = uid;
        p.name = uid;
        p.width = 612.0 / 72.0;
        p.height = 792.0 / 72.0;
        p.scale_factor1 = 1.0;
        p.scale_factor2 = 1.0;
        return p;
    }

    // boolean_rule_conditions(): 'count' (Count), 'attachment' (Attachment, same box), 'count_block' (Count, 60 x 60 x 30
    // from z -10: swallows a 'count' box at the same x).
    inline std::vector<Condition> rule_conditions()
    {
        return {box_condition("count", ConditionType::Count), box_condition("attachment", ConditionType::Attachment),
                box_condition("count_block", ConditionType::Count, 60.0, 30.0, -10.0)};
    }

    inline ost_data::ProjectData project_of(const std::vector<Condition> &conditions,
                                            const std::vector<Takeoff> &takeoffs,
                                            const std::vector<ost_domain::Page> &pages = {make_page("p")})
    {
        ost_data::ProjectData project;
        for (const auto &p : pages)
            project.pages[p.uid] = p;
        for (const auto &c : conditions)
            project.conditions[c.uid] = c;
        project.takeoffs = takeoffs;
        return project;
    }

    inline ost_data::ProjectData rule_project(const std::vector<Takeoff> &takeoffs)
    {
        return project_of(rule_conditions(), takeoffs);
    }

    // The four scenarios of boolean_rule_support.py (positions in OST units; the iOS mesh frame has X = -x).
    inline std::vector<Takeoff> other_type_negative_takeoffs()
    {
        return {box_takeoff("positive", "count", 0.0), box_takeoff("negative", "attachment", 10.0, true)};
    }
    inline std::vector<Takeoff> same_type_negative_takeoffs()
    {
        return {box_takeoff("positive", "count", 0.0), box_takeoff("negative", "count", 10.0, true)};
    }
    inline std::vector<Takeoff> consumed_positive_takeoffs()
    {
        return {box_takeoff("consumed", "count", 0.0), box_takeoff("survivor", "count", 200.0),
                box_takeoff("negative", "count_block", 0.0, true)};
    }
    inline std::vector<Takeoff> negative_only_takeoffs()
    {
        return {box_takeoff("negative", "count", 0.0, true)};
    }

    // ---- mesh readers ----
    struct Span
    {
        double lo = std::numeric_limits<double>::infinity();
        double hi = -std::numeric_limits<double>::infinity();
    };
    inline Span span_of(const Mesh &m, int axis)
    {
        Span s;
        for (const auto &v : m.vertices)
        {
            s.lo = std::min(s.lo, v[static_cast<size_t>(axis)]);
            s.hi = std::max(s.hi, v[static_cast<size_t>(axis)]);
        }
        return s;
    }

    // Axis-aligned box with outward faces (positive signed volume): vertices 0..3 at z0, 4..7 at z1.
    inline Mesh make_box(double x0, double y0, double z0, double x1, double y1, double z1)
    {
        Mesh m;
        m.vertices = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                      {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
        m.faces = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                   {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
        return m;
    }

    // A stand-in for Manifold where the scenario only has cuts that span the whole Y and Z extent of the positive box (the
    // host build without Manifold throws): a negative that covers the positive removes it (nullopt, like
    // boolean_difference_batch for an empty result), one that covers an end of it shortens it, a negative that does not
    // touch it changes nothing. Anything else is not a slab cut and throws.
    inline std::optional<Mesh> box_difference(const Mesh &positive, const std::vector<Mesh> &negatives)
    {
        auto box = positive.compute_aabb();
        bool touched = false;
        for (const auto &n : negatives)
        {
            const auto q = n.compute_aabb();
            if (!box.intersects(q))
                continue;
            const bool spans_yz = q.min_bounds[1] <= box.min_bounds[1] && q.max_bounds[1] >= box.max_bounds[1] &&
                                  q.min_bounds[2] <= box.min_bounds[2] && q.max_bounds[2] >= box.max_bounds[2];
            if (!spans_yz)
                throw std::logic_error("box_difference: not a slab cut");
            touched = true;
            if (q.min_bounds[0] <= box.min_bounds[0] && q.max_bounds[0] >= box.max_bounds[0])
                return std::nullopt;
            if (q.min_bounds[0] <= box.min_bounds[0])
                box.min_bounds[0] = q.max_bounds[0];
            else if (q.max_bounds[0] >= box.max_bounds[0])
                box.max_bounds[0] = q.min_bounds[0];
            else
                throw std::logic_error("box_difference: a cut in the middle");
        }
        if (!touched)
            return positive;
        return make_box(box.min_bounds[0], box.min_bounds[1], box.min_bounds[2], box.max_bounds[0],
                        box.max_bounds[1], box.max_bounds[2]);
    }

    inline ost_meshing::BooleanDifferenceFn box_difference_fn()
    {
        return [](const Mesh &p, const std::vector<Mesh> &n)
        { return box_difference(p, n); };
    }

    inline std::vector<std::string> uids_of(const std::vector<ost_meshing::BooleanMesh> &meshes)
    {
        std::vector<std::string> out;
        for (const auto &m : meshes)
            out.push_back(m.takeoff->uid);
        return out;
    }

    inline std::vector<std::string> uids_of(const std::vector<ost_meshing::TakeoffMesh> &meshes)
    {
        std::vector<std::string> out;
        for (const auto &m : meshes)
            out.push_back(m.takeoff->uid);
        return out;
    }

    // All surviving meshes of a grouped export, group by group.
    inline std::vector<const ost_meshing::BooleanMesh *> all_meshes(const ost_meshing::GroupedExport &e)
    {
        std::vector<const ost_meshing::BooleanMesh *> out;
        for (const auto &g : e.groups)
            for (const auto &m : g.meshes)
                out.push_back(&m);
        return out;
    }
} // namespace rule_test
