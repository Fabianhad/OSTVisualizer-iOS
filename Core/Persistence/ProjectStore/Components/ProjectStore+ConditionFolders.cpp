// ProjectStore component: condition folders (create, rename, delete, Move to Folder). Contract: ProjectStore_ConditionFolders.hpp.
// Windows (HEAD 49a2d59): condition_folder_operations.py :11-60 insert, :62-85 rename, :87-142 delete; project_write_service.py :779-822,
// :824-853, :866-928, :936-953 (the in-use rule: Domain plan_condition_folder_delete); condition_action_handler.py :1577-1617 (move).
// Plan 4.4: one transaction per call; the lock, the ownership and the name rules run INSIDE it, from database rows.
#include "../ProjectStore_Internal.hpp"
#include "../../../Domain/ConditionFolderRules.hpp"

#include <algorithm>
#include <initializer_list>
#include <set>
#include <unordered_map>
#include <vector>

namespace ost_store
{
    namespace
    {
        using ost_db::OstDatabase;
        using ost_db::Statement;
        using Refusal = ConditionFolderRefusal;
        using Result = ConditionFolderResult;

        Result refused(Refusal refusal, const std::string &message = std::string())
        {
            Result result;
            result.refusal = refusal;
            result.message = message;
            return result;
        }

        Result succeeded(const std::string &uid = std::string())
        {
            Result result;
            result.ok = true;
            result.uid = uid;
            return result;
        }

        // Nothing throws across the Objective-C boundary (plan 4.4): an exception (bad_alloc, length_error) ends the call as a failed
        // write; the connection and its open transaction die with the stack frame (sqlite rolls back on close).
        template <typename Body>
        Result guarded(Body &&body)
        {
            try
            {
                return body();
            }
            catch (...)
            {
                return refused(Refusal::WriteFailed);
            }
        }

        // The call's own connection with the write lock held (BEGIN IMMEDIATE waits the busy timeout).
        bool open_and_begin(const std::string &db_path, OstDatabase &db)
        {
            return db.open(db_path) && db.begin_transaction();
        }

        // The table exists and has every named column (a missing one is SchemaUnsupported: the writers never create schema).
        bool has_columns(OstDatabase &db, const char *table, std::initializer_list<const char *> columns)
        {
            if (!db.table_exists(table))
                return false;
            for (const char *column : columns)
                if (!db.column_exists(table, column))
                    return false;
            return true;
        }

        // The bid lock rule (plan 4.1) on this connection, fail closed: a read that failed is a lock.
        bool bid_is_locked_here(OstDatabase &db, long long bid)
        {
            bool locked = true;
            const bool read = integrity::bid_is_locked(db, bid, locked);
            return !read || locked;
        }

        // The folders of the bid as the Domain rules see them (what the screen shows); every bid's when `only_bid` is negative.
        // `with_name` false reads no Name column (the delete needs only UID, BidUID and ParentUID, like Windows: a file without Name still
        // deletes) and leaves every name empty. `owner_of` (optional) receives the BidUID of every folder read (-1 for a NULL BidUID).
        bool read_folders(OstDatabase &db, long long only_bid, bool has_parent_column, bool with_name,
                          std::unordered_map<std::string, ost_domain::ConditionFolder> &folders,
                          std::unordered_map<std::string, long long> *owner_of = nullptr)
        {
            const std::string sql = std::string("SELECT [UID], ") + (with_name ? "[Name]" : "''") + ", " +
                                    (has_parent_column ? "[ParentUID]" : "NULL") + ", [BidUID] FROM [BidConditionFolders]";
            Statement stmt = db.prepare(sql);
            if (!stmt.valid())
                return false;
            while (stmt.step())
            {
                const long long owner = stmt.column_type(3) == Statement::TypeNull ? -1 : stmt.column_int64(3);
                if (only_bid >= 0 && owner != only_bid)
                    continue;
                ost_domain::ConditionFolder folder;
                folder.uid = stmt.column_text(0);
                folder.name = stmt.column_text_exact(1);
                folder.parent_uid = stmt.column_type(2) == Statement::TypeNull ? std::string() : stmt.column_text(2);
                if (owner_of)
                    (*owner_of)[folder.uid] = owner;
                const std::string key = folder.uid;
                folders[key] = std::move(folder);
            }
            return !stmt.failed();
        }

