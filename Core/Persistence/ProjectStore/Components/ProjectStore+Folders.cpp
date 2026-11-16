// ProjectStore component — folder/hierarchy CRUD (BidProjects table).
#include "../ProjectStore_Internal.hpp"

namespace ost_store
{
    static const char *DELETED_BIDS_PROJECT_NAME = "Deleted Bids";

    // `ok` is set to false when the lookup itself failed: the answer is then unknown, and a caller that is about to
    // change the folder must not treat "unknown" as "not the system folder".
    static bool folder_is_deleted_bids(ost_db::OstDatabase &db,
                                       const std::string &folder_uid,
                                       bool &ok)
    {
        ok = true;
        ost_db::Statement stmt = db.prepare(
            "SELECT 1 FROM BidProjects WHERE UID = ? AND Name = ?");
        if (!stmt.valid())
        {
            ok = false;
            return false;
        }
        stmt.bind_text(1, folder_uid);
        stmt.bind_text(2, DELETED_BIDS_PROJECT_NAME);
        const ost_db::StepResult result = stmt.step_result();
        if (result == ost_db::StepResult::Error)
        {
            ok = false;
            return false;
        }
        return result == ost_db::StepResult::Row;
    }

    ost_domain::DatabaseHierarchy load_hierarchy(const std::string &db_path)
    {
        ost_domain::DatabaseHierarchy result;
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return result;

        std::unordered_map<std::string, size_t> folder_index;
        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name "
                "FROM BidProjects ORDER BY Name");
            if (stmt.valid())
            {
                while (stmt.step())
                {
                    ost_domain::FolderGroup group;
                    group.folder.uid = stmt.column_text(0);
                    group.folder.name = stmt.column_text(1);
                    folder_index[group.folder.uid] = result.folders.size();
                    result.folders.push_back(std::move(group));
                }
            }
        }

        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, JobName, BidProjectUID "
                "FROM Bids ORDER BY UID DESC");
            if (stmt.valid())
            {
                while (stmt.step())
                {
                    ost_domain::BidSummary bid;
                    bid.bid_uid = stmt.column_text(0);
                    bid.name = stmt.column_text(1);
                    bid.folder_uid = stmt.column_text(2);

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
            }
        }

        return result;
    }

    std::string create_folder(const std::string &db_path,
                              const std::string &name)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return "";

        if (name == DELETED_BIDS_PROJECT_NAME)
        {
            ost_db::Statement existing = db.prepare(
                "SELECT UID FROM BidProjects WHERE Name = ? ORDER BY UID LIMIT 1");
            if (!existing.valid())
                return "";
            existing.bind_text(1, DELETED_BIDS_PROJECT_NAME);
            const ost_db::StepResult existing_result = existing.step_result();
            if (existing_result == ost_db::StepResult::Error)
                return "";
            if (existing_result == ost_db::StepResult::Row)
                return existing.column_text(0);
        }

        std::string folder_uid = "1";
        ost_db::Statement max_stmt = db.prepare(
            "SELECT MAX(UID) FROM BidProjects");
        if (!max_stmt.valid())
            return "";
        const ost_db::StepResult max_result = max_stmt.step_result();
        if (max_result == ost_db::StepResult::Error)
            return "";
        if (max_result == ost_db::StepResult::Row)
        {
            int max_uid = max_stmt.column_int(0);
            folder_uid = std::to_string(max_uid + 1);
        }

        ost_db::Statement stmt = db.prepare(
            "INSERT INTO BidProjects (UID, Name) VALUES (?, ?)");
        if (!stmt.valid())
            return "";

        stmt.bind_text(1, folder_uid);
        stmt.bind_text(2, name);
        if (!stmt.run())
            return "";
        return folder_uid;
    }

    bool rename_folder(const std::string &db_path,
                       const std::string &folder_uid,
                       const std::string &new_name)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (new_name == DELETED_BIDS_PROJECT_NAME)
        {
            bool lookup_ok = true;
            if (!folder_is_deleted_bids(db, folder_uid, lookup_ok) || !lookup_ok)
                return false;
        }

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidProjects SET Name = ? WHERE UID = ?");
        if (!stmt.valid())
            return false;

        stmt.bind_text(1, new_name);
        stmt.bind_text(2, folder_uid);
        return stmt.run();
    }

    bool delete_folder(const std::string &db_path,
                       const std::string &folder_uid)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        bool lookup_ok = true;
        if (folder_is_deleted_bids(db, folder_uid, lookup_ok) || !lookup_ok)
            return false;

        if (!db.begin_transaction())
            return false;

        {
            ost_db::Statement stmt = db.prepare(
                "UPDATE Bids SET BidProjectUID = NULL WHERE BidProjectUID = ?");
            if (!stmt.valid())
            {
                db.rollback();
                return false;
            }
            stmt.bind_text(1, folder_uid);
            if (!stmt.run())
            {
                db.rollback();
                return false;
            }
        }

        {
            ost_db::Statement stmt = db.prepare(
                "DELETE FROM BidProjects WHERE UID = ?");
            if (!stmt.valid())
            {
                db.rollback();
                return false;
            }
            stmt.bind_text(1, folder_uid);
            if (!stmt.run())
            {
                db.rollback();
                return false;
            }
        }

        return db.commit();
    }

    bool assign_bid_to_folder(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &folder_uid)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        ost_db::Statement stmt = db.prepare(
            "UPDATE Bids SET BidProjectUID = ? WHERE UID = ?");
        if (!stmt.valid())
            return false;

        stmt.bind_text(1, folder_uid);
        stmt.bind_text(2, bid_uid);
        return stmt.run();
    }

    bool remove_bid_from_folder(const std::string &db_path,
                                const std::string &bid_uid)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        ost_db::Statement stmt = db.prepare(
            "UPDATE Bids SET BidProjectUID = NULL WHERE UID = ?");
        if (!stmt.valid())
            return false;

        stmt.bind_text(1, bid_uid);
        return stmt.run();
    }

}
