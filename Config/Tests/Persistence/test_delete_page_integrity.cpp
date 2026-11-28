// M1 Delete Page (plan 5.6): what the delete must leave alone and what must stay consistent afterwards (research map section 4: locked / shared
// layers, other bids, cycles and graph shapes, integer limits, survivor integrity, end-to-end load, undo). Every "untouched" claim is a
// before / after comparison of the rows themselves (row_dump, dump, snapshot_all), never a count alone.
// Windows 49a2d59 behaviours pinned here: settings_operations.py:701-711 (master clear, same bid only), :712-741 (comment detach), :763-775
// (takeoff references), :866-889 (selected page and cover sheet selection are NOT bid scoped), :832-865 (named views and hot links). The graph
// shapes (cycles, holes across pages) are data the Windows loader refuses (bid_owned_identity.py:53-65, require_acyclic_bid_owned_parent_graph):
// the delete must still complete and the bid must load afterwards.
#include "DeletePageTestSupport.hpp"

#include "Data/PageNavigation.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using ost_delpage::run;
using ost_delpage::Uids;
using ost_ptest::TempDb;

namespace
{
    // seed_two_bids (bid 10: pages 100, 101, conditions 30, 31; bid 20: page 200, takeoff 900) plus page 102 of bid 10.
    void world(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        ost_ptest::seed_two_bids(db);
        db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (102, 10, 'P102')");
    }

    std::string pages_of(const TempDb &db, const std::string &bid)
    {
        return ost_ptest::list(db, "SELECT UID FROM BidPages WHERE BidUID = " + bid + " ORDER BY UID");
    }

