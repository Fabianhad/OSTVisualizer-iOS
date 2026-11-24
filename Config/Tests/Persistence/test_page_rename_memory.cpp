// Rename Page, the in-memory side (ost_data::ProjectData::apply_page_name) and the way the screen uses the three pieces together: the rule
// (ost_domain::decide_page_rename), the store (ost_store::rename_page) and the project in memory (plan 4.11 "Rename page": ProjectData::
// apply_page_name in place, nothing reloaded, the selection and the meshes untouched; plan 5.5; register RP-3, RP-4, RP-5).
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/domain/services/project_data_service.py:420-447   apply_page_name / _sync_page_metadata
//   tests/domain/services/test_project_data_service.py:2436          the page gets the name, the record of the other page stays
//   tests/integration/refresh/test_local_family_ownership.py:60, :215   the rename keeps the page and bid objects (no reload); a failed
//                                                                       rename keeps every name
//   tests/integration/mdb/test_project_roundtrip.py:252-256          rename, reload: the new name and nothing else changed
//   tests/application/services/test_project_write_service.py:16857   a projected rename publishes without a reload
//   tests/presentation/coordinators/test_ui_event_coordinator.py:31502   an unknown page is refused before any write
//   tests/presentation/components/test_page_combo.py:1182           the label of the renamed page ("5 - S9 - Renamed")
// iOS keeps ONE copy of the name (ProjectData::pages), Windows three records (Page, the bid info, the cover sheet snapshot): the tests of
// the other two copies do not port. The "flow" helper below is what the Objective-C layer does on a Write decision: store first, memory only
// after a successful write (the caller must not touch memory after a failed one).
#include "StoreTestSupport.hpp"

#include "Data/PageNavigation.hpp"
#include "Domain/PageRules.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

using ost_domain::Page;
using ost_domain::PageRenameDecision;
using ost_ptest::SeedPage;
using ost_ptest::TempDb;

namespace
{
    std::string num(double value)
    {
        char buffer[40];
        std::snprintf(buffer, sizeof buffer, "%.17g", value);
        return buffer;
    }

    // Every field of a page, one string.
    std::string page_text(const Page &p)
    {
        return "uid=" + p.uid + " name=[" + p.name + "] w=" + num(p.width) + " h=" + num(p.height) + " sf1=" + num(p.scale_factor1) +
               " sf2=" + num(p.scale_factor2) + " rot=" + std::to_string(p.rotation) + " fx=" + std::to_string(p.flip_x) +
               " fy=" + std::to_string(p.flip_y) + " folder=" + p.folder_uid + " index=" + std::to_string(p.page_index) +
               " seq=" + std::to_string(p.sequence) + " sheet=[" + p.sheet_no + "] img=" + p.image_path + " ovl=" + p.overlay_image_path +
               " ox=" + num(p.overlay_offset_x) + " oy=" + num(p.overlay_offset_y) + " orot=" + num(p.overlay_rotation) +
               " dsk=" + num(p.deskew_rotation_overlay) + " resized=" + std::to_string(p.overlay_resized) + " rect=" + p.overlay_rect +
               " show=" + std::to_string(p.image_show_mode);
    }

    std::string folder_text(const ost_domain::Folder &folder)
    {
        std::string out = folder.uid + ":" + folder.name + "[";
        for (const std::string &uid : folder.page_uids)
            out += uid + ";";
        out += "]{";
        for (const auto &pair : folder.subfolders)
            out += folder_text(pair.second) + ",";
        return out + "}";
    }

