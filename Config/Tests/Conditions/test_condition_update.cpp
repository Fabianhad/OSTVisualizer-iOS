// COND-UPD-01 (alias DBP-16), COND-UPD-02 (aliases DLG-SAVE-01, COND-M06), COND-UPD-03 and COND-M05 (and, through the
// shared update transaction, the Core side of COND-M03): saving a condition writes only the columns of the fields that
// changed, writes NULL for a cleared reference or note, is scoped to the bid, and reports a failed write. The last section before
// the failed-write one is the reference-number shift at the edges of the number line (Windows b9902e6): DLG-FINITE-01 (a renumber
// whose shift would push another condition past 2147483647 fails before it writes anything and says why) and COND-UPD-02 (a
// condition without a number is "different from the new number", the normal conflict check runs and such rows are never shifted).
//
// Windows: ost_visualizer/infrastructure/mdb/components/condition_operations.py update_condition,
// application/dtos/update_condition_dto.py, presentation/dialogs/edit_condition_dialog.py _validate_and_build_dto.
#include "ConditionsTestSupport.hpp"

#include "Domain/ConditionFormRules.hpp"
#include "Domain/ConditionStyleChange.hpp"

#include <functional>
#include <unordered_map>

using ost_ctest::TempDb;
using ost_domain::Condition;
using ost_domain::ConditionChanges;
using ost_domain::ConditionField;

