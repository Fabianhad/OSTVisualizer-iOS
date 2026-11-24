// M1 Delete Page (plan 5.6): the pure rules of Core/Domain/PageDeletion.* and Core/Data/PageDeletePlan.*: may this page be deleted, which page is shown afterwards,
// the name of the page in the dialog, the normalisation of the list, the texts.
// Windows 49a2d59 sources: ui_event_coordinator.py:5881-5893 can_delete_current_page, :5989-6007 _stage_selection_after_page_delete, :6009-6015
// _page_delete_display_name; project_write_service.py:6884-6892 _unique_nonempty_uids; messagebox.py:8, :52-61.
// Windows tests ported: Sp3a5hStageSelectionAfterPageDeleteTests (test_ui_event_coordinator.py:31872-31921: no sidebar -> nothing staged, a page not in the order
// -> not staged, the last page -> refused with "Cannot delete the last page in the bid.", order a, b, c: a -> b, b -> c, c -> b), Sp3a5hPageDeleteDisplayNameTests
// (:31978-32002), the can-delete rules of Sp3a5hDeleteCurrentPageTests (:31655-31693, the parts that depend on the page tree), test_project_write_service.py:1656
// (test_delete_pages_normalizes_empty_and_duplicate_uids) and its parametrised rows (:4296, :4407).
// The expected values are the answers of the REAL Windows functions (DeletePageOracleData.hpp); the next-page rule is executed for every deleted position of
// orders of 1 to 8 pages, in a flat order and in a folder tree order.
#include "DeletePageTestSupport.hpp"

#include "Data/PageDeletePlan.hpp"
#include "Data/PageNavigation.hpp"

#include <set>
#include <string>
#include <vector>

using ost_ptest::TempDb;

namespace
{
    std::vector<std::string> split(const std::string &text)
    {
        std::vector<std::string> out;
        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t comma = text.find(',', start);
            if (comma == std::string::npos)
                comma = text.size();
            if (comma > start)
                out.push_back(text.substr(start, comma - start));
            start = comma + 1;
        }
        return out;
    }

    // A project (not loaded from a database) whose pages are loose pages in exactly this order: page_order = `order`, no folder.
    ost_data::ProjectData flat_project(const std::vector<std::string> &order)
    {
        ost_data::ProjectData project;
        project.bid.uid = "10";
        int sequence = 1;
        for (const std::string &uid : order)
        {
            ost_domain::Page page;
            page.uid = uid;
            page.name = "Page " + uid;
            page.sequence = sequence++;
            project.pages[uid] = page;
            project.page_order.push_back(uid);
        }
        return project;
    }

    const char *refusal_name(ost_data::PageDeleteRefusal refusal)
    {
        switch (refusal)
        {
        case ost_data::PageDeleteRefusal::None: return "None";
        case ost_data::PageDeleteRefusal::NoProject: return "NoProject";
        case ost_data::PageDeleteRefusal::UnknownPage: return "UnknownPage";
        case ost_data::PageDeleteRefusal::LastPage: return "LastPage";
        }
        return "?";
    }

    // Checks one oracle case against the plan of `project`.
    void expect_oracle_case(const ost_data::ProjectData &project, const ost_delpage_oracle::NextPageCase &c)
    {
        SCOPED_CONTEXT(std::string("order [") + c.order + "] deleting [" + c.deleted + "]");
        const ost_data::PageDeletePlan plan = ost_data::plan_page_delete(project, c.deleted);
        const std::string outcome = c.outcome;
        if (outcome == "ok")
        {
            CHECK(plan.allowed);
            CHECK_STR_EQ(refusal_name(plan.refusal), "None");
            CHECK_STR_EQ(plan.next_page_uid, c.next);
        }
        else
        {
            CHECK_FALSE(plan.allowed);
            CHECK_STR_EQ(refusal_name(plan.refusal), outcome == "last" ? "LastPage" : "UnknownPage");
            CHECK_STR_EQ(plan.next_page_uid, "");
        }
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------------------------
// The next page (ui_event_coordinator.py:5989-6007)
// ---------------------------------------------------------------------------------------------------------------------------------

// Port of :31872-31921 on the real Windows function: for every order of 1 to 6 pages (a, b, ...) and every deleted page: the page that FOLLOWED the deleted
// one is selected, the previous page when the last one goes (a, b, c: a -> b, b -> c, c -> b); a one-page order is the last-page refusal; a page that is not in
// the order is not staged. 27 cases of the oracle.
TEST(DelPagePlan, TheNextPageIsTheOneThatFollowsAndThePreviousOneForTheLast)
{
    int checked = 0;
    for (const ost_delpage_oracle::NextPageCase &c : ost_delpage_oracle::next_page_cases())
    {
        const std::vector<std::string> order = split(c.order);
        if (order.empty() || order[0].size() != 1 || order[0][0] < 'a' || order[0][0] > 'f')
            continue; // the folder / numeric orders have their own tests
        const ost_data::ProjectData project = flat_project(order);
        expect_oracle_case(project, c);
        ++checked;
    }
    CHECK_EQ(checked, 27);
}

// Port of test_deleting_the_first_page_selects_the_next_page / ..._a_middle_page_... / ..._the_last_page_selects_the_previous_page (:31905-31921) by name:
// order a, b, c.
TEST(DelPagePlan, TheThreePageExampleOfTheWindowsTests)
{
    const ost_data::ProjectData project = flat_project({"a", "b", "c"});
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "a").next_page_uid, "b");
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "b").next_page_uid, "c");
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "c").next_page_uid, "b");
    CHECK(ost_data::plan_page_delete(project, "a").allowed);
}

