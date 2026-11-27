// M1 layers (plan 5.3), the IN-USE READ that guards a delete (ost_store::read_layer_uids_in_use), the layers-only reload (ost_store::reload_layers) and the
// cross-feature chains: usage read -> delete plan -> delete, the list a writer produced == what load_project shows, Hide All includes a new layer, the
// Edit Condition picker offers it and a condition can be saved with it, a deleted layer leaves no condition reference.
//
// Windows: settings_reader.py:450-482 get_layer_uids_in_use (the 11 tables, NULL ignored, tables with BidUID filtered to the bid, missing tables / columns
// skipped); the ports are in test_layer_edit_store.cpp (:268, :299, :874). Deliberate differences: LY-2 (project_read_service.py:122-127 and :478-482
// answer an EMPTY set when the read fails, so every layer looks unused and the delete clears the references: here a failed read is `false` and the caller
// refuses), LY-3 (project_data_service.py:560-572 counts conditions WITH takeoffs plus annotations only: here the 11-table rule everywhere). The usage
// read is the REFUSAL of an in-use delete (plan 5.3, tests-first plan: "tested as one chain and not inside delete_layers"). The world is seed_m1: Walls (105)
// is used by condition 301 (a condition WITHOUT takeoffs), Floors (106) only by the annotation rectangle 501, Unused (107) by nothing; bid 20's condition 302
// uses its layer 201.
#include "LayerEditTestSupport.hpp"

#include "Domain/ConditionChoices.hpp"
#include "Domain/Layer.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using ost_ltest::add_bids;
using ost_ltest::kM1Layers;
using ost_ltest::layers_text;
using ost_ltest::merged_text;
using ost_ltest::own_text;
using ost_ltest::put_layer;
using ost_ltest::put_reference;
using ost_ltest::reference_tables;
using ost_ltest::seed_m1;
using ost_ptest::TempDb;

namespace
{
    using Uids = std::set<std::string>;

    Uids used_of(const TempDb &db, const std::string &bid)
    {
        Uids used;
        REQUIRE(ost_store::read_layer_uids_in_use(db.path(), bid, used));
        return used;
    }

    ost_data::ProjectData load(const TempDb &db, const std::string &bid = "10")
    {
        ost_data::ProjectData project;
        std::string error;
        if (!ost_store::load_project(db.path(), bid, project, &error))
            FAIL_TEST("load failed: " + error);
        return project;
    }

    // The gate a screen runs before a delete (plan 5.3 / 6.2, LY-2): an unreadable usage refuses, else the plan decides. Returns the plan's kind
    // text, "refused: unreadable" when the read failed.
    std::string delete_gate(const std::string &db_path, const std::string &bid, const ost_data::ProjectData &project, const std::string &uid,
                            ost_domain::LayerDeletePlan *plan_out = nullptr)
    {
        Uids used;
        if (!ost_store::read_layer_uids_in_use(db_path, bid, used))
            return "refused: unreadable";
        const ost_domain::LayerDeletePlan plan = ost_domain::plan_layer_delete(project.merged_layers, {uid}, used);
        if (plan_out)
            *plan_out = plan;
        switch (plan.kind)
        {
        case ost_domain::LayerDeleteKind::NothingToDelete:
            return "NothingToDelete";
        case ost_domain::LayerDeleteKind::OnlyInUse:
            return "OnlyInUse";
        case ost_domain::LayerDeleteKind::ConfirmMixed:
            return "ConfirmMixed";
        case ost_domain::LayerDeleteKind::Confirm:
            return "Confirm";
        }
        return "?";
    }

