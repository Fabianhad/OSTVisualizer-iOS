// Layer: Import (pure C++)
// The validation of a parsed OST file before anything is written (IMP-09): the Windows importer rejects or repairs an invalid project
// on the raw rows, before its UIDs are re-numbered and before the first database write.
//
// Windows: ost_visualizer/infrastructure/mdb/raw_bid_integrity.py (validate_raw_bid_integrity, resolve_takeoff_graph,
// clear_missing_selected_page_references, clear_missing_annotation_takeoff_references, format_integrity_issues, RAW_BID_RELATIONSHIPS) and
// ost_visualizer/infrastructure/mdb/importers/ost_importer.py (_validated_raw_data, _discard_stale_named_views, _validate_page_references);
// the relationship table is ost_schema::BID_RELATIONSHIPS (ported verbatim). The texts of the issues and of the log records are the
// Windows texts; the records go through ost_log (the Windows logger.warning / logger.error calls).
//
// Python's int(text) refuses more than 4300 digits (ValueError); the Windows validation lets that propagate and the import fails. The
// functions here do the same: they throw PythonIntLimitError, validate_raw_data turns it into a failed validation.
#pragma once
#include "OstXmlParser.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace ost_import
{
    class PythonIntLimitError : public std::runtime_error
    {
    public:
        explicit PythonIntLimitError(size_t digits);
    };

    struct IntegrityIssue
    {
        enum class Kind
        {
            MissingReference,   // RawBidIntegrityIssue: a reference column names a row that is not there
            DuplicateUid,       // RawBidDuplicateUid
            MalformedUid,       // RawBidMalformedUid
            ParentCycle,        // RawBidParentCycle
            TakeoffPageMismatch,// RawBidTakeoffPageMismatch: a takeoff whose parent lies on another page
            Cardinality         // RawBidCardinalityIssue
        };
        Kind kind = Kind::MissingReference;
        std::string table;
        std::string row_uid;      // MissingReference, MalformedUid: the UID of the row ("" = none)
        std::string column;       // MissingReference, MalformedUid
        std::string value;        // MissingReference: the missing UID ("<missing>" when a required column is absent or empty); MalformedUid: the value
        std::string parent_table; // MissingReference
        std::string uid;          // DuplicateUid, ParentCycle, TakeoffPageMismatch
        long long count = 0;      // DuplicateUid, Cardinality
        long long maximum = 0;    // Cardinality
        std::string owner_table;  // Cardinality
        std::string owner_uid;    // Cardinality

        // issue.format()
        std::string format() const;
    };

    bool operator==(const IntegrityIssue &a, const IntegrityIssue &b);

    struct RawRelationship
    {
        std::string child_table;
        std::string child_column;
        std::string parent_table;
        bool required = false;
    };

    // RAW_BID_RELATIONSHIPS: the relationships between tables that exist in a raw file, without the references the import reconciles
    // itself (Bids.JobStatusUID / EstimatorUID / PrManagerUID / JobSiteManagerUID), in the Windows order.
    const std::vector<RawRelationship> &raw_bid_relationships();

    // validate_raw_bid_integrity: every problem found, in the Windows order (duplicate / malformed UIDs per table in table order,
    // parent cycles of areas / condition folders / page folders, takeoffs whose parent lies on another page, more than one BidSettings row
    // per bid, then the references that name a missing row, relationship by relationship).
    std::vector<IntegrityIssue> validate_raw_bid_integrity(const OstRawData &raw);
    std::vector<IntegrityIssue> validate_raw_bid_integrity(const OstRawData &raw, const std::vector<RawRelationship> &relationships);

    // format_integrity_issues: the first `limit` formatted and joined with "; ", then "; +N more".
    std::string format_integrity_issues(const std::vector<IntegrityIssue> &issues, size_t limit = 10);

    struct TakeoffGraphResolution
    {
        std::vector<IntegrityIssue> fatal_issues;
        std::vector<IntegrityIssue> missing_parent_roots;
        std::vector<IntegrityIssue> dependent_descendants;
        size_t skipped_count() const { return missing_parent_roots.size() + dependent_descendants.size(); }
    };

    // resolve_takeoff_graph: a malformed or duplicate takeoff UID, a malformed ParentUID or a ParentUID cycle is fatal (fatal_issues, the
    // data is left as it is). Takeoffs whose parent is missing are removed from raw.page_tables["BidTakeoffs"] together with all their
    // descendants (missing_parent_roots, dependent_descendants).
    TakeoffGraphResolution resolve_takeoff_graph(OstRawData &raw);

    // clear_missing_selected_page_references: BidSettings.BidPageSelectedUID that is a number (Unicode decimal digits count) but not the UID of a
    // page becomes "0". Returns what was cleared.
    std::vector<IntegrityIssue> clear_missing_selected_page_references(OstRawData &raw);

    // clear_missing_annotation_takeoff_references: BidTakeoffFromUID / BidTakeoffToUID of dimensions, arrows and lines that name a takeoff
    // that is not there become "0". Returns what was cleared.
    std::vector<IntegrityIssue> clear_missing_annotation_takeoff_references(OstRawData &raw);

    // ost_importer._validated_raw_data without the file parsing: resolves the takeoff graph, clears the references of the two kinds above,
    // discards the Named Views of a missing page with their Hot Links (only when nothing else is invalid) and rejects the import for
    // everything validate_raw_bid_integrity still finds. `raw` is repaired in place.
    //   false, `error` = the Windows ValueError text:
    //     "The imported takeoff graph is invalid: <issues>"
    //     "The imported project contains invalid references."   (the issues are logged: "Rejecting OST import because it contains invalid
    //                                                            database references: <issues>"; `details` holds them too)
    //     Python's int() limit message (a UID of more than 4300 digits)
    bool validate_raw_data(OstRawData &raw, std::string &error, std::string *details = nullptr);
}
