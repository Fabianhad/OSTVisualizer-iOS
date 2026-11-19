// Changing the style of a condition in the Edit Condition dialog (ConditionStyleChange.hpp): COND-STYLE-01 (a style change keeps
// notes, pattern, the shared options and every unsaved text; only the controls of the new style get defaults), COND-STYLE-02
// (a style change persists the type default spacing unless the user edited the field; the shape is reset only when entering
// Count / Attachment from another style; a metric bid persists the METRIC type defaults, an imperial bid the Windows table),
// COND-M08 (an unsupported stored type shows Linear), COND-METRIC-01 (the defaults of a metric bid are millimetres),
// COL-PICK-01 (a save never writes ColorLine).
//
// Windows oracle: ost_visualizer/presentation/dialogs/edit_condition_dialog.py _on_style_changed / _populate_defaults_for_type /
// _validate_and_build_dto and tests/presentation/dialogs/test_edit_condition_dialog.py. The persisted values after a style
// change (spacing 6.0 for Area, thickness 0.0 for Attachment, Count(Circle) -> Attachment keeping the shape) were verified by
// driving the Windows dialog headless (see the COND-STYLE-02 record of PARITY_MAP.md).
// The metric half of COND-STYLE-02 is a deliberate decision (2026-11-19) that deviates from Windows, which persists the imperial
// table in a metric bid too.
#include "TestHarness.hpp"

#include "Domain/ConditionChanges.hpp"
#include "Domain/ConditionChoices.hpp"
#include "Domain/ConditionStyleChange.hpp"
#include "Domain/DimensionFormat.hpp"

#include <algorithm>
#include <string>

using namespace ost_domain;

namespace
{
    // tests/presentation/dialogs/test_edit_condition_dialog.py::_notes_dialog: Condition(uid c1, name 'Footing', ref_no 7,
    // is_top True, uom1..3 -1, notes)
    Condition notes_condition(ConditionType type = ConditionType::Count, const std::string &notes = "")
    {
        Condition c;
        c.uid = "1";
        c.name = "Footing";
        c.condition_type = type;
        c.ref_no = 7;
        c.is_top = true;
        c.uom1 = c.uom2 = c.uom3 = -1;
        c.notes = notes;
        return c;
    }

    bool resets(const StyleChange &change, StyleField field)
    {
        return std::find(change.reset_fields.begin(), change.reset_fields.end(), field) != change.reset_fields.end();
    }