// Port of test_last_remaining_page_cannot_be_deleted_and_warns (:31894): one page -> refused, LastPage, nothing staged (no next page).
TEST(DelPagePlan, TheOnlyPageIsTheLastPageRefusal)
{
    const ost_data::ProjectData project = flat_project({"p1"});
    const ost_data::PageDeletePlan plan = ost_data::plan_page_delete(project, "p1");
    CHECK_FALSE(plan.allowed);
    CHECK_STR_EQ(refusal_name(plan.refusal), "LastPage");
    CHECK_STR_EQ(plan.next_page_uid, "");
}

// Port of test_page_missing_from_the_sidebar_order_is_not_staged (:31888) and the can-delete rules "no active page uid" / "the active page is not loaded"
// (:31665-31680): an empty uid and a uid that is no page of the project are UnknownPage; a default project (no bid loaded) is NoProject whatever the uid.
TEST(DelPagePlan, UnknownPagesAndAnEmptyProjectAreRefused)
{
    const ost_data::ProjectData project = flat_project({"p1", "p2"});
    for (const char *uid : {"", "p9", "P1", " p1", "p1 ", "0"})
    {
        SCOPED_CONTEXT(std::string("uid [") + uid + "]");
        const ost_data::PageDeletePlan plan = ost_data::plan_page_delete(project, uid);
        CHECK_FALSE(plan.allowed);
        CHECK_STR_EQ(refusal_name(plan.refusal), "UnknownPage");
        CHECK_STR_EQ(plan.next_page_uid, "");
        CHECK_STR_EQ(plan.display_name, "");
    }
    const ost_data::ProjectData empty;
    for (const char *uid : {"", "p1", "10"})
    {
        const ost_data::PageDeletePlan plan = ost_data::plan_page_delete(empty, uid);
        CHECK_FALSE(plan.allowed);
        CHECK_STR_EQ(refusal_name(plan.refusal), "NoProject");
        CHECK_STR_EQ(plan.next_page_uid, "");
    }
}

