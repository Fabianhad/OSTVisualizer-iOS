// Internal header shared by ProjectStore component files.
// Not part of the public API — only include from ProjectStore*.cpp files.
#pragma once
#include "ProjectStore.hpp"
#include "../OstDatabase.hpp"
#include "../OstSchema.hpp"
#include "../../Domain/PositionParser.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <set>
#include <unordered_map>

namespace ost_store
{

    static inline double safe_double(const std::string &s, double def = 0.0)
    {
        if (s.empty())
            return def;
        try
        {
            return std::stod(s);
        }
        catch (...)
        {
            return def;
        }
    }

    static inline int safe_int(const std::string &s, int def = 0)
    {
        if (s.empty())
            return def;
        try
        {
            return std::stoi(s);
        }
        catch (...)
        {
            return def;
        }
    }

    static inline std::string get_val(const ost_import::Row &row, const char *key,
                                      const std::string &def = "")
    {
        auto it = row.find(key);
        return (it != row.end()) ? it->second : def;
    }

    std::vector<double> parse_position(const std::string &pos_str);

    std::pair<double, bool> extract_z_value(const std::string &name);

    // The import used to build "INSERT OR REPLACE" statements here (build_insert/bind_row): a colliding primary key
    // silently overwrote another project's row (IMP-06). Rows are now written by import_insert_raw_row
    // (ProjectStore_ImportInternal.hpp), a plain INSERT with typed values.

    // Windows serialization.encode_position(position, preserve_indices=<every index>), the form every takeoff write
    // path uses (full float repr joined by ';', trailing newline): no quantization of dragged positions (DBP-07).
    static inline std::string serialize_position(const std::vector<double> &position)
    {
        return ost_domain::encode_position(position);
    }

    static inline std::string get_pref(ost_db::OstDatabase &db, const std::string &key,
                                       const std::string &def = "")
    {
        ost_db::Statement stmt = db.prepare(
            "SELECT Value FROM _AppPreferences WHERE Key = ?");
        if (!stmt.valid())
            return def;
        stmt.bind_text(1, key);
        if (stmt.step())
            return stmt.column_text(0);
        return def;
    }

    // false when the preference could not be written (statement failed to prepare or run).
    static inline bool set_pref(ost_db::OstDatabase &db, const std::string &key,
                                const std::string &value)
    {
        ost_db::Statement stmt = db.prepare(
            "INSERT OR REPLACE INTO _AppPreferences (Key, Value) VALUES (?, ?)");
        if (!stmt.valid())
            return false;
        stmt.bind_text(1, key);
        stmt.bind_text(2, value);
        return stmt.run();
    }

}
