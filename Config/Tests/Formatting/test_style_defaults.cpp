// COND-METRIC-01: the metric defaults of a new condition style and the metric unit facts the dialog relies on.
//
// Windows oracle: presentation/dialogs/edit_condition_dialog.py TYPE_DEFAULTS (:97-150), _DEFAULT_COUNT_WIDTH_IN /
// _DEFAULT_COUNT_WIDTH_MM and _populate_defaults_for_type (:1255-1266):
//   metric:   spacing = mm_to_inches(spacing_metric), thickness = mm_to_inches(thickness_metric),
//             width = mm_to_inches(300.0), uom1 = uom1_metric
//   imperial: spacing, thickness, width = 12.0, uom1 straight from the table
// and domain/services/uom_service.py (labels, divisors, valid unit lists) with tests/domain/services/test_uom_service.py.
#include "TestHarness.hpp"

#include "Domain/ConditionStyleDefaults.hpp"
#include "Domain/DimensionFormat.hpp"
#include "Domain/QuantityDisplay.hpp"
#include "Domain/UomService.hpp"

using namespace ost_domain;

namespace
{
    // The Windows values after mm_to_inches (computed by running the Windows code: 100.0 / 25.4 etc.)
    constexpr double kMm100 = 3.937007874015748;
    constexpr double kMm150 = 5.905511811023622;
    constexpr double kMm300 = 11.811023622047244;
}

TEST(Metric01_StyleDefaults, MetricLinearDefaults)
{
    const StyleDimensionDefaults d = style_dimension_defaults(ConditionType::Linear, true);
    CHECK_EQ(d.spacing, kMm100);
    CHECK_EQ(d.thickness, kMm100);
    CHECK_EQ(d.width, kMm300);
    CHECK_EQ(d.uom1, 13); // m
}

TEST(Metric01_StyleDefaults, MetricAreaDefaults)
{
    const StyleDimensionDefaults d = style_dimension_defaults(ConditionType::Area, true);
    CHECK_EQ(d.spacing, kMm150);
    CHECK_EQ(d.thickness, kMm100);
    CHECK_EQ(d.width, kMm300);
    CHECK_EQ(d.uom1, 14); // m2
}

TEST(Metric01_StyleDefaults, MetricCountAndAttachmentDefaults)
{
    const StyleDimensionDefaults count = style_dimension_defaults(ConditionType::Count, true);
    CHECK_EQ(count.spacing, kMm100);
    CHECK_EQ(count.thickness, kMm100);
    CHECK_EQ(count.width, kMm300);
    CHECK_EQ(count.uom1, 0); // EA
    const StyleDimensionDefaults attachment = style_dimension_defaults(ConditionType::Attachment, true);
    CHECK_EQ(attachment.spacing, kMm100);
    CHECK_EQ(attachment.thickness, 0.0);
    CHECK_EQ(attachment.width, kMm300);
    CHECK_EQ(attachment.uom1, 0);
}

TEST(Metric01_StyleDefaults, ImperialDefaultsAreTheTableValues)
{
    const StyleDimensionDefaults linear = style_dimension_defaults(ConditionType::Linear, false);
    CHECK_EQ(linear.spacing, 4.0);
    CHECK_EQ(linear.thickness, 4.0);
    CHECK_EQ(linear.width, 12.0);
    CHECK_EQ(linear.uom1, 2); // LF
    const StyleDimensionDefaults area = style_dimension_defaults(ConditionType::Area, false);
    CHECK_EQ(area.spacing, 6.0); // Windows: Area spacing 6.0 (the TYPE_DEFAULTS row; a style change does not save it, COND-STYLE-02)
    CHECK_EQ(area.thickness, 4.0);
    CHECK_EQ(area.width, 12.0);
    CHECK_EQ(area.uom1, 5); // SF
    const StyleDimensionDefaults count = style_dimension_defaults(ConditionType::Count, false);
    CHECK_EQ(count.spacing, 4.0);
    CHECK_EQ(count.thickness, 4.0);
    CHECK_EQ(count.width, 12.0);
    CHECK_EQ(count.uom1, 0);
    const StyleDimensionDefaults attachment = style_dimension_defaults(ConditionType::Attachment, false);
    CHECK_EQ(attachment.spacing, 4.0);
    CHECK_EQ(attachment.thickness, 0.0);
    CHECK_EQ(attachment.width, 12.0);
    CHECK_EQ(attachment.uom1, 0);
}

