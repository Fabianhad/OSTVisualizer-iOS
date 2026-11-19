// The page order, the first page and the page selection rules of the page selector (Core/Data/PageNavigation.*,
// ProjectStore+Load.cpp page_order / selected_page_uid):
//   LP-08 (+BRW-18, DBP-27)  pages are ordered by BidPages.Sequence, Name, UID (not Index1); the tree walk lists a folder's
//                            subfolders first, then its own pages, and the loose pages last
//   LP-10                    a folder check box adds / removes only the pages below that folder
//   LP-11                    activating the active page again keeps it
//   LP-12                    the first page is the saved BidSettings.BidPageSelectedUID, else the first page of the tree walk
// Windows oracle: tests/infrastructure/mdb/components/test_hierarchy_reader.py (test_page_info_carries_every_stored_field_in_
// sequence_name_uid_order, test_pages_with_equal_sequence_and_name_are_ordered_by_uid, test_folders_nest_by_parent_drop_empty_
// branches_and_keep_name_order), tests/presentation/components/test_page_combo.py (PageComboPreferenceTests._folder_bid,
// test_folder_checkbox_*, test_toggling_a_partial_folder_checks_all_of_its_pages, test_folder_without_pages_has_no_checkbox,
// test_get_first_page_uid_follows_model_order_and_is_none_when_empty, test_clicking_a_page_row_activates_it_without_toggling_
// its_check), tests/integration/navigation/test_bid_switch_last_page.py and bid_data_reader._parse_bid_selected_page.
#include "StoreTestSupport.hpp"

#include "Data/PageNavigation.hpp"

#include <set>
#include <string>
#include <vector>

using ost_ptest::TempDb;
using Uids = std::vector<std::string>;

