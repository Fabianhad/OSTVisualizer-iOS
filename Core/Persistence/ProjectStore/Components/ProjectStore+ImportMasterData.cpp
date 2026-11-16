// ProjectStore component — import master data: CdnTypes, JobStatuses, AccessLevels, PayClasses, Employees.
// Windows counterpart: ost_visualizer/infrastructure/mdb/components/import_operations.py (_resolve_cdn_types,
// _resolve_job_statuses, _resolve_access_levels, _resolve_pay_classes, _resolve_employees, the _load_existing_* loaders,
// _employee_identity_key) and ost_visualizer/infrastructure/database/master_data_identity.py.
#include "../ProjectStore_ImportInternal.hpp"
#include "../../../Import/TextRules.hpp"
#include <algorithm>
#include <functional>

namespace ost_store
{
    namespace
    {
        using ost_import::OstRawData;
        using ost_import::Row;
        using ost_import::RowList;
        using ost_import::UidMap;
        namespace text_rules = ost_import::text_rules;

        // master_data_identity.MasterDataCandidateIndex: identity key -> distinct UIDs in insertion order.
        using CandidateIndex = std::unordered_map<std::string, std::vector<std::string>>;

        std::string get_text(const Row &row, const char *key)
        {
            auto it = row.find(key);
            return it == row.end() ? std::string() : it->second;
        }

        void add_candidate(CandidateIndex &candidates, const std::string &key, const std::string &uid)
        {
            std::vector<std::string> &matching = candidates[key];
            if (std::find(matching.begin(), matching.end(), uid) == matching.end())
                matching.push_back(uid);
        }

        std::string ambiguity_message(const std::vector<std::string> &matching, const std::string &label)
        {
            std::vector<std::string> sorted = matching;
            std::sort(sorted.begin(), sorted.end());
            std::string joined;
            for (size_t i = 0; i < sorted.size(); ++i)
            {
                if (i)
                    joined += ", ";
                joined += sorted[i];
            }
            return "Imported master-data identity is ambiguous for " + label + ": matching UIDs " + joined + ".";
        }

        // resolve_master_data_candidate: false = ambiguous (error set); otherwise `found` says whether a UID matched.
        bool resolve_candidate(const CandidateIndex &candidates, const std::string &key, const std::string &label,
                               bool &found, std::string &uid, std::string &error)
        {
            found = false;
            auto it = candidates.find(key);
            if (it == candidates.end() || it->second.empty())
                return true;
            if (it->second.size() > 1)
            {
                error = ambiguity_message(it->second, label);
                return false;
            }
            found = true;
            uid = it->second.front();
            return true;
        }

        // require_unique_master_data_uids
        bool require_unique_uids(const std::vector<std::string> &uids, const std::string &table, std::string &error)
        {
            std::vector<std::string> seen;
            for (const std::string &uid : uids)
            {
                if (std::find(seen.begin(), seen.end(), uid) != seen.end())
                {
                    error = table + " contains duplicate UID " + uid +
                            "; authoritative master-data UIDs must be unique.";
                    return false;
                }
                seen.push_back(uid);
            }
            return true;
        }

        // require_unambiguous_incoming_identities
        bool require_unambiguous_incoming(const RowList &rows,
                                          const std::function<std::string(const Row &)> &identity_key,
                                          const std::string &label, bool ignore_empty, std::string &error)
        {
            CandidateIndex candidates;
            std::vector<std::string> key_order;
            size_t position = 0;
            for (const auto &row : rows)
            {
                ++position;
                const std::string key = identity_key(row);
                if (ignore_empty && key.empty())
                    continue;
                const std::string raw_uid = get_text(row, "UID");
                const std::string source_identity = raw_uid.empty() ? "source row " + std::to_string(position) : raw_uid;
                if (candidates.find(key) == candidates.end())
                    key_order.push_back(key);
                add_candidate(candidates, key, source_identity);
            }
            for (const std::string &key : key_order)
            {
                bool found = false;
                std::string uid;
                if (!resolve_candidate(candidates, key, label, found, uid, error))
                    return false;
            }
            return true;
        }

        // _load_existing_uid_candidates_by_column. Statement failures other than a schema mismatch fail the import.
        bool load_existing_by_column(ost_db::OstDatabase &db, const std::string &table, const std::string &column,
                                     CandidateIndex &out, std::string &error)
        {
            out.clear();
            if (!db.table_exists(table))
                return true;
            if (!db.column_exists(table, "UID") || !db.column_exists(table, column))
                return true;
            ost_db::Statement stmt = db.prepare("SELECT [UID], [" + column + "] FROM [" + table + "]");
            if (!stmt.valid())
            {
                error = "SELECT [UID], [" + column + "] FROM [" + table + "] failed: " + stmt.error_message();
                return false;
            }
            std::vector<std::pair<std::string, std::string>> rows;
            while (stmt.step())
                rows.emplace_back(stmt.column_text(0), stmt.column_text(1));
            if (stmt.failed())
            {
                error = "SELECT [UID], [" + column + "] FROM [" + table + "] failed: " + stmt.error_message();
                return false;
            }
            std::vector<std::string> uids;
            for (const auto &row : rows)
                uids.push_back(row.first);
            if (!require_unique_uids(uids, table, error))
                return false;
            for (const auto &row : rows)
                add_candidate(out, import_master_data_identity_key(table, column, row.second), row.first);
            return true;
        }

