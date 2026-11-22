// Layer: Import (pure C++)
// See OstImportTransform.hpp.
#include "OstImportTransform.hpp"
#include "TextRules.hpp"
#include <limits>
#include <set>

namespace ost_import
{
    namespace
    {
        // Windows ost_importer._GLOBAL_ZERO_UID_FIELDS: references into the SOURCE database that mean nothing in the
        // target (the bid lands unfiled; the writer sets BidProjectUID itself).
        bool is_global_zero_uid_field(const std::string &field)
        {
            static const std::set<std::string> fields = {
                "BidProjectUID", "ParentBidUID", "OrigBidProjectUID", "OrigParentBidUID",
                "SourceBidUID", "OCRUID", "CoverSheetSelItemUID"};
            return fields.count(field) > 0;
        }

        // Windows ost_importer._EMPLOYEE_UID_FIELDS.
        bool is_employee_uid_field(const std::string &field)
        {
            static const std::set<std::string> fields = {
                "EstimatorUID", "PrManagerUID", "JobSiteManagerUID", "EmployeeUID"};
            return fields.count(field) > 0;
        }

        // Windows ost_importer._BID_INTERNAL_UID_FIELDS (BidPageSelectedUID is handled before this is consulted).
        bool is_bid_internal_uid_field(const std::string &field)
        {
            static const std::set<std::string> fields = {
                "UID", "BidUID", "BidPageUID", "BidConditionUID", "BidLayerUID", "BidZoneUID", "BidAreaUID",
                "BidTypAreaUID", "ParentUID", "ParentCommentUID", "MasterPageUID", "MasterAreaUID",
                "TranslateAreaUID", "BidConditionFolderUID", "BidPageFolderUID", "BidTakeoffFromUID",
                "BidTakeoffToUID", "BidPageViewUID", "TypGroupTakeoffUID", "TypPageTakeoffUID",
                "TypGroupMarkerUID", "TypGroupUID", "BidPageSelectedUID"};
            return fields.count(field) > 0;
        }

        bool ends_with_uid(const std::string &field)
        {
            return field.size() >= 3 && field.compare(field.size() - 3, 3, "UID") == 0;
        }

        const std::string *lookup(const UidMap &map, const std::string &key)
        {
            auto it = map.find(key);
            return it == map.end() ? nullptr : &it->second;
        }

        // Windows page_area_selection._integer_or_zero: int(value or 0), ValueError/TypeError -> 0.
        long long integer_or_zero(const std::string &value)
        {
            if (value.empty())
                return 0;
            long long parsed = 0;
            switch (text_rules::py_parse_int(value, parsed))
            {
            case text_rules::NumberParse::Ok:
                return parsed;
            case text_rules::NumberParse::Overflow:
            {
                // Python has unbounded integers; beyond 64 bits only the sign still orders correctly.
                const std::string stripped = text_rules::py_strip(value);
                return (!stripped.empty() && stripped[0] == '-') ? std::numeric_limits<long long>::min()
                                                                  : std::numeric_limits<long long>::max();
            }
            default:
                return 0;
            }
        }

        // Windows page_area_selection._page_group_key.
        std::string page_group_key(const Row &row)
        {
            auto it = row.find("BidPageUID");
            const std::string text = (it == row.end()) ? std::string() : it->second;
            return (text.empty() || text == "0" || text == "NULL") ? std::string() : text;
        }

        std::string get_text(const Row &row, const char *key)
        {
            auto it = row.find(key);
            return it == row.end() ? std::string() : it->second;
        }
    }

    UidMap build_uid_map(const OstRawData &raw_data, long long max_uid)
    {
        UidMap uid_map;
        long long next_uid = max_uid + 1;

        auto assign = [&](const std::string &uid_value)
        {
            if (!uid_value.empty() && uid_value != "0" && uid_map.find(uid_value) == uid_map.end())
            {
                uid_map[uid_value] = std::to_string(next_uid);
                ++next_uid;
            }
        };

        assign(get_text(raw_data.bid_row, "UID"));
        for (const auto &table : raw_data.bid_tables)
            for (const auto &row : table.second)
                assign(get_text(row, "UID"));
        for (const auto &table : raw_data.page_tables)
            for (const auto &row : table.second)
                assign(get_text(row, "UID"));
        return uid_map;
    }

    UidMap build_page_uid_map(const OstRawData &raw_data, const UidMap &uid_map)
    {
        UidMap page_uid_map;
        auto pages = raw_data.bid_tables.find("BidPages");
        if (pages == raw_data.bid_tables.end())
            return page_uid_map;
        for (const auto &page_row : pages->second)
        {
            const std::string source_uid = text_rules::py_strip(get_text(page_row, "UID"));
            if (source_uid.empty() || source_uid == "0")
                continue;
            const std::string *mapped = lookup(uid_map, source_uid);
            if (mapped && !mapped->empty())
                page_uid_map[source_uid] = *mapped;
        }
        return page_uid_map;
    }