        Refusal refusal_of(ost_domain::FolderNameIssue issue)
        {
            switch (issue)
            {
            case ost_domain::FolderNameIssue::Blank:
                return Refusal::BlankName;
            case ost_domain::FolderNameIssue::InvalidText:
                return Refusal::InvalidName;
            case ost_domain::FolderNameIssue::TooLong:
                return Refusal::NameTooLong;
            case ost_domain::FolderNameIssue::DuplicateSibling:
                return Refusal::DuplicateName;
            case ost_domain::FolderNameIssue::None:
                break;
            }
            return Refusal::WriteFailed;
        }

        // The refusal of a failed name check; the message is the Domain text where the Domain has one (too long, duplicate).
        Result name_refusal(const ost_domain::FolderNameCheck &check)
        {
            const bool has_text = check.issue == ost_domain::FolderNameIssue::TooLong || check.issue == ost_domain::FolderNameIssue::DuplicateSibling;
            return refused(refusal_of(check.issue), has_text ? check.dialog.message : std::string());
        }

        // `sql_prefix` + "(?, ?, ...)" over the ids, bound as integers and run to completion, at most kChunk variables per statement
        // (sqlite's variable limit is 999 on old builds): no statement binds an unbounded list.
        constexpr std::size_t kChunk = 500;

        bool run_in_chunks(OstDatabase &db, const std::string &sql_prefix, const std::vector<long long> &ids)
        {
            for (std::size_t start = 0; start < ids.size(); start += kChunk)
            {
                const std::size_t end = std::min(ids.size(), start + kChunk);
                std::string sql = sql_prefix + "(";
                for (std::size_t i = start; i < end; ++i)
                    sql += i == start ? "?" : ", ?";
                sql += ")";
                Statement stmt = db.prepare(sql);
                if (!stmt.valid())
                    return false;
                for (std::size_t i = start; i < end; ++i)
                    stmt.bind_int64(static_cast<int>(i - start + 1), ids[i]);
                if (!stmt.run())
                    return false;
            }
            return true;
        }

