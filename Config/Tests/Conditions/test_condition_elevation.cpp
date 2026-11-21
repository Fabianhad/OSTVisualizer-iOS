// COND-ELEV-01 (decision of 2026-11-19; plain parity with Windows since b9902e6, kept as a pin): after a Save of the Edit Condition
// dialog the in-memory condition gets is_top and z_value DERIVED FROM THE SAVED NAME with the derivation the loader uses, so
// that memory equals what a reload shows. Before, condition_to_save copied what the dialog held (is_top from the Top / Bottom
// segment, z from the elevation text): Top is the default segment, so a condition without an elevation was is_top = true in
// memory and false / 0.0 after a reload, and the dialog validated the elevation with display_to_inches, which reads texts the
// loader grammar does not ("3200" in a metric bid, "10' 6.3\"": z 125.98 / 126.3 in the dialog, 0.0 / false after a reload), so the
// 3D position of the condition differed until the bid was reopened.
// Since Windows b9902e6 the dialog validates the elevation with the loader's own parser (elevation_text_inches: see
// test_condition_elevation_dialog.cpp), so a text it accepts already has the number a reload gives. The derivation stays: a blank
// elevation with the Top segment is is_top = true in the form but false after a reload (Windows reloads after the Save anyway), and
// a name can be typed whole into the Name field, whatever it contains.
//
// Windows: infrastructure/parsers/position_parser.py:23-33 extract_z_value_from_name (= parse_elevation + elevation_text_inches,
// domain/services/elevation_text.py:131-144), read by infrastructure/mdb/components/bid_data_reader.py:896-906 (an unreadable
// name, a ValueError, stores (0.0, False));
// presentation/dialogs/edit_condition_dialog.py:1423-1457 puts the dialog's is_top / z_value into the DTO, but
// infrastructure/mdb/components/condition_operations.py update_condition writes only the Name column (is_top and z_value are no
// columns) and Windows shows the reloaded condition. Every expected value below was computed by running the Windows
// extract_z_value_from_name (Python 3.11.4, python -B) on the name, or is a row of the Windows oracle table
// Parsers/ElevationOracleData.hpp.
#include "ConditionsTestSupport.hpp"

#include "../Parsers/ElevationOracleData.hpp"

#include "Domain/ConditionChanges.hpp"
#include "Domain/ConditionFormRules.hpp"
#include "Domain/ConditionStyleChange.hpp"
#include "Domain/PositionParser.hpp"
#include "Import/TextRules.hpp"
#include "Persistence/ProjectStore/ProjectStore_Internal.hpp"

#include <cmath>
#include <string>
#include <vector>

using ost_ctest::TempDb;
using ost_domain::Condition;
using ost_domain::ConditionType;

namespace
{
    struct NameCase
    {
        const char *name;
        double z;     // Windows extract_z_value_from_name(name)[0], or 0.0 when it raises (the reader stores (0.0, False))
        bool is_top;  // [1]
        const char *why;
    };

