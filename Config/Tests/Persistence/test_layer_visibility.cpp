// LP-22 and LP-24 (a): Hide All / Show All and the layer rows the bid does not own.
//
// Windows oracle (HEAD b9902e6):
//  - domain/entities/layer.py is_shared_template_layer (IsTemplate and IsLocked and str(layer.bid_uid or "") != str(bid_uid or ""):
//    uids are compared as text, the bid's own locked copy is not shared) and is_layer_visible (a falsy layer uid and an unknown
//    layer uid are visible);
//  - presentation/coordinators/ui_event_coordinator.py update_all_layers_visibility_deferred: only the non-shared layers of the
//    layer list are captured, an empty capture returns False before anything is queued (the sidebar path ignores the result),
//    layers that already have the requested state return True without a write;
//  - infrastructure/mdb/components/layer_operations.py update_all_layers_show: every captured uid must be the bid's own or a
//    locked template row, then only the rows with BidUID = bid are written;
//  - domain/services/project_data_service.py update_all_layer_visibility: shared layers keep their visibility, a condition is
//    set to the requested state only when its layer is one of the bid's own layers (every other condition is visible, which is
//    what the loader computes), the pages are left alone when the Image layer is a shared row;
//  - tests/domain/services/test_project_data_service.py (test_hide_all_keeps_conditions_without_a_known_layer_visible_like_a_
//    reload, test_hide_all_matches_numeric_layer_keys_by_text, test_hide_all_leaves_shared_locked_template_layers_as_a_reload_
//    shows_them) and tests/integration/visibility/test_hide_all_shared_layers.py (memory, layer list and reload agree).
//
// iOS has no per-condition flag: ProjectData::is_condition_visible derives it from the bid's own layers on every use, so a
// condition without a layer, with a blank or unknown layer or on a shared layer is visible by construction (the LP-22 tests
// below pin that); the layer list is the project's merged list with the live visibility of the bid's own layers.
#include "StoreTestSupport.hpp"

#include "Domain/Layer.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using ost_domain::BidLayer;
using ost_domain::BulkLayerOutcome;
using ost_domain::BulkLayerPlan;
using ost_domain::Layer;
using ost_ptest::TempDb;

namespace
{
    BidLayer entry(const std::string &uid, const std::string &bid_uid, const std::string &name, bool show, int sequence,
                   bool is_template, bool is_locked)
    {
        BidLayer l;
        l.uid = uid;
        l.bid_uid = bid_uid;
        l.name = name;
        l.show = show;
        l.sequence = sequence;
        l.is_template = is_template;
        l.is_locked = is_locked;
        return l;
    }

    Layer live(const std::string &uid, const std::string &name, bool visible, int sequence)
    {
        Layer l;
        l.uid = uid;
        l.name = name;
        l.visible = visible;
        l.sequence = sequence;
        return l;
    }

    std::string join(const std::vector<std::string> &items)
    {
        std::string out;
        for (const std::string &item : items)
            out += (out.empty() ? "" : ",") + item;
        return out;
    }

    // "NoOwnLayers" / "AlreadyThere" / "Write:13,own": one string per plan, so a failure shows the whole outcome (uids are
    // appended for the two outcomes that must not carry any).
    std::string describe(const BulkLayerPlan &plan)
    {
        const std::string rest = plan.uids.empty() ? std::string() : ":" + join(plan.uids);
        if (plan.outcome == BulkLayerOutcome::NoOwnLayers)
            return "NoOwnLayers" + rest;
        if (plan.outcome == BulkLayerOutcome::AlreadyThere)
            return "AlreadyThere" + rest;
        return "Write:" + join(plan.uids);
    }

    // The project the way load_project builds it for `bid_uid`: the rows of `list` that belong to the bid (BidUID = bid, so the
    // bid's own locked copy is in, a shared template row is not) are its layers, with their stored visibility.
    ost_data::ProjectData project_of(const std::vector<BidLayer> &list, const std::string &bid_uid,
                                     const std::vector<std::pair<std::string, std::string>> &condition_layers = {})
    {
        ost_data::ProjectData project;
        project.bid.uid = bid_uid;
        project.merged_layers = list;
        for (const BidLayer &l : list)
            if (l.bid_uid == bid_uid)
                project.layers[l.uid] = live(l.uid, l.name, l.show, l.sequence);
        for (const auto &c : condition_layers)
        {
            ost_domain::Condition cdn;
            cdn.uid = c.first;
            cdn.layer_uid = c.second;
            project.conditions[cdn.uid] = cdn;
        }
        return project;
    }