        Result create_impl(const std::string &db_path, const std::string &bid_uid, const std::string &name, const std::string &parent_uid)
        {
            // 1. the uid texts, before the database is touched (a bad one burns no uid and opens nothing)
            long long bid = 0;
            long long parent = 0;
            if (!integrity::parse_uid(bid_uid, bid) || (!parent_uid.empty() && !integrity::parse_uid(parent_uid, parent)))
                return refused(Refusal::InvalidUid);

            OstDatabase db;
            if (!open_and_begin(db_path, db))
                return refused(Refusal::WriteFailed);
            auto fail = [&db](Result result)
            {
                db.rollback();
                return result;
            };

            // 2. the schema
            if (!has_columns(db, "BidConditionFolders", {"UID", "BidUID", "Name"}))
                return fail(refused(Refusal::SchemaUnsupported));
            const bool has_parent_column = db.column_exists("BidConditionFolders", "ParentUID");
            const bool has_expand_column = db.column_exists("BidConditionFolders", "ExpandState");

            // 3. ownership: the bid, then the parent folder (condition_folder_operations.py:22-36)
            if (!integrity::bid_exists(db, bid))
                return fail(refused(Refusal::BidNotFound));
            if (!parent_uid.empty())
            {
                if (!has_parent_column) // :26-30
                    return fail(refused(Refusal::HierarchyNotSupported, ost_domain::kConditionFolderHierarchyNotSupportedText));
                if (!integrity::row_exists(db, "BidConditionFolders", parent))
                    return fail(refused(Refusal::ParentNotFound));
                if (!integrity::row_belongs_to_bid(db, "BidConditionFolders", parent, bid))
                    return fail(refused(Refusal::ParentOfAnotherBid));
            }

            // 4. the bid lock (project_write_service.py:786-787)
            if (bid_is_locked_here(db, bid))
                return fail(refused(Refusal::BidLocked));

            // 5. the name rules over the folders of this bid, read now (register CF-2, CF-5, CF-6, CF-12)
            std::unordered_map<std::string, ost_domain::ConditionFolder> folders;
            if (!read_folders(db, bid, has_parent_column, true, folders))
                return fail(refused(Refusal::WriteFailed));
            const ost_domain::FolderNameCheck check =
                ost_domain::check_condition_folder_name(folders, parent_uid.empty() ? std::string() : std::to_string(parent), std::string(), name);
            if (check.issue != ost_domain::FolderNameIssue::None)
                return fail(name_refusal(check));

            // 6. the uid (a dangling ParentUID / BidConditionFolderUID reserves its number) within the Access limit (plan 4.2)
            long long uid = 0;
            bool no_identity_left = false;
            if (!integrity::next_uid_within_access_limit(db, "BidConditionFolders", 1, uid, &no_identity_left))
                return fail(no_identity_left ? refused(Refusal::UidExhausted, integrity::no_identity_left_text("folder"))
                                             : refused(Refusal::WriteFailed));

            // 7. the write: the columns of condition_folder_operations.py:41-54; ParentUID and ExpandState are skipped when the file lacks
            // them (mdb_writer.py:227-245); Description stays NULL
            std::string columns = "[UID], [BidUID]";
            std::string values = "?, ?";
            if (has_parent_column)
            {
                columns += ", [ParentUID]";
                values += ", ?";
            }
            columns += ", [Name]";
            values += ", ?";
            if (has_expand_column)
            {
                columns += ", [ExpandState]";
                values += ", -1";
            }
            Statement insert = db.prepare("INSERT INTO [BidConditionFolders] (" + columns + ") VALUES (" + values + ")");
            if (!insert.valid())
                return fail(refused(Refusal::WriteFailed));
            int index = 1;
            insert.bind_int64(index++, uid);
            insert.bind_int64(index++, bid);
            if (has_parent_column)
            {
                if (parent_uid.empty())
                    insert.bind_null(index++);
                else
                    insert.bind_int64(index++, parent);
            }
            insert.bind_text(index++, check.name);
            if (!insert.run())
                return fail(refused(Refusal::WriteFailed));
            if (!db.commit())
                return refused(Refusal::WriteFailed);
            return succeeded(std::to_string(uid));
        }

        Result rename_impl(const std::string &db_path, const std::string &bid_uid, const std::string &folder_uid, const std::string &name)
        {
            long long bid = 0;
            long long folder = 0;
            if (!integrity::parse_uid(bid_uid, bid) || !integrity::parse_uid(folder_uid, folder))
                return refused(Refusal::InvalidUid);

            OstDatabase db;
            if (!open_and_begin(db_path, db))
                return refused(Refusal::WriteFailed);
            auto fail = [&db](Result result)
            {
                db.rollback();
                return result;
            };

            if (!has_columns(db, "BidConditionFolders", {"UID", "BidUID", "Name"}))
                return fail(refused(Refusal::SchemaUnsupported));

            // ownership: Windows binds a rename to no bid (register CF-4); here the folder must belong to the open bid
            if (!integrity::bid_exists(db, bid))
                return fail(refused(Refusal::BidNotFound));
            if (!integrity::row_exists(db, "BidConditionFolders", folder))
                return fail(refused(Refusal::FolderNotFound));
            if (!integrity::row_belongs_to_bid(db, "BidConditionFolders", folder, bid))
                return fail(refused(Refusal::FolderOfAnotherBid));

            if (bid_is_locked_here(db, bid))
                return fail(refused(Refusal::BidLocked));

            std::unordered_map<std::string, ost_domain::ConditionFolder> folders;
            if (!read_folders(db, bid, db.column_exists("BidConditionFolders", "ParentUID"), true, folders))
                return fail(refused(Refusal::WriteFailed));
            std::string new_name;
            ost_domain::FolderNameCheck check;
            const ost_domain::FolderRenameDecision decision =
                ost_domain::decide_condition_folder_rename(folders, std::to_string(folder), name, new_name, &check);
            if (decision == ost_domain::FolderRenameDecision::Refused)
                return fail(check.issue == ost_domain::FolderNameIssue::None ? refused(Refusal::FolderNotFound) : name_refusal(check));

            // conditions_sidebar.py:1470-1471: the name the folder has = no write, and no UPDATE runs
            if (decision == ost_domain::FolderRenameDecision::Write)
            {
                Statement update = db.prepare("UPDATE [BidConditionFolders] SET [Name] = ? WHERE [UID] = ? AND [BidUID] = ?");
                if (!update.valid())
                    return fail(refused(Refusal::WriteFailed));
                update.bind_text(1, new_name);
                update.bind_int64(2, folder);
                update.bind_int64(3, bid);
                if (!update.run())
                    return fail(refused(Refusal::WriteFailed));
            }
            if (!db.commit())
                return refused(Refusal::WriteFailed);
            return succeeded(std::to_string(folder));
        }