// The orders of the oracle with numeric uids and a long order (8 pages), as flat orders.
TEST(DelPagePlan, NumericAndLongOrdersFollowTheWindowsRule)
{
    int checked = 0;
    for (const ost_delpage_oracle::NextPageCase &c : ost_delpage_oracle::next_page_cases())
    {
        const std::vector<std::string> order = split(c.order);
        if (order.empty() || (order[0][0] >= 'a' && order[0][0] <= 'f'))
            continue;
        expect_oracle_case(flat_project(order), c);
        ++checked;
    }
    CHECK_EQ(checked, 23);
}

// The tree order, not the page order: the page selector walks the folders first, each folder's SUBFOLDERS before its own pages, and the loose pages last
// (page_combo.py _walk_page_uids; ost_data::page_tree_order). With the fixture of test_page_combo.py (folder f1: pages 1, 2 and subfolder f2: pages 3, 4;
// folder f3: page 5; loose: page 6) the order is 3, 4, 1, 2, 5, 6 and the Windows function gives, for each deleted page, the page that follows IN THAT ORDER
// (3 -> 4, 4 -> 1, 1 -> 2, 2 -> 5, 5 -> 6, 6 -> 5). Loaded from a database, like the app does.
TEST(DelPagePlan, TheNextPageFollowsTheFolderTreeOrder)
{
    TempDb db("delpage_plan_tree");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'one', 1)");
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (10, 1, 'Folder')");
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name, ParentUID) VALUES (11, 1, 'Nested', 10)");
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (12, 1, 'Other')");
    const struct
    {
        int uid;
        const char *folder;
    } pages[] = {{1, "10"}, {2, "10"}, {3, "11"}, {4, "11"}, {5, "12"}, {6, "NULL"}};
    for (const auto &page : pages)
        db.must("INSERT INTO BidPages (UID, BidUID, Name, Sequence, Index1, BidPageFolderUID) VALUES (" + std::to_string(page.uid) + ", 1, 'A10" +
                std::to_string(page.uid) + "', " + std::to_string(page.uid) + ", 1, " + page.folder + ")");
    ost_data::ProjectData project;
    std::string error;
    REQUIRE(ost_store::load_project(db.path(), "1", project, &error));
    REQUIRE_EQ(ost_data::page_tree_order(project), (std::vector<std::string>{"3", "4", "1", "2", "5", "6"}));

    int checked = 0;
    for (const ost_delpage_oracle::NextPageCase &c : ost_delpage_oracle::next_page_cases())
        if (std::string(c.order) == "3,4,1,2,5,6")
        {
            expect_oracle_case(project, c);
            ++checked;
        }
    CHECK_EQ(checked, 7);
    // the page order (Sequence) would have said 1 -> 2, 2 -> 3, 3 -> 4: the plan does not use it
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "2").next_page_uid, "5");
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "4").next_page_uid, "1");
}

// A page whose only neighbour is in another folder, and a project with one page inside a folder: the folder does not matter, the last page is still the last page.
TEST(DelPagePlan, AFolderedOnlyPageIsTheLastPage)
{
    TempDb db("delpage_plan_folder_only");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'one', 1)");
    db.must("INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (10, 1, 'Folder')");
    db.must("INSERT INTO BidPages (UID, BidUID, Name, Sequence, BidPageFolderUID) VALUES (1, 1, 'Only', 1, 10)");
    ost_data::ProjectData project;
    REQUIRE(ost_store::load_project(db.path(), "1", project));
    const ost_data::PageDeletePlan plan = ost_data::plan_page_delete(project, "1");
    CHECK_FALSE(plan.allowed);
    CHECK_STR_EQ(refusal_name(plan.refusal), "LastPage");
    // a second page, loose, makes it deletable and the loose page follows
    db.must("INSERT INTO BidPages (UID, BidUID, Name, Sequence) VALUES (2, 1, 'Loose', 2)");
    REQUIRE(ost_store::load_project(db.path(), "1", project));
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "1").next_page_uid, "2");
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "2").next_page_uid, "1");
}