    // What the Hide All / Show All handler does: plan, write (when a database is given), then the change in memory. *written
    // tells whether the store was called at all. A refused store call would leave the project as it was, so it fails the test.
    BulkLayerPlan bulk(ost_data::ProjectData &project, const TempDb *db, bool show, bool *written = nullptr)
    {
        if (written)
            *written = false;
        const BulkLayerPlan plan =
            ost_domain::plan_bulk_layer_visibility(project.merged_layers, project.bid.uid, project.layers, show);
        if (plan.outcome != BulkLayerOutcome::Write)
            return plan;
        if (db)
        {
            if (written)
                *written = true;
            REQUIRE(ost_store::set_layers_visibility(db->path(), project.bid.uid, plan.uids, show));
        }
        project.apply_layer_visibility(plan.uids, show);
        return plan;
    }

    // The conditions of the project as "uid=visible" sorted by uid.
    std::string condition_states(const ost_data::ProjectData &project)
    {
        std::vector<std::string> uids;
        for (const auto &c : project.conditions)
            uids.push_back(c.first);
        std::sort(uids.begin(), uids.end());
        std::string out;
        for (const std::string &uid : uids)
            out += (out.empty() ? "" : ",") + uid + "=" +
                   (project.is_condition_visible(project.conditions.at(uid)) ? "1" : "0");
        return out;
    }

    // The layer list as the layer sheet shows it: "uid=visible" in list order.
    std::string sheet_states(const ost_data::ProjectData &project)
    {
        std::string out;
        for (const BidLayer &l : project.merged_layers)
            out += (out.empty() ? "" : ",") + l.uid + "=" + (project.is_listed_layer_visible(l.uid) ? "1" : "0");
        return out;
    }

    // The visibility of the bid's own layers as "uid=visible" sorted by uid.
    std::string layer_states(const ost_data::ProjectData &project)
    {
        std::vector<std::string> uids;
        for (const auto &l : project.layers)
            uids.push_back(l.first);
        std::sort(uids.begin(), uids.end());
        std::string out;
        for (const std::string &uid : uids)
            out += (out.empty() ? "" : ",") + uid + "=" + (project.layers.at(uid).visible ? "1" : "0");
        return out;
    }

    // The rows of tests/integration/visibility/test_hide_all_shared_layers.py (UID, BidUID, Name, Show, Sequence, IsTemplate,
    // IsLocked) for bid 7; bid 8 owns the layer 20. The layer list of bid 7 is 1 (shared Image), 13 (its own locked copy of
    // Annotation, which replaces the shared Annotation 3), 2 (shared Default, hidden), 10 (Walls). The conditions are the ones
    // of that test (the layer-less one is NULL).
    void seed_windows_world(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        db.must("DELETE FROM BidLayers");
        db.must("INSERT INTO BidProjects (UID, Name) VALUES (2, 'Folder A')");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (7, 2, 'Seven', 7, 0)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (8, 2, 'Eight', 8, 0)");
        const char *layers[] = {"(1, NULL, 'Image', -1, 0, -1, -1)",      "(2, NULL, 'Default', 0, 2, -1, -1)",
                                "(3, NULL, 'Annotation', -1, 1, -1, -1)", "(13, 7, 'Annotation', -1, 1, -1, -1)",
                                "(10, 7, 'Walls', -1, 5, 0, 0)",          "(20, 8, 'Other', -1, 5, 0, 0)"};
        for (const char *row : layers)
            db.must(std::string("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence, IsTemplate, IsLocked) VALUES ") +
                    row);
        const char *conditions[] = {"(30, 7, 1, 'walls', 0, 10)", "(31, 7, 2, 'default', 0, 2)",
                                    "(32, 7, 3, 'annotation', 0, 13)", "(33, 7, 4, 'image', 0, 1)",
                                    "(34, 7, 5, 'none', 0, NULL)"};
        for (const char *row : conditions)
            db.must(std::string("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, BidLayerUID) VALUES ") + row);
    }

    ost_data::ProjectData load(const TempDb &db, const std::string &bid)
    {
        ost_data::ProjectData project;
        std::string error;
        if (!ost_store::load_project(db.path(), bid, project, &error))
            FAIL_TEST("load failed: " + error);
        return project;
    }