namespace
{
    // The iOS-schema counterpart of ConditionUpdateContractTests._seed / _full_db: bid 1 and 2, a layer and a condition
    // folder of each bid (30/40 of bid 1, 31/41 of bid 2) and condition 10 (bid 1, RefNo 1, 'Old', Type 0); every other
    // column of the row is NULL.
    void seed_update_world(const TempDb &db)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (1, 1, 'Bid one', 1, 0)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (2, 1, 'Bid two', 2, 1)");
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (30, 1, 'L30', -1, 1)");
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (31, 2, 'L31', -1, 1)");
        db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (40, 1, 'F40')");
        db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (41, 2, 'F41')");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 1, 'Old', 0)");
    }

    // One updatable field: how to set it on a Condition (the Windows _FULL_SPEC value) and what the column holds after
    // the update (SQL quote() of the column expression).
    struct FieldCase
    {
        ConditionField field;
        const char *label;
        const char *column;      // column name (excluded from the "nothing else changed" comparison)
        const char *expression;  // SQL expression of the stored value
        std::function<void(Condition &)> set;
        const char *expected;    // quote() of the stored value
    };

    const std::vector<FieldCase> &field_cases()
    {
        using ost_domain::ConditionType;
        using ost_domain::Pattern;
        using ost_domain::ShapeId;
        static const std::vector<FieldCase> cases = {
            // Port of ConditionUpdateContractTests.test_every_updatable_field_writes_its_own_column (_FULL_SPEC values)
            {ConditionField::Name, "name", "Name", "Name", [](Condition &c) { c.name = "Wall"; }, "'Wall'"},
            {ConditionField::ConditionType, "condition_type", "Type", "Type",
             [](Condition &c) { c.condition_type = ConditionType::Attachment; }, "3"},
            {ConditionField::Backout, "backout", "Backout", "Backout", [](Condition &c) { c.backout = true; }, "-1"},
            // ref_no is part of the table so the "every field" tests cover it; Windows' own test skips it because the reference
            // number has dedicated tests (below). 5 is free in the seeded bid, so nothing shifts.
            {ConditionField::RefNo, "ref_no", "RefNo", "RefNo", [](Condition &c) { c.ref_no = 5; }, "5"},
            {ConditionField::CdnTypeUid, "cdn_type_uid", "CdnTypeUID", "CdnTypeUID",
             [](Condition &c) { c.cdn_type_uid = "7"; }, "7"},
            {ConditionField::LayerUid, "layer_uid", "BidLayerUID", "BidLayerUID",
             [](Condition &c) { c.layer_uid = "30"; }, "30"},
            {ConditionField::Height, "height", "Height", "Height", [](Condition &c) { c.height = 1.5; }, "1.5"},
            {ConditionField::Width, "width", "Width", "Width", [](Condition &c) { c.width = 2.5; }, "2.5"},
            {ConditionField::Depth, "depth", "Depth", "Depth", [](Condition &c) { c.depth = 3.5; }, "3.5"},
            {ConditionField::Thickness, "thickness", "Thickness", "Thickness",
             [](Condition &c) { c.thickness = 4.5; }, "4.5"},
            {ConditionField::Rise, "rise", "Rise", "Rise", [](Condition &c) { c.rise = 5.5; }, "5.5"},
            {ConditionField::Run, "run", "Run", "Run", [](Condition &c) { c.run = 6.5; }, "6.5"},
            {ConditionField::DisplaySize, "display_size", "DisplaySize", "DisplaySize",
             [](Condition &c) { c.display_size = 7.5; }, "7.5"},
            {ConditionField::Shape, "shape", "Shape", "Shape", [](Condition &c) { c.shape = ShapeId::Triangle; }, "2"},
            {ConditionField::Pattern, "pattern", "Pattern", "Pattern",
             [](Condition &c) { c.pattern = Pattern::Vertical; }, "3"},
            {ConditionField::Spacing, "spacing", "Spacing", "Spacing", [](Condition &c) { c.spacing = 8.5; }, "8.5"},
            {ConditionField::ColorFill, "color_fill", "ColorFill", "ColorFill",
             [](Condition &c) { c.color_fill = 9; }, "9"},
            {ConditionField::Uom1, "uom1", "UOM1", "UOM1", [](Condition &c) { c.uom1 = 1; }, "1"},
            {ConditionField::Uom2, "uom2", "UOM2", "UOM2", [](Condition &c) { c.uom2 = 2; }, "2"},
            {ConditionField::Uom3, "uom3", "UOM3", "UOM3", [](Condition &c) { c.uom3 = 3; }, "3"},
            {ConditionField::CalcType1, "calc_type1", "Quantity1", "Quantity1",
             [](Condition &c) { c.calc_type1 = 4; }, "4"},
            {ConditionField::CalcType2, "calc_type2", "Quantity2", "Quantity2",
             [](Condition &c) { c.calc_type2 = 5; }, "5"},
            {ConditionField::CalcType3, "calc_type3", "Quantity3", "Quantity3",
             [](Condition &c) { c.calc_type3 = 6; }, "6"},
            // Windows stores the text as a blob (encode_text_blob); the platform storage form is not pinned, the value is
            {ConditionField::Notes, "notes", "Notes", "CAST(Notes AS TEXT)", [](Condition &c) { c.notes = "memo"; },
             "'memo'"},
            {ConditionField::DropRun, "drop_run", "DropRun", "DropRun", [](Condition &c) { c.drop_run = true; }, "1"},
            {ConditionField::DropValue, "drop_value", "DropValue", "DropValue",
             [](Condition &c) { c.drop_value = 9.5; }, "9.5"},
            {ConditionField::RoundQuantity, "round_quantity", "RoundQuantity", "RoundQuantity",
             [](Condition &c) { c.round_quantity = true; }, "1"},
            {ConditionField::RoundUp, "round_up", "RoundUp", "RoundUp", [](Condition &c) { c.round_up = 10.5; }, "10.5"},
            {ConditionField::Trim, "trim", "Trim", "Trim", [](Condition &c) { c.trim = true; }, "1"},
            {ConditionField::IsCurvedSegment, "is_curved_segment", "IsCurvedSegment", "IsCurvedSegment",
             [](Condition &c) { c.is_curved_segment = true; }, "1"},
            {ConditionField::Grid, "grid", "Grid", "Grid", [](Condition &c) { c.grid = true; }, "1"},
            {ConditionField::GridSize1, "grid_size1", "GridSize1", "GridSize1",
             [](Condition &c) { c.grid_size1 = 11.5; }, "11.5"},
            {ConditionField::GridSize2, "grid_size2", "GridSize2", "GridSize2",
             [](Condition &c) { c.grid_size2 = 12.5; }, "12.5"},
            {ConditionField::Gap, "gap", "Gap", "Gap", [](Condition &c) { c.gap = 13.5; }, "13.5"},
            {ConditionField::DisplayDimension, "display_dimension", "DisplayDimension", "DisplayDimension",
             [](Condition &c) { c.display_dimension = true; }, "1"},
            {ConditionField::DisplayName, "display_name", "DisplayName", "DisplayName",
             [](Condition &c) { c.display_name = true; }, "1"},
            {ConditionField::DisplayGridWhileDrawing, "display_grid_while_drawing", "DisplayGridWhileDrawing",
             "DisplayGridWhileDrawing", [](Condition &c) { c.display_grid_while_drawing = true; }, "1"},
            {ConditionField::FolderUid, "folder_uid", "BidConditionFolderUID", "BidConditionFolderUID",
             [](Condition &c) { c.condition_folder_uid = "40"; }, "40"},
            // iOS-only controls that stay by decision (COND-EXTRA-01): written like every other field, only when changed
            {ConditionField::Connect, "connect", "Connect", "Connect", [](Condition &c) { c.connect = true; }, "1"},
            {ConditionField::ConnectTolerance, "connect_tolerance", "ConnectTolerance", "ConnectTolerance",
             [](Condition &c) { c.connect_tolerance = 7.5; }, "7.5"},
            {ConditionField::SnapToLinear, "snap_to_linear", "SnapToLinear", "SnapToLinear",
             [](Condition &c) { c.snap_to_linear = 2; }, "2"},
        };
        return cases;
    }

    // The condition Windows' tests start from: every field at its default.
    Condition blank_condition()
    {
        Condition c;
        c.uid = "10";
        c.name = "Old";
        c.ref_no = 1;
        return c;
    }

    // before -> after exactly like the dialog does it (a field is in the change set when it differs).
    bool save(const TempDb &db, const std::string &bid, const std::string &uid, const Condition &before,
              const Condition &after)
    {
        return ost_store::update_condition(db.path(), bid, uid, ost_domain::collect_condition_changes(before, after));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// collect_condition_changes: the form-state side (DLG-SAVE-01, COND-M06)
// ---------------------------------------------------------------------------------------------------------------------

TEST(ConditionChangesSet, AnUnchangedConditionHasNoChanges)
{
    const Condition before = blank_condition();
    const ConditionChanges changes = ost_domain::collect_condition_changes(before, before);
    CHECK(changes.empty());
    CHECK_EQ(changes.fields.count(), size_t(0));
}

// Every field flips exactly its own bit: nothing is missing from the set and nothing leaks into another field.
TEST(ConditionChangesSet, EveryFieldFlipsExactlyItsOwnBit)
{
    REQUIRE_EQ(field_cases().size(), ost_domain::kConditionFieldCount);
    for (const FieldCase &fc : field_cases())
    {
        SCOPED_CONTEXT(std::string("field ") + fc.label);
        const Condition before = blank_condition();
        Condition after = before;
        fc.set(after);
        const ConditionChanges changes = ost_domain::collect_condition_changes(before, after);
        CHECK(changes.has(fc.field));
        CHECK_EQ(changes.fields.count(), size_t(1));
        CHECK_FALSE(changes.empty());
    }
}

// The edited values that are not persistable fields never make the form dirty: Windows' DTO may carry is_top and z_value,
// but update_condition ignores them; ColorLine, the condition type NAME and the uid are no fields either.
TEST(ConditionChangesSet, ValuesThatAreNotPersistableFieldsAreIgnored)
{
    const Condition before = blank_condition();
    Condition after = before;
    after.uid = "99";
    after.color_line = 12345;
    after.z_value = 6.0;
    after.is_top = true;
    after.cdn_type_name = "Whatever";
    CHECK(ost_domain::collect_condition_changes(before, after).empty());
}

// COND-M06: changing a field and changing it back leaves the form clean again (dirty is "form != saved state").
TEST(ConditionChangesSet, ChangingAFieldBackMakesTheFormCleanAgain)
{
    const Condition saved = blank_condition();
    Condition form = saved;
    form.name = "Renamed";
    form.height = 3.0;
    CHECK_FALSE(ost_domain::collect_condition_changes(saved, form).empty());
    form.name = saved.name;
    CHECK_FALSE(ost_domain::collect_condition_changes(saved, form).empty());
    form.height = saved.height;
    CHECK(ost_domain::collect_condition_changes(saved, form).empty());
}

// Windows compares with != (exact): a value that differs in the last bit is a change.
TEST(ConditionChangesSet, TheComparisonIsExact)
{
    Condition before = blank_condition();
    before.thickness = 4.25;
    Condition after = before;
    CHECK(ost_domain::collect_condition_changes(before, after).empty());
    after.thickness = std::nextafter(4.25, 5.0);
    CHECK(ost_domain::collect_condition_changes(before, after).has(ConditionField::Thickness));
}

TEST(ConditionChangesSet, AllConditionFieldsMarksEveryField)
{
    const ConditionChanges all = ost_domain::all_condition_fields(blank_condition());
    CHECK_EQ(all.fields.count(), ost_domain::kConditionFieldCount);
    for (const FieldCase &fc : field_cases())
        CHECK(all.has(fc.field));
}

// ---------------------------------------------------------------------------------------------------------------------
// update_condition: what is written (COND-UPD-02)
// ---------------------------------------------------------------------------------------------------------------------

// Port of ConditionUpdateContractTests.test_every_updatable_field_writes_its_own_column, strengthened: besides the
// field's own column, NO other column of the row (and no other row) changes.
TEST(ConditionUpdate, EveryUpdatableFieldWritesItsOwnColumnOnly)
{
    for (const FieldCase &fc : field_cases())
    {
        SCOPED_CONTEXT(std::string("field ") + fc.label);
        TempDb db(std::string("update_field_") + fc.label);
        REQUIRE(db.schema_ok());
        seed_update_world(db);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (11, 1, 2, 'Other', 1)");
        const std::string row_before = db.row("BidConditions", "10", {fc.column});
        const std::string other_before = db.row("BidConditions", "11");

        Condition after = blank_condition();
        fc.set(after);
        CHECK(save(db, "1", "10", blank_condition(), after));

        CHECK_EQ(db.text(std::string("SELECT quote(") + fc.expression + ") FROM BidConditions WHERE UID = 10"),
                 fc.expected);
        CHECK_EQ(db.row("BidConditions", "10", {fc.column}), row_before);
        CHECK_EQ(db.row("BidConditions", "11"), other_before);
    }
}

// A field that is NOT in the change set is never written, whatever the new-value carrier holds.
TEST(ConditionUpdate, ValuesOfUnmarkedFieldsAreIgnored)
{
    TempDb db("update_mask");
    REQUIRE(db.schema_ok());
    seed_update_world(db);
    const std::string before = db.row("BidConditions", "10", {"Height"});

    ConditionChanges changes;
    changes.values = blank_condition();
    changes.values.name = "Must not be written";
    changes.values.width = 99.0;
    changes.values.height = 2.0;
    changes.mark(ConditionField::Height);
    CHECK(ost_store::update_condition(db.path(), "1", "10", changes));

    CHECK_EQ(db.text("SELECT quote(Height) FROM BidConditions WHERE UID = 10"), "2.0");
    CHECK_EQ(db.row("BidConditions", "10", {"Height"}), before);
}

// COND-UPD-02 test plan: a row with NULL Width/Height/Depth, DisplaySize 0, Backout 1, ColorLine 111, ColorFill 222 and
// ConnectTolerance NULL; saving the condition loaded from it with only the name changed changes the name and nothing else
// (iOS wrote Width NULL->0.0, DisplaySize 0->100, Backout 1->-1, ConnectTolerance NULL->0.0).
TEST(ConditionUpdate, SavingOnlyTheNameKeepsEveryOtherColumnAndItsNullness)
{
    TempDb db("update_name_only");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (1, 1, 'Bid', 1, 0)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, DisplaySize, Backout, ColorLine, ColorFill) "
            "VALUES (10, 1, 1, 'Old name', 0, 0, 1, 111, 222)");
    const std::string row_before = db.row("BidConditions", "10", {"Name"});

    const ost_data::ProjectData project = ost_store::load_project(db.path(), "1");
    REQUIRE_EQ(project.conditions.count("10"), size_t(1));
    const Condition before = project.conditions.at("10");
    Condition after = before;
    after.name = "New name";

    CHECK(save(db, "1", "10", before, after));

    CHECK_EQ(db.text("SELECT Name FROM BidConditions WHERE UID = 10"), "New name");
    CHECK_EQ(db.row("BidConditions", "10", {"Name"}), row_before);
    CHECK_EQ(db.text("SELECT typeof(Width) || typeof(Height) || typeof(Depth) || typeof(ConnectTolerance) || "
                     "DisplaySize || Backout FROM BidConditions WHERE UID = 10"),
             "nullnullnullnull01");
}

// Changing the colour writes ColorFill only: ColorLine stays 111 even though the colour picker sets color_line too.
TEST(ConditionUpdate, ChangingTheColourWritesColorFillOnly)
{
    TempDb db("update_colour");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (1, 1, 'Bid', 1, 0)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, ColorLine, ColorFill) "
            "VALUES (10, 1, 1, 'Old name', 0, 111, 222)");
    const std::string row_before = db.row("BidConditions", "10", {"ColorFill"});

    const Condition before = ost_store::load_project(db.path(), "1").conditions.at("10");
    Condition after = before;
    after.color_fill = 333;
    after.color_line = after.color_fill; // what the colour picker's Done does in the controller

    const ConditionChanges changes = ost_domain::collect_condition_changes(before, after);
    CHECK(changes.has(ConditionField::ColorFill));
    CHECK_EQ(changes.fields.count(), size_t(1));
    CHECK(ost_store::update_condition(db.path(), "1", "10", changes));

    CHECK_EQ(db.count("SELECT ColorFill FROM BidConditions WHERE UID = 10"), 333);
    CHECK_EQ(db.count("SELECT ColorLine FROM BidConditions WHERE UID = 10"), 111);
    CHECK_EQ(db.row("BidConditions", "10", {"ColorFill"}), row_before);
}