    ost_domain::ConditionChanges layer_change(const std::string &layer_uid)
    {
        ost_domain::ConditionChanges changes;
        changes.values.layer_uid = layer_uid;
        changes.mark(ost_domain::ConditionField::LayerUid);
        return changes;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// read_layer_uids_in_use
// ---------------------------------------------------------------------------------------------------------------

// settings_reader.py:454-476: every one of the 11 tables counts (BidConditions, BidZones, BidComments, the four annotation shapes, BidTexts, BidHighlights,
// BidHotLinks, BidCallOuts); the seed already uses 105 (a condition without takeoffs) and 106 (an annotation only), the reference added in turn uses 107.
TEST(LayerEditUsage, EachOfTheElevenTablesCounts)
{
    TempDb db("use_each_table");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(used_of(db, "10") == (Uids{"105", "106"}));
    for (const std::string &table : reference_tables())
    {
        SCOPED_CONTEXT(std::string("") + table);
        put_reference(db, table, 900, "10", "107");
        CHECK(used_of(db, "10") == (Uids{"105", "106", "107"}));
        db.must("DELETE FROM [" + table + "] WHERE UID = 900");
        CHECK(used_of(db, "10") == (Uids{"105", "106"}));
    }
}

// A condition WITHOUT takeoffs is in use (105: the in-memory Windows rule of project_data_service.py:560-572 would say unused, LY-3), and so is a layer used
// by an annotation shape alone (106).
TEST(LayerEditUsage, AConditionWithoutTakeoffsAndAnAnnotationOnlyLayerAreInUse)
{
    TempDb db("use_strict_rule");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 0);
    const Uids used = used_of(db, "10");
    CHECK(used.count("105") == 1);
    CHECK(used.count("106") == 1);
    CHECK(used.count("107") == 0);
    for (const char *locked_copy : {"101", "102", "103", "104"})
        CHECK(used.count(locked_copy) == 0);
}

// settings_reader.py:469-471: the tables with a BidUID column count the bid's rows only; bid 20's rows do not appear for bid 10 and the reverse.
TEST(LayerEditUsage, OnlyTheBidsOwnReferencesCount)
{
    TempDb db("use_per_bid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(used_of(db, "10") == (Uids{"105", "106"}));
    CHECK(used_of(db, "20") == (Uids{"201"}));
    put_reference(db, "BidTexts", 900, "20", "202");
    CHECK(used_of(db, "20") == (Uids{"201", "202"}));
    CHECK(used_of(db, "10") == (Uids{"105", "106"}));
}

// NULL references are ignored (settings_reader.py:469-476 `[BidLayerUID] IS NOT NULL`), also in a row that has a bid.
TEST(LayerEditUsage, NullReferencesAreIgnored)
{
    TempDb db("use_null");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_reference(db, "BidZones", 900, "10", "NULL");
    put_reference(db, "BidHotLinks", 900, "10", "NULL");
    CHECK(used_of(db, "10") == (Uids{"105", "106"}));
}

// A bid whose layers nothing references: the empty set is a SUCCESS (true), distinct from a failed read.
TEST(LayerEditUsage, ABidWithoutReferencesHasAnEmptySet)
{
    TempDb db("use_empty");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    add_bids(db, {30});
    put_layer(db, 150, "30", "Alone", "-1", "1");
    Uids used = {"junk"};
    CHECK(ost_store::read_layer_uids_in_use(db.path(), "30", used));
    CHECK(used.empty()); // on success `used` is REPLACED
}

// "007" is 7: a leading-zero bid is the same bid.
TEST(LayerEditUsage, ALeadingZeroBidUidIsTheSameBid)
{
    TempDb db("use_leading_zero");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(used_of(db, "010") == (Uids{"105", "106"}));
}

// LY-2: Windows returns an EMPTY set when the read fails (project_read_service.py:122-127: every layer then looks unused and the delete clears the
// references). Here a read that cannot be done answers false, and the screen refuses: a database that cannot be opened, a file that is not a database,
// a malformed bid uid.
TEST(LayerEditUsage, AFailedReadIsFalseNotAnEmptySet)
{
    Uids used = {"junk"};
    CHECK_FALSE(ost_store::read_layer_uids_in_use(ost_ltest::unopenable_db_path(), "10", used));
    const std::string garbage = ost_ltest::garbage_db_path("usage");
    CHECK_FALSE(ost_store::read_layer_uids_in_use(garbage, "10", used));
    std::remove(garbage.c_str());
    TempDb db("use_bad_bid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *bid : {"0", "-10", "abc", "", " 10", "10 ", "1.0", "99999999999999999999"})
    {
        SCOPED_CONTEXT(std::string("bid '") + bid + "'");
        Uids out = {"junk"};
        CHECK_FALSE(ost_store::read_layer_uids_in_use(db.path(), bid, out));
    }
}

// A read writes nothing and leaves nothing behind: the database is byte-identical and a write works right after (no lock, no open transaction); it also
// works on a LOCKED bid (the lock guards writes only).
TEST(LayerEditUsage, TheReadWritesNothingAndLeavesNoLockBehind)
{
    TempDb db("use_read_only");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ptest::seed_locked_bid(db, 20, 5);
    const std::string before = db.snapshot_all();
    CHECK(used_of(db, "10") == (Uids{"105", "106"}));
    CHECK(used_of(db, "20") == (Uids{"201"})); // bid 20 is locked: reading is fine
    CHECK_STR_EQ(db.snapshot_all(), before);
    CHECK(ost_store::rename_layer(db.path(), "10", "107", "Spare"));
}

// ---------------------------------------------------------------------------------------------------------------
// the chain: usage read -> delete plan -> delete (plan 5.3 tests-first plan)
// ---------------------------------------------------------------------------------------------------------------

// test_layer_delete_usage.py:38 and test_nested_layer_editor.py:74 at the store level: Walls (a condition) and Floors (an annotation only) are refused with
// the Windows text; Unused asks the question; the usage is read when the action starts, so a reference added after the first look turns the answer
// into a refusal; an unreadable read refuses.
TEST(LayerEditUsage, TheDeleteChainRefusesAnInUseLayerAndAsksForAnUnusedOne)
{
    TempDb db("use_chain");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const ost_data::ProjectData project = load(db);
    ost_domain::LayerDeletePlan plan;
    CHECK_STR_EQ(delete_gate(db.path(), "10", project, "105", &plan), "OnlyInUse");
    CHECK_STR_EQ(plan.title, "Delete Layer");
    CHECK_STR_EQ(plan.message, "Cannot delete \"Walls\" because it is in use.");
    CHECK_STR_EQ(delete_gate(db.path(), "10", project, "106", &plan), "OnlyInUse");
    CHECK_STR_EQ(plan.message, "Cannot delete \"Floors\" because it is in use.");
    CHECK_STR_EQ(delete_gate(db.path(), "10", project, "107", &plan), "Confirm");
    CHECK_STR_EQ(plan.message, "Delete 1 item?");
    // a locked copy and a shared row are never offered
    CHECK_STR_EQ(delete_gate(db.path(), "10", project, "101"), "NothingToDelete");
    CHECK_STR_EQ(delete_gate(db.path(), "10", project, "8"), "NothingToDelete");

    // action-time usage: content added after the project was loaded makes Unused used (test_nested_layer_editor.py:74)
    put_reference(db, "BidTexts", 900, "10", "107");
    CHECK_STR_EQ(delete_gate(db.path(), "10", project, "107", &plan), "OnlyInUse");
    CHECK_STR_EQ(plan.message, "Cannot delete \"Unused\" because it is in use.");
    db.must("DELETE FROM BidTexts WHERE UID = 900");
    CHECK_STR_EQ(delete_gate(db.path(), "10", project, "107"), "Confirm");

    // an unreadable usage refuses (LY-2)
    CHECK_STR_EQ(delete_gate(ost_ltest::unopenable_db_path(), "10", project, "107"), "refused: unreadable");

    // the confirmed delete goes through the store and the layer is gone, the referenced ones are still there
    CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 107"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID IN (105, 106)"), 2);
}

// ---------------------------------------------------------------------------------------------------------------
// reload_layers: the same lists as load_project
// ---------------------------------------------------------------------------------------------------------------

// reload_layers fills BOTH structures like load_project: the bid's own layers (ProjectData::layers) and the sheet's list (merged_layers).
TEST(LayerEditReload, MatchesLoadProjectOnTheSeed)
{
    TempDb db("reload_seed");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const ost_data::ProjectData project = load(db);
    std::unordered_map<std::string, ost_domain::Layer> own;
    std::vector<ost_domain::BidLayer> merged;
    own["stale"].name = "stale";
    merged.resize(3);
    std::string error;
    CHECK(ost_store::reload_layers(db.path(), "10", own, merged, &error));
    CHECK_STR_EQ(own_text(own), own_text(project.layers));
    CHECK_STR_EQ(merged_text(merged), merged_text(project.merged_layers));
    CHECK_EQ(own.size(), static_cast<std::size_t>(7)); // 101-107, Comments included, no shared row, no other bid
    CHECK_EQ(own.count("stale"), static_cast<std::size_t>(0));
    // the layers of the sheet in fields
    REQUIRE_EQ(merged.size(), project.merged_layers.size());
    for (std::size_t i = 0; i < merged.size(); ++i)
    {
        SCOPED_CONTEXT("row " + std::to_string(i));
        CHECK_STR_EQ(merged[i].uid, project.merged_layers[i].uid);
        CHECK_STR_EQ(merged[i].bid_uid, project.merged_layers[i].bid_uid);
        CHECK_STR_EQ(merged[i].name, project.merged_layers[i].name);
        CHECK_EQ(merged[i].show, project.merged_layers[i].show);
        CHECK_EQ(merged[i].sequence, project.merged_layers[i].sequence);
        CHECK_EQ(merged[i].is_template, project.merged_layers[i].is_template);
        CHECK_EQ(merged[i].is_locked, project.merged_layers[i].is_locked);
    }
}

// After EACH writer the reload shows the same lists load_project shows (research section 4, cross-feature): create, rename, move, delete, in turn.
TEST(LayerEditReload, MatchesLoadProjectAfterEveryWriter)
{
    TempDb db("reload_after_writes");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    std::unordered_map<std::string, ost_domain::Layer> own;
    std::vector<ost_domain::BidLayer> merged;
    auto check_same = [&](const std::string &step)
    {
        SCOPED_CONTEXT(std::string("") + step);
        const ost_data::ProjectData project = load(db);
        std::string error;
        REQUIRE(ost_store::reload_layers(db.path(), "10", own, merged, &error));
        CHECK_STR_EQ(own_text(own), own_text(project.layers));
        CHECK_STR_EQ(merged_text(merged), merged_text(project.merged_layers));
    };
    check_same("seed");

    const std::string created = ost_store::insert_layer(db.path(), "10", "Roofing", 4); // below Walls
    REQUIRE(created == "203");
    check_same("insert");
    REQUIRE(own.count("203") == 1);
    CHECK_STR_EQ(own["203"].name, "Roofing");
    CHECK(own["203"].visible);
    CHECK_EQ(own["203"].sequence, 5);
    bool listed = false;
    for (const ost_domain::BidLayer &layer : merged)
        listed = listed || (layer.uid == "203" && layer.name == "Roofing" && layer.sequence == 5);
    CHECK(listed);

    CHECK(ost_store::rename_layer(db.path(), "10", "203", "Roofing 2"));
    check_same("rename");
    CHECK_STR_EQ(own["203"].name, "Roofing 2");

    CHECK(ost_store::swap_layer_sequence(db.path(), "10", "203", "105"));
    check_same("swap");
    CHECK_EQ(own["203"].sequence, 4);
    CHECK_EQ(own["105"].sequence, 5);

    CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
    check_same("delete");
    CHECK_EQ(own.count("107"), static_cast<std::size_t>(0));
}

// A failed read leaves BOTH outputs exactly as they were (the screen keeps its old state): a database that cannot be opened, a file that is not a
// database, a malformed bid uid; and a reload never writes.
TEST(LayerEditReload, AFailedReadLeavesTheOutputsUntouched)
{
    std::unordered_map<std::string, ost_domain::Layer> own;
    own["9"].uid = "9";
    own["9"].name = "Kept";
    own["9"].visible = false;
    own["9"].sequence = 3;
    std::vector<ost_domain::BidLayer> merged(1);
    merged[0].uid = "9";
    merged[0].name = "Kept";
    merged[0].sequence = 3;
    const std::string own_before = own_text(own);
    const std::string merged_before = merged_text(merged);

    std::string error;
    CHECK_FALSE(ost_store::reload_layers(ost_ltest::unopenable_db_path(), "10", own, merged, &error));
    CHECK_STR_EQ(own_text(own), own_before);
    CHECK_STR_EQ(merged_text(merged), merged_before);
    CHECK(!error.empty());

    const std::string garbage = ost_ltest::garbage_db_path("reload");
    CHECK_FALSE(ost_store::reload_layers(garbage, "10", own, merged, nullptr));
    CHECK_STR_EQ(own_text(own), own_before);
    CHECK_STR_EQ(merged_text(merged), merged_before);
    std::remove(garbage.c_str());

    TempDb db("reload_bad_bid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string snapshot = db.snapshot_all();
    for (const char *bid : {"0", "-10", "abc", "", " 10", "10 ", "1.0", "99999999999999999999"})
    {
        SCOPED_CONTEXT(std::string("bid '") + bid + "'");
        CHECK_FALSE(ost_store::reload_layers(db.path(), bid, own, merged, nullptr));
        CHECK_STR_EQ(own_text(own), own_before);
        CHECK_STR_EQ(merged_text(merged), merged_before);
    }
    CHECK_STR_EQ(db.snapshot_all(), snapshot);
}

// ---------------------------------------------------------------------------------------------------------------
// cross-feature: what the writers produce is what the rest of the app uses
// ---------------------------------------------------------------------------------------------------------------

// plan 5.3 tests-first plan: Hide All includes the new layer; Show All / Hide All never write a shared row (LP-24). The Hide All plan of the reloaded list
// names the new layer, no shared row, no Comments layer; writing it changes exactly those rows.
TEST(LayerEditCross, HideAllIncludesTheNewLayerAndNeverTheSharedRows)
{
    TempDb db("cross_hide_all");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string created = ost_store::insert_layer(db.path(), "10", "Roofing", 6);
    REQUIRE(created == "203");
    std::unordered_map<std::string, ost_domain::Layer> own;
    std::vector<ost_domain::BidLayer> merged;
    REQUIRE(ost_store::reload_layers(db.path(), "10", own, merged, nullptr));
    const ost_domain::BulkLayerPlan plan = ost_domain::plan_bulk_layer_visibility(merged, "10", own, false);
    REQUIRE(plan.outcome == ost_domain::BulkLayerOutcome::Write);
    CHECK(std::find(plan.uids.begin(), plan.uids.end(), "203") != plan.uids.end());
    for (const char *shared : {"1", "2", "3", "4", "8", "104"}) // shared rows and the unlisted Comments copy
        CHECK(std::find(plan.uids.begin(), plan.uids.end(), shared) == plan.uids.end());
    CHECK(ost_store::set_layers_visibility(db.path(), "10", plan.uids, false));
    CHECK_STR_EQ(ost_ltest::table_text(db, "BidLayers", "UID, Show", "UID IN (1, 2, 3, 4, 8, 104, 203)"), "(1,-1);(2,-1);(3,-1);(4,-1);(8,-1);(104,-1);(203,0)");
}

// A created layer is offered by the Edit Condition picker at once and a condition can be saved with it (update_condition's ownership check); a deleted
// layer is gone from the picker and can no longer be assigned.
TEST(LayerEditCross, ACreatedLayerIsOfferedAndAssignableADeletedOneIsNot)
{
    TempDb db("cross_picker");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string created = ost_store::insert_layer(db.path(), "10", "Roofing", 6);
    REQUIRE(created == "203");
    std::unordered_map<std::string, ost_domain::Layer> own;
    std::vector<ost_domain::BidLayer> merged;
    REQUIRE(ost_store::reload_layers(db.path(), "10", own, merged, nullptr));
    const std::vector<std::string> choices = ost_domain::condition_layer_choices(merged, own);
    CHECK(std::find(choices.begin(), choices.end(), "203") != choices.end());
    CHECK(std::find(choices.begin(), choices.end(), "104") == choices.end()); // the hidden Comments copy is never offered
    CHECK(ost_store::update_condition(db.path(), "10", "301", layer_change("203")));
    CHECK_STR_EQ(db.text("SELECT BidLayerUID FROM BidConditions WHERE UID = 301"), "203");

    // a deleted layer: not offered, not assignable (the row is gone)
    CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
    REQUIRE(ost_store::reload_layers(db.path(), "10", own, merged, nullptr));
    const std::vector<std::string> after = ost_domain::condition_layer_choices(merged, own);
    CHECK(std::find(after.begin(), after.end(), "107") == after.end());
    CHECK_FALSE(ost_store::update_condition(db.path(), "10", "301", layer_change("107")));
    CHECK_STR_EQ(db.text("SELECT BidLayerUID FROM BidConditions WHERE UID = 301"), "203");
}

// A rename is what the conditions and the picker see after the reload: same uid, new name (a rename never changes which conditions use the layer).
TEST(LayerEditCross, ARenamedLayerKeepsItsUidAndItsConditions)
{
    TempDb db("cross_rename");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::rename_layer(db.path(), "10", "105", "Foundation walls"));
    const ost_data::ProjectData project = load(db);
    CHECK_STR_EQ(project.layers.at("105").name, "Foundation walls");
    CHECK_STR_EQ(project.conditions.at("301").layer_uid, "105");
    bool found = false;
    for (const ost_domain::BidLayer &layer : project.merged_layers)
        found = found || (layer.uid == "105" && layer.name == "Foundation walls");
    CHECK(found);
}

// "the in-memory project after a delete (full reload) has no condition with the removed layer_uid": Walls (105) is used by condition 301; the store keeps
// the Windows contract and clears the reference when it is asked to delete it, so the reloaded condition has no layer; a project loaded BEFORE keeps the old
// uid (the screen must reload, plan 4.11).
TEST(LayerEditCross, AfterADeleteNoConditionPointsAtTheRemovedLayer)
{
    TempDb db("cross_delete_conditions");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const ost_data::ProjectData before = load(db);
    CHECK_STR_EQ(before.conditions.at("301").layer_uid, "105");
    CHECK(ost_store::delete_layers(db.path(), "10", {"105"}));
    const ost_data::ProjectData after = load(db);
    CHECK_STR_EQ(after.conditions.at("301").layer_uid, "");
    CHECK_EQ(after.layers.count("105"), static_cast<std::size_t>(0));
    for (const auto &entry : after.conditions)
        CHECK_NE(entry.second.layer_uid, std::string("105"));
    CHECK_STR_EQ(before.conditions.at("301").layer_uid, "105"); // the old in-memory copy is stale until reloaded
}

// "close and reopen the database file: the same state": every store call opens its own connection, so a fresh load after a series of writes shows exactly
// what the writers left, in the order the sheet uses.
TEST(LayerEditCross, TheStateSurvivesReopeningTheFile)
{
    TempDb db("cross_reopen");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    REQUIRE(ost_store::insert_layer(db.path(), "10", "Roofing", 6) == "203");
    CHECK(ost_store::rename_layer(db.path(), "10", "106", "Slabs"));
    CHECK(ost_store::swap_layer_sequence(db.path(), "10", "105", "107"));
    CHECK(ost_store::delete_layers(db.path(), "10", {"203"}));
    const std::string layers = layers_text(db, "BidUID = 10");
    CHECK_STR_EQ(layers,
                 "(101,10,'Image',-1,0,0,-1);(102,10,'Annotation',-1,1,0,-1);(103,10,'Default',-1,2,0,-1);(104,10,'Comments',-1,3,0,-1);"
                 "(105,10,'Walls',-1,6,0,0);(106,10,'Slabs',-1,5,0,0);(107,10,'Unused',-1,4,0,0)");
    const ost_data::ProjectData project = load(db);
    CHECK_STR_EQ(own_text(project.layers),
                 "101:Image:1:0,102:Annotation:1:1,103:Default:1:2,104:Comments:1:3,105:Walls:1:6,106:Slabs:1:5,107:Unused:1:4");
    // a second read of the same file gives the same lists
    const ost_data::ProjectData again = load(db);
    CHECK_STR_EQ(merged_text(again.merged_layers), merged_text(project.merged_layers));
}
