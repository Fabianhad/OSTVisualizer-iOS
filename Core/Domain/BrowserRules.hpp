// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The rules of the project browser that the Windows app keeps in its project tree and write handler and that have no
// database in them: how a folder name is normalised before it is written (BRW-19), what is shown for an empty folder or
// bid name (BRW-21), the confirmation before a bid is trashed or deleted for good (BRW-01) and the failure dialogs of the
// browser's writes (DBP-11). The screens (DatabaseViewController) call these functions and only add the platform widgets.
//
// Windows sources:
//   ost_visualizer/presentation/components/project_tree_view.py        _on_rename_editor_closed, bid row text
//   ost_visualizer/presentation/handlers/project_write_handler.py      rename_project, _delete_bids, restore_bids, move_bids,
//                                                                       _delete_projects
//   ost_visualizer/domain/entities/project_factory.py                  build_projects (the project name fallback)
//   ost_visualizer/presentation/utils/messagebox.py                    DB_LOCKED_HINT
#pragma once
#include <cstddef>
#include <string>

namespace ost_domain
{
    // ---- BRW-19: folder names ---------------------------------------------------------------------------------------------

    // The name as it is written: Python str.strip() of what was typed (surrounding Unicode whitespace as str.isspace() sees it,
    // which is broader than NSCharacterSet whitespaceAndNewlineCharacterSet: it also removes U+001C..U+001F and U+0085). An empty
    // result means the name is refused, nothing is written (project_write_handler.rename_project: "not new_name.strip()").
    // Applies to a new folder as well as to a rename.
    std::string normalize_folder_name(const std::string &entered);

    enum class FolderRenameDecision
    {
        Refused,   // blank or whitespace-only: no write, the old name stays
        Unchanged, // the same name as the one shown: no write
        Write      // write `new_name`
    };

    // project_tree_view._on_rename_editor_closed: the typed text is stripped; blank is refused; a name equal to the name the editor
    // started with (the text the tree SHOWS, i.e. display_name_for_folder) is a no-op; anything else is written stripped.
    // `new_name` is set for every decision (the stripped text).
    FolderRenameDecision decide_folder_rename(const std::string &entered, const std::string &shown_name, std::string &new_name);

    // ---- BRW-21: what an empty name shows ---------------------------------------------------------------------------------

    // project_factory.build_projects: `project_info.name or f"Project {project_uid}"`: only the EMPTY name falls back (a name of
    // spaces is shown as it is).
    std::string display_name_for_folder(const std::string &folder_uid, const std::string &name);

    // project_tree_view: `bid.name or f"Bid {bid.uid}"`.
    std::string display_name_for_bid(const std::string &bid_uid, const std::string &name);

    // ---- BRW-01: confirmation before a bid leaves the browser -------------------------------------------------------------

    struct DialogText
    {
        std::string title;
        std::string message;
    };

    // project_write_handler._delete_bids, bids that are NOT in Deleted Bids: they are moved there after a confirmation.
    //   1 bid : "Move to Deleted Bids" / "Move this bid to Deleted Bids?\nYou can permanently delete it from there."
    //   n bids: "Move to Deleted Bids" / "Move {n} bids to Deleted Bids?\nYou can permanently delete them from there."
    DialogText trash_bids_confirmation(size_t count);

    // _delete_bids, bids that ARE in Deleted Bids: permanent delete after a confirmation.
    //   1 bid : "Delete Bids" / "Permanently delete this bid and all its data?\nThis cannot be undone."
    //   n bids: "Delete Bids" / "Permanently delete {n} bids and all their data?\nThis cannot be undone."
    DialogText delete_bids_confirmation(size_t count);

    // _delete_projects: the confirmation of deleting empty projects (the browser deletes one folder at a time).
    //   1: "Delete Projects" / "Permanently delete this empty project?\nThis cannot be undone."
    //   n: "Delete Projects" / "Permanently delete {n} empty projects?\nThis cannot be undone."
    DialogText delete_folders_confirmation(size_t count);

    // ---- DBP-11: a write that failed ---------------------------------------------------------------------------------------

    // messagebox.DB_LOCKED_HINT, appended to the Windows failure texts.
    constexpr const char *DB_LOCKED_HINT = "The database may be locked by another application.";

    enum class BrowserFailure
    {
        RenameFolder,   // "Rename Error"  / "Failed to rename project. <hint>"                     (project_write_handler.rename_project)
        DeleteBids,     // "Delete Error"  / "Failed to delete bids. <hint>"                       (_delete_bids, permanent)
        TrashBids,      // "Move Error"    / "Failed to move bids to Deleted Bids. <hint>"         (_delete_bids, trash)
        RestoreBids,    // "Restore Error" / "Failed to restore bids. <hint>"                      (restore_bids)
        MoveBids,       // "Move Error"    / "Failed to move bids. <hint>"                         (move_bids)
        DeleteFolders,  // "Delete Error"  / "Failed to delete projects. <hint>"                   (_delete_projects)
        CreateFolder,   // "New Project"   / "Failed to create project. <hint>"                    (iOS wording: Windows shows the exception text of the queued create)
        SaveSettings    // "Save Failed"   / "Failed to save the display settings."                (iOS wording: Windows saves the display mode without a dialog)
    };

    DialogText browser_failure_text(BrowserFailure failure);

    // file_operation_handler: a database that cannot be read is "Error Loading File" / "Failed to load <name>:\n<reason>". The reason is a
    // user sentence (load_failure_reason in FailureText.hpp); the text the store reports is logged, never shown (EXP-21).
    DialogText load_failure_text(const std::string &display_name, const std::string &reason);
}
