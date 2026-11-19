#include "ExportMeshSet.hpp"
#include "../Data/PageNavigation.hpp"
#include "../Export/export_text.hpp"
#include <cstddef>
#include <unordered_map>
#include <utility>

namespace ost_meshing
{

    namespace
    {
        // Python condition_uid[:8]: the first eight characters (code points), not bytes.
        std::string first_eight_characters(const std::string &text)
        {
            size_t characters = 0;
            size_t i = 0;
            while (i < text.size())
            {
                // a byte that is not a UTF-8 continuation byte starts a character
                if ((static_cast<unsigned char>(text[i]) & 0xC0u) != 0x80u)
                {
                    if (characters == 8)
                        break;
                    ++characters;
                }
                ++i;
            }
            return text.substr(0, i);
        }

        // The meshes of the takeoff meshes after the boolean pass, in the order of the pass result. `generated` is
        // consumed (its meshes move into the pass).
        std::vector<BooleanMesh> run_boolean_pass(std::vector<TakeoffMesh> &generated,
                                                  const ost_domain::RenderConfig &config,
                                                  const BooleanDifferenceFn &difference)
        {
            // The metadata of the Windows pass: IsNegativeQuantity, condition_type (always set here: the condition is
            // known), takeoff_uid.
            std::vector<BooleanPassMesh> entries;
            entries.reserve(generated.size());
            for (auto &g : generated)
            {
                BooleanPassMesh e;
                e.mesh = std::move(g.mesh);
                e.is_negative = g.takeoff->is_negative;
                e.condition_type = static_cast<int>(g.condition->condition_type);
                e.takeoff_uid = g.takeoff->uid;
                entries.push_back(std::move(e));
            }
            std::vector<BooleanPassResult> results = apply_boolean_operations(std::move(entries), difference);
            std::vector<BooleanMesh> out;
            out.reserve(results.size());
            for (auto &r : results)
            {
                // A mesh without vertices or faces (the repair after a cut removed every face) draws and writes nothing;
                // it is not counted as a surviving mesh. (Windows keeps such an entry, so an export whose only meshes
                // were emptied by the repair would "succeed" with an empty file; an unreachable corner that looks
                // accidental, so iOS keeps reporting nothing to export there.)
                if (!r.mesh.is_valid())
                    continue;
                const TakeoffMesh &source = generated[r.source_index];
                BooleanMesh m;
                m.takeoff = source.takeoff;
                m.condition = source.condition;
                m.mesh = std::move(r.mesh);
                m.color = ost_domain::display_color(*source.condition, config);
                out.push_back(std::move(m));
            }
            return out;
        }
    } // namespace

    std::string export_condition_name(const std::string &condition_uid, const ost_domain::Condition &condition)
    {
        return condition.name.empty() ? "Condition_" + condition_uid : condition.name;
    }

    std::string export_material_name(const std::string &condition_uid, const ost_domain::Condition &condition)
    {
        const std::string cdn_type = condition.cdn_type_name.empty() ? std::string("Unknown") : condition.cdn_type_name;
        return ost_geometry::export_text::sanitize_material_name(
            cdn_type + "_" + export_condition_name(condition_uid, condition) + "_" + first_eight_characters(condition_uid));
    }

    std::vector<BooleanMesh> generate_view_meshes(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        const ost_domain::RenderConfig &config,
        const BooleanDifferenceFn &difference)
    {
        const ElementTakeoffs element_takeoffs = collect_element_takeoffs(project, selected_pages);
        std::vector<TakeoffMesh> generated =
            mesh_takeoffs(project, element_takeoffs, order_by_condition_type(project, element_takeoffs), true);
        return run_boolean_pass(generated, config, difference);
    }

    SceneExport generate_html_export(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        const ost_domain::RenderConfig &config,
        const BooleanDifferenceFn &difference)
    {
        SceneExport out;
        const ElementTakeoffs element_takeoffs = collect_element_takeoffs(project, selected_pages);
        if (element_takeoffs.collected == 0)
        {
            out.status = ExportStatus::NoTakeoffs;
            return out;
        }
        // visualize_with_threejs: no conditions or no takeoffs give None; so does a scene without a surviving mesh.
        if (project.conditions.empty())
        {
            out.status = ExportStatus::NothingToExport;
            return out;
        }
        std::vector<TakeoffMesh> generated =
            mesh_takeoffs(project, element_takeoffs, order_by_condition_type(project, element_takeoffs), false);
        out.meshes = run_boolean_pass(generated, config, difference);
        if (out.meshes.empty())
            out.status = ExportStatus::NothingToExport;
        return out;
    }

