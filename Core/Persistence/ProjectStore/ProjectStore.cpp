// Layer: Persistence
// Depends on: Domain (via ProjectData.hpp, DatabaseHierarchy.hpp), OstDatabase, OstSchema
// Forbidden: UIKit, Foundation, Renderer, ImportService
// ProjectStore is the ONLY class that executes SQL. All DB <-> domain mapping happens here.
// All CRUD operates directly on the shared schema tables (BidTakeoffs, BidLayers, BidProjects, etc.).
//
// Component files in Components/:
//   ProjectStore+Load.cpp       — load_project
//   ProjectStore+Import.cpp     — import_project
//   ProjectStore+Folders.cpp    — load_hierarchy, folder CRUD
//   ProjectStore+Takeoffs.cpp   — takeoff CRUD, layer visibility
//   ProjectStore+Conditions.cpp — condition CRUD, render config
#include "ProjectStore.hpp"
#include "ProjectStore_Integrity.hpp"
#include "../OstDatabase.hpp"
#include "../OstSchema.hpp"
#include "../../Domain/CoreLog.hpp"
#include "../../Domain/PageDeletion.hpp"
#include "../../Domain/PositionParser.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <ctime>
#include <sstream>

namespace ost_store
{
    static const char *DELETED_BIDS_PROJECT_NAME = ost_domain::DELETED_BIDS_FOLDER_NAME;
    static const char *DELETED_BIDS_PROJECT_UID = ost_domain::DELETED_BIDS_FOLDER_UID;

    // Returns the UID of the system project 'Deleted Bids' ("1"), creating the row when the database has none.
    // Windows (project_constants.py): the system project is the BidProjects row with UID 1, whatever its name; nothing is looked up,
    // renamed, merged or deleted by name. database_creator.py puts (UID 1, 'Deleted Bids') into every new database.
    // Returns "" when any statement failed: the caller's open transaction must then be rolled back.
    static std::string resolve_deleted_bids_project_uid(ost_db::OstDatabase &db)
    {
        ost_db::Statement existing = db.prepare(
            "SELECT 1 FROM BidProjects WHERE UID = ?");
        if (!existing.valid())
            return "";
        existing.bind_text(1, DELETED_BIDS_PROJECT_UID);
        const ost_db::StepResult existing_result = existing.step_result();
        if (existing_result == ost_db::StepResult::Error)
            return "";
        if (existing_result == ost_db::StepResult::Row)
            return DELETED_BIDS_PROJECT_UID;

        ost_db::Statement insert = db.prepare(
            "INSERT INTO BidProjects (UID, Name) VALUES (?, ?)");
        if (!insert.valid())
            return "";
        insert.bind_text(1, DELETED_BIDS_PROJECT_UID);
        insert.bind_text(2, DELETED_BIDS_PROJECT_NAME);
        if (!insert.run())
            return "";
        return DELETED_BIDS_PROJECT_UID;
    }

    static bool bid_is_in_deleted_bids(ost_db::OstDatabase &db,
                                       const std::string &bid_uid,
                                       const std::string &deleted_project_uid)
    {
        ost_db::Statement stmt = db.prepare(
            "SELECT 1 FROM Bids WHERE UID = ? AND BidProjectUID = ?");
        if (!stmt.valid())
            return false;
        stmt.bind_text(1, bid_uid);
        stmt.bind_text(2, deleted_project_uid);
        return stmt.step();
    }

    // parse_position and extract_z_value are thin wrappers over the Windows ports in Domain/PositionParser.hpp
    // (domain/utils/position.py::parse_position, infrastructure/parsers/position_parser.py::extract_z_value_from_name).
    std::vector<double> parse_position(const std::string &pos_str)
    {
        return ost_domain::parse_position(pos_str);
    }