    // "UID=Show" of the rows matching `where`, by UID.
    std::string shows(const TempDb &db, const std::string &where)
    {
        return ost_ptest::list(db, "SELECT UID || '=' || Show FROM BidLayers WHERE " + where + " ORDER BY UID");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// LP-24 (a): which rows are shared
// ---------------------------------------------------------------------------------------------------------------

// Windows test_layer.py SharedTemplateLayerTests: shared = template AND locked AND owned by no bid or by another bid.
TEST(Lp24_SharedTemplateLayer, OnlyLockedTemplateRowsOfNoOrAnotherBidAreShared)
{
    const std::string bid = "b1";
    CHECK(ost_domain::is_shared_template_layer(entry("1", "", "Image", true, 0, true, true), bid));    // owned by no bid (NULL / 0)
    CHECK(ost_domain::is_shared_template_layer(entry("23", "b2", "Callouts", true, 3, true, true), bid)); // another bid's locked copy
    CHECK_FALSE(ost_domain::is_shared_template_layer(entry("13", "b1", "Annotation", true, 1, true, true), bid)); // the bid's own copy
    CHECK_FALSE(ost_domain::is_shared_template_layer(entry("2", "", "Default", true, 2, true, false), bid)); // not locked
    CHECK_FALSE(ost_domain::is_shared_template_layer(entry("3", "", "Image", true, 0, false, true), bid));   // not a template
    CHECK_FALSE(ost_domain::is_shared_template_layer(entry("own", "b1", "Walls", true, 4, false, false), bid));
    CHECK_FALSE(ost_domain::is_shared_template_layer(entry("9", "b2", "Other", true, 4, false, false), bid)); // plain layer of another bid
}

// is_shared_template_layer compares str(layer.bid_uid or "") with str(bid_uid or ""): text, so '' equals '' and "1" is not "01".
TEST(Lp24_SharedTemplateLayer, UidsAreComparedAsText)
{
    const BidLayer own_copy = entry("13", "1", "Image", true, 0, true, true);
    CHECK_FALSE(ost_domain::is_shared_template_layer(own_copy, "1"));
    CHECK(ost_domain::is_shared_template_layer(own_copy, "01"));
    CHECK(ost_domain::is_shared_template_layer(own_copy, ""));
    const BidLayer no_bid = entry("1", "", "Image", true, 0, true, true);
    CHECK_FALSE(ost_domain::is_shared_template_layer(no_bid, "")); // '' == '' (no bid open, no bid owns it)
    CHECK(ost_domain::is_shared_template_layer(no_bid, "1"));
}

// ---------------------------------------------------------------------------------------------------------------
// LP-24 (a): the plan of a Hide All / Show All
// ---------------------------------------------------------------------------------------------------------------

// Windows update_all_layers_visibility_deferred: a list without a layer of the bid (only shared rows, an empty list, a list that
// only has the Comments layer) returns False before anything is queued: nothing to write, in either direction.
TEST(Lp24_BulkPlan, OnlySharedLayersFailAndWriteNothing)
{
    const std::vector<BidLayer> shared = {entry("1", "", "Image", true, 0, true, true),
                                          entry("2", "", "Default", false, 2, true, true),
                                          entry("23", "b2", "Callouts", true, 3, true, true)};
    const std::unordered_map<std::string, Layer> none;
    for (const bool show : {false, true})
    {
        SCOPED_CONTEXT(show ? "Show All" : "Hide All");
        CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(shared, "b1", none, show)), "NoOwnLayers");
        CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility({}, "b1", none, show)), "NoOwnLayers");
        // a shared uid that is wrongly among the bid's layers is not planned either: the row flags decide
        const std::unordered_map<std::string, Layer> wrongly_own = {{"1", live("1", "Image", true, 0)},
                                                                    {"2", live("2", "Default", false, 2)}};
        CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(shared, "b1", wrongly_own, show)), "NoOwnLayers");
        // only the Comments layer of the bid: never changed
        const std::vector<BidLayer> comments = {entry("c", "b1", "Comments", true, 3, false, false)};
        const std::unordered_map<std::string, Layer> own_comments = {{"c", live("c", "Comments", true, 3)}};
        CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(comments, "b1", own_comments, show)), "NoOwnLayers");
        // the Comments layer is recognised by the listed name and by the live name alone (the two always agree for data from
        // load_project, so each check is a redundant guard; pinned by the mutation check of 2026-11-20)
        const std::vector<BidLayer> comments_listed = {entry("c", "b1", "Comments", true, 3, false, false)};
        const std::unordered_map<std::string, Layer> walls_live = {{"c", live("c", "Walls", !show, 3)}};
        CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(comments_listed, "b1", walls_live, show)), "NoOwnLayers");
        const std::vector<BidLayer> walls_listed = {entry("c", "b1", "Walls", true, 3, false, false)};
        const std::unordered_map<std::string, Layer> comments_live = {{"c", live("c", "Comments", !show, 3)}};
        CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(walls_listed, "b1", comments_live, show)), "NoOwnLayers");
        // a listed row of the bid without a live layer has no visibility to change here: not an own layer
        const std::vector<BidLayer> not_live = {entry("x", "b1", "Walls", true, 0, false, false)};
        CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(not_live, "b1", none, show)), "NoOwnLayers");
    }
}