    // `text` (the output of ost_delpage::describe) without the page, its takeoffs, and with the saved page cleared when it was that page.
    std::string without_page(const std::string &text, const std::string &page_uid)
    {
        std::istringstream in(text);
        std::string out;
        std::string line;
        while (std::getline(in, line))
        {
            if (line.rfind("page " + page_uid + " ", 0) == 0)
                continue;
            if (line.rfind("takeoff ", 0) == 0 && line.find(" page=" + page_uid + " ") != std::string::npos)
                continue;
            if (line == "selected=" + page_uid)
                line = "selected=";
            out += line + "\n";
        }
        return out;
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------------------------
// Layers, conditions, other bids
// ---------------------------------------------------------------------------------------------------------------------------------

// Layers are never modified: the bid's own layer, a locked shared template layer (BidUID NULL), the Image layer that create_schema makes and a layer of
// another bid keep every column. The annotations of the page that used them are deleted, the ones of the surviving page are not, and the conditions
// that use the layers stay exactly as they were (Windows touches no layer row, research map 2.3).
TEST(DelPageIntegrity, LayersAndConditionsAreNeverTouchedWhileTheAnnotationsOfThePageGo)
{
    TempDb db("delpage_int_layers");
    world(db);
    db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence, IsLocked, IsTemplate) VALUES (71, NULL, 'Shared template', -1, 2, -1, -1)");
    db.must("UPDATE BidConditions SET BidLayerUID = 70 WHERE UID = 30");
    db.must("UPDATE BidConditions SET BidLayerUID = 71 WHERE UID = 31");
    const char *layered[] = {"BidTexts", "BidHighlights", "BidCallOuts", "BidAnnotationRects", "BidAnnotationOvals", "BidAnnotationPolygons",
                             "BidAnnotationClouds", "BidComments", "BidHotLinks"};
    int uid = 1;
    for (const char *table : layered)
    {
        const std::string t = table;
        db.must("INSERT INTO [" + t + "] (UID, BidUID, BidPageUID, BidLayerUID) VALUES (" + std::to_string(uid++) + ", 10, 100, 70)");
        db.must("INSERT INTO [" + t + "] (UID, BidUID, BidPageUID, BidLayerUID) VALUES (" + std::to_string(uid++) + ", 10, 100, 71)");
        db.must("INSERT INTO [" + t + "] (UID, BidUID, BidPageUID, BidLayerUID) VALUES (" + std::to_string(uid++) + ", 10, 100, (SELECT UID FROM BidLayers WHERE Name = 'Image' LIMIT 1))");
        db.must("INSERT INTO [" + t + "] (UID, BidUID, BidPageUID, BidLayerUID) VALUES (" + std::to_string(uid++) + ", 10, 101, 70)");
    }
    const std::string layers_before = db.dump("BidLayers");
    const std::string conditions_before = db.dump("BidConditions");
    REQUIRE(layers_before.find("Image") != std::string::npos);

    CHECK(run(db, "10", {"100"}));
    CHECK_EQ(db.dump("BidLayers"), layers_before);
    CHECK_EQ(db.dump("BidConditions"), conditions_before);
    for (const char *table : layered)
    {
        SCOPED_CONTEXT(std::string("table ") + table);
        CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM [") + table + "] WHERE BidPageUID = 100"), 0);
        CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM [") + table + "] WHERE BidPageUID = 101 AND BidLayerUID = 70"), 1);
    }
}

// A page of bid 10 is deleted: everything of bid 20 (its page, takeoff, conditions, layers, areas) is exactly as before (valid data, no references
// between the bids), and so are the other pages of bid 10 and its conditions, areas and layers.
TEST(DelPageIntegrity, OtherBidsAndOtherPagesAreUntouched)
{
    TempDb db("delpage_int_other_bids");
    world(db);
    const std::string bid20_before = ost_delpage::row_dump(db, "BidPages", "BidUID = 20") + "|" + ost_delpage::row_dump(db, "BidTakeoffs", "BidUID = 20") + "|" +
                                     ost_delpage::row_dump(db, "BidConditions", "BidUID = 20") + "|" + ost_delpage::row_dump(db, "BidAreas", "BidUID = 20") +
                                     "|" + ost_delpage::row_dump(db, "BidLayers", "BidUID = 20") + "|" + ost_delpage::row_dump(db, "Bids", "UID = 20");
    const std::string page101_before = ost_delpage::row_dump(db, "BidPages", "UID = 101") + "|" + ost_delpage::row_dump(db, "BidPages", "UID = 102");
    const std::string others_before = db.dump("BidConditions") + db.dump("BidAreas") + db.dump("BidLayers") + db.dump("BidProjects") + db.dump("Bids");

    CHECK(run(db, "10", {"100"}));
    const std::string bid20_after = ost_delpage::row_dump(db, "BidPages", "BidUID = 20") + "|" + ost_delpage::row_dump(db, "BidTakeoffs", "BidUID = 20") + "|" +
                                    ost_delpage::row_dump(db, "BidConditions", "BidUID = 20") + "|" + ost_delpage::row_dump(db, "BidAreas", "BidUID = 20") +
                                    "|" + ost_delpage::row_dump(db, "BidLayers", "BidUID = 20") + "|" + ost_delpage::row_dump(db, "Bids", "UID = 20");
    CHECK_EQ(bid20_after, bid20_before);
    CHECK_EQ(ost_delpage::row_dump(db, "BidPages", "UID = 101") + "|" + ost_delpage::row_dump(db, "BidPages", "UID = 102"), page101_before);
    CHECK_EQ(db.dump("BidConditions") + db.dump("BidAreas") + db.dump("BidLayers") + db.dump("BidProjects") + db.dump("Bids"), others_before);
    CHECK_STR_EQ(pages_of(db, "10"), "101,102");
    CHECK_STR_EQ(pages_of(db, "20"), "200");
}

// Windows steps 13 and 14 are NOT bid scoped (settings_operations.py:866-889): the saved page of ANY BidSettings row and the cover sheet selection of type 1 of
// ANY bid that name the deleted page are cleared; a selection of another page, and a selection of another type, stay.
TEST(DelPageIntegrity, TheSavedPageAndTheCoverSheetSelectionAreClearedInEveryBidThatNamesThePage)
{
    TempDb db("delpage_int_selection");
    world(db);
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (1, 10, 100)");
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (2, 20, 100)");
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (3, 20, 200)");
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (4, 10, 101)");
    db.must("UPDATE Bids SET CoverSheetSelItemType = 1, CoverSheetSelItemUID = 100 WHERE UID = 10");
    db.must("UPDATE Bids SET CoverSheetSelItemType = 1, CoverSheetSelItemUID = 100 WHERE UID = 20");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, CoverSheetSelItemType, CoverSheetSelItemUID) VALUES (30, 2, 'third', 2, 100)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, CoverSheetSelItemType, CoverSheetSelItemUID) VALUES (31, 2, 'fourth', 1, 101)");

    CHECK(run(db, "10", {"100"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(BidPageSelectedUID, 'N') FROM BidSettings ORDER BY UID"), "1:N,2:N,3:200,4:101");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(CoverSheetSelItemType, 'N') || ':' || ifnull(CoverSheetSelItemUID, 'N') FROM Bids WHERE UID IN (10, 20, 30, 31) ORDER BY UID"),
                 "10:1:N,20:1:N,30:2:100,31:1:101");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Cycles and graph shapes (data the Windows loader refuses; the delete completes and the bid loads afterwards)
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
    // bid 10 with pages 1000 'A', 1001 'B', 1002 'C' and one condition 7000, created through seed_pages (a bid the store loads).
    void loadable_bid(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        ost_ptest::seed_pages(db, 10, std::vector<ost_ptest::SeedPage>{{"A"}, {"B"}, {"C"}});
        db.must("INSERT OR IGNORE INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (7000, 10, 1, 'Seed condition', 0)");
    }

    void add_takeoff(const TempDb &db, int uid, int page, const std::string &parent_sql = "0", const std::string &extra_columns = "", const std::string &extra_values = "")
    {
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, ParentUID, Position" + extra_columns + ") VALUES (" + std::to_string(uid) +
                ", 10, 7000, " + std::to_string(page) + ", " + parent_sql + ", '1;2;3;4'" + extra_values + ")");
    }
} // namespace

