// Internal header shared by ProjectStore component files.
// Not part of the public API — only include from ProjectStore*.cpp files.
#pragma once
#include "ProjectStore.hpp"
#include "ProjectStore_Integrity.hpp"
#include "../OstDatabase.hpp"
#include "../OstSchema.hpp"
#include "../../Domain/PositionParser.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <sstream>
#include <set>
#include <unordered_map>

namespace ost_store
{

    // Windows _new_ost_guid: "{" + str(uuid.uuid4()).upper() + "}", a random (version 4, variant 1) UUID in
    // 8-4-4-4-12 upper-case hex.
    static inline std::string new_ost_guid()
    {
        std::array<std::uint32_t, 4> words{};
        try
        {
            std::random_device device;
            for (std::uint32_t &word : words)
                word = static_cast<std::uint32_t>(device());
        }
        catch (...)
        {
            // no entropy source: fall back to a clock-seeded generator rather than failing the write
            std::mt19937 generator(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
            for (std::uint32_t &word : words)
                word = static_cast<std::uint32_t>(generator());
        }
        unsigned char bytes[16];
        for (std::size_t i = 0; i < 4; ++i)
        {
            bytes[i * 4 + 0] = static_cast<unsigned char>(words[i] >> 24);
            bytes[i * 4 + 1] = static_cast<unsigned char>(words[i] >> 16);
            bytes[i * 4 + 2] = static_cast<unsigned char>(words[i] >> 8);
            bytes[i * 4 + 3] = static_cast<unsigned char>(words[i]);
        }
        bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
        bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);
        static const char hex[] = "0123456789ABCDEF";
        std::string guid = "{";
        for (std::size_t i = 0; i < 16; ++i)
        {
            if (i == 4 || i == 6 || i == 8 || i == 10)
                guid += '-';
            guid += hex[bytes[i] >> 4];
            guid += hex[bytes[i] & 0x0F];
        }
        guid += '}';
        return guid;
    }

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

    // The stored text of a preference; false when there is no such row (or the table cannot be read), value untouched.
    static inline bool get_pref_if_present(ost_db::OstDatabase &db, const std::string &key,
                                           std::string &value)
    {
        ost_db::Statement stmt = db.prepare(
            "SELECT Value FROM _AppPreferences WHERE Key = ?");
        if (!stmt.valid())
            return false;
        stmt.bind_text(1, key);
        if (!stmt.step())
            return false;
        value = stmt.column_text(0);
        return true;
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