    const NameCase kNames[] = {
        {"Wall", 0.0, false, "no suffix: the Top segment is the default of the dialog, a reload gives false / 0.0"},
        {"Wall @T 12' 6 1/2\"", 150.5, true, "feet, inches and a fraction at the top"},
        {"Wall @B 5' 0\"", 60.0, false, "bottom"},
        {"Wall @ 12' 0\"", 144.0, false, "the legacy bare marker is a bottom"},
        {"Wall @T 3200", 0.0, false, "a bare number: the loader grammar needs a unit (the dialog refuses it: only typed whole into the Name)"},
        {"Wall @B 3200", 0.0, false, "the same at the bottom"},
        {"Wall @T 3200 mm", 0.0, false, "a unit the grammar does not know"},
        {"Wall @T 3.2 m", 125.98425196850394, true, "metres"},
        {"Wall @T 1m 20cm", 47.24409448818898, true, "metres and centimetres"},
        {"Wall @T 30 cm", 11.811023622047244, true, "centimetres"},
        {"Wall @B 1/0\"", 0.0, false, "an unreadable fraction: Windows raises ValueError, the reader stores (0.0, False)"},
        {"Wall @T 1/0\"", 0.0, false, "the same at the top: not is_top"},
        {"Wall @T abc", 0.0, false, "text outside the grammar"},
        {"Wall @T", 0.0, false, "a marker without a value"},
        {"Wall @T 10'6", 120.0, true, "feet only, 120.0: the dialog reads it the same way since b9902e6 (display_to_inches gave 126.0)"},
        {"Wall @T 10' 6.3\"", 0.0, false, "the loader grammar has no decimal inches (the dialog refuses it; display_to_inches gave 126.3)"},
        {"Wall @B 1' 2 3/4\"", 14.75, false, "a mixed fraction"},
        {"Wall @T -3 1/2\"", -3.5, true, "a negative elevation"},
        {"Wall @T 5'x", 60.0, true, "trailing text after feet"},
        {"Wall @B 6\"", 6.0, false, "inches only"},
    };

    Condition stored_condition(const std::string &name = "Old")
    {
        Condition c;
        c.uid = "10";
        c.name = name;
        c.condition_type = ConditionType::Linear;
        c.ref_no = 1;
        c.uom1 = c.uom2 = c.uom3 = -1;
        return c;
    }

    // The form the dialog builds for a condition named `name` (_validateAndCollectTextFields): is_top from the Top / Bottom
    // segment (parse_elevation: no suffix is Top, the default segment), z from the elevation text (parse_elevation_z, which reads it
    // with the loader's parser). true when the name came from the elevation controls. false when the dialog refuses that elevation
    // text (one the loader cannot read): the name is then taken as typed whole into the Name field with an empty elevation (Top
    // segment, z 0.0); every name can be reached that way, because a base name may itself contain " @T 5' 0\"".
    bool dialog_form(const Condition &stored, const std::string &name, bool metric, Condition &form)
    {
        const ost_domain::ElevationParts parts = ost_domain::parse_elevation(name);
        form = stored;
        form.name = name;
        form.is_top = (parts.type == 0);
        if (!ost_domain::parse_elevation_z(ost_import::text_rules::py_strip(parts.value), metric, form.z_value).has_value())
            return true;
        form.is_top = true; // the Top segment is the default of the dialog
        form.z_value = 0.0; // the empty elevation field
        return false;
    }

    // A form whose is_top / z_value are the opposite of the Windows result of the name: whatever the form holds must not matter.
    Condition adversarial_form(const Condition &stored, const std::string &name, double z, bool is_top)
    {
        Condition form = stored;
        form.name = name;
        form.is_top = !is_top;
        form.z_value = z + 7.0;
        return form;
    }

    std::string sql_text(const std::string &text)
    {
        std::string out = "'";
        for (char ch : text)
        {
            out += ch;
            if (ch == '\'')
                out += '\'';
        }
        return out + "'";
    }

