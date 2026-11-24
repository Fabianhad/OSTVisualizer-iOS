// Layer: Domain. See ConditionCreate.hpp.
//
// Windows 49a2d59: condition_action_handler.py:547-619 (default layer, synthetic condition, build_spec), edit_condition_dialog.py:97-150 (TYPE_DEFAULTS),
// create_condition_spec_dto.py:5-43, conditions_sidebar.py:1499-1513. The deliberate safe deviations are listed in ConditionCreate.hpp.
#include "ConditionCreate.hpp"

#include "../Import/TextRules.hpp"
#include "ConditionStyleChange.hpp"
#include "ConditionStyleDefaults.hpp"
#include "DimensionFormat.hpp"
#include "TextLimits.hpp"

#include <cmath>

namespace ost_domain
{

    namespace
    {
        // The Windows synthetic condition starts as a Linear condition (condition_action_handler.py:552-587): the colour 13353215 (0xCBC0FF, ColorFill is
        // BGR, so R 255, G 192, B 203) is the pink the dialog proposes, the pattern is Transparent.
        constexpr int kNewConditionColorFill = 13353215;

        bool is_known_style(int condition_type)
        {
            return condition_type >= static_cast<int>(ConditionType::Linear) && condition_type <= static_cast<int>(ConditionType::Attachment);
        }

        bool all_finite(const CreateConditionSpec &s)
        {
            const double numbers[] = {s.height, s.width,        s.depth,       s.thickness,  s.rise, s.run, s.display_size,
                                      s.spacing, s.drop_value, s.round_up,    s.grid_size1, s.grid_size2, s.gap};
            for (const double value : numbers)
                if (!std::isfinite(value))
                    return false;
            return !s.connect_tolerance || std::isfinite(*s.connect_tolerance);
        }

        bool has_nul(const std::string &text)
        {
            return text.find('\0') != std::string::npos;
        }
    }

    Condition new_condition_form(const std::string &default_layer_uid, bool metric)
    {
        // The start values are those the dialog shows after choosing Linear in the bid's measurement system (style_dimension_defaults:
        // imperial = Windows, metric = the millimetre columns, N-M1-08).
        const StyleDimensionDefaults defaults = style_dimension_defaults(ConditionType::Linear, metric);

        Condition form;
        form.uid = "__new__";
        form.name = "";
        form.condition_type = ConditionType::Linear;
        form.thickness = defaults.thickness;
        form.pattern = Pattern::Transparent;
        form.display_size = 100.0;
        form.width = 12.0; // the Windows value in both systems; a Count / Attachment in a metric bid changes it through its style choice
        form.height = 0.0;
        form.depth = 0.0;
        form.rise = 0.0;
        form.run = 0.0;
        form.spacing = defaults.spacing;
        form.color_fill = kNewConditionColorFill;
        form.shape = static_cast<ShapeId>(-1);
        form.layer_uid = default_layer_uid;
        form.uom1 = defaults.uom1;
        form.uom2 = -1;
        form.uom3 = -1;
        form.calc_type1 = 1;
        form.calc_type2 = 0;
        form.calc_type3 = 0;
        form.round_up = 0.0;
        form.round_quantity = false;
        form.backout = false;
        form.drop_run = false;
        form.drop_value = 0.0;
        form.trim = false;
        form.is_curved_segment = false;
        form.grid = false;
        form.grid_size1 = 0.0;
        form.grid_size2 = 0.0;
        form.gap = 0.0;
        form.display_dimension = false;
        form.display_name = false;
        form.display_grid_while_drawing = false;
        // The iOS-only controls (COND-EXTRA-01) as apply_style_change leaves them for the style Linear: the form is "Linear, already chosen".
        form.connect = true;
        form.connect_tolerance = 6.0;
        form.snap_to_linear = -1;
        // ref_no stays 0 (Windows leaves it at the Condition default); the create form shows the next number instead (N-M1-07)
        return form;
    }

    std::optional<TypeDefaultsRow> type_defaults_row(int condition_type, bool metric)
    {
        if (!is_known_style(condition_type)) // `TYPE_DEFAULTS.get(cond_type, {})`: a table miss has no row
            return std::nullopt;
        const ConditionType style = static_cast<ConditionType>(condition_type);
        // N-M1-08: no second copy of TYPE_DEFAULTS. The dimensions are style_dimension_defaults, the rest is what apply_style_change leaves in the
        // form of that style (calc_type1, shape, backout, display_grid_while_drawing).
        const StyleDimensionDefaults dimensions = style_dimension_defaults(style, metric);
        const Condition styled = apply_style_change(new_condition_form("", metric), style, metric).condition;
        TypeDefaultsRow row;
        row.thickness = dimensions.thickness;
        row.spacing = dimensions.spacing;
        row.calc_type1 = styled.calc_type1;
        row.uom1 = dimensions.uom1;
        row.shape = static_cast<int>(styled.shape);
        row.backout = styled.backout;
        row.display_grid_while_drawing = styled.display_grid_while_drawing;
        return row;
    }