// Two takeoffs of the page that are each other's parent (a ParentUID cycle the loader refuses, "participates in a ParentUID cycle"): the delete completes,
// both are gone and the bid loads again.
TEST(DelPageIntegrity, ATakeoffCycleOnThePageDoesNotStopTheDeleteAndTheBidLoadsAfterwards)
{
    TempDb db("delpage_int_cycle");
    loadable_bid(db);
    add_takeoff(db, 7, 1000, "8");
    add_takeoff(db, 8, 1000, "7");
    add_takeoff(db, 9, 1001);
    ost_data::ProjectData project;
    std::string load_error;
    REQUIRE(!ost_store::load_project(db.path(), "10", project, &load_error));

    CHECK(run(db, "10", {"1000"}));
    CHECK(ost_store::load_project(db.path(), "10", project, &load_error));
    CHECK_STR_EQ(load_error, "");
    REQUIRE_EQ(project.takeoffs.size(), static_cast<size_t>(1));
    CHECK_STR_EQ(project.takeoffs[0].uid, "9");
}

// A hole on ANOTHER page whose parent is on the deleted page (invalid data: "A takeoff and its parent must belong to the same page"): the hole is
// detached (ParentUID NULL, a root) and kept, not deleted with the page; the bid that could not load before loads now.
TEST(DelPageIntegrity, AHoleOnAnotherPageWhoseParentIsOnTheDeletedPageIsDetachedNotDeleted)
{
    TempDb db("delpage_int_hole");
    loadable_bid(db);
    add_takeoff(db, 20, 1000);
    add_takeoff(db, 21, 1001, "20");
    ost_data::ProjectData project;
    REQUIRE(!ost_store::load_project(db.path(), "10", project));

    CHECK(run(db, "10", {"1000"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(ParentUID, 'N') FROM BidTakeoffs ORDER BY UID"), "21:N");
    REQUIRE(ost_store::load_project(db.path(), "10", project));
    REQUIRE_EQ(project.takeoffs.size(), static_cast<size_t>(1));
    CHECK_STR_EQ(project.takeoffs[0].parent_uid, "0");
}

// Typical-group references across pages: a takeoff of a surviving page that points at a takeoff of the deleted page through TypGroupTakeoffUID,
// TypPageTakeoffUID or TypGroupMarkerUID loses those references (and keeps the ones to a surviving takeoff).
TEST(DelPageIntegrity, TypicalGroupReferencesAcrossPagesAreClearedOnTheSurvivor)
{
    TempDb db("delpage_int_typical");
    loadable_bid(db);
    add_takeoff(db, 30, 1000);
    add_takeoff(db, 31, 1001);
    add_takeoff(db, 32, 1001, "0", ", TypGroupTakeoffUID, TypPageTakeoffUID, TypGroupMarkerUID", ", 30, 31, 30");

    CHECK(run(db, "10", {"1000"}));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(TypGroupTakeoffUID, 'N') || ',' || ifnull(TypPageTakeoffUID, 'N') || ',' || ifnull(TypGroupMarkerUID, 'N') FROM BidTakeoffs WHERE UID = 32"),
                 "32:N,31,N");
    ost_data::ProjectData project;
    CHECK(ost_store::load_project(db.path(), "10", project));
}

// A comment thread over three pages: comment 1 on page A, 2 on B (reply to 1), 3 on C (reply to 2). Deleting A makes 2 a root and keeps 3 under 2; deleting
// B deletes 2 and makes 3 a root while 1 stays; deleting A and B together leaves 3 as a root.
TEST(DelPageIntegrity, ACommentThreadAcrossThreePagesIsDetachedAtTheDeletedPages)
{
    struct Case
    {
        Uids delete_pages;
        const char *expected;
    };
    const Case cases[] = {{{"1000"}, "2:N,3:2"}, {{"1001"}, "1:N,3:N"}, {{"1000", "1001"}, "3:N"}};
    for (const Case &c : cases)
    {
        std::string label;
        for (const std::string &uid : c.delete_pages)
            label += "[" + uid + "]";
        SCOPED_CONTEXT("deleting " + label);
        TempDb db("delpage_int_thread");
        loadable_bid(db);
        db.must("INSERT INTO BidComments (UID, BidUID, BidPageUID, ParentCommentUID) VALUES (1, 10, 1000, NULL)");
        db.must("INSERT INTO BidComments (UID, BidUID, BidPageUID, ParentCommentUID) VALUES (2, 10, 1001, 1)");
        db.must("INSERT INTO BidComments (UID, BidUID, BidPageUID, ParentCommentUID) VALUES (3, 10, 1002, 2)");
        CHECK(run(db, "10", c.delete_pages));
        CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || ifnull(ParentCommentUID, 'N') FROM BidComments ORDER BY UID"), c.expected);
    }
}

// A BidPageFolders.ParentUID cycle in the data is none of the delete's business: the folder rows are byte-identical afterwards and the folder that held the
// deleted page stays, now empty (research map 2.3: no code touches BidPageFolders).
TEST(DelPageIntegrity, PageFoldersAreNeverTouchedNotEvenAnEmptiedOneOrACycle)
{
    TempDb db("delpage_int_folders");
    loadable_bid(db);
    db.must("INSERT INTO BidPageFolders (UID, BidUID, ParentUID, Name) VALUES (91, 10, 92, 'cyclic one')");
    db.must("INSERT INTO BidPageFolders (UID, BidUID, ParentUID, Name) VALUES (92, 10, 91, 'cyclic two')");
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (93, 10, 'will be empty')");
    db.must("UPDATE BidPages SET BidPageFolderUID = 93 WHERE UID = 1000");
    db.must("UPDATE BidPages SET BidPageFolderUID = 91 WHERE UID = 1001");
    const std::string folders_before = db.dump("BidPageFolders");

    CHECK(run(db, "10", {"1000"}));
    CHECK_EQ(db.dump("BidPageFolders"), folders_before);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE BidPageFolderUID = 93"), 0);
    CHECK_EQ(db.count("SELECT BidPageFolderUID FROM BidPages WHERE UID = 1001"), 91);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Survivors, files, the loaded project
// ---------------------------------------------------------------------------------------------------------------------------------

// Nothing is renumbered and no survivor changes a column: with four pages of a multi-page PDF (the same ImagePath, Index1 1..4, Sequence 1..4, a folder, a
// scale) deleting page 2 leaves the other three rows byte-identical, gaps in Sequence / Index1 and the shared ImagePath included.
TEST(DelPageIntegrity, SurvivorsKeepEveryColumnAndNothingIsRenumbered)
{
    TempDb db("delpage_int_survivors");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_pages(db, 10, std::vector<ost_ptest::SeedPage>{{"Sheet 1"}, {"Sheet 2", 0.5, 24.0}, {"Sheet 3"}, {"Sheet 4", 1.0, 12.0}});
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (90, 10, 'Plans')");
    db.must("UPDATE BidPages SET ImagePath = 'C:\\plans\\set.pdf', OverlayImagePath = 'C:\\plans\\overlay.png', BidPageFolderUID = 90");
    std::string before[4];
    for (int i = 0; i < 4; ++i)
        before[i] = ost_delpage::row_dump(db, "BidPages", "UID = " + std::to_string(1000 + i));
    REQUIRE(before[0].find("ImagePath='C:\\plans\\set.pdf'") != std::string::npos);

    CHECK(run(db, "10", {"1001"}));
    CHECK_EQ(ost_delpage::row_dump(db, "BidPages", "UID = 1001"), "");
    for (const int i : {0, 2, 3})
    {
        SCOPED_CONTEXT("page " + std::to_string(1000 + i));
        CHECK_EQ(ost_delpage::row_dump(db, "BidPages", "UID = " + std::to_string(1000 + i)), before[i]);
    }
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT CAST(Sequence AS INTEGER) || '/' || CAST(Index1 AS INTEGER) FROM BidPages WHERE BidUID = 10 ORDER BY UID"), "1/1,3/3,4/4");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPageFolders WHERE UID = 90"), 1);
}

// Image files are never deleted (research map 2.3, plan decision D6): a real file at the page's ImagePath is still there, byte for byte, after the delete.
TEST(DelPageIntegrity, TheImageFileOfTheDeletedPageIsNeverDeleted)
{
    TempDb db("delpage_int_image_file");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_pages(db, 10, 2);
    const std::string image = ost_test::temp_path("delpage_image_" + ost_ptest::run_token() + ".pdf");
    {
        std::ofstream file(image, std::ios::binary);
        file << "%PDF-1.4 not a real pdf, a file that must survive";
    }
    db.must("UPDATE BidPages SET ImagePath = " + ost_ptest::quoted(image));
    const auto size_before = std::filesystem::file_size(image);

    CHECK(run(db, "10", {"1000"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE UID = 1000"), 0);
    CHECK(std::filesystem::exists(image));
    CHECK_EQ(static_cast<unsigned long long>(std::filesystem::file_size(image)), static_cast<unsigned long long>(size_before));
    std::remove(image.c_str());
}

// End to end: a bid with four pages (takeoffs 2, 1, 0, 3 on them) is loaded, its second page (the saved one) is deleted, and the bid loaded again equals the
// first load minus that page and its takeoffs: the same conditions, the same other pages in the same order, the saved page cleared, no error. The page that
// opens first is then the first page of the tree.
TEST(DelPageIntegrity, TheLoadedProjectAfterTheDeleteIsTheProjectBeforeMinusThePage)
{
    TempDb db("delpage_int_end_to_end");
    REQUIRE(db.schema_ok());
    std::vector<ost_ptest::SeedPage> pages = {{"A", 0.25, 12.0, 2}, {"B", 0.25, 12.0, 1}, {"C", 0.25, 12.0, 0}, {"D", 0.25, 12.0, 3}};
    ost_ptest::seed_pages(db, 10, pages);
    db.must("INSERT INTO BidSettings (UID, BidUID, BidPageSelectedUID) VALUES (1, 10, 1001)");
    ost_data::ProjectData before;
    std::string error;
    REQUIRE(ost_store::load_project(db.path(), "10", before, &error));
    REQUIRE_EQ(before.takeoffs.size(), static_cast<size_t>(6));
    CHECK_STR_EQ(before.selected_page_uid, "1001");
    const std::string before_text = ost_delpage::describe(before);

    CHECK(run(db, "10", {"1001"}));
    ost_data::ProjectData after;
    REQUIRE(ost_store::load_project(db.path(), "10", after, &error));
    CHECK_STR_EQ(error, "");
    CHECK_EQ(after.takeoffs.size(), static_cast<size_t>(5));
    CHECK_EQ(after.pages.size(), static_cast<size_t>(3));
    CHECK_EQ(ost_delpage::describe(after), without_page(before_text, "1001"));
    CHECK_STR_EQ(after.selected_page_uid, "");
    CHECK_STR_EQ(ost_data::initial_page_uid(after), "1000");
}

// The same with two pages deleted in one call (the first and the last of the tree): the project equals the project before minus both.
TEST(DelPageIntegrity, TwoPagesDeletedInOneCallLeaveTheProjectMinusBoth)
{
    TempDb db("delpage_int_end_to_end_two");
    REQUIRE(db.schema_ok());
    std::vector<ost_ptest::SeedPage> pages = {{"A", 0.25, 12.0, 2}, {"B", 0.25, 12.0, 1}, {"C", 0.25, 12.0, 0}, {"D", 0.25, 12.0, 3}};
    ost_ptest::seed_pages(db, 10, pages);
    ost_data::ProjectData before;
    REQUIRE(ost_store::load_project(db.path(), "10", before));
    const std::string expected = without_page(without_page(ost_delpage::describe(before), "1000"), "1003");

    CHECK(run(db, "10", {"1000", "1003"}));
    ost_data::ProjectData after;
    REQUIRE(ost_store::load_project(db.path(), "10", after));
    CHECK_EQ(ost_delpage::describe(after), expected);
    CHECK_EQ(after.takeoffs.size(), static_cast<size_t>(1));
}

// Undo: a "re-insert the takeoff on the deleted page" that is still in the history is refused by insert_takeoff (the page must belong to the bid, ProjectStore+Takeoffs.cpp:319-321)
// and writes nothing; the page that survived still takes a takeoff.
TEST(DelPageIntegrity, AStaleReinsertOnTheDeletedPageIsRefused)
{
    TempDb db("delpage_int_stale_reinsert");
    world(db);
    CHECK(run(db, "10", {"100"}));
    const std::string before = db.snapshot_all();

    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("30", "100")), "");
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("30", "101")), "901");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// The 2147483647 limits
// ---------------------------------------------------------------------------------------------------------------------------------

// Access Long columns hold at most 2147483647; a page with that uid, and takeoffs, annotations, comments and percents with that uid, are deleted completely.
// SQLite also holds 2147483648 and 9223372036854775807 (no Windows analog: an .ost cannot have them; documented, and the delete works there too).
TEST(DelPageIntegrity, AHighestUidPageWithHighestUidRowsIsDeletedCompletely)
{
    for (const char *uid : {"2147483647", "2147483648", "9223372036854775807"})
    {
        SCOPED_CONTEXT(std::string("uid ") + uid);
        TempDb db("delpage_int_limits");
        world(db);
        const std::string u = uid;
        db.must("INSERT INTO BidPages (UID, BidUID, Name, MasterPageUID) VALUES (" + u + ", 10, 'highest', NULL)");
        db.must("UPDATE BidPages SET MasterPageUID = " + u + " WHERE UID = 101");
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (" + u + ", 10, 30, " + u + ", '1;2;3;4')");
        db.must("INSERT INTO BidDimensions (UID, BidUID, BidPageUID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (" + u + ", 10, 101, " + u + ", 500)");
        db.must("INSERT INTO BidComments (UID, BidUID, BidPageUID) VALUES (" + u + ", 10, " + u + ")");
        db.must("INSERT INTO BidPercents (UID, BidTakeoffUID) VALUES (" + u + ", " + u + ")");
        db.must("INSERT INTO BidTexts (UID, BidUID, BidPageUID) VALUES (" + u + ", 10, " + u + ")");

        std::string error;
        CHECK(run(db, "10", {uid}, error));
        CHECK_STR_EQ(error, "");
        for (const char *table : {"BidTakeoffs", "BidDimensions", "BidComments", "BidPercents", "BidTexts"})
        {
            SCOPED_CONTEXT(std::string("table ") + table);
            CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM [") + table + "] WHERE UID = " + u), 0);
        }
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE UID = " + u), 0);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE UID = 101 AND MasterPageUID IS NULL"), 1);
    }
}

