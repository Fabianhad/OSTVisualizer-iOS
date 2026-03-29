// Internal header shared by ProjectStore component files.
// Not part of the public API — only include from ProjectStore*.cpp files.
#pragma once
#include "ProjectStore.hpp"
#include "../OstDatabase.hpp"
#include "../OstSchema.hpp"
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

    static inline std::string build_insert(const std::string &table,
                                           const ost_import::Row &row)
    {
        if (row.empty())
            return "";
        std::string cols, placeholders;
        bool first = true;
        for (const auto &pair : row)
        {
            if (!first)
            {
                cols += ", ";
                placeholders += ", ";
            }
            cols += "[" + pair.first + "]";
            placeholders += "?";
            first = false;
        }
        return "INSERT OR REPLACE INTO [" + table + "] (" + cols + ") VALUES (" + placeholders + ")";
    }

    static inline void bind_row(ost_db::Statement &stmt, const ost_import::Row &row)
    {
        int idx = 1;
        for (const auto &pair : row)
        {
            const std::string &val = pair.second;
            if (val.empty() || val == "NULL")
                stmt.bind_null(idx);
            else
                stmt.bind_text(idx, val);
            idx++;
        }
    }

    static inline std::string serialize_position(const std::vector<double> &position)
    {
        std::string s;
        for (size_t i = 0; i < position.size(); i++)
        {
            if (i > 0)
                s += ';';
            char buf[64];
            snprintf(buf, sizeof(buf), "%.10g", position[i]);
            s += buf;
        }
        return s;
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

    static inline void set_pref(ost_db::OstDatabase &db, const std::string &key,
                                const std::string &value)
    {
        ost_db::Statement stmt = db.prepare(
            "INSERT OR REPLACE INTO _AppPreferences (Key, Value) VALUES (?, ?)");
        if (stmt.valid())
        {
            stmt.bind_text(1, key);
            stmt.bind_text(2, value);
            stmt.step();
        }
    }

}
