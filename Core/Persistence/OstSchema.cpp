#include "OstSchema.hpp"

namespace ost_schema
{

    const std::vector<const char *> TABLE_DDL = {

        "CREATE TABLE IF NOT EXISTS AccessLevels ("
        "UID INTEGER PRIMARY KEY, "
        "Description TEXT, "
        "Privileges INTEGER)",

        "CREATE TABLE IF NOT EXISTS AffectDPCTypGroupViews ("
        "UID INTEGER PRIMARY KEY, "
        "BidTypGroupViewUID INTEGER, "
        "BidUID INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidALines ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidTakeoffFromUID INTEGER, "
        "BidTakeoffToUID INTEGER, "
        "Position BLOB, "
        "Color INTEGER DEFAULT 0, "
        "Width INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidAnnoInk ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "Color INTEGER DEFAULT 0, "
        "Position BLOB, "
        "Width INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidAnnotationClouds ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidLayerUID INTEGER, "
        "Color INTEGER DEFAULT 0, "
        "Position BLOB, "
        "Width INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidAnnotationOvals ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidLayerUID INTEGER, "
        "Color INTEGER DEFAULT 0, "
        "Position BLOB, "
        "Width INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidAnnotationPolygons ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidLayerUID INTEGER, "
        "Color INTEGER DEFAULT 0, "
        "Position BLOB, "
        "Width INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidAnnotationRects ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidLayerUID INTEGER, "
        "Color INTEGER DEFAULT 0, "
        "Position BLOB, "
        "Width INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidAreaTranslations ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "MasterAreaUID INTEGER, "
        "TranslateAreaUID INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidAreas ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "ParentUID INTEGER, "
        "Name TEXT, "
        "Sequence INTEGER DEFAULT 0, "
        "WasSent INTEGER, "
        "GUID TEXT)",

        "CREATE TABLE IF NOT EXISTS BidArrows ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidTakeoffFromUID INTEGER, "
        "BidTakeoffToUID INTEGER, "
        "Position BLOB, "
        "Color INTEGER DEFAULT 0, "
        "Width INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidCallOuts ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidLayerUID INTEGER, "
        "Name BLOB, "
        "FontName TEXT, "
        "FontColor INTEGER DEFAULT 0, "
        "FontSize INTEGER, "
        "FontBold INTEGER, "
        "FontItalic INTEGER, "
        "FontUnderline INTEGER, "
        "TextAlign INTEGER, "
        "Position TEXT, "
        "Color INTEGER DEFAULT 0, "
        "Width INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidComments ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "ParentCommentUID INTEGER, "
        "BidLayerUID INTEGER, "
        "Color INTEGER DEFAULT 0, "
        "UserName TEXT, "
        "Comment BLOB, "
        "Position TEXT, "
        "CommentModified TEXT)",

        "CREATE TABLE IF NOT EXISTS BidConditionFolders ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "ParentUID INTEGER, "
        "Name TEXT, "
        "Description BLOB, "
        "ExpandState INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidConditionUser ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "UserUID INTEGER, "
        "ConditionUID INTEGER, "
        "GUID TEXT, "
        "VALUE TEXT, "
        "Sequence INTEGER, "
        "StyleUID INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidConditions ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidConditionFolderUID INTEGER, "
        "BidLayerUID INTEGER, "
        "CdnTypeUID INTEGER, "
        "GUID TEXT, "
        "IsTemplate INTEGER, "
        "ExternalID INTEGER, "
        "RefNo INTEGER, "
        "Name TEXT, "
        "Notes BLOB, "
        "Type INTEGER, "
        "Shape INTEGER, "
        "Pattern INTEGER, "
        "ColorLine INTEGER, "
        "ColorFill INTEGER, "
        "Width REAL, "
        "Height REAL, "
        "Spacing REAL DEFAULT 0, "
        "Thickness REAL, "
        "Rise REAL, "
        "Run REAL, "
        "Depth REAL, "
        "UOM1 INTEGER, "
        "UOM2 INTEGER, "
        "UOM3 INTEGER, "
        "RoundUp REAL, "
        "Backout INTEGER, "
        "DropRun INTEGER, "
        "DropValue REAL, "
        "Grid INTEGER, "
        "GridSize1 REAL, "
        "GridSize2 REAL, "
        "Gap REAL, "
        "Connect INTEGER, "
        "ConnectTolerance REAL, "
        "Trim INTEGER, "
        "Curve INTEGER, "
        "SnapToGrid INTEGER, "
        "SnapToLinear INTEGER, "
        "SnapToLinearTolerance REAL, "
        "ManualLength INTEGER, "
        "MatAmount REAL, "
        "LabAmount REAL, "
        "SubAmount REAL, "
        "DirectQuantity1 REAL, "
        "DirectQuantity2 REAL, "
        "DirectQuantity3 REAL, "
        "FontName TEXT, "
        "FontSize INTEGER, "
        "FontBold INTEGER, "
        "FontItalic INTEGER, "
        "DisplayDimension INTEGER, "
        "RoundQuantity INTEGER, "
        "Quantity1 INTEGER, "
        "Quantity2 INTEGER, "
        "Quantity3 INTEGER, "
        "ExcelCell1 TEXT, "
        "ExcelCell2 TEXT, "
        "ExcelCell3 TEXT, "
        "IsCurvedSegment INTEGER, "
        "OpenExternalID INTEGER, "
        "DisplayGridWhileDrawing INTEGER, "
        "TypGroupUID INTEGER, "
        "ShowTakeoff INTEGER, "
        "BFperLF REAL, "
        "DisplaySize INTEGER, "
        "DisplayName INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidDPCSubscribers ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidEmployeeUID INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidDimensions ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidTakeoffFromUID INTEGER, "
        "BidTakeoffToUID INTEGER, "
        "Position BLOB, "
        "FontName TEXT, "
        "FontColor INTEGER DEFAULT 0, "
        "FontSize INTEGER, "
        "FontBold INTEGER, "
        "FontItalic INTEGER, "
        "FontUnderline INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidEmployees ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "EmployeeUID INTEGER, "
        "PayClassUID INTEGER, "
        "DateAdded TEXT, "
        "IsActive INTEGER, "
        "Notes TEXT, "
        "GUID TEXT)",

        "CREATE TABLE IF NOT EXISTS BidHighlights ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidLayerUID INTEGER, "
        "Color INTEGER DEFAULT 0, "
        "Position BLOB)",

        "CREATE TABLE IF NOT EXISTS BidHotLinks ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidPageViewUID INTEGER DEFAULT 0, "
        "BidLayerUID INTEGER, "
        "Color INTEGER DEFAULT 0, "
        "Position BLOB)",

        "CREATE TABLE IF NOT EXISTS BidLaborActivity ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidConditionUID INTEGER, "
        "BidLaborCostCodeUID INTEGER, "
        "Quantity REAL, "
        "Hours REAL, "
        "Sequence INTEGER, "
        "IsActive INTEGER, "
        "UsesConditionQuantity INTEGER, "
        "RuleType TEXT, "
        "RuleValue INTEGER DEFAULT 0, "
        "UseQtyIndex INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidLaborCostCodeTotals ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidAreaUID INTEGER, "
        "BidLaborCostCodeUID INTEGER, "
        "Date TEXT, "
        "InstalledQty REAL, "
        "CompletedHrs REAL)",

        "CREATE TABLE IF NOT EXISTS BidLaborCostCodes ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "Type INTEGER DEFAULT 0, "
        "CostCodeUID INTEGER, "
        "UOM INTEGER, "
        "IsActive INTEGER, "
        "GUID TEXT, "
        "Notes BLOB)",

        "CREATE TABLE IF NOT EXISTS BidLayers ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "IsTemplate INTEGER, "
        "Name TEXT, "
        "Show INTEGER, "
        "IsLocked INTEGER, "
        "Sequence INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidLegends ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "Position BLOB, "
        "Rotation REAL, "
        "FontName TEXT, "
        "FontColor INTEGER DEFAULT 0, "
        "FontSize INTEGER, "
        "FontBold INTEGER, "
        "FontItalic INTEGER, "
        "FontUnderline INTEGER, "
        "IsShowTotals INTEGER, "
        "MoveToCorner INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidMarkedPages ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "MarkTakeoffComplete INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidNamedViews ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "Name TEXT, "
        "Position BLOB, "
        "Color INTEGER, "
        "Origin INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidNotes ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "DateCreated TEXT, "
        "Notes BLOB, "
        "GUID TEXT)",

        "CREATE TABLE IF NOT EXISTS BidPageFolders ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "ParentUID INTEGER, "
        "Name TEXT, "
        "Description BLOB, "
        "WasSent INTEGER, "
        "GUID TEXT, "
        "ExpandState INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidPageSettings ("
        "UID INTEGER PRIMARY KEY, "
        "BidPageUID INTEGER, "
        "BidAreaUID INTEGER, "
        "BidTypAreaUID INTEGER, "
        "BidAreaSelected INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidPages ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageFolderUID INTEGER, "
        "Name TEXT, "
        "ImagePath TEXT, "
        "OverlayImagePath TEXT, "
        "RasterDrawMethod INTEGER, "
        "Show INTEGER, "
        "ScaleStyle INTEGER, "
        "IsCustomScale INTEGER, "
        "ScaleFactor1 REAL, "
        "ScaleFactor2 REAL, "
        "Scale REAL, "
        "Width REAL, "
        "Height REAL, "
        "ZoomFac REAL, "
        "CurrentX REAL, "
        "CurrentY REAL, "
        "FlipX INTEGER, "
        "FlipY INTEGER, "
        "Rotation REAL, "
        "DeskewRotation REAL, "
        "OverlayOffsetX REAL, "
        "OverlayOffsetY REAL, "
        "OverlayRotation REAL, "
        "MultiPageCount INTEGER, "
        "Index1 INTEGER, "
        "Sequence REAL, "
        "Invert INTEGER, "
        "Bitonal INTEGER, "
        "DigitizerNX1 INTEGER DEFAULT 0, "
        "DigitizerNY1 INTEGER DEFAULT 0, "
        "DigitizerNX2 INTEGER DEFAULT 0, "
        "DigitizerNY2 INTEGER DEFAULT 0, "
        "DigitizerWidth REAL DEFAULT 0, "
        "DigitizerHeight REAL DEFAULT 0, "
        "DigitizerResX INTEGER DEFAULT 0, "
        "DigitizerResY INTEGER DEFAULT 0, "
        "WasSent INTEGER, "
        "MasterPageUID INTEGER, "
        "TypicalPageRepeats INTEGER, "
        "GUID TEXT, "
        "OverlayRect TEXT, "
        "OverlayResized TEXT, "
        "DeskewRotationOverlay REAL, "
        "ZoomFlag INTEGER, "
        "SheetNo TEXT, "
        "OCRState INTEGER, "
        "OCRUID INTEGER, "
        "ALState INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidPercents ("
        "UID INTEGER PRIMARY KEY, "
        "BidTimeCardStateUID INTEGER, "
        "BidLaborCostCodeUID INTEGER, "
        "BidLaborActivityUID INTEGER, "
        "BidTakeoffUID INTEGER, "
        "Percent REAL, "
        "BidPageUID INTEGER, "
        "GUID TEXT)",

        "CREATE TABLE IF NOT EXISTS BidPlanRooms ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "ProjectID TEXT, "
        "DocumentsURL TEXT, "
        "Documents BLOB)",

        "CREATE TABLE IF NOT EXISTS BidProjects ("
        "UID INTEGER PRIMARY KEY, "
        "Name TEXT, "
        "Description BLOB)",

        "CREATE TABLE IF NOT EXISTS BidSettings ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageSelectedUID INTEGER, "
        "STSGUID TEXT, "
        "STSServerName TEXT, "
        "STSClientName TEXT)",

        "CREATE TABLE IF NOT EXISTS BidTakeoffTotals ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidZoneUID INTEGER, "
        "BidAreaUID INTEGER, "
        "BidTypAreaUID INTEGER, "
        "BidConditionUID INTEGER, "
        "Quantity1 REAL, "
        "Quantity2 REAL, "
        "Quantity3 REAL, "
        "SumQuantity1 REAL, "
        "SumQuantity2 REAL, "
        "SumQuantity3 REAL)",

        "CREATE TABLE IF NOT EXISTS BidTakeoffs ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidConditionUID INTEGER, "
        "BidZoneUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidAreaUID INTEGER, "
        "BidTypAreaUID INTEGER, "
        "ParentUID INTEGER, "
        "No INTEGER, "
        "Quantity REAL, "
        "Count REAL DEFAULT 0, "
        "Rotation REAL, "
        "Position TEXT, "
        "GridOffsetX REAL DEFAULT 0, "
        "GridOffsetY REAL DEFAULT 0, "
        "GridRotation REAL DEFAULT 0, "
        "IsNegativeQuantity INTEGER DEFAULT 0, "
        "FontName TEXT, "
        "FontColor INTEGER DEFAULT 0, "
        "FontSize INTEGER, "
        "FontBold INTEGER, "
        "FontItalic INTEGER, "
        "FontUnderline INTEGER, "
        "TypGroupTakeoffUID INTEGER, "
        "TypPageTakeoffUID INTEGER, "
        "TakeoffModified INTEGER, "
        "TypGroupUID INTEGER, "
        "TypGroupMarkerUID INTEGER, "
        "FlipX INTEGER, "
        "FlipY INTEGER, "
        "GUID TEXT, "
        "NameFontName TEXT, "
        "NameFontColor INTEGER DEFAULT 0, "
        "NameFontSize INTEGER, "
        "NameFontBold INTEGER, "
        "NameFontItalic INTEGER, "
        "NameFontUnderline INTEGER, "
        "Curve INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidTimeCardStates ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "Date TEXT, "
        "State INTEGER, "
        "Type INTEGER, "
        "Percent REAL, "
        "ProjectedOver REAL, "
        "IsValid INTEGER, "
        "GUID TEXT)",

        "CREATE TABLE IF NOT EXISTS BidTimeCards ("
        "UID INTEGER PRIMARY KEY, "
        "BidTimeCardStateUID INTEGER, "
        "BidEmployeeUID INTEGER, "
        "BidAreaUID INTEGER, "
        "BidTypicalAreaUID INTEGER, "
        "BidLaborCostCodeUID INTEGER, "
        "Hours1 REAL, "
        "Hours2 REAL, "
        "Hours3 REAL, "
        "Hours4 REAL, "
        "GUID TEXT)",

        "CREATE TABLE IF NOT EXISTS BidTexts ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidLayerUID INTEGER, "
        "Name BLOB, "
        "FontName TEXT, "
        "FontColor INTEGER DEFAULT 0, "
        "FontSize INTEGER, "
        "FontBold INTEGER, "
        "FontItalic INTEGER, "
        "FontUnderline INTEGER, "
        "TextAlign INTEGER, "
        "Position TEXT)",

        "CREATE TABLE IF NOT EXISTS BidTransactionsHistory ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "TransactionID INTEGER DEFAULT 0, "
        "SendDate TEXT, "
        "ReceiveDate TEXT, "
        "LoginNameFrom TEXT, "
        "LoginNameTo TEXT, "
        "Description TEXT, "
        "EarnedHrs INTEGER, "
        "UsedHrs INTEGER, "
        "Type TEXT, "
        "File TEXT)",

        "CREATE TABLE IF NOT EXISTS BidTypAreas ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "Name TEXT, "
        "Sequence INTEGER DEFAULT 0, "
        "WasSent INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidTypAreaCounts ("
        "UID INTEGER PRIMARY KEY, "
        "BidAreaUID INTEGER, "
        "BidTypAreaUID INTEGER, "
        "Count INTEGER, "
        "WasSent INTEGER)",

        "CREATE TABLE IF NOT EXISTS BidTypGroupViews ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidConditionUID INTEGER, "
        "BidPageUID INTEGER, "
        "Position BLOB)",

        "CREATE TABLE IF NOT EXISTS BidTypicalGroupTotals ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "BidZoneUID INTEGER, "
        "BidAreaUID INTEGER, "
        "BidTypGroupUID INTEGER, "
        "BidConditionUID INTEGER, "
        "Quantity1 REAL, "
        "Quantity2 REAL, "
        "Quantity3 REAL)",

        "CREATE TABLE IF NOT EXISTS BidZones ("
        "UID INTEGER PRIMARY KEY, "
        "BidUID INTEGER, "
        "BidLayerUID INTEGER, "
        "ExternalID INTEGER, "
        "Name TEXT, "
        "Notes BLOB, "
        "Pattern INTEGER, "
        "ColorLine INTEGER, "
        "ColorFill INTEGER, "
        "Spacing REAL, "
        "IsNegativeValues INTEGER, "
        "Sequence INTEGER, "
        "GUID TEXT)",

        "CREATE TABLE IF NOT EXISTS Bids ("
        "UID INTEGER PRIMARY KEY, "
        "BidProjectUID INTEGER, "
        "ParentBidUID INTEGER, "
        "OrigBidProjectUID INTEGER, "
        "OrigParentBidUID INTEGER, "
        "JobStatusUID INTEGER, "
        "EstimatorUID INTEGER, "
        "PrManagerUID INTEGER, "
        "JobSiteManagerUID INTEGER, "
        "SourceBidUID TEXT, "
        "ExternalID INTEGER DEFAULT 0, "
        "QuickBidDB TEXT, "
        "BidNo INTEGER, "
        "BidType INTEGER, "
        "JobID TEXT, "
        "JobName TEXT, "
        "ImageFolder TEXT, "
        "Notes BLOB, "
        "IsAccepted INTEGER, "
        "RecalcNeeded INTEGER, "
        "CreateDateTime TEXT, "
        "ModDateTime TEXT, "
        "PriceUsing INTEGER, "
        "PriceUsingDatabase TEXT, "
        "PriceUsingWorksheet TEXT, "
        "TakeoffIncrements REAL, "
        "HoursPerDay INTEGER DEFAULT 0, "
        "MeasureBase INTEGER DEFAULT 0, "
        "WeekStartDay INTEGER DEFAULT 0, "
        "QuantitiesInLegend INTEGER, "
        "JobSendRec INTEGER, "
        "ScaleStyle INTEGER, "
        "IsCustomScale INTEGER, "
        "ScaleFactor1 REAL, "
        "ScaleFactor2 REAL, "
        "PageScale REAL, "
        "PageWidth REAL, "
        "PageHeight REAL, "
        "LastReceiveDateTime TEXT, "
        "LastSendDateTime TEXT, "
        "DeliverEntireBid INTEGER, "
        "EstimatedDays REAL, "
        "Percent REAL, "
        "ProjectOver REAL, "
        "DPCMode INTEGER DEFAULT 0, "
        "IsDPCUpdated INTEGER DEFAULT 0, "
        "IgnoreBidAreas INTEGER DEFAULT 0, "
        "SendImageFiles INTEGER DEFAULT 0, "
        "HasSTSClash INTEGER DEFAULT 0, "
        "SRPending INTEGER DEFAULT 0, "
        "IsUnlocked INTEGER DEFAULT 0, "
        "FullBidSent INTEGER DEFAULT 0, "
        "TypicalType INTEGER, "
        "GUID TEXT, "
        "BidDate TEXT, "
        "CopyFromBidNO INTEGER, "
        "CopyTimeStamp TEXT, "
        "CoverSheetSelItemType INTEGER, "
        "CoverSheetSelItemUID INTEGER, "
        "LegendFlags INTEGER, "
        "IsCalculatedForSlope INTEGER, "
        "IsCalculatedForLaborCostCodeTotals INTEGER)",

        "CREATE TABLE IF NOT EXISTS Boost ("
        "UID INTEGER PRIMARY KEY, "
        "TransactionID TEXT, "
        "BidUID INTEGER, "
        "BidPageUID INTEGER, "
        "State INTEGER, "
        "UploadURL BLOB, "
        "CallbackURL BLOB, "
        "ImagePath TEXT)",

        "CREATE TABLE IF NOT EXISTS CdnTypes ("
        "UID INTEGER PRIMARY KEY, "
        "Name TEXT, "
        "ExpandState INTEGER)",

        "CREATE TABLE IF NOT EXISTS ConditionSetStyles ("
        "UID INTEGER PRIMARY KEY, "
        "ConditionSetUID INTEGER, "
        "ConditionStyleUID INTEGER, "
        "Sequence INTEGER)",

        "CREATE TABLE IF NOT EXISTS ConditionSets ("
        "UID INTEGER PRIMARY KEY, "
        "EmployeeUID INTEGER, "
        "Name TEXT)",

        "CREATE TABLE IF NOT EXISTS CostCodes ("
        "UID INTEGER PRIMARY KEY, "
        "Type INTEGER, "
        "Name TEXT, "
        "Description TEXT, "
        "Sequence INTEGER DEFAULT 0, "
        "RuleType TEXT, "
        "RuleValue INTEGER DEFAULT 0)",

        "CREATE TABLE IF NOT EXISTS DPCCalcFilter ("
        "UID INTEGER PRIMARY KEY, "
        "BidPageUID INTEGER, "
        "BidUID INTEGER)",

        "CREATE TABLE IF NOT EXISTS Employees ("
        "UID INTEGER PRIMARY KEY, "
        "PayClassUID INTEGER, "
        "AccessLevelUID INTEGER, "
        "EmployeeNo TEXT, "
        "FirstName TEXT, "
        "LastName TEXT, "
        "EnableLogin INTEGER, "
        "LoginName TEXT, "
        "Password INTEGER, "
        "Address1 TEXT, "
        "Address2 TEXT, "
        "City TEXT, "
        "State TEXT, "
        "Zip TEXT, "
        "HomePhone TEXT, "
        "MobilePhone TEXT, "
        "EMail TEXT)",

        "CREATE TABLE IF NOT EXISTS JobStatuses ("
        "UID INTEGER PRIMARY KEY, "
        "Locked INTEGER, "
        "Name TEXT, "
        "Sequence INTEGER)",

        "CREATE TABLE IF NOT EXISTS Locking ("
        "UID INTEGER PRIMARY KEY, "
        "Essence TEXT NOT NULL, "
        "Owner TEXT NOT NULL, "
        "Data BLOB)",

        "CREATE TABLE IF NOT EXISTS OCRProps ("
        "UID INTEGER PRIMARY KEY, "
        "NumberRegion TEXT, "
        "NameRegion TEXT, "
        "NameTemplate TEXT)",

        "CREATE TABLE IF NOT EXISTS PayClasses ("
        "UID INTEGER PRIMARY KEY, "
        "Name TEXT)",

        "CREATE TABLE IF NOT EXISTS STSTransactionHistory ("
        "UID INTEGER PRIMARY KEY, "
        "ObjectUID INTEGER, "
        "BidUID INTEGER, "
        "TableName TEXT, "
        "ChangeType INTEGER DEFAULT 0, "
        "ExtraInfo TEXT, "
        "ExtraInfo2 TEXT)",

        "CREATE TABLE IF NOT EXISTS SchemaRegistry ("
        "UID INTEGER PRIMARY KEY, "
        "Version INTEGER, "
        "Product INTEGER)",

        "CREATE TABLE IF NOT EXISTS Settings ("
        "UID INTEGER PRIMARY KEY, "
        "Name TEXT, "
        "Created TEXT, "
        "NextBidNo INTEGER, "
        "LoginRequired INTEGER, "
        "MeasureBase INTEGER, "
        "PriceUsing INTEGER, "
        "QuantitiesInLegend INTEGER DEFAULT 0, "
        "HoursPerDay INTEGER DEFAULT 0, "
        "StartWeekOn INTEGER DEFAULT 0, "
        "GridCountMethod INTEGER DEFAULT 0, "
        "TakeoffIncrements REAL, "
        "ScaleStyle INTEGER, "
        "IsCustomScale INTEGER, "
        "ScaleFactor1 REAL, "
        "ScaleFactor2 REAL, "
        "PageScale REAL, "
        "PageWidth REAL, "
        "PageHeight REAL, "
        "LabelHours1 TEXT, "
        "LabelHours2 TEXT, "
        "LabelHours3 TEXT, "
        "LabelHours4 TEXT, "
        "IgnoreBidAreas INTEGER DEFAULT 0, "
        "SendImageFiles INTEGER DEFAULT 0, "
        "STSClientID TEXT, "
        "STSClientPassword INTEGER, "
        "BackupNo INTEGER DEFAULT 2, "
        "BackupPeriod INTEGER DEFAULT 2, "
        "CompressPeriod INTEGER DEFAULT 3, "
        "LegendFlags INTEGER, "
        "StyleLibrarySelItemType INTEGER, "
        "StyleLibrarySelItemUID INTEGER, "
        "SLUExpandState INTEGER)",

        "CREATE TABLE IF NOT EXISTS Transactions ("
        "UID INTEGER PRIMARY KEY, "
        "ID TEXT NOT NULL, "
        "ParentID TEXT NOT NULL, "
        "State INTEGER, "
        "Type INTEGER, "
        "Owner TEXT NOT NULL, "
        "Data BLOB)",

        "CREATE TABLE IF NOT EXISTS UserMasterConditions ("
        "UID INTEGER PRIMARY KEY, "
        "Name TEXT)",

        "CREATE TABLE IF NOT EXISTS _AppMeta ("
        "key TEXT PRIMARY KEY, "
        "value TEXT)",
    };