// A condition at the highest number keeps its row and its number (the delete never changes a RefNo or a condition UID), and the guard of Duplicate Condition
// still refuses the overflow afterwards (the text of the Windows limit, condition_operations.py:56-62); nothing is written by the refusal.
TEST(DelPageIntegrity, TheConditionNumberLimitIsNotAffected)
{
    TempDb db("delpage_int_refno_limit");
    world(db);
    db.must("UPDATE BidConditions SET RefNo = 2147483647 WHERE UID = 31");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (800, 10, 31, 100, '1;2;3;4')");
    const std::string conditions_before = db.dump("BidConditions");

    CHECK(run(db, "10", {"100"}));
    CHECK_STR_EQ(pages_of(db, "10"), "101,102"); // the delete really happened (page 100 and its takeoff are gone)
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 800"), 0);
    CHECK_EQ(db.dump("BidConditions"), conditions_before);
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 31"), 2147483647);

    const std::string before = db.snapshot_all();
    std::string error;
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "10", "31", &error), "");
    CHECK_STR_EQ(error, "Cannot add 1 condition: the new condition numbers would reach 2147483648, past 2147483647, the highest condition number.");
    CHECK_EQ(db.snapshot_all(), before);
}

// After the takeoff with the highest uid went with its page, the next takeoff gets MAX(UID) + 1 (the uid of the deleted one) and NOTHING references that uid:
// the survivors' references to the deleted takeoff (parent, typical group, percents, annotation ends) were cleared or deleted by the cascade, so the new takeoff
// cannot adopt a dangling reference (integrity::next_uid_preserving_references).
TEST(DelPageIntegrity, TheNextTakeoffUidAfterDeletingTheHighestOneIsNotReferencedByAnything)
{
    TempDb db("delpage_int_next_uid");
    world(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (950, 10, 30, 101, '1;2;3;4')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (951, 10, 30, 100, '1;2;3;4')");
    db.must("UPDATE BidTakeoffs SET ParentUID = 951, TypGroupTakeoffUID = 951, TypPageTakeoffUID = 951, TypGroupMarkerUID = 951 WHERE UID = 950");
    db.must("INSERT INTO BidPercents (UID, BidTakeoffUID) VALUES (3951, 951)");
    db.must("INSERT INTO BidArrows (UID, BidUID, BidPageUID, BidTakeoffFromUID, BidTakeoffToUID) VALUES (4951, 10, 101, 950, 951)");

    CHECK(run(db, "10", {"100"}));
    const std::string uid = ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("30", "101"));
    CHECK_STR_EQ(uid, "951");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID <> 951 AND (ParentUID = 951 OR TypGroupTakeoffUID = 951 OR TypPageTakeoffUID = 951 OR TypGroupMarkerUID = 951)"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPercents WHERE BidTakeoffUID = 951"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidArrows WHERE BidTakeoffFromUID = 951 OR BidTakeoffToUID = 951"), 0);
}
