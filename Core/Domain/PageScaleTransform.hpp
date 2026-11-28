// Layer: Domain (pure C++)
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The page-scale transform of Set Scale (plan 4.10 and 5.4): the two ratio rules, the factor, the rescale of every stored coordinate text
// (the 17 position tables, the XML legends, the overlay rectangle) and the arithmetic of the undo replay. Windows source of truth:
//   ost_visualizer/domain/services/page_scale_transform.py:1-68              page_scale_ratio, position_rescale_factor_between_page_scales,
//                                                                            rescale_position_values / _between_page_scales, annotation twin
//   ost_visualizer/domain/entities/overlay.py:7-19                           overlay_units_per_sheet_inch (the validity rule of the database path)
//   ost_visualizer/domain/entities/annotation.py:20-27                       annotation_rotation_index
//   ost_visualizer/infrastructure/database/annotation_storage.py:18-40       ANNOTATION_TYPE_BY_TABLE (the 13 exact-value tables)
//   ost_visualizer/infrastructure/mdb/components/page_operations.py:57-217   the factor, the overlay rectangle, the 17 tables in a fixed order
//   ost_visualizer/infrastructure/mdb/components/serialization.py:18-51      the text forms of a stored position
//   ost_visualizer/infrastructure/mdb/components/legend_position.py:6-26     rescale_legend_position
//   ost_visualizer/infrastructure/mdb/components/overlay_rect.py:17-48       the overlay rectangle text
//   ost_visualizer/domain/services/project_data_service.py:358-418           apply_page_scales (the memory projection, see apply_page_scales below)
//
// TWO ratio rules, one per Windows contract (the two differ on purpose, plan 4.10, register SS-4):
//   units_per_sheet_inch                        STRICT (overlay.py:7-19): nullopt unless sf1 is finite and > 0 and sf2 / sf1 is finite and > 0.
//                                               Used by the store (a stored or new scale that is not usable refuses the whole change) and by the overlay rect.
//   position_rescale_factor_between_page_scales LENIENT (page_scale_transform.py:23-30): an sf1 of 0 (or a missing scale) has ratio 1.0, a source ratio of
//                                               0 gives the factor 1.0. Used by the undo replay (and by M2's paste redo); its CALLER refuses a factor that is
//                                               not finite and > 0 (rescale_entry_positions does).
// Deliberate differences from Windows (register SS-2, SS-10, SS-13), each pinned by a test that says so:
//   - a factor, a coordinate or a result that is not finite, and a non-zero coordinate that underflows to 0.0, REFUSE the change instead of writing "inf" /
//     "nan" text or destroying the value. Windows refuses them too since 0a24294 (require_storable_scaled_values, PageScaleRangeError): a result that is not finite
//     everywhere, and in the takeoff-like tables (written with three decimals) a non-zero coordinate whose three-decimal text is "0" ("-0"; Python round(x, 3) == 0).
//     iOS is STRICTER for the 13 annotation tables, the legend and the overlay rectangle: an exactly-zero underflow refuses there too (Windows: finiteness only);
//   - the legend XML is rewritten IN PLACE (only the dX / dY values change, the rest of the text is kept byte for byte, UTF-8) instead of being
//     re-serialised through xml.etree (latin-1, `<Legend ... />`): the seven Windows legend oracles that expect the xml.etree spelling are adapted.
//
// STATE: implemented (PageScaleTransform.cpp; rescale_legend_xml is in PageScaleLegend.cpp). The declarations are the contract the host tests of
// Config/Tests/Geometry, Persistence and Settings pin.
#pragma once