    const std::vector<std::pair<const char *, const char *>> FK_INDEXES = {
        {"AffectDPCTypGroupViews", "BidUID"},
        {"BidALines", "BidPageUID"},
        {"BidALines", "BidTakeoffFromUID"},
        {"BidALines", "BidTakeoffToUID"},
        {"BidALines", "BidUID"},
        {"BidAnnoInk", "BidPageUID"},
        {"BidAnnoInk", "BidUID"},
        {"BidAnnotationClouds", "BidLayerUID"},
        {"BidAnnotationClouds", "BidPageUID"},
        {"BidAnnotationClouds", "BidUID"},
        {"BidAnnotationOvals", "BidLayerUID"},
        {"BidAnnotationOvals", "BidPageUID"},
        {"BidAnnotationOvals", "BidUID"},
        {"BidAnnotationPolygons", "BidLayerUID"},
        {"BidAnnotationPolygons", "BidPageUID"},
        {"BidAnnotationPolygons", "BidUID"},
        {"BidAnnotationRects", "BidLayerUID"},
        {"BidAnnotationRects", "BidPageUID"},
        {"BidAnnotationRects", "BidUID"},
        {"BidAreaTranslations", "BidPageUID"},
        {"BidAreas", "BidUID"},
        {"BidAreas", "GUID"},
        {"BidAreas", "ParentUID"},
        {"BidArrows", "BidPageUID"},
        {"BidArrows", "BidTakeoffFromUID"},
        {"BidArrows", "BidTakeoffToUID"},
        {"BidArrows", "BidUID"},
        {"BidCallOuts", "BidLayerUID"},
        {"BidCallOuts", "BidPageUID"},
        {"BidCallOuts", "BidUID"},
        {"BidComments", "BidLayerUID"},
        {"BidComments", "BidPageUID"},
        {"BidComments", "BidUID"},
        {"BidComments", "ParentCommentUID"},
        {"BidConditionFolders", "BidUID"},
        {"BidConditionFolders", "ParentUID"},
        {"BidConditions", "BidConditionFolderUID"},
        {"BidConditions", "BidLayerUID"},
        {"BidConditions", "BidUID"},
        {"BidConditions", "CdnTypeUID"},
        {"BidDPCSubscribers", "BidEmployeeUID"},
        {"BidDPCSubscribers", "BidUID"},
        {"BidDimensions", "BidPageUID"},
        {"BidDimensions", "BidTakeoffFromUID"},
        {"BidDimensions", "BidTakeoffToUID"},
        {"BidDimensions", "BidUID"},
        {"BidEmployees", "BidUID"},
        {"BidEmployees", "EmployeeUID"},
        {"BidEmployees", "GUID"},
        {"BidEmployees", "PayClassUID"},
        {"BidHighlights", "BidLayerUID"},
        {"BidHighlights", "BidPageUID"},
        {"BidHighlights", "BidUID"},
        {"BidHotLinks", "BidLayerUID"},
        {"BidHotLinks", "BidPageUID"},
        {"BidHotLinks", "BidPageViewUID"},
        {"BidHotLinks", "BidUID"},
        {"BidLaborActivity", "BidConditionUID"},
        {"BidLaborActivity", "BidLaborCostCodeUID"},
        {"BidLaborActivity", "BidUID"},
        {"BidLaborCostCodeTotals", "BidPageUID"},
        {"BidLaborCostCodeTotals", "BidUID"},
        {"BidLaborCostCodes", "BidUID"},
        {"BidLaborCostCodes", "CostCodeUID"},
        {"BidLaborCostCodes", "GUID"},
        {"BidLayers", "BidUID"},
        {"BidLayers", "IsTemplate"},
        {"BidLegends", "BidPageUID"},
        {"BidLegends", "BidUID"},
        {"BidNamedViews", "BidPageUID"},
        {"BidNamedViews", "BidUID"},
        {"BidNotes", "BidUID"},
        {"BidNotes", "GUID"},
        {"BidPageFolders", "BidUID"},
        {"BidPageFolders", "GUID"},
        {"BidPageFolders", "ParentUID"},
        {"BidPageSettings", "BidAreaUID"},
        {"BidPageSettings", "BidPageUID"},
        {"BidPageSettings", "BidTypAreaUID"},
        {"BidPages", "BidPageFolderUID"},
        {"BidPages", "BidUID"},
        {"BidPages", "GUID"},
        {"BidPercents", "BidLaborActivityUID"},
        {"BidPercents", "BidLaborCostCodeUID"},
        {"BidPercents", "BidPageUID"},
        {"BidPercents", "BidTakeoffUID"},
        {"BidPercents", "BidTimeCardStateUID"},
        {"BidPercents", "GUID"},
        {"BidPlanRooms", "BidUID"},
        {"BidProjects", "Name"},
        {"BidSettings", "BidPageSelectedUID"},
        {"BidTakeoffTotals", "BidPageUID"},
        {"BidTakeoffTotals", "BidUID"},
        {"BidTakeoffs", "BidAreaUID"},
        {"BidTakeoffs", "BidConditionUID"},
        {"BidTakeoffs", "BidPageUID"},
        {"BidTakeoffs", "BidTypAreaUID"},
        {"BidTakeoffs", "BidUID"},
        {"BidTakeoffs", "BidZoneUID"},
        {"BidTakeoffs", "GUID"},
        {"BidTakeoffs", "No"},
        {"BidTakeoffs", "ParentUID"},
        {"BidTexts", "BidLayerUID"},
        {"BidTexts", "BidPageUID"},
        {"BidTexts", "BidUID"},
        {"BidTimeCardStates", "BidUID"},
        {"BidTimeCardStates", "GUID"},
        {"BidTimeCards", "BidAreaUID"},
        {"BidTimeCards", "BidEmployeeUID"},
        {"BidTimeCards", "BidLaborCostCodeUID"},
        {"BidTimeCards", "BidTimeCardStateUID"},
        {"BidTimeCards", "BidTypicalAreaUID"},
        {"BidTimeCards", "GUID"},
        {"BidTransactionsHistory", "BidUID"},
        {"BidTypAreaCounts", "BidAreaUID"},
        {"BidTypAreaCounts", "BidTypAreaUID"},
        {"BidTypAreas", "BidUID"},
        {"BidTypGroupViews", "BidPageUID"},
        {"BidTypGroupViews", "BidUID"},
        {"BidTypicalGroupTotals", "BidPageUID"},
        {"BidTypicalGroupTotals", "BidUID"},
        {"BidZones", "BidLayerUID"},
        {"BidZones", "BidUID"},
        {"Bids", "BidProjectUID"},
        {"Bids", "BidType"},
        {"Bids", "ParentBidUID"},
        {"CdnTypes", "Name"},
        {"ConditionSetStyles", "ConditionSetUID"},
        {"ConditionSetStyles", "ConditionStyleUID"},
        {"ConditionSets", "EmployeeUID"},
        {"DPCCalcFilter", "BidUID"},
        {"Employees", "AccessLevelUID"},
        {"Employees", "LoginName"},
        {"Employees", "PayClassUID"},
        {"STSTransactionHistory", "ChangeType"},
        {"STSTransactionHistory", "ObjectUID"},
        {"UserMasterConditions", "Name"},
    };

