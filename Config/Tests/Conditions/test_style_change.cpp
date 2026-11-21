// Changing the style of a condition in the Edit Condition dialog (ConditionStyleChange.hpp): COND-STYLE-01 (a style change keeps
// notes, pattern, the shared options and every unsaved text; only the controls of the new style get defaults), COND-STYLE-02
// (what the field shows is what is saved: on a style change the Spacing and the Linear / Area Thickness are written from their
// fields even when equal to the stored value, only the thickness of a Count / Attachment and the backout come from the table, a
// metric bid's thickness default is metric; the shape is reset only when entering Count / Attachment from another style),
// COND-M08 (a stored type outside 0-3 is kept), COND-METRIC-01 (the defaults of a metric bid are millimetres),
// COL-PICK-01 (a save never writes ColorLine).
//
// Windows oracle: ost_visualizer/presentation/dialogs/edit_condition_dialog.py _on_style_changed / _populate_defaults_for_type /
// _validate_and_build_dto and tests/presentation/dialogs/test_edit_condition_dialog.py (Windows b9902e6: the six COND-STYLE tests
// of EditConditionTypeChangeAndElevationTests, ported as CondStyle02_FieldWins). The persisted thickness after a style change
// (0.0 for Attachment, Count(Circle) -> Attachment keeping the shape) was verified by driving the Windows dialog headless (see the
// COND-STYLE-02 record of PARITY_MAP.md). The old iOS rule "the type default spacing unless the field was edited" (decision of
// 2026-11-16) and the metric deviation of 2026-11-19 are gone: Windows b9902e6 writes the fields and the metric thickness default.
#include "TestHarness.hpp"

#include "Domain/ConditionChanges.hpp"
#include "Domain/ConditionChoices.hpp"
#include "Domain/ConditionFormRules.hpp"
#include "Domain/ConditionStyleChange.hpp"
#include "Domain/DimensionFormat.hpp"