// DBP-16 test plan: a row whose reference columns are NULL: a save with only the name changed keeps them NULL
// (typeof 'null'), and the layer stays the integer 50.
TEST(ConditionUpdate, ASaveOfTheNameKeepsNullReferencesNullAndTheLayerAnInteger)
{
    TempDb db("update_keeps_null");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (10, 1, 'Bid', 10, 0)");
    db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (50, 10, 'L', -1, 1)");
    db.must("INSERT INTO BidConditions (UID, BidUID, Name, Type, BidLayerUID, RefNo) VALUES (1, 10, 'Old', 0, 50, 1)");

    const Condition before = ost_store::load_project(db.path(), "10").conditions.at("1");
    Condition after = before;
    after.name = "New";
    CHECK(save(db, "10", "1", before, after));

    CHECK_EQ(db.text("SELECT typeof(CdnTypeUID) || ',' || typeof(BidConditionFolderUID) || ',' || "
                     "typeof(ConnectTolerance) || ',' || typeof(Quantity1) || ',' || typeof(Notes) || ',' || "
                     "typeof(BidLayerUID) FROM BidConditions WHERE UID = 1"),
             "null,null,null,null,null,integer");
    CHECK_EQ(db.count("SELECT BidLayerUID FROM BidConditions WHERE UID = 1"), 50);
}

// A save with no change must not write at all (Windows: an empty DTO is a success without a write): not even a database
// that refuses every UPDATE notices it.
TEST(ConditionUpdate, AnEmptyChangeSetSucceedsWithoutWriting)
{
    TempDb db("update_empty");
    REQUIRE(db.schema_ok());
    seed_update_world(db);
    db.block("BidConditions", "UPDATE");
    const std::string before = db.dump("BidConditions");

    const Condition unchanged = blank_condition();
    CHECK(save(db, "1", "10", unchanged, unchanged));
    CHECK(ost_store::update_condition(db.path(), "1", "10", ConditionChanges()));
    CHECK_EQ(db.dump("BidConditions"), before);
}

// Port of ConditionOperationsPersistenceTests.test_condition_update_without_persistable_changes_is_a_noop_success: the
// empty change set does not even look at the database (no Bids table here).
TEST(ConditionUpdate, AnEmptyChangeSetNeverTouchesTheSchema)
{
    TempDb db("update_empty_no_schema", false);
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, Name TEXT)");
    db.must("INSERT INTO BidConditions VALUES (50, 1, 'Original')");

    CHECK(ost_store::update_condition(db.path(), "1", "50", ConditionChanges()));
    CHECK_EQ(db.text("SELECT Name FROM BidConditions"), "Original");
}

// ---------------------------------------------------------------------------------------------------------------------
// update_condition: NULL for cleared values (COND-UPD-01)
// ---------------------------------------------------------------------------------------------------------------------

// Port of ConditionOperationsPersistenceTests.test_condition_update_persists_mapped_fields_and_clears_empty_relationships
// and the DBP-16 / COND-UPD-01 test plan.
TEST(ConditionUpdate, PersistsMappedFieldsAndClearsEmptyRelationshipsToNull)
{
    TempDb db("update_clear_relations");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
    db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (7, 1, 'L7', -1, 1)");
    db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (8, 1, 'L8', -1, 2)");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (9, 1, 'F9')");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (10, 1, 'F10')");
    for (const char *uid : {"50", "51"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, Name, Type, Backout, BidLayerUID, "
                            "BidConditionFolderUID, Notes, CdnTypeUID, UOM1) VALUES (") +
                uid + ", 1, 'Original', 1, 0, 7, 9, X'41', 4, 0)");
    const std::string untouched = db.row("BidConditions", "51");

    Condition before;
    before.uid = "50";
    before.name = "Original";
    before.condition_type = ost_domain::ConditionType::Area;
    before.layer_uid = "7";
    before.condition_folder_uid = "9";
    before.notes = "A";
    before.cdn_type_uid = "4";
    Condition after = before;
    after.name = "Renamed";
    after.backout = true;
    after.layer_uid = "8";
    after.condition_folder_uid = "10";
    after.notes = "note";
    after.uom1 = 13;
    CHECK(save(db, "1", "50", before, after));

    CHECK_EQ(db.text("SELECT Name || '|' || Backout || '|' || BidLayerUID || '|' || BidConditionFolderUID || '|' || "
                     "CAST(Notes AS TEXT) || '|' || CdnTypeUID || '|' || UOM1 FROM BidConditions WHERE UID = 50"),
             "Renamed|-1|8|10|note|4|13");
    CHECK_EQ(db.row("BidConditions", "51"), untouched);

    Condition cleared = after;
    cleared.layer_uid = "";
    cleared.condition_folder_uid = "";
    cleared.notes = "";
    cleared.cdn_type_uid = "";
    cleared.backout = false;
    CHECK(save(db, "1", "50", after, cleared));

    CHECK_EQ(db.text("SELECT Backout || '|' || typeof(BidLayerUID) || '|' || typeof(BidConditionFolderUID) || '|' || "
                     "typeof(Notes) || '|' || typeof(CdnTypeUID) FROM BidConditions WHERE UID = 50"),
             "0|null|null|null|null");
}

// Port of ConditionUpdateContractTests.test_clearing_a_reference_or_the_notes_writes_null_and_backout_false_writes_zero
TEST(ConditionUpdate, ClearingAReferenceOrTheNotesWritesNullAndBackoutFalseWritesZero)
{
    TempDb db("update_clear_all");
    REQUIRE(db.schema_ok());
    seed_update_world(db);
    db.must("UPDATE BidConditions SET CdnTypeUID = 7, BidLayerUID = 30, BidConditionFolderUID = 40, Notes = 'x', "
            "Backout = -1");

    Condition before = blank_condition();
    before.cdn_type_uid = "7";
    before.layer_uid = "30";
    before.condition_folder_uid = "40";
    before.notes = "x";
    before.backout = true;
    Condition after = blank_condition();
    CHECK(save(db, "1", "10", before, after));

    CHECK_EQ(db.text("SELECT typeof(CdnTypeUID) || ',' || typeof(BidLayerUID) || ',' || "
                     "typeof(BidConditionFolderUID) || ',' || typeof(Notes) || ',' || Backout "
                     "FROM BidConditions WHERE UID = 10"),
             "null,null,null,null,0");
}

// ---------------------------------------------------------------------------------------------------------------------
// update_condition: scoped to the bid (COND-M05)
// ---------------------------------------------------------------------------------------------------------------------

// COND-M05 test plan: condition 40 belongs to bid 2; an update under bid 1 returns false and leaves the row unchanged
// (iOS answered true and renamed it).
TEST(ConditionUpdate, ARenameOfAConditionOfAnotherBidIsRefused)
{
    TempDb db("update_other_bid");
    REQUIRE(db.schema_ok());
    seed_update_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (40, 2, 1, 'Theirs', 0)");
    const std::string before = db.dump("BidConditions");

    Condition after = blank_condition();
    after.name = "Hijacked";
    CHECK_FALSE(save(db, "1", "40", blank_condition(), after));
    CHECK_EQ(db.dump("BidConditions"), before);

    // Port of ConditionUpdateContractTests.test_updating_a_condition_of_another_bid_or_into_another_bids_layer_is_refused
    // (the third case: a condition of bid 1 updated under bid 2)
    Condition renamed = blank_condition();
    renamed.name = "N";
    CHECK_FALSE(save(db, "2", "10", blank_condition(), renamed));
    CHECK_EQ(db.dump("BidConditions"), before);
}

// Port of ConditionOperationsPersistenceTests.test_condition_update_rejects_cross_bid_relationships_atomically and
// ConditionUpdateContractTests.test_updating_a_condition_of_another_bid_or_into_another_bids_layer_is_refused:
// a layer or folder of another bid is rejected and NOTHING is written (the rename in the same change set included).
TEST(ConditionUpdate, ALayerOrFolderOfAnotherBidIsRefusedAtomically)
{
    struct Case
    {
        const char *layer;
        const char *folder;
    };
    for (const Case &c : {Case{"31", "40"}, Case{"30", "41"}, Case{"31", "41"}, Case{"31", ""}, Case{"", "41"}})
    {
        SCOPED_CONTEXT(std::string("layer ") + c.layer + ", folder " + c.folder);
        TempDb db("update_cross_bid_relations");
        REQUIRE(db.schema_ok());
        seed_update_world(db);
        db.must("UPDATE BidConditions SET BidLayerUID = 30, BidConditionFolderUID = 40 WHERE UID = 10");
        const std::string before = db.dump("BidConditions");

        Condition after = blank_condition();
        after.layer_uid = "30";
        after.condition_folder_uid = "40";
        Condition edited = after;
        edited.name = "Unexpected mutation";
        edited.layer_uid = c.layer;
        edited.condition_folder_uid = c.folder;
        // the stored condition holds layer 30 / folder 40; the edit moves it into the other bid's layer / folder
        Condition stored = blank_condition();
        stored.layer_uid = "30";
        stored.condition_folder_uid = "40";
        CHECK_FALSE(save(db, "1", "10", stored, edited));
        CHECK_EQ(db.dump("BidConditions"), before);
    }
}