    const std::vector<const char *> UNIQUE_INDEX_DDL = {
        "CREATE UNIQUE INDEX IF NOT EXISTS [UI_Locking2] ON [Locking] ([Essence])",
        "CREATE UNIQUE INDEX IF NOT EXISTS [UI_Locking] ON [Locking] ([Essence], [Owner])",
    };

    const std::vector<int> SCHEMA_VERSIONS = {
        86,
        87,
        88,
        89,
        90,
        91,
        94,
        95,
        96,
        97,
        98,
        99,
        100,
        101,
        102,
        103,
        104,
        105,
        106,
        107,
        108,
        109,
        110,
        111,
        112,
        113,
    };

    const std::vector<DefaultLayerSeed> DEFAULT_LAYERS = {
        {"Default", -1, -1, 2},
        {"Annotation", -1, -1, 1},
        {"Image", -1, -1, 0},
        {"Comments", -1, -1, 3},
    };

    const std::vector<const char *> BID_SECTIONS = {
        "BidPlanRooms",
        "BidAreas",
        "BidTypAreas",
        "BidConditionFolders",
        "BidPageFolders",
        "BidSettings",
        "BidEmployees",
        "BidLayers",
        "BidTypAreaCounts",
        "BidConditions",
        "UserMasterConditions",
        "BidConditionUser",
        "BidZones",
        "BidTypGroupViews",
        "OCRProps",
    };