namespace
{
    void seed_bids(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'one', 1)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'two', 2)");
    }

    // One BidPages row; `name_sql` / `sequence_sql` / `folder_sql` are SQL expressions ('A', NULL, 12).
    void add_page(const TempDb &db, int uid, int bid, const std::string &name_sql, const std::string &sequence_sql,
                  int index1 = 1, const std::string &folder_sql = "NULL")
    {
        db.must("INSERT INTO BidPages (UID, BidUID, Name, Sequence, Index1, BidPageFolderUID) VALUES (" + std::to_string(uid) +
                ", " + std::to_string(bid) + ", " + name_sql + ", " + sequence_sql + ", " + std::to_string(index1) + ", " +
                folder_sql + ")");
    }

    void add_folder(const TempDb &db, int uid, const std::string &name, const std::string &parent_sql)
    {
        db.must("INSERT INTO BidPageFolders (UID, BidUID, Name, ParentUID) VALUES (" + std::to_string(uid) + ", 1, " +
                ost_ptest::quoted(name) + ", " + parent_sql + ")");
    }

    ost_data::ProjectData load(const TempDb &db, const std::string &bid = "1")
    {
        return ost_store::load_project(db.path(), bid);
    }

    // The page selector fixture of the Windows tests (test_page_combo.py PageComboPreferenceTests._folder_bid):
    //   Folder f1 'Folder'  : own pages p1 'A101', p2 'A102'; subfolder f2 'Nested': p3 'A103', p4 'A104'
    //   Folder f3 'Other'   : p5 'A105'
    //   loose               : p6 'A106'
    // The page UIDs are 1..6 (p1 = 1 ...), the folder UIDs f1 = 10, f2 = 11, f3 = 12.
    void seed_folder_bid(const TempDb &db)
    {
        seed_bids(db);
        add_folder(db, 10, "Folder", "NULL");
        add_folder(db, 11, "Nested", "10");
        add_folder(db, 12, "Other", "NULL");
        add_page(db, 1, 1, "'A101'", "1", 1, "10");
        add_page(db, 2, 1, "'A102'", "2", 1, "10");
        add_page(db, 3, 1, "'A103'", "3", 1, "11");
        add_page(db, 4, 1, "'A104'", "4", 1, "11");
        add_page(db, 5, 1, "'A105'", "5", 1, "12");
        add_page(db, 6, 1, "'A106'", "6", 1, "NULL");
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// LP-08
// ---------------------------------------------------------------------------------------------------------------

// The item plan: rows (UID, Name, Sequence, Index1) (30,'B',2,1), (31,'A',1,2), (32,'C',3,1): Windows order [31, 30, 32]
// (Sequence first); the old iOS order by Index1 then name was [30, 32, 31].
TEST(Lp08_PageOrder, PagesFollowSequenceNotIndex1)
{
    TempDb db("pageorder_sequence");
    seed_bids(db);
    add_page(db, 30, 1, "'B'", "2", 1);
    add_page(db, 31, 1, "'A'", "1", 2);
    add_page(db, 32, 1, "'C'", "3", 1);
    const ost_data::ProjectData project = load(db);
    CHECK_EQ(project.page_order, (Uids{"31", "30", "32"}));
    CHECK_EQ(ost_data::page_tree_order(project), (Uids{"31", "30", "32"}));
}

// Port of test_hierarchy_reader.py::test_pages_with_equal_sequence_and_name_are_ordered_by_uid: UIDs 32, 30, 31 with the same
// name and sequence (inserted in that order) are listed [30, 31, 32].
TEST(Lp08_PageOrder, EqualSequenceAndNameAreOrderedByUid)
{
    TempDb db("pageorder_ties");
    seed_bids(db);
    add_page(db, 32, 1, "'Same'", "4", 1);
    add_page(db, 30, 1, "'Same'", "4", 3);
    add_page(db, 31, 1, "'Same'", "4", 2);
    const ost_data::ProjectData project = load(db);
    CHECK_EQ(project.page_order, (Uids{"30", "31", "32"}));
    CHECK_EQ(ost_data::page_tree_order(project), (Uids{"30", "31", "32"}));
}

// Port of test_hierarchy_reader.py::test_page_info_carries_every_stored_field_in_sequence_name_uid_order: pages 20 ('A-101',
// Sequence 2), 21 (NULL name, NULL sequence), 22 ('B', 1), 23 ('A', 1) come in the order [21, 23, 22, 20] (a NULL sorts
// first; then Sequence, Name, UID).
TEST(Lp08_PageOrder, TheWindowsFixtureWithNullsIsListedByNullThenSequenceThenName)
{
    TempDb db("pageorder_fixture");
    seed_bids(db);
    add_page(db, 20, 1, "'A-101'", "2", 1);
    add_page(db, 21, 1, "NULL", "NULL", 1);
    add_page(db, 22, 1, "'B'", "1", 1);
    add_page(db, 23, 1, "'A'", "1", 1);
    const ost_data::ProjectData project = load(db);
    CHECK_EQ(project.page_order, (Uids{"21", "23", "22", "20"}));
    CHECK_EQ(ost_data::page_tree_order(project), (Uids{"21", "23", "22", "20"}));
}

// DBP-27 plan: (UID, Name, Index1, Sequence) (3,'B',1,1), (1,'A',2,1), (2,'C',1,0) order [2, 1, 3] (Sequence, then Name, then
// UID; the Name order is the database's binary order, see ProjectStore+Load.cpp).
TEST(Lp08_PageOrder, TheDbp27PlanOrdersBySequenceThenName)
{
    TempDb db("pageorder_dbp27");
    seed_bids(db);
    add_page(db, 3, 1, "'B'", "1", 1);
    add_page(db, 1, 1, "'A'", "1", 2);
    add_page(db, 2, 1, "'C'", "0", 1);
    const ost_data::ProjectData project = load(db);
    CHECK_EQ(ost_data::page_tree_order(project), (Uids{"2", "1", "3"}));
}

// The UID of a page is a number: 9 comes before 10 (the order comes from the database, not from comparing the UID text).
TEST(Lp08_PageOrder, UidsOfTheSameNameAndSequenceAreNumeric)
{
    TempDb db("pageorder_numeric_uid");
    seed_bids(db);
    add_page(db, 10, 1, "'Same'", "1", 1);
    add_page(db, 9, 1, "'Same'", "1", 1);
    add_page(db, 100, 1, "'Same'", "1", 1);
    const ost_data::ProjectData project = load(db);
    CHECK_EQ(ost_data::page_tree_order(project), (Uids{"9", "10", "100"}));
}

// Port of test_page_combo.py::test_get_first_page_uid_follows_model_order_and_is_none_when_empty (PageComboPreferenceTests.
// _folder_bid): the walk lists the subfolder's pages first, then the folder's own pages, then the next folder, then the
// loose page: [p3, p4, p1, p2, p5, p6], so the first page is p3. The old iOS selector laid out p1, p2, p3, p4, p5, p6.
TEST(Lp08_PageOrder, TheTreeWalkListsSubfoldersBeforeTheFoldersOwnPagesAndLoosePagesLast)
{
    TempDb db("pageorder_walk");
    seed_folder_bid(db);
    const ost_data::ProjectData project = load(db);
    CHECK_EQ(ost_data::page_tree_order(project), (Uids{"3", "4", "1", "2", "5", "6"}));
    CHECK_EQ(project.folders.at("10").all_page_uids(), (Uids{"3", "4", "1", "2"}));
    // the page selector's own page list (without folders) keeps the page order
    CHECK_EQ(project.page_order, (Uids{"1", "2", "3", "4", "5", "6"}));
}

// The pages inside one folder follow the page order (Sequence, Name, UID) too, and the folders follow Name, UID.
TEST(Lp08_PageOrder, PagesInsideAFolderFollowTheSequence)
{
    TempDb db("pageorder_in_folder");
    seed_bids(db);
    add_folder(db, 10, "Zeta", "NULL");
    add_folder(db, 11, "Alpha", "NULL");
    add_page(db, 1, 1, "'P1'", "3", 1, "10");
    add_page(db, 2, 1, "'P2'", "1", 2, "10");
    add_page(db, 3, 1, "'P3'", "2", 3, "10");
    add_page(db, 4, 1, "'P4'", "9", 1, "11");
    add_page(db, 5, 1, "'P5'", "0", 1, "NULL");
    const ost_data::ProjectData project = load(db);
    // Alpha (page 4), Zeta (pages 2, 3, 1 by Sequence), then the loose page 5
    CHECK_EQ(ost_data::page_tree_order(project), (Uids{"4", "2", "3", "1", "5"}));
}

TEST(Lp08_PageOrder, AProjectWithoutPagesHasNoOrder)
{
    TempDb db("pageorder_empty");
    seed_bids(db);
    const ost_data::ProjectData project = load(db);
    CHECK(project.page_order.empty());
    CHECK(ost_data::page_tree_order(project).empty());
    CHECK(ost_data::initial_page_uid(project).empty());
}

// A project that was not read by load_project (no page_order) still lists every page: after the listed ones, in
// (Sequence, Name, UID text) order.
TEST(Lp08_PageOrder, APageThatPageOrderDoesNotListIsNotLost)
{
    ost_data::ProjectData project;
    for (const char *uid : {"b", "a", "c", "d"})
    {
        ost_domain::Page page;
        page.uid = uid;
        page.name = uid;
        page.sequence = 5;
        project.pages[uid] = page;
    }
    project.pages["d"].sequence = 1;
    project.page_order = {"c"};
    // c is listed; then d (Sequence 1), then a, b (Sequence 5, Name)
    CHECK_EQ(ost_data::page_tree_order(project), (Uids{"c", "d", "a", "b"}));
    CHECK_EQ(ost_data::selected_pages_in_tree_order(project, {"b", "d", "x", "c"}), (Uids{"c", "d", "b"}));
}

// ---------------------------------------------------------------------------------------------------------------
// LP-12
// ---------------------------------------------------------------------------------------------------------------

// Plan: pages 31, 30, 32 (LP-08 rows) and BidPageSelectedUID = 32: the initial page is 32; NULL: the first page of the tree
// walk (31); a saved uid whose page is not in the bid falls back to the first page.
TEST(Lp12_InitialPage, TheSavedPageIsRestoredElseTheFirstPageOfTheTreeWalk)
{
    TempDb db("initial_saved");
    seed_bids(db);
    add_page(db, 30, 1, "'B'", "2", 1);
    add_page(db, 31, 1, "'A'", "1", 2);
    add_page(db, 32, 1, "'C'", "3", 1);
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (900, 1, 32)");
    const ost_data::ProjectData saved = load(db);
    CHECK_STR_EQ(saved.selected_page_uid, "32");
    CHECK_STR_EQ(ost_data::initial_page_uid(saved), "32");

    db.must("UPDATE BidSettings SET BidPageSelectedUID = NULL WHERE UID = 900");
    const ost_data::ProjectData none = load(db);
    CHECK(none.selected_page_uid.empty());
    CHECK_STR_EQ(ost_data::initial_page_uid(none), "31");
}

TEST(Lp12_InitialPage, ASavedPageOfAnotherBidFallsBackToTheFirstPage)
{
    TempDb db("initial_foreign");
    seed_bids(db);
    add_page(db, 30, 1, "'B'", "2", 1);
    add_page(db, 31, 1, "'A'", "1", 2);
    add_page(db, 40, 2, "'Other bid'", "1", 1);
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (900, 1, 40)");
    const ost_data::ProjectData project = load(db);
    CHECK_STR_EQ(project.selected_page_uid, "40"); // what is stored
    CHECK_STR_EQ(ost_data::initial_page_uid(project), "31"); // the page is not one of this bid's pages
}

TEST(Lp12_InitialPage, ZeroMeansNoSavedPageAndOtherBidsSettingsAreIgnored)
{
    TempDb db("initial_zero");
    seed_bids(db);
    add_page(db, 30, 1, "'B'", "2", 1);
    add_page(db, 31, 1, "'A'", "1", 2);
    add_page(db, 40, 2, "'Other bid'", "1", 1);
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (900, 1, 0)");
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (901, 2, 40)");
    const ost_data::ProjectData one = load(db, "1");
    CHECK(one.selected_page_uid.empty());
    CHECK_STR_EQ(ost_data::initial_page_uid(one), "31");
    const ost_data::ProjectData two = load(db, "2");
    CHECK_STR_EQ(ost_data::initial_page_uid(two), "40");
}

TEST(Lp12_InitialPage, ABidWithoutASettingsRowOrTableHasNoSavedPage)
{
    TempDb db("initial_no_settings");
    seed_bids(db);
    add_page(db, 30, 1, "'B'", "2", 1);
    const ost_data::ProjectData no_row = load(db);
    CHECK(no_row.selected_page_uid.empty());
    CHECK_STR_EQ(ost_data::initial_page_uid(no_row), "30");

    db.must("DROP TABLE BidSettings");
    const ost_data::ProjectData no_table = load(db);
    CHECK(!no_table.empty());
    CHECK(no_table.selected_page_uid.empty());
    CHECK_STR_EQ(ost_data::initial_page_uid(no_table), "30");
}

// Windows fetch_optional_bid_settings_row: a second BidSettings row of the bid raises BidSettingsCardinalityError (not a
// database error, so the bid does not load; test_bid_data_reader.py: 'BidSettings has multiple rows for Bids.UID=102').
// The other bid keeps loading.
TEST(Lp12_InitialPage, TwoSettingsRowsOfOneBidFailTheLoadOfThatBid)
{
    TempDb db("initial_two_rows");
    seed_bids(db);
    add_page(db, 30, 1, "'B'", "2", 1);
    add_page(db, 40, 2, "'Other bid'", "1", 1);
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (900, 1, 30)");
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (901, 1, 30)");
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (902, 2, 40)");
    ost_data::ProjectData failed;
    std::string error;
    CHECK(!ost_store::load_project(db.path(), "1", failed, &error));
    CHECK(failed.empty());
    CHECK_STR_EQ(error, "BidSettings has multiple rows for Bids.UID=1; expected at most one.");
    CHECK(load(db, "1").empty());
    const ost_data::ProjectData other = load(db, "2");
    CHECK_STR_EQ(other.selected_page_uid, "40");
}

TEST(Lp12_InitialPage, AProjectWithoutPagesHasNoInitialPage)
{
    ost_data::ProjectData project;
    CHECK(ost_data::initial_page_uid(project).empty());
    project.selected_page_uid = "5";
    CHECK(ost_data::initial_page_uid(project).empty());
}

// ---------------------------------------------------------------------------------------------------------------
// LP-10
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    using Selection = std::set<std::string>;

    Uids folder_pages(const ost_data::ProjectData &project, const std::string &folder_uid)
    {
        return project.folders.at(folder_uid).all_page_uids();
    }
} // namespace

