// Layer: Domain (pure C++). The refusal of an undo / redo: see UndoRedoRules.hpp.
#include "UndoRedoRules.hpp"

#include "../Data/ProjectData.hpp"

#include <unordered_map>

namespace ost_domain
{
    namespace
    {
        // _replay_page_scale: the page must be in the project (get_page("") is None)
        bool page_exists(const ost_data::ProjectData &project, const std::string &page_uid)
        {
            return !page_uid.empty() && project.pages.find(page_uid) != project.pages.end();
        }

        // _replay_takeoff_scale: the takeoff must be in the project and so must its page. `page_of_takeoff` maps each takeoff uid to its page uid
        // (the first takeoff with a uid wins, like the linear search it replaces), built once per call so a group of k takeoffs costs O(k + N).
        bool takeoff_and_page_exist(const ost_data::ProjectData &project, const std::unordered_map<std::string, const std::string *> &page_of_takeoff,
                                    const std::string &takeoff_uid)
        {
            if (takeoff_uid.empty())
                return false;
            const auto found = page_of_takeoff.find(takeoff_uid);
            return found != page_of_takeoff.end() && page_exists(project, *found->second);
        }
    }

    HistoryReplayTargets history_replay_targets(HistoryEntryKind kind, HistoryDirection direction, const std::vector<std::string> &takeoff_uids,
                                                const std::vector<std::string> &stored_page_uids)
    {
        HistoryReplayTargets targets;
        switch (kind)
        {
        case HistoryEntryKind::Move:
        case HistoryEntryKind::MoveGroup:
        case HistoryEntryKind::SetNegative:
            targets.takeoff_uids = takeoff_uids;
            break;
        case HistoryEntryKind::Delete:
            if (direction == HistoryDirection::Undo)
                targets.page_uids = stored_page_uids;
            break;
        case HistoryEntryKind::Place:
            if (direction == HistoryDirection::Redo)
                targets.page_uids = stored_page_uids;
            break;
        }
        return targets;
    }

    bool history_targets_unavailable(const ost_data::ProjectData &project, const HistoryReplayTargets &targets)
    {
        if (!targets.takeoff_uids.empty())
        {
            std::unordered_map<std::string, const std::string *> page_of_takeoff;
            page_of_takeoff.reserve(project.takeoffs.size());
            for (const Takeoff &takeoff : project.takeoffs)
                page_of_takeoff.emplace(takeoff.uid, &takeoff.page_uid); // emplace keeps the first takeoff of a uid
            for (const std::string &uid : targets.takeoff_uids)
            {
                if (!takeoff_and_page_exist(project, page_of_takeoff, uid))
                    return true;
            }
        }
        for (const std::string &uid : targets.page_uids)
        {
            if (!page_exists(project, uid))
                return true;
        }
        return false;
    }

    std::optional<DialogText> history_refusal_dialog(HistoryDirection direction, HistoryRefusal refusal, bool user_initiated)
    {
        // an automatic undo / redo only logs (undo_redo_service.py:559)
        if (!user_initiated)
            return std::nullopt;
        const bool undoing = direction == HistoryDirection::Undo;
        const bool stale = refusal == HistoryRefusal::TargetUnavailable;
        // texts.py:1752-1767
        if (undoing)
            return DialogText{"Undo", stale ? "The change could not be undone because the page or item it belongs to no longer exists. It is still in the undo history."
                                            : "The change could not be undone. It is still in the undo history."};
        return DialogText{"Redo", stale ? "The change could not be redone because the page or item it belongs to no longer exists. It is still in the redo history."
                                        : "The change could not be redone. It is still in the redo history."};
    }
}