#include "Page.hpp"
#include "Takeoff.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_domain
{
    // |factor - 1| must be GREATER than this for a change of scale to rewrite content (page_scale_transform.py:6, page_operations.py:78-82).
    inline constexpr double kScaleEpsilon = 1e-9;

    // One page scale: ScaleFactor1 (drawing side), ScaleFactor2 (real side).
    struct PageScale
    {
        double sf1 = 1.0;
        double sf2 = 1.0;
    };

    // ---- the two ratio rules ----------------------------------------------------------------------------------------------------------

    // overlay.py:7-19 (strict). 0.1875, 12 -> 64; 0.125, 12 -> 96; 0.3, 15 -> 50; nullopt for sf1 <= 0, sf2 <= 0, NaN, an infinity, and a ratio that
    // overflows to infinity or underflows to 0 (1e-320 over 1e300 is fine for sf1, 1e300 / 1e-320 is not).
    std::optional<double> units_per_sheet_inch(double sf1, double sf2);

    // page_scale_transform.py:9-20, the lenient ratio of a pair whose missing parts are 0 (Python `float(x or 0.0)`): sf1 == 0 -> 1.0, else sf2 / sf1
    // (a missing sf2 collapses the ratio to 0, not to 1). (0.125, 12) -> 96, (2, 1) -> 0.5, (0, 12) -> 1, (1, 0) -> 0.
    double page_scale_ratio(double sf1, double sf2);

    // page_scale_transform.py:23-30: target ratio / source ratio, 1.0 when the source ratio is 0. A missing scale (nullopt) is the pair (0, 0): ratio 1.0.
    // ((1,96) -> (1,48)) 0.5, (nullopt -> (1,2)) 2, ((1,2) -> nullopt) 0.5, (nullopt -> nullopt) 1, ((0,12) -> (0.25,12)) 48, ((1,0) -> (1,48)) 1,
    // ((1,96) -> (1,0)) 0. Not validated: the result may be 0, infinite or NaN; the caller decides.
    double position_rescale_factor_between_page_scales(const std::optional<PageScale> &source, const std::optional<PageScale> &target);

    // |factor - 1| > epsilon, EXCLUSIVE (page_operations.py:78-82, the test with the constant patched to 0.5 and a factor of exactly 1.5:
    // test_page_operations.py:1957-1977). false for a NaN factor.
    bool factor_needs_rescale(double factor, double epsilon = kScaleEpsilon);

    // ---- positions as numbers ---------------------------------------------------------------------------------------------------------

    // page_scale_transform.py:33-40 for numbers: every value times factor (no rounding).
    std::vector<double> rescale_position(const std::vector<double> &position, double factor);

    // :43-51: a COPY unchanged when |factor - 1| <= kScaleEpsilon (factor from the two scales), else rescale_position.
    std::vector<double> rescale_position_between_page_scales(const std::vector<double> &position, const std::optional<PageScale> &source,
                                                             const std::optional<PageScale> &target);

    // The 13 annotation kinds (annotation.py:5-17) and their tables (annotation_storage.py:18-40, ANNOTATION_TABLE_BY_TYPE).
    enum class AnnotationKind
    {
        Arrow,     // BidArrows
        Callout,   // BidCallOuts
        Cloud,     // BidAnnotationClouds
        Dimension, // BidDimensions
        Highlight, // BidHighlights
        Hotlink,   // BidHotLinks
        Ink,       // BidAnnoInk
        Line,      // BidALines
        NamedView, // BidNamedViews
        Oval,      // BidAnnotationOvals
        Polygon,   // BidAnnotationPolygons
        Rect,      // BidAnnotationRects
        Text       // BidTexts
    };

    // The kind of one of the 13 exact-value tables; nullopt for every other table name (BidTakeoffs, BidComments, BidTypGroupViews, BidLegends, ...).
    std::optional<AnnotationKind> annotation_kind_for_table(const std::string &table);

    // annotation.py:20-27: the index of the rotation value that a rescale keeps: Ink -> 0 when the length is odd; Text -> 4 when the length is >= 5;
    // every other kind -> the last index when the length is odd; nullopt otherwise.
    std::optional<std::size_t> annotation_rotation_index(AnnotationKind kind, const std::vector<double> &position);

    // page_scale_transform.py:54-68: rescale_position_between_page_scales, then the rotation value is put back (Text: every value from index 4 to the end).
    std::vector<double> rescale_annotation_position_between_page_scales(AnnotationKind kind, const std::vector<double> &position,
                                                                        const std::optional<PageScale> &source,
                                                                        const std::optional<PageScale> &target);

    // ---- undo / redo replay (plan 4.7, N-M1-06) ---------------------------------------------------------------------------------------------

    // The positions of an undo / redo entry at the page scale that is current NOW. `captured` is the pair the entry stored when it was created (nullopt =
    // nothing was captured: the positions come back unchanged, factor 1, NOT the pair (1, 1) that Windows would read). The factor is
    // position_rescale_factor_between_page_scales(captured, current); a factor that is not finite or not > 0 refuses (nullopt: the undo / redo does not
    // run), and so does a result that is not finite (a coordinate that overflows, or a stored NaN / infinity) or whose non-zero coordinate underflows to exactly 0.0 (the
    // SS-2 hardening of the store); a factor within kScaleEpsilon of 1 returns the positions unchanged (an infinite or NaN stored value is then not looked at). The
    // result is NOT rounded (the replay writes the exact repr like every takeoff write, unlike the three-decimal text of the store). Pinned scenario: test_plan_view_action_handler.py:3849.
    std::optional<std::vector<double>> rescale_entry_positions(const std::vector<double> &positions, const std::optional<PageScale> &captured,
                                                               const PageScale &current);

    // ---- stored text forms ------------------------------------------------------------------------------------------------------------

    // The 17 tables whose Position the change rewrites, in the order Windows rewrites them (constants.py:83-121 PAGE_CONTENT_TABLES + "BidTypGroupViews",
    // page_operations.py:26): BidTakeoffs, BidDimensions, BidALines, BidArrows, BidHighlights, BidTexts, BidCallOuts, BidAnnotationRects,
    // BidAnnotationOvals, BidAnnotationPolygons, BidAnnotationClouds, BidAnnoInk, BidLegends, BidComments, BidNamedViews, BidHotLinks, BidTypGroupViews.
    const std::vector<std::string> &page_position_tables();

    // The three-decimal text of a stored takeoff / comment / typical-group position (serialization.py:18-28, encode_position without preserved indices):
    // each value format_fixed_trimmed(v, 3), joined by ';', plus "\n". {0.5, 1, 1.5, 2} -> "0.5;1;1.5;2\n". An empty position gives "\n".
    std::string encode_position_3dp(const std::vector<double> &position);

    // What rescaling one stored Position text did.
    enum class PositionRescaleKind
    {
        Empty,      // the stored text is empty: nothing to do, no warning (page_operations.py:177-178 `if not raw_position`)
        NotNumeric, // parse_position gives nothing (a bad token, only blanks): the row is LEFT as it is and Windows warns (SS-13, parity)
        Rescaled,   // `text` is the new stored text
        NotFinite,  // a stored or a scaled coordinate is NaN / infinite: the whole change is refused (SS-2, hardening)
        Underflow,  // a non-zero coordinate became exactly 0.0 by the multiplication, or (BidTakeoffs, BidComments, BidTypGroupViews: the tables written with three
                    // decimals) a non-zero coordinate whose three-decimal text is "0" / "-0": the whole change is refused (SS-2; Windows 0a24294 for the second case)
        BadFactor   // the factor is not finite or not > 0 (the store never passes one: it refuses the change first)
    };
    struct PositionRescale
    {
        PositionRescaleKind kind = PositionRescaleKind::Empty;
        std::string text;
    };

    // page_operations.py:154-217 for one row of any table except BidLegends: parse_position, every value times factor, the rotation slot of the 13
    // annotation tables put back (annotation_rotation_index; BidTexts: from index 4 to the end), then the text form of the table: the 13 annotation tables
    // write EVERY value as Python str(float(v)) (ost_domain::encode_position: "0.5;1.0;1.5;2.0\n", no rounding), BidTakeoffs / BidComments /
    // BidTypGroupViews write encode_position_3dp. `factor` must be finite and > 0 (a different one is BadFactor, nothing is parsed).
    PositionRescale rescale_position_text(const std::string &table, const std::string &stored, double factor);

    // legend_position.py:6-26 with the IN-PLACE rewrite (SS-10): the root element must be Legends; every Legends / Legend element (any depth, exact names,
    // not inside a comment, a CDATA section or an attribute value) has its dX and dY attributes (only when present) replaced by format_fixed_trimmed(
    // value * factor, 3), the quote character kept; nothing else changes (no normalisation, UTF-8 kept, comments / processing instructions / text kept).
    // Any finite factor is accepted at this level, a zero or negative one included (test_legend_position.py:96-117 pins 4 x -0.5 -> -2, :169-171 pins
    // x 0.0 -> 0); the store only ever passes a finite factor > 0.
    // false (and `out` untouched) when: the text is not well-formed XML (an empty text is not), the root is not Legends (error
    // "Legend Position requires a Legends XML root"), an attribute value is not a Python float (error names the value), a scaled value is not finite
    // (error "Legend Position coordinates must be finite"; also a non-finite stored value and a non-finite factor), or a non-zero stored value becomes
    // exactly 0.0 although the factor is not 0.0 (underflow, SS-2 hardening; error "Legend Position coordinates underflow").
    bool rescale_legend_xml(const std::string &xml, double factor, std::string &out, std::string *error = nullptr);

    // The overlay rectangle of a page (overlay_rect.py:17-48, page_operations.py:84-128), stored as "x,y,width,height".
    // What a Set Scale did to it:
    enum class OverlayRescaleKind
    {
        Untouched, // empty / "*" (the native empty marker) / a rectangle whose width or height is 0: nothing is written (the offsets stay)
        Rescaled,  // `rect_text` (the four values times factor, "%.6f,%.6f,%.6f,%.6f") and the offsets (the scaled x and y) are to be written
        Malformed, // not four numbers, not numeric, not finite, or a negative width / height: the whole change is refused (parity: Windows raises too)
        NotFinite, // a scaled value is not finite: refused (hardening)
        Underflow, // a non-zero value became exactly 0.0: refused (hardening)
        BadFactor  // the factor is not finite or not > 0 (the store never passes one)
    };
    struct OverlayRescale
    {
        OverlayRescaleKind kind = OverlayRescaleKind::Untouched;
        std::string rect_text;
        double offset_x = 0.0;
        double offset_y = 0.0;
        std::string error; // the Windows ValueError text for Malformed
    };
    OverlayRescale rescale_overlay_rect(const std::string &stored, double factor);

    // ---- the memory twin (plan 5.4, 4.11) ---------------------------------------------------------------------------------------------------

    // Windows apply_page_scales for the model iOS keeps (pages and takeoffs; there are no annotations in M1), written like the STORE, not like Windows
    // project_data_service.py:358-418 (register SS-4): takeoff positions get the three-decimal text round trip (the value a reload of the written text
    // gives), the overlay rect text and BOTH offsets are taken from the scaled rectangle, so that memory EQUALS a reload. Refuses (returns {}, nothing
    // changes) when `page_uids` is empty or names a uid that is not in `pages`, and for every reason the store refuses (the page's scale pair or the new pair
    // is not usable, the factor / a coordinate / the rectangle fails). Repeated uids count once. Returns the uids changed, in the order given.
    // ProjectData::apply_page_scales is a one-line wrapper of this function (plan 5.3; ProjectData.hpp is not a file of this stream's first phase).
    std::vector<std::string> apply_page_scales(std::unordered_map<std::string, Page> &pages, std::vector<Takeoff> &takeoffs,
                                               const std::vector<std::string> &page_uids, double sf1, double sf2);
}