// Port of test_page_combo.py::test_folder_checkbox_checks_and_unchecks_every_page_beneath_it and
// test_folder_checkbox_keeps_pages_checked_elsewhere (folder 'Folder' = p1, p2 and the nested p3, p4; p5 is elsewhere) and
// the item plan: selected = {C}; tap F gives {C, A, B}; tap F again with all of F selected gives {C}.
TEST(Lp10_FolderSelection, ACheckAddsTheFolderPagesAndAnUncheckRemovesOnlyThem)
{
    TempDb db("folder_toggle");
    seed_folder_bid(db);
    const ost_data::ProjectData project = load(db);
    const Uids pages = folder_pages(project, "10");
    REQUIRE_EQ(pages.size(), static_cast<size_t>(4));

    Selection selected = {"5"}; // p5 is checked elsewhere
    CHECK(ost_data::toggle_folder_pages(selected, pages));
    CHECK(selected == (Selection{"1", "2", "3", "4", "5"}));
    CHECK(ost_data::toggle_folder_pages(selected, pages));
    CHECK(selected == (Selection{"5"}));

    Selection empty;
    CHECK(ost_data::toggle_folder_pages(empty, pages));
    CHECK(empty == (Selection{"1", "2", "3", "4"}));
    CHECK(ost_data::toggle_folder_pages(empty, pages));
    CHECK(empty.empty());
}

