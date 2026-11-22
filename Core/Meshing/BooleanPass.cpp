#include "BooleanPass.hpp"
#include "../Domain/CoreLog.hpp"
#include "../Geometry/boolean_ops.hpp"
#include "../Geometry/mesh_repair.hpp"
#include <exception>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace ost_meshing
{

    void require_condition_types(const std::vector<BooleanPassMesh> &meshes)
    {
        // Windows _require_condition_types: the first mesh (list order) without a condition_type names its takeoff.
        for (const auto &entry : meshes)
        {
            if (!entry.condition_type.has_value())
                throw std::invalid_argument("Boolean operations need condition_type for takeoff " + entry.takeoff_uid);
        }
    }

    namespace
    {
        // The fate of one positive in the native rule (module_geometry.cpp apply_boolean_operations_impl).
        enum class Fate
        {
            Unchanged,
            Replaced,
            Dropped
        };
    } // namespace

    std::vector<BooleanPassResult> apply_boolean_operations(
        std::vector<BooleanPassMesh> meshes,
        const BooleanDifferenceFn &difference)
    {
        std::vector<BooleanPassResult> results;
        if (meshes.empty())
            return results;
        require_condition_types(meshes);

        bool has_negative = false;
        for (const auto &entry : meshes)
            if (entry.is_negative)
            {
                has_negative = true;
                break;
            }

        auto take_unchanged = [&](size_t i)
        {
            BooleanPassResult r;
            r.source_index = i;
            r.mesh = std::move(meshes[i].mesh);
            results.push_back(std::move(r));
        };

        // No negative: the positives come out as they are, in input order (Python shortcut before the native call).
        if (!has_negative)
        {
            for (size_t i = 0; i < meshes.size(); ++i)
                take_unchanged(i);
            return results;
        }

        std::vector<Fate> fate(meshes.size(), Fate::Unchanged);
        std::unordered_map<size_t, ost_geometry::CppMeshData> replaced;
        try
        {
            // negatives_by_type: the negatives of every condition type, in input order.
            std::unordered_map<int, std::vector<ost_geometry::CppMeshData>> negatives_by_type;
            for (auto &entry : meshes)
                if (entry.is_negative)
                    negatives_by_type[*entry.condition_type].push_back(std::move(entry.mesh));

            for (size_t i = 0; i < meshes.size(); ++i)
            {
                if (meshes[i].is_negative)
                    continue;
                auto negatives = negatives_by_type.find(*meshes[i].condition_type);
                if (negatives == negatives_by_type.end() || negatives->second.empty())
                    continue; // a positive without a negative of its type comes out unchanged
                std::optional<ost_geometry::CppMeshData> diff =
                    difference ? difference(meshes[i].mesh, negatives->second)
                               : ost_geometry::boolean_difference_batch(meshes[i].mesh, negatives->second);
                if (diff.has_value() && diff->is_valid())
                {
                    ost_geometry::remove_degenerate_faces(*diff);
                    fate[i] = Fate::Replaced;
                    replaced[i] = std::move(*diff);
                }
                else
                {
                    fate[i] = Fate::Dropped; // nothing left of the positive: dropped, never regenerated
                }
            }
        }
        catch (const std::exception &e)
        {
            // Windows apply_boolean_operations: the whole pass falls back to the positives, unchanged.
            ost_log::log(ost_log::Level::Error,
                         std::string("Boolean operations failed; returning positive meshes unchanged: ") + e.what());
            for (size_t i = 0; i < meshes.size(); ++i)
                if (!meshes[i].is_negative)
                    take_unchanged(i);
            return results;
        }

        for (size_t i = 0; i < meshes.size(); ++i)
        {
            if (meshes[i].is_negative)
                continue;
            if (fate[i] == Fate::Dropped)
                continue;
            if (fate[i] == Fate::Replaced)
            {
                BooleanPassResult r;
                r.source_index = i;
                r.mesh = std::move(replaced[i]);
                results.push_back(std::move(r));
            }
            else
            {
                take_unchanged(i);
            }
        }
        return results;
    }

}