// A layer or folder that does not exist (or whose uid is not a positive number) is refused like a foreign one; the
// same for a condition type uid that is not a number (Windows int(value) raises).
TEST(ConditionUpdate, UnknownOrMalformedReferencesAreRefused)
{
    struct Case
    {
        ConditionField field;
        const char *value;
    };
    const Case cases[] = {{ConditionField::LayerUid, "999"},        {ConditionField::LayerUid, "0"},
                          {ConditionField::LayerUid, "abc"},        {ConditionField::LayerUid, "-30"},
                          {ConditionField::FolderUid, "999"},       {ConditionField::FolderUid, "0"},
                          {ConditionField::FolderUid, "abc"},       {ConditionField::CdnTypeUid, "abc"},
                          {ConditionField::CdnTypeUid, "1.5"}};
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("value ") + c.value);
        TempDb db("update_bad_reference");
        REQUIRE(db.schema_ok());
        seed_update_world(db);
        const std::string before = db.dump("BidConditions");

        ConditionChanges changes;
        changes.values = blank_condition();
        changes.values.name = "Unexpected mutation";
        changes.mark(ConditionField::Name);
        if (c.field == ConditionField::LayerUid)
            changes.values.layer_uid = c.value;
        else if (c.field == ConditionField::FolderUid)
            changes.values.condition_folder_uid = c.value;
        else
            changes.values.cdn_type_uid = c.value;
        changes.mark(c.field);

        CHECK_FALSE(ost_store::update_condition(db.path(), "1", "10", changes));
        CHECK_EQ(db.dump("BidConditions"), before);
    }
}

// Port of ...::test_orphan_condition_edit_and_delete_reject_before_mutation (the edit part): a bid without a Bids row.
TEST(ConditionUpdate, ABidWithoutARowIsRefused)
{
    TempDb db("update_orphan_bid", false);
    db.must("CREATE TABLE Bids (UID INTEGER)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, Name TEXT, RefNo INTEGER)");
    db.must("INSERT INTO BidConditions VALUES (7, 99, 'Orphan condition', 4)");

    ConditionChanges changes;
    changes.values.name = "Changed";
    changes.mark(ConditionField::Name);
    CHECK_FALSE(ost_store::update_condition(db.path(), "99", "7", changes));
    CHECK_EQ(db.text("SELECT Name FROM BidConditions WHERE UID = 7"), "Orphan condition");
}

TEST(ConditionUpdate, UidsThatAreNotNumbersAreRefused)
{
    TempDb db("update_bad_uids");
    REQUIRE(db.schema_ok());
    seed_update_world(db);
    const std::string before = db.dump("BidConditions");
    ConditionChanges changes;
    changes.values.name = "Changed";
    changes.mark(ConditionField::Name);

    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "x", changes));
    CHECK_FALSE(ost_store::update_condition(db.path(), "x", "10", changes));
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "", changes));
    CHECK_FALSE(ost_store::update_condition(db.path(), "", "10", changes));
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "99", changes)); // no such condition
    CHECK_EQ(db.dump("BidConditions"), before);
}

// Port of ConditionUpdateContractTests.test_a_missing_column_is_reported_under_the_update_label_and_the_uid_columns_are_required:
// a column the schema lacks is skipped, the others are written; without the UID or BidUID column nothing can be updated.
TEST(ConditionUpdate, ASchemaWithoutAColumnSkipsItAndNeedsTheIdentityColumns)
{
    {
        TempDb db("update_missing_column", false);
        db.must("CREATE TABLE Bids (UID INTEGER)");
        db.must("INSERT INTO Bids VALUES (1)");
        db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, Name TEXT)");
        db.must("INSERT INTO BidConditions VALUES (10, 1, 'Old')");
        ConditionChanges changes;
        changes.values.name = "New";
        changes.values.height = 2.0;
        changes.mark(ConditionField::Name);
        changes.mark(ConditionField::Height);
        CHECK(ost_store::update_condition(db.path(), "1", "10", changes));
        CHECK_EQ(db.text("SELECT Name FROM BidConditions"), "New");
    }
    for (const char *dropped : {"UID", "BidUID"})
    {
        SCOPED_CONTEXT(std::string("without ") + dropped);
        TempDb db(std::string("update_missing_identity_") + dropped, false);
        db.must("CREATE TABLE Bids (UID INTEGER)");
        db.must("INSERT INTO Bids VALUES (1)");
        db.must(std::string("CREATE TABLE BidConditions (") + (std::string(dropped) == "UID" ? "BidUID INTEGER" : "UID INTEGER") +
                ", Name TEXT)");
        ConditionChanges changes;
        changes.values.name = "New";
        changes.mark(ConditionField::Name);
        CHECK_FALSE(ost_store::update_condition(db.path(), "1", "10", changes));
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// update_condition: the reference-number shift is part of the same transaction (Windows
// _shift_conflicting_ref_nos_for_update; Core side of COND-M03 / DBP-35)
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
    ConditionChanges ref_change(int ref_no)
    {
        ConditionChanges changes;
        changes.values.ref_no = ref_no;
        changes.mark(ConditionField::RefNo);
        return changes;
    }
}

