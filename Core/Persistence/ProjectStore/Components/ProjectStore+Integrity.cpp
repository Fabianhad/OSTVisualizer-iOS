// ProjectStore component - identity / ownership checks and UID allocation shared by the writers (see ProjectStore_Integrity.hpp).
#include "../ProjectStore_Integrity.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ost_store
{
    namespace integrity
    {

        namespace
        {
            struct Relationship
            {
                const char *child_table;
                const char *child_column;
                const char *parent_table; // the parent column is always UID
            };

            // raw_bid_integrity.BID_RELATIONSHIPS, in the Windows order (every parent column is UID).
            const Relationship kBidRelationships[] = {
                {"Bids", "BidProjectUID", "BidProjects"},
                {"Bids", "OrigBidProjectUID", "BidProjects"},
                {"Bids", "JobStatusUID", "JobStatuses"},
                {"Bids", "EstimatorUID", "Employees"},
                {"Bids", "PrManagerUID", "Employees"},
                {"Bids", "JobSiteManagerUID", "Employees"},
                {"AffectDPCTypGroupViews", "BidTypGroupViewUID", "BidTypGroupViews"},
                {"AffectDPCTypGroupViews", "BidUID", "Bids"},
                {"BidPlanRooms", "BidUID", "Bids"},
                {"BidAreas", "BidUID", "Bids"},
                {"BidAreas", "ParentUID", "BidAreas"},
                {"BidTypAreas", "BidUID", "Bids"},
                {"BidConditionFolders", "BidUID", "Bids"},
                {"BidConditionFolders", "ParentUID", "BidConditionFolders"},
                {"BidPageFolders", "BidUID", "Bids"},
                {"BidPageFolders", "ParentUID", "BidPageFolders"},
                {"BidSettings", "BidUID", "Bids"},
                {"BidSettings", "BidPageSelectedUID", "BidPages"},
                {"BidEmployees", "BidUID", "Bids"},
                {"BidEmployees", "EmployeeUID", "Employees"},
                {"BidEmployees", "PayClassUID", "PayClasses"},
                {"BidLayers", "BidUID", "Bids"},
                {"BidTypAreaCounts", "BidAreaUID", "BidAreas"},
                {"BidTypAreaCounts", "BidTypAreaUID", "BidTypAreas"},
                {"BidConditions", "BidUID", "Bids"},
                {"BidConditions", "BidConditionFolderUID", "BidConditionFolders"},
                {"BidConditions", "BidLayerUID", "BidLayers"},
                {"BidConditions", "CdnTypeUID", "CdnTypes"},
                {"BidConditionUser", "BidUID", "Bids"},
                {"BidConditionUser", "ConditionUID", "BidConditions"},
                {"BidDPCSubscribers", "BidUID", "Bids"},
                {"BidDPCSubscribers", "BidEmployeeUID", "Employees"},
                {"BidLaborActivity", "BidUID", "Bids"},
                {"BidLaborActivity", "BidConditionUID", "BidConditions"},
                {"BidLaborActivity", "BidLaborCostCodeUID", "BidLaborCostCodes"},
                {"BidLaborCostCodes", "BidUID", "Bids"},
                {"BidLaborCostCodes", "CostCodeUID", "CostCodes"},
                {"BidLaborCostCodeTotals", "BidUID", "Bids"},
                {"BidLaborCostCodeTotals", "BidPageUID", "BidPages"},
                {"BidLaborCostCodeTotals", "BidAreaUID", "BidAreas"},
                {"BidLaborCostCodeTotals", "BidLaborCostCodeUID", "BidLaborCostCodes"},
                {"BidNotes", "BidUID", "Bids"},
                {"BidPercents", "BidTimeCardStateUID", "BidTimeCardStates"},
                {"BidPercents", "BidLaborCostCodeUID", "BidLaborCostCodes"},
                {"BidPercents", "BidLaborActivityUID", "BidLaborActivity"},
                {"BidPercents", "BidTakeoffUID", "BidTakeoffs"},
                {"BidPercents", "BidPageUID", "BidPages"},
                {"BidZones", "BidUID", "Bids"},
                {"BidZones", "BidLayerUID", "BidLayers"},
                {"BidTypGroupViews", "BidUID", "Bids"},
                {"BidTypGroupViews", "BidPageUID", "BidPages"},
                {"BidTypGroupViews", "BidConditionUID", "BidConditions"},
                {"BidPages", "BidUID", "Bids"},
                {"BidPages", "BidPageFolderUID", "BidPageFolders"},
                {"BidPages", "MasterPageUID", "BidPages"},
                {"BidTakeoffs", "BidUID", "Bids"},
                {"BidTakeoffs", "BidPageUID", "BidPages"},
                {"BidTakeoffs", "BidConditionUID", "BidConditions"},
                {"BidTakeoffs", "BidZoneUID", "BidZones"},
                {"BidTakeoffs", "BidAreaUID", "BidAreas"},
                {"BidTakeoffs", "BidTypAreaUID", "BidTypAreas"},
                {"BidTakeoffs", "ParentUID", "BidTakeoffs"},
                {"BidTakeoffs", "TypGroupTakeoffUID", "BidTakeoffs"},
                {"BidTakeoffs", "TypPageTakeoffUID", "BidTakeoffs"},
                {"BidTakeoffs", "TypGroupMarkerUID", "BidTakeoffs"},
                {"BidTakeoffTotals", "BidUID", "Bids"},
                {"BidTakeoffTotals", "BidPageUID", "BidPages"},
                {"BidTakeoffTotals", "BidZoneUID", "BidZones"},
                {"BidTakeoffTotals", "BidAreaUID", "BidAreas"},
                {"BidTakeoffTotals", "BidTypAreaUID", "BidTypAreas"},
                {"BidTakeoffTotals", "BidConditionUID", "BidConditions"},
                {"BidTimeCardStates", "BidUID", "Bids"},
                {"BidTimeCards", "BidTimeCardStateUID", "BidTimeCardStates"},
                {"BidTimeCards", "BidEmployeeUID", "BidEmployees"},
                {"BidTimeCards", "BidAreaUID", "BidAreas"},
                {"BidTimeCards", "BidTypicalAreaUID", "BidTypAreas"},
                {"BidTimeCards", "BidLaborCostCodeUID", "BidLaborCostCodes"},
                {"BidTransactionsHistory", "BidUID", "Bids"},
                {"BidHighlights", "BidUID", "Bids"},
                {"BidHighlights", "BidPageUID", "BidPages"},
                {"BidHighlights", "BidLayerUID", "BidLayers"},
                {"BidTexts", "BidUID", "Bids"},
                {"BidTexts", "BidPageUID", "BidPages"},
                {"BidTexts", "BidLayerUID", "BidLayers"},
                {"BidDimensions", "BidUID", "Bids"},
                {"BidDimensions", "BidPageUID", "BidPages"},
                {"BidDimensions", "BidTakeoffFromUID", "BidTakeoffs"},
                {"BidDimensions", "BidTakeoffToUID", "BidTakeoffs"},
                {"BidArrows", "BidUID", "Bids"},
                {"BidArrows", "BidPageUID", "BidPages"},
                {"BidArrows", "BidTakeoffFromUID", "BidTakeoffs"},
                {"BidArrows", "BidTakeoffToUID", "BidTakeoffs"},
                {"BidALines", "BidUID", "Bids"},
                {"BidALines", "BidPageUID", "BidPages"},
                {"BidALines", "BidTakeoffFromUID", "BidTakeoffs"},
                {"BidALines", "BidTakeoffToUID", "BidTakeoffs"},
                {"BidCallOuts", "BidUID", "Bids"},
                {"BidCallOuts", "BidPageUID", "BidPages"},
                {"BidCallOuts", "BidLayerUID", "BidLayers"},
                {"BidAnnotationRects", "BidUID", "Bids"},
                {"BidAnnotationRects", "BidPageUID", "BidPages"},
                {"BidAnnotationRects", "BidLayerUID", "BidLayers"},
                {"BidAnnotationOvals", "BidUID", "Bids"},
                {"BidAnnotationOvals", "BidPageUID", "BidPages"},
                {"BidAnnotationOvals", "BidLayerUID", "BidLayers"},
                {"BidAnnotationPolygons", "BidUID", "Bids"},
                {"BidAnnotationPolygons", "BidPageUID", "BidPages"},
                {"BidAnnotationPolygons", "BidLayerUID", "BidLayers"},
                {"BidAnnotationClouds", "BidUID", "Bids"},
                {"BidAnnotationClouds", "BidPageUID", "BidPages"},
                {"BidAnnotationClouds", "BidLayerUID", "BidLayers"},
                {"BidAnnoInk", "BidUID", "Bids"},
                {"BidAnnoInk", "BidPageUID", "BidPages"},
                {"BidLegends", "BidUID", "Bids"},
                {"BidLegends", "BidPageUID", "BidPages"},
                {"BidPageSettings", "BidPageUID", "BidPages"},
                {"BidPageSettings", "BidAreaUID", "BidAreas"},
                {"BidPageSettings", "BidTypAreaUID", "BidTypAreas"},
                {"BidAreaTranslations", "BidPageUID", "BidPages"},
                {"BidAreaTranslations", "MasterAreaUID", "BidAreas"},
                {"BidAreaTranslations", "TranslateAreaUID", "BidAreas"},
                {"BidMarkedPages", "BidPageUID", "BidPages"},
                {"BidComments", "BidUID", "Bids"},
                {"BidComments", "BidPageUID", "BidPages"},
                {"BidComments", "BidLayerUID", "BidLayers"},
                {"BidComments", "ParentCommentUID", "BidComments"},
                {"BidNamedViews", "BidUID", "Bids"},
                {"BidNamedViews", "BidPageUID", "BidPages"},
                {"BidHotLinks", "BidUID", "Bids"},
                {"BidHotLinks", "BidPageUID", "BidPages"},
                {"BidHotLinks", "BidPageViewUID", "BidNamedViews"},
                {"BidHotLinks", "BidLayerUID", "BidLayers"},
                {"BidTypicalGroupTotals", "BidUID", "Bids"},
                {"BidTypicalGroupTotals", "BidPageUID", "BidPages"},
                {"BidTypicalGroupTotals", "BidZoneUID", "BidZones"},
                {"BidTypicalGroupTotals", "BidAreaUID", "BidAreas"},
                {"BidTypicalGroupTotals", "BidConditionUID", "BidConditions"},
                {"Boost", "BidUID", "Bids"},
                {"Boost", "BidPageUID", "BidPages"},
                {"DPCCalcFilter", "BidUID", "Bids"},
                {"DPCCalcFilter", "BidPageUID", "BidPages"},
                {"STSTransactionHistory", "BidUID", "Bids"},
                {"Employees", "PayClassUID", "PayClasses"},
                {"Employees", "AccessLevelUID", "AccessLevels"},
            };

            // Steps the statement once: false when it failed; `found` tells whether a row came back.
            bool single_row(ost_db::Statement &stmt, bool &found)
            {
                const ost_db::StepResult result = stmt.step_result();
                if (result == ost_db::StepResult::Error)
                    return false;
                found = (result == ost_db::StepResult::Row);
                return true;
            }
        } // namespace

        bool parse_uid(const std::string &text, long long &out)
        {
            if (text.empty())
                return false;
            const unsigned long long limit = 9223372036854775807ull;
            unsigned long long value = 0;
            for (const char c : text)
            {
                if (c < '0' || c > '9')
                    return false;
                const unsigned long long digit = static_cast<unsigned long long>(c - '0');
                if (value > (limit - digit) / 10ull)
                    return false;
                value = value * 10ull + digit;
            }
            if (value == 0)
                return false;
            out = static_cast<long long>(value);
            return true;
        }

        bool row_exists(ost_db::OstDatabase &db, const char *table, long long uid)
        {
            ost_db::Statement stmt = db.prepare(std::string("SELECT 1 FROM [") + table + "] WHERE [UID] = ?");
            if (!stmt.valid())
                return false;
            stmt.bind_int64(1, uid);
            bool found = false;
            return single_row(stmt, found) && found;
        }

        bool bid_exists(ost_db::OstDatabase &db, long long bid_uid)
        {
            return row_exists(db, "Bids", bid_uid);
        }

        bool row_belongs_to_bid(ost_db::OstDatabase &db, const char *table, long long uid, long long bid_uid)
        {
            if (bid_uid <= 0)
                return false;
            {
                ost_db::Statement stmt = db.prepare(std::string("SELECT [BidUID] FROM [") + table + "] WHERE [UID] = ?");
                if (!stmt.valid())
                    return false;
                stmt.bind_int64(1, uid);
                bool found = false;
                if (!single_row(stmt, found) || !found)
                    return false;
                if (stmt.column_type(0) == ost_db::Statement::TypeNull || stmt.column_int64(0) != bid_uid)
                    return false;
            }
            return bid_exists(db, bid_uid);
        }

        bool owner_bid_of(ost_db::OstDatabase &db, const char *table, long long uid, long long &bid_uid)
        {
            long long owner = 0;
            {
                ost_db::Statement stmt = db.prepare(std::string("SELECT [BidUID] FROM [") + table + "] WHERE [UID] = ?");
                if (!stmt.valid())
                    return false;
                stmt.bind_int64(1, uid);
                bool found = false;
                if (!single_row(stmt, found) || !found)
                    return false;
                if (stmt.column_type(0) == ost_db::Statement::TypeNull)
                    return false;
                owner = stmt.column_int64(0);
            }
            if (owner <= 0 || !bid_exists(db, owner))
                return false;
            bid_uid = owner;
            return true;
        }

        long long next_uid_preserving_references(ost_db::OstDatabase &db, const char *table)
        {
            long long next_uid = 1;
            {
                ost_db::Statement stmt = db.prepare(std::string("SELECT MAX([UID]) FROM [") + table + "]");
                if (!stmt.valid())
                    return 0;
                bool found = false;
                if (!single_row(stmt, found))
                    return 0;
                if (found && stmt.column_type(0) != ost_db::Statement::TypeNull)
                {
                    const long long max_uid = stmt.column_int64(0);
                    if (max_uid == std::numeric_limits<long long>::max())
                        return 0; // no UID is left above it
                    next_uid = max_uid + 1;
                }
            }

            std::set<std::pair<std::string, std::string>> checked;
            for (const Relationship &relationship : kBidRelationships)
            {
                if (std::string(relationship.parent_table) != table)
                    continue;
                if (!checked.insert({relationship.child_table, relationship.child_column}).second)
                    continue;
                // A reference column the schema does not have cannot hold a dangling reference.
                if (!db.table_exists(relationship.child_table) ||
                    !db.column_exists(relationship.child_table, relationship.child_column))
                    continue;
                // CAST: a NULL, an empty string and 0 all read as 0 (Windows skips None, "", 0 and "0"); integers stay as they are.
                ost_db::Statement stmt = db.prepare(std::string("SELECT MAX(CAST([") + relationship.child_column +
                                                    "] AS INTEGER)) FROM [" + relationship.child_table + "]");
                if (!stmt.valid())
                    return 0;
                bool found = false;
                if (!single_row(stmt, found))
                    return 0;
                if (!found || stmt.column_type(0) == ost_db::Statement::TypeNull)
                    continue;
                const long long referenced = stmt.column_int64(0);
                if (referenced >= next_uid)
                {
                    if (referenced == std::numeric_limits<long long>::max())
                        return 0;
                    next_uid = referenced + 1;
                }
            }
            return next_uid;
        }

        std::string uid_list_problem(const std::vector<std::string> &uids, const char *table)
        {
            // Windows compares str(int(uid)); a UID beyond 63 bits cannot be stored in the INTEGER column and counts as malformed.
            std::set<long long> seen;
            for (const std::string &uid : uids)
            {
                long long value = 0;
                if (!parse_uid(uid, value))
                    return std::string(table) + " contains malformed UID " + (uid.empty() ? std::string("<missing>") : uid) +
                           "; authoritative bid-owned UIDs must be positive integers.";
                if (!seen.insert(value).second)
                    return std::string(table) + " contains duplicate UID " + std::to_string(value) +
                           "; authoritative bid-owned UIDs must be unique.";
            }
            return std::string();
        }

        std::string master_data_uid_problem(const std::vector<std::string> &uids, const char *table)
        {
            std::unordered_set<std::string> seen;
            for (const std::string &uid : uids)
                if (!seen.insert(uid).second)
                    return std::string(table) + " contains duplicate UID " + uid +
                           "; authoritative master-data UIDs must be unique.";
            return std::string();
        }

        std::string find_parent_cycle_uid(const std::vector<std::pair<std::string, std::string>> &uid_and_parent)
        {
            std::unordered_map<std::string, std::string> parent_of;
            std::vector<std::string> order; // the uids in row order (Windows: dict insertion order)
            for (const auto &row : uid_and_parent)
            {
                if (row.second.empty() || row.second == "0")
                    continue;
                if (parent_of.emplace(row.first, row.second).second)
                    order.push_back(row.first);
                else
                    parent_of[row.first] = row.second;
            }

            std::unordered_set<std::string> completed;
            std::unordered_set<std::string> cycle_uids;
            for (const std::string &start : order)
            {
                if (completed.count(start))
                    continue;
                std::vector<std::string> path;
                std::unordered_map<std::string, std::size_t> path_index;
                std::string current = start;
                while (parent_of.count(current) && !completed.count(current))
                {
                    auto seen = path_index.find(current);
                    if (seen != path_index.end())
                    {
                        for (std::size_t i = seen->second; i < path.size(); ++i)
                            cycle_uids.insert(path[i]);
                        break;
                    }
                    path_index[current] = path.size();
                    path.push_back(current);
                    current = parent_of.at(current);
                }
                for (const std::string &uid : path)
                    completed.insert(uid);
            }
            if (cycle_uids.empty())
                return std::string();

            // min(cycle_uids, key=(0 if ascii-decimal else 1, int(uid) or uid)): decimal uids first, by value, then the others by text
            auto is_decimal = [](const std::string &uid)
            {
                if (uid.empty())
                    return false;
                for (const char c : uid)
                    if (c < '0' || c > '9')
                        return false;
                return true;
            };
            auto numeric_less = [](const std::string &a, const std::string &b)
            {
                const std::size_t a_start = std::min(a.find_first_not_of('0'), a.size());
                const std::size_t b_start = std::min(b.find_first_not_of('0'), b.size());
                const std::size_t a_len = a.size() - a_start;
                const std::size_t b_len = b.size() - b_start;
                if (a_len != b_len)
                    return a_len < b_len;
                return a.compare(a_start, a_len, b, b_start, b_len) < 0;
            };
            const std::string *smallest = nullptr;
            for (const std::string &uid : cycle_uids)
            {
                if (!smallest)
                {
                    smallest = &uid;
                    continue;
                }
                const bool uid_decimal = is_decimal(uid);
                const bool smallest_decimal = is_decimal(*smallest);
                bool less;
                if (uid_decimal != smallest_decimal)
                    less = uid_decimal;
                else if (uid_decimal)
                    less = numeric_less(uid, *smallest);
                else
                    less = uid < *smallest;
                if (less)
                    smallest = &uid;
            }
            return *smallest;
        }

    }
}