    // every named case, every Windows row that raises, and every 17th row of the oracle table
    std::vector<std::string> round_trip_names()
    {
        std::vector<std::string> names;
        for (const NameCase &c : kNames)
            names.push_back(c.name);
        size_t index = 0;
        for (const auto &row : ost_parsers_test::kElevationOracle)
        {
            if (row.status == 1 || index % 17 == 0)
                names.push_back(row.name);
            ++index;
        }
        return names;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// condition_to_save derives is_top / z_value from the name
// ---------------------------------------------------------------------------------------------------------------------

// The named cases of the decision, with the form a dialog builds and with a form that holds the opposite values, in an imperial
// and a metric bid: the saved condition holds the Windows result of the NAME.
TEST(CondElev01_SaveDerivesFromName, TheNamedCasesGiveTheWindowsReload)
{
    for (const bool metric : {false, true})
    {
        for (const NameCase &c : kNames)
        {
            SCOPED_CONTEXT(std::string("name=") + c.name + " (" + c.why + ") metric=" + (metric ? "1" : "0"));
            const Condition stored = stored_condition();

            Condition form;
            dialog_form(stored, c.name, metric, form);
            const Condition from_dialog = ost_domain::condition_to_save(stored, form, metric);
            CHECK_STR_EQ(from_dialog.name, c.name);
            CHECK_EQ(from_dialog.z_value, c.z);
            CHECK_EQ(from_dialog.is_top, c.is_top);

            const Condition saved = ost_domain::condition_to_save(stored, adversarial_form(stored, c.name, c.z, c.is_top), metric);
            CHECK_EQ(saved.z_value, c.z);
            CHECK_EQ(saved.is_top, c.is_top);
            CHECK_EQ(std::signbit(saved.z_value), std::signbit(c.z));
        }
    }
}

// The dialog's default: the Top segment with an empty elevation is is_top = true, z 0.0 in the form, and the saved name has no
// suffix, so a reload gives false / 0.0 (the example of the decision).
TEST(CondElev01_SaveDerivesFromName, TopSegmentWithAnEmptyElevationIsNotTop)
{
    const Condition stored = stored_condition("Wall");
    Condition form = stored;
    form.name = "Wall (renamed)";
    form.is_top = true; // the Top segment is selected by default
    form.z_value = 0.0; // the empty elevation field
    const Condition saved = ost_domain::condition_to_save(stored, form, false);
    CHECK_STR_EQ(saved.name, "Wall (renamed)");
    CHECK_FALSE(saved.is_top);
    CHECK_EQ(saved.z_value, 0.0);
}

// Neither the form nor the stored condition decides: a stored condition that carries stale values (from before this change) and an
// unchanged name gets the values of the name, and deriving never marks a field as changed.
TEST(CondElev01_SaveDerivesFromName, StaleStoredValuesAreReplacedAndNoFieldIsWritten)
{
    Condition stored = stored_condition("Wall @T 3200");
    stored.is_top = true;
    stored.z_value = 125.98425196850394;
    Condition form = stored;
    form.notes = "a note";
    const Condition saved = ost_domain::condition_to_save(stored, form, true);
    CHECK_FALSE(saved.is_top);
    CHECK_EQ(saved.z_value, 0.0);
    const ost_domain::ConditionChanges changes = ost_domain::collect_condition_changes(stored, saved);
    CHECK_EQ(changes.fields.count(), size_t(1));
    CHECK(changes.has(ost_domain::ConditionField::Notes));

    // an untouched form is no change at all, whatever is_top / z_value hold
    CHECK(ost_domain::collect_condition_changes(stored, ost_domain::condition_to_save(stored, stored, true)).empty());
}

// The result does not depend on the style: a style change (which also writes the table's thickness of a Count and the backout) derives from the name as well.
TEST(CondElev01_SaveDerivesFromName, AStyleChangeDerivesFromTheNameToo)
{
    const Condition stored = stored_condition("Pier @B 4' 6\"");
    Condition form = stored;
    form.condition_type = ConditionType::Count;
    form.is_top = true;
    form.z_value = 1.0;
    const Condition saved = ost_domain::condition_to_save(stored, form, false);
    CHECK(saved.condition_type == ConditionType::Count);
    CHECK_FALSE(saved.is_top);
    CHECK_EQ(saved.z_value, 54.0);
}

// All the rows of the Windows oracle table (3000+ names: the Windows tests, the Phase 1 cases, combinations of the imperial and
// metric grammars, Unicode whitespace and lookalike marks, seeded token soups; the rows Windows raises on are (0.0, False)).
TEST(CondElev01_SaveDerivesFromName, EveryOracleRowGivesTheWindowsReload)
{
    const Condition stored = stored_condition();
    size_t rows = 0, mismatches = 0;
    std::string first_mismatch;
    for (const auto &row : ost_parsers_test::kElevationOracle)
    {
        const Condition saved = ost_domain::condition_to_save(stored, adversarial_form(stored, row.name, row.z, row.is_top), false);
        const bool same = saved.z_value == row.z && saved.is_top == row.is_top && std::signbit(saved.z_value) == std::signbit(row.z);
        if (!same && first_mismatch.empty())
            first_mismatch = std::string("name=") + row.name + " saved z=" + std::to_string(saved.z_value) +
                             " is_top=" + (saved.is_top ? "1" : "0") + " want z=" + std::to_string(row.z) +
                             " is_top=" + (row.is_top ? "1" : "0");
        mismatches += same ? 0 : 1;
        ++rows;
    }
    CHECK(rows > 3000); // not an accidentally truncated table
    SCOPED_CONTEXT("first mismatch: " + first_mismatch);
    CHECK_EQ(mismatches, size_t(0));
}

// ---------------------------------------------------------------------------------------------------------------------
// One derivation for the loader and for the Save
// ---------------------------------------------------------------------------------------------------------------------

// The function both sides call: on every oracle row the shared derivation, the loader's helper (ost_store::extract_z_value, what
// ProjectStore+Load.cpp stores) and the condition condition_to_save returns give the Windows row, so the two cannot drift apart.
TEST(CondElev01_SharedDerivation, TheLoaderAndTheSaveGiveTheSameValueOnEveryRow)
{
    const Condition stored = stored_condition();
    size_t rows = 0, mismatches = 0;
    std::string first_mismatch;
    for (const auto &row : ost_parsers_test::kElevationOracle)
    {
        double z = 123.0;
        bool top = !row.is_top;
        ost_domain::derive_elevation_from_name(row.name, z, top);
        const std::pair<double, bool> loader = ost_store::extract_z_value(row.name);
        const Condition saved = ost_domain::condition_to_save(stored, adversarial_form(stored, row.name, row.z, row.is_top), false);
        const bool same = z == row.z && top == row.is_top && loader.first == z && loader.second == top && saved.z_value == z &&
                          saved.is_top == top && std::signbit(z) == std::signbit(row.z);
        if (!same && first_mismatch.empty())
            first_mismatch = std::string("name=") + row.name;
        mismatches += same ? 0 : 1;
        ++rows;
    }
    CHECK(rows > 3000);
    SCOPED_CONTEXT("first mismatch: " + first_mismatch);
    CHECK_EQ(mismatches, size_t(0));
}

// A name Windows cannot read (extract_z_value_from_name raises ValueError: zero denominator) is (0.0, false), whatever the
// out-parameters held: the reader that catches the ValueError stores exactly that.
TEST(CondElev01_SharedDerivation, AnUnreadableNameIsZeroAndNotTopWhateverTheOutputsHeld)
{
    for (const char *name : {"Wall @B 1/0\"", "Wall @T 1/0\"", "Wall @T 3 1/0\""})
    {
        SCOPED_CONTEXT(std::string("name=") + name);
        double z = 0.0;
        bool top = false;
        CHECK_FALSE(ost_domain::extract_z_value_from_name(name, z, top)); // Windows raises
        z = 99.0;
        top = true;
        ost_domain::derive_elevation_from_name(name, z, top);
        CHECK_EQ(z, 0.0);
        CHECK_FALSE(top);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The round trip through the real store: persist with the update the dialog Save uses, then load with the loader
// ---------------------------------------------------------------------------------------------------------------------

// For every name of the table: the condition condition_to_save returns (what the app keeps in memory after the Save) has the same
// is_top and z_value as the condition the loader reads back from the database after the update the dialog's Save sends
// (collect_condition_changes -> ost_store::update_condition), in an imperial bid and in a metric bid.
TEST(CondElev01_ReloadParity, TheSavedInMemoryConditionEqualsTheReloadedOne)
{
    TempDb db("elevation_round_trip");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (1, 1, 'Imperial bid', 1, 0)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (2, 1, 'Metric bid', 2, 1)");

    const std::vector<std::string> names = round_trip_names();
    const auto bid_of = [](size_t i) { return std::string(i % 2 == 1 ? "2" : "1"); }; // odd rows: the metric bid
    const auto uid_of = [](size_t i) { return std::to_string((i % 2 == 1 ? 20000 : 10000) + static_cast<int>(i)); };
    for (size_t i = 0; i < names.size(); ++i)
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (" + uid_of(i) + ", " + bid_of(i) + ", " +
                std::to_string(i + 1) + ", " + sql_text("Old " + std::to_string(i)) + ", 0)");

    // the conditions as the app holds them before the dialog opens
    const ost_data::ProjectData imperial_before = ost_store::load_project(db.path(), "1");
    const ost_data::ProjectData metric_before = ost_store::load_project(db.path(), "2");
    REQUIRE(!imperial_before.empty());
    REQUIRE(!metric_before.empty());

    // Save each one like the dialog: condition_to_save, then the update of the changed fields
    std::vector<Condition> in_memory(names.size());
    size_t from_elevation_controls = 0;
    for (size_t i = 0; i < names.size(); ++i)
    {
        const bool metric = (i % 2 == 1);
        const ost_data::ProjectData &before = metric ? metric_before : imperial_before;
        REQUIRE_EQ(before.conditions.count(uid_of(i)), size_t(1));
        const Condition stored = before.conditions.at(uid_of(i));

        Condition form;
        from_elevation_controls += dialog_form(stored, names[i], metric, form) ? 1 : 0;
        in_memory[i] = ost_domain::condition_to_save(stored, form, metric);
        REQUIRE(ost_store::update_condition(db.path(), bid_of(i), uid_of(i), ost_domain::collect_condition_changes(stored, in_memory[i])));
    }
    // both ways to reach a name are in the table: the elevation controls (a suffix the dialog accepts, i.e. one the loader reads:
    // 93 of the 261 names since b9902e6, measured; it was 28 while the dialog validated with display_to_inches) and a name typed
    // whole into the Name field (168; 233 before)
    CHECK(names.size() > 250);
    CHECK(from_elevation_controls > 60);
    CHECK(names.size() - from_elevation_controls > 150);

    const ost_data::ProjectData imperial = ost_store::load_project(db.path(), "1");
    const ost_data::ProjectData metric = ost_store::load_project(db.path(), "2");
    REQUIRE(!imperial.empty());
    REQUIRE(!metric.empty());

    size_t compared = 0, mismatches = 0;
    std::string first_mismatch;
    for (size_t i = 0; i < names.size(); ++i)
    {
        const ost_data::ProjectData &project = (i % 2 == 1) ? metric : imperial;
        const auto it = project.conditions.find(uid_of(i));
        REQUIRE(it != project.conditions.end());
        const Condition &reloaded = it->second;
        SCOPED_CONTEXT("name=" + names[i]);
        CHECK_STR_EQ(reloaded.name, in_memory[i].name); // the row really holds the saved name
        const bool same = reloaded.is_top == in_memory[i].is_top && reloaded.z_value == in_memory[i].z_value &&
                          std::signbit(reloaded.z_value) == std::signbit(in_memory[i].z_value);
        if (!same && first_mismatch.empty())
            first_mismatch = "name=" + names[i] + " in memory is_top=" + (in_memory[i].is_top ? "1" : "0") +
                             " z=" + std::to_string(in_memory[i].z_value) + " reloaded is_top=" + (reloaded.is_top ? "1" : "0") +
                             " z=" + std::to_string(reloaded.z_value) + " bid=" + bid_of(i);
        mismatches += same ? 0 : 1;
        ++compared;
    }
    CHECK(compared > 250);
    SCOPED_CONTEXT("first mismatch: " + first_mismatch);
    CHECK_EQ(mismatches, size_t(0));
}