    ConditionChanges save_changes(const Condition &stored, const Condition &form, bool metric = false)
    {
        return collect_condition_changes(stored, condition_to_save(stored, form, metric));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-STYLE-01
// ---------------------------------------------------------------------------------------------------------------------

// Port of test_shared_fields_carry_over_and_type_specific_fields_get_defaults
TEST(CondStyle01_StyleChange, SharedFieldsCarryOverAndTypeSpecificFieldsGetDefaults)
{
    Condition form = notes_condition(ConditionType::Count, "keep me");
    form.name = "Pier";
    form.pattern = Pattern::Solid;
    form.spacing = 9.0;
    form.display_name = true;
    form.width = 50.0;
    form.thickness = 77.0;

    const StyleChange area = apply_style_change(form, ConditionType::Area, false);
    // Shared by count and area: kept.
    CHECK(area.condition.condition_type == ConditionType::Area);
    CHECK_STR_EQ(area.condition.name, "Pier");
    CHECK(area.condition.pattern == Pattern::Solid);
    CHECK_EQ(area.condition.spacing, 9.0);
    CHECK(area.condition.display_name);
    CHECK_STR_EQ(area.condition.notes, "keep me");
    CHECK_EQ(area.condition.ref_no, 7);
    // Area dimensions and area-only options get defaults.
    CHECK_NE(area.condition.thickness, 77.0);
    CHECK_EQ(area.condition.thickness, 4.0);
    CHECK_FALSE(area.condition.display_dimension);
    CHECK_FALSE(area.condition.grid);
    CHECK_EQ(area.condition.grid_size1, 0.0);
    CHECK_EQ(area.condition.grid_size2, 0.0);
    CHECK_EQ(area.condition.gap, 0.0);
    CHECK(area.condition.display_grid_while_drawing); // TYPE_DEFAULTS Area display_grid_while_drawing
    for (StyleField field : {StyleField::Thickness, StyleField::Rise, StyleField::Run, StyleField::TileX, StyleField::TileY,
                             StyleField::Gap, StyleField::RoundTo})
        CHECK(resets(area, field));

    // Back to count: its dimensions get defaults again, not the old 50.
    const StyleChange count = apply_style_change(area.condition, ConditionType::Count, false);
    CHECK(count.condition.condition_type == ConditionType::Count);
    CHECK_NE(count.condition.width, 50.0);
    CHECK_EQ(count.condition.width, 12.0);
    CHECK_EQ(count.condition.display_size, 100.0);
    CHECK_STR_EQ(count.condition.notes, "keep me");
    CHECK(count.condition.display_name); // carried from Area (both are display-name styles)
    for (StyleField field : {StyleField::Height, StyleField::Width, StyleField::Depth, StyleField::DisplaySize})
        CHECK(resets(count, field));
}

// Nothing typed into a field that no style change resets is lost: the name, Cond. No., elevation, notes and spacing texts are
// never in the reset list (iOS rebuilt every text from the stored condition, discarding the unsaved edits).
TEST(CondStyle01_StyleChange, NoStyleChangeResetsTheNameRefNoElevationNotesOrSpacingText)
{
    const ConditionType types[] = {ConditionType::Linear, ConditionType::Area, ConditionType::Count, ConditionType::Attachment};
    for (ConditionType from : types)
    {
        for (ConditionType to : types)
        {
            SCOPED_CONTEXT("from " + std::to_string(static_cast<int>(from)) + " to " + std::to_string(static_cast<int>(to)));
            const StyleChange change = apply_style_change(notes_condition(from, "n"), to, false);
            // the StyleField enum has no entry for name / ref no / elevation / notes / spacing: nothing of them can be reset
            CHECK_STR_EQ(change.condition.name, "Footing");
            CHECK_STR_EQ(change.condition.notes, "n");
            CHECK_EQ(change.condition.ref_no, 7);
            CHECK(change.condition.pattern == Pattern::Solid);
            CHECK_EQ(change.condition.spacing, 0.0); // the model's spacing is the form's: not replaced by a default
        }
    }
}

// Port of test_rounding_carries_over_between_linear_and_area_only
TEST(CondStyle01_StyleChange, RoundingCarriesOverBetweenLinearAndAreaOnly)
{
    Condition form = notes_condition(ConditionType::Linear);
    form.round_quantity = true;
    form.drop_run = true;
    form.round_up = 6.0;

    const StyleChange area = apply_style_change(form, ConditionType::Area, false);
    CHECK(area.condition.round_quantity);
    CHECK_EQ(area.condition.round_up, 6.0);
    CHECK_FALSE(resets(area, StyleField::RoundTo)); // the Round to text is carried as typed

    const StyleChange count = apply_style_change(area.condition, ConditionType::Count, false);
    const StyleChange linear = apply_style_change(count.condition, ConditionType::Linear, false);
    CHECK_FALSE(linear.condition.round_quantity);
    CHECK_FALSE(linear.condition.drop_run);
    CHECK_EQ(linear.condition.round_up, 0.0);
    CHECK(resets(linear, StyleField::RoundTo));
    CHECK(resets(linear, StyleField::AddLength));
    CHECK_FALSE(linear.condition.trim);
    CHECK_FALSE(linear.condition.is_curved_segment);
}

// display_name is carried when both styles are in {Count, Attachment, Area}, never from or to Linear
TEST(CondStyle01_StyleChange, DisplayNameIsCarriedAmongCountAttachmentAndArea)
{
    Condition form = notes_condition(ConditionType::Count);
    form.display_name = true;
    CHECK(apply_style_change(form, ConditionType::Attachment, false).condition.display_name);
    CHECK(apply_style_change(form, ConditionType::Area, false).condition.display_name);
    const StyleChange linear = apply_style_change(form, ConditionType::Linear, false);
    // Linear has no Display Name control: the value is not touched by the change ...
    CHECK(linear.condition.display_name);
    // ... and does not come back when leaving Linear (Windows rebuilds the advanced tab and carries nothing out of Linear)
    CHECK_FALSE(apply_style_change(linear.condition, ConditionType::Count, false).condition.display_name);
    CHECK_FALSE(apply_style_change(linear.condition, ConditionType::Area, false).condition.display_name);
}

// Port of test_notes_survive_count_to_linear_and_back: the notes survive, and the save carries no notes / type change
TEST(CondStyle01_StyleChange, NotesSurviveCountToLinearAndBack)
{
    const Condition stored = notes_condition(ConditionType::Count, "pad [crane mast=10]");
    const StyleChange linear = apply_style_change(stored, ConditionType::Linear, false);
    CHECK_STR_EQ(linear.condition.notes, "pad [crane mast=10]");
    const StyleChange count = apply_style_change(linear.condition, ConditionType::Count, false);
    CHECK_STR_EQ(count.condition.notes, "pad [crane mast=10]");
    const ConditionChanges changes = save_changes(stored, count.condition);
    CHECK_FALSE(changes.has(ConditionField::Notes));
    CHECK_FALSE(changes.has(ConditionField::ConditionType));
}

// The test plan: "Count with notes 'pad [crane mast=10]' -> Linear -> Count keeps the notes and produces no notes/condition_type
// change" and the change set of a plain Count -> Linear save names the type
TEST(CondStyle01_StyleChange, ASaveAfterAStyleChangeNamesTheTypeAndKeepsTheNotes)
{
    const Condition stored = notes_condition(ConditionType::Count, "plain notes");
    const StyleChange linear = apply_style_change(stored, ConditionType::Linear, false);
    const ConditionChanges changes = save_changes(stored, linear.condition);
    CHECK(changes.has(ConditionField::ConditionType));
    CHECK(changes.values.condition_type == ConditionType::Linear);
    CHECK_FALSE(changes.has(ConditionField::Notes));
    CHECK_FALSE(changes.has(ConditionField::Pattern));
    CHECK_FALSE(changes.has(ConditionField::DisplayName));
    CHECK_FALSE(changes.has(ConditionField::ColorFill));
}

// A style change is not saved with fields the new style has no control for: Count -> Linear -> Count is no change of the
// thickness (a Count has none) even though the Linear style set one
TEST(CondStyle01_StyleChange, FieldsWithoutAControlKeepTheirStoredValue)
{
    Condition stored = notes_condition(ConditionType::Count);
    stored.thickness = 9.0;
    stored.height = 3.0;
    stored.rise = 2.0;
    stored.round_up = 5.0;
    stored.grid_size1 = 7.0;
    const StyleChange linear = apply_style_change(stored, ConditionType::Linear, false);
    const StyleChange back = apply_style_change(linear.condition, ConditionType::Count, false);
    const Condition saved = condition_to_save(stored, back.condition, false);
    CHECK_EQ(saved.thickness, 9.0);
    CHECK_EQ(saved.rise, 2.0);
    CHECK_EQ(saved.round_up, 5.0);
    CHECK_EQ(saved.grid_size1, 7.0);
    CHECK_FALSE(save_changes(stored, back.condition).has(ConditionField::Thickness));
    // a Count's Height is a control of the style: the reset value 0 is a change of the stored 3.0
    CHECK_EQ(saved.height, 0.0);
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-STYLE-02
// ---------------------------------------------------------------------------------------------------------------------

// The test plan: Linear->Area untouched spacing -> 6.0; Linear->Count -> 4.0; Area->Linear -> 4.0; any->Attachment -> 4.0 and
// thickness 0.0 (verified headless against the Windows dialog)
TEST(CondStyle02_Spacing, AStyleChangePersistsTheTypeDefaultSpacingUnlessEdited)
{
    struct Row
    {
        ConditionType from;
        ConditionType to;
        double spacing;
    };
    const Row rows[] = {
        {ConditionType::Linear, ConditionType::Area, 6.0},      {ConditionType::Count, ConditionType::Area, 6.0},
        {ConditionType::Linear, ConditionType::Count, 4.0},     {ConditionType::Area, ConditionType::Linear, 4.0},
        {ConditionType::Linear, ConditionType::Attachment, 4.0}, {ConditionType::Area, ConditionType::Attachment, 4.0},
        {ConditionType::Count, ConditionType::Attachment, 4.0}, {ConditionType::Attachment, ConditionType::Count, 4.0},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("from " + std::to_string(static_cast<int>(row.from)) + " to " + std::to_string(static_cast<int>(row.to)));
        Condition stored = notes_condition(row.from);
        stored.spacing = 2.5; // any stored value: the field is untouched, so the type default is persisted
        const StyleChange change = apply_style_change(stored, row.to, false);
        const Condition saved = condition_to_save(stored, change.condition, false);
        CHECK_EQ(saved.spacing, row.spacing);
        CHECK(save_changes(stored, change.condition).has(ConditionField::Spacing));
    }
}

TEST(CondStyle02_Spacing, ASpacingEditedBeforeTheChangePersists)
{
    Condition stored = notes_condition(ConditionType::Linear);
    stored.spacing = 4.0;
    Condition form = stored;
    form.spacing = 9.0; // the user typed 9 into Spacing
    const StyleChange change = apply_style_change(form, ConditionType::Area, false);
    CHECK_EQ(change.condition.spacing, 9.0);
    CHECK_EQ(condition_to_save(stored, change.condition, false).spacing, 9.0);
    // without a style change the edited value is the change, as before
    CHECK_EQ(condition_to_save(stored, form, false).spacing, 9.0);
}

// Without a style change an untouched spacing is no change at all (the type default applies only to a style change)
TEST(CondStyle02_Spacing, WithoutAStyleChangeTheStoredSpacingStays)
{
    Condition stored = notes_condition(ConditionType::Area);
    stored.spacing = 2.5;
    const Condition saved = condition_to_save(stored, stored, false);
    CHECK_EQ(saved.spacing, 2.5);
    CHECK(collect_condition_changes(stored, saved).empty());
}

// The values a style change persists, per measurement system of the bid.
//   IMPERIAL (= Windows, edit_condition_dialog.py:1445-1448): td["spacing"] / td["thickness"] / td["backout"], the imperial
//   TYPE_DEFAULTS columns, in inches.
//   METRIC (COND-STYLE-02, decided 2026-11-19: a deliberate DEVIATION from the literal Windows behaviour): the metric columns
//   spacing_metric / thickness_metric as inches (mm / 25.4), the values _populate_defaults_for_type (:1241-1252) shows after the
//   style change. Windows persists the imperial values even in a metric bid (possible Windows defect, see PORTING_BLOCKERS.md).
//   The backout does not depend on the unit.
TEST(CondStyle02_Spacing, TheTypeChangeDefaultsFollowTheMeasurementSystemOfTheBid)
{
    // imperial: the Windows table values
    CHECK_EQ(type_change_defaults(ConditionType::Linear, false).spacing, 4.0);
    CHECK_EQ(type_change_defaults(ConditionType::Area, false).spacing, 6.0);
    CHECK_EQ(type_change_defaults(ConditionType::Count, false).spacing, 4.0);
    CHECK_EQ(type_change_defaults(ConditionType::Attachment, false).spacing, 4.0);
    CHECK_EQ(type_change_defaults(ConditionType::Linear, false).thickness, 4.0);
    CHECK_EQ(type_change_defaults(ConditionType::Area, false).thickness, 4.0);
    CHECK_EQ(type_change_defaults(ConditionType::Count, false).thickness, 4.0);
    CHECK_EQ(type_change_defaults(ConditionType::Attachment, false).thickness, 0.0);
    CHECK_FALSE(type_change_defaults(ConditionType::Linear, false).backout);
    CHECK_FALSE(type_change_defaults(ConditionType::Area, false).backout);
    CHECK(type_change_defaults(ConditionType::Count, false).backout);
    CHECK(type_change_defaults(ConditionType::Attachment, false).backout);

    // metric: spacing_metric Linear 100 / Area 150 / Count 100 / Attachment 100 mm, thickness_metric 100 / 100 / 100 / 0 mm
    // (the Windows mm_to_inches(mm) is mm / 25.4, values computed with python -B)
    CHECK_EQ(type_change_defaults(ConditionType::Linear, true).spacing, 3.937007874015748);
    CHECK_EQ(type_change_defaults(ConditionType::Area, true).spacing, 5.905511811023622);
    CHECK_EQ(type_change_defaults(ConditionType::Count, true).spacing, 3.937007874015748);
    CHECK_EQ(type_change_defaults(ConditionType::Attachment, true).spacing, 3.937007874015748);
    CHECK_EQ(type_change_defaults(ConditionType::Linear, true).thickness, 3.937007874015748);
    CHECK_EQ(type_change_defaults(ConditionType::Area, true).thickness, 3.937007874015748);
    CHECK_EQ(type_change_defaults(ConditionType::Count, true).thickness, 3.937007874015748);
    CHECK_EQ(type_change_defaults(ConditionType::Attachment, true).thickness, 0.0);
    CHECK_FALSE(type_change_defaults(ConditionType::Linear, true).backout);
    CHECK_FALSE(type_change_defaults(ConditionType::Area, true).backout);
    CHECK(type_change_defaults(ConditionType::Count, true).backout);
    CHECK(type_change_defaults(ConditionType::Attachment, true).backout);

    // a metric bid persists the metric table values: Linear -> Area writes 150 mm, not the imperial 6.0 in
    Condition stored = notes_condition(ConditionType::Linear);
    stored.spacing = 100.0 / 25.4;
    const StyleChange change = apply_style_change(stored, ConditionType::Area, true);
    const Condition saved = condition_to_save(stored, change.condition, true);
    CHECK_EQ(saved.spacing, 5.905511811023622);
    CHECK_STR_EQ(inches_to_display(saved.spacing, true), "150");
    // the imperial bid keeps the Windows value for the same change
    CHECK_EQ(condition_to_save(stored, apply_style_change(stored, ConditionType::Area, false).condition, false).spacing, 6.0);
}

namespace
{
    constexpr ConditionType kStyles[] = {ConditionType::Linear, ConditionType::Area, ConditionType::Count, ConditionType::Attachment};

    bool count_like_style(ConditionType type)
    {
        return type == ConditionType::Count || type == ConditionType::Attachment;
    }

    // Spacing 2.5 and thickness 6.0 are never a table value, so every style change is a real change of both.
    Condition stored_of_style(ConditionType type)
    {
        Condition stored = notes_condition(type);
        stored.spacing = 2.5;
        stored.thickness = 6.0;
        stored.backout = !count_like_style(type);
        return stored;
    }
}

// Metric bid, every pair of styles: Save persists style_dimension_defaults(to, true) for spacing and thickness, i.e. what the
// dialog shows after the style change (thickness) and the TYPE_DEFAULTS metric columns; the backout is the table's.
TEST(CondStyle02_Spacing, AMetricBidPersistsTheMetricTypeDefaultsForEveryStyleChange)
{
    const double spacing_mm[] = {100.0, 150.0, 100.0, 100.0}; // Linear, Area, Count, Attachment (spacing_metric)
    const double thickness_mm[] = {100.0, 100.0, 100.0, 0.0}; // thickness_metric
    const char *spacing_text[] = {"100", "150", "100", "100"};
    const char *thickness_text[] = {"100", "100", "100", ""}; // 0 is shown as an empty field
    for (int from = 0; from < 4; ++from)
    {
        for (int to = 0; to < 4; ++to)
        {
            if (from == to)
                continue;
            SCOPED_CONTEXT("from " + std::to_string(from) + " to " + std::to_string(to));
            Condition stored = stored_of_style(kStyles[from]);
            stored.backout = !count_like_style(kStyles[to]); // the opposite of the table value of the new style: a real change
            const StyleChange change = apply_style_change(stored, kStyles[to], true);
            const Condition saved = condition_to_save(stored, change.condition, true);
            const StyleDimensionDefaults shown = style_dimension_defaults(kStyles[to], true);

            CHECK_EQ(saved.spacing, shown.spacing);
            CHECK_EQ(saved.spacing, spacing_mm[to] / 25.4);
            CHECK_STR_EQ(inches_to_display(saved.spacing, true), spacing_text[to]);
            CHECK_EQ(saved.thickness, shown.thickness);
            CHECK_EQ(saved.thickness, thickness_mm[to] / 25.4);
            CHECK_STR_EQ(inches_to_display(saved.thickness, true), thickness_text[to]);
            // the Thickness field of a Linear / Area form shows exactly what Save writes
            if (!count_like_style(kStyles[to]))
                CHECK_EQ(change.condition.thickness, saved.thickness);
            // backout does not depend on the unit
            CHECK_EQ(saved.backout, count_like_style(kStyles[to]));
            CHECK(saved.condition_type == kStyles[to]);

            const ConditionChanges changes = save_changes(stored, change.condition, true);
            CHECK(changes.has(ConditionField::Spacing));
            CHECK(changes.has(ConditionField::Thickness));
            CHECK(changes.has(ConditionField::Backout));
            CHECK_EQ(changes.values.backout, count_like_style(kStyles[to]));
            CHECK_EQ(changes.values.spacing, spacing_mm[to] / 25.4);
            CHECK_EQ(changes.values.thickness, thickness_mm[to] / 25.4);
        }
    }
}

// Imperial bid, every pair of styles: exactly the Windows TYPE_DEFAULTS table (edit_condition_dialog.py:90-143 imperial columns)
TEST(CondStyle02_Spacing, AnImperialBidKeepsTheWindowsTableForEveryStyleChange)
{
    const double spacing_in[] = {4.0, 6.0, 4.0, 4.0}; // Linear, Area, Count, Attachment
    const double thickness_in[] = {4.0, 4.0, 4.0, 0.0};
    for (int from = 0; from < 4; ++from)
    {
        for (int to = 0; to < 4; ++to)
        {
            if (from == to)
                continue;
            SCOPED_CONTEXT("from " + std::to_string(from) + " to " + std::to_string(to));
            const Condition stored = stored_of_style(kStyles[from]);
            const StyleChange change = apply_style_change(stored, kStyles[to], false);
            const Condition saved = condition_to_save(stored, change.condition, false);
            CHECK_EQ(saved.spacing, spacing_in[to]);
            CHECK_EQ(saved.thickness, thickness_in[to]);
            CHECK_EQ(saved.backout, count_like_style(kStyles[to]));
            const ConditionChanges changes = save_changes(stored, change.condition, false);
            CHECK_EQ(changes.values.spacing, spacing_in[to]);
            CHECK_EQ(changes.values.thickness, thickness_in[to]);
        }
    }
}

// The existing rule holds in a metric bid: a spacing or thickness the user typed (it differs from the stored value) wins over
// the type default; what was not typed is the metric type default.
TEST(CondStyle02_Spacing, AMetricBidKeepsWhatTheUserTypedAfterAStyleChange)
{
    const Condition stored = stored_of_style(ConditionType::Linear);

    // spacing typed before the change (the style change carries the text; Windows sets it only when the dialog is populated)
    Condition form = stored;
    form.spacing = 7.5;
    const StyleChange change = apply_style_change(form, ConditionType::Area, true);
    CHECK_EQ(change.condition.spacing, 7.5);
    const Condition untouched_thickness = condition_to_save(stored, change.condition, true);
    CHECK_EQ(untouched_thickness.spacing, 7.5);
    CHECK_EQ(untouched_thickness.thickness, 3.937007874015748);

    // thickness typed after the change, spacing untouched
    Condition typed = apply_style_change(stored, ConditionType::Area, true).condition;
    typed.thickness = 5.5;
    const Condition saved = condition_to_save(stored, typed, true);
    CHECK_EQ(saved.thickness, 5.5);
    CHECK_EQ(saved.spacing, 5.905511811023622);

    // a Count has no thickness field: the table value is written whatever the form holds
    Condition count = apply_style_change(stored, ConditionType::Count, true).condition;
    count.thickness = 5.5;
    CHECK_EQ(condition_to_save(stored, count, true).thickness, 3.937007874015748);
}

// The thickness of a Linear / Area style is a type default only when its field still equals the stored thickness. A condition
// created in a metric bid usually stores exactly the metric default (100 mm), so the field after the style change (100 mm)
// equals the stored value and the table value is written: it must be the metric one, 100 mm = no change at all, and not the
// imperial 4.0 in = 101.6 mm.
TEST(CondStyle02_Spacing, AMetricBidWritesTheMetricThicknessWhenTheFieldEqualsTheStoredOne)
{
    const double metric_thickness = 100.0 / 25.4;
    for (int from = 0; from < 4; ++from)
    {
        for (int to = 0; to < 2; ++to) // Linear, Area: the styles with a Thickness field
        {
            if (from == to)
                continue;
            SCOPED_CONTEXT("from " + std::to_string(from) + " to " + std::to_string(to));
            Condition stored = stored_of_style(kStyles[from]);
            stored.thickness = metric_thickness;
            stored.spacing = 2.5;
            stored.backout = !count_like_style(kStyles[to]); // the thickness is the only thing the table could still change
            const StyleChange change = apply_style_change(stored, kStyles[to], true);
            CHECK_EQ(change.condition.thickness, metric_thickness);
            const Condition saved = condition_to_save(stored, change.condition, true);
            CHECK_EQ(saved.thickness, metric_thickness);
            CHECK_STR_EQ(inches_to_display(saved.thickness, true), "100");
            CHECK_FALSE(save_changes(stored, change.condition, true).has(ConditionField::Thickness));
            // the same change in an imperial bid writes the imperial table value (Windows)
            const Condition imperial = condition_to_save(stored, apply_style_change(stored, kStyles[to], false).condition, false);
            CHECK_EQ(imperial.thickness, 4.0);
        }
    }
}

// Without a style change nothing is defaulted, in a metric bid either: an untouched form is no change and a rename writes the
// name only (the metric type defaults apply to a style change only).
TEST(CondStyle02_Spacing, AMetricSaveWithoutAStyleChangeWritesNoDefaults)
{
    for (ConditionType type : kStyles)
    {
        SCOPED_CONTEXT("style " + std::to_string(static_cast<int>(type)));
        const Condition stored = stored_of_style(type);
        CHECK(save_changes(stored, stored, true).empty());
        Condition form = stored;
        form.name = "Renamed";
        const Condition saved = condition_to_save(stored, form, true);
        CHECK_EQ(saved.spacing, 2.5);
        CHECK_EQ(saved.thickness, 6.0);
        CHECK_EQ(saved.backout, stored.backout);
        const ConditionChanges changes = save_changes(stored, form, true);
        CHECK(changes.has(ConditionField::Name));
        CHECK_EQ(changes.fields.count(), 1u);
    }
}

// Attachment: thickness 0.0 and backout on; Count: thickness 4.0, backout on; Linear / Area: backout off
TEST(CondStyle02_Spacing, ATypeChangeWritesThicknessAndBackoutOfTheTable)
{
    Condition stored = notes_condition(ConditionType::Linear);
    stored.thickness = 6.0;
    stored.backout = false;
    const Condition attachment = condition_to_save(stored, apply_style_change(stored, ConditionType::Attachment, false).condition, false);
    CHECK_EQ(attachment.thickness, 0.0);
    CHECK(attachment.backout);
    const Condition count = condition_to_save(stored, apply_style_change(stored, ConditionType::Count, false).condition, false);
    CHECK_EQ(count.thickness, 4.0);
    CHECK(count.backout);

    Condition count_stored = notes_condition(ConditionType::Count);
    count_stored.backout = true;
    const Condition linear = condition_to_save(count_stored, apply_style_change(count_stored, ConditionType::Linear, false).condition, false);
    CHECK_FALSE(linear.backout);
    CHECK_EQ(linear.thickness, 4.0);
}

// Area thickness differs from a typed 77: the reset text is the default, and a Linear / Area thickness the user typed AFTER the
// change wins over the table value (the field wins when it differs from the stored value)
TEST(CondStyle02_Spacing, ATypedThicknessAfterTheChangeWins)
{
    Condition stored = notes_condition(ConditionType::Count);
    const StyleChange area = apply_style_change(stored, ConditionType::Area, false);
    CHECK_EQ(area.condition.thickness, 4.0);
    Condition typed = area.condition;
    typed.thickness = 5.5; // typed into the Thickness field of the Area style
    CHECK_EQ(condition_to_save(stored, typed, false).thickness, 5.5);
    CHECK_EQ(condition_to_save(stored, area.condition, false).thickness, 4.0);
}

// Shape: Count(shape Circle) -> Attachment keeps shape 1; Linear -> Count sets Square (0); Count -> Linear -> Count resets it
TEST(CondStyle02_Shape, TheShapeIsResetOnlyWhenEnteringCountFromAnotherStyle)
{
    Condition circle = notes_condition(ConditionType::Count);
    circle.shape = ShapeId::Circle;
    CHECK(apply_style_change(circle, ConditionType::Attachment, false).condition.shape == ShapeId::Circle);
    CHECK(apply_style_change(apply_style_change(circle, ConditionType::Attachment, false).condition, ConditionType::Count, false)
              .condition.shape == ShapeId::Circle);

    Condition linear = notes_condition(ConditionType::Linear);
    linear.shape = ShapeId::Hexagon;
    CHECK(apply_style_change(linear, ConditionType::Count, false).condition.shape == ShapeId::Square);
    CHECK(apply_style_change(linear, ConditionType::Attachment, false).condition.shape == ShapeId::Square);

    // Count -> Linear -> Count: the shape of the first Count is gone
    const StyleChange via_linear = apply_style_change(circle, ConditionType::Linear, false);
    CHECK(apply_style_change(via_linear.condition, ConditionType::Count, false).condition.shape == ShapeId::Square);
    // the shape of a style without a shape control is left alone (Windows only writes the shape of a Count / Attachment)
    CHECK(apply_style_change(circle, ConditionType::Area, false).condition.shape == ShapeId::Circle);
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-METRIC-01 (style defaults) and COND-M08
// ---------------------------------------------------------------------------------------------------------------------

// a new style in a metric bid starts from the millimetre defaults (QTY-05: uom1 13 / 14)
TEST(CondMetric01_StyleDefaults, AMetricBidGetsTheMetricDefaults)
{
    const Condition linear_form = notes_condition(ConditionType::Count);
    const StyleChange linear = apply_style_change(linear_form, ConditionType::Linear, true);
    CHECK_EQ(linear.condition.thickness, 100.0 / 25.4);
    CHECK_EQ(linear.condition.uom1, 13);
    CHECK_EQ(linear.condition.calc_type1, 1);
    const StyleChange area = apply_style_change(linear_form, ConditionType::Area, true);
    CHECK_EQ(area.condition.thickness, 100.0 / 25.4);
    CHECK_EQ(area.condition.uom1, 14);
    CHECK_EQ(area.condition.calc_type1, 11);
    const StyleChange count = apply_style_change(notes_condition(ConditionType::Linear), ConditionType::Count, true);
    CHECK_EQ(count.condition.width, 300.0 / 25.4);
    CHECK_EQ(count.condition.uom1, 0);
    CHECK_EQ(count.condition.calc_type1, 23);
    CHECK_EQ(count.condition.display_size, 100.0);
    // imperial
    CHECK_EQ(apply_style_change(linear_form, ConditionType::Linear, false).condition.uom1, 2);
    CHECK_EQ(apply_style_change(linear_form, ConditionType::Area, false).condition.uom1, 5);
    CHECK_EQ(apply_style_change(notes_condition(ConditionType::Linear), ConditionType::Count, false).condition.width, 12.0);
}

// the quantity slots of the new style: the first one is the style default, the others are empty (calc 0, uom -1)
TEST(CondStyle01_StyleChange, TheQuantitySlotsRestartFromTheStyleDefault)
{
    Condition form = notes_condition(ConditionType::Linear);
    form.calc_type2 = 4;
    form.uom2 = 2;
    form.calc_type3 = 5;
    form.uom3 = 5;
    const StyleChange change = apply_style_change(form, ConditionType::Area, false);
    CHECK_EQ(change.condition.calc_type1, 11);
    CHECK_EQ(change.condition.uom1, 5);
    CHECK_EQ(change.condition.calc_type2, 0);
    CHECK_EQ(change.condition.uom2, -1);
    CHECK_EQ(change.condition.calc_type3, 0);
    CHECK_EQ(change.condition.uom3, -1);
}

// _validate_and_build_dto: `uom = -1 if calc == 0 else uom_combo.currentData()`, compared with the stored unit. A stored slot of
// calc 0 with another unit (computed by driving the Windows dialog headless: a rename of such a condition sends uom2 / uom3 = -1)
// is written as -1 by any Save; a slot with a quantity keeps its unit.
TEST(CondUpd02_ConditionToSave, ASlotWithoutAQuantityHasTheUnitMinusOne)
{
    Condition stored = notes_condition(ConditionType::Linear);
    stored.calc_type1 = 1;
    stored.uom1 = 2;
    stored.calc_type2 = 0;
    stored.uom2 = 0;
    stored.calc_type3 = 0;
    stored.uom3 = 5;
    Condition form = stored;
    form.name = "Renamed";
    const Condition saved = condition_to_save(stored, form, false);
    CHECK_EQ(saved.uom1, 2);
    CHECK_EQ(saved.uom2, -1);
    CHECK_EQ(saved.uom3, -1);
    const ConditionChanges changes = collect_condition_changes(stored, saved);
    CHECK(changes.has(ConditionField::Name));
    CHECK(changes.has(ConditionField::Uom2));
    CHECK(changes.has(ConditionField::Uom3));
    CHECK_FALSE(changes.has(ConditionField::Uom1));
    CHECK_FALSE(changes.has(ConditionField::CalcType2));
    CHECK_EQ(changes.fields.count(), 3u);
}

// COND-M08, decision a: the editor shows Linear for a type the database holds outside 0..3; a Save then rewrites the type
TEST(CondM08_UnsupportedType, TheEditorShowsLinear)
{
    CHECK(editor_condition_type(ConditionType::Linear) == ConditionType::Linear);
    CHECK(editor_condition_type(ConditionType::Area) == ConditionType::Area);
    CHECK(editor_condition_type(ConditionType::Count) == ConditionType::Count);
    CHECK(editor_condition_type(ConditionType::Attachment) == ConditionType::Attachment);
    CHECK(editor_condition_type(static_cast<ConditionType>(4)) == ConditionType::Linear);
    CHECK(editor_condition_type(static_cast<ConditionType>(-1)) == ConditionType::Linear);
    CHECK(editor_condition_type(static_cast<ConditionType>(99)) == ConditionType::Linear);
}

TEST(CondM08_UnsupportedType, ASaveRewritesTheTypeLikeAStyleChange)
{
    Condition stored = notes_condition(ConditionType::Linear);
    stored.condition_type = static_cast<ConditionType>(4);
    stored.spacing = 2.5;
    Condition form = stored;
    form.condition_type = editor_condition_type(stored.condition_type);
    form.name = "Renamed";
    const ConditionChanges changes = save_changes(stored, form);
    CHECK(changes.has(ConditionField::ConditionType));
    CHECK(changes.values.condition_type == ConditionType::Linear);
    CHECK(changes.has(ConditionField::Name));
    CHECK_EQ(changes.values.spacing, 4.0); // the type default of the style the type was rewritten to
    CHECK(changes.has(ConditionField::Spacing));
}

// ... and in a metric bid the type default of the rewritten style is the metric one (COND-STYLE-02, decision of 2026-11-19)
TEST(CondM08_UnsupportedType, ASaveRewritesTheTypeWithTheMetricDefaultsInAMetricBid)
{
    Condition stored = notes_condition(ConditionType::Linear);
    stored.condition_type = static_cast<ConditionType>(4);
    stored.spacing = 2.5;
    Condition form = stored;
    form.condition_type = editor_condition_type(stored.condition_type);
    const ConditionChanges changes = save_changes(stored, form, true);
    CHECK(changes.has(ConditionField::ConditionType));
    CHECK(changes.values.condition_type == ConditionType::Linear);
    CHECK_EQ(changes.values.spacing, 3.937007874015748); // 100 mm
    CHECK(changes.has(ConditionField::Spacing));
}

// ---------------------------------------------------------------------------------------------------------------------
// COL-PICK-01 / COND-UPD-02 (the save writes only the style's fields, never ColorLine)
// ---------------------------------------------------------------------------------------------------------------------

TEST(CondUpd02_ConditionToSave, ASaveOfAnUntouchedFormIsEmptyForEveryStyle)
{
    const ConditionType types[] = {ConditionType::Linear, ConditionType::Area, ConditionType::Count, ConditionType::Attachment};
    for (ConditionType type : types)
    {
        SCOPED_CONTEXT("style " + std::to_string(static_cast<int>(type)));
        Condition stored = notes_condition(type, "n");
        stored.color_fill = 0x336699;
        stored.color_line = 0x123456;
        stored.height = 1.5;
        stored.layer_uid = "5";
        stored.cdn_type_uid = "9";
        const ConditionChanges changes = save_changes(stored, stored);
        CHECK(changes.empty());
    }
}

TEST(CondUpd02_ConditionToSave, OnlyTheFieldsOfTheStylesControlsAreTaken)
{
    // an Area form that carries a changed height (an Area has no height control): the stored height stays
    Condition stored = notes_condition(ConditionType::Area);
    stored.height = 1.0;
    stored.thickness = 4.0;
    stored.display_size = 100.0;
    Condition form = stored;
    form.height = 99.0;
    form.display_size = 250.0;
    form.thickness = 6.0; // an Area control
    form.gap = 3.0;       // an Area control
    const ConditionChanges changes = save_changes(stored, form);
    CHECK_FALSE(changes.has(ConditionField::Height));
    CHECK_FALSE(changes.has(ConditionField::DisplaySize));
    CHECK(changes.has(ConditionField::Thickness));
    CHECK(changes.has(ConditionField::Gap));
    CHECK_EQ(changes.fields.count(), 2u);
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-LAYER-01 (an empty layer is saved as no layer)
// ---------------------------------------------------------------------------------------------------------------------

// Test plan: a condition with layer_uid '' saved without touching the layer keeps layer NULL (iOS assigned the first offered layer,
// Default, on every save); the picker's "(none)" clears a layer, which the change set carries as an empty uid (NULL in the database).
TEST(CondLayer01_EmptyLayer, ALayerlessConditionStaysLayerless)
{
    Condition stored = notes_condition(ConditionType::Linear);
    stored.layer_uid = "";
    Condition form = stored;
    form.name = "Renamed";
    const ConditionChanges changes = save_changes(stored, form);
    CHECK(changes.has(ConditionField::Name));
    CHECK_FALSE(changes.has(ConditionField::LayerUid));
    CHECK_STR_EQ(condition_to_save(stored, form, false).layer_uid, "");
}

TEST(CondLayer01_EmptyLayer, AClearedLayerIsAChangeToNone)
{
    Condition stored = notes_condition(ConditionType::Linear);
    stored.layer_uid = "5";
    Condition form = stored;
    form.layer_uid = "";
    const ConditionChanges changes = save_changes(stored, form);
    CHECK(changes.has(ConditionField::LayerUid));
    CHECK_STR_EQ(changes.values.layer_uid, "");
    CHECK_EQ(changes.fields.count(), 1u);
}
