// Layer: Persistence
// Depends on: Domain (ConditionFolderRules), OstDatabase
// Forbidden: UIKit, Foundation, Renderer
// The store functions of the condition folders (plan 5.2, owned by S1): create, rename, delete and "Move to Folder" of ONE condition.
// (The empty stub of S0, included once at the end of ProjectStore.hpp, is filled here.)
// Windows (HEAD 49a2d59): infrastructure/mdb/components/condition_folder_operations.py :11-60 insert, :62-85 rename, :87-142 delete;
// application/services/project_write_service.py :779-822, :824-853, :866-928 (the lock guard first), :936-953 (THE in-use rule);
// presentation/handlers/condition_action_handler.py :1577-1617 (move a condition: update_condition with folder_uid, lock-blocked).
//
// The write pattern of plan 4.4 for every function: one transaction (BEGIN IMMEDIATE) on one connection; INSIDE it, in this order:
//   1. (before the database is opened) the uid texts: integrity::parse_uid; a bad one is InvalidUid and the database is not touched;
//   2. the schema the function writes (the table and its UID / BidUID / Name ... columns): a missing one is SchemaUnsupported (a missing
//      ParentUID column is only a refusal when a parent is given; ParentUID and ExpandState are skipped when the file lacks them, like
//      Windows mdb_writer.py:227-245);
//   3. the bid exists (BidNotFound), the rows belong to the OPEN bid (FolderNotFound / FolderOfAnotherBid, ParentNotFound /
//      ParentOfAnotherBid, ConditionNotFound / ConditionOfAnotherBid: Windows binds rename / delete to no bid, register CF-4);
//   4. the bid lock (integrity::bid_is_locked on the same connection; fail closed: a failed read is BidLocked too);
//   5. the name rules of ConditionFolderRules.hpp from the DATABASE rows of the bid (never from a screen's memory, register CF-2);
//   6. the UID limit (integrity::next_uid_within_access_limit, plan 4.2) for create;
//   7. the writes, then commit.
// ANY failed statement (and a failed open, BEGIN or COMMIT) = rollback and a refusal (WriteFailed): a refused call leaves the database
// byte-identical and the write lock released; nothing throws.
#pragma once
#include <string>
#include <vector>

namespace ost_store
{
    enum class ConditionFolderRefusal
    {
        None,                  // no refusal (set together with ok == true)
        BidLocked,             // the bid's job status is Locked, unresolved or unreadable (plan 4.1); message ""
        BidNotFound,           // Windows MissingBidOwnedUidError "Bids has no row for UID <n>"
        ParentNotFound,        // create: the parent uid is no row of BidConditionFolders
        ParentOfAnotherBid,    // create: the parent folder belongs to another bid
        HierarchyNotSupported, // create with a parent in a file without BidConditionFolders.ParentUID; message = the Windows text
        FolderNotFound,        // rename / delete / move: a folder uid is no row
        FolderOfAnotherBid,    // rename / delete / move: a folder belongs to another bid (a batch is refused as a whole)
        ConditionNotFound,     // move: the condition uid is no row of BidConditions
        ConditionOfAnotherBid, // move: the condition belongs to another bid
        InvalidUid,            // a uid text is not a positive decimal integer ("", "0", "-1", " 7", "abc", beyond 63 bits); the database is not opened
        BlankName,             // nothing left after the strip
        InvalidName,           // not valid UTF-8, or a NUL byte
        NameTooLong,           // more than 50 UTF-16 units; message = the Domain text
        DuplicateName,         // a sibling folder has the name (strip + lower-case); message = "Folder <name> already exists." (Windows)
        InUse,                 // delete: a requested folder, or one below it, holds a condition; `blocked` lists them in request order
        UidExhausted,          // create: no UID is left below 2147483647; message = integrity::no_identity_left_text("folder")
        SchemaUnsupported,     // a table or column the function needs is missing (Windows "Missing BidConditionFolders.<column>")
        WriteFailed            // anything else: a failed open / statement / commit (including a busy database); rolled back
    };

    struct ConditionFolderResult
    {
        bool ok = false;
        std::string uid;                  // create: the new folder's uid (decimal text); rename and move: the folder's uid
        ConditionFolderRefusal refusal = ConditionFolderRefusal::None;
        std::string message;              // the text the user must see where there is one (see the refusals above), else ""
        std::vector<std::string> blocked; // delete, InUse: the requested folders that are in use, request order
    };

    // Windows insert_condition_folder (a new folder is stored with ExpandState -1, Description NULL, BidUID and ParentUID bound as integers,
    // Name = the name stripped) after the iOS name rules (plan N-M1-15: the sibling rule also at create). `parent_uid` "" = a root folder.
    // The UID is integrity::next_uid_within_access_limit(db, "BidConditionFolders", 1, ...) (a dangling ParentUID or
    // BidConditions.BidConditionFolderUID reserves its number: test_condition_folder_operations.py:280-309).
    ConditionFolderResult create_condition_folder(const std::string &db_path,
                                                  const std::string &bid_uid,
                                                  const std::string &name,
                                                  const std::string &parent_uid);

    // Windows rename_condition_folder: only the Name of that row changes. The name rules of decide_condition_folder_rename: a blank name is
    // BlankName, a name equal to the stored one is a SUCCESS that writes nothing (no UPDATE runs), a sibling duplicate is DuplicateName.
    ConditionFolderResult rename_condition_folder(const std::string &db_path,
                                                  const std::string &bid_uid,
                                                  const std::string &folder_uid,
                                                  const std::string &name);

    // Windows delete_condition_folders (+ the service's in-use rule, register CF-1): all or nothing. `folder_uids` empty = ok without opening
    // the database (and without reading the bid); a uid that is not a positive integer anywhere in the list = InvalidUid before the database is
    // opened; duplicates are dropped (a uid text is compared as a number: "007" is 7), order kept. A requested folder that holds a condition
    // (of ANY bid: a safe superset, plan CF-1) or has a descendant folder that does is InUse (nothing is deleted, `blocked` lists the blocked
    // ones in request order). Otherwise the folders are deleted and every folder whose ParentUID names a deleted folder (of any bid) gets
    // ParentUID NULL: the sub-folders move to the TOP level (CF-3), conditions are never touched. No statement binds an unbounded list of uids.
    ConditionFolderResult delete_condition_folders(const std::string &db_path,
                                                   const std::string &bid_uid,
                                                   const std::vector<std::string> &folder_uids);

    // "Move to Folder": BidConditions.BidConditionFolderUID of ONE condition of the bid = `folder_uid` ("" = no folder, the root), nothing else
    // of the row or of any other row changes. The condition must belong to the bid and so must the folder (the condition is checked first:
    // a condition of another bid moved into a folder of another bid is ConditionOfAnotherBid). Unlike update_condition (which has no lock
    // check) it re-checks the bid lock (plan 5.2, N-M1-14); "Move to Folder" never calls update_condition. Moving a condition to the folder
    // it is in is a success. `uid` of the result is the folder's uid ("" for the root).
    ConditionFolderResult move_condition_to_folder(const std::string &db_path,
                                                   const std::string &bid_uid,
                                                   const std::string &condition_uid,
                                                   const std::string &folder_uid);
}
