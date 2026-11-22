// ProjectStore component — import (OST file → database).
//
// Windows counterpart: ImportOperationsMixin.import_ost_data (import_operations.py) driven by OstImporter._transform
// (ost_importer.py). The steps (UID floor, master-data reconciliation, UID remap, bid number, typed row writes) live in
// ProjectStore+ImportWriter.cpp / ProjectStore+ImportMasterData.cpp and Import/OstImportTransform.cpp.
#include "../ProjectStore_ImportInternal.hpp"
#include "../../OstSchema.hpp"

namespace ost_store
{

    bool import_project(const std::string &db_path,
                        const ost_import::OstRawData &raw_data,
                        ImportResult *result)
    {
        ImportResult scratch;
        ImportResult &out = result ? *result : scratch;
        out = ImportResult();

        if (raw_data.empty())
        {
            out.error = "The OST file has no Bid element.";
            return false;
        }

        ost_db::OstDatabase db;
        if (!db.open(db_path))
        {
            out.error = "The database could not be opened: " + db.last_error_message();
            return false;
        }

        if (!db.begin_transaction())
        {
            out.error = "The import transaction could not be started: " + db.last_error_message();
            return false;
        }

        // Windows runs the whole import (master data, bid number, bid row, every table) in one connection scope: any
        // failure rolls everything back and the import reports failure (import_operations.py import_ost_data).
        auto fail = [&db, &out](const std::string &message) -> bool
        {
            if (db.in_transaction())
                db.rollback();
            out.error = message;
            return false;
        };

        std::string error;
        long long max_uid = 0;
        if (!import_get_max_uid(db, max_uid, error))
            return fail(error);

        ost_import::UidMap cdn_uid_map, job_status_uid_map, access_level_uid_map, pay_class_uid_map, employee_uid_map;
        if (!import_resolve_cdn_types(db, raw_data, max_uid, cdn_uid_map, error))
            return fail(error);
        if (!import_resolve_job_statuses(db, raw_data, max_uid, job_status_uid_map, error))
            return fail(error);
        if (!import_resolve_access_levels(db, raw_data, access_level_uid_map, error))
            return fail(error);
        if (!import_resolve_pay_classes(db, raw_data, pay_class_uid_map, error))
            return fail(error);
        if (!import_resolve_employees(db, raw_data, pay_class_uid_map, access_level_uid_map, employee_uid_map, error))
            return fail(error);

        ost_import::OstRawData remapped = ost_import::transform(
            raw_data, max_uid, cdn_uid_map, job_status_uid_map, employee_uid_map, pay_class_uid_map);

        if (!import_assign_next_bid_no(db, remapped.bid_row, error))
            return fail(error);

        // The importer has no target-project picker on iOS (Windows ImportHandler passes target_project_uid): the bid
        // is stored without a project, i.e. Bids.BidProjectUID = NULL (import_operations.py:74-77).
        remapped.bid_row["BidProjectUID"] = "NULL";

        if (!import_write_to_db(db, remapped, error))
            return fail(error);

        // A failed COMMIT is a failed import (OstDatabase::commit has already rolled the transaction back).
        if (!db.commit())
            return fail("The import could not be committed: " + db.last_error_message());

        auto bid_uid = remapped.bid_row.find("UID");
        if (bid_uid != remapped.bid_row.end() && !bid_uid->second.empty() && bid_uid->second != "0" &&
            bid_uid->second != "NULL")
        {
            out.bid_uid = bid_uid->second;
        }
        else
        {
            // The file's Bid had no UID: the database numbered the row itself (autonumber = highest UID so far).
            ost_db::Statement stmt = db.prepare("SELECT MAX([UID]) FROM [Bids]");
            if (stmt.valid() && stmt.step())
                out.bid_uid = stmt.column_text(0);
        }
        return true;
    }

}
