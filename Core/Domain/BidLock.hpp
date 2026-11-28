// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
// The rule behind the bid lock (plan 4.1). Windows ost_visualizer/domain/entities/cover_sheet.py:15-23 resolve_bid_status_lock:
//     if bid_status_uid is None or bid_status_uid == "": return False          # a bid without a status is unlocked
//     for status in job_statuses:
//         if str(status.uid) == str(bid_status_uid): return status.locked      # the FIRST status with this uid text wins
//     return None                                                              # unresolved: the callers fail closed (locked)
// and application/services/project_read_service.py:81-94 is_bid_locked, which turns None into True (and logs).
#pragma once
#include <string>
#include <vector>

namespace ost_domain
{

    // One JobStatuses row as the rule needs it: `uid` is the stored UID as text (compared VERBATIM: no trimming, no case folding, "01"
    // is not "1"), `locked` is the Windows flag (settings_reader.py:71: Locked is set unless its stored text is "0", "False" or "";
    // a NULL reads as the text "NULL", so it is set).
    struct JobStatusLock
    {
        std::string uid;
        bool locked = false;
    };

    enum class BidStatusLock
    {
        Unlocked,  // Windows False
        Locked,    // Windows True
        Unresolved // Windows None: the bid has a status that no row of `statuses` names
    };

    // `status_uid` empty means the bid has no status (Windows None or ""): Unlocked whatever `statuses` holds.
    BidStatusLock resolve_bid_status_lock(const std::vector<JobStatusLock> &statuses, const std::string &status_uid);

    // is_bid_locked's last step: an unresolved status counts as locked (fail closed).
    inline bool bid_status_is_locked(BidStatusLock lock) { return lock != BidStatusLock::Unlocked; }

}
