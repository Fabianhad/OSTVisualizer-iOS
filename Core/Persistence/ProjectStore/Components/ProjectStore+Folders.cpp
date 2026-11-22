// ProjectStore component — folder/hierarchy CRUD (BidProjects table).
#include "../ProjectStore_Internal.hpp"

namespace ost_store
{
    // project_operations.py delete_projects: the writer's refusal of the system project.
    static const char *SYSTEM_FOLDER_DELETE_MESSAGE = "Cannot delete the system project 'Deleted Bids' (UID 1)";

    // The system folder "Deleted Bids" is the BidProjects row with UID 1, whatever its name; "1" and "01" are the same uid
    // (_normalize_int_uids turns the uid into a number first).
    static bool is_system_folder(long long folder)
    {
        return ost_domain::is_deleted_bids_folder_uid(std::to_string(folder));
    }

    enum class FolderDeleteCheck
    {
        Deletable,
        Refused,
        Failed
    };

    // The rule of project_write_handler._delete_projects + project_operations.delete_projects for one folder: the system folder is never
    // deleted and a folder that has bids is not deletable (project_has_bids counts the bids whose BidProjectUID is the folder, for a folder
    // that exists; a bid that only has OrigBidProjectUID = the folder does not count). `message` is the reason when the answer is not
    // Deletable (the Windows text for a refusal, the failed read otherwise).
    static FolderDeleteCheck check_folder_deletable(ost_db::OstDatabase &db, long long folder, std::string &message)
    {
        if (is_system_folder(folder))
        {
            message = SYSTEM_FOLDER_DELETE_MESSAGE;
            return FolderDeleteCheck::Refused;
        }
        ost_db::Statement stmt = db.prepare(
            "SELECT COUNT(*) FROM Bids WHERE BidProjectUID = ? "
            "AND EXISTS (SELECT 1 FROM BidProjects WHERE UID = ?)");
        if (!stmt.valid())
        {
            message = "Cannot read Bids: " + stmt.error_message();
            return FolderDeleteCheck::Failed;
        }
        stmt.bind_int64(1, folder);
        stmt.bind_int64(2, folder);
        if (stmt.step_result() != ost_db::StepResult::Row)
        {
            message = "Cannot read Bids: " + stmt.error_message();
            return FolderDeleteCheck::Failed;
        }
        if (stmt.column_int64(0) > 0)
        {
            message = ost_domain::CANNOT_DELETE_FOLDER_MESSAGE;
            return FolderDeleteCheck::Refused;
        }
        return FolderDeleteCheck::Deletable;
    }

    bool load_hierarchy(const std::string &db_path,
                        ost_domain::DatabaseHierarchy &hierarchy,
                        std::string *error)
    {
        hierarchy = ost_domain::DatabaseHierarchy();
        if (error)
            error->clear();
        // BidProjects and Bids are required (hierarchy_reader.py: schema.require_column raises, a failing read propagates): a read
        // that fails, at the start or half way, is a failure and never an empty or partial hierarchy.
        auto fail = [&hierarchy, error](const char *what, const std::string &message) -> bool
        {
            hierarchy = ost_domain::DatabaseHierarchy();
            if (error)
                *error = std::string("Cannot read ") + what + ": " + message;
            return false;
        };

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return fail("the database", db.last_error_message());

        ost_domain::DatabaseHierarchy result;
        std::unordered_map<std::string, size_t> folder_index;
        {
            // Windows reads ORDER BY [Name], [UID]; the folder order of the browser is the decided one (BRW-M01): case-insensitive ASCII
            // compare, then lowercase first, then UID (ost_domain::folder_precedes).
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name "
                "FROM BidProjects ORDER BY Name, UID");
            if (!stmt.valid())
                return fail("BidProjects", stmt.error_message());
            while (stmt.step())
            {
                ost_domain::FolderGroup group;
                group.folder.uid = stmt.column_text(0);
                group.folder.name = stmt.column_text(1);
                result.folders.push_back(std::move(group));
            }
            if (stmt.failed())
                return fail("BidProjects", stmt.error_message());
            std::stable_sort(result.folders.begin(), result.folders.end(),
                             [](const ost_domain::FolderGroup &a, const ost_domain::FolderGroup &b)
                             { return ost_domain::folder_precedes(a.folder, b.folder); });
            for (size_t i = 0; i < result.folders.size(); ++i)
                folder_index[result.folders[i].folder.uid] = i;
        }