TEST(Metric01_StyleDefaults, AnUnknownStyleFallsBackLikeTheWindowsTableMiss)
{
    // TYPE_DEFAULTS.get(type, {}) -> td.get("spacing", 4.0) / td.get("spacing_metric", 100.0) / uom1 0
    const ConditionType unknown = static_cast<ConditionType>(99);
    const StyleDimensionDefaults imperial = style_dimension_defaults(unknown, false);
    CHECK_EQ(imperial.spacing, 4.0);
    CHECK_EQ(imperial.thickness, 4.0);
    CHECK_EQ(imperial.width, 12.0);
    CHECK_EQ(imperial.uom1, 0);
    const StyleDimensionDefaults metric = style_dimension_defaults(unknown, true);
    CHECK_EQ(metric.spacing, kMm100);
    CHECK_EQ(metric.thickness, kMm100);
    CHECK_EQ(metric.width, kMm300);
    CHECK_EQ(metric.uom1, 0);
}

TEST(Metric01_StyleDefaults, TheMetricDefaultsReadBackAsWholeMillimetres)
{
    // the dialog shows inches_to_display(default, metric) and the user sees the table's millimetre values
    CHECK_STR_EQ(inches_to_display(style_dimension_defaults(ConditionType::Linear, true).spacing, true), "100");
    CHECK_STR_EQ(inches_to_display(style_dimension_defaults(ConditionType::Area, true).spacing, true), "150");
    CHECK_STR_EQ(inches_to_display(style_dimension_defaults(ConditionType::Count, true).width, true), "300");
    CHECK_STR_EQ(inches_to_display(style_dimension_defaults(ConditionType::Area, true).thickness, true), "100");
    CHECK_STR_EQ(inches_to_display(style_dimension_defaults(ConditionType::Count, false).width, false), "1' 0\"");
    CHECK_STR_EQ(inches_to_display(style_dimension_defaults(ConditionType::Linear, false).spacing, false), "4\"");
}

// ---------------------------------------------------------------------------------------------------------------------
// The unit facts of the item's plan (the Core unit table itself is QTY-05 / QTY-06; these pin that a metric bid has them)
// ---------------------------------------------------------------------------------------------------------------------

TEST(Metric01_MetricUnits, ConversionsLabelsAndListsMatchTheWindowsMetricBid)
{
    CHECK_EQ(convert_to_uom(100.0, 13), 100.0 / (1000.0 / 25.4)); // 2.54
    CHECK_NEAR(convert_to_uom(100.0, 13), 2.54, 1e-12);
    CHECK_NEAR(convert_to_uom(100.0, 11), 2540.0, 1e-9);
    CHECK_STR_EQ(get_uom_label(13), "m");
    CHECK_STR_EQ(get_uom_label(11), "mm");
    CHECK_STR_EQ(get_uom_label(14), "m\xC2\xB2");
    CHECK_STR_EQ(get_uom_label(15), "mm\xC2\xB3");

    const std::vector<UomOption> length = get_valid_uoms_for_calc_type(1, true);
    REQUIRE_EQ(length.size(), 2u);
    CHECK_EQ(length[0].first, 13);
    CHECK_STR_EQ(length[0].second, "m");
    CHECK_EQ(length[1].first, 11);
    CHECK_STR_EQ(length[1].second, "mm");

    const std::vector<UomOption> area = get_valid_uoms_for_calc_type(11, true);
    REQUIRE_EQ(area.size(), 2u);
    CHECK_EQ(area[0].first, 14);
    CHECK_STR_EQ(area[0].second, "m\xC2\xB2");
    CHECK_EQ(area[1].first, 12);
    CHECK_STR_EQ(area[1].second, "mm\xC2\xB2");

    const std::vector<UomOption> volume = get_valid_uoms_for_calc_type(20, true);
    REQUIRE_EQ(volume.size(), 2u);
    CHECK_EQ(volume[0].first, 16);
    CHECK_STR_EQ(volume[0].second, "m\xC2\xB3");
    CHECK_EQ(volume[1].first, 15);
    CHECK_STR_EQ(volume[1].second, "mm\xC2\xB3");
}

TEST(Metric01_MetricUnits, AMetricConditionShowsMetresNotRawInches)
{
    // item plan: iOS showed the raw inches (100.0) for a metric length; 100 inches are 2.54 m and 2540 mm
    CHECK_STR_EQ(format_quantity_with_uom(convert_to_uom(100.0, 13), 13), "2.54 m");
    CHECK_STR_EQ(format_quantity_with_uom(convert_to_uom(100.0, 11), 11), "2,540 mm");
    CHECK_STR_EQ(format_quantity_with_uom(convert_to_uom(100.0, 2), 2), "8 LF"); // 8.333... feet, no decimals
}