// Windows project_data_service case: shared Image (shown), shared Default (hidden), the bid's own locked copy 13, another
// bid's locked row 23, own Walls. Hide All writes 13 and own (list order), never 1, 2 or 23; Show All after that writes them
// back and nothing else.
TEST(Lp24_BulkPlan, OwnLockedCopyIsPlannedSharedRowsAreNot)
{
    const std::vector<BidLayer> list = {entry("1", "", "Image", true, 0, true, true),
                                        entry("13", "b1", "Annotation", true, 1, true, true),
                                        entry("2", "", "Default", false, 2, true, true),
                                        entry("23", "b2", "Callouts", true, 3, true, true),
                                        entry("own", "b1", "Walls", true, 4, false, false)};
    std::unordered_map<std::string, Layer> own = {{"13", live("13", "Annotation", true, 1)},
                                                  {"own", live("own", "Walls", true, 4)}};
    CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(list, "b1", own, false)), "Write:13,own");
    CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(list, "b1", own, true)), "AlreadyThere");
    own["13"].visible = false;
    own["own"].visible = false;
    CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(list, "b1", own, true)), "Write:13,own");
    CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(list, "b1", own, false)), "AlreadyThere");
    // the copy of b1 is another bid's locked row for b3: shared there, so it is not planned even when it is among the layers
    const std::vector<BidLayer> others_copy = {entry("13", "b1", "Annotation", true, 1, true, true)};
    CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(others_copy, "b3", own, true)), "NoOwnLayers");
}

// Layers that already have the requested visibility are a success with nothing to write (Windows returns True without queuing
// a write); a hidden SHARED row does not count as "still to show".
TEST(Lp24_BulkPlan, AlreadyInStateIsASuccessWithNothingToWrite)
{
    const std::vector<BidLayer> list = {entry("1", "", "Image", true, 0, true, true),
                                        entry("2", "", "Default", false, 2, true, true),
                                        entry("a", "b1", "A", true, 3, false, false),
                                        entry("b", "b1", "B", true, 4, false, false)};
    const std::unordered_map<std::string, Layer> shown = {{"a", live("a", "A", true, 3)}, {"b", live("b", "B", true, 4)}};
    CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(list, "b1", shown, true)), "AlreadyThere");
    const std::unordered_map<std::string, Layer> hidden = {{"a", live("a", "A", false, 3)}, {"b", live("b", "B", false, 4)}};
    CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(list, "b1", hidden, false)), "AlreadyThere");
    // one differs: only that one is planned, whatever the stored Show of its list entry says (the live visibility decides)
    const std::unordered_map<std::string, Layer> mixed = {{"a", live("a", "A", false, 3)}, {"b", live("b", "B", true, 4)}};
    CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(list, "b1", mixed, true)), "Write:a");
    CHECK_STR_EQ(describe(ost_domain::plan_bulk_layer_visibility(list, "b1", mixed, false)), "Write:b");
}

// ---------------------------------------------------------------------------------------------------------------
// LP-22: conditions after Hide All / Show All, in memory
// ---------------------------------------------------------------------------------------------------------------

