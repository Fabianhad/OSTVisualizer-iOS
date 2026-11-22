// BRW-19 (folder names are trimmed, blank names refused), BRW-21 (name fallbacks), BRW-01 (confirmation texts), DBP-11 (failure
// dialog texts). The Windows project browser keeps these rules in its project tree and write handler; Core/Domain/BrowserRules.hpp is
// the platform independent copy the iOS screen calls.
// Windows oracle:
//   tests/presentation/handlers/test_project_write_handler.py::test_rename_project_normalizes_the_name_and_picks_the_database,
//     test_rename_project_refuses_blank_names_and_unsaved_pending_changes, test_access_rename_failure_opens_one_critical_dialog,
//     test_trashing_one_bid_confirms_and_moves_it_with_its_original_project,
//     test_permanent_delete_confirms_with_the_bid_count_and_writes_per_database,
//     test_permanent_delete_of_one_bid_uses_the_singular_text, test_delete_projects_confirmation_text_counts_several_projects
//   tests/presentation/components/test_project_tree_view.py::test_project_inline_rename_commits_changed_name_and_ignores_blank_or_unchanged
//   tests/domain/entities/test_project_factory.py::ProjectFactoryHierarchyTests::test_build_projects_keeps_file_order_names_paths_and_adds_one_unassigned_group
#include "TestHarness.hpp"

#include "Domain/BrowserRules.hpp"
#include "Domain/DatabaseHierarchy.hpp"

#include <string>

using ost_domain::BrowserFailure;
using ost_domain::FolderRenameDecision;

// ---------------------------------------------------------------------------------------------------------------
// BRW-19: folder names
// ---------------------------------------------------------------------------------------------------------------

// Port of test_rename_project_normalizes_the_name_and_picks_the_database: "  New name \n" is written as "New name".
TEST(Brw19_FolderName, TheNameIsStrippedBeforeItIsWritten)
{
    CHECK_STR_EQ(ost_domain::normalize_folder_name("  New name \n"), "New name");
    CHECK_STR_EQ(ost_domain::normalize_folder_name("Renamed"), "Renamed");
    CHECK_STR_EQ(ost_domain::normalize_folder_name("  Renamed  "), "Renamed");
    // only the ends: inner whitespace stays
    CHECK_STR_EQ(ost_domain::normalize_folder_name(" a  b\tc "), "a  b\tc");
}

// Port of test_rename_project_refuses_blank_names_and_unsaved_pending_changes: "" and "   \t" are refused (no write).
TEST(Brw19_FolderName, ABlankNameIsRefused)
{
    CHECK_STR_EQ(ost_domain::normalize_folder_name(""), "");
    CHECK_STR_EQ(ost_domain::normalize_folder_name("   \t"), "");
    CHECK_STR_EQ(ost_domain::normalize_folder_name(" \n\r\t "), "");
}

// Python str.strip() (the Windows call) removes every Unicode whitespace as str.isspace() sees it. Expected values computed with the
// Windows interpreter: '\x1c\x1d\x1e\x1f \x85\xa0\u2003\u3000a b\u200b\ufeff'.strip() == 'a b\u200b\ufeff' (U+200B and U+FEFF are not
// whitespace), '\u200b x \u180e'.strip() is unchanged (U+180E is not whitespace since Unicode 6.3),
// '\x0b\x0c\u2028\u2029 q\u1680\u2000\u200a\u202f\u205f'.strip() == 'q'.
TEST(Brw19_FolderName, TheWhitespaceSetIsPythonsNotFoundations)
{
    CHECK_STR_EQ(ost_domain::normalize_folder_name("\x1c\x1d\x1e\x1f \xC2\x85\xC2\xA0\xE2\x80\x83\xE3\x80\x80"
                                                   "a b\xE2\x80\x8B\xEF\xBB\xBF"),
                 "a b\xE2\x80\x8B\xEF\xBB\xBF");
    CHECK_STR_EQ(ost_domain::normalize_folder_name("\xE2\x80\x8B x \xE1\xA0\x8E"), "\xE2\x80\x8B x \xE1\xA0\x8E");
    CHECK_STR_EQ(ost_domain::normalize_folder_name("\x0b\x0c\xE2\x80\xA8\xE2\x80\xA9 q\xE1\x9A\x80\xE2\x80\x80\xE2\x80\x8A\xE2\x80\xAF\xE2\x81\x9F"),
                 "q");
    // only whitespace of that set: nothing is left
    CHECK_STR_EQ(ost_domain::normalize_folder_name("\x1c\xC2\xA0\xE3\x80\x80"), "");
}