    // The whole project as far as a rename could touch it: bid, names, the selection, the page order, every page (all fields), the page
    // folders, the conditions, the layers and the takeoffs.
    std::string fingerprint(const ost_data::ProjectData &project)
    {
        std::string out = "bid=" + project.bid.uid + " project=" + project.project_name + " bidname=" + project.bid_name +
                          " selected=" + project.selected_page_uid + "\norder=";
        for (const std::string &uid : project.page_order)
            out += uid + ",";
        std::vector<std::string> page_uids;
        for (const auto &pair : project.pages)
            page_uids.push_back(pair.first);
        std::sort(page_uids.begin(), page_uids.end());
        for (const std::string &uid : page_uids)
            out += "\npage " + page_text(project.pages.at(uid));
        out += "\nfolders=";
        for (const auto &pair : project.folders)
            out += folder_text(pair.second) + ",";
        std::vector<std::string> condition_uids;
        for (const auto &pair : project.conditions)
            condition_uids.push_back(pair.first);
        std::sort(condition_uids.begin(), condition_uids.end());
        for (const std::string &uid : condition_uids)
        {
            const ost_domain::Condition &c = project.conditions.at(uid);
            out += "\ncondition " + uid + ":" + c.name + ":" + std::to_string(c.ref_no) + ":" + c.layer_uid;
        }
        std::vector<std::string> layer_uids;
        for (const auto &pair : project.layers)
            layer_uids.push_back(pair.first);
        std::sort(layer_uids.begin(), layer_uids.end());
        for (const std::string &uid : layer_uids)
            out += "\nlayer " + uid + ":" + project.layers.at(uid).name + ":" + std::to_string(project.layers.at(uid).visible);
        for (const ost_domain::Takeoff &t : project.takeoffs)
        {
            out += "\ntakeoff " + t.uid + ":" + t.condition_uid + ":" + t.page_uid + ":";
            for (const double value : t.position)
                out += num(value) + ";";
        }
        return out;
    }

    // What the screen does on a page rename: decide, write to the store, and only after a successful write change the memory. True when a
    // name was written (store) AND applied (memory). `decision` (optional) reports the rule's answer.
    bool rename_flow(const TempDb &db, ost_data::ProjectData &project, const std::string &page_uid, const std::string &entered,
                     PageRenameDecision *decision = nullptr)
    {
        const auto known = project.pages.find(page_uid);
        const std::string stored = known == project.pages.end() ? std::string() : known->second.name;
        const ost_domain::PageRenamePlan plan = ost_domain::decide_page_rename(entered, stored);
        if (decision)
            *decision = plan.decision;
        if (plan.decision != PageRenameDecision::Write)
            return false;
        if (!ost_store::rename_page(db.path(), project.bid.uid, page_uid, plan.new_name))
            return false;
        return project.apply_page_name(page_uid, plan.new_name);
    }

    ost_data::ProjectData load(const TempDb &db, const std::string &bid = "10")
    {
        ost_data::ProjectData project;
        std::string error;
        if (!ost_store::load_project(db.path(), bid, project, &error))
            FAIL_TEST("load failed: " + error);
        return project;
    }

    // Bid 10 with three pages 1000 'Sheet A' (sequence 1, two takeoffs), 1001 'Sheet B' (2, scale 1 = 12, one takeoff), 1002 'Sheet C' (3), a
    // page folder 95 holding page 1001, a sheet number and a selected page.
    void seed_sheets(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        std::vector<SeedPage> pages(3);
        pages[0].name = "Sheet A";
        pages[0].takeoffs = 2;
        pages[1].name = "Sheet B";
        pages[1].scale_factor1 = 1.0;
        pages[1].takeoffs = 1;
        pages[2].name = "Sheet C";
        ost_ptest::seed_pages(db, 10, pages);
        db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (95, 10, 'F95')");
        db.must("UPDATE BidPages SET BidPageFolderUID = 95, SheetNo = 'S2' WHERE UID = 1001");
        db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (1, 10, 1001)");
    }

