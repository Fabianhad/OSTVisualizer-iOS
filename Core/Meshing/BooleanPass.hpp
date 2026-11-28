#pragma once
// The shared boolean pass of the 3D view and of every export (Windows presentation/visualization/core/
// boolean_operations.py apply_boolean_operations + cpp_extensions/src/module_geometry.cpp
// apply_boolean_operations_impl): one rule for all of them (MSH-16 / MSH-15 / EXP-05 / RB-02).
//   * a negative subtracts only from the positives of the SAME condition type;
//   * a positive that the negatives of its type remove completely is dropped (it does not come back);
//   * negatives are never output;
//   * a positive without a negative of its type comes out unchanged;
//   * condition_type is mandatory (require_condition_types, RB-01).
// ExportMeshSet.hpp builds its input from the takeoff meshes; Services/ProjectService.mm (3D view) and
// Services/ExportService.mm (OBJ / DXF / HTML) call those functions.
#include "../Geometry/mesh_data.hpp"
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ost_meshing
{

    // One (mesh, metadata) pair of the Windows pass: the element mesh plus the metadata keys the pass reads.
    struct BooleanPassMesh
    {
        ost_geometry::CppMeshData mesh;
        bool is_negative = false;           // metadata "IsNegativeQuantity"
        std::optional<int> condition_type;  // metadata "condition_type"; nullopt = the key is missing (type 0 is a real type)
        std::string takeoff_uid;            // metadata "takeoff_uid" ("" when absent), named in the error
    };

    // Windows boolean_operations._require_condition_types: every mesh, positive or negative, must carry a
    // condition_type. The first mesh without one (list order) throws std::invalid_argument
    // "Boolean operations need condition_type for takeoff <takeoff_uid>"; an empty list is accepted. A missing type is
    // never treated as type 0 (the old native default that made the OBJ / DXF export subtract all against all).
    void require_condition_types(const std::vector<BooleanPassMesh> &meshes);

    // The boolean difference of one positive and its negatives (ost_geometry::boolean_difference_batch); a function
    // with this signature can replace it (the host tests do). nullopt = nothing is left of the positive.
    using BooleanDifferenceFn = std::function<std::optional<ost_geometry::CppMeshData>(
        const ost_geometry::CppMeshData &positive, const std::vector<ost_geometry::CppMeshData> &negatives)>;

    // One mesh that survived the pass: the index of its entry in the input and its (possibly cut) mesh.
    struct BooleanPassResult
    {
        size_t source_index = 0;
        ost_geometry::CppMeshData mesh;
    };

    // Windows apply_boolean_operations(meshes_with_metadata) (the result is in the order of the positives of the input):
    //   * an empty input gives an empty result; require_condition_types runs first (std::invalid_argument, no boolean
    //     operation has been made);
    //   * without any negative the positives come out as they are (the difference is not called);
    //   * otherwise each positive whose condition type has negatives gets difference(positive, negatives of that type in
    //     input order); a value that is valid (vertices and faces) is repaired with remove_degenerate_faces and replaces
    //     the positive, anything else (nullopt, no vertices or no faces) drops the positive (module_geometry.cpp has no
    //     else for it); a positive of a type without negatives comes out unchanged;
    //   * an exception of the difference (std::exception) makes the whole pass return every positive unchanged and logs
    //     one error "Boolean operations failed; returning positive meshes unchanged: <what>" through ost_log.
    // The winding is not touched. Windows (boolean_operations.py apply_boolean_operations) hands back the ORIGINAL mesh
    // of a positive whose bounding box does not strictly overlap (< on all three axes, touching counts as apart) the bounding
    // box of any negative of its type; only the positives that may be cut (an empty positive counts as one, empty negatives
    // are ignored) go through the native difference. Windows d1c430a made every shape outward (outward_faces in
    // domain/mesh.py:62) and removed the face flip from that pass; iOS already stored every mesh outward-facing, cut or not:
    // winding parity (GEO-15, EXP-13, EXP-M02, MSH-21). Nothing here or downstream reverses or relies on a flip. EXP-17 (the
    // order of the triangles after the bounding-box shortcut) stays a difference: every positive of a type that has a
    // negative goes through the difference (no bounding-box pre-check, so an uncut one comes back rebuilt by it, not as the
    // identical generator mesh).
    std::vector<BooleanPassResult> apply_boolean_operations(
        std::vector<BooleanPassMesh> meshes,
        const BooleanDifferenceFn &difference = {});

}