// Port of test_toggling_a_partial_folder_checks_all_of_its_pages: p1 selected (a partially checked folder); the view toggles it
// to Checked: all four pages are selected, one update.
TEST(Lp10_FolderSelection, APartiallySelectedFolderIsCheckedNotUnchecked)
{
    TempDb db("folder_partial");
    seed_folder_bid(db);
    const ost_data::ProjectData project = load(db);
    const Uids pages = folder_pages(project, "10");

    Selection selected = {"1"};
    CHECK(ost_data::toggle_folder_pages(selected, pages));
    CHECK(selected == (Selection{"1", "2", "3", "4"}));

    // the nested folder alone: p3 selected, tapping Nested checks p3 and p4 only
    const Uids nested_pages = project.folders.at("10").subfolders.at("11").all_page_uids();
    CHECK_EQ(nested_pages, (Uids{"3", "4"}));
    Selection nested = {"3", "6"};
    CHECK(ost_data::toggle_folder_pages(nested, nested_pages));
    CHECK(nested == (Selection{"3", "4", "6"}));
}

// Port of test_folder_without_pages_has_no_checkbox: nothing to check, nothing changes.
TEST(Lp10_FolderSelection, AFolderWithoutPagesChangesNothing)
{
    Selection selected = {"7"};
    CHECK(!ost_data::toggle_folder_pages(selected, {}));
    CHECK(selected == (Selection{"7"}));
}

