// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// New Condition (plan 5.1, M1): the rules that sit between the Edit Condition form in its create mode and the store writer
// ost_store::insert_condition (ProjectStore_ConditionCreate.hpp).
//
// Windows source of truth (49a2d59):
//   application/dtos/create_condition_spec_dto.py:5-43              CreateConditionSpec and every default (there is no ref_no field)
//   presentation/dialogs/edit_condition_dialog.py:97-150            TYPE_DEFAULTS (imperial and metric columns), :1250-1266 the style defaults
//   presentation/handlers/condition_action_handler.py:547-551       the default layer, :552-587 the synthetic condition,
//                                                                   :593-619 build_spec, :621-653 save_new_condition (failure text)
//   presentation/components/conditions_sidebar.py:1499-1513         _resolve_folder_context_uid
// Deliberate safe deviations (plan section 7.1 and 8, register NC-1 to NC-12): the default layer must belong to the bid (NC-2), a metric
// bid starts with the metric defaults (NC-6, N-M1-08), the store refuses a blank name, a name over 255 UTF-16 units and non-finite numbers
// (NC-4, NC-5, NC-10, N-M1-13).
//
// The contract is in these comments and in Config/Tests/Conditions/test_condition_create_form.cpp.
#pragma once
#include "Condition.hpp"
#include "ConditionChanges.hpp"
#include "ConditionFormRules.hpp"
#include "ConditionTree.hpp"
#include "Layer.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_domain
{

    // create_condition_spec_dto.py:5-43, the same fields in the same order with the same defaults. condition_type is the RAW int (the Windows
    // test creates type 99). "" in a uid field is None (NULL). There is no ref_no and no uid: the store assigns both.
    struct CreateConditionSpec
    {
        std::string name = "Untitled";
        int condition_type = 0;
        bool backout = false;
        std::string cdn_type_uid;
        std::string layer_uid;
        std::string folder_uid;
        double height = 0.0;
        double width = 12.0;
        double depth = 0.0;
        double thickness = 4.0;
        double rise = 0.0;
        double run = 0.0;
        double display_size = 100.0;
        int shape = -1;
        int pattern = 0;
        double spacing = 4.0;
        int color_fill = 0;
        int uom1 = 0;
        int uom2 = -1;
        int uom3 = -1;
        int calc_type1 = 0;
        int calc_type2 = 0;
        int calc_type3 = 0;
        std::string notes;
        bool drop_run = false;
        double drop_value = 0.0;
        bool round_quantity = false;
        double round_up = 0.0;
        bool trim = false;
        bool is_curved_segment = false;
        bool grid = false;
        double grid_size1 = 0.0;
        double grid_size2 = 0.0;
        double gap = 0.0;
        bool display_dimension = false;
        bool display_name = false;
        bool display_grid_while_drawing = false;
        // iOS-only controls (COND-EXTRA-01, N-M1-12): Windows-schema columns Connect / ConnectTolerance / SnapToLinear that the Windows insert
        // never writes. The store writes a column only when its optional is set (unset = NULL); build_create_spec sets all three.
        std::optional<bool> connect;
        std::optional<double> connect_tolerance;
        std::optional<int> snap_to_linear;
    };

    // One row of TYPE_DEFAULTS (edit_condition_dialog.py:97-150) for the measurement system of the bid (N-M1-08): thickness and spacing in
    // INCHES (imperial = the table's inch columns, metric = mm_to_inches of the millimetre columns, i.e. style_dimension_defaults), uom1 the
    // imperial or metric unit column. Built from style_dimension_defaults and apply_style_change, not from a second copy of the table.
    struct TypeDefaultsRow
    {
        double thickness = 4.0;
        double spacing = 4.0;
        int calc_type1 = 0;
        int uom1 = 0;
        int shape = -1;
        bool backout = false;
        bool display_grid_while_drawing = false;
    };

    // The row of the style `condition_type` (0 Linear, 1 Area, 2 Count, 3 Attachment); nullopt for any other value (the Windows table miss,
    // `TYPE_DEFAULTS.get(cond_type, {})`: build_create_spec then uses the plain CreateConditionSpec defaults).
    std::optional<TypeDefaultsRow> type_defaults_row(int condition_type, bool metric);

    // condition_action_handler.py:547-551 with the NC-2 safe rule: the uid of the FIRST layer of `merged_layers` (list order, the order of
    // merge_layers_for_bid) whose name is exactly "Default" (case-sensitive, no trimming) AND which is one of the bid's own layers (its uid is a key of
    // `bid_layers`: a shared template row is skipped); "" (no layer) when there is none. Windows takes the first "Default" of the merged list, shared
    // template rows included, and every create then fails on a bid without its own "Default".
    std::string default_new_condition_layer(const std::vector<BidLayer> &merged_layers,
                                            const std::unordered_map<std::string, Layer> &bid_layers);

    // The synthetic condition of a new condition's form (condition_action_handler.py:552-587): uid "__new__", name "", Linear, thickness 4.0,
    // pattern Transparent (8), display_size 100.0, width 12.0 (imperial; 300 mm in a metric bid, below), height / depth / rise / run 0.0, spacing 4.0, color_fill 13353215, shape -1,
    // layer = `default_layer_uid`, uom1 2, uom2 / uom3 -1, calc_type1 1, calc_type2 / calc_type3 0, round_up 0.0, every flag false, grid sizes and
    // gap 0.0; ref_no stays 0 (the form shows "Next number" instead, N-M1-07). In a METRIC bid (N-M1-08) thickness and spacing are
    // mm_to_inches(100.0) and uom1 is 13 (Windows starts from the imperial values there, NC-6); the width is mm_to_inches(300.0) = 11.811023622047244
    // (Windows d1c430a: mm_to_inches(300.0) if metric else 12.0, every style, and build_spec starts from it). It is the form as it looks after choosing the
    // style Linear: apply_style_change(form, Linear, metric) changes nothing in it (including the iOS-only Connect controls).
    Condition new_condition_form(const std::string &default_layer_uid, bool metric);

    // condition_action_handler.py:593-619 build_spec. `form` is the synthetic condition (new_condition_form) and `changes` what the dialog's Save
    // sends (condition_changes_for_save(form, edited form, metric); an empty set for an untouched form). The spec starts as the plain
    // CreateConditionSpec defaults with name "Untitled", pattern Transparent, color_fill = form.color_fill, width = form.width (Windows d1c430a
    // width=synthetic.width: the dialog sends only values that differ from the form's start, so a metric Count would otherwise fall back to
    // the spec default 12.0), layer_uid = `default_layer_uid`,
    // display_dimension false and the row of type_defaults_row(type, metric) for thickness, spacing, shape, uom1, calc_type1,
    // display_grid_while_drawing and backout, where type = changes.values.condition_type when the change set has ConditionType, else
    // form.condition_type (a table miss keeps the spec defaults). Then every field of `changes` that is a spec field replaces its value (RefNo and
    // every field that is not a spec field are dropped); the folder is `folder_uid` ("" = the top level) and wins over a FolderUid change. The
    // three iOS-only controls are set from the same style (connect, connect_tolerance and snap_to_linear as apply_style_change leaves them for
    // that style) unless `changes` carries them.
    CreateConditionSpec build_create_spec(const Condition &form, const ConditionChanges &changes, const std::string &folder_uid,
                                          const std::string &default_layer_uid, bool metric);

    // N-M1-13 / NC-4: a condition name counts at most 255 UTF-16 code units (Name VARCHAR(255): database_creator.py:166), the " @T value" suffix
    // included (the spec's name is the whole name).
    inline constexpr std::size_t kMaxConditionNameUnits = 255;
    inline constexpr const char *kConditionNameTooLongMessage = "Condition name must not be longer than 255 characters.";

    // The store-level guards (the form refuses the same values with the Windows texts before it gets here). nullopt = the spec may be written.
    //   - a name longer than 255 UTF-16 units (ost_domain::utf16_length): "Validation Error" / kConditionNameTooLongMessage;
    //   - a blank name (text.strip() empty, Python str.strip: Unicode white space), a name or notes that are not valid UTF-8, a NUL in the name or
    //     the notes (a C string cut), a NaN or infinite number
    //     in any real field (height, width, depth, thickness, rise, run, display_size, spacing, drop_value, round_up, grid_size1, grid_size2,
    //     gap, connect_tolerance): condition_create_failed_issue() (the generic text, decision D10).
    // A huge FINITE number is accepted; display_size is not range-checked here (the form checks 10..500).
    std::optional<FormIssue> validate_create_spec(const CreateConditionSpec &spec);

    // condition_action_handler.py:650-653: "Save Failed" (the dialog's title for a refused save) with `detail` when the store gave one, else
    // "Failed to create condition.".
    FormIssue condition_create_failed_issue(const std::string &detail = std::string());

    // condition_action_handler.py:764-773: the condition was created, but the conditions list could not be read again (the form shows it once it has
    // closed): the title and the text of the Windows warning, verbatim.
    inline constexpr const char *kConditionCreatedRefreshFailedTitle = "Refresh Error";
    inline constexpr const char *kConditionCreatedRefreshFailedText =
        "The condition was created, but the conditions list could not be refreshed. Reopen the database to see the new condition.";

    // The discard alert of the create form (iOS-only texts, plan 4.14; register NC-3): Windows forces the dirty flag of the New Condition dialog and
    // asks "Save changes before closing?" even for an untouched form (condition_action_handler.py:758); the iOS form closes an untouched form
    // silently and asks for an EDITED one whether to discard it ("Keep Editing" / "Discard"). Pinned by test_condition_folder_ui.cpp.
    inline constexpr const char *kDiscardNewConditionTitle = "Unsaved Changes";
    inline constexpr const char *kDiscardNewConditionMessage = "Discard this new condition?";
    inline constexpr const char *kKeepEditingTitle = "Keep Editing";

    // conditions_sidebar.py:1499-1513 _resolve_folder_context_uid over the pre-order nodes of build_condition_tree: the uid of the nearest FOLDER
    // node at or above nodes[index] (the node itself when it is a folder; otherwise the closest preceding node with a smaller depth, repeated, until a
    // folder is found); "" (the top level) when there is none or when `index` is out of range.
    std::string folder_context_uid(const std::vector<ConditionTreeNode> &nodes, std::size_t index);

}
