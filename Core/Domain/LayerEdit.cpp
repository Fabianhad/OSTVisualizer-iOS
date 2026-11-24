// Layer: Domain. The rules of the M1 layer editing (see LayerEdit.hpp for the contract of each function and the Windows lines).
// Pure C++: no sqlite, no Foundation. The store re-runs check_layer_name inside its own transaction (ProjectStore+Layers.cpp), so these rules
// cannot be bypassed by a screen.
#include "LayerEdit.hpp"

#include "TextLimits.hpp"
#include "../Import/TextRules.hpp"

#include <unordered_set>

namespace ost_domain
{

    std::string duplicate_layer_message(const std::string &name)
    {
        // layers_dialog.py:405, :435; layers_sidebar.py:338, :350: f"Layer {name} already exists."
        return "Layer " + name + " already exists.";
    }

    bool can_modify_bid_layer(const BidLayer &layer)
    {
        // layers_dialog.py:285-288 (SELECT mode) `not layer.is_template and not layer.is_locked`; layers_sidebar.py:230-231. A shared template row
        // (IsTemplate and IsLocked) and the bid's own locked copy of a template layer both fail it.
        return !layer.is_template && !layer.is_locked;
    }

    bool can_delete_bid_layer(const BidLayer &layer)
    {
        // layers_dialog.py:290-291 `_can_modify(layer) and not is_locked_built_in_default_layer(layer)`: in the bid dialog the second half is
        // already false for every row that passes the first (a built-in default is locked AND template), so a layer with the uid "1" that is
        // an ordinary bid layer stays deletable (test_layers_dialog.py:818).
        return can_modify_bid_layer(layer);
    }

    LayerNameCheck check_layer_name(const std::string &typed, const std::string &stored_name, const std::vector<BidLayer> &listed,
                                    const std::unordered_map<std::string, Layer> &own_layers, const std::string &exclude_uid)
    {
        const bool is_create = exclude_uid.empty();
        LayerNameCheck check;
        // 1. name = typed.strip() (Python Unicode whitespace; layers_dialog.py:397, :430). Blank: nothing happens, no message.
        check.name = ost_import::text_rules::py_strip(typed);
        if (check.name.empty())
        {
            check.verdict = LayerNameVerdict::Blank;
            return check;
        }
        // 2. rename only: `new_name == layer.name` (layers_dialog.py:401-402), exact and case sensitive, the stored text as it is.
        if (!is_create && check.name == stored_name)
        {
            check.verdict = LayerNameVerdict::Unchanged;
            return check;
        }
        // 3. control characters and line breaks (LY-14); text that is not valid UTF-8 counts too (both predicates answer true for it).
        if (has_control_character(check.name) || has_line_break(check.name))
        {
            check.verdict = LayerNameVerdict::ControlCharacter;
            check.title = is_create ? kNewLayerTitle : kRenameLayerTitle;
            check.message = std::string(is_create ? kNewLayerFailed : kRenameLayerFailed) + "\n" + kLayerNameControlHint;
            return check;
        }
        // 4. at most 50 UTF-16 units (Name VARCHAR(50), LY-8). utf16_length is empty only for invalid UTF-8, which step 3 already refused;
        // an empty answer is refused here as well so that this function never accepts text it could not count.
        const std::optional<std::size_t> units = utf16_length(check.name);
        if (!units.has_value() || *units > kMaxLayerNameUnits)
        {
            check.verdict = units.has_value() ? LayerNameVerdict::TooLong : LayerNameVerdict::ControlCharacter;
            check.title = is_create ? kNewLayerTitle : kRenameLayerTitle;
            check.message = std::string(is_create ? kNewLayerFailed : kRenameLayerFailed) + "\n" +
                            (units.has_value() ? kLayerNameTooLongHint : kLayerNameControlHint);
            return check;
        }
        // 5. the Comments layer is never listed, so Windows lets "Comments" through (LY-1); here it is always taken, with the duplicate text.
        if (is_comments_layer_name(check.name))
        {
            check.verdict = LayerNameVerdict::Reserved;
            check.title = kDuplicateLayerTitle;
            check.message = duplicate_layer_message(check.name);
            return check;
        }
        // 6. _find_layer_by_name (layers_dialog.py:293-298): normalize_layer_name (strip + Python lower) against every row but the layer's own.
        // The listed rows are Windows' set; the bid's own layers add the rows the merge hid (stricter, never looser).
        const std::string key = normalize_layer_name(check.name);
        bool taken = false;
        for (const BidLayer &row : listed)
        {
            if (row.uid != exclude_uid && normalize_layer_name(row.name) == key)
            {
                taken = true;
                break;
            }
        }
        if (!taken)
        {
            for (const auto &entry : own_layers)
            {
                if (entry.first != exclude_uid && normalize_layer_name(entry.second.name) == key)
                {
                    taken = true;
                    break;
                }
            }
        }
        if (taken)
        {
            check.verdict = LayerNameVerdict::Duplicate;
            check.title = kDuplicateLayerTitle;
            check.message = duplicate_layer_message(check.name);
            return check;
        }
        // 7.
        check.verdict = LayerNameVerdict::Accept;
        return check;
    }

