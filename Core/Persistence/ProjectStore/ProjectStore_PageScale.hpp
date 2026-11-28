// Layer: Persistence
// Depends on: Domain (PageScaleTransform, OverlayRect, NumberText), OstDatabase
// Forbidden: UIKit, Foundation, Renderer
// Public declarations of the store functions of one M1 feature: Set Scale (plan 4.4, 5.4; owned by the stream S4).
//
// Windows source of truth:
//   ost_visualizer/infrastructure/mdb/components/page_operations.py:28-217       save_page_scale: scope check, stored scale, factor, overlay rectangle, the 17
//                                                                                tables in a fixed order, the final UPDATE of ScaleFactor1 / ScaleFactor2, one transaction
//   ost_visualizer/application/services/project_write_service.py:6306-6339       save_page_scale (the lock guard, one page)
//   ost_visualizer/application/services/project_write_service.py:6371-6433       save_page_scales (a list of pages, empty / repeated uids dropped)
//   ost_visualizer/application/services/project_write_service.py:6885-6892       _unique_nonempty_uids
// Deliberate differences from Windows (register SS-1, SS-2, SS-10, SS-11, SS-12; each pinned by a host test that says so):
//   - ALL-OR-NOTHING: the whole batch is ONE transaction; any refusal or failed statement rolls every page back (Windows pins a PARTIAL result with a
//     mocked writer: test_project_write_service.py:873-904);
//   - a page that is not a page of `bid_uid` (another bid, no bid) is refused (Windows derives the bid from the page and never compares it);
//   - a locked bid is refused INSIDE the transaction (plan 4.1);
//   - a factor / coordinate that is not finite and a non-zero coordinate that underflows to 0 refuse the whole change. Windows refuses a result that is not finite and,
//     in BidTakeoffs / BidComments / BidTypGroupViews, a non-zero coordinate stored as 0 at three decimals since 0a24294 (PageScaleRangeError; the same refusals here,
//     reason NonFiniteResult / Underflow); the exactly-zero underflow of an annotation table, the legend or the overlay rectangle is refused by iOS only (stricter);
//   - the legend XML is rewritten in place (PageScaleTransform.hpp).
// Kept from Windows: a stored NULL / 0 / negative / NaN / infinite scale refuses every Set Scale of the page, no repair (SS-1); a change whose factor is within
// 1e-9 of 1 writes the new pair and touches no content; a Position row that is not numeric is left as it is with a warning while the scale changes (SS-13);
// only ScaleFactor1 / ScaleFactor2 are written (ScaleStyle, IsCustomScale, Scale, Quantity, the grid offsets, the totals tables stay: SS-8, SS-9).
//
// STATE: implemented (Components/ProjectStore+PageScale.cpp); the host tests of Config/Tests/Persistence pin it.
#pragma once

#include <string>
#include <vector>

namespace ost_store
{
    // Why a Set Scale wrote nothing (None = it wrote).
    enum class PageScaleRefusal
    {
        None,            // written
        MalformedUid,    // the bid uid or a page uid is not a positive decimal integer (integrity::parse_uid)
        EmptyBatch,      // no page uid is left after the empty ones are dropped: refused WITHOUT opening the database
        NoSuchBid,       // the bid does not exist
        NoSuchPage,      // a page uid names no BidPages row
        ForeignBid,      // a page belongs to another bid, or to no bid (BidUID NULL)
        Locked,          // the bid is locked (job status Locked), or its status could not be resolved / read (fail closed)
        MissingColumns,  // BidPages lacks UID, BidUID, ScaleFactor1 or ScaleFactor2 (Windows _require_write_columns)
        BadStoredScale,  // the stored ScaleFactor1 / ScaleFactor2 of a page is NULL, 0, negative, NaN or infinite (units_per_sheet_inch is nullopt)
        BadNewScale,     // the new pair is not usable (units_per_sheet_inch is nullopt) or the ratio of the two ratios is not finite and > 0
        NonFiniteResult, // a stored or scaled coordinate / rectangle value is not finite
        Underflow,       // a non-zero coordinate became exactly 0.0, or (the three-decimal tables) would be stored as 0 at three decimals
        BadOverlayRect,  // OverlayRect is not four numbers / not finite / has a negative size
        BadLegend,       // a legend is not well-formed XML, has another root than Legends, a dX / dY that is not a number, or a non-finite result
        StatementFailed  // a statement failed (a failed read, a trigger that aborts an UPDATE, a busy database, a database that cannot be opened)
    };

    // Whether a refusal is the one Windows reports as PageScaleRangeError (page_operations.py:56-59): the takeoffs and annotations of a page cannot be stored at the new scale
    // (a value that is not finite, a takeoff coordinate that would be stored as 0, the iOS exactly-zero refusals). The dialog gives only these the specific text
    // (ost_domain::scale_save_failed_issue); every other reason is "Failed to save page scale.".
    inline bool page_scale_refusal_is_unstorable_content(PageScaleRefusal refusal)
    {
        return refusal == PageScaleRefusal::NonFiniteResult || refusal == PageScaleRefusal::Underflow;
    }

    struct PageScaleOutcome
    {
        PageScaleRefusal refusal = PageScaleRefusal::None;
        // Names the reason of every refusal (never empty when refusal != None). For BadStoredScale and BadNewScale it is the Windows text
        // "Page <uid> requires finite positive scale factors" (page_operations.py:75-77); for BadLegend / BadOverlayRect the Windows ValueError text.
        std::string error;
    };

    // Sets the scale of ONE OR MANY pages of the open bid to (sf1, sf2): every stored coordinate of those pages (the 17 position tables, the legend XML, the
    // overlay rectangle and its offsets) is multiplied by factor = new ratio / old ratio (units_per_sheet_inch of the new and the stored pair), then
    // BidPages.ScaleFactor1 / ScaleFactor2 get the new values exactly as given. ONE transaction for the whole list; any refusal or failed statement rolls
    // everything back (the database is byte-identical afterwards) and returns false with the reason in `outcome`.
    // Order inside the transaction (plan 4.4): parse the uids (a malformed one refuses the batch; empty uids are dropped, repeated uids count once, by value),
    // nothing left -> EmptyBatch without opening the database; open; bid exists; every page belongs to the bid; the bid lock; BidPages columns; then page
    // by page in the given order: stored scale, new scale, factor, overlay rectangle, tables (a Position row that is not numeric is skipped with a
    // ost_log warning "Skipping page-scale rescale for <table> UID <uid> because Position is not numeric"); finally the UPDATE of the scale columns and commit.
    // A table that is missing, or that lacks UID, BidPageUID or Position, is skipped silently (page_operations.py:162-169); a missing OverlayRect column skips the
    // rectangle, a missing OverlayOffsetX / OverlayOffsetY column skips that offset.
    // When |factor - 1| <= 1e-9 nothing but the two scale columns is written (page_operations.py:78-82, :44-47).
    bool save_page_scales(const std::string &db_path, const std::string &bid_uid, const std::vector<std::string> &page_uids, double sf1, double sf2,
                          PageScaleOutcome *outcome = nullptr);

    // = save_page_scales with the one page.
    bool save_page_scale(const std::string &db_path, const std::string &bid_uid, const std::string &page_uid, double sf1, double sf2,
                         PageScaleOutcome *outcome = nullptr);
}