// Port of ConditionOperationsPersistenceTests.test_condition_update_ref_no_shifts_only_conflicting_peers
TEST(ConditionUpdate, TakingATakenReferenceNumberShiftsOnlyTheConflictingPeers)
{
    TempDb db("update_refno_peers");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Bid', 2)");
    for (const char *row : {"(50, 1, 4)", "(51, 1, 1)", "(52, 1, 2)", "(53, 1, 3)", "(60, 2, 2)"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo) VALUES ") + row);

    CHECK(ost_store::update_condition(db.path(), "1", "50", ref_change(2)));
    CHECK_EQ(db.list("SELECT UID || ':' || RefNo FROM BidConditions ORDER BY UID"), "50:2,51:1,52:3,53:4,60:2");
}

// Port of ConditionUpdateContractTests.test_taking_a_reference_number_pushes_the_others_up
TEST(ConditionUpdate, TakingAReferenceNumberPushesTheOthersUp)
{
    TempDb db("update_refno_push");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Bid', 2)");
    for (const char *row : {"(10, 1, 1)", "(11, 1, 2)", "(12, 1, 3)", "(20, 2, 1)"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo) VALUES ") + row);

    CHECK(ost_store::update_condition(db.path(), "1", "12", ref_change(1)));
    CHECK_EQ(db.list("SELECT UID || ':' || RefNo FROM BidConditions ORDER BY UID"), "10:2,11:3,12:1,20:1");
}

// Port of ConditionUpdateContractTests.test_keeping_the_same_number_or_taking_a_free_one_shifts_nothing
TEST(ConditionUpdate, KeepingTheSameNumberOrTakingAFreeOneShiftsNothing)
{
    TempDb db("update_refno_free");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Bid', 2)");
    for (const char *row : {"(10, 1, 1)", "(11, 1, 2)", "(12, 1, 3)", "(20, 2, 1)"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo) VALUES ") + row);

    CHECK(ost_store::update_condition(db.path(), "1", "12", ref_change(3)));
    CHECK_EQ(db.list("SELECT UID || ':' || RefNo FROM BidConditions ORDER BY UID"), "10:1,11:2,12:3,20:1");
    CHECK(ost_store::update_condition(db.path(), "1", "12", ref_change(9)));
    CHECK_EQ(db.list("SELECT UID || ':' || RefNo FROM BidConditions ORDER BY UID"), "10:1,11:2,12:9,20:1");
}

// Port of ConditionEdgeContractTests.test_keeping_a_number_that_another_condition_also_holds_shifts_nothing
TEST(ConditionUpdate, KeepingANumberThatAnotherConditionAlsoHoldsShiftsNothing)
{
    TempDb db("update_refno_duplicate_numbers");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
    for (const char *row : {"(12, 1, 3)", "(13, 1, 3)", "(14, 1, 4)"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo) VALUES ") + row);

    CHECK(ost_store::update_condition(db.path(), "1", "12", ref_change(3)));
    CHECK_EQ(db.list("SELECT UID || ':' || RefNo FROM BidConditions ORDER BY UID"), "12:3,13:3,14:4");
}

// COND-M03 test plan (alias): refs 1,2,3 (a, b, c); a BEFORE UPDATE trigger aborts when Name = 'boom'. Updating c to ref 2
// together with Name 'boom' fails as a whole and the refs stay a=1, b=2, c=3; without the trigger c=2, b=3, a=1.
TEST(ConditionUpdate, TheRefNoShiftAndTheUpdateCommitOrRollBackTogether)
{
    TempDb db("update_refno_atomic");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
    for (const char *row : {"(21, 1, 1, 'a')", "(22, 1, 2, 'b')", "(23, 1, 3, 'c')"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo, Name) VALUES ") + row);
    db.must("CREATE TRIGGER boom BEFORE UPDATE ON BidConditions WHEN NEW.Name = 'boom' "
            "BEGIN SELECT RAISE(ABORT, 'boom'); END");

    ConditionChanges changes;
    changes.values.ref_no = 2;
    changes.values.name = "boom";
    changes.mark(ConditionField::RefNo);
    changes.mark(ConditionField::Name);
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "23", changes));
    CHECK_EQ(db.list("SELECT UID || ':' || RefNo || ':' || Name FROM BidConditions ORDER BY UID"),
             "21:1:a,22:2:b,23:3:c");

    db.must("DROP TRIGGER boom");
    CHECK(ost_store::update_condition(db.path(), "1", "23", changes));
    CHECK_EQ(db.list("SELECT UID || ':' || RefNo || ':' || Name FROM BidConditions ORDER BY UID"),
             "21:1:a,22:3:b,23:2:boom");
}

// The in-memory project follows the same rule as the database (ProjectData::shift_conflicting_ref_nos, called before the
// edited condition is stored back): for every condition and every new number the numbers of bid 1 in memory equal the
// numbers load_project reads after the database update.
TEST(ConditionUpdate, TheInMemoryRefNoShiftEqualsTheDatabaseShift)
{
    for (const char *uid : {"10", "11", "12", "13"})
    {
        for (int new_ref : {1, 2, 3, 4, 5, 6, 9})
        {
            SCOPED_CONTEXT(std::string("condition ") + uid + " takes number " + std::to_string(new_ref));
            TempDb db("update_refno_in_memory");
            REQUIRE(db.schema_ok());
            db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
            db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Bid', 2)");
            for (const char *row : {"(10, 1, 1)", "(11, 1, 2)", "(12, 1, 3)", "(13, 1, 5)", "(20, 2, 2)"})
                db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo) VALUES ") + row);

            ost_data::ProjectData in_memory = ost_store::load_project(db.path(), "1");
            REQUIRE_EQ(in_memory.conditions.size(), size_t(4));
            REQUIRE(ost_store::update_condition(db.path(), "1", uid, ref_change(new_ref)));
            in_memory.shift_conflicting_ref_nos(uid, new_ref);
            in_memory.conditions[uid].ref_no = new_ref;

            const ost_data::ProjectData reloaded = ost_store::load_project(db.path(), "1");
            REQUIRE_EQ(reloaded.conditions.size(), size_t(4));
            for (const auto &pair : reloaded.conditions)
                CHECK_EQ(in_memory.conditions.at(pair.first).ref_no, pair.second.ref_no);
        }
    }
}

// A condition that is not in the project, or that keeps its number, shifts nothing in memory.
TEST(ConditionUpdate, TheInMemoryRefNoShiftIgnoresAnUnknownConditionAndAnUnchangedNumber)
{
    ost_data::ProjectData project;
    for (int i = 1; i <= 3; ++i)
    {
        Condition c;
        c.uid = std::to_string(i);
        c.ref_no = i;
        project.conditions[c.uid] = c;
    }
    project.shift_conflicting_ref_nos("99", 1);
    project.shift_conflicting_ref_nos("2", 2);
    CHECK_EQ(project.conditions.at("1").ref_no, 1);
    CHECK_EQ(project.conditions.at("2").ref_no, 2);
    CHECK_EQ(project.conditions.at("3").ref_no, 3);

    // the free function (for a map that is not a ProjectData, e.g. the edit dialog's copy): condition 3 takes number 1; the
    // others from 1 up move up, the moving condition keeps its old number until the caller stores the edited one back
    ost_domain::shift_conflicting_ref_nos(project.conditions, "3", 1);
    CHECK_EQ(project.conditions.at("1").ref_no, 2);
    CHECK_EQ(project.conditions.at("2").ref_no, 3);
    CHECK_EQ(project.conditions.at("3").ref_no, 3);
}

// ---------------------------------------------------------------------------------------------------------------------
// DLG-FINITE-01 (the highest condition number) and COND-UPD-02 (a condition without a number): the reference-number shift at the
// edges of the number line (Windows b9902e6, condition_operations.py _shift_conflicting_ref_nos_for_update)
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
    constexpr int kHighestNumber = 2147483647;

    // the Windows texts for N = 2 and N = 3, written out so that a typo in the Core text cannot hide in both places
    constexpr const char *kOverflowForTwo =
        "Condition number 2 cannot be used: the conditions numbered 2 and up would move up by one, and one of them is already "
        "2147483647, the highest condition number.";
    constexpr const char *kOverflowForThree =
        "Condition number 3 cannot be used: the conditions numbered 3 and up would move up by one, and one of them is already "
        "2147483647, the highest condition number.";

    // bid 1: 10 -> 1, 11 -> 2, 12 -> 3; bid 2: 20 -> 1 (Windows test_condition_operations._numbered)
    void seed_numbered_world(const TempDb &db)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Bid', 2)");
        for (const char *row : {"(10, 1, 1)", "(11, 1, 2)", "(12, 1, 3)", "(20, 2, 1)"})
            db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo) VALUES ") + row);
    }

    // "uid:RefNo" of every condition, a NULL RefNo as 'NULL' (Windows _refs: None)
    std::string ref_list(const TempDb &db)
    {
        return db.list("SELECT UID || ':' || COALESCE(RefNo, 'NULL') FROM BidConditions ORDER BY UID");
    }

    ConditionChanges ref_and_name(int ref_no, const std::string &name)
    {
        ConditionChanges changes = ref_change(ref_no);
        changes.values.name = name;
        changes.mark(ConditionField::Name);
        return changes;
    }
}