    const std::vector<const char *> PAGE_SECTIONS = {
        "BidTakeoffs",
        "BidHighlights",
        "BidTexts",
        "BidDimensions",
        "BidArrows",
        "BidALines",
        "BidCallOuts",
        "BidAnnotationRects",
        "BidAnnotationOvals",
        "BidAnnotationPolygons",
        "BidAnnotationClouds",
        "BidAnnoInk",
        "BidLegends",
        "BidPageSettings",
        "BidAreaTranslations",
        "BidMarkedPages",
        "BidComments",
    };

    const std::vector<const char *> BID_TAIL_SECTIONS = {
        "BidNamedViews",
        "BidHotLinks",
    };

    const std::vector<const char *> GLOBAL_SECTIONS = {
        "Employees",
        "PayClasses",
        "AccessLevels",
        "CdnTypes",
        "JobStatuses",
    };

    // Windows schema_contract.singular: two overrides, otherwise the trailing "s" is dropped.
    std::string singular(const std::string &table_name)
    {
        if (table_name == "JobStatuses")
            return "JobStatuse";
        if (table_name == "PayClasses")
            return "PayClass";
        if (!table_name.empty() && table_name.back() == 's')
            return table_name.substr(0, table_name.size() - 1);
        return table_name;
    }

    // Columns of type YESNO in the Windows schema (Windows database_creator.py _TABLE_DDL, 85 columns). The iOS DDL
    // flattened them to INTEGER, but the importer converts their values like Access does (import_operations.py
    // _convert_access_value: 'True'/'true'/'-1'/'1' -> -1, anything else -> 0).
    const std::vector<std::pair<const char *, const char *>> YESNO_COLUMNS = {
        {"BidAreas", "WasSent"},
        {"BidCallOuts", "FontBold"},
        {"BidCallOuts", "FontItalic"},
        {"BidCallOuts", "FontUnderline"},
        {"BidConditionFolders", "ExpandState"},
        {"BidConditions", "IsTemplate"},
        {"BidConditions", "Backout"},
        {"BidConditions", "DropRun"},
        {"BidConditions", "Grid"},
        {"BidConditions", "Connect"},
        {"BidConditions", "Trim"},
        {"BidConditions", "Curve"},
        {"BidConditions", "SnapToGrid"},
        {"BidConditions", "ManualLength"},
        {"BidConditions", "FontBold"},
        {"BidConditions", "FontItalic"},
        {"BidConditions", "DisplayDimension"},
        {"BidConditions", "RoundQuantity"},
        {"BidConditions", "IsCurvedSegment"},
        {"BidConditions", "DisplayGridWhileDrawing"},
        {"BidConditions", "ShowTakeoff"},
        {"BidConditions", "DisplayName"},
        {"BidDimensions", "FontBold"},
        {"BidDimensions", "FontItalic"},
        {"BidDimensions", "FontUnderline"},
        {"BidEmployees", "IsActive"},
        {"BidLaborActivity", "IsActive"},
        {"BidLaborActivity", "UsesConditionQuantity"},
        {"BidLaborCostCodes", "IsActive"},
        {"BidLayers", "IsTemplate"},
        {"BidLayers", "Show"},
        {"BidLayers", "IsLocked"},
        {"BidLegends", "FontBold"},
        {"BidLegends", "FontItalic"},
        {"BidLegends", "FontUnderline"},
        {"BidLegends", "IsShowTotals"},
        {"BidLegends", "MoveToCorner"},
        {"BidPageFolders", "WasSent"},
        {"BidPages", "IsCustomScale"},
        {"BidPages", "FlipX"},
        {"BidPages", "FlipY"},
        {"BidPages", "Invert"},
        {"BidPages", "Bitonal"},
        {"BidPages", "WasSent"},
        {"BidPages", "ZoomFlag"},
        {"BidTakeoffs", "IsNegativeQuantity"},
        {"BidTakeoffs", "FontBold"},
        {"BidTakeoffs", "FontItalic"},
        {"BidTakeoffs", "FontUnderline"},
        {"BidTakeoffs", "TakeoffModified"},
        {"BidTakeoffs", "FlipX"},
        {"BidTakeoffs", "FlipY"},
        {"BidTakeoffs", "NameFontBold"},
        {"BidTakeoffs", "NameFontItalic"},
        {"BidTakeoffs", "NameFontUnderline"},
        {"BidTimeCardStates", "IsValid"},
        {"BidTexts", "FontBold"},
        {"BidTexts", "FontItalic"},
        {"BidTexts", "FontUnderline"},
        {"BidTypAreas", "WasSent"},
        {"BidTypAreaCounts", "WasSent"},
        {"BidZones", "IsNegativeValues"},
        {"Bids", "IsAccepted"},
        {"Bids", "RecalcNeeded"},
        {"Bids", "QuantitiesInLegend"},
        {"Bids", "JobSendRec"},
        {"Bids", "IsCustomScale"},
        {"Bids", "DeliverEntireBid"},
        {"Bids", "DPCMode"},
        {"Bids", "IsDPCUpdated"},
        {"Bids", "IgnoreBidAreas"},
        {"Bids", "SendImageFiles"},
        {"Bids", "HasSTSClash"},
        {"Bids", "SRPending"},
        {"Bids", "IsUnlocked"},
        {"Bids", "FullBidSent"},
        {"Bids", "IsCalculatedForSlope"},
        {"Bids", "IsCalculatedForLaborCostCodeTotals"},
        {"Employees", "EnableLogin"},
        {"JobStatuses", "Locked"},
        {"Settings", "LoginRequired"},
        {"Settings", "QuantitiesInLegend"},
        {"Settings", "IsCustomScale"},
        {"Settings", "IgnoreBidAreas"},
        {"Settings", "SendImageFiles"},
    };