    // The Windows extract_z_value_from_name raises ValueError for an unreadable fraction (zero denominator, e.g.
    // "Wall @B 1/0\"") and, since 0a24294, for a number beyond the float range ("Huge @T " + 400 nines + "'x"); the Windows MDB
    // reader (bid_data_reader.py:900-910) catches it, logs "Ignoring unreadable elevation in condition ..." as a warning and stores
    // (0.0, False): the other conditions of the bid load as usual. The result is ost_domain::derive_elevation_from_name's, the
    // derivation the loader shares with condition_to_save (COND-ELEV-01): what the app keeps in memory after a Save of the dialog
    // equals what this loader reads back. The warning goes through ost_log (only the loader logs it, not a Save of the dialog).
    std::pair<double, bool> extract_z_value(const std::string &name)
    {
        double z_inches = 0.0;
        bool is_top = false;
        std::string error;
        if (!ost_domain::extract_z_value_from_name(name, z_inches, is_top, &error))
        {
            ost_log::warning("Ignoring unreadable elevation in condition '" + name + "': " + error);
            z_inches = 0.0; // the derivation of derive_elevation_from_name: (0.0, false)
            is_top = false;
        }
        return {z_inches, is_top};
    }

    bool create_schema(const std::string &db_path)
    {
        auto fail = [](ost_db::OstDatabase &db) -> bool
        {
            db.rollback();
            return false;
        };

        auto sql_escape = [](const std::string &value) -> std::string
        {
            std::string escaped;
            escaped.reserve(value.size());
            for (char ch : value)
            {
                escaped += ch;
                if (ch == '\'')
                    escaped += '\'';
            }
            return escaped;
        };

        auto default_settings_name = [](const std::string &path) -> std::string
        {
            size_t slash = path.find_last_of("/\\");
            std::string name = (slash == std::string::npos)
                                   ? path
                                   : path.substr(slash + 1);
            size_t dot = name.rfind('.');
            if (dot != std::string::npos)
                name = name.substr(0, dot);
            return name.empty() ? "OSTVisualizer" : name;
        };

        auto current_timestamp = []() -> std::string
        {
            std::time_t now = std::time(nullptr);
            std::tm local_tm{};
#if defined(_WIN32)
            localtime_s(&local_tm, &now);
#else
            localtime_r(&now, &local_tm);
#endif

            char buffer[20] = {};
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
            return buffer;
        };

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;

        for (const char *ddl : ost_schema::TABLE_DDL)
        {
            if (!db.exec(ddl))
                return fail(db);
        }

        for (const auto &index : ost_schema::FK_INDEXES)
        {
            const char *table = index.first;
            const char *column = index.second;
            if (!db.exec(
                    "CREATE INDEX IF NOT EXISTS [" + std::string(table) + "_" +
                    std::string(column) + "] ON [" + table + "] ([" + column + "])"))
            {
                return fail(db);
            }
        }

        for (const char *ddl : ost_schema::UNIQUE_INDEX_DDL)
        {
            if (!db.exec(ddl))
                return fail(db);
        }

        if (!db.exec("INSERT OR IGNORE INTO _AppMeta (key, value) VALUES ('schema_version', '1')"))
            return fail(db);

        if (!db.exec(
                "CREATE TABLE IF NOT EXISTS _AppPreferences ("
                "Key TEXT PRIMARY KEY, "
                "Value TEXT NOT NULL)"))
        {
            return fail(db);
        }

        const std::string settings_name = sql_escape(default_settings_name(db_path));
        const std::string created_at = sql_escape(current_timestamp());
        if (!db.exec(
                "INSERT INTO [Settings] ([Name], [Created], [NextBidNo], "
                "[LoginRequired], [MeasureBase], [PriceUsing], "
                "[QuantitiesInLegend], [HoursPerDay], [StartWeekOn], "
                "[GridCountMethod], [TakeoffIncrements], [ScaleStyle], "
                "[IsCustomScale], [ScaleFactor1], [ScaleFactor2], "
                "[PageScale], [PageWidth], [PageHeight], "
                "[LabelHours1], [LabelHours2], [LabelHours3], [LabelHours4], "
                "[IgnoreBidAreas], [SendImageFiles], "
                "[BackupNo], [BackupPeriod], [CompressPeriod]) "
                "SELECT '" +
                settings_name + "', '" + created_at + "', "
                                                      "1, 0, 0, 0, -1, 8, 0, 0, 1.0, 1, 0, 0.125, 12.0, "
                                                      "1.0, 42.0, 30.0, "
                                                      "'Regular', 'Overtime', 'Time + 1/2', 'Double', "
                                                      "0, 0, 2, 2, 2 "
                                                      "WHERE NOT EXISTS (SELECT 1 FROM [Settings])"))
        {
            return fail(db);
        }

        if (resolve_deleted_bids_project_uid(db).empty())
            return fail(db);

        for (const auto &layer : ost_schema::DEFAULT_LAYERS)
        {
            const std::string layer_name = sql_escape(layer.name);
            if (!db.exec(
                    "INSERT INTO [BidLayers] "
                    "([IsTemplate], [Name], [Show], [IsLocked], [Sequence]) "
                    "SELECT -1, '" +
                    layer_name + "', " + std::to_string(layer.show) +
                    ", " + std::to_string(layer.is_locked) + ", " +
                    std::to_string(layer.sequence) + " "
                                                     "WHERE NOT EXISTS (SELECT 1 FROM [BidLayers] "
                                                     "WHERE [IsTemplate] <> 0 AND [Name] = '" +
                    layer_name + "')"))
            {
                return fail(db);
            }
        }

        for (int version : ost_schema::SCHEMA_VERSIONS)
        {
            if (!db.exec(
                    "INSERT INTO [SchemaRegistry] ([Version], [Product]) "
                    "SELECT " +
                    std::to_string(version) + ", 2 "
                                              "WHERE NOT EXISTS (SELECT 1 FROM [SchemaRegistry] "
                                              "WHERE [Version] = " +
                    std::to_string(version) +
                    " AND [Product] = 2)"))
            {
                return fail(db);
            }
        }

        if (!db.commit())
            return fail(db);

        return true;
    }