        {
            // hierarchy_reader.py: ORDER BY [BidProjectUID], [BidNo], [UID] (NULLs first). The list is then put in BidNo order with a stable
            // sort below, like the tree of the Windows app does (BRW-13).
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, JobName, BidProjectUID, BidNo "
                "FROM Bids ORDER BY BidProjectUID, BidNo, UID");
            if (!stmt.valid())
                return fail("Bids", stmt.error_message());
            while (stmt.step())
            {
                ost_domain::BidSummary bid;
                bid.bid_uid = stmt.column_text(0);
                bid.name = stmt.column_text(1);
                bid.folder_uid = stmt.column_text(2);
                bid.has_bid_no = stmt.column_type(3) != ost_db::Statement::TypeNull;
                if (bid.has_bid_no)
                    bid.bid_no = stmt.column_int64(3);

                if (!bid.folder_uid.empty())
                {
                    auto it = folder_index.find(bid.folder_uid);
                    if (it != folder_index.end())
                    {
                        result.folders[it->second].bids.push_back(std::move(bid));
                        continue;
                    }
                }
                bid.folder_uid.clear();
                result.orphan_bids.push_back(std::move(bid));
            }
            if (stmt.failed())
                return fail("Bids", stmt.error_message());
        }

        // The display order: BidNo ascending (a NULL BidNo first), equal keys keep the reader order above (BidProjectUID, UID).
        for (ost_domain::FolderGroup &group : result.folders)
            std::stable_sort(group.bids.begin(), group.bids.end(), ost_domain::bid_precedes);
        std::stable_sort(result.orphan_bids.begin(), result.orphan_bids.end(), ost_domain::bid_precedes);

        hierarchy = std::move(result);
        return true;
    }

    ost_domain::DatabaseHierarchy load_hierarchy(const std::string &db_path)
    {
        ost_domain::DatabaseHierarchy hierarchy;
        if (!load_hierarchy(db_path, hierarchy, nullptr))
            return ost_domain::DatabaseHierarchy();
        return hierarchy;
    }

    std::string create_folder(const std::string &db_path,
                              const std::string &name)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return "";

        // Windows create_project takes any name, also 'Deleted Bids' (duplicate names are allowed; the system folder is the row with
        // UID 1, never "the folder with that name"). The UID is MAX(UID) + 1, raised above every column that references BidProjects
        // (Bids.BidProjectUID, Bids.OrigBidProjectUID), so the new folder never claims the bids of a dangling reference. The
        // allocation and the insert are one transaction.
        if (!db.begin_transaction())
            return "";
        const long long new_uid = integrity::next_uid_preserving_references(db, "BidProjects");
        if (new_uid <= 0)
        {
            db.rollback();
            return "";
        }

        ost_db::Statement stmt = db.prepare(
            "INSERT INTO BidProjects (UID, Name) VALUES (?, ?)");
        if (!stmt.valid())
        {
            db.rollback();
            return "";
        }

        stmt.bind_int64(1, new_uid);
        stmt.bind_text(2, name);
        if (!stmt.run())
        {
            db.rollback();
            return "";
        }
        if (!db.commit())
            return "";
        return std::to_string(new_uid);
    }

    bool rename_folder(const std::string &db_path,
                       const std::string &folder_uid,
                       const std::string &new_name)
    {
        // Windows rename_project: the uid must be a valid project uid (require_unique_bid_owned_uid_matches), the row need not exist
        // (an absent project is a silent success) and ANY name is accepted, also 'Deleted Bids'. The system folder (UID 1) is never renamed:
        // the Windows tree disables Rename for it (_can_rename_context) and the store enforces that for every screen.
        long long folder = 0;
        if (!integrity::parse_uid(folder_uid, folder) || is_system_folder(folder))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidProjects SET Name = ? WHERE UID = ?");
        if (!stmt.valid())
            return false;

        stmt.bind_text(1, new_name);
        stmt.bind_int64(2, folder);
        return stmt.run();
    }

    bool can_delete_folder(const std::string &db_path,
                           const std::string &folder_uid,
                           std::string *error)
    {
        if (error)
            error->clear();
        long long folder = 0;
        if (!integrity::parse_uid(folder_uid, folder))
        {
            if (error)
                *error = "Invalid BidProjects UID: '" + folder_uid + "'";
            return false;
        }
        ost_db::OstDatabase db;
        if (!db.open(db_path))
        {
            if (error)
                *error = "Cannot open the database: " + db.last_error_message();
            return false;
        }
        std::string message;
        if (check_folder_deletable(db, folder, message) != FolderDeleteCheck::Deletable)
        {
            if (error)
                *error = message;
            return false;
        }
        return true;
    }

    bool delete_folder(const std::string &db_path,
                       const std::string &folder_uid,
                       std::string *error)
    {
        if (error)
            error->clear();
        auto refuse = [error](const std::string &message) -> bool
        {
            if (error)
                *error = message;
            return false;
        };

        // _normalize_int_uids / require_valid_unique_bid_owned_uids: the uid must be a valid project uid; delete_projects refuses the system
        // project before it opens the database.
        long long folder = 0;
        if (!integrity::parse_uid(folder_uid, folder))
            return refuse("Invalid BidProjects UID: '" + folder_uid + "'");
        if (is_system_folder(folder))
            return refuse(SYSTEM_FOLDER_DELETE_MESSAGE);

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return refuse("Cannot open the database: " + db.last_error_message());

        if (!db.begin_transaction())
            return refuse("Cannot start the delete: " + db.last_error_message());

        // Only an empty folder is deleted (BRW-08, decided); the check runs inside the write transaction so no bid can arrive in between.
        std::string message;
        if (check_folder_deletable(db, folder, message) != FolderDeleteCheck::Deletable)
        {
            db.rollback();
            return refuse(message);
        }

        // project_operations.delete_projects: the references to the folder are cleared in BOTH columns (a trashed bid whose original folder
        // is gone has no restore target and a later folder with the same UID can not be taken for it), then the row goes (BRW-05).
        for (const char *sql : {"UPDATE Bids SET BidProjectUID = NULL WHERE BidProjectUID = ?",
                                "UPDATE Bids SET OrigBidProjectUID = NULL WHERE OrigBidProjectUID = ?",
                                "DELETE FROM BidProjects WHERE UID = ?"})
        {
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid())
            {
                db.rollback();
                return refuse("Cannot delete the folder: " + stmt.error_message());
            }
            stmt.bind_int64(1, folder);
            if (!stmt.run())
            {
                const std::string reason = stmt.error_message();
                db.rollback();
                return refuse("Cannot delete the folder: " + reason);
            }
        }

        if (!db.commit())
            return refuse("Cannot delete the folder: " + db.last_error_message());
        return true;
    }

    bool assign_bid_to_folder(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &folder_uid)
    {
        // Windows move_bids_to_project: the bid and the target project must exist (require_existing_unique_bid_owned_uid_matches
        // for both); otherwise nothing changes and the move fails.
        long long bid = 0;
        long long folder = 0;
        if (!integrity::parse_uid(bid_uid, bid) || !integrity::parse_uid(folder_uid, folder))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;
        if (!integrity::bid_exists(db, bid) || !integrity::row_exists(db, "BidProjects", folder))
        {
            db.rollback();
            return false;
        }

        ost_db::Statement stmt = db.prepare(
            "UPDATE Bids SET BidProjectUID = ? WHERE UID = ?");
        if (!stmt.valid())
        {
            db.rollback();
            return false;
        }

        stmt.bind_int64(1, folder);
        stmt.bind_int64(2, bid);
        if (!stmt.run())
        {
            db.rollback();
            return false;
        }
        return db.commit();
    }

    bool remove_bid_from_folder(const std::string &db_path,
                                const std::string &bid_uid)
    {
        // Windows orphan_bids: the uid must be a valid bid uid (_normalize_int_uids / require_unique_bid_owned_uid_matches refuse 'abc',
        // '0', '-1', '' and write nothing); a valid uid without a row is a success that changes nothing. Only BidProjectUID is cleared.
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        ost_db::Statement stmt = db.prepare(
            "UPDATE Bids SET BidProjectUID = NULL WHERE UID = ?");
        if (!stmt.valid())
            return false;

        stmt.bind_int64(1, bid);
        return stmt.run();
    }

}