// Port of test_hide_all_keeps_conditions_without_a_known_layer_visible_like_a_reload: a condition without a layer, with a blank
// or an unknown layer stays visible for Hide All AND Show All; one on a layer of the bid follows. (Windows distinguishes None
// and '' here, iOS reads both as '': one condition stands for both.)
TEST(Lp22_HideAllMemory, LayerLessBlankAndUnknownLayerConditionsStayVisible)
{
    const std::string bid = "b1";
    const std::vector<BidLayer> list = {entry("ann", bid, "Annotation", true, 1, false, false),
                                        entry("img", bid, "Image", true, 2, false, false)};
    ost_data::ProjectData project =
        project_of(list, bid, {{"layered", "ann"}, {"none", ""}, {"blank", " "}, {"unknown", "999"}, {"zero", "0"}});
    project.layers["cm"] = live("cm", "Comments", true, 3); // the bid's Comments row: its layer, not in the list
    CHECK_STR_EQ(condition_states(project), "blank=1,layered=1,none=1,unknown=1,zero=1");

    CHECK_STR_EQ(describe(bulk(project, nullptr, false)), "Write:ann,img");
    CHECK_STR_EQ(condition_states(project), "blank=1,layered=0,none=1,unknown=1,zero=1");
    CHECK(project.layers.at("cm").visible); // never touched

    CHECK_STR_EQ(describe(bulk(project, nullptr, true)), "Write:ann,img");
    CHECK_STR_EQ(condition_states(project), "blank=1,layered=1,none=1,unknown=1,zero=1");
}

// Port of test_hide_all_matches_numeric_layer_keys_by_text: layer uids are compared as text. Windows has an int key 5 and the
// conditions '5' and 5 there (both follow the layer); iOS keeps every uid as text, so "5" follows the layer and "05" (another
// text, an unknown layer) stays visible.
TEST(Lp22_HideAllMemory, NumericLayerUidsAreComparedAsText)
{
    const std::vector<BidLayer> list = {entry("5", "b1", "Walls", true, 1, false, false)};
    ost_data::ProjectData project = project_of(list, "b1", {{"text", "5"}, {"padded", "05"}});
    CHECK_STR_EQ(describe(bulk(project, nullptr, false)), "Write:5");
    CHECK_STR_EQ(condition_states(project), "padded=1,text=0");
    CHECK_STR_EQ(describe(bulk(project, nullptr, true)), "Write:5");
    CHECK_STR_EQ(condition_states(project), "padded=1,text=1");
}

// Port of test_hide_all_leaves_shared_locked_template_layers_as_a_reload_shows_them: a condition on a shared layer stays
// visible although that layer is hidden, one on the bid's own copy of a template layer is hidden with it.
TEST(Lp22_HideAllMemory, ConditionsOnSharedLayersStayVisibleAndOwnLockedCopyIsHidden)
{
    const std::vector<BidLayer> list = {entry("1", "", "Image", true, 0, true, true),
                                        entry("13", "b1", "Annotation", true, 1, true, true),
                                        entry("2", "", "Default", false, 2, true, true),
                                        entry("23", "b2", "Callouts", true, 3, true, true),
                                        entry("own", "b1", "Walls", true, 4, false, false)};
    ost_data::ProjectData project =
        project_of(list, "b1", {{"s", "2"}, {"o", "own"}, {"l", "13"}, {"i", "1"}, {"c23", "23"}});
    // a condition on the hidden shared layer 2 is visible from the start: 2 is not one of the bid's own layers (a reload reads it
    // the same way), although the layer sheet shows the row 2 as hidden
    CHECK_STR_EQ(condition_states(project), "c23=1,i=1,l=1,o=1,s=1");
    CHECK_STR_EQ(sheet_states(project), "1=1,13=1,2=0,23=1,own=1");

    CHECK_STR_EQ(describe(bulk(project, nullptr, false)), "Write:13,own");
    CHECK_STR_EQ(condition_states(project), "c23=1,i=1,l=0,o=0,s=1");
    CHECK_STR_EQ(sheet_states(project), "1=1,13=0,2=0,23=1,own=0"); // the shared Default stays hidden, the others as they were

    CHECK_STR_EQ(describe(bulk(project, nullptr, true)), "Write:13,own");
    CHECK_STR_EQ(condition_states(project), "c23=1,i=1,l=1,o=1,s=1");
    CHECK_STR_EQ(sheet_states(project), "1=1,13=1,2=0,23=1,own=1");
}