    bool delete_project(const std::string &db_path, const std::string &bid_uid)
    {
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;

        const std::string deleted_project_uid = resolve_deleted_bids_project_uid(db);
        if (deleted_project_uid.empty())
        {
            db.rollback();
            return false;
        }

        // The folder the bid is in. Windows only knows the projects of the hierarchy (project_data_service.find_project_uid_for_bid), so a
        // BidProjectUID that is NULL, 0 or a UID without a BidProjects row (a dangling reference) is NO project: the bid is an orphan
        // ("" here). A bid that does not exist fails (move_bids_to_project requires every bid to exist).
        std::string current_project_uid;
        {
            ost_db::Statement current = db.prepare(
                "SELECT project.UID FROM Bids AS bid "
                "LEFT JOIN BidProjects AS project ON project.UID = bid.BidProjectUID "
                "WHERE bid.UID = ?");
            if (!current.valid())
            {
                db.rollback();
                return false;
            }
            current.bind_int64(1, bid);
            if (current.step_result() != ost_db::StepResult::Row)
            {
                db.rollback();
                return false;
            }
            if (current.column_type(0) != ost_db::Statement::TypeNull)
                current_project_uid = current.column_text(0);
        }
        if (current_project_uid == deleted_project_uid)
            return db.commit();

        // project_write_handler._delete_bids -> move_bids_to_project(bid, trash, orig_project_uid = its folder or None): the restore
        // reference OrigBidProjectUID is written only when the bid IS in a folder (BRW-10: never a dangling uid); for an orphan the
        // writer does not touch the column, so a stored value survives (BRW-11, decided).
        const bool has_origin = !current_project_uid.empty();
        ost_db::Statement stmt = db.prepare(
            has_origin ? "UPDATE Bids SET BidProjectUID = ?, OrigBidProjectUID = ? WHERE UID = ?"
                       : "UPDATE Bids SET BidProjectUID = ? WHERE UID = ?");
        if (!stmt.valid())
        {
            db.rollback();
            return false;
        }
        stmt.bind_text(1, deleted_project_uid);
        if (has_origin)
        {
            stmt.bind_text(2, current_project_uid);
            stmt.bind_int64(3, bid);
        }
        else
        {
            stmt.bind_int64(2, bid);
        }
        if (!stmt.run())
        {
            db.rollback();
            return false;
        }

        return db.commit();
    }