// The folder pages that are all selected already and nothing else: unchecking leaves the selection empty; selecting a folder
// whose pages are all selected and one more page elsewhere keeps the other page.
TEST(Lp10_FolderSelection, AnUncheckKeepsEveryOtherPage)
{
    const Uids pages = {"3", "4", "1", "2"};
    Selection selected = {"1", "2", "3", "4", "5", "6"};
    CHECK(ost_data::toggle_folder_pages(selected, pages));
    CHECK(selected == (Selection{"5", "6"}));
}

// ---------------------------------------------------------------------------------------------------------------
// LP-11
// ---------------------------------------------------------------------------------------------------------------

// Plan: with the active 2D page P1, activating P1 again leaves P1 active (the old iOS select2DPage cleared it); Windows
// emits no change then (test_clicking_a_page_row_activates_it_without_toggling_its_check: a second click on the same row
// emits nothing).
TEST(Lp11_ActivePage, ActivatingTheActivePageKeepsIt)
{
    std::string active = "P1";
    CHECK(!ost_data::activate_page(active, "P1"));
    CHECK_STR_EQ(active, "P1");
}

TEST(Lp11_ActivePage, ActivatingAnotherPageChangesTheActivePage)
{
    std::string active = "P1";
    CHECK(ost_data::activate_page(active, "P2"));
    CHECK_STR_EQ(active, "P2");
    std::string none;
    CHECK(ost_data::activate_page(none, "P2"));
    CHECK_STR_EQ(none, "P2");
}

