// The rules of Rename Page (plan 5.5, see PageRules.hpp). Windows: rename_page_dialog.py:136-176 (_on_ok), ui_event_coordinator.py:5714-5731
// (the open guard), ui_access_manager.py (Feature.EDIT_PAGE_SETTINGS).
#include "PageRules.hpp"
#include "TextLimits.hpp"
#include "../Import/TextRules.hpp"
#include <cstddef>
#include <optional>
#include <string>

namespace ost_domain
{
    std::string normalize_page_name(const std::string &entered)
    {
        // rename_page_dialog.py:141 `new_name = self.new_name_edit.text().strip()`: the same Python strip as the folder rule.
        return ost_import::text_rules::py_strip(entered);
    }

    PageRenamePlan decide_page_rename(const std::string &entered, const std::string &stored_name)
    {
        PageRenamePlan plan;
        plan.new_name = normalize_page_name(entered); // the stripped text for EVERY decision

        // rename_page_dialog.py:142-145: blank after the strip -> "Validation Error" / "Page name cannot be empty.", nothing is written.
        if (plan.new_name.empty())
        {
            plan.decision = PageRenameDecision::Refused;
            return plan;
        }

        // iOS safety (N-M1-13, register RP-2): Windows checks neither the content nor the length of the name. A name that holds U+0000 would
        // be stored by sqlite3_bind_text with its explicit length but read back through a C string (cut at the NUL), and a name that is not
        // valid UTF-8 can neither be stored as text nor shown. The length is counted in UTF-16 units, the unit of the Access VARCHAR(255)
        // column (database_creator.py:373), so a name an iOS export holds can be opened on Windows again.
        const std::optional<std::size_t> units = utf16_length(plan.new_name);
        if (contains_nul(plan.new_name) || !units.has_value())
        {
            plan.decision = PageRenameDecision::Invalid;
            return plan;
        }
        if (*units > MAX_PAGE_NAME_UNITS)
        {
            plan.decision = PageRenameDecision::TooLong;
            return plan;
        }

        // rename_page_dialog.py:146-148 (RP-1): the STRIPPED typed text against the STORED, unstripped name, case-sensitive: equal -> no
        // write, the alert closes. A stored name with surrounding spaces is therefore rewritten stripped, and a case-only change is a write.
        if (plan.new_name == stored_name)
        {
            plan.decision = PageRenameDecision::Unchanged;
            return plan;
        }

        plan.decision = PageRenameDecision::Write;
        return plan;
    }

    DialogText page_name_refusal_text(PageRenameDecision decision)
    {
        switch (decision)
        {
        case PageRenameDecision::Refused:
            return {"Validation Error", "Page name cannot be empty."}; // rename_page_dialog.py:143
        case PageRenameDecision::TooLong:
            return {"Validation Error", "Page name cannot be longer than " + std::to_string(MAX_PAGE_NAME_UNITS) + " characters."};
        case PageRenameDecision::Invalid:
            return {"Validation Error", "Page name contains a character that cannot be saved."};
        case PageRenameDecision::Unchanged:
        case PageRenameDecision::Write:
            break;
        }
        return {std::string(), std::string()};
    }

    DialogText page_rename_failed_text()
    {
        return {"Save Failed", "Failed to rename page."}; // rename_page_dialog.py:173-175
    }

    bool can_rename_page(const RenameTargetState &state)
    {
        // ui_event_coordinator.py:5714-5731: a bid is selected and the page resolves; Feature.EDIT_PAGE_SETTINGS (ui_access_manager.py) is
        // refused while an area placement is in progress (_PLACEMENT_BLOCKED) or an inline text edit is active (_TEXT_EDIT_BLOCKED).
        return state.bid_open && state.target_page_exists_in_project && !state.area_placement_in_progress &&
               !state.placement_text_edit_active;
    }
}