    // Windows raw_bid_integrity.BID_RELATIONSHIPS (144 relationships, same order). The parent column is always UID.
    const std::vector<BidRelationship> BID_RELATIONSHIPS = {
        {"Bids", "BidProjectUID", "BidProjects", false},
        {"Bids", "OrigBidProjectUID", "BidProjects", false},
        {"Bids", "JobStatusUID", "JobStatuses", false},
        {"Bids", "EstimatorUID", "Employees", false},
        {"Bids", "PrManagerUID", "Employees", false},
        {"Bids", "JobSiteManagerUID", "Employees", false},
        {"AffectDPCTypGroupViews", "BidTypGroupViewUID", "BidTypGroupViews", false},
        {"AffectDPCTypGroupViews", "BidUID", "Bids", false},
        {"BidPlanRooms", "BidUID", "Bids", false},
        {"BidAreas", "BidUID", "Bids", false},
        {"BidAreas", "ParentUID", "BidAreas", false},
        {"BidTypAreas", "BidUID", "Bids", false},
        {"BidConditionFolders", "BidUID", "Bids", false},
        {"BidConditionFolders", "ParentUID", "BidConditionFolders", false},
        {"BidPageFolders", "BidUID", "Bids", false},
        {"BidPageFolders", "ParentUID", "BidPageFolders", false},
        {"BidSettings", "BidUID", "Bids", false},
        {"BidSettings", "BidPageSelectedUID", "BidPages", false},
        {"BidEmployees", "BidUID", "Bids", false},
        {"BidEmployees", "EmployeeUID", "Employees", false},
        {"BidEmployees", "PayClassUID", "PayClasses", false},
        {"BidLayers", "BidUID", "Bids", false},
        {"BidTypAreaCounts", "BidAreaUID", "BidAreas", false},
        {"BidTypAreaCounts", "BidTypAreaUID", "BidTypAreas", false},
        {"BidConditions", "BidUID", "Bids", false},
        {"BidConditions", "BidConditionFolderUID", "BidConditionFolders", false},
        {"BidConditions", "BidLayerUID", "BidLayers", false},
        {"BidConditions", "CdnTypeUID", "CdnTypes", false},
        {"BidConditionUser", "BidUID", "Bids", false},
        {"BidConditionUser", "ConditionUID", "BidConditions", false},
        {"BidDPCSubscribers", "BidUID", "Bids", false},
        {"BidDPCSubscribers", "BidEmployeeUID", "Employees", false},
        {"BidLaborActivity", "BidUID", "Bids", false},
        {"BidLaborActivity", "BidConditionUID", "BidConditions", false},
        {"BidLaborActivity", "BidLaborCostCodeUID", "BidLaborCostCodes", false},
        {"BidLaborCostCodes", "BidUID", "Bids", false},
        {"BidLaborCostCodes", "CostCodeUID", "CostCodes", false},
        {"BidLaborCostCodeTotals", "BidUID", "Bids", false},
        {"BidLaborCostCodeTotals", "BidPageUID", "BidPages", false},
        {"BidLaborCostCodeTotals", "BidAreaUID", "BidAreas", false},
        {"BidLaborCostCodeTotals", "BidLaborCostCodeUID", "BidLaborCostCodes", false},
        {"BidNotes", "BidUID", "Bids", false},
        {"BidPercents", "BidTimeCardStateUID", "BidTimeCardStates", false},
        {"BidPercents", "BidLaborCostCodeUID", "BidLaborCostCodes", false},
        {"BidPercents", "BidLaborActivityUID", "BidLaborActivity", false},
        {"BidPercents", "BidTakeoffUID", "BidTakeoffs", false},
        {"BidPercents", "BidPageUID", "BidPages", false},
        {"BidZones", "BidUID", "Bids", false},
        {"BidZones", "BidLayerUID", "BidLayers", false},
        {"BidTypGroupViews", "BidUID", "Bids", false},
        {"BidTypGroupViews", "BidPageUID", "BidPages", false},
        {"BidTypGroupViews", "BidConditionUID", "BidConditions", false},
        {"BidPages", "BidUID", "Bids", false},
        {"BidPages", "BidPageFolderUID", "BidPageFolders", false},
        {"BidPages", "MasterPageUID", "BidPages", false},
        {"BidTakeoffs", "BidUID", "Bids", false},
        {"BidTakeoffs", "BidPageUID", "BidPages", true},
        {"BidTakeoffs", "BidConditionUID", "BidConditions", true},
        {"BidTakeoffs", "BidZoneUID", "BidZones", false},
        {"BidTakeoffs", "BidAreaUID", "BidAreas", false},
        {"BidTakeoffs", "BidTypAreaUID", "BidTypAreas", false},
        {"BidTakeoffs", "ParentUID", "BidTakeoffs", false},
        {"BidTakeoffs", "TypGroupTakeoffUID", "BidTakeoffs", false},
        {"BidTakeoffs", "TypPageTakeoffUID", "BidTakeoffs", false},
        {"BidTakeoffs", "TypGroupMarkerUID", "BidTakeoffs", false},
        {"BidTakeoffTotals", "BidUID", "Bids", false},
        {"BidTakeoffTotals", "BidPageUID", "BidPages", false},
        {"BidTakeoffTotals", "BidZoneUID", "BidZones", false},
        {"BidTakeoffTotals", "BidAreaUID", "BidAreas", false},
        {"BidTakeoffTotals", "BidTypAreaUID", "BidTypAreas", false},
        {"BidTakeoffTotals", "BidConditionUID", "BidConditions", false},
        {"BidTimeCardStates", "BidUID", "Bids", false},
        {"BidTimeCards", "BidTimeCardStateUID", "BidTimeCardStates", false},
        {"BidTimeCards", "BidEmployeeUID", "BidEmployees", false},
        {"BidTimeCards", "BidAreaUID", "BidAreas", false},
        {"BidTimeCards", "BidTypicalAreaUID", "BidTypAreas", false},
        {"BidTimeCards", "BidLaborCostCodeUID", "BidLaborCostCodes", false},
        {"BidTransactionsHistory", "BidUID", "Bids", false},
        {"BidHighlights", "BidUID", "Bids", false},
        {"BidHighlights", "BidPageUID", "BidPages", false},
        {"BidHighlights", "BidLayerUID", "BidLayers", false},
        {"BidTexts", "BidUID", "Bids", false},
        {"BidTexts", "BidPageUID", "BidPages", false},
        {"BidTexts", "BidLayerUID", "BidLayers", false},
        {"BidDimensions", "BidUID", "Bids", false},
        {"BidDimensions", "BidPageUID", "BidPages", false},
        {"BidDimensions", "BidTakeoffFromUID", "BidTakeoffs", false},
        {"BidDimensions", "BidTakeoffToUID", "BidTakeoffs", false},
        {"BidArrows", "BidUID", "Bids", false},
        {"BidArrows", "BidPageUID", "BidPages", false},
        {"BidArrows", "BidTakeoffFromUID", "BidTakeoffs", false},
        {"BidArrows", "BidTakeoffToUID", "BidTakeoffs", false},
        {"BidALines", "BidUID", "Bids", false},
        {"BidALines", "BidPageUID", "BidPages", false},
        {"BidALines", "BidTakeoffFromUID", "BidTakeoffs", false},
        {"BidALines", "BidTakeoffToUID", "BidTakeoffs", false},
        {"BidCallOuts", "BidUID", "Bids", false},
        {"BidCallOuts", "BidPageUID", "BidPages", false},
        {"BidCallOuts", "BidLayerUID", "BidLayers", false},
        {"BidAnnotationRects", "BidUID", "Bids", false},
        {"BidAnnotationRects", "BidPageUID", "BidPages", false},
        {"BidAnnotationRects", "BidLayerUID", "BidLayers", false},
        {"BidAnnotationOvals", "BidUID", "Bids", false},
        {"BidAnnotationOvals", "BidPageUID", "BidPages", false},
        {"BidAnnotationOvals", "BidLayerUID", "BidLayers", false},
        {"BidAnnotationPolygons", "BidUID", "Bids", false},
        {"BidAnnotationPolygons", "BidPageUID", "BidPages", false},
        {"BidAnnotationPolygons", "BidLayerUID", "BidLayers", false},
        {"BidAnnotationClouds", "BidUID", "Bids", false},
        {"BidAnnotationClouds", "BidPageUID", "BidPages", false},
        {"BidAnnotationClouds", "BidLayerUID", "BidLayers", false},
        {"BidAnnoInk", "BidUID", "Bids", false},
        {"BidAnnoInk", "BidPageUID", "BidPages", false},
        {"BidLegends", "BidUID", "Bids", false},
        {"BidLegends", "BidPageUID", "BidPages", false},
        {"BidPageSettings", "BidPageUID", "BidPages", false},
        {"BidPageSettings", "BidAreaUID", "BidAreas", false},
        {"BidPageSettings", "BidTypAreaUID", "BidTypAreas", false},
        {"BidAreaTranslations", "BidPageUID", "BidPages", false},
        {"BidAreaTranslations", "MasterAreaUID", "BidAreas", false},
        {"BidAreaTranslations", "TranslateAreaUID", "BidAreas", false},
        {"BidMarkedPages", "BidPageUID", "BidPages", false},
        {"BidComments", "BidUID", "Bids", false},
        {"BidComments", "BidPageUID", "BidPages", false},
        {"BidComments", "BidLayerUID", "BidLayers", false},
        {"BidComments", "ParentCommentUID", "BidComments", false},
        {"BidNamedViews", "BidUID", "Bids", false},
        {"BidNamedViews", "BidPageUID", "BidPages", false},
        {"BidHotLinks", "BidUID", "Bids", false},
        {"BidHotLinks", "BidPageUID", "BidPages", false},
        {"BidHotLinks", "BidPageViewUID", "BidNamedViews", false},
        {"BidHotLinks", "BidLayerUID", "BidLayers", false},
        {"BidTypicalGroupTotals", "BidUID", "Bids", false},
        {"BidTypicalGroupTotals", "BidPageUID", "BidPages", false},
        {"BidTypicalGroupTotals", "BidZoneUID", "BidZones", false},
        {"BidTypicalGroupTotals", "BidAreaUID", "BidAreas", false},
        {"BidTypicalGroupTotals", "BidConditionUID", "BidConditions", false},
        {"Boost", "BidUID", "Bids", false},
        {"Boost", "BidPageUID", "BidPages", false},
        {"DPCCalcFilter", "BidUID", "Bids", false},
        {"DPCCalcFilter", "BidPageUID", "BidPages", false},
        {"STSTransactionHistory", "BidUID", "Bids", false},
        {"Employees", "PayClassUID", "PayClasses", false},
        {"Employees", "AccessLevelUID", "AccessLevels", false},
    };

