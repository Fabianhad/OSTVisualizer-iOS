// Layer: Domain. See PageScaleTransform.hpp. (rescale_legend_xml is in PageScaleLegend.cpp.)
// Windows source of truth: ost_visualizer/domain/services/page_scale_transform.py:1-68, domain/entities/overlay.py:7-19, domain/entities/annotation.py:20-27,
// infrastructure/database/annotation_storage.py:18-40, infrastructure/mdb/components/page_operations.py:57-217, serialization.py:18-51.
#include "PageScaleTransform.hpp"

#include "NumberText.hpp"
#include "OverlayRect.hpp"
#include "PositionParser.hpp"

#include <cmath>
#include <unordered_set>

namespace ost_domain
{
    namespace
    {
        bool usable_factor(double factor)
        {
            return std::isfinite(factor) && factor > 0.0;
        }

        // Python float(x or 0.0): a 0 (and a -0) becomes +0.0, anything else (a NaN too) is kept.
        double or_zero(double value)
        {
            return value == 0.0 ? 0.0 : value;
        }

        struct ScaledValues
        {
            PositionRescaleKind kind = PositionRescaleKind::Rescaled;
            std::vector<double> values;
        };

        // Every value times `factor`, except the preserved ones (the rotation slot of an annotation, kept exactly). A stored value that is not finite (preserved or not), a scaled value
        // that is not finite and a non-zero value that underflows to exactly 0.0 refuse (SS-2). `preserved(i)` says whether index i keeps its value.
        template <typename Preserved>
        ScaledValues scale_checked(const std::vector<double> &original, double factor, Preserved preserved)
        {
            ScaledValues result;
            for (const double value : original)
            {
                if (!std::isfinite(value))
                {
                    result.kind = PositionRescaleKind::NotFinite;
                    return result;
                }
            }
            result.values.resize(original.size());
            for (size_t i = 0; i < original.size(); ++i)
            {
                if (preserved(i))
                {
                    result.values[i] = original[i];
                    continue;
                }
                const double scaled = original[i] * factor;
                if (!std::isfinite(scaled))
                {
                    result.kind = PositionRescaleKind::NotFinite;
                    result.values.clear();
                    return result;
                }
                if (original[i] != 0.0 && scaled == 0.0)
                {
                    result.kind = PositionRescaleKind::Underflow;
                    result.values.clear();
                    return result;
                }
                result.values[i] = scaled;
            }
            return result;
        }
    }

    std::optional<double> units_per_sheet_inch(double sf1, double sf2)
    {
        if (!std::isfinite(sf1) || sf1 <= 0.0)
            return std::nullopt;
        const double ratio = sf2 / sf1;
        if (std::isfinite(ratio) && ratio > 0.0)
            return ratio;
        return std::nullopt;
    }

    double page_scale_ratio(double sf1, double sf2)
    {
        const double first = or_zero(sf1);
        const double second = or_zero(sf2);
        if (first == 0.0)
            return 1.0;
        return second / first;
    }

    double position_rescale_factor_between_page_scales(const std::optional<PageScale> &source, const std::optional<PageScale> &target)
    {
        const double source_ratio = source ? page_scale_ratio(source->sf1, source->sf2) : 1.0;
        const double target_ratio = target ? page_scale_ratio(target->sf1, target->sf2) : 1.0;
        if (source_ratio == 0.0)
            return 1.0;
        return target_ratio / source_ratio;
    }

    bool factor_needs_rescale(double factor, double epsilon)
    {
        return std::fabs(factor - 1.0) > epsilon;
    }

    std::vector<double> rescale_position(const std::vector<double> &position, double factor)
    {
        std::vector<double> scaled;
        scaled.reserve(position.size());
        for (const double value : position)
            scaled.push_back(value * factor);
        return scaled;
    }

    std::vector<double> rescale_position_between_page_scales(const std::vector<double> &position, const std::optional<PageScale> &source,
                                                             const std::optional<PageScale> &target)
    {
        const double factor = position_rescale_factor_between_page_scales(source, target);
        // abs(factor - 1.0) <= SCALE_EPSILON returns the copy; a NaN factor is NOT within the epsilon and so is multiplied (Python compares the same way).
        if (std::fabs(factor - 1.0) <= kScaleEpsilon)
            return position;
        return rescale_position(position, factor);
    }

    std::optional<AnnotationKind> annotation_kind_for_table(const std::string &table)
    {
        struct Row
        {
            const char *table;
            AnnotationKind kind;
        };
        static const Row rows[] = {
            {"BidALines", AnnotationKind::Line},          {"BidArrows", AnnotationKind::Arrow},        {"BidDimensions", AnnotationKind::Dimension},
            {"BidAnnotationClouds", AnnotationKind::Cloud}, {"BidAnnotationPolygons", AnnotationKind::Polygon}, {"BidAnnotationRects", AnnotationKind::Rect},
            {"BidAnnotationOvals", AnnotationKind::Oval}, {"BidAnnoInk", AnnotationKind::Ink},         {"BidTexts", AnnotationKind::Text},
            {"BidHighlights", AnnotationKind::Highlight}, {"BidNamedViews", AnnotationKind::NamedView}, {"BidHotLinks", AnnotationKind::Hotlink},
            {"BidCallOuts", AnnotationKind::Callout},
        };
        for (const Row &row : rows)
            if (table == row.table)
                return row.kind;
        return std::nullopt;
    }

