// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// Which fields of a Condition an edit changed. Windows never rewrites a whole condition row: the edit dialog builds an
// UpdateConditionDto that holds a field only when its value differs from the stored condition
// (edit_condition_dialog.py _validate_and_build_dto), an empty DTO is a success without a write, and update_condition
// writes exactly the columns of the fields in the DTO (condition_operations.py update_condition/_FIELD_TO_COLUMN).
#pragma once
#include "Condition.hpp"
#include "ConditionFormRules.hpp"
#include <bitset>
#include <cstddef>
#include <string>
#include <unordered_map>

namespace ost_domain
{

    // Every field update_condition can persist. The first block is Windows' _FIELD_TO_COLUMN; ColorLine is not in it
    // (Windows never writes it: a colour change writes ColorFill only). The last three are the iOS-only controls that
    // stay by user decision (COND-EXTRA-01); like every other field they are written only when they changed.
    enum class ConditionField : int
    {
        Name,
        ConditionType,
        Backout,
        RefNo,
        CdnTypeUid,
        LayerUid,
        Height,
        Width,
        Depth,
        Thickness,
        Rise,
        Run,
        DisplaySize,
        Shape,
        Pattern,
        Spacing,
        ColorFill,
        Uom1,
        Uom2,
        Uom3,
        CalcType1,
        CalcType2,
        CalcType3,
        Notes,
        DropRun,
        DropValue,
        RoundQuantity,
        RoundUp,
        Trim,
        IsCurvedSegment,
        Grid,
        GridSize1,
        GridSize2,
        Gap,
        DisplayDimension,
        DisplayName,
        DisplayGridWhileDrawing,
        FolderUid,
        // iOS-only (COND-EXTRA-01, decision: keep)
        Connect,
        ConnectTolerance,
        SnapToLinear,
        kCount
    };

    constexpr std::size_t kConditionFieldCount = static_cast<std::size_t>(ConditionField::kCount);

    // The set of changed fields plus their new values (UpdateConditionDto). Only the fields in the set are written;
    // every other member of `values` is ignored, so a caller never overwrites a column it did not edit.
    struct ConditionChanges
    {
        Condition values;
        std::bitset<kConditionFieldCount> fields;

        void mark(ConditionField field) { fields.set(static_cast<std::size_t>(field)); }
        bool has(ConditionField field) const { return fields.test(static_cast<std::size_t>(field)); }
        // UpdateConditionDto.is_empty(): nothing to save (the form is not dirty).
        bool empty() const { return fields.none(); }
    };

    // The fields whose value differs between the stored condition and the edited one (the dialog compares each form
    // value with `!=` against the stored condition and sets it only when different: exact comparison, no tolerance).
    // The new values are those of `after`.
    inline ConditionChanges collect_condition_changes(const Condition &before, const Condition &after)
    {
        ConditionChanges changes;
        changes.values = after;
#define OST_COND_DIFF(Field, member)        \
    if (before.member != after.member)      \
        changes.mark(ConditionField::Field);
        OST_COND_DIFF(Name, name)
        OST_COND_DIFF(ConditionType, condition_type)
        OST_COND_DIFF(Backout, backout)
        OST_COND_DIFF(RefNo, ref_no)
        OST_COND_DIFF(CdnTypeUid, cdn_type_uid)
        OST_COND_DIFF(LayerUid, layer_uid)
        OST_COND_DIFF(Height, height)
        OST_COND_DIFF(Width, width)
        OST_COND_DIFF(Depth, depth)
        OST_COND_DIFF(Thickness, thickness)
        OST_COND_DIFF(Rise, rise)
        OST_COND_DIFF(Run, run)
        OST_COND_DIFF(DisplaySize, display_size)
        OST_COND_DIFF(Shape, shape)
        OST_COND_DIFF(Pattern, pattern)
        OST_COND_DIFF(Spacing, spacing)
        OST_COND_DIFF(ColorFill, color_fill)
        OST_COND_DIFF(Uom1, uom1)
        OST_COND_DIFF(Uom2, uom2)
        OST_COND_DIFF(Uom3, uom3)
        OST_COND_DIFF(CalcType1, calc_type1)
        OST_COND_DIFF(CalcType2, calc_type2)
        OST_COND_DIFF(CalcType3, calc_type3)
        OST_COND_DIFF(Notes, notes)
        OST_COND_DIFF(DropRun, drop_run)
        OST_COND_DIFF(DropValue, drop_value)
        OST_COND_DIFF(RoundQuantity, round_quantity)
        OST_COND_DIFF(RoundUp, round_up)
        OST_COND_DIFF(Trim, trim)
        OST_COND_DIFF(IsCurvedSegment, is_curved_segment)
        OST_COND_DIFF(Grid, grid)
        OST_COND_DIFF(GridSize1, grid_size1)
        OST_COND_DIFF(GridSize2, grid_size2)
        OST_COND_DIFF(Gap, gap)
        OST_COND_DIFF(DisplayDimension, display_dimension)
        OST_COND_DIFF(DisplayName, display_name)
        OST_COND_DIFF(DisplayGridWhileDrawing, display_grid_while_drawing)
        OST_COND_DIFF(FolderUid, condition_folder_uid)
        OST_COND_DIFF(Connect, connect)
        OST_COND_DIFF(ConnectTolerance, connect_tolerance)
        OST_COND_DIFF(SnapToLinear, snap_to_linear)
#undef OST_COND_DIFF
        return changes;
    }

    // Every persistable field marked: for a caller that only holds the edited condition and cannot tell what changed
    // (it rewrites every column that has a field, minus ColorLine). Prefer collect_condition_changes.
    inline ConditionChanges all_condition_fields(const Condition &condition)
    {
        ConditionChanges changes;
        changes.values = condition;
        changes.fields.set();
        return changes;
    }

    // In-memory counterpart of the reference-number shift that ost_store::update_condition runs when a condition takes a
    // number (Windows _shift_conflicting_ref_nos_for_update): when the number really changes and another condition already
    // holds it, every other condition from that number up moves up by one. Call it BEFORE the edited condition is stored
    // back into `conditions`, and only after the database update succeeded.
    // Like the store (DLG-FINITE-01) it refuses a shift that would carry a condition past kMaxConditionNumber: it then changes
    // nothing and returns false (the store has refused the same renumber, so after a successful update this never happens; it
    // keeps an int from overflowing on a map that has drifted from the database). A number that is free, a number the condition
    // already holds and a condition that is not in the map are not refusals: true. A condition without a number reads as 0
    // (NULL in the file) and is never shifted, because the new number is at least 1.
    inline bool shift_conflicting_ref_nos(std::unordered_map<std::string, Condition> &conditions,
                                          const std::string &condition_uid, int new_ref_no)
    {
        auto current = conditions.find(condition_uid);
        if (current == conditions.end() || current->second.ref_no == new_ref_no)
            return true;
        bool taken = false;
        for (const auto &pair : conditions)
            if (pair.first != condition_uid && pair.second.ref_no == new_ref_no)
                taken = true;
        if (!taken)
            return true;
        for (const auto &pair : conditions)
            if (pair.first != condition_uid && pair.second.ref_no >= new_ref_no && pair.second.ref_no >= kMaxConditionNumber)
                return false;
        for (auto &pair : conditions)
            if (pair.first != condition_uid && pair.second.ref_no >= new_ref_no)
                pair.second.ref_no += 1;
        return true;
    }

}
