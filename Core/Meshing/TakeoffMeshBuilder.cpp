#include "TakeoffMeshBuilder.hpp"
#include "MeshFactory.hpp"
#include "../Geometry/coord_transform.hpp"
#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ost_meshing
{

    AreaHoleGrouping group_area_takeoffs_with_holes(
        const std::vector<const ost_domain::Takeoff *> &takeoffs,
        const std::unordered_map<std::string, ost_domain::Condition> &conditions)
    {
        AreaHoleGrouping grouping;
        std::vector<const ost_domain::Takeoff *> area_takeoffs;
        std::vector<const ost_domain::Takeoff *> non_area_takeoffs;
        for (const auto *t : takeoffs)
        {
            auto it = conditions.find(t->condition_uid);
            if (it != conditions.end() && it->second.is_area())
                area_takeoffs.push_back(t);
            else
                non_area_takeoffs.push_back(t);
        }
        if (area_takeoffs.empty())
        {
            grouping.takeoffs = takeoffs;
            return grouping;
        }
        std::unordered_set<std::string> area_uids;
        for (const auto *t : area_takeoffs)
            area_uids.insert(t->uid);
        std::unordered_set<std::string> child_uids;
        for (const auto *t : area_takeoffs)
        {
            if (t->is_hole() && area_uids.count(t->parent_uid))
            {
                grouping.holes[t->parent_uid].push_back(t);
                child_uids.insert(t->uid);
            }
        }
        grouping.takeoffs = std::move(non_area_takeoffs);
        for (const auto *t : area_takeoffs)
            if (!child_uids.count(t->uid))
                grouping.takeoffs.push_back(t);
        return grouping;
    }

    std::vector<std::vector<std::pair<double, double>>> holes_for_takeoff(
        const std::string &takeoff_uid,
        const AreaHoleGrouping &grouping)
    {
        std::vector<std::vector<std::pair<double, double>>> holes;
        auto it = grouping.holes.find(takeoff_uid);
        if (it == grouping.holes.end())
            return holes;
        for (const auto *child : it->second)
        {
            const auto &p = child->position;
            if (p.size() < 6)
                continue;
            std::vector<std::pair<double, double>> ring;
            for (size_t i = 0; i < p.size(); i += 2)
                if (i + 1 < p.size())
                    ring.emplace_back(p[i], p[i + 1]);
            if (ring.size() >= 3)
                holes.push_back(std::move(ring));
        }
        return holes;
    }

    namespace
    {
        constexpr double PDF_POINTS_PER_INCH = 72.0;

        // Windows _MeshGeneratorAdapter.generate_meshes: one coordinate system per page with sf1 = sf2 = 1 and the page
        // box scaled by the page ratio (width_pts * sf2 / sf1), so a page flip / rotation mirrors inside the page box
        // without ever rescaling the model.
        struct PageFrame
        {
            double width = 0.0, height = 0.0;
            int rotation = 0; // OSTCoordinateSystem: int(rotation) % 360
            bool flip_x = false, flip_y = false;
            bool active = false; // _apply_page_transform: rotation or flip_x or flip_y
            bool output_height_ready = false;
            double output_height = 0.0;
        };

        struct PageFrames
        {
            const ost_data::ProjectData &project;
            std::unordered_map<std::string, std::optional<PageFrame>> cache;

            // nullptr: no transform (unknown page, or a page size that is not finite and positive: Windows skips it).
            PageFrame *frame_for(const std::string &page_uid)
            {
                auto it = cache.find(page_uid);
                if (it == cache.end())
                    it = cache.emplace(page_uid, make(page_uid)).first;
                return it->second ? &*it->second : nullptr;
            }

            std::optional<PageFrame> make(const std::string &page_uid) const
            {
                auto pit = project.pages.find(page_uid);
                if (pit == project.pages.end())
                    return std::nullopt;
                const ost_domain::Page &page = pit->second;
                // _snapshot_page_render_info: float(scale_factor or 1.0); width_pts = Width (inches) * 72
                double sf1 = page.scale_factor1 != 0.0 ? page.scale_factor1 : 1.0;
                double sf2 = page.scale_factor2 != 0.0 ? page.scale_factor2 : 1.0;
                double width_pts = page.width * PDF_POINTS_PER_INCH;
                double height_pts = page.height * PDF_POINTS_PER_INCH;
                double scale_ratio = sf2 / sf1;
                double mesh_width = (width_pts != 0.0 ? width_pts : 0.0) * scale_ratio;
                double mesh_height = (height_pts != 0.0 ? height_pts : 0.0) * scale_ratio;
                if (!std::isfinite(mesh_width) || !std::isfinite(mesh_height) ||
                    mesh_width <= 0.0 || mesh_height <= 0.0)
                    return std::nullopt;
                PageFrame f;
                f.width = mesh_width;
                f.height = mesh_height;
                f.rotation = ((page.rotation % 360) + 360) % 360;
                f.flip_x = page.flip_x;
                f.flip_y = page.flip_y;
                f.active = f.rotation != 0 || f.flip_x || f.flip_y;
                return f;
            }
        };

        // OSTCoordinateSystem._transform_page_vertices_to_3d: the page box corners fix the output height.
        double page_output_height(const PageFrame &f)
        {
            double page_width_ost = f.width / PDF_POINTS_PER_INCH * 1.0;
            double page_height_ost = f.height / PDF_POINTS_PER_INCH * 1.0;
            auto corners = ost_coord::ost_to_pdf_coordinates(
                {0.0, 0.0, page_width_ost, 0.0, page_width_ost, page_height_ost, 0.0, page_height_ost},
                f.width, f.height, 1.0, 1.0, f.rotation, f.flip_x, f.flip_y);
            double h = corners[0][1];
            for (const auto &c : corners)
                h = std::max(h, c[1]);
            return h;
        }

        // mesh_builder._apply_page_transform: every vertex goes through transform_to_3d(-world_x, world_y) of the page
        // (Windows world frame (X, Y); iOS (x, z) = (X, -Y), Y up unchanged); exactly one flip reverses the faces.
        void apply_page_transform_to_mesh(ost_geometry::CppMeshData &mesh, PageFrame &f)
        {
            if (!f.active)
                return;
            if (!f.output_height_ready)
            {
                f.output_height = page_output_height(f);
                f.output_height_ready = true;
            }
            std::vector<double> position(2);
            for (auto &v : mesh.vertices)
            {
                double world_x = v[0];
                double world_y = -v[2];
                position[0] = -world_x;
                position[1] = world_y;
                auto pdf = ost_coord::ost_to_pdf_coordinates(position, f.width, f.height, 1.0, 1.0,
                                                             f.rotation, f.flip_x, f.flip_y);
                double tx = -pdf[0][0] / PDF_POINTS_PER_INCH;
                double ty = (f.output_height - pdf[0][1]) / PDF_POINTS_PER_INCH;
                v[0] = tx;
                v[2] = -ty;
            }
            if (f.flip_x != f.flip_y)
                for (auto &face : mesh.faces)
                    std::swap(face[0], face[2]);
        }

        // Takeoff.is_visible(conditions): no condition (or an unknown one) is visible, else the layer decides.
        bool takeoff_is_visible(const ost_data::ProjectData &project, const ost_domain::Takeoff &t)
        {
            if (t.condition_uid.empty())
                return true;
            auto it = project.conditions.find(t.condition_uid);
            if (it == project.conditions.end())
                return true;
            return project.is_condition_visible(it->second);
        }
    } // namespace

    std::vector<TakeoffMesh> generate_takeoff_meshes(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        bool apply_page_transform)
    {
        // collect_takeoffs_for_pages(visible_only=True): the grouping only sees the selected pages' visible takeoffs.
        std::vector<const ost_domain::Takeoff *> visible;
        for (const auto &t : project.takeoffs)
        {
            if (!selected_pages.empty() && !selected_pages.count(t.page_uid))
                continue;
            if (!takeoff_is_visible(project, t))
                continue;
            visible.push_back(&t);
        }
        AreaHoleGrouping grouping = group_area_takeoffs_with_holes(visible, project.conditions);
        // Hole children are not meshed on their own; every other takeoff keeps its input order.
        std::unordered_set<const ost_domain::Takeoff *> meshed(grouping.takeoffs.begin(), grouping.takeoffs.end());

        PageFrames frames{project, {}};
        std::vector<TakeoffMesh> out;
        for (const ost_domain::Takeoff *takeoff : visible)
        {
            if (!meshed.count(takeoff))
                continue;
            auto cdn = project.conditions.find(takeoff->condition_uid);
            if (cdn == project.conditions.end())
                continue;
            const ost_domain::Condition &condition = cdn->second;

            // Raw OST units (the page scale ratio never scales a mesh): the hole rings get the same
            // (x, z) = (-x_ost, -y_ost) frame as the outline.
            std::vector<std::vector<std::pair<double, double>>> holes;
            if (condition.is_area())
            {
                holes = holes_for_takeoff(takeoff->uid, grouping);
                for (auto &ring : holes)
                    for (auto &p : ring)
                        p = {-p.first, -p.second};
            }

            MeshResult result = create_mesh_for_takeoff(*takeoff, condition, 1.0, holes);
            if (!result.valid || result.mesh.faces.empty())
                continue;
            if (apply_page_transform)
                if (PageFrame *frame = frames.frame_for(takeoff->page_uid))
                    apply_page_transform_to_mesh(result.mesh, *frame);

            TakeoffMesh tm;
            tm.takeoff = takeoff;
            tm.condition = &condition;
            tm.mesh = std::move(result.mesh);
            tm.color_r = result.color_r;
            tm.color_g = result.color_g;
            tm.color_b = result.color_b;
            out.push_back(std::move(tm));
        }
        return out;
    }

}