        // _load_existing_employee_uid_candidates_by_key
        bool load_existing_employees(ost_db::OstDatabase &db, CandidateIndex &out, std::string &error)
        {
            out.clear();
            if (!db.table_exists("Employees"))
                return true;
            if (!db.column_exists("Employees", "UID"))
                return true;
            std::vector<std::string> columns;
            for (const char *column : {"UID", "EmployeeNo", "FirstName", "LastName", "EMail"})
                if (db.column_exists("Employees", column))
                    columns.push_back(column);
            std::string select_list;
            for (size_t i = 0; i < columns.size(); ++i)
                select_list += (i ? ", [" : "[") + columns[i] + "]";
            ost_db::Statement stmt = db.prepare("SELECT " + select_list + " FROM [Employees]");
            if (!stmt.valid())
            {
                error = "SELECT " + select_list + " FROM [Employees] failed: " + stmt.error_message();
                return false;
            }
            std::vector<Row> rows;
            while (stmt.step())
            {
                Row row;
                for (size_t i = 0; i < columns.size(); ++i)
                    row[columns[i]] = stmt.column_text(static_cast<int>(i));
                rows.push_back(std::move(row));
            }
            if (stmt.failed())
            {
                error = "SELECT " + select_list + " FROM [Employees] failed: " + stmt.error_message();
                return false;
            }
            std::vector<std::string> uids;
            for (const auto &row : rows)
                uids.push_back(row.at("UID"));
            if (!require_unique_uids(uids, "Employees", error))
                return false;
            for (const auto &row : rows)
            {
                const std::string key = import_employee_identity_key(row);
                if (!key.empty())
                    add_candidate(out, key, row.at("UID"));
            }
            return true;
        }

        // _resolve_cdn_types / _resolve_job_statuses: identity by Name, new UIDs max_uid + 1, + 2, ...
        bool resolve_name_table_above_floor(ost_db::OstDatabase &db, const OstRawData &raw, const std::string &table,
                                            long long &max_uid, UidMap &out, std::string &error)
        {
            out.clear();
            auto incoming_it = raw.global_tables.find(table);
            if (incoming_it == raw.global_tables.end() || incoming_it->second.empty())
                return true;
            const RowList &incoming = incoming_it->second;
            const std::string label = table + ".Name";
            if (!require_unambiguous_incoming(
                    incoming,
                    [&table](const Row &row)
                    { return import_master_data_identity_key(table, "Name", get_text(row, "Name")); },
                    label, false, error))
                return false;
            CandidateIndex existing_by_name;
            if (!load_existing_by_column(db, table, "Name", existing_by_name, error))
                return false;
            long long next_uid = max_uid + 1;
            const ImportTableInfo info = import_get_table_info(db, table);
            for (const auto &row : incoming)
            {
                const std::string old_uid = get_text(row, "UID");
                const std::string name = import_master_data_identity_key(table, "Name", get_text(row, "Name"));
                bool found = false;
                std::string existing_uid;
                if (!resolve_candidate(existing_by_name, name, label, found, existing_uid, error))
                    return false;
                if (found)
                {
                    out[old_uid] = existing_uid;
                    continue;
                }
                const std::string new_uid = std::to_string(next_uid);
                ++next_uid;
                out[old_uid] = new_uid;
                add_candidate(existing_by_name, name, new_uid);
                Row insert_row = row;
                insert_row["UID"] = new_uid;
                if (!import_insert_raw_row(db, table, insert_row, info, error))
                    return false;
            }
            max_uid = next_uid - 1;
            return true;
        }

        // _resolve_access_levels / _resolve_pay_classes: identity by one text column (blank never matches), new UIDs
        // from the table's own next UID.
        bool resolve_blank_free_table(ost_db::OstDatabase &db, const OstRawData &raw, const std::string &table,
                                      const std::string &column, UidMap &out, std::string &error)
        {
            out.clear();
            auto incoming_it = raw.global_tables.find(table);
            if (incoming_it == raw.global_tables.end() || incoming_it->second.empty())
                return true;
            if (!db.table_exists(table))
                return true;
            const RowList &incoming = incoming_it->second;
            const std::string label = table + "." + column;
            if (!require_unambiguous_incoming(
                    incoming,
                    [&table, &column](const Row &row)
                    { return import_master_data_identity_key(table, column, get_text(row, column.c_str())); },
                    label, true, error))
                return false;
            CandidateIndex existing;
            if (!load_existing_by_column(db, table, column, existing, error))
                return false;
            long long next_uid = 0;
            if (!import_next_table_uid(db, table, next_uid, error))
                return false;
            const ImportTableInfo info = import_get_table_info(db, table);
            for (const auto &row : incoming)
            {
                const std::string old_uid = get_text(row, "UID");
                const std::string key = import_master_data_identity_key(table, column, get_text(row, column.c_str()));
                bool found = false;
                std::string existing_uid;
                if (!key.empty() && !resolve_candidate(existing, key, label, found, existing_uid, error))
                    return false;
                if (found)
                {
                    out[old_uid] = existing_uid;
                    continue;
                }
                const std::string new_uid = std::to_string(next_uid);
                ++next_uid;
                out[old_uid] = new_uid;
                if (!key.empty())
                    add_candidate(existing, key, new_uid);
                Row insert_row = row;
                insert_row["UID"] = new_uid;
                if (!import_insert_raw_row(db, table, insert_row, info, error))
                    return false;
            }
            return true;
        }
    }