// Port of test_project_inline_rename_commits_changed_name_and_ignores_blank_or_unchanged (the project shows "Source"):
//   "  Renamed  " -> written as "Renamed";  the unchanged text "Source" -> no write;  "   " -> no write.
TEST(Brw19_FolderName, RenameWritesTheStrippedChangedNameOnly)
{
    std::string name;
    CHECK(ost_domain::decide_folder_rename("  Renamed  ", "Source", name) == FolderRenameDecision::Write);
    CHECK_STR_EQ(name, "Renamed");

    CHECK(ost_domain::decide_folder_rename("Source", "Source", name) == FolderRenameDecision::Unchanged);
    CHECK_STR_EQ(name, "Source");

    CHECK(ost_domain::decide_folder_rename("   ", "Source", name) == FolderRenameDecision::Refused);
    CHECK_STR_EQ(name, "");
    CHECK(ost_domain::decide_folder_rename("", "Source", name) == FolderRenameDecision::Refused);
}

// The comparison with the shown name is made AFTER the strip (the Windows tree strips item.text(0) first): " Source\n" is unchanged.
TEST(Brw19_FolderName, TheUnchangedCheckComparesTheStrippedText)
{
    std::string name;
    CHECK(ost_domain::decide_folder_rename(" Source\n", "Source", name) == FolderRenameDecision::Unchanged);
    // the comparison is exact: another case is another name
    CHECK(ost_domain::decide_folder_rename("source", "Source", name) == FolderRenameDecision::Write);
    CHECK_STR_EQ(name, "source");
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-21: what an empty name shows
// ---------------------------------------------------------------------------------------------------------------

// Port of test_build_projects_keeps_file_order_names_paths_and_adds_one_unassigned_group: project p2 with an empty name is shown as
// 'Project p2'; the bid analogue (project_tree_view bid row: bid.name or f"Bid {bid.uid}") is 'Bid b9'.
TEST(Brw21_NameFallback, AnEmptyNameShowsTheUid)
{
    CHECK_STR_EQ(ost_domain::display_name_for_folder("p2", ""), "Project p2");
    CHECK_STR_EQ(ost_domain::display_name_for_bid("b9", ""), "Bid b9");
    CHECK_STR_EQ(ost_domain::display_name_for_folder("7", ""), "Project 7");
    CHECK_STR_EQ(ost_domain::display_name_for_bid("12", ""), "Bid 12");
}

TEST(Brw21_NameFallback, AnyOtherNameIsShownAsItIs)
{
    CHECK_STR_EQ(ost_domain::display_name_for_folder("p1", "Source"), "Source");
    CHECK_STR_EQ(ost_domain::display_name_for_bid("b1", "Tower A"), "Tower A");
    // Python `name or fallback`: a name of spaces is truthy and is shown as it is
    CHECK_STR_EQ(ost_domain::display_name_for_folder("p1", "  "), "  ");
    CHECK_STR_EQ(ost_domain::display_name_for_bid("b1", " "), " ");
}

// ---------------------------------------------------------------------------------------------------------------
// BRW-01: the confirmations
// ---------------------------------------------------------------------------------------------------------------

// Port of test_trashing_one_bid_confirms_and_moves_it_with_its_original_project and the n bids variant
// ("Move 4 bids to 'Deleted Bids'?\nYou can permanently delete them from there.").
TEST(Brw01_Confirmation, TrashTextsAreTheWindowsStrings)
{
    ost_domain::DialogText one = ost_domain::trash_bids_confirmation(1);
    CHECK_STR_EQ(one.title, "Move to Deleted Bids");
    CHECK_STR_EQ(one.message, "Move this bid to 'Deleted Bids'?\nYou can permanently delete it from there.");

    ost_domain::DialogText four = ost_domain::trash_bids_confirmation(4);
    CHECK_STR_EQ(four.title, "Move to Deleted Bids");
    CHECK_STR_EQ(four.message, "Move 4 bids to 'Deleted Bids'?\nYou can permanently delete them from there.");

    CHECK_STR_EQ(ost_domain::trash_bids_confirmation(2).message, "Move 2 bids to 'Deleted Bids'?\nYou can permanently delete them from there.");
}

// Port of test_permanent_delete_confirms_with_the_bid_count_and_writes_per_database (3 bids) and
// test_permanent_delete_of_one_bid_uses_the_singular_text.
TEST(Brw01_Confirmation, PermanentDeleteTextsAreTheWindowsStrings)
{
    ost_domain::DialogText three = ost_domain::delete_bids_confirmation(3);
    CHECK_STR_EQ(three.title, "Delete Bids");
    CHECK_STR_EQ(three.message, "Permanently delete 3 bids and all their data?\nThis cannot be undone.");

    ost_domain::DialogText one = ost_domain::delete_bids_confirmation(1);
    CHECK_STR_EQ(one.title, "Delete Bids");
    CHECK_STR_EQ(one.message, "Permanently delete this bid and all its data?\nThis cannot be undone.");
}

// Port of test_delete_projects_confirmation_text_counts_several_projects (2 empty projects) + the single project text.
TEST(Brw01_Confirmation, DeleteFolderTextsAreTheWindowsStrings)
{
    ost_domain::DialogText two = ost_domain::delete_folders_confirmation(2);
    CHECK_STR_EQ(two.title, "Delete Projects");
    CHECK_STR_EQ(two.message, "Permanently delete 2 empty projects?\nThis cannot be undone.");

    ost_domain::DialogText one = ost_domain::delete_folders_confirmation(1);
    CHECK_STR_EQ(one.title, ost_domain::DELETE_FOLDER_CONFIRM_TITLE);
    CHECK_STR_EQ(one.message, ost_domain::DELETE_FOLDER_CONFIRM_MESSAGE);
    CHECK_STR_EQ(one.message, "Permanently delete this empty project?\nThis cannot be undone.");
}

// ---------------------------------------------------------------------------------------------------------------
// DBP-11: failure dialogs
// ---------------------------------------------------------------------------------------------------------------

// Port of test_access_rename_failure_opens_one_critical_dialog: ("Rename Error", f"Failed to rename project. {DB_LOCKED_HINT}") and the
// other critical dialogs of project_write_handler.py (lines 278, 367, 605, 698, 780, 987).
TEST(Dbp11_FailureText, TheWindowsCriticalDialogs)
{
    CHECK_STR_EQ(ost_domain::DB_LOCKED_HINT, "The database may be locked by another application.");

    const std::string hint = " The database may be locked by another application.";
    struct Case
    {
        BrowserFailure failure;
        const char *title;
        const char *message;
    };
    const Case cases[] = {
        {BrowserFailure::RenameFolder, "Rename Error", "Failed to rename project."},
        {BrowserFailure::DeleteBids, "Delete Error", "Failed to delete bids."},
        {BrowserFailure::TrashBids, "Move Error", "Failed to move bids to Deleted Bids."},
        {BrowserFailure::RestoreBids, "Restore Error", "Failed to restore bids."},
        {BrowserFailure::MoveBids, "Move Error", "Failed to move bids."},
        {BrowserFailure::DeleteFolders, "Delete Error", "Failed to delete projects."},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string(c.title) + " / " + c.message);
        ost_domain::DialogText text = ost_domain::browser_failure_text(c.failure);
        CHECK_STR_EQ(text.title, c.title);
        CHECK_STR_EQ(text.message, std::string(c.message) + hint);
    }
}

// Port of file_operation_handler.py show_warning("Error Loading File", f"Failed to load {name}:\n{error_message}").
TEST(Dbp11_FailureText, TheLoadFailureDialog)
{
    ost_domain::DialogText text = ost_domain::load_failure_text("Tower A", "BidTakeoffs.UID=7 participates in a ParentUID cycle.");
    CHECK_STR_EQ(text.title, "Error Loading File");
    CHECK_STR_EQ(text.message, "Failed to load Tower A:\nBidTakeoffs.UID=7 participates in a ParentUID cycle.");
}