    bool restore_bid(const std::string &db_path, const std::string &bid_uid)
    {
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;

        // project_write_handler.restore_bids: the bid goes back to its OrigBidProjectUID (BRW-09); no original (NULL / 0) -> orphan_bids
        // (BidProjectUID = NULL). OrigBidProjectUID itself is not changed. A bid that does not exist has no original either, and
        // orphaning an absent bid is a success that changes nothing.
        // Kept by user decision 2026-11-19 (BRW-09, see PORTING_BLOCKERS.md): when OrigBidProjectUID names a folder that no longer
        // exists, Windows move_bids_to_project requires the project and fails the whole restore (nothing changes, "Restore Error").
        // That can only be foreign data (delete_folder clears the reference); iOS restores the bid unassigned, what its Restore
        // action always did.
        long long target = 0;
        bool has_target = false;
        {
            ost_db::Statement origin = db.prepare(
                "SELECT project.UID FROM Bids AS bid "
                "INNER JOIN BidProjects AS project ON project.UID = bid.OrigBidProjectUID "
                "WHERE bid.UID = ?");
            if (!origin.valid())
            {
                db.rollback();
                return false;
            }
            origin.bind_int64(1, bid);
            const ost_db::StepResult found = origin.step_result();
            if (found == ost_db::StepResult::Error)
            {
                db.rollback();
                return false;
            }
            if (found == ost_db::StepResult::Row)
            {
                target = origin.column_int64(0);
                has_target = true;
            }
        }

        ost_db::Statement stmt = db.prepare(
            has_target ? "UPDATE Bids SET BidProjectUID = ? WHERE UID = ?"
                       : "UPDATE Bids SET BidProjectUID = NULL WHERE UID = ?");
        if (!stmt.valid())
        {
            db.rollback();
            return false;
        }
        if (has_target)
        {
            stmt.bind_int64(1, target);
            stmt.bind_int64(2, bid);
        }
        else
        {
            stmt.bind_int64(1, bid);
        }
        if (!stmt.run())
        {
            db.rollback();
            return false;
        }

        return db.commit();
    }

    namespace
    {
        // The Windows delete_bids cascade for ONE bid (bid_operations.py _delete_bids_cascade_chunk), step by step in the same
        // order and with the same guards: a step whose table or column the schema lacks is skipped (optional_table_missing /
        // column_exists), like the Windows writer does for a database that does not carry every optional table. Every statement
        // takes the bid uid as its only parameter; a statement that fails fails the cascade (the caller rolls the transaction back).
        class BidCascade
        {
        public:
            BidCascade(ost_db::OstDatabase &db, long long bid_uid) : _db(db), _bid(bid_uid) {}

