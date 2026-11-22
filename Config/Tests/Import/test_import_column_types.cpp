// IMP-20: DATETIME and binary (memo / IMAGE) column values are converted like the Windows importer does.
//
// Windows: ImportOperationsMixin._convert_access_value (ost_visualizer/infrastructure/mdb/components/import_operations.py):
//   - a DATETIME column (14 of them in database_creator.py _TABLE_DDL: Bids.CreateDateTime, ModDateTime, LastReceiveDateTime,
//     LastSendDateTime, BidDate, CopyTimeStamp, BidComments.CommentModified, BidEmployees.DateAdded, ...): a value that
//     is exactly six whitespace separated integers 'Y M D h m s' forming a valid datetime.datetime becomes a datetime; anything
//     else (blank, ISO text, 3 parts, month 13, non-numeric) becomes NULL; an integer outside the C int range (datetime raises
//     OverflowError) becomes NULL too: since Windows commit b9902e6 the function catches (ValueError, TypeError, OverflowError)
//     (parse_ost_datetime, ost_text_values.py:15-22, called at import_operations.py:597), so such a text no longer stops the import (sub-rule (1) below).
//   - a memo / IMAGE column (30 IMAGE columns, e.g. Bids.Notes, BidTakeoffs.Position, BidConditionFolders.Description):
//     '' becomes NULL, text becomes its UTF-8 bytes.
// The user's decision (IMP-20): the iOS database keeps the RAW six part text for a valid date (it has no datetime type and the
// bid date columns are never read) and stores NULL for an invalid one. Binary columns: '' is NULL, text stays text (the iOS
// columns are TEXT/BLOB read back as text).
//
// IMP-20 has three sub-rules (see PORTING_BLOCKERS.md). Sub-rule (1) is a PARITY, not a deviation: an integer outside the C int
// range reads as NULL and the import goes on, on Windows (b9902e6) and on iOS. Sub-rules (2) and (3) stay deliberate deviations from
// Windows, kept by the user's decision of 2026-11-19 under the general rule (an accidental, inconsistent or unsafe Windows behaviour is
// not ported, the user decides):
//   - (2) the Windows latin-1 bytes for BidTexts/BidCallOuts.Name are not stored (iOS reads text columns as UTF-8: latin-1 bytes
//     would be invalid strings, and a character above U+00FF would make the Windows code raise) and (3) neither is the BLOB
//     storage class of memo values (the iOS writers store TEXT in the same columns, so a column would mix both classes).
// The valid date itself is the standing difference too: Windows stores a datetime object, iOS the raw six part text.
//
// Oracle: Fixtures/import_datetime_oracle.txt holds ~3400 values and what the real Windows function (HEAD b9902e6) does with them
// (see the fixture header): N (None) or D (a datetime). iOS must read NULL for every N and keep the raw text for every D.
#include "ImportTestSupport.hpp"

#include "Persistence/OstSchema.hpp"
#include "Persistence/ProjectStore/ProjectStore_ImportInternal.hpp"

#include <sstream>

using ost_import::OstRawData;
using ost_import::parse_ost_xml;
using ost_itest::ScratchDb;
using ost_store::ImportValue;

namespace
{
    std::string show(const ImportValue &v)
    {
        switch (v.kind)
        {
        case ImportValue::Null:
            return "NULL";
        case ImportValue::Integer:
            return "int:" + std::to_string(v.integer);
        case ImportValue::Real:
            return "real";
        case ImportValue::Text:
            return "text:" + v.text;
        }
        return "?";
    }

    // true = the conversion succeeded; `out` the value; `failure` the Python message when it raised.
    bool convert(const std::string &value, const std::string &type_name, ImportValue &out, std::string *failure = nullptr)
    {
        return ost_store::import_convert_value(value, type_name, out, failure);
    }

    std::string converted(const std::string &value, const std::string &type_name)
    {
        ImportValue out;
        std::string failure;
        if (!convert(value, type_name, out, &failure))
            return "FAIL:" + failure;
        return show(out);
    }