    GroupedExport generate_condition_export(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        const ost_domain::RenderConfig &config,
        const BooleanDifferenceFn &difference)
    {
        GroupedExport out;
        const ElementTakeoffs element_takeoffs = collect_element_takeoffs(project, selected_pages);
        if (element_takeoffs.collected == 0)
        {
            out.status = ExportStatus::NoTakeoffs;
            return out;
        }
        // BaseExporter.export: no exportable takeoff gives False before anything else happens.
        if (element_takeoffs.takeoffs.empty())
        {
            out.status = ExportStatus::NothingToExport;
            return out;
        }

        // _prepare_hierarchical_export: takeoffs_by_condition in first-seen order of the condition uid; the colour comes
        // from the condition (the first takeoff of the group).
        std::vector<ExportGroup> groups;
        std::vector<std::vector<const ost_domain::Takeoff *>> members;
        std::unordered_map<std::string, size_t> group_of;
        for (const ost_domain::Takeoff *t : element_takeoffs.takeoffs)
        {
            auto it = group_of.find(t->condition_uid);
            if (it == group_of.end())
            {
                const ost_domain::Condition &condition = project.conditions.at(t->condition_uid);
                ExportGroup g;
                g.condition_uid = t->condition_uid;
                g.material_name = export_material_name(t->condition_uid, condition);
                g.name = export_condition_name(t->condition_uid, condition);
                g.color = ost_domain::display_color(condition, config);
                it = group_of.emplace(t->condition_uid, groups.size()).first;
                groups.push_back(std::move(g));
                members.emplace_back();
            }
            members[it->second].push_back(t);
        }

        // _apply_boolean_operations: the meshes go through the pass group by group.
        std::vector<const ost_domain::Takeoff *> order;
        order.reserve(element_takeoffs.takeoffs.size());
        for (const auto &group_members : members)
            order.insert(order.end(), group_members.begin(), group_members.end());
        std::vector<TakeoffMesh> generated = mesh_takeoffs(project, element_takeoffs, order, false);
        std::vector<BooleanMesh> survivors = run_boolean_pass(generated, config, difference);
        // The guard after the pass: an empty processed_mesh_cache returns False before anything is written.
        if (survivors.empty())
        {
            out.status = ExportStatus::NothingToExport;
            return out;
        }

        // processed_mesh_cache[takeoff_uid] = mesh (a takeoff without uid is not cached), read back per takeoff by the
        // writers (_exported_mesh_for_takeoff); a negative or a takeoff that is not in the cache writes nothing.
        std::unordered_map<std::string, size_t> cache;
        for (size_t i = 0; i < survivors.size(); ++i)
            if (!survivors[i].takeoff->uid.empty())
                cache[survivors[i].takeoff->uid] = i;
        // A cached mesh is read once per positive takeoff of its uid: the last reader takes it instead of copying it.
        std::vector<size_t> readers(survivors.size(), 0);
        for (size_t g = 0; g < groups.size(); ++g)
            for (const ost_domain::Takeoff *t : members[g])
            {
                auto hit = cache.find(t->uid);
                if (!t->is_negative && hit != cache.end())
                    ++readers[hit->second];
            }
        for (size_t g = 0; g < groups.size(); ++g)
        {
            for (const ost_domain::Takeoff *t : members[g])
            {
                if (t->is_negative)
                    continue;
                auto hit = cache.find(t->uid);
                if (hit == cache.end())
                    continue;
                BooleanMesh &survivor = survivors[hit->second];
                const bool last_reader = --readers[hit->second] == 0;
                if (last_reader)
                    groups[g].meshes.push_back(std::move(survivor));
                else
                    groups[g].meshes.push_back(survivor);
            }
        }
        out.groups = std::move(groups);
        return out;
    }

    GroupedExport generate_condition_export_for_selection(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        const ost_domain::RenderConfig &config,
        const BooleanDifferenceFn &difference)
    {
        GroupedExport nothing;
        if (selected_pages.empty())
        {
            nothing.status = ExportStatus::NoTakeoffs;
            return nothing;
        }
        return generate_condition_export(project, selected_pages, config, difference);
    }

    SceneExport generate_html_export_for_selection(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages,
        const ost_domain::RenderConfig &config,
        const BooleanDifferenceFn &difference)
    {
        SceneExport nothing;
        if (selected_pages.empty())
        {
            nothing.status = ExportStatus::NoTakeoffs;
            return nothing;
        }
        return generate_html_export(project, selected_pages, config, difference);
    }

    std::vector<std::string> export_page_names(
        const ost_data::ProjectData &project,
        const std::set<std::string> &selected_pages)
    {
        std::vector<std::string> names;
        for (const std::string &uid :
             pages_with_visible_takeoffs(project, ost_data::selected_pages_in_tree_order(project, selected_pages)))
            names.push_back(project.pages.at(uid).name);
        return names;
    }

}
