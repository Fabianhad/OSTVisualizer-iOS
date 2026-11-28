// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The rules of Rename Page (plan 5.5). The screen (the rename alert of the page menu) calls these functions and only adds the widgets.
//
// Windows sources:
//   ost_visualizer/presentation/dialogs/rename_page_dialog.py:136-176   _on_ok: strip, the empty refusal, the unchanged shortcut, the
//                                                                        save and its failure text
//   ost_visualizer/presentation/coordinators/ui_event_coordinator.py:5714-5731   open_rename_page_dialog: the open guard
//   ost_visualizer/presentation/managers/ui_access_manager.py            Feature.EDIT_PAGE_SETTINGS (area placement, inline text edit)
//   ost_visualizer/infrastructure/mdb/database_creator.py:373           BidPages.Name VARCHAR(255)
#pragma once
#include "BrowserRules.hpp"
#include <cstddef>
#include <string>

namespace ost_domain
{
    // The longest page name, in UTF-16 code units (the unit of the Access VARCHAR(255) column, plan 4.2 "Text limits", N-M1-13). Windows
    // checks no length at all (register RP-2): this is the safe behaviour. The units are counted by ost_domain::utf16_length.
    constexpr std::size_t MAX_PAGE_NAME_UNITS = 255;

    // The name as it is written: Python str.strip() of what was typed (rename_page_dialog.py:141), the same function as
    // normalize_folder_name (ost_import::text_rules::py_strip: all Unicode whitespace as str.isspace(), including U+001C..U+001F and
    // U+0085). Nothing else is changed (case, inner spaces, control characters other than whitespace stay).
    std::string normalize_page_name(const std::string &entered);

    enum class PageRenameDecision
    {
        Refused,   // blank after the strip: "Validation Error" / "Page name cannot be empty.", nothing is written, the alert stays
        TooLong,   // iOS safety (N-M1-13): more than MAX_PAGE_NAME_UNITS UTF-16 units; nothing is written
        Invalid,   // iOS safety (N-M1-13): contains U+0000 or is not valid UTF-8; nothing is written
        Unchanged, // equal to the stored name: nothing is written, the alert closes
        Write      // write `new_name`
    };

    struct PageRenamePlan
    {
        PageRenameDecision decision = PageRenameDecision::Refused;
        std::string new_name; // the stripped text for EVERY decision (empty for Refused)
    };

    // rename_page_dialog._on_ok (:141-148) plus the iOS safety rules, in this order:
    //   1. strip; empty -> Refused;
    //   2. contains NUL or is not valid UTF-8 -> Invalid;
    //   3. more than MAX_PAGE_NAME_UNITS UTF-16 units -> TooLong;
    //   4. equal to `stored_name` (the stripped text against the STORED, unstripped name, case-sensitive: RP-1) -> Unchanged;
    //   5. otherwise Write.
    PageRenamePlan decide_page_rename(const std::string &entered, const std::string &stored_name);

    // The alert of a refusal: Refused {"Validation Error", "Page name cannot be empty."} (Windows, :143);
    // TooLong {"Validation Error", "Page name cannot be longer than 255 characters."} (iOS, N-M1-13);
    // Invalid {"Validation Error", "Page name contains a character that cannot be saved."} (iOS wording, N-M1-03 may reword);
    // Unchanged and Write have nothing to show: both strings empty.
    DialogText page_name_refusal_text(PageRenameDecision decision);

    // A write that failed (rename_page_dialog.py:173-175): {"Save Failed", "Failed to rename page."}.
    DialogText page_rename_failed_text();

    // The open guard (ui_event_coordinator.py:5714-5731 and the Feature.EDIT_PAGE_SETTINGS gate of ui_access_manager.py): whether the
    // menu item "Rename Page..." may act. Every flag false means "no".
    struct RenameTargetState
    {
        bool bid_open = false;                      // a bid is open (Windows: a selected bid ref exists)
        bool target_page_exists_in_project = false; // the pressed page is a page of the open project (Windows: the active page is in the
                                                    // page targets of the sidebar and resolves; blank or unknown uid = false)
        bool area_placement_in_progress = false;    // Windows _PLACEMENT_BLOCKED (an area placement is running)
        bool placement_text_edit_active = false;    // Windows _TEXT_EDIT_BLOCKED (no iOS counterpart yet)
    };

    // true only when bid_open and target_page_exists_in_project and neither blocking flag is set.
    bool can_rename_page(const RenameTargetState &state);
}