        Result delete_impl(const std::string &db_path, const std::string &bid_uid, const std::vector<std::string> &folder_uids)
        {
            // condition_folder_operations.py:88-89: nothing to delete = success before any connection
            if (folder_uids.empty())
                return succeeded();

            long long bid = 0;
            if (!integrity::parse_uid(bid_uid, bid))
                return refused(Refusal::InvalidUid);
            // bulk_write_helpers.py:22-36: every uid is read as a number, a bad one anywhere refuses the call; duplicates drop, order stays
            std::vector<long long> ids;
            std::vector<std::string> texts;
            {
                std::set<long long> seen;
                for (const std::string &text : folder_uids)
                {
                    long long id = 0;
                    if (!integrity::parse_uid(text, id))
                        return refused(Refusal::InvalidUid);
                    if (seen.insert(id).second)
                    {
                        ids.push_back(id);
                        texts.push_back(std::to_string(id));
                    }
                }
            }

            OstDatabase db;
            if (!open_and_begin(db_path, db))
                return refused(Refusal::WriteFailed);
            auto fail = [&db](Result result)
            {
                db.rollback();
                return result;
            };

            if (!has_columns(db, "BidConditionFolders", {"UID", "BidUID"}))
                return fail(refused(Refusal::SchemaUnsupported));
            if (!integrity::bid_exists(db, bid))
                return fail(refused(Refusal::BidNotFound));

            // every folder of the file, with its owner: the ownership check below and the ancestor walk of the in-use rule read them
            const bool has_parent_column = db.column_exists("BidConditionFolders", "ParentUID");
            std::unordered_map<std::string, ost_domain::ConditionFolder> all_folders;
            std::unordered_map<std::string, long long> owner_of;
            if (!read_folders(db, -1, has_parent_column, false, all_folders, &owner_of))
                return fail(refused(Refusal::WriteFailed));
            for (const std::string &text : texts) // condition_folder_operations.py:101-105 (one bid); here the OPEN bid (CF-4)
            {
                const auto owner = owner_of.find(text);
                if (owner == owner_of.end())
                    return fail(refused(Refusal::FolderNotFound));
                if (owner->second != bid)
                    return fail(refused(Refusal::FolderOfAnotherBid));
            }

            if (bid_is_locked_here(db, bid))
                return fail(refused(Refusal::BidLocked));

            // THE in-use rule (project_write_service.py:936-953) from the database rows of this transaction (CF-1, CF-2). The conditions of
            // EVERY bid count (condition_folder_operations.py:106-123 filters by no bid: a safe superset); only their folder uids matter.
            std::unordered_map<std::string, ost_domain::Condition> referencing;
            if (has_columns(db, "BidConditions", {"BidConditionFolderUID"}))
            {
                Statement stmt = db.prepare("SELECT DISTINCT [BidConditionFolderUID] FROM [BidConditions] WHERE [BidConditionFolderUID] IS NOT NULL");
                if (!stmt.valid())
                    return fail(refused(Refusal::WriteFailed));
                while (stmt.step())
                {
                    ost_domain::Condition condition;
                    condition.uid = std::to_string(referencing.size());
                    condition.condition_folder_uid = stmt.column_text(0);
                    const std::string key = condition.uid;
                    referencing[key] = std::move(condition);
                }
                if (stmt.failed())
                    return fail(refused(Refusal::WriteFailed));
            }
            const ost_domain::ConditionFolderDeletePlan plan = ost_domain::plan_condition_folder_delete(all_folders, referencing, texts);
            if (!plan.blocked.empty())
            {
                Result result = refused(Refusal::InUse);
                result.blocked = plan.blocked;
                return fail(result);
            }

            // condition_folder_operations.py:124-134: the children of every deleted folder (of any bid) become root folders (CF-3), then
            // the folders go; conditions are never touched
            if (has_parent_column && !run_in_chunks(db, "UPDATE [BidConditionFolders] SET [ParentUID] = NULL WHERE [ParentUID] IN ", ids))
                return fail(refused(Refusal::WriteFailed));
            if (!run_in_chunks(db, "DELETE FROM [BidConditionFolders] WHERE [UID] IN ", ids))
                return fail(refused(Refusal::WriteFailed));
            if (!db.commit())
                return refused(Refusal::WriteFailed);
            return succeeded();
        }