            bool run()
            {
                // BidPercents of the bid's takeoffs (also reachable through no page and no bid column).
                if (has_column("BidPercents", "BidTakeoffUID") && has_column("BidTakeoffs", "UID") &&
                    has_column("BidTakeoffs", "BidUID"))
                {
                    if (!execute("DELETE FROM [BidPercents] WHERE [BidTakeoffUID] IN "
                                 "(SELECT [UID] FROM [BidTakeoffs] WHERE [BidUID] = ?)"))
                        return false;
                }
                // Another bid's selected page that lies in the deleted bid is cleared before the pages go.
                if (has_column("BidSettings", "BidPageSelectedUID") && has_column("BidPages", "UID") &&
                    has_column("BidPages", "BidUID"))
                {
                    if (!execute("UPDATE [BidSettings] SET [BidPageSelectedUID] = NULL WHERE [BidPageSelectedUID] IN "
                                 "(SELECT [UID] FROM [BidPages] WHERE [BidUID] = ?)"))
                        return false;
                }
                // Annotations that start or end at a takeoff of the bid.
                for (const char *table : {"BidDimensions", "BidALines", "BidArrows"})
                    if (!delete_takeoff_links(table))
                        return false;
                // Page scoped tables: through the bid's pages AND by their own BidUID. The list is the one of Delete Page
                // (ost_domain::PAGE_DELETE_CHILD_TABLES, constants.py:83-110), shared so that a page table is added in ONE place and
                // both cascades cover it (the catalog test DelPageCatalog pins both).
                for (std::size_t i = 0; i < ost_domain::PAGE_DELETE_CHILD_TABLE_COUNT; ++i)
                {
                    const char *table = ost_domain::PAGE_DELETE_CHILD_TABLES[i];
                    if (!delete_through_pages(table) || !delete_by_bid(table))
                        return false;
                }
                for (const char *table : {"BidLaborActivity", "BidTakeoffTotals", "BidLaborCostCodeTotals", "BidTypicalGroupTotals",
                                          "AffectDPCTypGroupViews", "BidTypGroupViews", "Boost", "DPCCalcFilter", "BidDPCSubscribers",
                                          "BidNotes", "BidTransactionsHistory", "STSTransactionHistory", "BidConditionUser",
                                          "BidPlanRooms"})
                {
                    if (!delete_by_bid(table))
                        return false;
                }
                // Rows that hang off a labor code, a time card state, an employee, an area or a typical area of the bid.
                if (!delete_through_parent("BidPercents", "BidLaborCostCodeUID", "BidLaborCostCodes") ||
                    !delete_through_parent("BidPercents", "BidTimeCardStateUID", "BidTimeCardStates"))
                    return false;
                if (!delete_through_parent("BidTimeCards", "BidEmployeeUID", "BidEmployees") ||
                    !delete_through_parent("BidTimeCards", "BidAreaUID", "BidAreas") ||
                    !delete_through_parent("BidTimeCards", "BidTypicalAreaUID", "BidTypAreas") ||
                    !delete_through_parent("BidTimeCards", "BidLaborCostCodeUID", "BidLaborCostCodes") ||
                    !delete_through_parent("BidTimeCards", "BidTimeCardStateUID", "BidTimeCardStates"))
                    return false;
                if (!delete_through_parent("BidTypAreaCounts", "BidAreaUID", "BidAreas") ||
                    !delete_through_parent("BidPageSettings", "BidAreaUID", "BidAreas") ||
                    !delete_through_parent("BidTypAreaCounts", "BidTypAreaUID", "BidTypAreas") ||
                    !delete_through_parent("BidPageSettings", "BidTypAreaUID", "BidTypAreas"))
                    return false;
                if (has_column("ConditionSetStyles", "ConditionStyleUID") && has_column("BidConditions", "UID") &&
                    has_column("BidConditions", "BidUID"))
                {
                    if (!execute("DELETE FROM [ConditionSetStyles] WHERE [ConditionStyleUID] IN "
                                 "(SELECT [UID] FROM [BidConditions] WHERE [BidUID] = ?)"))
                        return false;
                }
                // The takeoffs: by BidUID (also the ones without a page); a BidTakeoffs without BidUID goes through the pages.
                if (_db.table_exists("BidTakeoffs"))
                {
                    if (_db.column_exists("BidTakeoffs", "BidUID"))
                    {
                        if (!execute("DELETE FROM [BidTakeoffs] WHERE [BidUID] = ?"))
                            return false;
                    }
                    else if (!delete_through_pages("BidTakeoffs"))
                        return false;
                }
                for (const char *table : {"BidHotLinks", "BidNamedViews", "BidSettings", "BidLaborCostCodes", "BidTimeCardStates",
                                          "BidEmployees", "BidConditions", "BidZones", "BidConditionFolders", "BidLayers", "BidAreas",
                                          "BidTypAreas", "BidPages", "BidPageFolders"})
                {
                    if (!delete_by_bid(table))
                        return false;
                }
                return execute("DELETE FROM [Bids] WHERE [UID] = ?");
            }

