// Generated from the Windows module ost_visualizer/infrastructure/mdb/reference_schema_metadata.py (FIELD_DEFAULTS, read-only
// exec of the source with python -B; the 61 entries equal tests/fixtures/reference_mdb_schema.json "field_defaults"):
// the DefaultValue the Windows database creator sets on a field of every new database.
#pragma once

namespace ost_ptest
{
    struct FieldDefault
    {
        const char *table;
        const char *column;
        const char *value; // the DefaultValue text, e.g. "0"
    };

    static const FieldDefault kReferenceFieldDefaults[] = {
        {"BidALines", "Color", "0"},
        {"BidAnnoInk", "Color", "0"},
        {"BidAnnotationClouds", "Color", "0"},
        {"BidAnnotationOvals", "Color", "0"},
        {"BidAnnotationPolygons", "Color", "0"},
        {"BidAnnotationRects", "Color", "0"},
        {"BidAreas", "Sequence", "0"},
        {"BidArrows", "Color", "0"},
        {"BidCallOuts", "FontColor", "0"},
        {"BidCallOuts", "Color", "0"},
        {"BidComments", "Color", "0"},
        {"BidConditions", "Spacing", "0"},
        {"BidDimensions", "FontColor", "0"},
        {"BidHighlights", "Color", "0"},
        {"BidHotLinks", "BidPageViewUID", "0"},
        {"BidHotLinks", "Color", "0"},
        {"BidLaborActivity", "RuleValue", "0"},
        {"BidLaborCostCodes", "Type", "0"},
        {"BidLegends", "FontColor", "0"},
        {"BidPages", "DigitizerNX1", "0"},
        {"BidPages", "DigitizerNY1", "0"},
        {"BidPages", "DigitizerNX2", "0"},
        {"BidPages", "DigitizerNY2", "0"},
        {"BidPages", "DigitizerWidth", "0"},
        {"BidPages", "DigitizerHeight", "0"},
        {"BidPages", "DigitizerResX", "0"},
        {"BidPages", "DigitizerResY", "0"},
        {"Bids", "ExternalID", "0"},
        {"Bids", "HoursPerDay", "0"},
        {"Bids", "MeasureBase", "0"},
        {"Bids", "WeekStartDay", "0"},
        {"Bids", "DPCMode", "0"},
        {"Bids", "IsDPCUpdated", "0"},
        {"Bids", "IgnoreBidAreas", "0"},
        {"Bids", "SendImageFiles", "0"},
        {"Bids", "HasSTSClash", "0"},
        {"Bids", "SRPending", "0"},
        {"Bids", "IsUnlocked", "0"},
        {"Bids", "FullBidSent", "0"},
        {"BidTakeoffs", "Count", "0"},
        {"BidTakeoffs", "GridOffsetX", "0"},
        {"BidTakeoffs", "GridOffsetY", "0"},
        {"BidTakeoffs", "GridRotation", "0"},
        {"BidTakeoffs", "IsNegativeQuantity", "0"},
        {"BidTakeoffs", "FontColor", "0"},
        {"BidTakeoffs", "NameFontColor", "0"},
        {"BidTexts", "FontColor", "0"},
        {"BidTransactionsHistory", "TransactionID", "0"},
        {"BidTypAreas", "Sequence", "0"},
        {"CostCodes", "Sequence", "0"},
        {"CostCodes", "RuleValue", "0"},
        {"Settings", "QuantitiesInLegend", "0"},
        {"Settings", "HoursPerDay", "0"},
        {"Settings", "StartWeekOn", "0"},
        {"Settings", "GridCountMethod", "0"},
        {"Settings", "IgnoreBidAreas", "0"},
        {"Settings", "SendImageFiles", "0"},
        {"Settings", "BackupNo", "2"},
        {"Settings", "BackupPeriod", "2"},
        {"Settings", "CompressPeriod", "3"},
        {"STSTransactionHistory", "ChangeType", "0"},
    };
}
