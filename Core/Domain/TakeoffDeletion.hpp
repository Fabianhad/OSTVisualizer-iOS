// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The set of takeoffs a delete removes (and an undo restores), extracted from UndoRedoController.deleteSelection.
//
// Windows: ost_visualizer/domain/services/takeoff_domain_service.py expand_takeoff_uids_with_descendants and
// presentation/handlers/plan_view_action_handler.py on_elements_deleted: deleting a takeoff deletes EVERY descendant through
// the parent_uid links (any depth, whether or not the selected takeoff is a root); the history restores the whole set.
#pragma once
#include "Takeoff.hpp"
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_domain
{

    // Windows expand_takeoff_uids_with_descendants(takeoffs, selected_uids): the selected uids (kept even when no takeoff has
    // such a uid) plus every takeoff whose parent_uid chain reaches one of them. Siblings and ancestors are not included. A
    // cycle in the parent links terminates (each uid enters the set once).
    inline std::set<std::string> expand_takeoff_uids_with_descendants(const std::vector<Takeoff> &takeoffs,
                                                                      const std::vector<std::string> &selected_uids)
    {
        std::unordered_map<std::string, std::vector<std::string>> children;
        for (const Takeoff &takeoff : takeoffs)
            children[takeoff.parent_uid].push_back(takeoff.uid);

        std::set<std::string> expanded(selected_uids.begin(), selected_uids.end());
        std::vector<std::string> pending(expanded.begin(), expanded.end());
        while (!pending.empty())
        {
            const std::string uid = std::move(pending.back());
            pending.pop_back();
            auto found = children.find(uid);
            if (found == children.end())
                continue;
            for (const std::string &child : found->second)
            {
                if (expanded.insert(child).second)
                    pending.push_back(child);
            }
        }
        return expanded;
    }

    // The takeoffs one delete removes, as complete copies for the history entry (Windows on_elements_deleted saved_takeoffs):
    // the selected takeoffs that exist (selection order, a repeated or unknown uid is ignored), then every other descendant in
    // list order. iOS restores a takeoff with its own uid (Windows allocates a new one), so a takeoff that has its parent in
    // the set is moved behind it: a restore that inserts the list front to back always finds the parent already there. For a
    // list that is parent-first already (the usual case) the order is exactly the Windows one.
    inline std::vector<Takeoff> collect_takeoffs_for_deletion(const std::vector<Takeoff> &takeoffs,
                                                              const std::vector<std::string> &selected_uids)
    {
        std::vector<const Takeoff *> ordered;
        std::set<std::string> selected_found;
        for (const std::string &uid : selected_uids)
        {
            if (selected_found.count(uid))
                continue;
            for (const Takeoff &takeoff : takeoffs)
            {
                if (takeoff.uid == uid)
                {
                    selected_found.insert(uid);
                    ordered.push_back(&takeoff);
                    break;
                }
            }
        }

        const std::set<std::string> doomed = expand_takeoff_uids_with_descendants(
            takeoffs, std::vector<std::string>(selected_found.begin(), selected_found.end()));
        for (const Takeoff &takeoff : takeoffs)
        {
            if (doomed.count(takeoff.uid) && !selected_found.count(takeoff.uid))
                ordered.push_back(&takeoff);
        }

        // Parents before children: place every takeoff whose parent is not still waiting, repeat until none is left. A cycle
        // (nothing can be placed any more) keeps the remaining takeoffs in their order.
        std::vector<Takeoff> result;
        result.reserve(ordered.size());
        std::set<std::string> placed;
        std::set<std::string> in_set;
        for (const Takeoff *takeoff : ordered)
            in_set.insert(takeoff->uid);
        while (!ordered.empty())
        {
            std::vector<const Takeoff *> waiting;
            for (const Takeoff *takeoff : ordered)
            {
                const bool parent_pending = takeoff->parent_uid != takeoff->uid && in_set.count(takeoff->parent_uid) &&
                                            !placed.count(takeoff->parent_uid);
                if (parent_pending)
                {
                    waiting.push_back(takeoff);
                }
                else
                {
                    result.push_back(*takeoff);
                    placed.insert(takeoff->uid);
                }
            }
            if (waiting.size() == ordered.size())
            {
                for (const Takeoff *takeoff : waiting)
                    result.push_back(*takeoff);
                break;
            }
            ordered.swap(waiting);
        }
        return result;
    }

}