    std::optional<std::size_t> annotation_rotation_index(AnnotationKind kind, const std::vector<double> &position)
    {
        const std::size_t length = position.size();
        if (kind == AnnotationKind::Ink)
            return length % 2 ? std::optional<std::size_t>(0) : std::nullopt;
        if (kind == AnnotationKind::Text)
            return length >= 5 ? std::optional<std::size_t>(4) : std::nullopt;
        return length % 2 ? std::optional<std::size_t>(length - 1) : std::nullopt;
    }

    std::vector<double> rescale_annotation_position_between_page_scales(AnnotationKind kind, const std::vector<double> &position,
                                                                        const std::optional<PageScale> &source, const std::optional<PageScale> &target)
    {
        std::vector<double> scaled = rescale_position_between_page_scales(position, source, target);
        const std::optional<std::size_t> index = annotation_rotation_index(kind, position);
        if (index)
        {
            if (kind == AnnotationKind::Text)
            {
                for (std::size_t i = *index; i < scaled.size() && i < position.size(); ++i)
                    scaled[i] = position[i];
            }
            else if (*index < scaled.size())
            {
                scaled[*index] = position[*index];
            }
        }
        return scaled;
    }

    std::optional<std::vector<double>> rescale_entry_positions(const std::vector<double> &positions, const std::optional<PageScale> &captured,
                                                               const PageScale &current)
    {
        // Nothing captured: factor 1, the positions come back as they are (NOT the pair (1, 1) of the Windows lookup).
        const double factor = captured ? position_rescale_factor_between_page_scales(captured, std::optional<PageScale>(current)) : 1.0;
        if (!usable_factor(factor))
            return std::nullopt;
        if (std::fabs(factor - 1.0) <= kScaleEpsilon)
            return positions;
        ScaledValues scaled = scale_checked(positions, factor, [](std::size_t) { return false; });
        if (scaled.kind != PositionRescaleKind::Rescaled)
            return std::nullopt;
        return scaled.values;
    }

    const std::vector<std::string> &page_position_tables()
    {
        static const std::vector<std::string> tables = {"BidTakeoffs",         "BidDimensions",   "BidALines",          "BidArrows",          "BidHighlights",
                                                        "BidTexts",            "BidCallOuts",     "BidAnnotationRects", "BidAnnotationOvals", "BidAnnotationPolygons",
                                                        "BidAnnotationClouds", "BidAnnoInk",      "BidLegends",         "BidComments",        "BidNamedViews",
                                                        "BidHotLinks",         "BidTypGroupViews"};
        return tables;
    }

    std::string encode_position_3dp(const std::vector<double> &position)
    {
        std::string text;
        for (std::size_t i = 0; i < position.size(); ++i)
        {
            if (i > 0)
                text += ';';
            text += format_fixed_trimmed(position[i], 3);
        }
        text += '\n';
        return text;
    }

    PositionRescale rescale_position_text(const std::string &table, const std::string &stored, double factor)
    {
        PositionRescale result;
        if (!usable_factor(factor))
        {
            result.kind = PositionRescaleKind::BadFactor;
            return result;
        }
        if (stored.empty())
        {
            result.kind = PositionRescaleKind::Empty;
            return result;
        }
        const std::vector<double> original = parse_position(stored);
        if (original.empty())
        {
            result.kind = PositionRescaleKind::NotNumeric;
            return result;
        }
        const std::optional<AnnotationKind> kind = annotation_kind_for_table(table);
        const std::optional<std::size_t> rotation = kind ? annotation_rotation_index(*kind, original) : std::nullopt;
        const bool keep_to_end = kind && *kind == AnnotationKind::Text;
        const ScaledValues scaled = scale_checked(original, factor,
                                                  [&](std::size_t i)
                                                  {
                                                      if (!rotation)
                                                          return false;
                                                      return keep_to_end ? i >= *rotation : i == *rotation;
                                                  });
        if (scaled.kind != PositionRescaleKind::Rescaled)
        {
            result.kind = scaled.kind;
            return result;
        }
        result.kind = PositionRescaleKind::Rescaled;
        // The 13 annotation tables keep every value exactly (str(float(v))), the others are written with three decimals.
        result.text = kind ? encode_position(scaled.values) : encode_position_3dp(scaled.values);
        return result;
    }

