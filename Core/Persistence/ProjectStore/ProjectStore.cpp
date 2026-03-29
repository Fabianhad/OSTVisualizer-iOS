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
            preferred_available = !existing.step();

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
            insert.step();
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
            move_bids.step();

            ost_db::Statement delete_folder = db.prepare(
                "DELETE FROM BidProjects WHERE UID = ?");
            if (!delete_folder.valid())
                return "";
            delete_folder.bind_text(1, uid);
            delete_folder.step();
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

    static bool is_xml_newline_entity_at(const std::string &text,
                                         size_t pos,
                                         size_t &entity_len)
    {
        static const char *patterns[] = {
            "&#xa;",
            "&#x0a;",
            "&#10;",
        };

        for (const char *pattern : patterns)
        {
            size_t len = std::strlen(pattern);
            if (pos + len > text.size())
                continue;

            bool match = true;
            for (size_t i = 0; i < len; ++i)
            {
                unsigned char a = static_cast<unsigned char>(text[pos + i]);
                unsigned char b = static_cast<unsigned char>(pattern[i]);
                if (std::tolower(a) != std::tolower(b))
                {
                    match = false;
                    break;
                }
            }

            if (match)
            {
                entity_len = len;
                return true;
            }
        }

        return false;
    }

    std::vector<double> parse_position(const std::string &pos_str)
    {
        if (pos_str.empty())
            return {};

        std::string clean;
        clean.reserve(pos_str.size());
        size_t i = 0;
        while (i < pos_str.size())
        {
            size_t entity_len = 0;
            if (is_xml_newline_entity_at(pos_str, i, entity_len))
            {
                i += entity_len;
                continue;
            }
            clean.push_back(pos_str[i]);
            i++;
        }

        std::vector<double> result;
        std::istringstream ss(clean);
        std::string token;
        while (std::getline(ss, token, ';'))
        {
            size_t start = token.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
                continue;
            size_t end = token.find_last_not_of(" \t\r\n");
            token = token.substr(start, end - start + 1);
            if (token.empty())
                continue;
            try
            {
                result.push_back(std::stod(token));
            }
            catch (...)
            {
                return {};
            }
        }

        return result;
    }

    static double parse_fractional_inches(const std::string &text)
    {
        std::string s = text;
        size_t start = s.find_first_not_of(" \t");
        if (start != std::string::npos)
            s = s.substr(start);

        bool neg = (!s.empty() && s[0] == '-');
        if (neg)
            s = s.substr(1);
        start = s.find_first_not_of(" \t");
        if (start != std::string::npos)
            s = s.substr(start);

        double val = 0.0;
        size_t space = s.find(' ');
        if (space != std::string::npos && s.find('/') != std::string::npos)
        {
            val = std::stod(s.substr(0, space));
            std::string frac_s = s.substr(space + 1);
            size_t slash = frac_s.find('/');
            if (slash != std::string::npos)
            {
                double n = std::stod(frac_s.substr(0, slash));
                double d = std::stod(frac_s.substr(slash + 1));
                if (d != 0.0)
                    val += n / d;
            }
        }
        else if (s.find('/') != std::string::npos)
        {
            size_t slash = s.find('/');
            double n = std::stod(s.substr(0, slash));
            double d = std::stod(s.substr(slash + 1));
            if (d != 0.0)
                val = n / d;
        }
        else
        {
            try
            {
                val = std::stod(s);
            }
            catch (...)
            {
            }
        }

        return neg ? -val : val;
    }

    static void skip_spaces(const std::string &s, size_t &i)
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
            i++;
    }

    static bool parse_number_at(const std::string &s, size_t &i, double &out)
    {
        skip_spaces(s, i);
        if (i >= s.size())
            return false;
        size_t start = i;
        if (s[i] == '-')
            i++;
        skip_spaces(s, i);
        if (i >= s.size() || (!std::isdigit(s[i]) && s[i] != '.'))
        {
            i = start;
            return false;
        }
        while (i < s.size() && (std::isdigit(s[i]) || s[i] == '.'))
            i++;
        try
        {
            out = std::stod(s.substr(start, i - start));
            return true;
        }
        catch (...)
        {
            i = start;
            return false;
        }
    }

    std::pair<double, bool> extract_z_value(const std::string &name)
    {
        if (name.empty())
            return {0.0, false};

        size_t at_pos = name.find('@');
        if (at_pos == std::string::npos)
            return {0.0, false};

        size_t i = at_pos + 1;
        skip_spaces(name, i);

        bool is_top = false;
        if (i < name.size() && (name[i] == 't' || name[i] == 'T'))
        {
            is_top = true;
            i++;
        }
        else if (i < name.size() && (name[i] == 'b' || name[i] == 'B'))
        {
            is_top = false;
            i++;
        }
        skip_spaces(name, i);

        double num1 = 0;
        if (!parse_number_at(name, i, num1))
            return {0.0, false};

        skip_spaces(name, i);
        if (i >= name.size())
            return {0.0, false};

        char unit = name[i];

        if (unit == '\'' || unit == (char)0xE2)
        {
            i++;
            if (unit == (char)0xE2 && i + 1 < name.size())
                i += 2;
            skip_spaces(name, i);
            if (i < name.size() && name[i] == '-')
            {
                i++;
                skip_spaces(name, i);
            }

            double inches = 0;
            size_t inch_start = i;
            if (parse_number_at(name, i, inches))
            {
                skip_spaces(name, i);
                if (i < name.size() && name[i] == '/')
                {
                    i = inch_start;
                    inches = parse_fractional_inches(name.substr(inch_start));
                    while (i < name.size() && name[i] != '"' && name[i] != '\'' && name[i] != ' ')
                        i++;
                }
                else if (i < name.size() && name[i] == ' ')
                {
                    size_t sp = i;
                    skip_spaces(name, i);
                    if (i < name.size() && std::isdigit(name[i]))
                    {
                        i = inch_start;
                        inches = parse_fractional_inches(name.substr(inch_start));
                    }
                    else
                    {
                        i = sp;
                    }
                }
            }
            double total = std::abs(num1) * 12.0 + inches;
            if (std::signbit(num1))
                total = -total;
            return {total, is_top};
        }

        if (unit == '"')
        {
            return {num1, is_top};
        }

        std::string rest = name.substr(i);
        std::string rest_low = rest;
        std::transform(rest_low.begin(), rest_low.end(), rest_low.begin(), ::tolower);

        if (rest_low.substr(0, 2) == "cm")
        {
            return {num1 / 2.54, is_top};
        }
        if (rest_low[0] == 'm' && (rest_low.size() < 2 || rest_low[1] == ' ' || rest_low[1] == '\0' || rest_low.substr(0, 6) == "meters" || rest_low.substr(0, 5) == "meter"))
        {
            size_t mi = 1;
            while (mi < rest_low.size() && std::isalpha(rest_low[mi]))
                mi++;
            skip_spaces(rest_low, mi);

            if (mi < rest_low.size() && (rest_low.substr(mi, 3) == "and" || rest_low[mi] == '+'))
            {
                while (mi < rest_low.size() && !std::isdigit(rest_low[mi]) && rest_low[mi] != '-')
                    mi++;
                double cm_val = 0;
                if (parse_number_at(rest_low, mi, cm_val))
                {
                    skip_spaces(rest_low, mi);
                    if (mi < rest_low.size() && rest_low.substr(mi, 2) == "cm")
                        return {(num1 * 100.0 + cm_val) / 2.54, is_top};
                }
            }
            return {num1 * 100.0 / 2.54, is_top};
        }

        return {0.0, false};
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
        stmt.step();

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
            stmt.step();
            return true;
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
        stmt.step();

        return db.commit();
    }

}