        Result move_impl(const std::string &db_path, const std::string &bid_uid, const std::string &condition_uid, const std::string &folder_uid)
        {
            long long bid = 0;
            long long condition = 0;
            long long folder = 0;
            if (!integrity::parse_uid(bid_uid, bid) || !integrity::parse_uid(condition_uid, condition) ||
                (!folder_uid.empty() && !integrity::parse_uid(folder_uid, folder)))
                return refused(Refusal::InvalidUid);

            OstDatabase db;
            if (!open_and_begin(db_path, db))
                return refused(Refusal::WriteFailed);
            auto fail = [&db](Result result)
            {
                db.rollback();
                return result;
            };

            if (!has_columns(db, "BidConditions", {"UID", "BidUID", "BidConditionFolderUID"}))
                return fail(refused(Refusal::SchemaUnsupported));
            if (!integrity::bid_exists(db, bid))
                return fail(refused(Refusal::BidNotFound));
            if (!integrity::row_exists(db, "BidConditions", condition))
                return fail(refused(Refusal::ConditionNotFound));
            if (!integrity::row_belongs_to_bid(db, "BidConditions", condition, bid))
                return fail(refused(Refusal::ConditionOfAnotherBid));
            if (!folder_uid.empty())
            {
                if (!integrity::row_exists(db, "BidConditionFolders", folder))
                    return fail(refused(Refusal::FolderNotFound));
                if (!integrity::row_belongs_to_bid(db, "BidConditionFolders", folder, bid))
                    return fail(refused(Refusal::FolderOfAnotherBid));
            }

            // update_condition has no lock check; Windows guards the move (project_write_service.py:1303), so this writer does
            if (bid_is_locked_here(db, bid))
                return fail(refused(Refusal::BidLocked));

            Statement update = db.prepare("UPDATE [BidConditions] SET [BidConditionFolderUID] = ? WHERE [UID] = ? AND [BidUID] = ?");
            if (!update.valid())
                return fail(refused(Refusal::WriteFailed));
            if (folder_uid.empty())
                update.bind_null(1);
            else
                update.bind_int64(1, folder);
            update.bind_int64(2, condition);
            update.bind_int64(3, bid);
            if (!update.run())
                return fail(refused(Refusal::WriteFailed));
            if (!db.commit())
                return refused(Refusal::WriteFailed);
            return succeeded(folder_uid.empty() ? std::string() : std::to_string(folder));
        }
    }

    ConditionFolderResult create_condition_folder(const std::string &db_path,
                                                  const std::string &bid_uid,
                                                  const std::string &name,
                                                  const std::string &parent_uid)
    {
        return guarded([&] { return create_impl(db_path, bid_uid, name, parent_uid); });
    }

    ConditionFolderResult rename_condition_folder(const std::string &db_path,
                                                  const std::string &bid_uid,
                                                  const std::string &folder_uid,
                                                  const std::string &name)
    {
        return guarded([&] { return rename_impl(db_path, bid_uid, folder_uid, name); });
    }

    ConditionFolderResult delete_condition_folders(const std::string &db_path,
                                                   const std::string &bid_uid,
                                                   const std::vector<std::string> &folder_uids)
    {
        return guarded([&] { return delete_impl(db_path, bid_uid, folder_uids); });
    }

    ConditionFolderResult move_condition_to_folder(const std::string &db_path,
                                                   const std::string &bid_uid,
                                                   const std::string &condition_uid,
                                                   const std::string &folder_uid)
    {
        return guarded([&] { return move_impl(db_path, bid_uid, condition_uid, folder_uid); });
    }
}