// apply_layer_visibility sets the visibility of the named layers of the bid and nothing else (no layer is created for an
// unknown uid); is_listed_layer_visible is what the sheet row shows: the live value of a layer of the bid (not the Show the list
// entry was loaded with), the stored Show of a shared row, visible for a uid the list does not have.
TEST(Lp22_HideAllMemory, ApplyChangesOnlyNamedOwnLayersAndTheSheetStateFollowsTheLiveValue)
{
    const std::vector<BidLayer> list = {entry("a", "b1", "A", false, 1, false, false), // loaded hidden
                                        entry("b", "b1", "B", true, 2, false, false),
                                        entry("2", "", "Default", false, 3, true, true)};
    ost_data::ProjectData project = project_of(list, "b1");
    CHECK_FALSE(project.is_listed_layer_visible("a"));
    CHECK(project.is_listed_layer_visible("b"));
    CHECK_FALSE(project.is_listed_layer_visible("2"));
    CHECK(project.is_listed_layer_visible("zzz")); // not listed, not the bid's: visible

    project.apply_layer_visibility({"a", "nope", "2"}, true); // 2 is a shared row: not one of the bid's layers
    CHECK(project.is_listed_layer_visible("a")); // the live value, although the entry's Show is still false
    CHECK(project.layers.at("a").visible);
    CHECK_EQ(project.layers.size(), static_cast<size_t>(2)); // no layer was added for "nope" or "2"
    CHECK_FALSE(project.is_listed_layer_visible("2"));
    CHECK_STR_EQ(layer_states(project), "a=1,b=1");

    project.apply_layer_visibility({"b"}, false);
    CHECK_STR_EQ(layer_states(project), "a=1,b=0");
    project.apply_layer_visibility({}, false);
    CHECK_STR_EQ(layer_states(project), "a=1,b=0");
}

// Port of tests/integration/visibility/test_hide_all_shared_layers.py for the conditions (store write, in-memory change, then a
// load_project of the same database): the conditions of the memory equal the ones of the reload after Hide All and Show All.
TEST(Lp22_HideAllMemory, MemoryAgreesWithAReload)
{
    TempDb db("lp22_reload");
    seed_windows_world(db);
    ost_data::ProjectData project = load(db, "7");
    // the conditions by uid: 30 walls (layer 10), 31 default (shared 2), 32 annotation (own locked 13), 33 image (shared 1), 34 none
    CHECK_STR_EQ(condition_states(project), "30=1,31=1,32=1,33=1,34=1");

    bool written = false;
    CHECK_STR_EQ(describe(bulk(project, &db, false, &written)), "Write:13,10"); // list order: 1, 13, 2, 10
    CHECK(written);
    const std::string hidden = "30=0,31=1,32=0,33=1,34=1"; // walls and annotation hidden; default, image and none stay visible
    CHECK_STR_EQ(condition_states(project), hidden);
    CHECK_STR_EQ(condition_states(load(db, "7")), hidden);

    CHECK_STR_EQ(describe(bulk(project, &db, true, &written)), "Write:13,10");
    const std::string shown = "30=1,31=1,32=1,33=1,34=1";
    CHECK_STR_EQ(condition_states(project), shown);
    CHECK_STR_EQ(condition_states(load(db, "7")), shown);
}

// ---------------------------------------------------------------------------------------------------------------
// LP-24 (a): memory, layer list and reload show the same state
// ---------------------------------------------------------------------------------------------------------------

// Port of HideAllSharedLayerAgreementTests.test_hide_all_and_show_all_change_only_the_bids_own_layers_everywhere: the layers
// captured are 13 and 10; after Hide All and after Show All the layers of the memory, the sheet state and a reload are equal,
// the shared and the other bid's rows keep (-1, 0, -1, -1) and the page background (Image layer 1 is a shared row) stays.
TEST(Lp24_HideAllAgreement, MemorySheetAndReloadShowTheSameState)
{
    TempDb db("lp24_agreement");
    seed_windows_world(db);
    ost_data::ProjectData project = load(db, "7");
    CHECK_STR_EQ(sheet_states(project), "1=1,13=1,2=0,10=1");
    CHECK(ost_domain::is_image_layer_visible(project.layers));

    CHECK_STR_EQ(describe(bulk(project, &db, false)), "Write:13,10");
    {
        SCOPED_CONTEXT("after Hide All");
        const ost_data::ProjectData reloaded = load(db, "7");
        CHECK_STR_EQ(sheet_states(project), "1=1,13=0,2=0,10=0");
        CHECK_STR_EQ(sheet_states(reloaded), sheet_states(project));
        CHECK_STR_EQ(layer_states(project), "10=0,13=0");
        CHECK_STR_EQ(layer_states(reloaded), layer_states(project));
        CHECK_STR_EQ(condition_states(reloaded), condition_states(project));
        CHECK_STR_EQ(shows(db, "BidUID IS NULL OR BidUID = 8"), "1=-1,2=0,3=-1,20=-1");
        CHECK(ost_domain::is_image_layer_visible(project.layers));
        CHECK(ost_domain::is_image_layer_visible(reloaded.layers));
    }

    CHECK_STR_EQ(describe(bulk(project, &db, true)), "Write:13,10");
    {
        SCOPED_CONTEXT("after Show All");
        const ost_data::ProjectData reloaded = load(db, "7");
        CHECK_STR_EQ(sheet_states(project), "1=1,13=1,2=0,10=1");
        CHECK_STR_EQ(sheet_states(reloaded), sheet_states(project));
        CHECK_STR_EQ(layer_states(reloaded), layer_states(project));
        CHECK_STR_EQ(condition_states(reloaded), condition_states(project));
        CHECK_STR_EQ(shows(db, "BidUID IS NULL OR BidUID = 8"), "1=-1,2=0,3=-1,20=-1");
        CHECK(ost_domain::is_image_layer_visible(reloaded.layers));
    }
}