#include <algorithm>
#include <bitset>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

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

    // What the dialog hands to the store (the single call of EditConditionViewController.mm _applyChanges)
    ConditionChanges save_changes(const Condition &stored, const Condition &form, bool metric = false)
    {
        return condition_changes_for_save(stored, form, metric);
    }

    // tests/presentation/dialogs/test_edit_condition_dialog.py EditConditionUnknownTypeTests._dialog: Condition(uid c1, name "Odd",
    // condition_type N, ref_no 7, is_top True, uom1..3 -1, thickness 4.0, height 8.0), everything else at its default
    Condition unknown_condition(int type)
    {
        Condition c = notes_condition(ConditionType::Linear);
        c.name = "Odd";
        c.condition_type = static_cast<ConditionType>(type);
        c.thickness = 4.0;
        c.height = 8.0;
        return c;
    }

    // every control of the other styles away from its default (the "rich" values of the Windows oracle of the verification)
    Condition rich_unknown_condition(int type)
    {
        Condition c = unknown_condition(type);
        c.spacing = 2.5;
        c.pattern = Pattern::Horizontal;
        c.rise = -3.0;
        c.run = -4.0;
        c.display_name = true;
        c.round_up = 0.5;
        c.round_quantity = true;
        c.shape = ShapeId::Rectangle;
        c.width = 5.0;
        return c;
    }

    bool fields_are(const ConditionChanges &changes, std::initializer_list<ConditionField> expected)
    {
        std::bitset<kConditionFieldCount> want;
        for (ConditionField field : expected)
            want.set(static_cast<std::size_t>(field));
        return changes.fields == want;
    }

    // what the dialog does: load the condition into the form, pick the style (apply_style_change on the form), Save
    ConditionChanges pick_from(const Condition &stored, ConditionType new_type, bool metric)
    {
        const StyleChange change = apply_style_change(condition_form_for_edit(stored), new_type, metric);
        return save_changes(stored, change.condition, metric);
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

// Linear->Area untouched spacing keeps the stored value (it was 6.0 under the old rule "the type default unless edited"), as do
// Linear->Count, Area->Linear and any->Attachment (they were 4.0); the thickness is the default of the new style (Attachment 0.0).
// Windows b9902e6: the Spacing text is not reset by a style change and a type change saves the field, not the table.
TEST(CondStyle02_Spacing, AStyleChangeKeepsTheSpacingTheFieldShows)
{
    struct Row
    {
        ConditionType from;
        ConditionType to;
        double thickness;
    };
    const Row rows[] = {
        {ConditionType::Linear, ConditionType::Area, 4.0},       {ConditionType::Count, ConditionType::Area, 4.0},
        {ConditionType::Linear, ConditionType::Count, 4.0},      {ConditionType::Area, ConditionType::Linear, 4.0},
        {ConditionType::Linear, ConditionType::Attachment, 0.0}, {ConditionType::Area, ConditionType::Attachment, 0.0},
        {ConditionType::Count, ConditionType::Attachment, 0.0},  {ConditionType::Attachment, ConditionType::Count, 4.0},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("from " + std::to_string(static_cast<int>(row.from)) + " to " + std::to_string(static_cast<int>(row.to)));
        Condition stored = notes_condition(row.from);
        stored.spacing = 2.5; // any stored value: the field is untouched, so it is what the field shows that is saved
        stored.thickness = 6.0;
        const StyleChange change = apply_style_change(stored, row.to, false);
        const Condition saved = condition_to_save(stored, change.condition, false);
        CHECK_EQ(saved.spacing, 2.5);
        CHECK_EQ(saved.thickness, row.thickness);
        const ConditionChanges changes = save_changes(stored, change.condition);
        CHECK(changes.has(ConditionField::Spacing)); // equal to the stored value, written all the same on a style change
        CHECK_EQ(changes.values.spacing, 2.5);
        CHECK(changes.has(ConditionField::Thickness)); // the thickness is set on every style change too
        CHECK_EQ(changes.values.thickness, row.thickness);
        CHECK(changes.has(ConditionField::Backout));
        CHECK(changes.has(ConditionField::ConditionType));
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

// Without a style change an untouched spacing is no change at all (a field equal to the stored value is not written)
TEST(CondStyle02_Spacing, WithoutAStyleChangeTheStoredSpacingStays)
{
    Condition stored = notes_condition(ConditionType::Area);
    stored.spacing = 2.5;
    const Condition saved = condition_to_save(stored, stored, false);
    CHECK_EQ(saved.spacing, 2.5);
    CHECK(collect_condition_changes(stored, saved).empty());
}

// The values a type change takes from the table, per measurement system of the bid: the thickness (in inches) and the backout.
//   IMPERIAL: td["thickness"] / td["backout"], the imperial TYPE_DEFAULTS columns (edit_condition_dialog.py:1250-1253, :1474-1479).
//   METRIC: _type_default_thickness reads thickness_metric as inches (mm / 25.4), the value _populate_defaults_for_type
//   (:1255-1266) shows after the style change (Windows b9902e6; before it Windows persisted the imperial value in a metric bid).
//   The backout does not depend on the unit. There is no spacing default: a type change saves the Spacing field.
TEST(CondStyle02_Spacing, TheTypeChangeDefaultsFollowTheMeasurementSystemOfTheBid)
{
    // imperial: the Windows table values
    CHECK_EQ(type_change_defaults(ConditionType::Linear, false).thickness, 4.0);
    CHECK_EQ(type_change_defaults(ConditionType::Area, false).thickness, 4.0);
    CHECK_EQ(type_change_defaults(ConditionType::Count, false).thickness, 4.0);
    CHECK_EQ(type_change_defaults(ConditionType::Attachment, false).thickness, 0.0);
    CHECK_FALSE(type_change_defaults(ConditionType::Linear, false).backout);
    CHECK_FALSE(type_change_defaults(ConditionType::Area, false).backout);
    CHECK(type_change_defaults(ConditionType::Count, false).backout);
    CHECK(type_change_defaults(ConditionType::Attachment, false).backout);

    // metric: thickness_metric 100 / 100 / 100 / 0 mm
    // (the Windows mm_to_inches(mm) is mm / 25.4, values computed with python -B)
    CHECK_EQ(type_change_defaults(ConditionType::Linear, true).thickness, 3.937007874015748);
    CHECK_EQ(type_change_defaults(ConditionType::Area, true).thickness, 3.937007874015748);
    CHECK_EQ(type_change_defaults(ConditionType::Count, true).thickness, 3.937007874015748);
    CHECK_EQ(type_change_defaults(ConditionType::Attachment, true).thickness, 0.0);
    CHECK_FALSE(type_change_defaults(ConditionType::Linear, true).backout);
    CHECK_FALSE(type_change_defaults(ConditionType::Area, true).backout);
    CHECK(type_change_defaults(ConditionType::Count, true).backout);
    CHECK(type_change_defaults(ConditionType::Attachment, true).backout);

    // the spacing is the field's whatever the unit: Linear -> Area keeps the 100 mm the Spacing field shows (it neither becomes the
    // 150 mm of the metric Area table nor the imperial 6.0 in), and the Thickness field's default is the metric one
    Condition stored = notes_condition(ConditionType::Linear);
    stored.spacing = 100.0 / 25.4;
    const StyleChange change = apply_style_change(stored, ConditionType::Area, true);
    const Condition saved = condition_to_save(stored, change.condition, true);
    CHECK_EQ(saved.spacing, stored.spacing);
    CHECK_STR_EQ(inches_to_display(saved.spacing, true), "100");
    CHECK_EQ(saved.thickness, type_change_defaults(ConditionType::Area, true).thickness);
    CHECK_EQ(condition_to_save(stored, apply_style_change(stored, ConditionType::Area, false).condition, false).spacing, stored.spacing);
}

namespace
{
    constexpr ConditionType kStyles[] = {ConditionType::Linear, ConditionType::Area, ConditionType::Count, ConditionType::Attachment};

    bool count_like_style(ConditionType type)
    {
        return type == ConditionType::Count || type == ConditionType::Attachment;
    }

    // Spacing 2.5 and thickness 6.0 are never a table value, so the saved value shows whether a field or the table was written.
    Condition stored_of_style(ConditionType type)
    {
        Condition stored = notes_condition(type);
        stored.spacing = 2.5;
        stored.thickness = 6.0;
        stored.backout = !count_like_style(type);
        return stored;
    }
}

// Metric bid, every pair of styles: Save writes the metric thickness default (style_dimension_defaults(to, true), what the dialog
// shows after the style change and the TYPE_DEFAULTS thickness_metric column) and the table's backout; the spacing is the stored
// 2.5 the Spacing field still shows, never the spacing_metric column (renamed from AMetricBidPersistsTheMetricTypeDefaultsForEveryStyleChange).
TEST(CondStyle02_Spacing, AMetricBidSavesTheMetricThicknessAndTheSpacingItShowsForEveryStyleChange)
{
    const double thickness_mm[] = {100.0, 100.0, 100.0, 0.0}; // thickness_metric
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

            CHECK_EQ(saved.spacing, 2.5);
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
            CHECK(changes.has(ConditionField::Spacing)); // the field equals the stored value, written all the same on a style change
            CHECK_EQ(changes.values.spacing, 2.5);
            CHECK(changes.has(ConditionField::Thickness));
            CHECK(changes.has(ConditionField::Backout));
            CHECK_EQ(changes.values.backout, count_like_style(kStyles[to]));
            CHECK_EQ(changes.values.thickness, thickness_mm[to] / 25.4);
        }
    }
}

// Imperial bid, every pair of styles: the thickness is the Windows TYPE_DEFAULTS imperial column (edit_condition_dialog.py:97-150)
// and the spacing is the stored 2.5 the field still shows, not the table's 4.0 / 6.0 (renamed from
// AnImperialBidKeepsTheWindowsTableForEveryStyleChange).
TEST(CondStyle02_Spacing, AnImperialBidSavesTheWindowsThicknessTableAndTheSpacingItShowsForEveryStyleChange)
{
    const double thickness_in[] = {4.0, 4.0, 4.0, 0.0}; // Linear, Area, Count, Attachment
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
            CHECK_EQ(saved.spacing, 2.5);
            CHECK_EQ(saved.thickness, thickness_in[to]);
            CHECK_EQ(saved.backout, count_like_style(kStyles[to]));
            const ConditionChanges changes = save_changes(stored, change.condition, false);
            CHECK(changes.has(ConditionField::Spacing)); // equal to the stored 2.5, written all the same on a style change
            CHECK_EQ(changes.values.spacing, 2.5);
            CHECK(changes.has(ConditionField::Thickness));
            CHECK(changes.has(ConditionField::Backout));
            CHECK_EQ(changes.values.thickness, thickness_in[to]);
        }
    }
}

// A metric bid keeps what the fields show: a spacing or thickness the user typed wins, a spacing that was not typed stays the
// stored one, a thickness that was not typed is the metric default the field started from.
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
    CHECK_EQ(saved.spacing, stored.spacing); // not the 150 mm of the metric Area table

    // a Count has no thickness field: the table value is written whatever the form holds
    Condition count = apply_style_change(stored, ConditionType::Count, true).condition;
    count.thickness = 5.5;
    CHECK_EQ(condition_to_save(stored, count, true).thickness, 3.937007874015748);
}