    // A hand-built project of two pages (no database).
    ost_data::ProjectData two_pages()
    {
        ost_data::ProjectData project;
        project.bid.uid = "b1";
        Page one;
        one.uid = "p1";
        one.name = "Old";
        one.width = 42;
        one.height = 30;
        one.scale_factor1 = 0.125;
        one.scale_factor2 = 12;
        one.sequence = 3;
        one.sheet_no = "S1";
        one.folder_uid = "f1";
        one.image_path = "plan.pdf";
        Page two;
        two.uid = "p2";
        two.name = "Two";
        two.sequence = 4;
        project.pages["p1"] = one;
        project.pages["p2"] = two;
        project.page_order = {"p1", "p2"};
        return project;
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// ProjectData::apply_page_name
// ---------------------------------------------------------------------------------------------------------------

// Port (reduced) of tests/domain/services/test_project_data_service.py:2436 test_page_name_changes_need_the_current_bid_and_the_exact_page_
// object_and_sync_records: apply_page_name(ref, page, 'New') is True; the page has the name 'New' and the record of the OTHER page keeps
// 'Two'. iOS has one copy of the name: the page changes only its name (every other field of both pages equal to before), and applying the
// name it already has is also true and changes nothing.
TEST(PageRenameMemory, Win2436_ThePageGetsTheNameAndTheOtherPageStays)
{
    ost_data::ProjectData project = two_pages();
    const std::string other_before = page_text(project.pages.at("p2"));
    Page expected = project.pages.at("p1");
    expected.name = "New";
    CHECK(project.apply_page_name("p1", "New"));
    CHECK_EQ(page_text(project.pages.at("p1")), page_text(expected));
    CHECK_EQ(page_text(project.pages.at("p2")), other_before);
    CHECK_EQ(project.pages.at("p2").name, std::string("Two"));
    const std::string settled = fingerprint(project);
    CHECK(project.apply_page_name("p1", "New")); // the same name again
    CHECK_EQ(fingerprint(project), settled);
}

// Port (reduced) of test_project_data_service.py:2436: the refusals. A page uid the project does not have ('zz') is refused (False) and
// changes nothing; iOS must not create it (no unordered_map operator[] insertion): the page count, the keys and the order stay. (The refused
// bid and the equal-but-different page object of the Windows test have no iOS counterpart: one project, pages by uid.) The first call is a
// control that a known page IS renamed, so that a function that always refuses cannot pass.
TEST(PageRenameMemory, Win2436_AnUnknownPageIsRefusedAndNothingIsCreated)
{
    ost_data::ProjectData project = two_pages();
    CHECK(project.apply_page_name("p2", "Control"));
    const std::string before = fingerprint(project);
    for (const char *uid : {"zz", "", "P1", "p1 ", " p1", "p", "p10", "0"})
    {
        SCOPED_CONTEXT(std::string("uid '") + uid + "'");
        CHECK_FALSE(project.apply_page_name(uid, "X"));
        CHECK_EQ(project.pages.size(), static_cast<std::size_t>(2));
        CHECK_EQ(project.pages.count(uid), static_cast<std::size_t>(0));
        CHECK_EQ(fingerprint(project), before);
    }
    ost_data::ProjectData empty;
    CHECK_FALSE(empty.apply_page_name("p1", "X"));
    CHECK(empty.pages.empty());
}

// Port (reduced) of tests/integration/refresh/test_local_family_ownership.py:60 test_page_rename_preserves_page_and_bid_without_database_
// reload and test_project_write_service.py:16857 test_a_projected_page_rename_publishes_page_metadata_without_reloading: after the rename the
// SAME page object is kept (the map entry keeps its address: nothing is inserted, no rehash), its name is the new one, and nothing else of
// the project changed (the whole fingerprint equals a copy with only that name set): no reload happens in memory.
TEST(PageRenameMemory, Win60_TheSamePageEntryIsKeptAndOnlyTheNameChanges)
{
    ost_data::ProjectData project = two_pages();
    for (int i = 0; i < 80; ++i) // enough pages that an insertion would rehash the map
    {
        Page filler;
        filler.uid = "q" + std::to_string(i);
        filler.name = "Filler " + std::to_string(i);
        project.pages[filler.uid] = filler;
        project.page_order.push_back(filler.uid);
    }
    ost_data::ProjectData expected = project;
    expected.pages.at("p1").name = "Renamed";
    const Page *entry = &project.pages.at("p1");
    CHECK(project.apply_page_name("p1", "Renamed"));
    CHECK(entry == &project.pages.at("p1"));
    CHECK_EQ(project.pages.at("p1").name, std::string("Renamed"));
    CHECK_EQ(fingerprint(project), fingerprint(expected));
}

// Research section 4 case 23 (plan 4.11): the page keeps its folder, the page order, the folders and the selection; a loaded project is
// renamed in place: folders, page_order, selected page and the page tree order are exactly as before. The renamed page is the one that sits
// in a folder and is the saved selected page.
TEST(PageRenameMemory, TheFolderTheOrderAndTheSelectionStay)
{
    TempDb db("rename_mem_folder");
    seed_sheets(db);
    ost_data::ProjectData project = load(db);
    REQUIRE_EQ(project.pages.size(), static_cast<std::size_t>(3));
    REQUIRE_EQ(project.pages.at("1001").folder_uid, std::string("95"));
    const std::string before = fingerprint(project);
    const std::vector<std::string> tree = ost_data::page_tree_order(project);
    ost_data::ProjectData expected = project;
    expected.pages.at("1001").name = "Renamed";
    CHECK(project.apply_page_name("1001", "Renamed"));
    CHECK_EQ(fingerprint(project), fingerprint(expected));
    CHECK(fingerprint(project) != before); // the name did change
    CHECK_EQ(project.pages.at("1001").folder_uid, std::string("95"));
    CHECK_EQ(project.selected_page_uid, std::string("1001"));
    CHECK_EQ(ost_data::page_tree_order(project), tree);
    CHECK_EQ(ost_data::initial_page_uid(project), std::string("1001"));
}

// ---------------------------------------------------------------------------------------------------------------
// The flow: rule, store, memory
// ---------------------------------------------------------------------------------------------------------------

// A successful rename (the Windows success path, test_project_write_service.py:16857): the entered text is stripped, written, applied in
// memory, and memory EQUALS a fresh load (the agreement test of plan 4.11: the success path does not reload, so it must give what a reload
// would), page order included (the sequences are distinct).
TEST(PageRenameMemory, Flow_ASuccessfulRenameGivesWhatAReloadGives)
{
    TempDb db("rename_mem_success");
    seed_sheets(db);
    ost_data::ProjectData project = load(db);
    PageRenameDecision decision = PageRenameDecision::Refused;
    CHECK(rename_flow(db, project, "1000", "  Plan A revised \t", &decision));
    CHECK_EQ(static_cast<int>(decision), static_cast<int>(PageRenameDecision::Write));
    CHECK_EQ(project.pages.at("1000").name, std::string("Plan A revised"));
    CHECK_EQ(db.text("SELECT Name FROM BidPages WHERE UID = 1000"), std::string("Plan A revised"));
    CHECK_EQ(fingerprint(project), fingerprint(load(db)));
}

// Port of tests/integration/refresh/test_local_family_ownership.py:215 test_failed_rename_preserves_all_metadata and of the write service's
// `failure` scenario (test_project_write_service.py, save_page_name|failure: the writer gives False, no change in memory, no event, no
// reload): the store write fails (a trigger aborts every UPDATE of BidPages): the flow answers false, the database is byte-identical, the
// project in memory is identical to its copy (the name and every label are the old ones), and after the trigger is dropped the same flow
// works.
TEST(PageRenameMemory, Win215_AFailedWriteLeavesTheDatabaseAndTheProjectUnchanged)
{
    TempDb db("rename_mem_failed");
    seed_sheets(db);
    ost_data::ProjectData project = load(db);
    const std::string project_before = fingerprint(project);
    const std::string db_before = db.snapshot_all();
    const std::string label_before = ost_data::page_label(project.pages.at("1000"));
    const std::string trigger = db.block("BidPages", "UPDATE");
    CHECK_FALSE(rename_flow(db, project, "1000", "Renamed"));
    CHECK_EQ(db.snapshot_all(), db_before);
    CHECK_EQ(fingerprint(project), project_before);
    CHECK_EQ(ost_data::page_label(project.pages.at("1000")), label_before);
    CHECK_EQ(project.pages.at("1000").name, std::string("Sheet A"));
    db.unblock(trigger);
    CHECK(rename_flow(db, project, "1000", "Renamed"));
    CHECK_EQ(project.pages.at("1000").name, std::string("Renamed"));
}

// Port (reduced) of tests/presentation/coordinators/test_ui_event_coordinator.py:31502 test_unknown_target_page_returns_false_without_flush_
// or_write: a page that is not a page of the project is refused before anything is written. Two kinds: a uid no page has, and the page of
// ANOTHER bid (in the database, not in this project: RP-7). Database and project stay identical; a control renames a page of the project.
TEST(PageRenameMemory, Win31502_AnUnknownOrForeignPageNeverChangesAnything)
{
    TempDb db("rename_mem_unknown");
    seed_sheets(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (20, 2, 'Job twenty', 20)");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (2000, 20, 'Foreign')");
    ost_data::ProjectData project = load(db);
    const std::string project_before = fingerprint(project);
    const std::string db_before = db.snapshot_all();
    CHECK_FALSE(rename_flow(db, project, "99999", "Ghost"));
    CHECK_FALSE(rename_flow(db, project, "2000", "Stolen"));
    CHECK_FALSE(rename_flow(db, project, "", "Blank uid"));
    CHECK_EQ(db.snapshot_all(), db_before);
    CHECK_EQ(fingerprint(project), project_before);
    CHECK(rename_flow(db, project, "1002", "Control"));
    CHECK_EQ(db.text("SELECT Name FROM BidPages WHERE UID = 2000"), std::string("Foreign"));
}

// A rename never touches the lock: on a locked bid the store refuses (plan 4.1) and the memory stays the old one; this is the contract the
// screen relies on (it shows "This bid is locked." first, and the store is the second line).
TEST(PageRenameMemory, Flow_ALockedBidChangesNeitherTheDatabaseNorTheProject)
{
    TempDb db("rename_mem_locked");
    seed_sheets(db);
    ost_data::ProjectData project = load(db);
    ost_ptest::seed_locked_bid(db, 10, 5);
    const std::string project_before = fingerprint(project);
    const std::string db_before = db.snapshot_all();
    CHECK_FALSE(rename_flow(db, project, "1000", "Renamed"));
    CHECK_EQ(db.snapshot_all(), db_before);
    CHECK_EQ(fingerprint(project), project_before);
    db.must("UPDATE JobStatuses SET Locked = 0 WHERE UID = 5");
    CHECK(rename_flow(db, project, "1000", "Renamed"));
}

// ---------------------------------------------------------------------------------------------------------------
// Reload: the round trip and the order
// ---------------------------------------------------------------------------------------------------------------

// Port (slice) of tests/integration/mdb/test_project_roundtrip.py:32 test_mixed_project_scale_edit_reload_export_import, the rename at
// :252-256 (save_page_name(page_a, 'Plan A revised'), reload, edited[3][page_a].name == 'Plan A revised', the semantic snapshot otherwise
// unchanged): load_project before and after: the new name, and everything else of the project equal (conditions, takeoffs, layers, pages'
// scales and sheet numbers, folders, the selection, the order).
TEST(PageRenameMemory, Win32_ARenameThenAReloadShowsTheNewNameAndNothingElseChanged)
{
    TempDb db("rename_mem_roundtrip");
    seed_sheets(db);
    ost_data::ProjectData before = load(db);
    ost_data::ProjectData live = before;
    CHECK(rename_flow(db, live, "1000", "Plan A revised"));
    const ost_data::ProjectData after = load(db);
    CHECK_EQ(after.pages.at("1000").name, std::string("Plan A revised"));
    before.pages.at("1000").name = "Plan A revised";
    CHECK_EQ(fingerprint(after), fingerprint(before));
    CHECK_EQ(after.takeoffs.size(), static_cast<std::size_t>(3));
}

// RP-4 (research section 4 case 22): two pages with the SAME Sequence sort by name on a reload, but a rename in memory never re-sorts: the
// page keeps its place until the next load. Pages 1000 'B' and 1001 'C' (sequence 1): order [1000, 1001]; rename 1001 to 'A': memory keeps
// [1000, 1001] (and the page tree), the reload gives [1001, 1000].
TEST(PageRenameMemory, Order_RP4_ARenameDoesNotReSortUntilTheNextLoad)
{
    TempDb db("rename_mem_order");
    seed_sheets(db);
    db.must("UPDATE BidPages SET Name = 'B', Sequence = 1 WHERE UID = 1000");
    db.must("UPDATE BidPages SET Name = 'C', Sequence = 1 WHERE UID = 1001");
    db.must("UPDATE BidPages SET Name = 'Z', Sequence = 2 WHERE UID = 1002");
    ost_data::ProjectData project = load(db);
    REQUIRE_EQ(project.page_order, (std::vector<std::string>{"1000", "1001", "1002"}));
    const std::vector<std::string> tree = ost_data::page_tree_order(project);
    CHECK(rename_flow(db, project, "1001", "A"));
    CHECK_EQ(project.page_order, (std::vector<std::string>{"1000", "1001", "1002"}));
    CHECK_EQ(ost_data::page_tree_order(project), tree);
    const ost_data::ProjectData reloaded = load(db);
    CHECK_EQ(reloaded.page_order, (std::vector<std::string>{"1001", "1000", "1002"}));
}

// ---------------------------------------------------------------------------------------------------------------
// What the page button shows afterwards (research section 4 case 25, test_page_combo.py:1182)
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/presentation/components/test_page_combo.py:1182 test_label_option_change_and_label_refresh_update_the_editor_text, through
// the real flow: the page 1001 (sheet 'S2', sequence 2) renamed to 'Renamed' with the sheet 'S9' and sequence 5 of the Windows test: both
// options on '5 - S9 - Renamed', both off 'Renamed', one each '5 - Renamed' and 'S9 - Renamed'.
TEST(PageRenameMemory, Win1182_ARenamedPageIsLabelledWithTheNewName)
{
    TempDb db("rename_mem_label");
    seed_sheets(db);
    db.must("UPDATE BidPages SET SheetNo = 'S9', Sequence = 5 WHERE UID = 1001");
    ost_data::ProjectData project = load(db);
    CHECK_EQ(ost_data::page_label(project.pages.at("1001"), true, true), std::string("5 - S9 - Sheet B")); // before the rename
    CHECK(rename_flow(db, project, "1001", "Renamed"));
    const Page &page = project.pages.at("1001");
    CHECK_EQ(ost_data::page_label(page, true, true), std::string("5 - S9 - Renamed"));
    CHECK_EQ(ost_data::page_label(page, false, false), std::string("Renamed"));
    CHECK_EQ(ost_data::page_label(page, true, false), std::string("5 - Renamed"));
    CHECK_EQ(ost_data::page_label(page, false, true), std::string("S9 - Renamed"));
}

// RP-5 / RP-6: a page that LOADED with an empty name shows its uid, and renaming it replaces the uid label by the name (the status text of
// Windows shows the raw empty name: iOS always uses page_label). A NULL name loads the same way.
TEST(PageRenameMemory, AnUnnamedPageShowsItsUidUntilItIsRenamed)
{
    TempDb db("rename_mem_unnamed");
    seed_sheets(db);
    db.must("UPDATE BidPages SET Name = '' WHERE UID = 1000");
    db.must("UPDATE BidPages SET Name = NULL WHERE UID = 1002");
    ost_data::ProjectData project = load(db);
    CHECK_EQ(ost_data::page_label(project.pages.at("1000")), std::string("1000"));
    CHECK_EQ(ost_data::page_label(project.pages.at("1002")), std::string("1002"));
    CHECK(rename_flow(db, project, "1000", "Named"));
    CHECK(rename_flow(db, project, "1002", "Also named"));
    CHECK_EQ(ost_data::page_label(project.pages.at("1000")), std::string("Named"));
    CHECK_EQ(ost_data::page_label(project.pages.at("1002")), std::string("Also named"));
}