    bool is_yesno_column(const std::string &table, const std::string &column)
    {
        for (const auto &entry : YESNO_COLUMNS)
            if (table == entry.first && column == entry.second)
                return true;
        return false;
    }

    // Columns of type DATETIME (14) and IMAGE (30) in the Windows schema (Windows database_creator.py _TABLE_DDL). The iOS DDL
    // types them TEXT / BLOB, but the importer converts their values by the Windows type (import_operations.py
    // _convert_access_value, IMP-20): a DATETIME value that is not six whitespace separated integers forming a valid date
    // becomes NULL; an IMAGE value that is empty becomes NULL. The ODBC driver reports IMAGE as "longbinary".
    const std::vector<std::pair<const char *, const char *>> DATETIME_COLUMNS = {
        {"BidComments", "CommentModified"},
        {"BidEmployees", "DateAdded"},
        {"BidLaborCostCodeTotals", "Date"},
        {"BidNotes", "DateCreated"},
        {"BidTimeCardStates", "Date"},
        {"BidTransactionsHistory", "SendDate"},
        {"BidTransactionsHistory", "ReceiveDate"},
        {"Bids", "CreateDateTime"},
        {"Bids", "ModDateTime"},
        {"Bids", "LastReceiveDateTime"},
        {"Bids", "LastSendDateTime"},
        {"Bids", "BidDate"},
        {"Bids", "CopyTimeStamp"},
        {"Settings", "Created"},
    };

