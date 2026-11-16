// Layer: Persistence
// Depends on: Domain (via ProjectData.hpp, DatabaseHierarchy.hpp), OstDatabase, OstSchema
// Forbidden: UIKit, Foundation, Renderer, ImportService
// ProjectStore is the ONLY class that executes SQL. All DB <-> domain mapping happens here.
// All CRUD operates directly on the shared schema tables (BidTakeoffs, BidLayers, BidProjects, etc.).
//
// Component files in Components/:
//   ProjectStore+Load.cpp       — load_project
//   ProjectStore+Import.cpp     — import_project, is_bid_imported
//   ProjectStore+Folders.cpp    — load_hierarchy, folder CRUD
//   ProjectStore+Takeoffs.cpp   — takeoff CRUD, layer visibility
//   ProjectStore+Conditions.cpp — condition CRUD, render config
#include "ProjectStore.hpp"
#include "../OstDatabase.hpp"
#include "../OstSchema.hpp"
#include "../../Domain/PositionParser.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <ctime>
#include <sstream>

namespace ost_store
{
    static const char *DELETED_BIDS_PROJECT_NAME = "Deleted Bids";
    static const char *PREFERRED_DELETED_BIDS_PROJECT_UID = "1";

    // Returns the UID of the canonical 'Deleted Bids' project, creating it (and merging duplicates) when needed.
    // Returns "" when any statement failed: the caller's open transaction must then be rolled back.
    static std::string resolve_deleted_bids_project_uid(ost_db::OstDatabase &db)
    {
        std::vector<std::string> deleted_uids;
        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID FROM BidProjects WHERE Name = ? ORDER BY UID");
            if (!stmt.valid())
                return "";
            stmt.bind_text(1, DELETED_BIDS_PROJECT_NAME);
            while (stmt.step())
                deleted_uids.push_back(stmt.column_text(0));
            if (stmt.failed())
                return "";
        }

        std::string canonical_uid;
        for (const std::string &uid : deleted_uids)
        {
            if (uid == PREFERRED_DELETED_BIDS_PROJECT_UID)
            {
                canonical_uid = uid;
                break;
            }
        }
        if (canonical_uid.empty() && !deleted_uids.empty())
            canonical_uid = deleted_uids.front();

        if (canonical_uid.empty())
        {
            bool preferred_available = true;
            ost_db::Statement existing = db.prepare(
                "SELECT UID FROM BidProjects WHERE UID = ?");
            if (!existing.valid())
                return "";
            existing.bind_text(1, PREFERRED_DELETED_BIDS_PROJECT_UID);
            const ost_db::StepResult existing_result = existing.step_result();
            if (existing_result == ost_db::StepResult::Error)
                return "";
            preferred_available = (existing_result == ost_db::StepResult::Done);

            if (preferred_available)
            {
                canonical_uid = PREFERRED_DELETED_BIDS_PROJECT_UID;
            }
            else
            {
                ost_db::Statement max_stmt = db.prepare(
                    "SELECT COALESCE(MAX(UID), 0) + 1 FROM BidProjects");
                if (!max_stmt.valid() || !max_stmt.step())
                    return "";
                canonical_uid = std::to_string(max_stmt.column_int(0));
            }

            ost_db::Statement insert = db.prepare(
                "INSERT INTO BidProjects (UID, Name) VALUES (?, ?)");
            if (!insert.valid())
                return "";
            insert.bind_text(1, canonical_uid);
            insert.bind_text(2, DELETED_BIDS_PROJECT_NAME);
            if (!insert.run())
                return "";
            return canonical_uid;
        }

        for (const std::string &uid : deleted_uids)
        {
            if (uid == canonical_uid)
                continue;

            ost_db::Statement move_bids = db.prepare(
                "UPDATE Bids SET BidProjectUID = ? WHERE BidProjectUID = ?");
            if (!move_bids.valid())
                return "";
            move_bids.bind_text(1, canonical_uid);
            move_bids.bind_text(2, uid);
            if (!move_bids.run())
                return "";

            ost_db::Statement delete_folder = db.prepare(
                "DELETE FROM BidProjects WHERE UID = ?");
            if (!delete_folder.valid())
                return "";
            delete_folder.bind_text(1, uid);
            if (!delete_folder.run())
                return "";
        }