// 200 pages: the first goes to the second, a middle page to its successor, the last to its predecessor; and the plan never changes the project.
TEST(DelPagePlan, TwoHundredPagesAndTheProjectIsNeverChanged)
{
    std::vector<std::string> order;
    for (int i = 1; i <= 200; ++i)
        order.push_back("u" + std::to_string(i));
    const ost_data::ProjectData project = flat_project(order);
    const std::string before = ost_delpage::describe(project);
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "u1").next_page_uid, "u2");
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "u100").next_page_uid, "u101");
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "u199").next_page_uid, "u200");
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "u200").next_page_uid, "u199");
    CHECK_EQ(ost_delpage::describe(project), before);
    CHECK_EQ(project.pages.size(), static_cast<std::size_t>(200));
    CHECK_EQ(project.page_order.size(), static_cast<std::size_t>(200));
}

// The plan carries the name the dialog shows: the page name, else the file name of the image, else "Page <uid>" (the same function the dialog uses).
TEST(DelPagePlan, TheAllowedPlanCarriesTheDisplayName)
{
    ost_data::ProjectData project = flat_project({"1", "2", "3"});
    project.pages["1"].name = "Lobby";
    project.pages["2"].name = "";
    project.pages["2"].image_path = "C:\\plans\\sheet 7.pdf";
    project.pages["3"].name = "";
    project.pages["3"].image_path = "";
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "1").display_name, "Lobby");
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "2").display_name, "sheet 7.pdf");
    CHECK_STR_EQ(ost_data::plan_page_delete(project, "3").display_name, "Page 3");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// The display name (ui_event_coordinator.py:6009-6015, test :31978-32002)
// ---------------------------------------------------------------------------------------------------------------------------------