// Port of ConditionOperationsPersistenceTests.test_a_renumber_whose_shift_would_pass_the_highest_number_changes_nothing: the shift
// from number 2 up would carry condition 12 (2147483647) to 2147483648, so the save fails before it writes anything (the new name of
// the same save included); a free number below the highest one needs no shift; taking the highest number from its holder is the same
// overflow; the holder itself can still move down (its own number is not part of the shift).
TEST(DlgFinite01_ConditionNumberLimit, ARenumberWhoseShiftWouldPassTheHighestNumberChangesNothing)
{
    TempDb db("limit_store_world");
    REQUIRE(db.schema_ok());
    seed_numbered_world(db);
    db.must("UPDATE BidConditions SET RefNo = 2147483647 WHERE UID = 12");
    db.must("UPDATE BidConditions SET Name = 'kept' WHERE UID = 10");
    const std::string before = db.dump("BidConditions");

    std::string error = "stale";
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "10", ref_and_name(2, "renamed"), &error));
    CHECK_STR_EQ(error, kOverflowForTwo);
    CHECK_STR_EQ(db.dump("BidConditions"), before);
    CHECK_STR_EQ(db.text("SELECT Name FROM BidConditions WHERE UID = 10"), "kept");

    error = "stale";
    CHECK(ost_store::update_condition(db.path(), "1", "10", ref_change(2147483646), &error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(ref_list(db), "10:2147483646,11:2,12:2147483647,20:1");

    error.clear();
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "11", ref_change(2147483647), &error));
    CHECK_STR_EQ(error, ost_domain::condition_number_overflow_message(2147483647));
    CHECK_STR_EQ(ref_list(db), "10:2147483646,11:2,12:2147483647,20:1");

    error = "stale";
    CHECK(ost_store::update_condition(db.path(), "1", "12", ref_change(2), &error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(ref_list(db), "10:2147483647,11:3,12:2,20:1");
}

// Port of UpdateConditionUseCaseTests.test_a_shift_past_the_highest_number_fails_through_the_use_case: bid 7 holds 1, 2, 3, 4 and
// bid 8 holds 3; condition 13 takes the highest number. Renumbering 11 to 3 fails with the text for N = 3 although numbers 5 up to
// 2147483646 are free (a gap hides nothing: the shift moves every number from 3 up) and every number stays as it was. The probe is
// scoped to the bid (WHERE [BidUID] = ?): the same holder in ANOTHER bid never blocks (inferred from the Windows SQL, not a Windows test).
TEST(DlgFinite01_ConditionNumberLimit, AGapBelowTheHighestNumberHidesNothingAndOnlyTheOwnBidCounts)
{
    TempDb db("limit_use_case_world");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (7, 1, 'Bid', 1)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (8, 1, 'Bid', 2)");
    for (const char *row : {"(10, 7, 1, 'a')", "(11, 7, 2, 'b')", "(12, 7, 3, 'c')", "(13, 7, 4, 'd')", "(20, 8, 3, 'other bid')"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo, Name) VALUES ") + row);
    db.must("UPDATE BidConditions SET RefNo = 2147483647 WHERE UID = 13");
    const std::string before = db.dump("BidConditions");

    std::string error;
    CHECK_FALSE(ost_store::update_condition(db.path(), "7", "11", ref_change(3), &error));
    CHECK_STR_EQ(error, kOverflowForThree);
    CHECK_STR_EQ(db.dump("BidConditions"), before);

    // the highest number now sits on condition 20 of the OTHER bid (and 13 is an ordinary number): bid 7's probe does not see it
    db.must("UPDATE BidConditions SET RefNo = 13 WHERE UID = 13");
    db.must("UPDATE BidConditions SET RefNo = 2147483647 WHERE UID = 20");
    error = "stale";
    CHECK(ost_store::update_condition(db.path(), "7", "11", ref_change(3), &error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(ref_list(db), "10:1,11:3,12:4,13:14,20:2147483647");
}

// Port of ConditionNumberLimitRealAccessTests._limits_through_real_access (the rows and the two saves; it runs on Access with the
// use case there): bid 11 holds 1, 2 and 2147483647, all named 'kept'. Renumbering 1 to 2 together with a new name fails with the text
// for N = 2 and changes nothing; renumbering 2 to 2147483646 (free) succeeds and renames.
TEST(DlgFinite01_ConditionNumberLimit, ThePortedRealAccessRowsFailOnceAndSucceedOnce)
{
    TempDb db("limit_real_access_rows");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (11, 1, 'B', 1)");
    for (const char *row : {"(1, 11, 1, 'kept')", "(2, 11, 2, 'kept')", "(3, 11, 2147483647, 'kept')"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo, Name) VALUES ") + row);
    const std::string rows = "SELECT UID || ':' || RefNo || ':' || Name FROM BidConditions ORDER BY UID";
    REQUIRE_EQ(db.list(rows), "1:1:kept,2:2:kept,3:2147483647:kept");

    std::string error;
    CHECK_FALSE(ost_store::update_condition(db.path(), "11", "1", ref_and_name(2, "renamed"), &error));
    CHECK_STR_EQ(error, kOverflowForTwo);
    CHECK_STR_EQ(db.list(rows), "1:1:kept,2:2:kept,3:2147483647:kept");

    CHECK(ost_store::update_condition(db.path(), "11", "2", ref_and_name(2147483646, "renamed"), &error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(db.list(rows), "1:1:kept,2:2147483646:renamed,3:2147483647:kept");
}

// Windows tests the probe with `>= 2147483647`, not `==`: a number a file already holds above the limit (an older iOS build wrote
// 2147483648 into the 64 bit column) blocks a shift that would move it up as well.
TEST(DlgFinite01_ConditionNumberLimit, ANumberAlreadyAboveTheHighestOneBlocksTheShiftToo)
{
    TempDb db("limit_above_limit");
    REQUIRE(db.schema_ok());
    seed_numbered_world(db);
    db.must("UPDATE BidConditions SET RefNo = 2147483648 WHERE UID = 12");
    const std::string before = db.dump("BidConditions");

    std::string error;
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "10", ref_change(2), &error));
    CHECK_STR_EQ(error, kOverflowForTwo);
    CHECK_STR_EQ(db.dump("BidConditions"), before);
}

// The failure has a channel of its own: the overflow comes back as its text, every other failure leaves `error` empty (the dialog
// then shows the generic text), a success and an empty change set clear a text left over from before, and a caller without an
// `error` still gets false (the ~40 other call sites).
TEST(DlgFinite01_ConditionNumberLimit, TheOverflowIsToldApartFromAnOrdinaryFailure)
{
    TempDb db("limit_error_channel");
    REQUIRE(db.schema_ok());
    seed_numbered_world(db);
    db.must("UPDATE BidConditions SET RefNo = 2147483647 WHERE UID = 12");

    std::string error = "stale";
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "10", ref_change(2), &error));
    CHECK_STR_EQ(error, kOverflowForTwo);
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "10", ref_change(2))); // no `error`: only the bool

    // an unknown condition, a uid that is no number and a write the database refuses: false with NO text
    ConditionChanges rename;
    rename.values.name = "Renamed";
    rename.mark(ConditionField::Name);
    error = "stale";
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "99", rename, &error));
    CHECK_STR_EQ(error, "");
    error = "stale";
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "x", rename, &error));
    CHECK_STR_EQ(error, "");
    const std::string trigger = db.block("BidConditions", "UPDATE");
    error = "stale";
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "10", rename, &error));
    CHECK_STR_EQ(error, "");
    // the overflow is found before the first write, so it is the overflow even where every write would fail
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "10", ref_change(2), &error));
    CHECK_STR_EQ(error, kOverflowForTwo);
    db.unblock(trigger);

    error = "stale";
    CHECK(ost_store::update_condition(db.path(), "1", "10", rename, &error));
    CHECK_STR_EQ(error, "");
    error = "stale";
    CHECK(ost_store::update_condition(db.path(), "1", "10", ConditionChanges(), &error)); // an empty change set
    CHECK_STR_EQ(error, "");
}

// The in-memory twin follows the store: the same renumber is refused (nothing changes, no int overflows) or shifts like the database
// did. Matrix over every condition of a world whose condition 13 holds the highest number and every number that is taken, free
// or the highest one.
TEST(DlgFinite01_ConditionNumberLimit, TheInMemoryShiftRefusesWhatTheStoreRefusesAndEqualsItOtherwise)
{
    for (const char *uid : {"10", "11", "12", "13"})
    {
        for (int new_ref : {1, 2, 3, 4, 2147483646, 2147483647})
        {
            SCOPED_CONTEXT(std::string("condition ") + uid + " takes number " + std::to_string(new_ref));
            TempDb db("limit_in_memory");
            REQUIRE(db.schema_ok());
            db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
            db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Bid', 2)");
            for (const char *row : {"(10, 1, 1)", "(11, 1, 2)", "(12, 1, 3)", "(13, 1, 2147483647)", "(20, 2, 2)"})
                db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo) VALUES ") + row);

            ost_data::ProjectData in_memory = ost_store::load_project(db.path(), "1");
            REQUIRE_EQ(in_memory.conditions.size(), size_t(4));
            const std::string before = db.dump("BidConditions");
            std::string error;
            const bool stored = ost_store::update_condition(db.path(), "1", uid, ref_change(new_ref), &error);
            const bool shifted = ost_domain::shift_conflicting_ref_nos(in_memory.conditions, uid, new_ref);
            CHECK(shifted == stored);
            if (stored)
            {
                CHECK_STR_EQ(error, "");
                in_memory.conditions[uid].ref_no = new_ref;
            }
            else
            {
                CHECK_STR_EQ(error, ost_domain::condition_number_overflow_message(new_ref));
                CHECK_STR_EQ(db.dump("BidConditions"), before);
            }

            const ost_data::ProjectData reloaded = ost_store::load_project(db.path(), "1");
            REQUIRE_EQ(reloaded.conditions.size(), size_t(4));
            for (const auto &pair : reloaded.conditions)
                CHECK_EQ(in_memory.conditions.at(pair.first).ref_no, pair.second.ref_no);
        }
    }
}

// The same rule on a bare map, where an int would overflow: nothing moves when a condition that would be shifted already holds the
// highest number (the call says so with false); the holder of that number moves down (it is not shifted itself); a free number and an
// unknown condition are not refusals.
TEST(DlgFinite01_ConditionNumberLimit, TheInMemoryShiftNeverPushesAConditionPastTheHighestNumber)
{
    std::unordered_map<std::string, Condition> conditions;
    const std::vector<std::pair<std::string, int>> numbers = {{"1", 1}, {"2", 2}, {"3", kHighestNumber}};
    for (const auto &entry : numbers)
    {
        Condition c;
        c.uid = entry.first;
        c.ref_no = entry.second;
        conditions[c.uid] = c;
    }

    CHECK_FALSE(ost_domain::shift_conflicting_ref_nos(conditions, "1", 2));
    CHECK_EQ(conditions.at("1").ref_no, 1);
    CHECK_EQ(conditions.at("2").ref_no, 2);
    CHECK_EQ(conditions.at("3").ref_no, kHighestNumber);
    CHECK_FALSE(ost_domain::shift_conflicting_ref_nos(conditions, "2", kHighestNumber));
    CHECK_EQ(conditions.at("2").ref_no, 2);
    CHECK_EQ(conditions.at("3").ref_no, kHighestNumber);

    CHECK(ost_domain::shift_conflicting_ref_nos(conditions, "3", 2)); // the holder moves down: 2 moves up, 3 is left to the caller
    CHECK_EQ(conditions.at("1").ref_no, 1);
    CHECK_EQ(conditions.at("2").ref_no, 3);
    CHECK_EQ(conditions.at("3").ref_no, kHighestNumber);

    CHECK(ost_domain::shift_conflicting_ref_nos(conditions, "1", 5));  // free: no shift, no refusal
    CHECK(ost_domain::shift_conflicting_ref_nos(conditions, "99", 2)); // not in the map: nothing to shift
    CHECK_EQ(conditions.at("2").ref_no, 3);
}