    long long new_layer_after_sequence(const std::vector<BidLayer> &listed, const std::string &selected_uid)
    {
        // layers_dialog.py:315-322 / layers_sidebar.py: after_sequence = the selected row's sequence, else _max_sequence (:299-306).
        if (!selected_uid.empty())
        {
            for (const BidLayer &row : listed)
            {
                if (row.uid == selected_uid)
                    return static_cast<long long>(row.sequence);
            }
        }
        if (listed.empty())
            return 0;
        long long highest = listed.front().sequence;
        for (const BidLayer &row : listed)
        {
            if (static_cast<long long>(row.sequence) > highest)
                highest = row.sequence;
        }
        return highest;
    }

    LayerMovePlan plan_layer_move(const std::vector<BidLayer> &listed, const std::string &bid_uid, const std::string &uid, int direction)
    {
        LayerMovePlan plan;
        auto refuse = [&plan](const char *why)
        {
            plan.allowed = false;
            plan.neighbor_uid.clear();
            plan.refusal = why;
            return plan;
        };
        if (direction != -1 && direction != 1)
            return refuse("direction is not -1 or +1");
        if (uid.empty())
            return refuse("no layer");
        std::size_t index = listed.size();
        for (std::size_t i = 0; i < listed.size(); ++i)
        {
            if (listed[i].uid == uid)
            {
                index = i;
                break;
            }
        }
        if (index == listed.size())
            return refuse("the layer is not listed");
        const BidLayer &row = listed[index];
        // layers_dialog.py:634 `not self._can_modify(layer)`; a row of another bid is not the open bid's own.
        if (row.bid_uid != bid_uid)
            return refuse("the layer belongs to another bid");
        if (!can_modify_bid_layer(row))
            return refuse("the layer is locked or a template");
        // :634-636 `target = row + direction`, `target < 0 or target >= len` has no neighbour; the neighbour is the adjacent LISTED row.
        if ((direction < 0 && index == 0) || (direction > 0 && index + 1 >= listed.size()))
            return refuse("there is no neighbour in that direction");
        const BidLayer &neighbor = listed[direction < 0 ? index - 1 : index + 1];
        // LY-6: swapping with a shared template row (no bid, or another bid's) raises the raw scope error on Windows: the move is disabled.
        // The bid's own locked copy is allowed (N-M1-19): its Sequence is rewritten.
        if (is_shared_template_layer(neighbor, bid_uid) || neighbor.bid_uid != bid_uid)
            return refuse("the neighbour is a shared template layer");
        plan.allowed = true;
        plan.neighbor_uid = neighbor.uid;
        plan.refusal.clear();
        return plan;
    }

    LayerDeletePlan plan_layer_delete(const std::vector<BidLayer> &listed, const std::vector<std::string> &selected_uids,
                                      const std::set<std::string> &used_uids)
    {
        LayerDeletePlan plan;
        // layers_dialog.py:490-496: only the selected rows that _can_delete go on; each uid once, in the order of the selection.
        std::unordered_set<std::string> seen;
        for (const std::string &uid : selected_uids)
        {
            if (!seen.insert(uid).second)
                continue;
            for (const BidLayer &row : listed)
            {
                if (row.uid != uid)
                    continue;
                if (can_delete_bid_layer(row))
                {
                    // messagebox.py:84-85 `str(uid) in used_uids`: the uid text decides
                    LayerRef ref{row.uid, row.name};
                    if (used_uids.count(row.uid) != 0)
                        plan.in_use.push_back(std::move(ref));
                    else
                        plan.deletable.push_back(std::move(ref));
                }
                break;
            }
        }
        if (plan.in_use.empty() && plan.deletable.empty())
        {
            plan.kind = LayerDeleteKind::NothingToDelete; // nothing happens, no message (layers_dialog.py:495-496)
            return plan;
        }
        plan.title = kDeleteLayerTitle;
        std::string lines;
        for (const LayerRef &ref : plan.in_use)
            lines += std::string(lines.empty() ? "" : "\n") + "Cannot delete \"" + ref.name + "\" because it is in use.";
        if (!plan.in_use.empty() && !plan.deletable.empty())
        {
            // messagebox.py:86-98
            plan.kind = LayerDeleteKind::ConfirmMixed;
            plan.message = lines + "\n\nDelete the other " + std::to_string(plan.deletable.size()) + " item(s)?";
        }
        else if (!plan.in_use.empty())
        {
            // messagebox.py:101-106
            plan.kind = LayerDeleteKind::OnlyInUse;
            plan.message = lines;
        }
        else
        {
            // messagebox.py:107-118
            plan.kind = LayerDeleteKind::Confirm;
            plan.message = "Delete " + std::to_string(plan.deletable.size()) + " item(s)?";
        }
        return plan;
    }

    bool layer_sequence_would_overflow(long long after_sequence, long long highest_later_sequence)
    {
        // The new row gets after_sequence + 1: compare against the limits shifted by one so that no sum is formed (64-bit safe for LLONG_MIN
        // and LLONG_MAX alike).
        if (after_sequence < kMinLayerSequence - 1 || after_sequence > kMaxLayerSequence - 1)
            return true;
        // A later row moves up by one: it must still fit. -1 stands for "no later row" and is far below the limit.
        if (highest_later_sequence >= kMaxLayerSequence)
            return true;
        return false;
    }

}