    const std::vector<std::pair<const char *, const char *>> IMAGE_COLUMNS = {
        {"BidALines", "Position"},
        {"BidAnnoInk", "Position"},
        {"BidAnnotationClouds", "Position"},
        {"BidAnnotationOvals", "Position"},
        {"BidAnnotationPolygons", "Position"},
        {"BidAnnotationRects", "Position"},
        {"BidArrows", "Position"},
        {"BidCallOuts", "Name"},
        {"BidComments", "Comment"},
        {"BidConditionFolders", "Description"},
        {"BidConditions", "Notes"},
        {"BidDimensions", "Position"},
        {"BidHighlights", "Position"},
        {"BidHotLinks", "Position"},
        {"BidLaborCostCodes", "Notes"},
        {"BidLegends", "Position"},
        {"BidNamedViews", "Position"},
        {"BidNotes", "Notes"},
        {"BidPageFolders", "Description"},
        {"BidPlanRooms", "Documents"},
        {"BidProjects", "Description"},
        {"BidTakeoffs", "Position"},
        {"BidTexts", "Name"},
        {"BidTypGroupViews", "Position"},
        {"BidZones", "Notes"},
        {"Bids", "Notes"},
        {"Boost", "UploadURL"},
        {"Boost", "CallbackURL"},
        {"Locking", "Data"},
        {"Transactions", "Data"},
    };

    bool is_datetime_column(const std::string &table, const std::string &column)
    {
        for (const auto &entry : DATETIME_COLUMNS)
            if (table == entry.first && column == entry.second)
                return true;
        return false;
    }

    bool is_image_column(const std::string &table, const std::string &column)
    {
        for (const auto &entry : IMAGE_COLUMNS)
            if (table == entry.first && column == entry.second)
                return true;
        return false;
    }

}