        return canonical_uid;
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
    // "Wall @B 1/0\""); the Windows MDB reader (bid_data_reader.py) catches it, logs a warning and stores (0.0, False).
    std::pair<double, bool> extract_z_value(const std::string &name)
    {
        double z_inches = 0.0;
        bool is_top = false;
        if (!ost_domain::extract_z_value_from_name(name, z_inches, is_top))
            return {0.0, false};
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

        ost_db::Statement current = db.prepare(
            "SELECT BidProjectUID FROM Bids WHERE UID = ?");
        if (!current.valid())
        {
            db.rollback();
            return false;
        }
        current.bind_text(1, bid_uid);
        if (!current.step())
        {
            db.rollback();
            return false;
        }
        const std::string current_project_uid = current.column_text(0);
        if (current_project_uid == deleted_project_uid)
            return db.commit();

        ost_db::Statement stmt = db.prepare(
            "UPDATE Bids SET BidProjectUID = ?, OrigBidProjectUID = ? WHERE UID = ?");
        if (!stmt.valid())
        {
            db.rollback();
            return false;
        }
        stmt.bind_text(1, deleted_project_uid);
        if (current_project_uid.empty())
            stmt.bind_null(2);
        else
            stmt.bind_text(2, current_project_uid);
        stmt.bind_text(3, bid_uid);
        if (!stmt.run())
        {
            db.rollback();
            return false;
        }

        return db.commit();
    }

    bool permanently_delete_project(const std::string &db_path, const std::string &bid_uid)
    {
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

        auto exec_bid = [&](const std::string &sql) -> bool
        {
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid())
                return false;
            stmt.bind_text(1, bid_uid);
            return stmt.run();
        };

        // Match the desktop MDB delete_bids cascade, including the tables that
        // do not carry BidUID directly and must be cleaned via page/takeoff/area
        // or condition subqueries to avoid leaving orphans behind.
        if (!exec_bid(
                "DELETE FROM BidPercents WHERE BidTakeoffUID IN "
                "(SELECT UID FROM BidTakeoffs WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidDimensions WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidALines WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidArrows WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidHighlights WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidTexts WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidCallOuts WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidAnnotationRects WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidAnnotationOvals WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidAnnotationPolygons WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidAnnotationClouds WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidAnnoInk WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidLegends WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidPageSettings WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidAreaTranslations WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidMarkedPages WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidComments WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid("DELETE FROM BidLaborActivity WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidTakeoffTotals WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidLaborCostCodeTotals WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidTypicalGroupTotals WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM AffectDPCTypGroupViews WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidTypGroupViews WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM Boost WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM DPCCalcFilter WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidDPCSubscribers WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidNotes WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidTransactionsHistory WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM STSTransactionHistory WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidConditionUser WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidPlanRooms WHERE BidUID = ?") ||
            !exec_bid(
                "DELETE FROM BidTimeCards WHERE BidEmployeeUID IN "
                "(SELECT UID FROM BidEmployees WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidTypAreaCounts WHERE BidAreaUID IN "
                "(SELECT UID FROM BidAreas WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM ConditionSetStyles WHERE ConditionStyleUID IN "
                "(SELECT UID FROM BidConditions WHERE BidUID = ?)") ||
            !exec_bid(
                "DELETE FROM BidTakeoffs WHERE BidPageUID IN "
                "(SELECT UID FROM BidPages WHERE BidUID = ?)") ||
            !exec_bid("DELETE FROM BidHotLinks WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidNamedViews WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidSettings WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidLaborCostCodes WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidTimeCardStates WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidEmployees WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidConditions WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidZones WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidConditionFolders WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidLayers WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidAreas WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidTypAreas WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidPages WHERE BidUID = ?") ||
            !exec_bid("DELETE FROM BidPageFolders WHERE BidUID = ?"))
        {
            db.rollback();
            return false;
        }

        ost_db::Statement stmt = db.prepare("DELETE FROM Bids WHERE UID = ?");
        if (!stmt.valid())
        {
            db.rollback();
            return false;
        }
        stmt.bind_text(1, bid_uid);
        if (!stmt.run())
        {
            db.rollback();
            return false;
        }

        return db.commit();
    }

}
