// Layer: Domain. resolve_bid_status_lock (see BidLock.hpp).
// Windows source of truth: ost_visualizer/domain/entities/cover_sheet.py:15-23.
#include "BidLock.hpp"

namespace ost_domain
{

    BidStatusLock resolve_bid_status_lock(const std::vector<JobStatusLock> &statuses, const std::string &status_uid)
    {
        // cover_sheet.py:17-18: a bid without a status is unlocked, whatever the statuses say.
        if (status_uid.empty())
            return BidStatusLock::Unlocked;
        // cover_sheet.py:19-22: the FIRST status whose uid text equals the bid's status text decides (str(a) == str(b): verbatim).
        for (const JobStatusLock &status : statuses)
            if (status.uid == status_uid)
                return status.locked ? BidStatusLock::Locked : BidStatusLock::Unlocked;
        // cover_sheet.py:23: no status has this uid (Windows None); the callers fail closed.
        return BidStatusLock::Unresolved;
    }

}
