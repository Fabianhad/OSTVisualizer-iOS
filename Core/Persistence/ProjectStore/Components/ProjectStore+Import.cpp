// ProjectStore component — import (OST file → database).
#include "../ProjectStore_Internal.hpp"
#include "../../OstSchema.hpp"

namespace ost_store
{

    static bool insert_rows(ost_db::OstDatabase &db,
                            const std::string &table,
                            const ost_import::RowList &rows)
    {
        if (rows.empty())
            return true;

        for (const auto &row : rows)
        {
            std::string row_sql = build_insert(table, row);
            if (row_sql.empty())
                return false;
            ost_db::Statement stmt = db.prepare(row_sql);
            if (!stmt.valid())
                return false;
            bind_row(stmt, row);
            stmt.step();
        }
        return true;
    }

    static std::unordered_map<std::string, std::string>
    resolve_global_table_by_name(ost_db::OstDatabase &db,
                                 const std::string &table,
                                 const ost_import::RowList &incoming)
    {
        std::unordered_map<std::string, std::string> uid_map;
        if (incoming.empty())
            return uid_map;

        std::unordered_map<std::string, std::string> existing_by_name;
        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name FROM [" + table + "]");
            if (stmt.valid())
            {
                while (stmt.step())
                {
                    existing_by_name[stmt.column_text(1)] = stmt.column_text(0);
                }
            }
        }

        int next_uid = 1;
        {
            ost_db::Statement stmt = db.prepare(
                "SELECT COALESCE(MAX(UID), 0) FROM [" + table + "]");
            if (stmt.valid() && stmt.step())
                next_uid = stmt.column_int(0) + 1;
        }

        for (const auto &row : incoming)
        {
            auto uid_it = row.find("UID");
            auto name_it = row.find("Name");
            std::string old_uid = (uid_it != row.end()) ? uid_it->second : "";
            std::string name = (name_it != row.end()) ? name_it->second : "";

            auto exist_it = existing_by_name.find(name);
            if (exist_it != existing_by_name.end())
            {
                uid_map[old_uid] = exist_it->second;
            }
            else
            {
                std::string new_uid = std::to_string(next_uid++);
                uid_map[old_uid] = new_uid;
                existing_by_name[name] = new_uid;

                ost_import::Row new_row = row;
                new_row["UID"] = new_uid;
                std::string sql = build_insert(table, new_row);
                if (!sql.empty())
                {
                    ost_db::Statement stmt = db.prepare(sql);
                    if (stmt.valid())
                    {
                        bind_row(stmt, new_row);
                        stmt.step();
                    }
                }
            }
        }
        return uid_map;
    }

    bool import_project(const std::string &db_path,
                        const ost_import::OstRawData &raw_data)
    {
        if (raw_data.empty())
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        db.begin_transaction();

        std::unordered_map<std::string, std::string> cdn_uid_map;
        std::unordered_map<std::string, std::string> job_status_uid_map;
        {
            auto cdn_it = raw_data.global_tables.find("CdnTypes");
            if (cdn_it != raw_data.global_tables.end())
                cdn_uid_map = resolve_global_table_by_name(db, "CdnTypes", cdn_it->second);

            auto js_it = raw_data.global_tables.find("JobStatuses");
            if (js_it != raw_data.global_tables.end())
                job_status_uid_map = resolve_global_table_by_name(db, "JobStatuses", js_it->second);
        }

        {
            ost_import::Row bid_row = raw_data.bid_row;
            if (!job_status_uid_map.empty())
            {
                auto js_it = bid_row.find("JobStatusUID");
                if (js_it != bid_row.end() && !js_it->second.empty())
                {
                    auto map_it = job_status_uid_map.find(js_it->second);
                    if (map_it != job_status_uid_map.end())
                        js_it->second = map_it->second;
                }
            }
            std::string sql = build_insert("Bids", bid_row);
            if (!sql.empty())
            {
                ost_db::Statement stmt = db.prepare(sql);
                if (stmt.valid())
                {
                    bind_row(stmt, bid_row);
                    stmt.step();
                }
            }
        }

        for (const char *table : ost_schema::BID_SECTIONS)
        {
            auto it = raw_data.bid_tables.find(table);
            if (it == raw_data.bid_tables.end())
                continue;

            if (it->first == "BidConditions" && !cdn_uid_map.empty())
            {
                for (const auto &row : it->second)
                {
                    ost_import::Row remapped = row;
                    auto ctu_it = remapped.find("CdnTypeUID");
                    if (ctu_it != remapped.end() && !ctu_it->second.empty())
                    {
                        auto map_it = cdn_uid_map.find(ctu_it->second);
                        if (map_it != cdn_uid_map.end())
                            ctu_it->second = map_it->second;
                    }
                    std::string sql = build_insert("BidConditions", remapped);
                    if (!sql.empty())
                    {
                        ost_db::Statement stmt = db.prepare(sql);
                        if (stmt.valid())
                        {
                            bind_row(stmt, remapped);
                            stmt.step();
                        }
                    }
                }
            }
            else
            {
                insert_rows(db, it->first, it->second);
            }
        }

        // Insert remaining bid tables not in BID_SECTIONS
        for (const auto &pair : raw_data.bid_tables)
        {
            bool in_sections = false;
            for (const char *s : ost_schema::BID_SECTIONS)
                if (pair.first == s)
                {
                    in_sections = true;
                    break;
                }
            if (!in_sections)
                insert_rows(db, pair.first, pair.second);
        }

        for (const char *table : ost_schema::PAGE_SECTIONS)
        {
            auto it = raw_data.page_tables.find(table);
            if (it != raw_data.page_tables.end())
                insert_rows(db, it->first, it->second);
        }

        // Insert remaining page tables not in PAGE_SECTIONS
        for (const auto &pair : raw_data.page_tables)
        {
            bool in_sections = false;
            for (const char *s : ost_schema::PAGE_SECTIONS)
                if (pair.first == s)
                {
                    in_sections = true;
                    break;
                }
            if (!in_sections)
                insert_rows(db, pair.first, pair.second);
        }

        for (const auto &pair : raw_data.global_tables)
        {
            if (pair.first == "CdnTypes" || pair.first == "JobStatuses")
                continue;

            for (const auto &row : pair.second)
            {
                std::string sql = "INSERT OR IGNORE INTO [" + pair.first + "] (";
                std::string placeholders;
                bool first = true;
                for (const auto &col : row)
                {
                    if (!first)
                    {
                        sql += ", ";
                        placeholders += ", ";
                    }
                    sql += "[" + col.first + "]";
                    placeholders += "?";
                    first = false;
                }
                sql += ") VALUES (" + placeholders + ")";
                ost_db::Statement stmt = db.prepare(sql);
                if (stmt.valid())
                {
                    bind_row(stmt, row);
                    stmt.step();
                }
            }
        }

        db.commit();
        return true;
    }

    bool is_bid_imported(const std::string &db_path, const std::string &bid_uid)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        ost_db::Statement stmt = db.prepare(
            "SELECT COUNT(*) FROM Bids WHERE UID = ?");
        if (!stmt.valid())
            return false;
        stmt.bind_text(1, bid_uid);
        if (stmt.step())
            return stmt.column_int(0) > 0;
        return false;
    }

}
