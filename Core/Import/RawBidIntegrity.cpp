// Layer: Import (pure C++)
// See RawBidIntegrity.hpp.
#include "RawBidIntegrity.hpp"

#include "../Domain/CoreLog.hpp"
#include "../Persistence/OstSchema.hpp"
#include "ImportDiagnostics.hpp"
#include "TextRules.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ost_import
{
    PythonIntLimitError::PythonIntLimitError(size_t digits)
        : std::runtime_error("Exceeds the limit (4300 digits) for integer string conversion: value has " + std::to_string(digits) +
                             " digits; use sys.set_int_max_str_digits() to increase the limit")
    {
    }

    namespace
    {
        const std::string EMPTY;

        // row.get(key, "")
        const std::string &get_or_empty(const Row &row, const char *key)
        {
            const auto it = row.find(key);
            return it == row.end() ? EMPTY : it->second;
        }

        // reference_validation.is_present_uid
        bool is_present_uid(const std::string &value)
        {
            return !(value.empty() || value == "0" || value == "NULL");
        }

        // `value.isascii() and value.isdecimal() and int(value) > 0` (ASCII digits only, at least one non-zero digit). int() refuses more than
        // 4300 digits: PythonIntLimitError, as the Python code raises. `normalized` = str(int(value)) (no leading zeros).
        bool positive_ascii_decimal(const std::string &text, std::string *normalized = nullptr)
        {
            if (text.empty())
                return false;
            for (const char c : text)
                if (c < '0' || c > '9')
                    return false;
            if (text.size() > 4300)
                throw PythonIntLimitError(text.size());
            size_t first = 0;
            while (first < text.size() && text[first] == '0')
                ++first;
            if (first == text.size())
                return false;
            if (normalized)
                *normalized = text.substr(first);
            return true;
        }

        // _RAW_TABLES: ("Bids",) + BID_SECTIONS + ("BidPages",) + BID_TAIL_SECTIONS + PAGE_SECTIONS + GLOBAL_SECTIONS
        const std::vector<std::string> &raw_tables()
        {
            static const std::vector<std::string> tables = []
            {
                std::vector<std::string> out = {"Bids"};
                for (const char *name : ost_schema::BID_SECTIONS)
                    out.push_back(name);
                out.push_back("BidPages");
                for (const char *name : ost_schema::BID_TAIL_SECTIONS)
                    out.push_back(name);
                for (const char *name : ost_schema::PAGE_SECTIONS)
                    out.push_back(name);
                for (const char *name : ost_schema::GLOBAL_SECTIONS)
                    out.push_back(name);
                return out;
            }();
            return tables;
        }

        const std::set<std::string> &authoritative_uid_tables()
        {
            static const std::set<std::string> tables = {
                "Bids",           "BidAreas",        "BidTypAreas",         "BidConditionFolders", "BidPageFolders",
                "BidLayers",      "BidConditions",   "BidZones",            "BidPages",            "BidAnnotationClouds",
                "BidAnnotationOvals", "BidAnnotationPolygons", "BidAnnotationRects", "BidAnnoInk",   "BidALines",
                "BidDimensions",  "BidArrows",       "BidTexts",            "BidHighlights",       "BidNamedViews",
                "BidHotLinks",    "BidCallOuts",     "BidComments"};
            return tables;
        }

        using RowsByTable = std::unordered_map<std::string, std::vector<const Row *>>;

        // raw_bid_table_rows: the Bid row as table "Bids", then every table of the bid, page and global tables (a table of the same name in
        // several sources is concatenated in that order).
        RowsByTable raw_bid_table_rows(const OstRawData &raw)
        {
            RowsByTable rows;
            if (!raw.bid_row.empty())
                rows["Bids"].push_back(&raw.bid_row);
            const TableMap *sources[] = {&raw.bid_tables, &raw.page_tables, &raw.global_tables};
            for (const TableMap *source : sources)
            {
                for (const auto &entry : *source)
                {
                    std::vector<const Row *> &destination = rows[entry.first];
                    for (const Row &row : entry.second)
                        destination.push_back(&row);
                }
            }
            return rows;
        }

        const std::vector<const Row *> &rows_of(const RowsByTable &rows, const std::string &table)
        {
            static const std::vector<const Row *> none;
            const auto it = rows.find(table);
            return it == rows.end() ? none : it->second;
        }

        // _present_uids: the present values of a column.
        std::unordered_set<std::string> present_uids(const std::vector<const Row *> &rows, const char *column)
        {
            std::unordered_set<std::string> out;
            for (const Row *row : rows)
            {
                const std::string &value = get_or_empty(*row, column);
                if (is_present_uid(value))
                    out.insert(value);
            }
            return out;
        }

        // find_takeoff_parent_cycle_uids: the uids that lie on a cycle of the parent graph (a parent of "" or "0" ends the chain).
        std::set<std::string> find_cycle_uids(const std::unordered_map<std::string, std::string> &parent_uid_by_uid)
        {
            std::unordered_map<std::string, std::string> parent_map;
            for (const auto &entry : parent_uid_by_uid)
                if (!entry.second.empty() && entry.second != "0")
                    parent_map[entry.first] = entry.second;
            std::unordered_set<std::string> completed;
            std::set<std::string> cycle_uids;
            for (const auto &start : parent_map)
            {
                if (completed.count(start.first))
                    continue;
                std::vector<std::string> path;
                std::unordered_map<std::string, size_t> path_index;
                std::string current = start.first;
                while (parent_map.count(current) && !completed.count(current))
                {
                    const auto seen = path_index.find(current);
                    if (seen != path_index.end())
                    {
                        for (size_t i = seen->second; i < path.size(); ++i)
                            cycle_uids.insert(path[i]);
                        break;
                    }
                    path_index[current] = path.size();
                    path.push_back(current);
                    current = parent_map[current];
                }
                for (const std::string &uid : path)
                    completed.insert(uid);
            }
            return cycle_uids;
        }

        IntegrityIssue missing_reference(const std::string &table, const std::string &row_uid, const std::string &column,
                                         const std::string &missing_uid, const std::string &parent_table)
        {
            IntegrityIssue issue;
            issue.kind = IntegrityIssue::Kind::MissingReference;
            issue.table = table;
            issue.row_uid = row_uid;
            issue.column = column;
            issue.value = missing_uid;
            issue.parent_table = parent_table;
            return issue;
        }

        IntegrityIssue malformed_uid(const std::string &table, const std::string &row_uid, const std::string &column, const std::string &value)
        {
            IntegrityIssue issue;
            issue.kind = IntegrityIssue::Kind::MalformedUid;
            issue.table = table;
            issue.row_uid = row_uid;
            issue.column = column;
            issue.value = value;
            return issue;
        }

        IntegrityIssue duplicate_uid(const std::string &table, const std::string &uid, long long count)
        {
            IntegrityIssue issue;
            issue.kind = IntegrityIssue::Kind::DuplicateUid;
            issue.table = table;
            issue.uid = uid;
            issue.count = count;
            return issue;
        }

        IntegrityIssue parent_cycle(const std::string &table, const std::string &uid)
        {
            IntegrityIssue issue;
            issue.kind = IntegrityIssue::Kind::ParentCycle;
            issue.table = table;
            issue.uid = uid;
            return issue;
        }
    }

    std::string IntegrityIssue::format() const
    {
        switch (kind)
        {
        case Kind::MissingReference:
            return table + ".UID=" + (row_uid.empty() ? std::string("<no UID>") : row_uid) + " " + column + "=" + value + " missing " +
                   parent_table + ".UID";
        case Kind::DuplicateUid:
            return table + ".UID=" + uid + " occurs " + std::to_string(count) + " times";
        case Kind::MalformedUid:
            return table + ".UID=" + (row_uid.empty() ? std::string("<no UID>") : row_uid) + " has malformed " + column + "=" +
                   (value.empty() ? std::string("<missing>") : value);
        case Kind::ParentCycle:
            return table + ".UID=" + uid + " participates in a ParentUID cycle";
        case Kind::TakeoffPageMismatch:
            return table + ".UID=" + uid + " has a parent on another Page";
        case Kind::Cardinality:
            return table + " has " + std::to_string(count) + " rows for " + owner_table + ".UID=" + owner_uid + "; expected at most " +
                   std::to_string(maximum);
        }
        return std::string();
    }

    bool operator==(const IntegrityIssue &a, const IntegrityIssue &b)
    {
        return a.kind == b.kind && a.table == b.table && a.row_uid == b.row_uid && a.column == b.column && a.value == b.value &&
               a.parent_table == b.parent_table && a.uid == b.uid && a.count == b.count && a.maximum == b.maximum &&
               a.owner_table == b.owner_table && a.owner_uid == b.owner_uid;
    }

    const std::vector<RawRelationship> &raw_bid_relationships()
    {
        static const std::vector<RawRelationship> relationships = []
        {
            std::set<std::string> raw_table_set(raw_tables().begin(), raw_tables().end());
            // _IMPORT_RECONCILED_OPTIONAL_MASTER_REFERENCES
            const std::set<std::pair<std::string, std::string>> reconciled = {
                {"Bids", "JobStatusUID"}, {"Bids", "EstimatorUID"}, {"Bids", "PrManagerUID"}, {"Bids", "JobSiteManagerUID"}};
            std::vector<RawRelationship> out;
            for (const ost_schema::BidRelationship &relationship : ost_schema::BID_RELATIONSHIPS)
            {
                if (!raw_table_set.count(relationship.child_table) || !raw_table_set.count(relationship.parent_table))
                    continue;
                if (reconciled.count({relationship.child_table, relationship.child_column}))
                    continue;
                out.push_back({relationship.child_table, relationship.child_column, relationship.parent_table, relationship.required});
            }
            return out;
        }();
        return relationships;
    }

    std::vector<IntegrityIssue> validate_raw_bid_integrity(const OstRawData &raw)
    {
        return validate_raw_bid_integrity(raw, raw_bid_relationships());
    }

    std::vector<IntegrityIssue> validate_raw_bid_integrity(const OstRawData &raw, const std::vector<RawRelationship> &relationships)
    {
        const RowsByTable rows_by_table = raw_bid_table_rows(raw);
        std::map<std::pair<std::string, std::string>, std::unordered_set<std::string>> parent_uid_cache;
        std::vector<IntegrityIssue> issues;

        for (const std::string &table : raw_tables())
        {
            const std::vector<const Row *> &table_rows = rows_of(rows_by_table, table);
            std::map<std::string, long long> uid_counts; // sorted by uid text, like sorted(Counter.items())
            if (authoritative_uid_tables().count(table))
            {
                for (const Row *row : table_rows)
                {
                    const std::string &uid = get_or_empty(*row, "UID");
                    std::string normalized;
                    if (!positive_ascii_decimal(uid, &normalized))
                    {
                        issues.push_back(malformed_uid(table, uid, "UID", uid));
                        continue;
                    }
                    ++uid_counts[normalized];
                }
            }
            else
            {
                for (const Row *row : table_rows)
                {
                    const std::string &uid = get_or_empty(*row, "UID");
                    if (is_present_uid(uid))
                        ++uid_counts[uid];
                }
            }
            for (const auto &entry : uid_counts)
                if (entry.second > 1)
                    issues.push_back(duplicate_uid(table, entry.first, entry.second));
        }

        for (const char *table : {"BidAreas", "BidConditionFolders", "BidPageFolders"})
        {
            std::unordered_map<std::string, std::string> parent_by_uid;
            for (const Row *row : rows_of(rows_by_table, table))
            {
                const std::string &uid = get_or_empty(*row, "UID");
                const std::string &parent = get_or_empty(*row, "ParentUID");
                if (is_present_uid(uid) && is_present_uid(parent))
                    parent_by_uid[uid] = parent;
            }
            for (const std::string &uid : find_cycle_uids(parent_by_uid))
                issues.push_back(parent_cycle(table, uid));
        }

        {
            std::unordered_map<std::string, std::string> parent_by_uid;
            std::unordered_map<std::string, std::string> page_by_uid;
            for (const Row *row : rows_of(rows_by_table, "BidTakeoffs"))
            {
                const std::string &uid = get_or_empty(*row, "UID");
                parent_by_uid[uid] = get_or_empty(*row, "ParentUID");
                page_by_uid[uid] = get_or_empty(*row, "BidPageUID");
            }
            std::set<std::string> mismatches;
            for (const auto &entry : parent_by_uid)
            {
                const std::string &parent = entry.second;
                if (parent.empty() || parent == "0")
                    continue;
                const auto parent_page = page_by_uid.find(parent);
                if (parent_page == page_by_uid.end())
                    continue;
                const auto own_page = page_by_uid.find(entry.first); // always there: every uid of parent_by_uid is a key of page_by_uid
                if (own_page == page_by_uid.end() || own_page->second != parent_page->second)
                    mismatches.insert(entry.first);
            }
            for (const std::string &uid : mismatches)
            {
                IntegrityIssue issue;
                issue.kind = IntegrityIssue::Kind::TakeoffPageMismatch;
                issue.table = "BidTakeoffs";
                issue.uid = uid;
                issues.push_back(issue);
            }
        }

        {
            std::map<std::string, long long> settings_counts;
            for (const Row *row : rows_of(rows_by_table, "BidSettings"))
            {
                const std::string &bid_uid = get_or_empty(*row, "BidUID");
                if (is_present_uid(bid_uid))
                    ++settings_counts[bid_uid];
            }
            for (const auto &entry : settings_counts)
            {
                if (entry.second > 1)
                {
                    IntegrityIssue issue;
                    issue.kind = IntegrityIssue::Kind::Cardinality;
                    issue.table = "BidSettings";
                    issue.owner_table = "Bids";
                    issue.owner_uid = entry.first;
                    issue.count = entry.second;
                    issue.maximum = 1;
                    issues.push_back(issue);
                }
            }
        }

        for (const RawRelationship &relationship : relationships)
        {
            const std::vector<const Row *> &child_rows = rows_of(rows_by_table, relationship.child_table);
            if (child_rows.empty())
                continue;
            // the parent column is always UID (Windows RawBidRelationship.parent_column default)
            const std::pair<std::string, std::string> key(relationship.parent_table, "UID");
            auto cached = parent_uid_cache.find(key);
            if (cached == parent_uid_cache.end())
                cached = parent_uid_cache.emplace(key, present_uids(rows_of(rows_by_table, relationship.parent_table), "UID")).first;
            const std::unordered_set<std::string> &valid_parent_uids = cached->second;
            for (const Row *row : child_rows)
            {
                const auto column = row->find(relationship.child_column);
                if (column == row->end())
                {
                    if (relationship.required)
                        issues.push_back(missing_reference(relationship.child_table, get_or_empty(*row, "UID"), relationship.child_column,
                                                           "<missing>", relationship.parent_table));
                    continue;
                }
                const std::string &value = column->second;
                if (!is_present_uid(value))
                {
                    if (relationship.required)
                        issues.push_back(missing_reference(relationship.child_table, get_or_empty(*row, "UID"), relationship.child_column,
                                                           "<missing>", relationship.parent_table));
                    continue;
                }
                if (valid_parent_uids.count(value))
                    continue;
                issues.push_back(missing_reference(relationship.child_table, get_or_empty(*row, "UID"), relationship.child_column, value,
                                                   relationship.parent_table));
            }
        }
        return issues;
    }

    std::string format_integrity_issues(const std::vector<IntegrityIssue> &issues, size_t limit)
    {
        std::string preview;
        for (size_t i = 0; i < issues.size() && i < limit; ++i)
        {
            if (i > 0)
                preview += "; ";
            preview += issues[i].format();
        }
        if (issues.size() > limit)
            preview += "; +" + std::to_string(issues.size() - limit) + " more";
        return preview;
    }

    TakeoffGraphResolution resolve_takeoff_graph(OstRawData &raw)
    {
        TakeoffGraphResolution resolution;
        const auto table = raw.page_tables.find("BidTakeoffs");
        if (table == raw.page_tables.end() || table->second.empty())
            return resolution;
        const RowList &rows = table->second;

        std::vector<IntegrityIssue> malformed;
        std::vector<std::pair<std::string, const Row *>> rows_by_uid; // insertion order of the Python dict
        std::unordered_map<std::string, size_t> position_by_uid;
        std::map<std::string, long long> duplicate_counts;
        for (const Row &row : rows)
        {
            const std::string &uid = get_or_empty(row, "UID");
            if (!positive_ascii_decimal(uid))
            {
                malformed.push_back(malformed_uid("BidTakeoffs", uid, "UID", uid));
                continue;
            }
            if (position_by_uid.count(uid))
            {
                const auto known = duplicate_counts.find(uid);
                duplicate_counts[uid] = (known == duplicate_counts.end() ? 1 : known->second) + 1;
            }
            else
            {
                position_by_uid[uid] = rows_by_uid.size();
                rows_by_uid.emplace_back(uid, &row);
            }
        }
        std::vector<IntegrityIssue> duplicates;
        for (const auto &entry : duplicate_counts)
            duplicates.push_back(duplicate_uid("BidTakeoffs", entry.first, entry.second));
        if (!malformed.empty() || !duplicates.empty())
        {
            resolution.fatal_issues = malformed;
            resolution.fatal_issues.insert(resolution.fatal_issues.end(), duplicates.begin(), duplicates.end());
            return resolution;
        }

        std::vector<std::pair<std::string, std::string>> parent_by_uid; // insertion order
        std::unordered_map<std::string, std::vector<std::string>> children_by_parent_uid;
        for (const auto &entry : rows_by_uid)
        {
            const std::string &parent_uid = get_or_empty(*entry.second, "ParentUID");
            if (!is_present_uid(parent_uid))
                continue;
            if (!positive_ascii_decimal(parent_uid))
            {
                malformed.push_back(malformed_uid("BidTakeoffs", entry.first, "ParentUID", parent_uid));
                continue;
            }
            parent_by_uid.emplace_back(entry.first, parent_uid);
            children_by_parent_uid[parent_uid].push_back(entry.first);
        }
        if (!malformed.empty())
        {
            resolution.fatal_issues = malformed;
            return resolution;
        }

        {
            std::unordered_map<std::string, std::string> parents(parent_by_uid.begin(), parent_by_uid.end());
            const std::set<std::string> cycle_uids = find_cycle_uids(parents);
            if (!cycle_uids.empty())
            {
                for (const std::string &uid : cycle_uids)
                    resolution.fatal_issues.push_back(parent_cycle("BidTakeoffs", uid));
                return resolution;
            }
        }

        for (const auto &entry : parent_by_uid)
            if (!position_by_uid.count(entry.second))
                resolution.missing_parent_roots.push_back(missing_reference("BidTakeoffs", entry.first, "ParentUID", entry.second, "BidTakeoffs"));
        std::unordered_set<std::string> invalid_uids;
        std::deque<std::string> pending;
        for (const IntegrityIssue &issue : resolution.missing_parent_roots)
        {
            invalid_uids.insert(issue.row_uid);
            pending.push_back(issue.row_uid);
        }
        while (!pending.empty())
        {
            const std::string invalid_parent_uid = pending.front();
            pending.pop_front();
            const auto children = children_by_parent_uid.find(invalid_parent_uid);
            if (children == children_by_parent_uid.end())
                continue;
            for (const std::string &child_uid : children->second)
            {
                if (invalid_uids.count(child_uid))
                    continue;
                invalid_uids.insert(child_uid);
                pending.push_back(child_uid);
                resolution.dependent_descendants.push_back(
                    missing_reference("BidTakeoffs", child_uid, "ParentUID", invalid_parent_uid, "BidTakeoffs"));
            }
        }
        if (!invalid_uids.empty())
        {
            RowList kept;
            for (const Row &row : rows)
                if (!invalid_uids.count(get_or_empty(row, "UID")))
                    kept.push_back(row);
            table->second = std::move(kept);
        }
        return resolution;
    }

    std::vector<IntegrityIssue> clear_missing_selected_page_references(OstRawData &raw)
    {
        std::vector<IntegrityIssue> cleared;
        const auto pages = raw.bid_tables.find("BidPages");
        std::unordered_set<std::string> page_uids;
        if (pages != raw.bid_tables.end())
            for (const Row &row : pages->second)
            {
                const std::string &uid = get_or_empty(row, "UID");
                if (is_present_uid(uid))
                    page_uids.insert(uid);
            }
        const auto settings = raw.bid_tables.find("BidSettings");
        if (settings == raw.bid_tables.end())
            return cleared;
        for (Row &row : settings->second)
        {
            const std::string selected_uid = get_or_empty(row, "BidPageSelectedUID");
            if (!is_present_uid(selected_uid) || page_uids.count(selected_uid) || !text_rules::py_isdecimal(selected_uid))
                continue;
            cleared.push_back(missing_reference("BidSettings", get_or_empty(row, "UID"), "BidPageSelectedUID", selected_uid, "BidPages"));
            row["BidPageSelectedUID"] = "0";
        }
        return cleared;
    }

    std::vector<IntegrityIssue> clear_missing_annotation_takeoff_references(OstRawData &raw)
    {
        std::vector<IntegrityIssue> cleared;
        std::unordered_set<std::string> takeoff_uids;
        const auto takeoffs = raw.page_tables.find("BidTakeoffs");
        if (takeoffs != raw.page_tables.end())
            for (const Row &row : takeoffs->second)
            {
                const std::string &uid = get_or_empty(row, "UID");
                if (is_present_uid(uid))
                    takeoff_uids.insert(uid);
            }
        for (const char *table : {"BidDimensions", "BidArrows", "BidALines"})
        {
            const auto rows = raw.page_tables.find(table);
            if (rows == raw.page_tables.end())
                continue;
            for (Row &row : rows->second)
            {
                for (const char *column : {"BidTakeoffFromUID", "BidTakeoffToUID"})
                {
                    const std::string takeoff_uid = get_or_empty(row, column);
                    if (!is_present_uid(takeoff_uid) || takeoff_uids.count(takeoff_uid) || !text_rules::py_isdecimal(takeoff_uid))
                        continue;
                    cleared.push_back(missing_reference(table, get_or_empty(row, "UID"), column, takeoff_uid, "BidTakeoffs"));
                    row[column] = "0";
                }
            }
        }
        return cleared;
    }

    namespace
    {
        // ost_importer._discard_stale_named_views
        void discard_stale_named_views(OstRawData &raw)
        {
            const auto views_table = raw.bid_tables.find("BidNamedViews");
            if (views_table == raw.bid_tables.end())
                return;
            // page_uids = {row.get("UID") for row in BidPages}: a page without UID contributes None, which no text equals
            std::unordered_set<std::string> page_uids;
            const auto pages = raw.bid_tables.find("BidPages");
            if (pages != raw.bid_tables.end())
                for (const Row &row : pages->second)
                {
                    const auto uid = row.find("UID");
                    if (uid != row.end())
                        page_uids.insert(uid->second);
                }
            const auto bid_uid = raw.bid_row.find("UID");

            // The set of stale issues (a Python set of frozen dataclasses: equal issues are one), in order of first appearance.
            std::vector<IntegrityIssue> stale;
            for (const Row &row : views_table->second)
            {
                // row.get("BidUID") == raw_data.bid_row.get("UID"): None == None is true, None == "" is not
                const auto view_bid = row.find("BidUID");
                const bool same_bid = (view_bid == row.end() && bid_uid == raw.bid_row.end()) ||
                                      (view_bid != row.end() && bid_uid != raw.bid_row.end() && view_bid->second == bid_uid->second);
                if (!same_bid)
                    continue;
                const std::string &page_uid = get_or_empty(row, "BidPageUID");
                if (!positive_ascii_decimal(page_uid) || page_uids.count(page_uid))
                    continue;
                IntegrityIssue issue = missing_reference("BidNamedViews", get_or_empty(row, "UID"), "BidPageUID", page_uid, "BidPages");
                if (std::find(stale.begin(), stale.end(), issue) == stale.end())
                    stale.push_back(issue);
            }
            if (stale.empty())
                return;
            for (const IntegrityIssue &issue : validate_raw_bid_integrity(raw))
                if (std::find(stale.begin(), stale.end(), issue) == stale.end())
                    return;

            std::unordered_set<std::string> stale_uids;
            for (const IntegrityIssue &issue : stale)
                stale_uids.insert(issue.row_uid);
            // Python assigns bid_tables["BidHotLinks"] (the key appears when it was absent); the lookup below may add the table, which
            // invalidates earlier iterators into the TableMap, so everything is looked up again.
            RowList &links = raw.bid_tables["BidHotLinks"];
            RowList &views = raw.bid_tables["BidNamedViews"];
            RowList retained_links;
            for (const Row &row : links)
            {
                const auto view_uid = row.find("BidPageViewUID");
                if (view_uid == row.end() || !stale_uids.count(view_uid->second))
                    retained_links.push_back(row);
            }
            RowList retained_views;
            for (const Row &row : views)
            {
                const auto uid = row.find("UID");
                if (uid == row.end() || !stale_uids.count(uid->second))
                    retained_views.push_back(row);
            }
            const size_t dropped_links = links.size() - retained_links.size();
            views = std::move(retained_views);
            links = std::move(retained_links);

            // sorted(stale, key=row_uid): a stable sort here (Python sorts a set, so equal keys are in no defined order)
            std::vector<IntegrityIssue> sorted_stale = stale;
            std::stable_sort(sorted_stale.begin(), sorted_stale.end(),
                             [](const IntegrityIssue &a, const IntegrityIssue &b) { return a.row_uid < b.row_uid; });
            ost_log::warning("Discarded " + std::to_string(stale_uids.size()) + " stale Named View(s) and " + std::to_string(dropped_links) +
                             " dependent Hot Link(s) during OST import for Bid " + (bid_uid == raw.bid_row.end() ? std::string("None") : bid_uid->second) +
                             "; absent Page references: " + format_integrity_issues(sorted_stale));
        }
    }

    bool validate_raw_data(OstRawData &raw, std::string &error, std::string *details)
    {
        error.clear();
        if (details)
            details->clear();
        try
        {
            const TakeoffGraphResolution graph = resolve_takeoff_graph(raw);
            if (!graph.fatal_issues.empty())
            {
                error = std::string(kTakeoffGraphInvalidPrefix) + format_integrity_issues(graph.fatal_issues);
                return false;
            }
            if (graph.skipped_count() > 0)
            {
                const std::string descendant_diagnostic =
                    graph.dependent_descendants.empty()
                        ? std::string()
                        : "; skipped dependent descendants: " + format_integrity_issues(graph.dependent_descendants);
                ost_log::warning("Skipping " + std::to_string(graph.skipped_count()) + " invalid takeoff(s) during OST import; missing-parent roots: " +
                                 format_integrity_issues(graph.missing_parent_roots) + descendant_diagnostic);
            }
            const std::vector<IntegrityIssue> cleared_selected_pages = clear_missing_selected_page_references(raw);
            if (!cleared_selected_pages.empty())
                ost_log::warning("Cleared " + std::to_string(cleared_selected_pages.size()) +
                                 " missing selected-page reference(s) during OST import: " + format_integrity_issues(cleared_selected_pages));
            const std::vector<IntegrityIssue> cleared_annotation_refs = clear_missing_annotation_takeoff_references(raw);
            if (!cleared_annotation_refs.empty())
                ost_log::warning("Cleared " + std::to_string(cleared_annotation_refs.size()) +
                                 " missing annotation takeoff attachment reference(s) during OST import: " +
                                 format_integrity_issues(cleared_annotation_refs));
            discard_stale_named_views(raw);
            const std::vector<IntegrityIssue> invalid_refs = validate_raw_bid_integrity(raw);
            if (!invalid_refs.empty())
            {
                const std::string text = format_integrity_issues(invalid_refs);
                ost_log::log(ost_log::Level::Error, "Rejecting OST import because it contains invalid database references: " + text);
                error = kInvalidReferencesError;
                if (details)
                    *details = text;
                return false;
            }
            return true;
        }
        catch (const PythonIntLimitError &limit)
        {
            error = limit.what();
            return false;
        }
    }
}