    OverlayRescale rescale_overlay_rect(const std::string &stored, double factor)
    {
        OverlayRescale result;
        if (!usable_factor(factor))
        {
            result.kind = OverlayRescaleKind::BadFactor;
            return result;
        }
        OverlayRect rect;
        std::string error;
        if (!parse_rect_storage(stored, rect, &error))
        {
            result.kind = OverlayRescaleKind::Malformed;
            result.error = error;
            return result;
        }
        // page_operations.py:104-105: a rectangle with a width or a height of 0 (the empty marker included) is not a real rectangle: nothing is written.
        if (rect.width <= 0.0 || rect.height <= 0.0)
        {
            result.kind = OverlayRescaleKind::Untouched;
            return result;
        }
        const double original[4] = {rect.x, rect.y, rect.width, rect.height};
        double scaled[4] = {0.0, 0.0, 0.0, 0.0};
        for (int i = 0; i < 4; ++i)
        {
            scaled[i] = original[i] * factor;
            if (!std::isfinite(scaled[i]))
            {
                result.kind = OverlayRescaleKind::NotFinite;
                return result;
            }
            if (original[i] != 0.0 && scaled[i] == 0.0)
            {
                result.kind = OverlayRescaleKind::Underflow;
                return result;
            }
        }
        OverlayRect scaled_rect;
        scaled_rect.x = scaled[0];
        scaled_rect.y = scaled[1];
        scaled_rect.width = scaled[2];
        scaled_rect.height = scaled[3];
        if (!serialize_rect_storage(scaled_rect, result.rect_text, &error))
        {
            result.kind = OverlayRescaleKind::Malformed;
            result.error = error;
            result.rect_text.clear();
            return result;
        }
        result.kind = OverlayRescaleKind::Rescaled;
        result.offset_x = scaled[0];
        result.offset_y = scaled[1];
        return result;
    }

    std::vector<std::string> apply_page_scales(std::unordered_map<std::string, Page> &pages, std::vector<Takeoff> &takeoffs,
                                               const std::vector<std::string> &page_uids, double sf1, double sf2)
    {
        if (page_uids.empty())
            return {};
        // Repeated uids count once, in the order given; a uid that is not a page refuses the whole call.
        std::vector<std::string> uids;
        std::unordered_set<std::string> seen;
        for (const std::string &uid : page_uids)
        {
            if (!seen.insert(uid).second)
                continue;
            if (pages.find(uid) == pages.end())
                return {};
            uids.push_back(uid);
        }
        const std::optional<double> target = units_per_sheet_inch(sf1, sf2);
        if (!target)
            return {};

        // Plan everything first: nothing of `pages` / `takeoffs` is touched until every page and every takeoff has been checked.
        struct PagePlan
        {
            std::string uid;
            double factor = 1.0;
            bool rewrite = false;
            OverlayRescale overlay;
        };
        std::vector<PagePlan> plans;
        std::unordered_map<std::string, std::size_t> plan_of;
        for (const std::string &uid : uids)
        {
            const Page &page = pages.at(uid);
            const std::optional<double> source = units_per_sheet_inch(page.scale_factor1, page.scale_factor2);
            if (!source)
                return {};
            PagePlan plan;
            plan.uid = uid;
            plan.factor = *target / *source;
            if (!usable_factor(plan.factor))
                return {};
            plan.rewrite = factor_needs_rescale(plan.factor);
            if (plan.rewrite)
            {
                plan.overlay = rescale_overlay_rect(page.overlay_rect, plan.factor);
                if (plan.overlay.kind != OverlayRescaleKind::Untouched && plan.overlay.kind != OverlayRescaleKind::Rescaled)
                    return {};
            }
            plan_of[uid] = plans.size();
            plans.push_back(plan);
        }
        std::vector<std::pair<std::size_t, std::vector<double>>> new_positions;
        for (std::size_t i = 0; i < takeoffs.size(); ++i)
        {
            const Takeoff &takeoff = takeoffs[i];
            if (takeoff.position.empty())
                continue;
            const auto found = plan_of.find(takeoff.page_uid);
            if (found == plan_of.end() || !plans[found->second].rewrite)
                continue;
            const ScaledValues scaled = scale_checked(takeoff.position, plans[found->second].factor, [](std::size_t) { return false; });
            if (scaled.kind != PositionRescaleKind::Rescaled)
                return {};
            // What a reload reads: the three-decimal text, parsed back.
            new_positions.emplace_back(i, parse_position(encode_position_3dp(scaled.values)));
        }

        for (const PagePlan &plan : plans)
        {
            Page &page = pages.at(plan.uid);
            page.scale_factor1 = sf1;
            page.scale_factor2 = sf2;
            if (plan.overlay.kind == OverlayRescaleKind::Rescaled)
            {
                page.overlay_rect = plan.overlay.rect_text;
                page.overlay_offset_x = plan.overlay.offset_x;
                page.overlay_offset_y = plan.overlay.offset_y;
            }
        }
        for (auto &entry : new_positions)
            takeoffs[entry.first].position = std::move(entry.second);
        return uids;
    }
}