// A bid with only shared layers: Hide All and Show All fail before anything is written, in memory and in the database.
TEST(Lp24_HideAllAgreement, OnlySharedLayersChangeNothingAnywhere)
{
    TempDb db("lp24_only_shared");
    seed_windows_world(db);
    db.must("DELETE FROM BidLayers WHERE BidUID = 7");
    ost_data::ProjectData project = load(db, "7");
    REQUIRE_EQ(project.merged_layers.size(), static_cast<size_t>(3)); // the shared Image, Annotation and Default
    REQUIRE(project.layers.empty());
    const std::string before = db.snapshot({"BidLayers"});
    const std::string sheet = sheet_states(project);
    const std::string conditions = condition_states(project);

    for (const bool show : {false, true})
    {
        SCOPED_CONTEXT(show ? "Show All" : "Hide All");
        bool written = true;
        CHECK_STR_EQ(describe(bulk(project, &db, show, &written)), "NoOwnLayers");
        CHECK_FALSE(written);
        CHECK_EQ(db.snapshot({"BidLayers"}), before);
        CHECK_STR_EQ(sheet_states(project), sheet);
        CHECK_STR_EQ(condition_states(project), conditions);
        CHECK_STR_EQ(sheet_states(load(db, "7")), sheet);
    }
}

// Layers that already have the requested state are a success that writes nothing.
TEST(Lp24_HideAllAgreement, LayersAlreadyInTheRequestedStateWriteNothing)
{
    TempDb db("lp24_already");
    seed_windows_world(db);
    ost_data::ProjectData project = load(db, "7");
    const std::string before = db.snapshot({"BidLayers"});
    bool written = true;
    CHECK_STR_EQ(describe(bulk(project, &db, true, &written)), "AlreadyThere"); // 13 and 10 are shown, the shared 2 is not counted
    CHECK_FALSE(written);
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
    CHECK_STR_EQ(sheet_states(project), "1=1,13=1,2=0,10=1");
}

// The Image layer decides the page background (PlanView.mm: is_image_layer_visible(project.layers)). When the Image layer is a
// shared row it is not one of the bid's layers, so Hide All leaves the pages as they are and Show All does too (Windows
// update_all_layer_visibility: `if get_image_layer_uid() in shared_layer_uids: return []`).
TEST(Lp24_HideAllAgreement, SharedImageLayerLeavesThePageBackgroundAlone)
{
    const std::vector<BidLayer> list = {entry("1", "", "Image", true, 0, true, true),
                                        entry("own", "b1", "Walls", true, 4, false, false)};
    ost_data::ProjectData project = project_of(list, "b1");
    CHECK(ost_domain::is_image_layer_visible(project.layers));
    CHECK_STR_EQ(describe(bulk(project, nullptr, false)), "Write:own");
    CHECK(ost_domain::is_image_layer_visible(project.layers));
    CHECK_STR_EQ(sheet_states(project), "1=1,own=0");
    CHECK_STR_EQ(describe(bulk(project, nullptr, true)), "Write:own");
    CHECK(ost_domain::is_image_layer_visible(project.layers));
}