    std::string from_hex(const std::string &hex)
    {
        std::string out;
        for (size_t i = 0; i + 1 < hex.size(); i += 2)
            out += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
        return out;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the conversion itself
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_value_conversion_follows_access_column_types
// (datetime): '2024 3 9 14 5 7' is a datetime; the decision keeps its raw text.
TEST(ImportColumnTypes, Imp20_AValidSixPartDateKeepsItsRawText)
{
    CHECK_EQ(converted("2024 3 9 14 5 7", "datetime"), std::string("text:2024 3 9 14 5 7"));
    // the text is kept as it is, not normalised
    CHECK_EQ(converted("  2024   03 9\t14 5 7\n", "datetime"), std::string("text:  2024   03 9\t14 5 7\n"));
    CHECK_EQ(converted("2024 2 29 23 59 59", "datetime"), std::string("text:2024 2 29 23 59 59"));
    CHECK_EQ(converted("1 1 1 0 0 0", "datetime"), std::string("text:1 1 1 0 0 0"));
    CHECK_EQ(converted("9999 12 31 23 59 59", "datetime"), std::string("text:9999 12 31 23 59 59"));
}

// Port of ::test_access_import_value_conversion_follows_access_column_types (bad_date) and
// ::test_datetime_text_needs_exactly_six_parts_otherwise_it_reads_as_null.
TEST(ImportColumnTypes, Imp20_AnythingElseReadsAsNull)
{
    for (const char *bad : {"", "2024-03-09", "2024 3 9", "2024 13 9 1 1 1", "a b c d e f", "2024", "2024 3 9 14 5", "2024 3 9 14 5 7 1",
                            "2024 3 9 14 5 7 1 2", "2023 2 29 0 0 0", "2024 4 31 0 0 0", "0 1 1 0 0 0", "10000 1 1 0 0 0", "2024 3 9 24 0 0",
                            "2024 3 9 0 60 0", "2024 3 9 0 0 60", "2024 3 9 14 5 7.5", "2024-03-09 14:05:07", "2024 0 9 1 1 1", "2024 3 0 1 1 1"})
    {
        SCOPED_CONTEXT(std::string("value '") + bad + "'");
        CHECK_EQ(converted(bad, "datetime"), std::string("NULL"));
    }
    for (const char *type_name : {"datetime", "longbinary", "memo"})
    {
        CHECK_EQ(converted("NULL", type_name), std::string("NULL"));
    }
}

// What an integer outside the C int range does: it reads as NULL, like every other invalid date, and the import goes on. Port of
// tests/infrastructure/mdb/components/test_import_operations.py::test_a_datetime_text_beyond_the_c_int_range_reads_as_null: since
// Windows commit b9902e6 datetime.datetime's OverflowError ('Python int too large to convert to C long') is caught together with
// ValueError and TypeError (parse_ost_datetime, ost_text_values.py:15-22, called at import_operations.py:597), so Windows and iOS agree (a pin: iOS already read NULL here). A part that is
// not an integer is NULL too, huge parts or not.
TEST(ImportColumnTypes, Imp20_AnIntegerBeyondTheCIntRangeReadsAsNull)
{
    // the three literal inputs of the Windows test: a huge year, a huge month, a huge negative second
    for (const char *value : {"99999999999999999999 1 1 0 0 0", "2024 99999999999999999999 1 0 0 0", "2024 1 1 0 0 -99999999999999999999"})
    {
        SCOPED_CONTEXT(std::string("Windows input '") + value + "'");
        ImportValue out;
        std::string failure;
        CHECK(convert(value, "datetime", out, &failure));
        CHECK_EQ(show(out), std::string("NULL"));
        CHECK(failure.empty());
    }
    // the valid control of the Windows test: datetime(2024, 3, 9, 14, 5, 7) there, its raw text here (the standing IMP-20 decision)
    CHECK_EQ(converted("2024 3 9 14 5 7", "datetime"), std::string("text:2024 3 9 14 5 7"));

    // the same in every slot, at the first value beyond the C int range, and beyond 64 bits
    for (const char *value : {"2147483648 1 1 0 0 0", "1 2147483648 1 0 0 0", "1 1 2147483648 0 0 0", "1 1 1 2147483648 0 0", "1 1 1 0 2147483648 0",
                              "1 1 1 0 0 2147483648", "-2147483649 1 1 0 0 0", "99999999999999999999 1 1 0 0 0", "2024 3 9 14 5 99999999999999999999",
                              "99999999999999999999999999999999999999 1 1 0 0 0", "1 1 1 0 0 -99999999999999999999"})
    {
        SCOPED_CONTEXT(std::string("value '") + value + "'");
        ImportValue out;
        std::string failure;
        CHECK(convert(value, "datetime", out, &failure));
        CHECK_EQ(show(out), std::string("NULL"));
        CHECK(failure.empty());
    }
    // within the int range, but not a date: NULL (ValueError)
    CHECK_EQ(converted("2147483647 1 1 0 0 0", "datetime"), std::string("NULL"));
    CHECK_EQ(converted("-2147483648 1 1 0 0 0", "datetime"), std::string("NULL"));
    // a part that is not an integer wins over a huge one (int() runs before datetime())
    CHECK_EQ(converted("x 99999999999 1 1 1 1", "datetime"), std::string("NULL"));
    CHECK_EQ(converted("99999999999 1 1 1 1 x", "datetime"), std::string("NULL"));
    CHECK_EQ(converted("99999999999 1 1 1 1 5.0", "datetime"), std::string("NULL"));
}

// The oracle: every value of the fixture through the REAL Windows function (HEAD b9902e6; N = None, D = a datetime whose raw text iOS
// keeps). The fixture used to mark 176 values X (an uncaught OverflowError stopped the Windows import); since b9902e6 Windows
// returns None for them and they are N like every other invalid date, so there is no X any more.
TEST(ImportColumnTypes, Imp20_TheConversionMatchesTheWindowsFunctionOnEveryOracleValue)
{
    std::string text;
    REQUIRE(ost_test::read_file(ost_test::fixture_path("import_datetime_oracle.txt"), text));
    std::istringstream stream(text);
    std::string line;
    size_t checked = 0, nulls = 0, kept = 0;
    int number = 0;
    while (std::getline(stream, line))
    {
        ++number;
        if (line.empty() || line[0] == '#')
            continue;
        const size_t tab = line.find('\t');
        REQUIRE(tab != std::string::npos);
        const std::string value = from_hex(line.substr(0, tab));
        const std::string expected = line.substr(tab + 1);
        SCOPED_CONTEXT("fixture line " + std::to_string(number) + " [" + line.substr(0, std::min<size_t>(tab, 60)) + "]");
        ImportValue out;
        const bool ok = convert(value, "datetime", out);
        std::string actual;
        if (!ok)
            actual = "X";
        else if (out.kind == ImportValue::Null)
            actual = "N";
        else if (out.kind == ImportValue::Text && out.text == value)
            actual = "D";
        else
            actual = "?";
        CHECK(expected == "N" || expected == "D"); // no X (an exception) is left in the fixture
        CHECK_EQ(actual, expected);
        ++checked;
        nulls += actual == "N";
        kept += actual == "D";
    }
    CHECK(checked > 3000u);
    CHECK(nulls > 1000u);
    CHECK(kept > 300u);
}

// Memo / IMAGE columns: '' is NULL, text is kept (UTF-8). The column type name of the Windows ODBC driver for IMAGE is
// 'longbinary' ('longbinary' contains 'long': it must not be read as a number).
TEST(ImportColumnTypes, Imp20_EmptyMemoAndBinaryValuesAreNullAndTextIsKept)
{
    for (const char *type_name : {"longbinary", "memo"})
    {
        SCOPED_CONTEXT(std::string("type ") + type_name);
        CHECK_EQ(converted("", type_name), std::string("NULL"));
        CHECK_EQ(converted("note", type_name), std::string("text:note"));
        CHECK_EQ(converted("caf\xC3\xA9", type_name), std::string("text:caf\xC3\xA9"));
        CHECK_EQ(converted("12", type_name), std::string("text:12"));
        CHECK_EQ(converted("1.5", type_name), std::string("text:1.5"));
        CHECK_EQ(converted("0", type_name), std::string("text:0"));
        CHECK_EQ(converted(" ", type_name), std::string("text: "));
        CHECK_EQ(converted("99999999999999999999", type_name), std::string("text:99999999999999999999"));
    }
    // the other types are as before: text keeps '' and a number is a number
    CHECK_EQ(converted("", "text"), std::string("text:"));
    CHECK_EQ(converted("", "integer"), std::string("NULL"));
    CHECK_EQ(converted("7", "integer"), std::string("int:7"));
}

// ---------------------------------------------------------------------------------------------------------------
// which columns
// ---------------------------------------------------------------------------------------------------------------

// The 14 DATETIME and 30 IMAGE columns of the Windows schema (database_creator.py _TABLE_DDL, read programmatically).
TEST(ImportColumnTypes, Imp20_TheColumnTablesAreTheWindowsSchemaColumns)
{
    const std::vector<std::pair<std::string, std::string>> datetime_columns = {
        {"BidComments", "CommentModified"}, {"BidEmployees", "DateAdded"},   {"BidLaborCostCodeTotals", "Date"}, {"BidNotes", "DateCreated"},
        {"BidTimeCardStates", "Date"},      {"BidTransactionsHistory", "SendDate"}, {"BidTransactionsHistory", "ReceiveDate"},
        {"Bids", "CreateDateTime"},         {"Bids", "ModDateTime"},         {"Bids", "LastReceiveDateTime"}, {"Bids", "LastSendDateTime"},
        {"Bids", "BidDate"},                {"Bids", "CopyTimeStamp"},       {"Settings", "Created"}};
    const std::vector<std::pair<std::string, std::string>> image_columns = {
        {"BidALines", "Position"},       {"BidAnnoInk", "Position"},          {"BidAnnotationClouds", "Position"}, {"BidAnnotationOvals", "Position"},
        {"BidAnnotationPolygons", "Position"}, {"BidAnnotationRects", "Position"}, {"BidArrows", "Position"},     {"BidCallOuts", "Name"},
        {"BidComments", "Comment"},      {"BidConditionFolders", "Description"}, {"BidConditions", "Notes"},      {"BidDimensions", "Position"},
        {"BidHighlights", "Position"},   {"BidHotLinks", "Position"},         {"BidLaborCostCodes", "Notes"},     {"BidLegends", "Position"},
        {"BidNamedViews", "Position"},   {"BidNotes", "Notes"},               {"BidPageFolders", "Description"},  {"BidPlanRooms", "Documents"},
        {"BidProjects", "Description"},  {"BidTakeoffs", "Position"},         {"BidTexts", "Name"},               {"BidTypGroupViews", "Position"},
        {"BidZones", "Notes"},           {"Bids", "Notes"},                   {"Boost", "UploadURL"},             {"Boost", "CallbackURL"},
        {"Locking", "Data"},             {"Transactions", "Data"}};
    CHECK_EQ(ost_schema::DATETIME_COLUMNS.size(), datetime_columns.size());
    CHECK_EQ(ost_schema::IMAGE_COLUMNS.size(), image_columns.size());
    for (const auto &column : datetime_columns)
    {
        SCOPED_CONTEXT("datetime " + column.first + "." + column.second);
        CHECK(ost_schema::is_datetime_column(column.first, column.second));
        CHECK_FALSE(ost_schema::is_image_column(column.first, column.second));
        CHECK_FALSE(ost_schema::is_yesno_column(column.first, column.second));
    }
    for (const auto &column : image_columns)
    {
        SCOPED_CONTEXT("image " + column.first + "." + column.second);
        CHECK(ost_schema::is_image_column(column.first, column.second));
        CHECK_FALSE(ost_schema::is_datetime_column(column.first, column.second));
        CHECK_FALSE(ost_schema::is_yesno_column(column.first, column.second));
    }
    // columns that sound alike are not in the tables
    for (const auto &column : std::vector<std::pair<std::string, std::string>>{{"Bids", "UID"}, {"Bids", "JobName"}, {"Bids", "BidNo"}, {"BidTakeoffs", "Name"},
                                                                              {"BidTexts", "Position"}, {"BidTakeoffs", "Notes"}, {"Employees", "DateAdded"},
                                                                              {"BidPages", "Name"}, {"bids", "bidDate"}, {"Bids", "bidDate"}, {"", ""}})
    {
        SCOPED_CONTEXT(column.first + "." + column.second);
        CHECK_FALSE(ost_schema::is_datetime_column(column.first, column.second));
        CHECK_FALSE(ost_schema::is_image_column(column.first, column.second));
    }
}

// The table description the importer converts by: DATETIME / IMAGE columns report the Windows ODBC type names, whatever the iOS
// DDL declares (TEXT, BLOB).
TEST(ImportColumnTypes, Imp20_TheTableInfoReportsTheWindowsTypeOfEveryDatetimeAndImageColumn)
{
    ScratchDb db("column_types_info", true);
    REQUIRE(db.schema_ok());
    ost_itest::Connection connection(db);
    for (const auto &entry : ost_schema::DATETIME_COLUMNS)
    {
        SCOPED_CONTEXT(std::string("datetime ") + entry.first + "." + entry.second);
        const ost_store::ImportTableInfo info = ost_store::import_get_table_info(connection.get(), entry.first);
        if (info.columns.empty() || !info.has_column(entry.second))
            continue; // a table or column the iOS schema does not have
        CHECK_EQ(info.types.at(entry.second), std::string("datetime"));
    }
    for (const auto &entry : ost_schema::IMAGE_COLUMNS)
    {
        SCOPED_CONTEXT(std::string("image ") + entry.first + "." + entry.second);
        const ost_store::ImportTableInfo info = ost_store::import_get_table_info(connection.get(), entry.first);
        if (info.columns.empty() || !info.has_column(entry.second))
            continue;
        CHECK_EQ(info.types.at(entry.second), std::string("longbinary"));
    }
    // the iOS schema does have the columns the tests below use
    const ost_store::ImportTableInfo bids = ost_store::import_get_table_info(connection.get(), "Bids");
    for (const char *column : {"CreateDateTime", "ModDateTime", "LastReceiveDateTime", "LastSendDateTime", "BidDate", "CopyTimeStamp"})
    {
        SCOPED_CONTEXT(std::string("column ") + column);
        REQUIRE(bids.has_column(column));
        CHECK_EQ(bids.types.at(column), std::string("datetime"));
    }
    REQUIRE(bids.has_column("Notes"));
    CHECK_EQ(bids.types.at("Notes"), std::string("longbinary"));
    CHECK_EQ(bids.types.at("JobName"), std::string("text"));
}

// ---------------------------------------------------------------------------------------------------------------
// end to end
// ---------------------------------------------------------------------------------------------------------------

// IMP-M04 test plan (alias of IMP-05): <Bid UID="6" JobName="" Notes=""/> -> typeof(JobName) = 'text' with value '', typeof(Notes)
// = 'null'. Plus the datetime columns of the Bids row: a valid date keeps its raw text, everything else is NULL.
TEST(ImportColumnTypes, Imp20_ImpM04_ABidRowIsStoredWithTypedDatesAndEmptyMemoAsNull)
{
    ScratchDb db("column_types_bid", true);
    REQUIRE(db.schema_ok());
    const OstRawData raw = parse_ost_xml(
        "<XML_ROOT><Bid UID=\"6\" JobName=\"\" Notes=\"\" CreateDateTime=\"2024 3 9 14 5 7\" ModDateTime=\"2024-03-09\" BidDate=\"\" "
        "LastSendDateTime=\"2024 13 9 1 1 1\" LastReceiveDateTime=\"2024 3 9\" CopyTimestamp=\"2024 2 29 0 0 0\"/></XML_ROOT>");
    REQUIRE(!raw.empty());
    REQUIRE(ost_store::import_project(db.path(), raw));
    CHECK_EQ(db.rows("SELECT typeof(JobName), JobName, typeof(Notes) FROM Bids", 3), std::string("text||null"));
    CHECK_EQ(db.rows("SELECT typeof(CreateDateTime), CreateDateTime FROM Bids", 2), std::string("text|2024 3 9 14 5 7"));
    CHECK_EQ(db.rows("SELECT typeof(CopyTimeStamp), CopyTimeStamp FROM Bids", 2), std::string("text|2024 2 29 0 0 0"));
    CHECK_EQ(db.rows("SELECT typeof(ModDateTime), typeof(BidDate), typeof(LastSendDateTime), typeof(LastReceiveDateTime) FROM Bids", 4),
             std::string("null|null|null|null"));
}

// Non-empty memo text is stored as it is; the same typed conversion applies to the rows of the bid tables.
TEST(ImportColumnTypes, Imp20_MemoTextAndDatesOfTheBidTablesAreConverted)
{
    ScratchDb db("column_types_tables", true);
    REQUIRE(db.schema_ok());
    const OstRawData raw = parse_ost_xml(
        "<XML_ROOT><Bid UID=\"6\" JobName=\"Job\" Notes=\"caf\xC3\xA9 note\">"
        "<BidConditionFolders><BidConditionFolder UID=\"7\" BidUID=\"6\" Name=\"A\" Description=\"\"/>"
        "<BidConditionFolder UID=\"8\" BidUID=\"6\" Name=\"B\" Description=\"described\"/></BidConditionFolders>"
        "<BidZones><BidZone UID=\"9\" BidUID=\"6\" Name=\"Z\" Notes=\"\"/></BidZones>"
        "</Bid></XML_ROOT>");
    REQUIRE(!raw.empty());
    REQUIRE(ost_store::import_project(db.path(), raw));
    CHECK_EQ(db.rows("SELECT typeof(Notes), Notes FROM Bids", 2), std::string("text|caf\xC3\xA9 note"));
    CHECK_EQ(db.rows("SELECT Name, typeof(Description), Description FROM BidConditionFolders ORDER BY Name", 3),
             std::string("A|null|NULL;B|text|described"));
    CHECK_EQ(db.rows("SELECT typeof(Notes) FROM BidZones", 1), std::string("null"));
}

// An out-of-range date reads as NULL and the import goes on: the bid and its rows are stored, only that column is NULL (Windows
// CHANGELOG.md of b9902e6: "a date outside the supported range is imported as empty instead of stopping the whole import"). A pin:
// iOS already behaved so.
// A date column holding the text 'NULL' is NULL too (the 'NULL' marker of the raw rows), next to the overflowing one.
TEST(ImportColumnTypes, Imp20_AnOutOfRangeDateStoresNullAndTheImportGoesOn)
{
    ScratchDb db("column_types_overflow", true);
    REQUIRE(db.schema_ok());
    const OstRawData raw = parse_ost_xml(
        "<XML_ROOT><Bid UID=\"6\" JobName=\"Job\" CreateDateTime=\"99999999999 1 1 0 0 0\" ModDateTime=\"2024 3 9 14 5 7\" "
        "LastSendDateTime=\"NULL\" LastReceiveDateTime=\"99999999999999999999 1 1 0 0 0\">"
        "<BidConditionFolders><BidConditionFolder UID=\"7\" BidUID=\"6\" Name=\"A\"/></BidConditionFolders>"
        "</Bid></XML_ROOT>");
    REQUIRE(!raw.empty());
    ost_store::ImportResult result;
    CHECK(ost_store::import_project(db.path(), raw, &result));
    CHECK(result.error.empty());
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 1);
    CHECK_EQ(db.rows("SELECT JobName, typeof(CreateDateTime), typeof(ModDateTime), ModDateTime, typeof(LastSendDateTime), "
                     "typeof(LastReceiveDateTime) FROM Bids",
                     6),
             std::string("Job|null|text|2024 3 9 14 5 7|null|null"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 1);
}

// Port of test_an_out_of_range_datetime_stores_null_and_the_row_is_still_written (Windows b9902e6): the importer's own row writer, a
// hand-made table Sample (UID INTEGER, Stamp DATETIME), one row whose Stamp is '99999999999999999999 1 1 0 0 0' and one whose Stamp is the
// text 'NULL'. Both rows are written and both stamps are NULL (Windows: [(1, None), (2, None)]). A pin: iOS already behaved so.
TEST(ImportColumnTypes, Imp20_AnOutOfRangeDateAndTheTextNullBothStoreNullAndTheRowIsWritten)
{
    ScratchDb db("column_types_sample");
    db.must("CREATE TABLE Sample (UID INTEGER, Stamp DATETIME)");
    ost_itest::Connection connection(db);
    const ost_store::ImportTableInfo info = ost_store::import_get_table_info(connection.get(), "Sample");
    REQUIRE(info.has_column("Stamp"));
    CHECK_STR_EQ(info.types.at("Stamp"), std::string("datetime"));
    std::string error;
    CHECK(ost_store::import_insert_raw_row(connection.get(), "Sample",
                                           ost_itest::row({{"UID", "1"}, {"Stamp", "99999999999999999999 1 1 0 0 0"}}), info, error));
    CHECK(error.empty());
    CHECK(ost_store::import_insert_raw_row(connection.get(), "Sample", ost_itest::row({{"UID", "2"}, {"Stamp", "NULL"}}), info, error));
    CHECK(error.empty());
    CHECK_EQ(db.rows("SELECT UID, Stamp FROM Sample ORDER BY UID", 2), std::string("1|NULL;2|NULL"));
    CHECK_EQ(db.rows("SELECT typeof(Stamp) FROM Sample ORDER BY UID", 1), std::string("null;null"));
    // and a valid stamp next to them keeps its raw text (the standing IMP-20 decision)
    CHECK(ost_store::import_insert_raw_row(connection.get(), "Sample", ost_itest::row({{"UID", "3"}, {"Stamp", "2024 3 9 14 5 7"}}), info, error));
    CHECK_EQ(db.rows("SELECT UID, typeof(Stamp), Stamp FROM Sample WHERE UID = 3", 3), std::string("3|text|2024 3 9 14 5 7"));
}