// The thickness of a Linear / Area style is what its field shows, also when that equals the stored thickness (the field wins;
// before COND-STYLE-02 this was "the type default when the field still equals the stored thickness"). A condition created in a
// metric bid usually stores exactly the metric default (100 mm), so the field after the style change (100 mm) equals the stored
// value: 100 mm is what is saved (written on the style change all the same, edit_condition_dialog.py:1575, :1605), and not the
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
            // a style change writes the Thickness although it equals the stored value (`type_changed or thickness != stored`)
            const ConditionChanges changes = save_changes(stored, change.condition, true);
            CHECK(changes.has(ConditionField::Thickness));
            CHECK_EQ(changes.values.thickness, metric_thickness);
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

// The Area style starts from the default thickness (the reset text), and a Linear / Area thickness the user typed AFTER the change
// wins over the table value (the field wins, also when it equals the stored value: COND-STYLE-02)
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

// ---------------------------------------------------------------------------------------------------------------------
// COND-STYLE-02, "what the field shows is what is saved": ports of the six tests of class EditConditionTypeChangeAndElevationTests
// in tests/presentation/dialogs/test_edit_condition_dialog.py (:1233-1317, the same lines in Windows b9902e6 and 49a2d59). On a type change Windows sends the
// type default only for the thickness of a Count / Attachment (which has no Thickness field) and the backout; Linear / Area
// thickness and the spacing are the parsed fields and are written even when they equal the stored value (`if type_changed or
// ...`, edit_condition_dialog.py:1575, :1605, :1640). The Windows tests drive the dialog with an identity unit conversion, so
// they pin the model values; the form built here is what the dialog holds after the switch (apply_style_change) plus what the
// user typed (the .mm parses the field texts into those members before condition_to_save).
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
    // The form after `target` was picked in the Style picker, before anything else was typed.
    Condition form_after_switch(const Condition &stored, ConditionType target, bool metric)
    {
        return apply_style_change(stored, target, metric).condition;
    }
}

// W1 test_a_metric_type_change_saves_the_metric_defaults_it_shows
TEST(CondStyle02_FieldWins, AMetricTypeChangeSavesTheMetricDefaultsItShows)
{
    const double hundred_mm = mm_to_inches(100.0);
    CHECK_EQ(hundred_mm, 3.937007874015748);
    struct Row
    {
        ConditionType target;
        double thickness;
    };
    const Row rows[] = {{ConditionType::Area, hundred_mm}, {ConditionType::Count, hundred_mm}, {ConditionType::Attachment, 0.0}};
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("target " + std::to_string(static_cast<int>(row.target)));
        Condition stored = notes_condition(ConditionType::Linear);
        stored.thickness = hundred_mm;
        stored.spacing = hundred_mm;
        const Condition saved = condition_to_save(stored, form_after_switch(stored, row.target, true), true);
        CHECK(saved.condition_type == row.target);
        CHECK_EQ(saved.thickness, row.thickness);
        CHECK_EQ(saved.spacing, hundred_mm);
    }
}

// W2 test_an_imperial_type_change_still_saves_the_imperial_defaults
TEST(CondStyle02_FieldWins, AnImperialTypeChangeSavesTheImperialDefaults)
{
    struct Row
    {
        ConditionType target;
        double thickness;
    };
    const Row rows[] = {{ConditionType::Area, 4.0}, {ConditionType::Count, 4.0}, {ConditionType::Attachment, 0.0}};
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("target " + std::to_string(static_cast<int>(row.target)));
        Condition stored = notes_condition(ConditionType::Linear);
        stored.thickness = 4.0;
        stored.spacing = 4.0;
        const Condition saved = condition_to_save(stored, form_after_switch(stored, row.target, false), false);
        CHECK(saved.condition_type == row.target);
        CHECK_EQ(saved.thickness, row.thickness);
        CHECK_EQ(saved.spacing, 4.0);
    }
}

// W3 test_a_metric_area_keeps_the_150_mm_spacing_it_shows (a regression guard: the metric Area default is 150 mm as well, so it
// also passed before COND-STYLE-02 changed). The second half is not in the Windows test but follows from the same code: any other
// spacing the field shows stays, it is never replaced by the 150 mm of the Area table.
TEST(CondStyle02_FieldWins, AMetricAreaKeepsThe150mmSpacingItShows)
{
    Condition stored = notes_condition(ConditionType::Linear);
    stored.thickness = 4.0;
    stored.spacing = mm_to_inches(150.0);
    const Condition form = form_after_switch(stored, ConditionType::Area, true);
    CHECK_EQ(form.spacing, stored.spacing); // the Spacing text is not reset by a style change
    const Condition saved = condition_to_save(stored, form, true);
    CHECK_EQ(saved.spacing, stored.spacing);
    CHECK_EQ(saved.spacing, 5.905511811023622);
    CHECK_NE(saved.spacing, 6.0);

    stored.spacing = mm_to_inches(120.0);
    CHECK_EQ(condition_to_save(stored, form_after_switch(stored, ConditionType::Area, true), true).spacing, stored.spacing);
}

// W4 test_a_typed_spacing_survives_a_type_change: the typed 9 equals the stored 9 and used to be replaced by the Area default 6.0
TEST(CondStyle02_FieldWins, ATypedSpacingEqualToTheStoredOneSurvivesATypeChange)
{
    for (double typed : {9.0, 7.0})
    {
        SCOPED_CONTEXT("typed " + std::to_string(typed));
        Condition stored = notes_condition(ConditionType::Linear);
        stored.thickness = 4.0;
        stored.spacing = 9.0;
        Condition form = form_after_switch(stored, ConditionType::Area, false);
        form.spacing = typed;
        form.thickness = 5.0;
        const Condition saved = condition_to_save(stored, form, false);
        CHECK(saved.condition_type == ConditionType::Area);
        CHECK_EQ(saved.spacing, typed);
        CHECK_EQ(saved.thickness, 5.0);
    }
}

// W5 test_a_typed_thickness_equal_to_the_stored_one_survives_a_type_change, Linear -> Area and Area -> Linear: the typed 5 equals
// the stored 5 and used to be replaced by the type default 4.0
TEST(CondStyle02_FieldWins, ATypedThicknessEqualToTheStoredOneSurvivesATypeChange)
{
    const ConditionType pairs[][2] = {{ConditionType::Linear, ConditionType::Area}, {ConditionType::Area, ConditionType::Linear}};
    for (const auto &pair : pairs)
    {
        SCOPED_CONTEXT("from " + std::to_string(static_cast<int>(pair[0])) + " to " + std::to_string(static_cast<int>(pair[1])));
        Condition stored = notes_condition(pair[0]);
        stored.thickness = 5.0;
        stored.spacing = 4.0;
        Condition form = form_after_switch(stored, pair[1], false);
        CHECK_EQ(form.thickness, 4.0); // the Thickness field starts from the default of the new style
        form.thickness = 5.0;          // the user types 5 into it
        const Condition saved = condition_to_save(stored, form, false);
        CHECK(saved.condition_type == pair[1]);
        CHECK_EQ(saved.thickness, 5.0);
    }
}