        private:
            bool has_column(const char *table, const char *column)
            {
                return _db.table_exists(table) && _db.column_exists(table, column);
            }

            bool execute(const std::string &sql)
            {
                ost_db::Statement stmt = _db.prepare(sql);
                if (!stmt.valid())
                    return false;
                stmt.bind_int64(1, _bid);
                return stmt.run();
            }

            // _del_bid: DELETE FROM table WHERE BidUID = bid
            bool delete_by_bid(const char *table)
            {
                if (!has_column(table, "BidUID"))
                    return true;
                return execute(std::string("DELETE FROM [") + table + "] WHERE [BidUID] = ?");
            }

            // _del_page: DELETE FROM table WHERE BidPageUID IN (pages of the bid)
            bool delete_through_pages(const char *table)
            {
                if (!has_column(table, "BidPageUID") || !has_column("BidPages", "UID") || !has_column("BidPages", "BidUID"))
                    return true;
                return execute(std::string("DELETE FROM [") + table +
                               "] WHERE [BidPageUID] IN (SELECT [UID] FROM [BidPages] WHERE [BidUID] = ?)");
            }

            // _del_takeoff_link: DELETE FROM table WHERE BidTakeoffFromUID / BidTakeoffToUID IN (takeoffs of the bid)
            bool delete_takeoff_links(const char *table)
            {
                if (!_db.table_exists(table) || !has_column("BidTakeoffs", "UID") || !has_column("BidTakeoffs", "BidUID"))
                    return true;
                for (const char *column : {"BidTakeoffFromUID", "BidTakeoffToUID"})
                {
                    if (!_db.column_exists(table, column))
                        continue;
                    if (!execute(std::string("DELETE FROM [") + table + "] WHERE [" + column +
                                 "] IN (SELECT [UID] FROM [BidTakeoffs] WHERE [BidUID] = ?)"))
                        return false;
                }
                return true;
            }

            // _del_child_by_bid_parent: DELETE FROM child WHERE fk IN (parents of the bid)
            bool delete_through_parent(const char *child, const char *foreign_key, const char *parent)
            {
                if (!has_column(child, foreign_key) || !has_column(parent, "UID") || !has_column(parent, "BidUID"))
                    return true;
                return execute(std::string("DELETE FROM [") + child + "] WHERE [" + foreign_key + "] IN (SELECT [UID] FROM [" +
                               parent + "] WHERE [BidUID] = ?)");
            }

            ost_db::OstDatabase &_db;
            long long _bid;
        };
    } // namespace

    bool permanently_delete_project(const std::string &db_path, const std::string &bid_uid)
    {
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;

        const std::string deleted_project_uid = resolve_deleted_bids_project_uid(db);
        if (deleted_project_uid.empty() ||
            !bid_is_in_deleted_bids(db, bid_uid, deleted_project_uid))
        {
            db.rollback();
            return false;
        }

        // Match the desktop MDB delete_bids cascade exactly (BidCascade): the tables that carry no BidUID are cleaned through
        // the bid's pages, takeoffs, areas, labor codes and so on, and every page scoped table also by its own BidUID.
        if (!BidCascade(db, bid).run())
        {
            db.rollback();
            return false;
        }

        return db.commit();
    }

}
