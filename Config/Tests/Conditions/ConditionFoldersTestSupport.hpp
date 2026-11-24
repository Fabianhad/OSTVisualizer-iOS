// ConditionFoldersTestSupport.hpp - helpers of the condition-folder tests (test_condition_folder_*.cpp), on top of ConditionsTestSupport.hpp
// (its TempDb is ost_ctest::TempDb; ConditionsTestSupport.hpp itself is not edited here: the New Condition tests add their own helpers there).
// Everything lives in ost_ftest.
#pragma once

#include "ConditionsTestSupport.hpp"

#include "Domain/ConditionFolderRules.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace ost_ftest
{
    using ost_ctest::TempDb;
    using ost_store::ConditionFolderRefusal;
    using ost_store::ConditionFolderResult;

    inline const char *refusal_name(ConditionFolderRefusal refusal)
    {
        switch (refusal)
        {
        case ConditionFolderRefusal::None: return "None";
        case ConditionFolderRefusal::BidLocked: return "BidLocked";
        case ConditionFolderRefusal::BidNotFound: return "BidNotFound";
        case ConditionFolderRefusal::ParentNotFound: return "ParentNotFound";
        case ConditionFolderRefusal::ParentOfAnotherBid: return "ParentOfAnotherBid";
        case ConditionFolderRefusal::HierarchyNotSupported: return "HierarchyNotSupported";
        case ConditionFolderRefusal::FolderNotFound: return "FolderNotFound";
        case ConditionFolderRefusal::FolderOfAnotherBid: return "FolderOfAnotherBid";
        case ConditionFolderRefusal::ConditionNotFound: return "ConditionNotFound";
        case ConditionFolderRefusal::ConditionOfAnotherBid: return "ConditionOfAnotherBid";
        case ConditionFolderRefusal::InvalidUid: return "InvalidUid";
        case ConditionFolderRefusal::BlankName: return "BlankName";
        case ConditionFolderRefusal::InvalidName: return "InvalidName";
        case ConditionFolderRefusal::NameTooLong: return "NameTooLong";
        case ConditionFolderRefusal::DuplicateName: return "DuplicateName";
        case ConditionFolderRefusal::InUse: return "InUse";
        case ConditionFolderRefusal::UidExhausted: return "UidExhausted";
        case ConditionFolderRefusal::SchemaUnsupported: return "SchemaUnsupported";
        case ConditionFolderRefusal::WriteFailed: return "WriteFailed";
        }
        return "?";
    }

    inline std::string join(const std::vector<std::string> &items, const char *separator = ",")
    {
        std::string out;
        for (const std::string &item : items)
            out += (out.empty() ? "" : separator) + item;
        return out;
    }

    // A result that is a success: ok, no refusal.
    inline std::string describe(const ConditionFolderResult &result)
    {
        return std::string("ok=") + (result.ok ? "true" : "false") + " refusal=" + refusal_name(result.refusal) + " uid='" + result.uid +
               "' message='" + result.message + "' blocked=[" + join(result.blocked) + "]";
    }

    inline bool file_exists(const std::string &path)
    {
        if (std::FILE *file = std::fopen(path.c_str(), "rb"))
        {
            std::fclose(file);
            return true;
        }
        return false;
    }

    // The same text, one line per folder row in UID order: "uid|bid|parent|name|expand" (NULL as NULL, exact types through quote()).
    inline std::string folder_rows(const TempDb &db)
    {
        return db.list("SELECT UID || '|' || quote(BidUID) || '|' || quote(ParentUID) || '|' || quote(Name) || '|' || quote(ExpandState) "
                       "FROM BidConditionFolders ORDER BY UID");
    }

    // ---------------------------------------------------------------------------------------------------------------------------------
    // The iOS-schema world (a TempDb made by create_schema)
    // ---------------------------------------------------------------------------------------------------------------------------------

    // Bids 1 and 2 (both in project 1 = 'Deleted Bids' of create_schema: the project does not matter here), no job status.
    inline void seed_bids(const TempDb &db)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid one', 1)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Bid two', 2)");
    }

    // parent: "" = NULL, else the SQL literal of the ParentUID ("7", "'0'", ...).
    inline void add_folder(const TempDb &db, long long uid, long long bid, const std::string &name, const std::string &parent = "")
    {
        db.must("INSERT INTO BidConditionFolders (UID, BidUID, ParentUID, Name, ExpandState) VALUES (" + std::to_string(uid) + ", " +
                std::to_string(bid) + ", " + (parent.empty() ? std::string("NULL") : parent) + ", '" + name + "', -1)");
    }

    // folder: "" = no folder (NULL).
    inline void add_condition(const TempDb &db, long long uid, long long bid, const std::string &folder = "", int ref_no = 0)
    {
        db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type, BidConditionFolderUID) VALUES (" + std::to_string(uid) + ", " +
                std::to_string(bid) + ", 'g" + std::to_string(uid) + "', " + std::to_string(ref_no == 0 ? static_cast<int>(uid) : ref_no) + ", 'C" +
                std::to_string(uid) + "', 0, " + (folder.empty() ? std::string("NULL") : folder) + ")");
    }

    // The bid's job status: a JobStatuses row `status_uid` with Locked = `locked_sql` ("-1" locked, "0" open) and the bid pointing at it.
    // Plan 4.1 / Windows cover_sheet.py:15-23: the first row with that uid wins; a status that has no row is locked (fail closed).
    inline void set_job_status(const TempDb &db, long long bid, long long status_uid, const std::string &locked_sql)
    {
        db.must("INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (" + std::to_string(status_uid) + ", 'S" + std::to_string(status_uid) + "', " +
                locked_sql + ", " + std::to_string(status_uid) + ")");
        db.must("UPDATE Bids SET JobStatusUID = " + std::to_string(status_uid) + " WHERE UID = " + std::to_string(bid));
    }

    // The bid is LOCKED (a job status with Locked = -1, what the importer stores for Yes).
    inline void lock_bid(const TempDb &db, long long bid = 1, long long status_uid = 5) { set_job_status(db, bid, status_uid, "-1"); }

    // Many folders in one statement: UIDs first..last of `bid`, named `name`, no parent.
    inline void add_folder_range(const TempDb &db, long long first, long long last, long long bid, const std::string &name = "F")
    {
        db.must("WITH RECURSIVE n(i) AS (SELECT " + std::to_string(first) + " UNION ALL SELECT i + 1 FROM n WHERE i < " + std::to_string(last) +
                ") INSERT INTO BidConditionFolders (UID, BidUID, Name, ExpandState) SELECT i, " + std::to_string(bid) + ", '" + name + "', -1 FROM n");
    }

    // The request "first".."last" as uid texts.
    inline std::vector<std::string> uid_range(long long first, long long last)
    {
        std::vector<std::string> out;
        for (long long uid = first; uid <= last; ++uid)
            out.push_back(std::to_string(uid));
        return out;
    }

    // ---------------------------------------------------------------------------------------------------------------------------------
    // The minimal Windows schemas of the ported persistence tests (tests/infrastructure/mdb/components/test_condition_folder_operations.py
    // _folder_db and the sqlite fixtures of the single tests): a TempDb made WITHOUT the iOS schema.
    // ADAPTATION: the Windows tests create `Bids (UID INTEGER)`. The iOS lock check reads Bids.JobStatusUID (a Bids table without that
    // column is an unreadable lock = locked, fail closed), so every minimal Bids table here is `Bids (UID INTEGER, JobStatusUID)` (NULL =
    // no status = unlocked). Nothing else of the Windows fixtures is changed.
    // ---------------------------------------------------------------------------------------------------------------------------------

    inline const char *kWindowsFolderColumns = "UID INTEGER, BidUID INTEGER, ParentUID INTEGER, Name TEXT, ExpandState INTEGER";

    // _folder_db(folders, *extra): Bids 1 (plus `more_bids`), the folder table with `folder_columns`, and
    // BidConditions (UID INTEGER, BidConditionFolderUID INTEGER) unless `conditions_table` is empty.
    inline void windows_folder_db(const TempDb &db, const std::string &folder_columns = kWindowsFolderColumns, const std::vector<int> &more_bids = {},
                                  const std::string &conditions_table = "CREATE TABLE BidConditions (UID INTEGER, BidConditionFolderUID INTEGER)")
    {
        db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
        db.must("INSERT INTO Bids (UID) VALUES (1)");
        for (const int bid : more_bids)
            db.must("INSERT INTO Bids (UID) VALUES (" + std::to_string(bid) + ")");
        db.must("CREATE TABLE BidConditionFolders (" + folder_columns + ")");
        if (!conditions_table.empty())
            db.must(conditions_table);
    }
}

// A refusal: not ok, and the refusal is the named one. The message shows the whole result on a failure.
#define CHECK_FOLDER_REFUSED(result_expression, expected_refusal)                                                                      \
    do                                                                                                                                 \
    {                                                                                                                                  \
        const ost_store::ConditionFolderResult &folder_result_ = (result_expression);                                                  \
        CHECK_FALSE(folder_result_.ok);                                                                                                \
        CHECK_STR_EQ(std::string(ost_ftest::refusal_name(folder_result_.refusal)),                                                     \
                     std::string(ost_ftest::refusal_name(expected_refusal)));                                                          \
    } while (0)

// A success: ok and no refusal.
#define CHECK_FOLDER_OK(result_expression)                                                                                             \
    do                                                                                                                                 \
    {                                                                                                                                  \
        const ost_store::ConditionFolderResult &folder_result_ = (result_expression);                                                  \
        CHECK(folder_result_.ok);                                                                                                      \
        CHECK_STR_EQ(std::string(ost_ftest::refusal_name(folder_result_.refusal)), std::string("None"));                               \
    } while (0)