// The bid's own copy of the Image layer (template and locked, BidUID = bid) is not shared: Hide All hides it and with it the
// page background, Show All brings it back; the shared Default next to it keeps its state (Windows test: own_image + shared_default).
TEST(Lp24_HideAllAgreement, OwnImageCopyHidesIt)
{
    const std::vector<BidLayer> list = {entry("img", "b1", "Image", true, 0, true, true),
                                        entry("2", "", "Default", true, 1, true, true)};
    ost_data::ProjectData project = project_of(list, "b1");
    CHECK_STR_EQ(describe(bulk(project, nullptr, false)), "Write:img");
    CHECK_FALSE(ost_domain::is_image_layer_visible(project.layers));
    CHECK_STR_EQ(sheet_states(project), "img=0,2=1");
    CHECK_STR_EQ(describe(bulk(project, nullptr, true)), "Write:img");
    CHECK(ost_domain::is_image_layer_visible(project.layers));
}

// The same with a database: the own locked Image copy 13 is written and hidden after a reload, the shared Image 1 is not.
TEST(Lp24_HideAllAgreement, OwnImageCopyIsHiddenInMemoryAndAfterAReload)
{
    TempDb db("lp24_own_image");
    seed_windows_world(db);
    db.must("DELETE FROM BidLayers WHERE UID IN (3, 13, 10)");
    db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence, IsTemplate, IsLocked) VALUES (13, 7, 'Image', -1, 0, -1, -1)");
    ost_data::ProjectData project = load(db, "7");
    REQUIRE_EQ(project.layers.size(), static_cast<size_t>(1));
    CHECK(ost_domain::is_image_layer_visible(project.layers));

    CHECK_STR_EQ(describe(bulk(project, &db, false)), "Write:13");
    const ost_data::ProjectData reloaded = load(db, "7");
    CHECK_FALSE(ost_domain::is_image_layer_visible(project.layers));
    CHECK_FALSE(ost_domain::is_image_layer_visible(reloaded.layers));
    CHECK_STR_EQ(sheet_states(reloaded), sheet_states(project));
    CHECK_STR_EQ(shows(db, "UID IN (1, 2, 13)"), "1=-1,2=0,13=0");
}

// ---------------------------------------------------------------------------------------------------------------
// Windows d1c430a, owner note b: the single toggle of ANOTHER bid's locked template row
// ---------------------------------------------------------------------------------------------------------------

// Windows refuses it in the UI layer (update_layer_visibility_deferred), the infrastructure update_layer_show still writes whatever bid owns the
// row. iOS also refuses in the store, so that a caller that bypasses the sheet cannot change another bid's row either (fail closed, nothing
// written, false). Rows: 61 = a locked template copy of bid 20, 62 = a locked template copy of bid 10 (the open bid's own), 63 = an unlocked
// template row of bid 20, 70 / 90 = plain layers of bid 10 / 20 (seed_two_bids).
TEST(Lp24_SingleToggle, AnotherBidsLockedTemplateRowIsRefusedByTheStoreAndNothingIsWritten)
{
    TempDb db("lp24_single_other_locked");
    ost_ptest::seed_two_bids(db);
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (61, 20, -1, 'Callouts', -1, -1, 5)");
    const std::string before = db.snapshot({"BidLayers"});
    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "10", "61", false));
    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "10", "61", true));
    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "", "61", false)); // no bid named: still not the bid's row
    CHECK_FALSE(ost_store::set_layer_visibility(db.path(), "abc", "61", false));
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
    // the bid that owns the row can still write it (its own locked copy is not shared)
    CHECK(ost_store::set_layer_visibility(db.path(), "20", "61", false));
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 61"), 0);
}

TEST(Lp24_SingleToggle, TheBidsOwnLockedCopyAndPlainLayersAreStillWritten)
{
    TempDb db("lp24_single_own");
    ost_ptest::seed_two_bids(db);
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (62, 10, -1, 'Annotation', -1, -1, 2)");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (63, 20, -1, 'Unlocked', -1, 0, 6)");
    CHECK(ost_store::set_layer_visibility(db.path(), "10", "62", false));
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 62"), 0);
    CHECK(ost_store::set_layer_visibility(db.path(), "10", "70", false));
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 70"), 0);
    // not changed by Windows d1c430a: an unlocked template row and a plain layer of another bid are written (no stricter scoping than Windows)
    CHECK(ost_store::set_layer_visibility(db.path(), "10", "63", false));
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 63"), 0);
    CHECK(ost_store::set_layer_visibility(db.path(), "10", "90", false));
    CHECK_EQ(db.count("SELECT Show FROM BidLayers WHERE UID = 90"), 0);
}
