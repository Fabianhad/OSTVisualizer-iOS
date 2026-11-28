// Layer: Persistence
// Depends on: Domain (BidLock), OstDatabase
// Forbidden: UIKit, Foundation, Renderer
// The bid lock guard for the UI (plan 4.1). A bid is locked when its job status is Locked; no job status = unlocked; a status that cannot
// be resolved = locked (fail closed). Windows: ost_visualizer/domain/entities/cover_sheet.py:15-23 (resolve_bid_status_lock),
// application/services/project_read_service.py:81-94 (is_bid_locked), infrastructure/mdb/components/settings_reader.py:63-77 and :361-370
// (the Locked flag and the JobStatuses read).
// Every NEW writer (M1 to M4) checks the lock INSIDE its own transaction with ost_store::integrity::bid_is_locked
// (ProjectStore_Integrity.hpp, same connection: a second connection would wait for the writer's BEGIN IMMEDIATE). This db_path function
// opens a connection of its own and is only for the UI, which shows "This bid is locked." before an action.
#pragma once
#include <string>

namespace ost_store
{

    // Reads Bids.JobStatusUID -> JobStatuses.Locked of the bid by the Windows rule (see ost_domain::resolve_bid_status_lock and
    // ost_store::integrity::bid_is_locked, which has the details) and sets `locked` ALWAYS (never left as it was):
    //   true,  locked == false  the bid has no status, or its status resolves to a status that is not Locked;
    //   true,  locked == true   the bid's status is Locked;
    //   locked == true          every case Windows treats as locked: a status uid that no JobStatuses row has, a JobStatuses read that
    //                           fails or has duplicate UIDs (Windows: an empty list = unresolved), a bid that does not exist;
    //   false, locked == true   the read failed: a database that cannot be opened, a missing table, a bid uid that is not a positive
    //                           integer (integrity::parse_uid), any failed statement (fail closed: the caller must treat it as locked).
    // Whether an unresolved status or a missing bid also returns false is not part of the contract: the caller refuses on `locked`.
    // Writes nothing.
    bool bid_is_locked(const std::string &db_path,
                       const std::string &bid_uid,
                       bool &locked);

}
