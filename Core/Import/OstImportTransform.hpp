// Layer: Import (pure C++)
// UID re-mapping of a parsed OST file into the target database: every imported row gets a fresh UID above the
// target's UID floor, bid-internal references are re-pointed, master-data references go through the reconciliation
// maps or become NULL, and the BidPageSettings rows are canonicalised (one selected area per page).
//
// Mirrors the Windows importer (ost_visualizer/infrastructure/mdb/importers/ost_importer.py _transform,
// _build_uid_map, _build_page_uid_map, _remap_uid_value, _remap_row, _remap_data) and
// ost_visualizer/infrastructure/database/page_area_selection.py. A UID value of "NULL" in a row means SQL NULL
// (the ProjectStore writer converts it), exactly like the Windows placeholder.
#pragma once
#include "OstXmlParser.hpp"
#include <string>
#include <unordered_map>

namespace ost_import
{
    using UidMap = std::unordered_map<std::string, std::string>;

    // The six maps the Windows writer hands to the transform.
    struct RemapMaps
    {
        UidMap uid;          // bid-internal: source UID -> new UID (build_uid_map)
        UidMap page;         // BidPages only: source UID -> new UID (build_page_uid_map)
        UidMap cdn_type;     // CdnTypes reconciliation
        UidMap job_status;   // JobStatuses reconciliation
        UidMap employee;     // Employees reconciliation
        UidMap pay_class;    // PayClasses reconciliation
    };

    // New UIDs max_uid+1, +2, ... in this order: the bid row, every bid table in insertion order, every row in file
    // order, then every page table in first-seen order. A row whose UID is missing, empty or "0" gets none; a UID that
    // repeats shares the one already assigned.
    UidMap build_uid_map(const OstRawData &raw_data, long long max_uid);

    // BidPages rows only: trimmed non-empty non-"0" source UID that has an entry in uid_map.
    UidMap build_page_uid_map(const OstRawData &raw_data, const UidMap &uid_map);

    // One attribute value. See ost_importer._remap_uid_value for the rule order.
    std::string remap_uid_value(const std::string &field, const std::string &value, const RemapMaps &maps);

    Row remap_row(const Row &row, const RemapMaps &maps);

    // Remaps the bid row, every bid table and every page table (never mutates raw_data), then canonicalises
    // BidPageSettings. The result's BidPageSettings key always exists (possibly empty). global_tables stay empty.
    OstRawData remap_data(const OstRawData &raw_data, const RemapMaps &maps);

    // Windows OstImporter._transform: build_uid_map + build_page_uid_map + remap_data.
    OstRawData transform(const OstRawData &raw_data,
                         long long max_uid,
                         const UidMap &cdn_uid_map,
                         const UidMap &job_status_uid_map,
                         const UidMap &employee_uid_map,
                         const UidMap &pay_class_uid_map);

    // Windows canonicalize_page_area_settings: inactive rows (BidAreaSelected <= 0, non-numeric counts as 0) first in
    // their original order, then exactly one selected row per page group (BidPageUID "", "0", "NULL" or absent is one
    // no-page group). The winner is the maximum (BidAreaSelected, UID) and on a tie the later row wins.
    RowList canonicalize_page_area_settings(const RowList &rows);
}
