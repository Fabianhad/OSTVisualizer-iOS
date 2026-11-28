// Layer: Domain (pure C++)
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The stored form of a page's overlay rectangle, "x,y,width,height" (BidPages.OverlayRect). Windows source of truth:
//   ost_visualizer/infrastructure/mdb/components/overlay_rect.py:8-48   EMPTY_OVERLAY_RECT, the "*" marker, _validated_overlay_rect,
//                                                                       parse_overlay_rect_storage, serialize_overlay_rect_storage ("%.6f")
// Created by Set Scale (M1, plan 5.4: the rectangle is rescaled with the page); M4 (5.15) extends this pair of files.
//
// STATE: implemented (OverlayRect.cpp); the host tests of Config/Tests/Geometry pin it.
#pragma once

#include <string>

namespace ost_domain
{
    // The rectangle: the position of its corner and its size, in the units of the overlay (OST units; see units_per_sheet_inch).
    struct OverlayRect
    {
        double x = 0.0;
        double y = 0.0;
        double width = 0.0;
        double height = 0.0;
    };

    // The native "no overlay rectangle" marker the Windows app stores ("*").
    inline constexpr const char *kEmptyOverlayRectMarker = "*";

    // overlay_rect.py:33-41. An empty text, a text of only whitespace and the marker "*" give (0, 0, 0, 0) and true (a NULL cell is read as ""). Otherwise
    // the text is split on ',' and each part is strip()ped and read as Python float(): exactly four parts, all numeric, all finite (NaN / infinity refused),
    // width and height not negative. false (`out` untouched) otherwise, and `error` (when given) is the Windows ValueError text:
    //   "OverlayRect must contain exactly four values"            wrong number of parts
    //   "OverlayRect values must be numeric"                      a part that float() refuses (an empty part included)
    //   "OverlayRect values must be finite"                       NaN or an infinity
    //   "OverlayRect width and height cannot be negative"
    bool parse_rect_storage(const std::string &stored, OverlayRect &out, std::string *error = nullptr);

    // overlay_rect.py:44-48: the four values as f"{v:.6f}" (format_fixed(v, 6)) joined by ','. "-0.000000" is kept for a negative zero and for a tiny
    // negative value, as Python does. false (`out` untouched) with the same errors as above when a value is not finite or the size is negative.
    bool serialize_rect_storage(const OverlayRect &rect, std::string &out, std::string *error = nullptr);
}