// ---------------------------------------------------------------------------------------------------------------
// LP-25: the text of a page button
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    ost_domain::Page labelled_page(const std::string &uid, const std::string &name, int sequence = 0, const std::string &sheet_no = "")
    {
        ost_domain::Page page;
        page.uid = uid;
        page.name = name;
        page.sequence = sequence;
        page.sheet_no = sheet_no;
        return page;
    }
}

// Test plan of LP-25: a page with Name '' and UID 21 shows '21', a page named 'A101' shows 'A101' (both preferences are off by
// default, iOS has no such preferences).
TEST(Lp25_PageLabel, AnUnnamedPageShowsItsUid)
{
    CHECK_STR_EQ(ost_data::page_label(labelled_page("21", "")), "21");
    CHECK_STR_EQ(ost_data::page_label(labelled_page("21", "A101")), "A101");
    // a name of spaces is a name (Python `if page.name`): it is shown as it is
    CHECK_STR_EQ(ost_data::page_label(labelled_page("21", " ")), " ");
    // a sequence or sheet number does not show while the preferences are off
    CHECK_STR_EQ(ost_data::page_label(labelled_page("21", "", 3, "S1")), "21");
    CHECK_STR_EQ(ost_data::page_label(labelled_page("21", "A101", 3, "S1")), "A101");
}

// Port of test_page_label_preferences_update_page_combo_labels (page A101, sheet S1, sequence 3): off -> 'A101', index on ->
// '3 - A101', sheet number on -> 'S1 - A101', both -> '3 - S1 - A101'.
TEST(Lp25_PageLabel, SequenceAndSheetNumberFollowThePreferences)
{
    const ost_domain::Page page = labelled_page("p1", "A101", 3, "S1");
    CHECK_STR_EQ(ost_data::page_label(page, false, false), "A101");
    CHECK_STR_EQ(ost_data::page_label(page, true, false), "3 - A101");
    CHECK_STR_EQ(ost_data::page_label(page, false, true), "S1 - A101");
    CHECK_STR_EQ(ost_data::page_label(page, true, true), "3 - S1 - A101");
}

// Port of test_page_label_missing_sequence_does_not_show_duplicate_one: no sequence (0) is not shown even with the preference on.
// A label made of the preference parts alone (no name) keeps those parts, the uid is only the fallback of an empty label.
TEST(Lp25_PageLabel, AMissingSequenceOrSheetNumberIsNotShown)
{
    CHECK_STR_EQ(ost_data::page_label(labelled_page("p1", "A101", 0), true, false), "A101");
    CHECK_STR_EQ(ost_data::page_label(labelled_page("p1", "A101", 0, ""), true, true), "A101");
    CHECK_STR_EQ(ost_data::page_label(labelled_page("p1", "", 7, ""), true, false), "7");
    CHECK_STR_EQ(ost_data::page_label(labelled_page("p1", "", 0, ""), true, true), "p1");
    CHECK_STR_EQ(ost_data::page_label(labelled_page("p1", "", 2, "S9"), true, true), "2 - S9");
}