    std::string remap_uid_value(const std::string &field, const std::string &value, const RemapMaps &maps)
    {
        if (field == "BidPageSelectedUID")
        {
            const std::string source_uid = text_rules::py_strip(value);
            if (source_uid.empty() || source_uid == "0")
                return "NULL";
            const std::string *mapped = lookup(maps.page, source_uid);
            return mapped ? *mapped : std::string("NULL");
        }
        if (value.empty())
            return value;
        if (value == "0")
            return ends_with_uid(field) ? std::string("NULL") : value;
        if (field == "CdnTypeUID")
        {
            const std::string *mapped = lookup(maps.cdn_type, value);
            return mapped ? *mapped : value;
        }
        if (field == "JobStatusUID")
        {
            const std::string *mapped = lookup(maps.job_status, value);
            return mapped ? *mapped : std::string("NULL");
        }
        if (is_employee_uid_field(field))
        {
            const std::string *mapped = lookup(maps.employee, value);
            return mapped ? *mapped : std::string("NULL");
        }
        if (field == "PayClassUID")
        {
            const std::string *mapped = lookup(maps.pay_class, value);
            return mapped ? *mapped : std::string("NULL");
        }
        if (is_global_zero_uid_field(field))
            return "NULL";
        if (is_bid_internal_uid_field(field))
        {
            const std::string *mapped = lookup(maps.uid, value);
            return mapped ? *mapped : value;
        }
        return value;
    }

    Row remap_row(const Row &row, const RemapMaps &maps)
    {
        Row remapped;
        for (const auto &field : row)
            remapped[field.first] = remap_uid_value(field.first, field.second, maps);
        return remapped;
    }

    OstRawData remap_data(const OstRawData &raw_data, const RemapMaps &maps)
    {
        OstRawData result;
        result.bid_row = remap_row(raw_data.bid_row, maps);
        for (const auto &table : raw_data.bid_tables)
        {
            RowList rows;
            rows.reserve(table.second.size());
            for (const auto &row : table.second)
                rows.push_back(remap_row(row, maps));
            result.bid_tables[table.first] = std::move(rows);
        }
        for (const auto &table : raw_data.page_tables)
        {
            RowList rows;
            rows.reserve(table.second.size());
            for (const auto &row : table.second)
                rows.push_back(remap_row(row, maps));
            result.page_tables[table.first] = std::move(rows);
        }
        // Assigning an existing key keeps its place; a new key goes last (dict semantics of the Windows code).
        auto settings = result.page_tables.find("BidPageSettings");
        const RowList current = (settings == result.page_tables.end()) ? RowList() : settings->second;
        result.page_tables["BidPageSettings"] = canonicalize_page_area_settings(current);
        return result;
    }

    OstRawData transform(const OstRawData &raw_data,
                         long long max_uid,
                         const UidMap &cdn_uid_map,
                         const UidMap &job_status_uid_map,
                         const UidMap &employee_uid_map,
                         const UidMap &pay_class_uid_map)
    {
        RemapMaps maps;
        maps.uid = build_uid_map(raw_data, max_uid);
        maps.page = build_page_uid_map(raw_data, maps.uid);
        maps.cdn_type = cdn_uid_map;
        maps.job_status = job_status_uid_map;
        maps.employee = employee_uid_map;
        maps.pay_class = pay_class_uid_map;
        return remap_data(raw_data, maps);
    }

    RowList canonicalize_page_area_settings(const RowList &rows)
    {
        RowList inactive_rows;
        // Insertion-ordered: a group keeps the position of its first selected row even when a later row replaces it.
        std::vector<std::string> group_order;
        std::unordered_map<std::string, Row> selected_by_page;

        for (const auto &row : rows)
        {
            const long long selected_value = integer_or_zero(get_text(row, "BidAreaSelected"));
            if (selected_value <= 0)
            {
                inactive_rows.push_back(row);
                continue;
            }
            const std::string page_uid = page_group_key(row);
            auto current = selected_by_page.find(page_uid);
            if (current == selected_by_page.end())
            {
                group_order.push_back(page_uid);
                selected_by_page[page_uid] = row;
                continue;
            }
            const long long current_selected = integer_or_zero(get_text(current->second, "BidAreaSelected"));
            const long long current_uid = integer_or_zero(get_text(current->second, "UID"));
            const long long row_uid = integer_or_zero(get_text(row, "UID"));
            const bool row_wins = (selected_value > current_selected) ||
                                  (selected_value == current_selected && row_uid >= current_uid);
            if (row_wins)
                current->second = row;
        }

        RowList result = std::move(inactive_rows);
        for (const auto &key : group_order)
            result.push_back(selected_by_page[key]);
        return result;
    }
}