// Port of Sp3a5hPageDeleteDisplayNameTests on the real function: a name wins over the image path; the nine path spellings give the file name; no image gives
// "Page u9"; plus a name of one space (a name is only "empty" when it has no characters), non-ASCII names and a path with spaces.
TEST(DelPageRules, TheDisplayNameIsTheNameThenTheFileNameThenThePageUid)
{
    CHECK_EQ(ost_delpage_oracle::display_name_cases().size(), static_cast<std::size_t>(16));
    for (const ost_delpage_oracle::DisplayNameCase &c : ost_delpage_oracle::display_name_cases())
    {
        SCOPED_CONTEXT(std::string("name [") + c.name + "] image [" + c.image_path + "]");
        ost_domain::Page page;
        page.uid = "u9";
        page.name = c.name;
        page.image_path = c.image_path;
        CHECK_STR_EQ(ost_domain::page_delete_display_name(page), c.expected);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// The list (project_write_service.py:6884-6892)
// ---------------------------------------------------------------------------------------------------------------------------------

// Port of test_delete_pages_normalizes_empty_and_duplicate_uids (:1656: ["page-1", "", "page-1", "page-2"] gives ["page-1", "page-2"]), the parametrised rows
// (:4296 ["20", "", "21", "20"] gives ["20", "21"]; :4407 ["", ""] gives nothing) and the other lists of the oracle: repeats and empty texts go, the first order
// stays, nothing else is changed ("007" and "7", " 7" stay different entries: the store judges the uids).
TEST(DelPageRules, TheListLosesEmptyTextsAndRepeatsAndKeepsTheFirstOrder)
{
    CHECK_EQ(ost_delpage_oracle::normalize_cases().size(), static_cast<std::size_t>(8));
    for (const ost_delpage_oracle::NormalizeCase &c : ost_delpage_oracle::normalize_cases())
    {
        std::vector<std::string> input(c.input.begin(), c.input.end());
        std::vector<std::string> expected(c.expected.begin(), c.expected.end());
        std::string label;
        for (const std::string &uid : input)
            label += "[" + uid + "]";
        SCOPED_CONTEXT("input " + label);
        CHECK_EQ(ost_domain::normalize_page_uids(input), expected);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// The texts (plan 5.6 touch adaptation, owner decisions N-M1-03 and N-M1-23; Windows messagebox.py:52-61 and ui_event_coordinator.py:5915-5968)
// ---------------------------------------------------------------------------------------------------------------------------------

// The confirmation: title "Delete Verification" (the Windows title). A page with content: the iOS wording that names every kind of content and says "from this
// bid" (register DP-3: the Windows text promises the removal "in the Base Bid, and any Alternates or Change Orders" which the cascade does not do); an empty page:
// the shorter text (decision D2: iOS always asks). The name goes into the quotes as it is.
TEST(DelPageRules, TheConfirmationTextsAreTheAgreedOnes)
{
    const ost_domain::DialogText with_content = ost_domain::page_delete_confirmation("Lobby", true);
    CHECK_STR_EQ(with_content.title, "Delete Verification");
    CHECK_STR_EQ(with_content.message,
                 "The Page 'Lobby' contains takeoffs, annotations, comments, named views or hot links. Deleting this page will remove the page and everything on it from this bid.\n\n"
                 "Deleting a Page cannot be undone, are you sure you want to delete this Page?");
    CHECK(with_content.message.find("Alternates") == std::string::npos);
    CHECK(with_content.message.find("Change Orders") == std::string::npos);

    const ost_domain::DialogText empty = ost_domain::page_delete_confirmation("Lobby", false);
    CHECK_STR_EQ(empty.title, "Delete Verification");
    CHECK_STR_EQ(empty.message, "Delete the Page 'Lobby'?\n\nDeleting a Page cannot be undone.");
}

// The name is inserted verbatim (Windows f-string): quotes, percent signs, braces, newlines, non-ASCII and a very long name are neither escaped nor cut.
TEST(DelPageRules, TheNameIsInsertedVerbatimIntoTheConfirmation)
{
    const std::string long_name(300, 'x');
    for (const std::string &name : {std::string("O'Brien"), std::string("100% {0} %s \\n"), std::string("two\nlines"), std::string("caf\xC3\xA9 \xE4\xB8\xAD"), long_name, std::string(" ")})
    {
        SCOPED_CONTEXT("name [" + name + "]");
        CHECK_STR_EQ(ost_domain::page_delete_confirmation(name, true).message.substr(0, 10 + name.size()), "The Page '" + name);
        CHECK(ost_domain::page_delete_confirmation(name, true).message.find("The Page '" + name + "' contains takeoffs") == 0);
        CHECK_STR_EQ(ost_domain::page_delete_confirmation(name, false).message, "Delete the Page '" + name + "'?\n\nDeleting a Page cannot be undone.");
    }
}

// The failure dialogs (ui_event_coordinator.py:5915-5921 and :5962-5968): title "Delete Page", text "Failed to ... <DB_LOCKED_HINT>"; the last-page text and title.
TEST(DelPageRules, TheFailureTextsAreTheWindowsOnes)
{
    const ost_domain::DialogText verify = ost_domain::page_delete_failure_text(ost_domain::PageDeleteFailure::VerifyContent);
    CHECK_STR_EQ(verify.title, "Delete Page");
    CHECK_STR_EQ(verify.message, "Failed to verify page contents. The database may be locked by another application.");
    const ost_domain::DialogText failed = ost_domain::page_delete_failure_text(ost_domain::PageDeleteFailure::Delete);
    CHECK_STR_EQ(failed.title, "Delete Page");
    CHECK_STR_EQ(failed.message, "Failed to delete page. The database may be locked by another application.");
    CHECK(failed.message.find(ost_domain::DB_LOCKED_HINT) != std::string::npos);
    CHECK_STR_EQ(ost_domain::LAST_PAGE_TITLE, "Delete Page");
    CHECK_STR_EQ(ost_domain::LAST_PAGE_MESSAGE, "Cannot delete the last page in the bid.");
}
