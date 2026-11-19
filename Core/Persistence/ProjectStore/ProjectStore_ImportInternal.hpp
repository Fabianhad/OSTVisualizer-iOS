// Internal header of the import (OST file -> database) steps. Not part of the public API: it exists so that
// ProjectStore+Import.cpp, ProjectStore+ImportWriter.cpp and ProjectStore+ImportMasterData.cpp share the steps and
// the host tests can run each of them on its own.
//
// Every step is the iOS counterpart of one method of the Windows ImportOperationsMixin
// (ost_visualizer/infrastructure/mdb/components/import_operations.py). A step returns false when the Windows method
// would raise (or, for the writer, when a statement fails); `error` then carries the Windows message where there is
// one ("Imported master-data identity is ambiguous for CdnTypes.Name: matching UIDs 40, 41.") or a description of
// the failed statement. The caller rolls the whole import transaction back.
#pragma once
#include "ProjectStore_Internal.hpp"
#include "../../Import/OstImportTransform.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ost_store
{
    // ---- typed values (_convert_access_value) -----------------------------------------------------------------

    struct ImportValue
    {
        enum Kind
        {
            Null,
            Integer,
            Real,
            Text
        };
        Kind kind = Null;
        long long integer = 0;
        double real = 0.0;
        std::string text;
    };

    // Windows _convert_access_value for the column types the iOS schema can express: NULL/"NULL" -> NULL for every
    // type; yesno -> -1 for "True"/"true"/"-1"/"1", otherwise 0; datetime (IMP-20): a value of exactly six whitespace
    // separated integers that form a valid date (datetime.datetime) keeps its raw text, anything else is NULL (also an
    // integer outside the C int range, where Windows raises an uncaught OverflowError: deliberate deviation, see
    // ProjectStore+ImportWriter.cpp); longbinary / memo: "" -> NULL, text kept; numeric types (type name contains one of
    // int, long, short, double, float, real, bit, byte, counter, currency, single, numeric, decimal): "" -> NULL, "0" -> 0,
    // a value with '.' -> float, otherwise integer, unparsable -> NULL; every other type keeps the text (including "").
    // Returns false only when the Windows code would raise: a number that is a valid integer SQLite cannot store (more
    // than 64 bits), which fails the import (`failure`, when given, gets the Python message).
    bool import_convert_value(const std::string &value, const std::string &type_name, ImportValue &out, std::string *failure = nullptr);

    // ---- table metadata and the row writer (_get_table_info, _insert_raw_row, _write_to_db) ------------------------

    struct ImportTableInfo
    {
        std::vector<std::string> columns;                           // as declared (PRAGMA table_info)
        std::unordered_set<std::string> column_set;                 // the same names, for O(1) lookups (filled with columns)
        std::unordered_map<std::string, std::string> types;         // column -> lower-cased declared type; YESNO columns "yesno"
        std::unordered_map<std::string, bool> primary_key;          // column -> part of the primary key
        // Exact (case-sensitive) membership, like the Python set of catalog column names.
        bool has_column(const std::string &name) const;
    };

    // Columns and types of `table` ({} for a table that does not exist or cannot be described, like the Windows
    // catalog lookup that swallows pyodbc.Error). A YESNO / DATETIME / IMAGE column of the Windows schema reports the type
    // "yesno" / "datetime" / "longbinary" (the iOS DDL declares them INTEGER / TEXT / BLOB).
    ImportTableInfo import_get_table_info(ost_db::OstDatabase &db, const std::string &table);

    // Windows _should_omit_autonumber_uid: the UID of an autonumber table with a blank value ("", "0", "NULL") is
    // left to the engine. The iOS counterpart of COUNTER is `UID INTEGER PRIMARY KEY` (or a declared type "counter").
    bool import_should_omit_autonumber_uid(const ImportTableInfo &info, const std::string &column, const std::string &value);

    // Windows _insert_raw_row: keeps only the attributes that are columns of the table, drops a blank autonumber UID,
    // converts the values by column type and INSERTs (never INSERT OR REPLACE). A row with no known column is
    // skipped. false = the INSERT failed (`error` names the table, row UID and columns like the Windows log line).
    bool import_insert_raw_row(ost_db::OstDatabase &db,
                               const std::string &table,
                               const ost_import::Row &row,
                               const ImportTableInfo &info,
                               std::string &error);

    // Windows _write_to_db: the Bids row, then BID_TABLES_WRITE_ORDER, then PAGE_SECTIONS. Tables that are not in
    // those lists are not written; a table without rows is not even described.
    bool import_write_to_db(ost_db::OstDatabase &db, const ost_import::OstRawData &remapped, std::string &error);

    // ---- UID floor and bid number (_get_max_uid, _assign_next_bid_no, _next_table_uid) ------------------------------

    // Highest UID of Bids, CdnTypes, JobStatuses, every bid/tail/page table the import can write, and of every
    // referencing child column (BID_RELATIONSHIPS), 0 for an empty database. Missing tables/columns are skipped.
    bool import_get_max_uid(ost_db::OstDatabase &db, long long &max_uid, std::string &error);

    // Settings.NextBidNo -> bid_row["BidNo"], Settings.NextBidNo + 1 stored. More than one Settings row, a missing
    // Settings table/column or a non-numeric value fail.
    bool import_assign_next_bid_no(ost_db::OstDatabase &db, ost_import::Row &bid_row, std::string &error);

    // MAX(UID)+1 of the table, raised above every UID that child tables already reference (a dangling reference
    // included).
    bool import_next_table_uid(ost_db::OstDatabase &db, const std::string &table, long long &next_uid, std::string &error);

    // ---- master data reconciliation (_resolve_*) ---------------------------------------------------------------

    // Windows master_data_identity_key: CdnTypes.Name is strip().lower(), everything else is the text itself.
    std::string import_master_data_identity_key(const std::string &table, const std::string &column, const std::string &value);

    // Windows ImportOperationsMixin._employee_identity_key: "no:<number>" (strip().casefold()) else
    // "name:<first>|<last>|<email>" (strip().casefold()) else "".
    std::string import_employee_identity_key(const ost_import::Row &row);

    // Each fills the old-UID -> new-UID map; CdnTypes/JobStatuses also advance max_uid to the last UID they inserted.
    bool import_resolve_cdn_types(ost_db::OstDatabase &db, const ost_import::OstRawData &raw, long long &max_uid,
                                  ost_import::UidMap &out, std::string &error);
    bool import_resolve_job_statuses(ost_db::OstDatabase &db, const ost_import::OstRawData &raw, long long &max_uid,
                                     ost_import::UidMap &out, std::string &error);
    bool import_resolve_access_levels(ost_db::OstDatabase &db, const ost_import::OstRawData &raw,
                                      ost_import::UidMap &out, std::string &error);
    bool import_resolve_pay_classes(ost_db::OstDatabase &db, const ost_import::OstRawData &raw,
                                    ost_import::UidMap &out, std::string &error);
    bool import_resolve_employees(ost_db::OstDatabase &db, const ost_import::OstRawData &raw,
                                  const ost_import::UidMap &pay_class_uid_map,
                                  const ost_import::UidMap &access_level_uid_map,
                                  ost_import::UidMap &out, std::string &error);
}