    std::string default_new_condition_layer(const std::vector<BidLayer> &merged_layers,
                                            const std::unordered_map<std::string, Layer> &bid_layers)
    {
        // condition_action_handler.py:547-551 `for bl in all_layers: if bl.name == "Default"`: the first of the list, case-sensitive, no trimming; NC-2:
        // only a layer of the bid itself counts (a shared template row would make every insert fail).
        for (const BidLayer &layer : merged_layers)
            if (layer.name == "Default" && bid_layers.find(layer.uid) != bid_layers.end())
                return layer.uid;
        return std::string();
    }

    CreateConditionSpec build_create_spec(const Condition &form, const ConditionChanges &changes, const std::string &folder_uid,
                                          const std::string &default_layer_uid, bool metric)
    {
        const Condition &values = changes.values;
        const int type = changes.has(ConditionField::ConditionType) ? static_cast<int>(values.condition_type)
                                                                    : static_cast<int>(form.condition_type);

        CreateConditionSpec spec;
        spec.name = "Untitled";
        spec.condition_type = type;
        spec.pattern = static_cast<int>(Pattern::Transparent);
        spec.color_fill = form.color_fill;
        spec.layer_uid = default_layer_uid;
        spec.display_dimension = false;

        // `row = TYPE_DEFAULTS.get(cond_type, {})`: the row of the style, nothing for a table miss (the spec keeps its plain defaults)
        const std::optional<TypeDefaultsRow> row = type_defaults_row(type, metric);
        // The iOS-only controls (N-M1-12): what apply_style_change leaves in the form of that style, so that a Save of an untouched style choice
        // writes what the Edit dialog would write; a table miss has no style and takes the form's own values.
        bool connect = form.connect;
        double connect_tolerance = form.connect_tolerance;
        int snap_to_linear = form.snap_to_linear;
        if (row)
        {
            spec.thickness = row->thickness;
            spec.spacing = row->spacing;
            spec.shape = row->shape;
            spec.uom1 = row->uom1;
            spec.calc_type1 = row->calc_type1;
            spec.display_grid_while_drawing = row->display_grid_while_drawing;
            spec.backout = row->backout;
            const Condition styled = apply_style_change(form, static_cast<ConditionType>(type), metric).condition;
            connect = styled.connect;
            connect_tolerance = styled.connect_tolerance;
            snap_to_linear = styled.snap_to_linear;
        }
        spec.connect = connect;
        spec.connect_tolerance = connect_tolerance;
        spec.snap_to_linear = snap_to_linear;

        // :612-617: every changed field that is a field of CreateConditionSpec replaces its value. RefNo (the number the dialog asks for and the
        // store ignores, NC-1), the folder (the argument decides, :618) and every field that is no spec field are dropped.
        const auto has = [&changes](ConditionField field) { return changes.has(field); };
        if (has(ConditionField::Name))
            spec.name = values.name;
        if (has(ConditionField::ConditionType))
            spec.condition_type = static_cast<int>(values.condition_type);
        if (has(ConditionField::Backout))
            spec.backout = values.backout;
        if (has(ConditionField::CdnTypeUid))
            spec.cdn_type_uid = values.cdn_type_uid;
        if (has(ConditionField::LayerUid))
            spec.layer_uid = values.layer_uid;
        if (has(ConditionField::Height))
            spec.height = values.height;
        if (has(ConditionField::Width))
            spec.width = values.width;
        if (has(ConditionField::Depth))
            spec.depth = values.depth;
        if (has(ConditionField::Thickness))
            spec.thickness = values.thickness;
        if (has(ConditionField::Rise))
            spec.rise = values.rise;
        if (has(ConditionField::Run))
            spec.run = values.run;
        if (has(ConditionField::DisplaySize))
            spec.display_size = values.display_size;
        if (has(ConditionField::Shape))
            spec.shape = static_cast<int>(values.shape);
        if (has(ConditionField::Pattern))
            spec.pattern = static_cast<int>(values.pattern);
        if (has(ConditionField::Spacing))
            spec.spacing = values.spacing;
        if (has(ConditionField::ColorFill))
            spec.color_fill = values.color_fill;
        if (has(ConditionField::Uom1))
            spec.uom1 = values.uom1;
        if (has(ConditionField::Uom2))
            spec.uom2 = values.uom2;
        if (has(ConditionField::Uom3))
            spec.uom3 = values.uom3;
        if (has(ConditionField::CalcType1))
            spec.calc_type1 = values.calc_type1;
        if (has(ConditionField::CalcType2))
            spec.calc_type2 = values.calc_type2;
        if (has(ConditionField::CalcType3))
            spec.calc_type3 = values.calc_type3;
        if (has(ConditionField::Notes))
            spec.notes = values.notes;
        if (has(ConditionField::DropRun))
            spec.drop_run = values.drop_run;
        if (has(ConditionField::DropValue))
            spec.drop_value = values.drop_value;
        if (has(ConditionField::RoundQuantity))
            spec.round_quantity = values.round_quantity;
        if (has(ConditionField::RoundUp))
            spec.round_up = values.round_up;
        if (has(ConditionField::Trim))
            spec.trim = values.trim;
        if (has(ConditionField::IsCurvedSegment))
            spec.is_curved_segment = values.is_curved_segment;
        if (has(ConditionField::Grid))
            spec.grid = values.grid;
        if (has(ConditionField::GridSize1))
            spec.grid_size1 = values.grid_size1;
        if (has(ConditionField::GridSize2))
            spec.grid_size2 = values.grid_size2;
        if (has(ConditionField::Gap))
            spec.gap = values.gap;
        if (has(ConditionField::DisplayDimension))
            spec.display_dimension = values.display_dimension;
        if (has(ConditionField::DisplayName))
            spec.display_name = values.display_name;
        if (has(ConditionField::DisplayGridWhileDrawing))
            spec.display_grid_while_drawing = values.display_grid_while_drawing;
        if (has(ConditionField::Connect))
            spec.connect = values.connect;
        if (has(ConditionField::ConnectTolerance))
            spec.connect_tolerance = values.connect_tolerance;
        if (has(ConditionField::SnapToLinear))
            spec.snap_to_linear = values.snap_to_linear;

        spec.folder_uid = folder_uid; // :618 `spec.folder_uid = folder_uid or None`
        return spec;
    }