// W6 test_without_a_type_change_unchanged_fields_are_not_written: the typed 9 and 4 equal the stored values, nothing is a change
TEST(CondStyle02_FieldWins, WithoutATypeChangeUnchangedFieldsAreNotWritten)
{
    Condition stored = notes_condition(ConditionType::Area);
    stored.thickness = 4.0;
    stored.spacing = 9.0;
    Condition form = stored;
    form.spacing = 9.0;
    form.thickness = 4.0;
    const Condition saved = condition_to_save(stored, form, false);
    CHECK_EQ(saved.spacing, 9.0);
    CHECK_EQ(saved.thickness, 4.0);
    CHECK(collect_condition_changes(stored, saved).empty());
    CHECK(save_changes(stored, form).empty()); // what the dialog hands to the store: nothing to write
    // a field that differs is written, one that is equal is not (no type change)
    form.spacing = 7.0;
    const ConditionChanges changes = save_changes(stored, form);
    CHECK(changes.has(ConditionField::Spacing));
    CHECK_FALSE(changes.has(ConditionField::Thickness));
    CHECK_FALSE(changes.has(ConditionField::Backout));
    CHECK_EQ(changes.fields.count(), 1u);
}

// Every pair of styles in both measurement systems, stored spacing 2.5 and thickness 6.0 (never a table value), nothing typed: the
// spacing is the field's (the stored 2.5, a style change never resets its text), the thickness of a Linear / Area is what its
// field shows after the change (the default of the new style) and the thickness of a Count / Attachment is the default of the
// table; the change set carries the spacing all the same (a style change writes it although it equals the stored one), whatever
// the style.
TEST(CondStyle02_FieldWins, AnUntouchedTypeChangeKeepsTheSpacingAndSavesTheThicknessItShows)
{
    for (bool metric : {false, true})
    {
        for (int from = 0; from < 4; ++from)
        {
            for (int to = 0; to < 4; ++to)
            {
                if (from == to)
                    continue;
                SCOPED_CONTEXT(std::string(metric ? "metric" : "imperial") + " from " + std::to_string(from) + " to " + std::to_string(to));
                const Condition stored = stored_of_style(kStyles[from]);
                const Condition form = form_after_switch(stored, kStyles[to], metric);
                const Condition saved = condition_to_save(stored, form, metric);
                const StyleDimensionDefaults defaults = style_dimension_defaults(kStyles[to], metric);
                CHECK(saved.condition_type == kStyles[to]);
                CHECK_EQ(saved.spacing, 2.5);
                CHECK_EQ(saved.thickness, defaults.thickness);
                if (!count_like_style(kStyles[to]))
                    CHECK_EQ(saved.thickness, form.thickness); // the field wins
                CHECK_EQ(saved.backout, count_like_style(kStyles[to]));
                const ConditionChanges changes = save_changes(stored, form, metric);
                CHECK(changes.has(ConditionField::Spacing)); // equal to the stored 2.5, written on a style change
                CHECK_EQ(changes.values.spacing, 2.5);
                CHECK(changes.has(ConditionField::ConditionType));
            }
        }
    }
}

// COND-STYLE-02, literal write parity (user spec: "Spacing and the visible thickness are saved from the fields even when equal to the
// stored value; without a type change, unchanged fields are left alone"). On a type change Windows puts the thickness and the
// backout of the table, the Linear / Area thickness field (`if type_changed or ...`, :1575, :1605) and the spacing field
// (:1640) into the DTO whatever the stored values are, so the UPDATE carries those columns although every one of them equals the
// stored value. That matters where the stored column is NULL: the loader reads it as 0.0, the blank field parses to 0.0, Windows
// writes 0.0. Here every stored value already equals what is saved (spacing 0.0 = NULL read back, the thickness and the backout
// of the new style), so only the type itself differs; collect_condition_changes alone marks none of the three.
TEST(CondStyle02_FieldWins, ATypeChangeWritesSpacingThicknessAndBackoutEvenWhenTheyEqualTheStoredValues)
{
    for (bool metric : {false, true})
    {
        for (int from = 0; from < 4; ++from)
        {
            for (int to = 0; to < 4; ++to)
            {
                if (from == to)
                    continue;
                SCOPED_CONTEXT(std::string(metric ? "metric" : "imperial") + " from " + std::to_string(from) + " to " + std::to_string(to));
                const StyleDimensionDefaults table = style_dimension_defaults(kStyles[to], metric);
                Condition stored = notes_condition(kStyles[from]);
                stored.spacing = 0.0; // a NULL column, as the loader reads it
                stored.thickness = table.thickness;
                stored.backout = count_like_style(kStyles[to]);
                const Condition form = form_after_switch(stored, kStyles[to], metric);
                CHECK_EQ(form.spacing, 0.0); // a style change leaves the Spacing text alone

                const Condition saved = condition_to_save(stored, form, metric);
                CHECK_EQ(saved.spacing, stored.spacing);
                CHECK_EQ(saved.thickness, stored.thickness);
                CHECK_EQ(saved.backout, stored.backout);
                const ConditionChanges by_value = collect_condition_changes(stored, saved);
                CHECK_FALSE(by_value.has(ConditionField::Spacing));
                CHECK_FALSE(by_value.has(ConditionField::Thickness));
                CHECK_FALSE(by_value.has(ConditionField::Backout));

                const ConditionChanges changes = condition_changes_for_save(stored, form, metric);
                CHECK(changes.has(ConditionField::ConditionType));
                CHECK(changes.has(ConditionField::Spacing));
                CHECK(changes.has(ConditionField::Thickness));
                CHECK(changes.has(ConditionField::Backout));
                CHECK_EQ(changes.values.spacing, 0.0);
                CHECK_EQ(changes.values.thickness, table.thickness);
                CHECK_EQ(changes.values.backout, count_like_style(kStyles[to]));
                // the three are added to what differs by value, nothing else is
                std::bitset<kConditionFieldCount> expected = by_value.fields;
                expected.set(static_cast<std::size_t>(ConditionField::Spacing));
                expected.set(static_cast<std::size_t>(ConditionField::Thickness));
                expected.set(static_cast<std::size_t>(ConditionField::Backout));
                CHECK(changes.fields == expected);
                // the condition to store is the one condition_to_save returns
                CHECK(changes.values.condition_type == saved.condition_type);
                CHECK_EQ(changes.values.uom1, saved.uom1);
                CHECK_EQ(changes.values.calc_type1, saved.calc_type1);
            }
        }
    }
}