// Port of ConditionOperationsPersistenceTests.test_a_condition_without_a_reference_number_takes_one_like_any_other (COND-UPD-02, a PIN:
// Windows b9902e6 made a NULL RefNo "different from the new number", the iOS store always did): conditions 13 and 14 have no number.
// 13 takes 9, which nobody holds: no shift and 14 stays NULL. 14 takes 2, which 11 holds: the normal conflict check runs, 11, 12 and 13
// move up by one, bid 2 is untouched and no NULL row is shifted (here none is left to see; see the next test).
TEST(CondUpd02_NullRefNo, AConditionWithoutAReferenceNumberTakesOneLikeAnyOther)
{
    TempDb db("null_refno_takes_one");
    REQUIRE(db.schema_ok());
    seed_numbered_world(db);
    for (const char *uid : {"13", "14"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (") + uid + ", 1, NULL, 'c', 0)");
    REQUIRE_EQ(ref_list(db), "10:1,11:2,12:3,13:NULL,14:NULL,20:1");
    REQUIRE_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 13"), "null");

    CHECK(ost_store::update_condition(db.path(), "1", "13", ref_change(9)));
    CHECK_STR_EQ(ref_list(db), "10:1,11:2,12:3,13:9,14:NULL,20:1");
    CHECK_STR_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 14"), "null");

    CHECK(ost_store::update_condition(db.path(), "1", "14", ref_change(2)));
    CHECK_STR_EQ(ref_list(db), "10:1,11:3,12:4,13:10,14:2,20:1");
    CHECK_STR_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 14"), "integer");
}

// COND-UPD-02, the NULL guard of the "same number" early return: a NULL RefNo is "different from the new number" even when the new
// number is 0 (Windows b9902e6 `current_row[0] is not None and int(current_row[0]) == new_ref_no`); a store that read NULL as 0 would
// return early here. Conditions 13 (RefNo NULL) and 14 (RefNo 0) beside the usual world; 13 takes 0. The expected rows come from
// reading the Windows SQL (condition_operations.py :614-655, the statements also run on an in-memory sqlite copy of the rows): the
// current value is NULL, so the early return does not fire; the conflict probe (BidUID = 1, RefNo = 0, UID <> 13) finds 14; MAX(RefNo)
// of the rows from 0 up (10, 11, 12, 14) is 3, no overflow; every row of the bid with RefNo >= 0 except 13 moves up (10 to 2, 11 to 3,
// 12 to 4, 14 to 1; 13 is NULL and not part of it); then 13 is written as 0; bid 2 is never touched. An early return would leave
// '10:1,11:2,12:3,13:0,14:0,20:1' (two conditions numbered 0).
TEST(CondUpd02_NullRefNo, ANullReferenceNumberIsDifferentFromTheNewNumberZero)
{
    TempDb db("null_refno_takes_zero");
    REQUIRE(db.schema_ok());
    seed_numbered_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (13, 1, NULL, 'c', 0)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (14, 1, 0, 'c', 0)");
    REQUIRE_EQ(ref_list(db), "10:1,11:2,12:3,13:NULL,14:0,20:1");
    REQUIRE_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 13"), "null");

    CHECK(ost_store::update_condition(db.path(), "1", "13", ref_change(0)));
    CHECK_STR_EQ(ref_list(db), "10:2,11:3,12:4,13:0,14:1,20:1");
    CHECK_STR_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 13"), "integer");

    // the same number again is the early return: 13 holds 0 now, nothing moves
    CHECK(ost_store::update_condition(db.path(), "1", "13", ref_change(0)));
    CHECK_STR_EQ(ref_list(db), "10:2,11:3,12:4,13:0,14:1,20:1");
}

// COND-UPD-02: a shift that DOES happen never touches a row without a number (NULL >= n is not true and NULL + 1 stays NULL), and a NULL
// row is no conflict either: renumbering 12 to 1 moves 10 and 11 up, and the two NULL rows keep their storage class NULL.
TEST(CondUpd02_NullRefNo, ARowWithoutAReferenceNumberIsNeverShifted)
{
    TempDb db("null_refno_never_shifted");
    REQUIRE(db.schema_ok());
    seed_numbered_world(db);
    for (const char *uid : {"15", "16"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (") + uid + ", 1, NULL, 'c', 0)");

    CHECK(ost_store::update_condition(db.path(), "1", "12", ref_change(1)));
    CHECK_STR_EQ(ref_list(db), "10:2,11:3,12:1,15:NULL,16:NULL,20:1");
    CHECK_STR_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 15"), "null");
    CHECK_STR_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 16"), "null");

    // a number no one holds is free although two rows hold no number at all: nothing is shifted
    CHECK(ost_store::update_condition(db.path(), "1", "10", ref_change(7)));
    CHECK_STR_EQ(ref_list(db), "10:7,11:3,12:1,15:NULL,16:NULL,20:1");
}

// COND-UPD-02, the in-memory counterpart: a stored NULL reads as 0 (the dialog shows "0"), the shift leaves such a condition at 0 like the
// database leaves the row NULL, and the project in memory equals a reload of the database after the same renumber.
TEST(CondUpd02_NullRefNo, TheInMemoryShiftReadsNullAsZeroAndLeavesItAloneLikeTheDatabase)
{
    TempDb db("null_refno_in_memory");
    REQUIRE(db.schema_ok());
    seed_numbered_world(db);
    for (const char *uid : {"13", "14"})
        db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (") + uid + ", 1, NULL, 'c', 0)");

    ost_data::ProjectData in_memory = ost_store::load_project(db.path(), "1");
    REQUIRE_EQ(in_memory.conditions.size(), size_t(5));
    CHECK_EQ(in_memory.conditions.at("13").ref_no, 0);
    CHECK_EQ(in_memory.conditions.at("14").ref_no, 0);

    REQUIRE(ost_store::update_condition(db.path(), "1", "14", ref_change(2)));
    in_memory.shift_conflicting_ref_nos("14", 2);
    in_memory.conditions["14"].ref_no = 2;

    const ost_data::ProjectData reloaded = ost_store::load_project(db.path(), "1");
    REQUIRE_EQ(reloaded.conditions.size(), size_t(5));
    for (const auto &pair : reloaded.conditions)
        CHECK_EQ(in_memory.conditions.at(pair.first).ref_no, pair.second.ref_no);
    CHECK_EQ(in_memory.conditions.at("10").ref_no, 1);
    CHECK_EQ(in_memory.conditions.at("11").ref_no, 3);
    CHECK_EQ(in_memory.conditions.at("12").ref_no, 4);
    CHECK_EQ(in_memory.conditions.at("13").ref_no, 0); // still without a number, in memory and in the file
    CHECK_STR_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 13"), "null");
}

// ---------------------------------------------------------------------------------------------------------------------
// update_condition: a failed write is reported (COND-UPD-03)
// ---------------------------------------------------------------------------------------------------------------------

// COND-UPD-03 test plan: a BEFORE UPDATE trigger that RAISE(ABORT)s on BidConditions: update_condition returns false
// (iOS returned true after the prepare succeeded) and the row is unchanged.
TEST(ConditionUpdate, AFailedWriteIsReportedAndLeavesTheRowUnchanged)
{
    TempDb db("update_failure");
    REQUIRE(db.schema_ok());
    seed_update_world(db);
    const std::string trigger = db.block("BidConditions", "UPDATE");
    const std::string before = db.dump("BidConditions");

    Condition after = blank_condition();
    after.name = "Renamed";
    CHECK_FALSE(save(db, "1", "10", blank_condition(), after));
    CHECK_EQ(db.dump("BidConditions"), before);

    db.unblock(trigger);
    CHECK(save(db, "1", "10", blank_condition(), after));
    CHECK_EQ(db.text("SELECT Name FROM BidConditions WHERE UID = 10"), "Renamed");
}

// A database that cannot be opened is a failure, not a success.
TEST(ConditionUpdate, ADatabaseThatCannotBeOpenedFails)
{
    ConditionChanges changes;
    changes.values.name = "Renamed";
    changes.mark(ConditionField::Name);
    CHECK_FALSE(ost_store::update_condition(ost_test::temp_path("no_such_dir/none.db"), "1", "10", changes));
}

// ---------------------------------------------------------------------------------------------------------------------
// the whole condition through the bid-scoped update (the legacy whole-row entry point is gone, COND-M05)
// ---------------------------------------------------------------------------------------------------------------------

// Every field marked (ost_domain::all_condition_fields) still writes a complete condition, with NULL for an empty reference or note
// and no ColorLine; an unknown condition is a failure. (Was ConditionUpdateLegacy.WritesTheWholeConditionWithNullForEmptyValues
// AndNeverColorLine through ost_store::update_condition(db_path, Condition).)
TEST(ConditionUpdate, AFullChangeSetWritesTheWholeConditionWithNullForEmptyValuesAndNeverColorLine)
{
    TempDb db("update_full_set");
    REQUIRE(db.schema_ok());
    seed_update_world(db);
    db.must("UPDATE BidConditions SET ColorLine = 111, CdnTypeUID = 7, Notes = 'x' WHERE UID = 10");

    Condition edited = blank_condition();
    edited.name = "Full";
    edited.color_fill = 5;
    edited.color_line = 5;
    CHECK(ost_store::update_condition(db.path(), "1", "10", ost_domain::all_condition_fields(edited)));

    CHECK_EQ(db.text("SELECT Name || '|' || ColorFill || '|' || ColorLine || '|' || typeof(CdnTypeUID) || '|' || "
                     "typeof(Notes) || '|' || typeof(BidLayerUID) FROM BidConditions WHERE UID = 10"),
             "Full|5|111|null|null|null");

    Condition unknown = blank_condition();
    unknown.uid = "999";
    CHECK_FALSE(ost_store::update_condition(db.path(), "1", "999", ost_domain::all_condition_fields(unknown)));
}

// Windows validates only what an edit changes (update_condition checks a layer or folder only when the DTO carries it). The change
// set the dialog builds (collect_condition_changes) leaves an unchanged layer or folder out, so a stored layer or folder that no
// longer resolves (a dangling or zero reference an older file may hold) never blocks the save of that condition; moving the
// condition into another bid's layer is still refused. (Was ConditionUpdateLegacy.AStoredReferenceThatDoesNotResolveNeverBlocksTheSave.)
TEST(ConditionUpdate, AStoredReferenceThatDoesNotResolveNeverBlocksTheSave)
{
    TempDb db("update_dangling");
    REQUIRE(db.schema_ok());
    seed_update_world(db);
    db.must("UPDATE BidConditions SET BidLayerUID = 999, BidConditionFolderUID = 0 WHERE UID = 10");

    Condition stored = blank_condition();
    stored.layer_uid = "999";
    stored.condition_folder_uid = "0";
    Condition edited = stored;
    edited.name = "Saved";
    CHECK(save(db, "1", "10", stored, edited));
    CHECK_EQ(db.text("SELECT Name || '|' || BidLayerUID || '|' || BidConditionFolderUID FROM BidConditions WHERE UID = 10"),
             "Saved|999|0");

    Condition moved = edited;
    moved.name = "Moved";
    moved.layer_uid = "31"; // a layer of bid 2
    CHECK_FALSE(save(db, "1", "10", edited, moved));
    CHECK_EQ(db.text("SELECT Name || '|' || BidLayerUID FROM BidConditions WHERE UID = 10"), "Saved|999");
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-M08 (Windows 49a2d59, edit_condition_dialog.py): a condition whose stored Type is outside 0-3
// (7, -1) opens with that type kept (_load_condition :839-847) and a Save writes only what changed: a notes-only edit writes only the
// Notes column and leaves the Type column, whatever it holds, as it was (:1464-1474); choosing a real style writes the Type and the
// defaults of the style. The whole path from the row in the database: load_project, the form of the editor, the change set of a
// Save, update_condition.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
    // bid 1 with the condition of a stored type outside 0-3 (10) and a neighbour of a real type (11). Every UOM is -1 (a stored
    // NULL unit loads as 0 and condition_to_save writes -1 for a slot without a quantity, which has nothing to do with the type);
    // SnapToLinear -1 is the value of a condition the app wrote (a NULL loads as 0, which the Area style resets to -1). The row has
    // Round Quantity on with Round to 0.5: a Linear / Area would carry that over a style change, "Unknown (N)" carries nothing.
    void seed_unknown_type_world(const TempDb &db, int type)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (1, 1, 'Bid', 1, 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, Thickness, Height, Spacing, UOM1, UOM2, UOM3, "
                "SnapToLinear, RoundQuantity, RoundUp, Notes) VALUES (10, 1, 1, 'Odd', " +
                std::to_string(type) + ", 4.0, 8.0, 2.5, -1, -1, -1, -1, -1, 0.5, 'old')");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, Thickness, Height, Spacing, UOM1, UOM2, UOM3, "
                "SnapToLinear, RoundQuantity, RoundUp, Notes) VALUES (11, 1, 2, 'Next', 1, 4.0, 0.0, 0.0, -1, -1, -1, -1, 0, 0.0, 'keep')");
    }
}

// test_editing_only_the_notes_saves_only_the_notes: the row of the condition changes in the Notes column only (the Type column
// still holds 7 / -1 as an integer), the neighbour is untouched. Run in an imperial and in a metric bid (the bid's MeasureBase and the
// system the change set is built for agree): the system takes part in the change set (a style change reads the metric defaults), and
// a notes-only edit must write the Notes column alone in both.
TEST(CondM08_UnknownStyle, ANotesOnlySaveWritesOnlyTheNotesColumn)
{
    for (const bool metric : {false, true})
    {
        for (int type : {7, -1})
        {
            SCOPED_CONTEXT("stored type " + std::to_string(type) + (metric ? " metric bid" : " imperial bid"));
            TempDb db(std::string(type < 0 ? "m08_notes_minus1" : "m08_notes_plus7") + (metric ? "_metric" : "_imperial"));
            REQUIRE(db.schema_ok());
            seed_unknown_type_world(db, type);
            if (metric)
                db.must("UPDATE Bids SET MeasureBase = 1 WHERE UID = 1"); // bid 1 is metric (the fixture's default is imperial)
            const std::string row_before = db.row("BidConditions", "10", {"Notes"});
            const std::string neighbour_before = db.row("BidConditions", "11");

            const Condition stored = ost_store::load_project(db.path(), "1").conditions.at("10");
            CHECK(stored.condition_type == static_cast<ost_domain::ConditionType>(type)); // the loader keeps the raw value
            Condition form = ost_domain::condition_form_for_edit(stored);
            form.notes = "edited";
            const ConditionChanges changes = ost_domain::condition_changes_for_save(stored, form, metric);
            CHECK_EQ(changes.fields.count(), size_t(1));
            CHECK(changes.has(ConditionField::Notes));
            CHECK(ost_store::update_condition(db.path(), "1", "10", changes));

            CHECK_EQ(db.row("BidConditions", "10", {"Notes"}), row_before);
            CHECK_EQ(db.text("SELECT CAST(Notes AS TEXT) FROM BidConditions WHERE UID = 10"), "edited");
            CHECK_EQ(db.text("SELECT Type || ':' || typeof(Type) FROM BidConditions WHERE UID = 10"), std::to_string(type) + ":integer");
            CHECK_EQ(db.row("BidConditions", "11"), neighbour_before);
        }
    }
}

// test_picking_a_real_type_still_changes_the_type: Unknown (7) -> Area writes Type 1 and the defaults of the Area style, and no
// other column of the row changes. The stored Round Quantity / Round to are NOT carried from an unknown style (Windows
// _shared_options(7) is {}), so the Area style writes them back to its defaults.
TEST(CondM08_UnknownStyle, ChoosingARealTypeWritesTheTypeColumnAndItsDefaults)
{
    TempDb db("m08_pick_area");
    REQUIRE(db.schema_ok());
    seed_unknown_type_world(db, 7);
    const std::vector<std::string> written = {"Type", "Thickness", "Backout", "Spacing", "Quantity1", "UOM1",
                                              "DisplayGridWhileDrawing", "RoundQuantity", "RoundUp"};
    const std::string row_before = db.row("BidConditions", "10", written);

    const Condition stored = ost_store::load_project(db.path(), "1").conditions.at("10");
    const Condition form =
        ost_domain::apply_style_change(ost_domain::condition_form_for_edit(stored), ost_domain::ConditionType::Area, false).condition;
    const ConditionChanges changes = ost_domain::condition_changes_for_save(stored, form, false);
    CHECK(changes.has(ConditionField::ConditionType));
    CHECK(ost_store::update_condition(db.path(), "1", "10", changes));

    CHECK_EQ(db.text("SELECT Type || ':' || typeof(Type) FROM BidConditions WHERE UID = 10"), "1:integer");
    CHECK_EQ(db.text("SELECT quote(Backout) FROM BidConditions WHERE UID = 10"), "0");
    CHECK_EQ(db.text("SELECT quote(Thickness) FROM BidConditions WHERE UID = 10"), "4.0");
    CHECK_EQ(db.text("SELECT quote(Spacing) FROM BidConditions WHERE UID = 10"), "2.5"); // the field's value, written on a type change
    CHECK_EQ(db.text("SELECT quote(Quantity1) FROM BidConditions WHERE UID = 10"), "11");
    CHECK_EQ(db.text("SELECT quote(UOM1) FROM BidConditions WHERE UID = 10"), "5");
    CHECK_EQ(db.text("SELECT quote(DisplayGridWhileDrawing) FROM BidConditions WHERE UID = 10"), "1");
    CHECK_EQ(db.text("SELECT quote(RoundQuantity) FROM BidConditions WHERE UID = 10"), "0");
    CHECK_EQ(db.text("SELECT quote(RoundUp) FROM BidConditions WHERE UID = 10"), "0.0");
    CHECK_EQ(db.text("SELECT CAST(Notes AS TEXT) FROM BidConditions WHERE UID = 10"), "old");
    // nothing else of the row changed
    CHECK_EQ(db.row("BidConditions", "10", written), row_before);
}