    FormIssue condition_create_failed_issue(const std::string &detail)
    {
        // condition_action_handler.py:650-653 `result.user_message or "Failed to create condition."`, shown under the dialog's "Save Failed"
        // (edit_condition_dialog.py:1810-1817)
        return FormIssue{"Save Failed", detail.empty() ? std::string("Failed to create condition.") : detail};
    }

    std::optional<FormIssue> validate_create_spec(const CreateConditionSpec &spec)
    {
        // Text that is not valid UTF-8 is never a name (utf16_length: nullopt): a writer refuses it, it never repairs it.
        const std::optional<std::size_t> units = utf16_length(spec.name);
        if (!units)
            return condition_create_failed_issue();
        // The same for the notes: they are stored as TEXT (plan 4.5), and the loader would read invalid bytes back as windows-1252, so the text read
        // back would not be the text written (Windows binds a str, which is always valid text).
        if (!utf16_length(spec.notes))
            return condition_create_failed_issue();
        // NC-5: the Windows writer accepts an empty name (the dialog refuses it first with name.strip()); a blank one is refused here
        if (ost_import::text_rules::py_strip(spec.name).empty())
            return condition_create_failed_issue();
        // a NUL cuts a C string: the name (and the notes) the loader reads back would not be the text that was written
        if (has_nul(spec.name) || has_nul(spec.notes))
            return condition_create_failed_issue();
        // NC-10: the Windows writer passes NaN / infinity on (SQLite stores NaN as NULL silently)
        if (!all_finite(spec))
            return condition_create_failed_issue();
        // NC-4 / N-M1-13: Name VARCHAR(255) counts UTF-16 units, the " @T value" suffix included (the spec's name is the whole name)
        if (*units > kMaxConditionNameUnits)
            return FormIssue{kFormValidationErrorTitle, kConditionNameTooLongMessage};
        return std::nullopt;
    }

    std::string folder_context_uid(const std::vector<ConditionTreeNode> &nodes, std::size_t index)
    {
        // conditions_sidebar.py:1499-1513: the item itself when it is a folder, else its parent chain, the first folder wins. The nodes are in
        // pre-order, so the parent of a node is the closest PRECEDING node with a smaller depth: one backward scan, no recursion over the depth.
        if (index >= nodes.size())
            return std::string();
        if (nodes[index].kind == ConditionTreeNode::Kind::Folder)
            return nodes[index].uid;
        int wanted_depth = nodes[index].depth;
        for (std::size_t i = index; i-- > 0;)
        {
            if (nodes[i].depth >= wanted_depth)
                continue;
            if (nodes[i].kind == ConditionTreeNode::Kind::Folder)
                return nodes[i].uid;
            wanted_depth = nodes[i].depth;
        }
        return std::string();
    }

}