// The other half of the spec: without a type change an unchanged field is left alone, so the change set is exactly the fields that
// differ from the stored condition (collect_condition_changes of what condition_to_save returns), and Spacing, Thickness and
// Backout are marked only when they differ: a rename, a typed spacing, a typed thickness, and a Count / Attachment whose form holds
// another thickness (it has no Thickness field: nothing is written for it).
TEST(CondStyle02_FieldWins, WithoutATypeChangeTheChangeSetIsExactlyTheFieldsThatDiffer)
{
    for (bool metric : {false, true})
    {
        for (ConditionType type : kStyles)
        {
            SCOPED_CONTEXT(std::string(metric ? "metric" : "imperial") + " style " + std::to_string(static_cast<int>(type)));
            const Condition stored = stored_of_style(type);
            Condition renamed = stored;
            renamed.name = "Renamed";
            Condition spacing = stored;
            spacing.spacing = 3.0;
            Condition thickness = stored;
            thickness.thickness = 7.0;
            for (const Condition &form : {stored, renamed, spacing, thickness})
            {
                const ConditionChanges expected = collect_condition_changes(stored, condition_to_save(stored, form, metric));
                const ConditionChanges changes = save_changes(stored, form, metric);
                CHECK(changes.fields == expected.fields);
                CHECK_EQ(changes.values.spacing, expected.values.spacing);
                CHECK_EQ(changes.values.thickness, expected.values.thickness);
                CHECK_EQ(changes.values.backout, expected.values.backout);
                CHECK_FALSE(changes.has(ConditionField::Backout));
            }
            CHECK(save_changes(stored, stored, metric).empty());
            CHECK_EQ(save_changes(stored, spacing, metric).has(ConditionField::Spacing), true);
            CHECK_EQ(save_changes(stored, renamed, metric).has(ConditionField::Spacing), false);
            // a typed thickness is a change of a Linear / Area only
            CHECK_EQ(save_changes(stored, thickness, metric).has(ConditionField::Thickness), !count_like_style(type));
            CHECK_EQ(save_changes(stored, thickness, metric).has(ConditionField::Name), false);
        }
    }
}

// COND-STYLE-02: Windows has no Thickness field for a Count / Attachment, so a Save of one after a style change writes the table's
// thickness whatever the form holds (_type_default_thickness, edit_condition_dialog.py:1250-1253, :1474-1479). The form here
// holds another thickness (7.0) after the switch, which the dialog cannot produce (the field does not exist), so this test pins the
// Core rule: imperial 4.0 for a Count and 0.0 for an Attachment, metric mm_to_inches(100) and 0.0. Without it, taking the form's
// thickness (or the table's in one measurement system only) goes unnoticed because apply_style_change already sets the default.
TEST(CondStyle02_FieldWins, ACountOrAttachmentSavesTheTableThicknessWhateverTheFormHolds)
{
    const double hundred_mm = mm_to_inches(100.0);
    CHECK_EQ(hundred_mm, 3.937007874015748);
    struct Row
    {
        ConditionType target;
        double imperial;
        double metric;
    };
    const Row rows[] = {{ConditionType::Count, 4.0, hundred_mm}, {ConditionType::Attachment, 0.0, 0.0}};
    for (const Row &row : rows)
    {
        for (bool metric : {false, true})
        {
            for (ConditionType from : kStyles)
            {
                if (from == row.target)
                    continue;
                SCOPED_CONTEXT(std::string(metric ? "metric" : "imperial") + " from " + std::to_string(static_cast<int>(from)) +
                               " to " + std::to_string(static_cast<int>(row.target)));
                const double expected = metric ? row.metric : row.imperial;
                const Condition stored = stored_of_style(from); // thickness 6.0, never a table value
                Condition form = form_after_switch(stored, row.target, metric);
                CHECK_EQ(form.thickness, expected); // the switch itself starts from the table value
                form.thickness = 7.0;
                const Condition saved = condition_to_save(stored, form, metric);
                CHECK(saved.condition_type == row.target);
                CHECK_EQ(saved.thickness, expected);
                CHECK_NE(saved.thickness, 7.0);
                const ConditionChanges changes = save_changes(stored, form, metric);
                CHECK(changes.has(ConditionField::Thickness));
                CHECK_EQ(changes.values.thickness, expected);
            }
        }
        // without a style change the stored thickness stays whatever the form holds (a Count / Attachment has no Thickness field)
        SCOPED_CONTEXT("no style change, style " + std::to_string(static_cast<int>(row.target)));
        const Condition stored = stored_of_style(row.target);
        Condition form = stored;
        form.thickness = 7.0;
        CHECK_EQ(condition_to_save(stored, form, false).thickness, 6.0);
        CHECK_FALSE(save_changes(stored, form).has(ConditionField::Thickness));
    }
}

// Inferred, not pinned by a Windows test: the Thickness field a Linear / Area form shows after a style change, parsed back the way
// the dialog parses it (_parse_dimension_field), is the default the model holds, so what the field shows is what is saved in a
// metric bid too (the Windows tests use an identity conversion and do not exercise the millimetre text).
TEST(CondStyle02_FieldWins, TheMetricThicknessTheFieldShowsParsesBackToTheDefault)
{
    for (ConditionType to : {ConditionType::Linear, ConditionType::Area})
    {
        SCOPED_CONTEXT("target " + std::to_string(static_cast<int>(to)));
        const Condition stored = stored_of_style(ConditionType::Count);
        const Condition form = form_after_switch(stored, to, true);
        const std::string shown = inches_to_display(form.thickness, true);
        CHECK_STR_EQ(shown, "100");
        Condition typed = form;
        double parsed = -1.0;
        CHECK_FALSE(parse_dimension_value(shown, "Thickness", true, parsed).has_value());
        typed.thickness = parsed;
        const Condition saved = condition_to_save(stored, typed, true);
        CHECK_EQ(saved.thickness, mm_to_inches(100.0));
    }
}