    std::string import_master_data_identity_key(const std::string &table, const std::string &column, const std::string &value)
    {
        if (table == "CdnTypes" && column == "Name")
            return text_rules::py_lower(text_rules::py_strip(value));
        return value;
    }

    std::string import_employee_identity_key(const Row &row)
    {
        const std::string employee_no = text_rules::py_casefold(text_rules::py_strip(get_text(row, "EmployeeNo")));
        if (!employee_no.empty())
            return "no:" + employee_no;
        const std::string first_name = text_rules::py_casefold(text_rules::py_strip(get_text(row, "FirstName")));
        const std::string last_name = text_rules::py_casefold(text_rules::py_strip(get_text(row, "LastName")));
        const std::string email = text_rules::py_casefold(text_rules::py_strip(get_text(row, "EMail")));
        if (!first_name.empty() || !last_name.empty() || !email.empty())
            return "name:" + first_name + "|" + last_name + "|" + email;
        return std::string();
    }

    bool import_resolve_cdn_types(ost_db::OstDatabase &db, const OstRawData &raw, long long &max_uid,
                                  UidMap &out, std::string &error)
    {
        return resolve_name_table_above_floor(db, raw, "CdnTypes", max_uid, out, error);
    }

    bool import_resolve_job_statuses(ost_db::OstDatabase &db, const OstRawData &raw, long long &max_uid,
                                     UidMap &out, std::string &error)
    {
        return resolve_name_table_above_floor(db, raw, "JobStatuses", max_uid, out, error);
    }

    bool import_resolve_access_levels(ost_db::OstDatabase &db, const OstRawData &raw, UidMap &out, std::string &error)
    {
        return resolve_blank_free_table(db, raw, "AccessLevels", "Description", out, error);
    }

    bool import_resolve_pay_classes(ost_db::OstDatabase &db, const OstRawData &raw, UidMap &out, std::string &error)
    {
        return resolve_blank_free_table(db, raw, "PayClasses", "Name", out, error);
    }

    bool import_resolve_employees(ost_db::OstDatabase &db, const OstRawData &raw, const UidMap &pay_class_uid_map,
                                  const UidMap &access_level_uid_map, UidMap &out, std::string &error)
    {
        out.clear();
        auto incoming_it = raw.global_tables.find("Employees");
        if (incoming_it == raw.global_tables.end() || incoming_it->second.empty())
            return true;
        if (!db.table_exists("Employees"))
            return true;
        const RowList &incoming = incoming_it->second;
        const std::string label = "Employees business key";
        if (!require_unambiguous_incoming(
                incoming, [](const Row &row) { return import_employee_identity_key(row); }, label, true, error))
            return false;
        CandidateIndex existing_by_key;
        if (!load_existing_employees(db, existing_by_key, error))
            return false;
        long long next_uid = 0;
        if (!import_next_table_uid(db, "Employees", next_uid, error))
            return false;
        const ImportTableInfo info = import_get_table_info(db, "Employees");
        for (const auto &row : incoming)
        {
            const std::string old_uid = get_text(row, "UID");
            const std::string employee_key = import_employee_identity_key(row);
            bool found = false;
            std::string existing_uid;
            if (!employee_key.empty() &&
                !resolve_candidate(existing_by_key, employee_key, label, found, existing_uid, error))
                return false;
            if (found)
            {
                out[old_uid] = existing_uid;
                continue;
            }
            const std::string new_uid = std::to_string(next_uid);
            ++next_uid;
            out[old_uid] = new_uid;
            if (!employee_key.empty())
                add_candidate(existing_by_key, employee_key, new_uid);
            Row insert_row = row;
            insert_row["UID"] = new_uid;
            const std::string pay_class_uid = get_text(insert_row, "PayClassUID");
            if (!pay_class_uid.empty())
            {
                auto mapped = pay_class_uid_map.find(pay_class_uid);
                insert_row["PayClassUID"] = mapped == pay_class_uid_map.end() ? std::string("NULL") : mapped->second;
            }
            const std::string access_level_uid = get_text(insert_row, "AccessLevelUID");
            if (!access_level_uid.empty())
            {
                auto mapped = access_level_uid_map.find(access_level_uid);
                insert_row["AccessLevelUID"] =
                    mapped == access_level_uid_map.end() ? std::string("NULL") : mapped->second;
            }
            if (!import_insert_raw_row(db, "Employees", insert_row, info, error))
                return false;
        }
        return true;
    }
}