// Inferred, not pinned by a Windows test: a Thickness field the user blanked is 0.0 (_parse_dimension_field: blank text is 0.0)
// and the field wins, also when the stored thickness is 0.0 as well (an Attachment's), where the old rule wrote the default.
TEST(CondStyle02_FieldWins, ABlankedThicknessFieldIsSavedAsZeroOnATypeChange)
{
    for (ConditionType to : {ConditionType::Linear, ConditionType::Area})
    {
        for (double stored_thickness : {0.0, 6.0})
        {
            SCOPED_CONTEXT("target " + std::to_string(static_cast<int>(to)) + " stored " + std::to_string(stored_thickness));
            Condition stored = notes_condition(ConditionType::Attachment);
            stored.thickness = stored_thickness;
            Condition form = form_after_switch(stored, to, false);
            double parsed = -1.0;
            CHECK_FALSE(parse_dimension_value("  ", "Thickness", false, parsed).has_value());
            CHECK_EQ(parsed, 0.0);
            form.thickness = parsed;
            CHECK_EQ(condition_to_save(stored, form, false).thickness, 0.0);
        }
    }
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

// ---------------------------------------------------------------------------------------------------------------------
// COND-M08 (Windows 49a2d59, edit_condition_dialog.py): a stored condition type outside 0..3 is KEPT.
// The editor opens with the type unchanged (_load_condition :839-847 selects the disabled "Unknown (N)" item and stores the raw type
// in _style_type), the layout is Area-like (:547, :740, :1601), a Save without a style choice writes only what changed (a notes-only
// edit writes only the notes: ct == cond.condition_type, so type_changed is False, :1464-1474), and picking a real style is a type
// change from an unknown previous style (_on_style_changed :1215-1226: nothing is carried, _populate_defaults_for_type).
// Windows tests: EditConditionUnknownTypeTests (:1432) test_editing_only_the_notes_saves_only_the_notes,
// test_picking_a_real_type_still_changes_the_type, test_a_condition_with_takeoffs_keeps_its_unknown_type,
// test_linear_branches_skip_widgets_that_were_never_built; the expected field sets of the picks are the dto keys of the Windows
// dialog driven headless for this verification (plus the iOS-only Connect field of COND-EXTRA-01, which Windows has no control for).
// ---------------------------------------------------------------------------------------------------------------------

// the form of a loaded condition: the stored type stays, whatever it is; Trim / Curved Segment never both on (COND-TRIM-01)
TEST(CondM08_UnknownStyle, TheFormOfALoadedConditionKeepsTheStoredType)
{
    for (int n : {7, -1, 4, 99})
    {
        SCOPED_CONTEXT("stored type " + std::to_string(n));
        const Condition stored = unknown_condition(n);
        const Condition form = condition_form_for_edit(stored);
        CHECK(form.condition_type == static_cast<ConditionType>(n));
        CHECK_STR_EQ(form.name, "Odd");
    }
    for (ConditionType known : {ConditionType::Linear, ConditionType::Area, ConditionType::Count, ConditionType::Attachment})
        CHECK(condition_form_for_edit(unknown_condition(static_cast<int>(known))).condition_type == known);

    // the one normalisation of a load
    Condition both = notes_condition(ConditionType::Linear);
    both.trim = true;
    both.is_curved_segment = true;
    const Condition form = condition_form_for_edit(both);
    CHECK(form.trim);
    CHECK_FALSE(form.is_curved_segment);
    CHECK(form.condition_type == ConditionType::Linear);
}

// test_editing_only_the_notes_saves_only_the_notes (7 and -1, and the same in a metric bid): saved == [{"notes": "edited"}]: no
// type, no spacing, no thickness, no backout. The fixtures store quantity 0 / UOM -1 on purpose: Windows also resets a stored
// quantity that the type's option list does not offer to (none) on every save (edit_condition_dialog.py :897-898, :984-990,
// :1658-1664 in 49a2d59), which iOS does not port (owner decision, recorded in the documents), so a type-7 row WITH a stored
// quantity writes only the notes on iOS.
TEST(CondM08_UnknownStyle, ANotesOnlyEditWritesOnlyTheNotes)
{
    for (int n : {7, -1})
        for (bool metric : {false, true})
        {
            SCOPED_CONTEXT("stored type " + std::to_string(n) + (metric ? " metric" : " imperial"));
            const Condition stored = unknown_condition(n);
            Condition form = condition_form_for_edit(stored);
            form.notes = "edited";
            const ConditionChanges changes = save_changes(stored, form, metric);
            CHECK(fields_are(changes, {ConditionField::Notes}));
            CHECK_EQ(changes.fields.count(), 1u);
            CHECK(changes.values.condition_type == stored.condition_type);
            CHECK_STR_EQ(changes.values.notes, "edited");
            CHECK_EQ(changes.values.thickness, 4.0);
            CHECK_EQ(changes.values.spacing, stored.spacing);
        }
}

// the same with many stored values away from the defaults (Windows oracle: still only {"notes": "edited"}), and for stored types far
// outside 0..3 as well (4, 99 and the ends of an int: the save path must not treat any of them as a type change; found by the
// independent mutation re-check, mutant: |type| > 1000 counts as a type change)
TEST(CondM08_UnknownStyle, ANotesOnlyEditOfAConditionWithManyStoredValuesWritesOnlyTheNotes)
{
    for (int n : {7, -1, 4, 99, 2147483647, -2147483647 - 1})
    {
        SCOPED_CONTEXT("stored type " + std::to_string(n));
        Condition stored = rich_unknown_condition(n);
        stored.rise = 3.0; // the Area-like form shows abs(rise) / abs(run) (DLG-RISE-01), so a positive value is what a reload saves
        stored.run = 4.0;
        stored.color_fill = 0x336699;
        stored.depth = 6.0;
        stored.grid = true;
        stored.grid_size1 = 12.0;
        stored.grid_size2 = 24.0;
        stored.gap = 1.0;
        stored.display_dimension = true;
        stored.display_grid_while_drawing = true;
        stored.drop_run = true;
        stored.drop_value = 2.0;
        stored.trim = true;
        stored.connect = true;
        stored.backout = true; // the table backout of a type change is not written without a type change
        Condition form = condition_form_for_edit(stored);
        form.notes = "edited";
        const ConditionChanges changes = save_changes(stored, form, false);
        CHECK(fields_are(changes, {ConditionField::Notes}));
        CHECK(changes.values.condition_type == stored.condition_type);
        CHECK(changes.values.backout);
    }
}

// The layout of an unknown type is the Area one for the save too (Windows :1601-1630, :1712-1741): the controls an Area has are
// written when they change, the ones it has not (Height, Width, Depth, Display Size, Shape, Drop/Run, Add Length, Connect, Trim,
// Curved Segment) are not taken from the form, and the type stays. The same edits on a stored Area give the same change set.
TEST(CondM08_UnknownStyle, EditingTheAreaControlsOfAnUnknownTypeSavesLikeAnArea)
{
    const auto edit = [](Condition &form)
    {
        // the controls of the Area layout
        form.thickness = 6.0;
        form.rise = 1.5;
        form.run = 2.0;
        form.round_quantity = true;
        form.round_up = 0.25;
        form.grid = true;
        form.grid_size1 = 12.0;
        form.grid_size2 = 24.0;
        form.gap = 1.0;
        form.display_grid_while_drawing = true;
        form.display_dimension = true;
        form.display_name = true;
        form.snap_to_linear = 0; // iOS-only (COND-EXTRA-01), kept for every Area-like layout
        // controls this layout does not have: whatever the form holds for them is not saved
        form.height = 9.0;
        form.width = 9.0;
        form.depth = 9.0;
        form.display_size = 50.0;
        form.shape = ShapeId::Circle;
        form.drop_run = true;
        form.drop_value = 1.0;
        form.trim = true;
        form.is_curved_segment = true;
        form.connect = true;
        form.connect_tolerance = 3.0;
    };
    const ConditionChanges area = [&]
    {
        const Condition stored = unknown_condition(1); // a real Area with the same values
        Condition form = condition_form_for_edit(stored);
        edit(form);
        return save_changes(stored, form, false);
    }();
    CHECK(fields_are(area, {ConditionField::Thickness, ConditionField::Rise, ConditionField::Run, ConditionField::RoundQuantity,
                            ConditionField::RoundUp, ConditionField::Grid, ConditionField::GridSize1, ConditionField::GridSize2,
                            ConditionField::Gap, ConditionField::DisplayGridWhileDrawing, ConditionField::DisplayDimension,
                            ConditionField::DisplayName, ConditionField::SnapToLinear}));
    for (int n : {7, -1})
    {
        SCOPED_CONTEXT("stored type " + std::to_string(n));
        const Condition stored = unknown_condition(n);
        Condition form = condition_form_for_edit(stored);
        edit(form);
        const ConditionChanges changes = save_changes(stored, form, false);
        CHECK(changes.fields == area.fields);
        CHECK(changes.values.condition_type == stored.condition_type);
        CHECK_FALSE(changes.has(ConditionField::ConditionType));
        CHECK_EQ(changes.values.thickness, 6.0);
        CHECK_EQ(changes.values.height, stored.height); // no Height control: the stored 8.0 stays
        CHECK_EQ(changes.values.width, stored.width);
        CHECK(changes.values.shape == stored.shape);
        CHECK_FALSE(changes.values.trim);
        CHECK_FALSE(changes.values.drop_run);
        CHECK_FALSE(changes.values.connect);
    }
}

// the Linear-only switches (drop_run, trim, curved, add length) do not exist for the Area-like layout: with the OLD loader (Linear
// shown) the curved flag below would have been rewritten with the type, the spacing, the thickness and the backout
TEST(CondM08_UnknownStyle, ANotesOnlyEditKeepsTrimAndCurvedOfAnUnknownType)
{
    Condition stored = unknown_condition(7);
    stored.trim = true;
    stored.is_curved_segment = true;
    Condition form = condition_form_for_edit(stored);
    form.notes = "edited";
    const ConditionChanges changes = save_changes(stored, form, false);
    CHECK(fields_are(changes, {ConditionField::Notes}));
    CHECK(changes.values.trim);
    CHECK(changes.values.is_curved_segment);
    CHECK(changes.values.condition_type == static_cast<ConditionType>(7));
}

// test_picking_a_real_type_still_changes_the_type: the type and the defaults of the style are written, nothing else of the stored
// row (the plain fixture has nothing the old style could carry). The dto keys are those of the Windows dialog driven headless.
TEST(CondM08_UnknownStyle, ChoosingARealTypeWritesTheTypeAndItsDefaults)
{
    for (int n : {7, -1})
        for (bool metric : {false, true})
        {
            SCOPED_CONTEXT("stored type " + std::to_string(n) + (metric ? " metric" : " imperial"));
            const Condition stored = unknown_condition(n);
            // the form starts from the stored type: it is the "previous style" of the pick (N, not Linear)
            REQUIRE(condition_form_for_edit(stored).condition_type == static_cast<ConditionType>(n));
            const double thick = metric ? mm_to_inches(100.0) : 4.0;
            const double width = metric ? mm_to_inches(300.0) : 12.0;

            {
                SCOPED_CONTEXT("to Linear");
                const ConditionChanges c = pick_from(stored, ConditionType::Linear, metric);
                CHECK(fields_are(c, {ConditionField::ConditionType, ConditionField::Height, ConditionField::Thickness,
                                     ConditionField::Backout, ConditionField::Spacing, ConditionField::CalcType1,
                                     ConditionField::Uom1, ConditionField::Connect}));
                CHECK(c.values.condition_type == ConditionType::Linear);
                CHECK_EQ(c.values.height, 0.0);
                CHECK_EQ(c.values.thickness, thick);
                CHECK_FALSE(c.values.backout);
                CHECK_EQ(c.values.spacing, 0.0);
                CHECK_EQ(c.values.calc_type1, 1);
                CHECK_EQ(c.values.uom1, metric ? 13 : 2);
                CHECK(c.values.connect);
                CHECK(c.values.pattern == stored.pattern);
            }
            {
                SCOPED_CONTEXT("to Area");
                const ConditionChanges c = pick_from(stored, ConditionType::Area, metric);
                CHECK(fields_are(c, {ConditionField::ConditionType, ConditionField::Thickness, ConditionField::Backout,
                                     ConditionField::Spacing, ConditionField::CalcType1, ConditionField::Uom1,
                                     ConditionField::DisplayGridWhileDrawing}));
                CHECK(c.values.condition_type == ConditionType::Area);
                CHECK_EQ(c.values.thickness, thick);
                CHECK_FALSE(c.values.backout);
                CHECK_EQ(c.values.spacing, 0.0);
                CHECK_EQ(c.values.calc_type1, 11);
                CHECK_EQ(c.values.uom1, metric ? 14 : 5);
                CHECK(c.values.display_grid_while_drawing);
                CHECK(c.values.pattern == stored.pattern);
            }
            for (ConditionType count_like : {ConditionType::Count, ConditionType::Attachment})
            {
                SCOPED_CONTEXT(count_like == ConditionType::Count ? "to Count" : "to Attachment");
                const ConditionChanges c = pick_from(stored, count_like, metric);
                CHECK(fields_are(c, {ConditionField::ConditionType, ConditionField::Height, ConditionField::Width,
                                     ConditionField::Thickness, ConditionField::Backout, ConditionField::Spacing,
                                     ConditionField::CalcType1, ConditionField::Uom1, ConditionField::Connect}));
                CHECK(c.values.condition_type == count_like);
                CHECK_EQ(c.values.height, 0.0);
                CHECK_EQ(c.values.width, width);
                CHECK_EQ(c.values.thickness, count_like == ConditionType::Count ? thick : 0.0); // Attachment: 0 in both systems
                CHECK(c.values.backout);
                CHECK_EQ(c.values.spacing, 0.0);
                CHECK_EQ(c.values.calc_type1, 23);
                CHECK_EQ(c.values.uom1, 0);
                CHECK(c.values.connect);
                CHECK(c.values.pattern == stored.pattern);
            }
        }
}

// Picking a real style from "Unknown (N)": nothing is carried (N has neither Display Name nor rounding), so the controls of the new
// style restart from the style's defaults, the shape is reset for Count / Attachment, spacing and pattern stay (Windows oracle, the
// "rich" table: spacing 2.5 kept and written, rounding and Display Name reset, shape Square)
TEST(CondM08_UnknownStyle, ChoosingARealTypeResetsWhatTheOldTypeDidNotCarry)
{
    for (int n : {7, -1})
    {
        SCOPED_CONTEXT("stored type " + std::to_string(n));
        const Condition stored = rich_unknown_condition(n);
        REQUIRE(condition_form_for_edit(stored).condition_type == static_cast<ConditionType>(n));
        {
            SCOPED_CONTEXT("to Linear");
            const ConditionChanges c = pick_from(stored, ConditionType::Linear, false);
            CHECK(fields_are(c, {ConditionField::ConditionType, ConditionField::Height, ConditionField::Thickness,
                                 ConditionField::Backout, ConditionField::Spacing, ConditionField::CalcType1,
                                 ConditionField::Uom1, ConditionField::Connect, ConditionField::Rise, ConditionField::Run,
                                 ConditionField::RoundQuantity, ConditionField::RoundUp}));
            CHECK_EQ(c.values.rise, 0.0);
            CHECK_EQ(c.values.run, 0.0);
            CHECK_FALSE(c.values.round_quantity);
            CHECK_EQ(c.values.round_up, 0.0);
            CHECK_EQ(c.values.spacing, 2.5);
            CHECK_EQ(c.values.height, 0.0);
            CHECK_FALSE(c.has(ConditionField::DisplayName)); // a Linear has no Display Name control
        }
        {
            SCOPED_CONTEXT("to Area");
            const ConditionChanges c = pick_from(stored, ConditionType::Area, false);
            CHECK(fields_are(c, {ConditionField::ConditionType, ConditionField::Thickness, ConditionField::Backout,
                                 ConditionField::Spacing, ConditionField::CalcType1, ConditionField::Uom1,
                                 ConditionField::DisplayGridWhileDrawing, ConditionField::Rise, ConditionField::Run,
                                 ConditionField::RoundQuantity, ConditionField::RoundUp, ConditionField::DisplayName}));
            CHECK_FALSE(c.values.round_quantity);
            CHECK_EQ(c.values.round_up, 0.0);
            CHECK_FALSE(c.values.display_name);
            CHECK(c.values.display_grid_while_drawing);
            CHECK_EQ(c.values.spacing, 2.5);
        }
        for (ConditionType count_like : {ConditionType::Count, ConditionType::Attachment})
        {
            SCOPED_CONTEXT(count_like == ConditionType::Count ? "to Count" : "to Attachment");
            const ConditionChanges c = pick_from(stored, count_like, false);
            CHECK(fields_are(c, {ConditionField::ConditionType, ConditionField::Height, ConditionField::Width,
                                 ConditionField::Thickness, ConditionField::Backout, ConditionField::Spacing,
                                 ConditionField::CalcType1, ConditionField::Uom1, ConditionField::Connect, ConditionField::Shape,
                                 ConditionField::DisplayName}));
            CHECK(c.values.shape == ShapeId::Square); // from the Rectangle of the stored condition
            CHECK_FALSE(c.values.display_name);
            CHECK_EQ(c.values.width, 12.0); // from 5.0
            CHECK_EQ(c.values.height, 0.0);
            CHECK_EQ(c.values.spacing, 2.5);
            // rise, run and the rounding have no control in a Count / Attachment: the stored values stay
            CHECK_FALSE(c.has(ConditionField::Rise));
            CHECK_FALSE(c.has(ConditionField::Run));
            CHECK_FALSE(c.has(ConditionField::RoundQuantity));
            CHECK_FALSE(c.has(ConditionField::RoundUp));
        }
    }
}

// Port of the assertions of test_linear_branches_skip_widgets_that_were_never_built that iOS can state, as an iOS CHARACTERISATION of
// the Linear branch of the change set (the Core half); it pins no COND-M08 rule (the keeping of the stored type and the notes-only
// save are pinned by the tests around it). The stored unknown type 7 with the form forced to Linear and every Linear-only value
// equal to the stored one: the type change and the notes are written, and none of the six fields the Windows test names
// (drop_run, drop_value, trim, is_curved_segment, round_quantity, round_up) is. The state of the Windows test (signals blocked
// over widgets that were built for another style) is unreachable on iOS: every control exists in every layout that needs it (the
// rows and the save branch both follow editor_layout(type)). The whole field set is NOT compared, because it is not the same on
// both sides: the real Windows dialog
// driven headless for that state (type 7, combo forced to Linear, notes edited) sends {backout False, condition_type 0, height 0.0,
// notes, spacing 0.0, thickness 0.0} (Height and a Thickness of 0.0 too, because its Linear widgets were never populated and are
// still read), where iOS writes the Type, the Notes and the Spacing, Thickness and Backout of the style change from the stored values.
TEST(CondM08_UnknownStyle, LinearOnlyFieldsThatEqualTheStoredOnesAreNotWritten)
{
    const Condition stored = unknown_condition(7);
    Condition form = condition_form_for_edit(stored);
    form.condition_type = ConditionType::Linear;
    form.notes = "edited";
    const ConditionChanges changes = save_changes(stored, form, false);
    CHECK(changes.has(ConditionField::ConditionType)); // Windows: dto.get("condition_type") == TYPE_LINEAR
    CHECK(changes.values.condition_type == ConditionType::Linear);
    CHECK(changes.has(ConditionField::Notes)); // Windows: dto.get("notes") == "edited"
    CHECK_STR_EQ(changes.values.notes, "edited");
    CHECK_FALSE(changes.has(ConditionField::DropRun));
    CHECK_FALSE(changes.has(ConditionField::DropValue));
    CHECK_FALSE(changes.has(ConditionField::Trim));
    CHECK_FALSE(changes.has(ConditionField::IsCurvedSegment));
    CHECK_FALSE(changes.has(ConditionField::RoundQuantity));
    CHECK_FALSE(changes.has(ConditionField::RoundUp));
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
