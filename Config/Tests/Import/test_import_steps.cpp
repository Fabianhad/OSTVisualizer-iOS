// IMP-06 (UID floor), IMP-01 (bid number) and IMP-05 (typed values / autonumber UID) at step level.
//
// Windows: ost_visualizer/infrastructure/mdb/components/import_operations.py (_get_max_uid, _assign_next_bid_no,
// _get_table_info, _insert_raw_row, _should_omit_autonumber_uid, _convert_access_value) with settings_cardinality.py.
// The Windows tests run these steps on small hand-written SQLite schemas (tests/helpers/mdb/import_export_support.py);
// the same schemas are used here.
#include "ImportTestSupport.hpp"

#include "Persistence/OstSchema.hpp"
#include "Persistence/ProjectStore/ProjectStore_ImportInternal.hpp"

using ost_itest::Connection;
using ost_itest::ScratchDb;
using ost_store::ImportValue;

namespace
{
    // tests/helpers/mdb/import_export_support.py::_create_import_schema (SQLite flavour of the Access schema).
    // next_bid_numbers are SQL literals: "7", "'not-a-number'", "NULL", "''".
    void create_import_schema(const ScratchDb &db, const std::vector<std::string> &next_bid_numbers = {"1"})
    {
        db.must("CREATE TABLE Settings (NextBidNo INTEGER)");
        for (const std::string &value : next_bid_numbers)
            db.must("INSERT INTO Settings (NextBidNo) VALUES (" + value + ")");
        db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, BidProjectUID INTEGER, EstimatorUID INTEGER, "
                "JobStatusUID INTEGER, JobName TEXT, BidNo INTEGER)");
        db.must("CREATE TABLE Employees (UID INTEGER PRIMARY KEY, EmployeeNo TEXT, FirstName TEXT, LastName TEXT, "
                "PayClassUID INTEGER)");
        db.must("CREATE TABLE PayClasses (UID INTEGER PRIMARY KEY, Name TEXT)");
        db.must("CREATE TABLE AccessLevels (UID INTEGER PRIMARY KEY, Name TEXT)");
        db.must("CREATE TABLE CdnTypes (UID INTEGER PRIMARY KEY, Name TEXT, ExpandState INTEGER)");
        db.must("CREATE TABLE JobStatuses (UID INTEGER PRIMARY KEY, Name TEXT, Locked INTEGER, Sequence INTEGER)");
        db.must("CREATE TABLE BidAreas (UID INTEGER PRIMARY KEY, BidUID INTEGER, ParentUID INTEGER, Name TEXT, "
                "Sequence INTEGER, GUID TEXT)");
        db.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT, Sequence INTEGER)");
        db.must("CREATE TABLE BidSettings (UID INTEGER PRIMARY KEY, BidUID INTEGER, BidPageSelectedUID INTEGER)");
        db.must("CREATE TABLE BidPageSettings (UID INTEGER PRIMARY KEY, BidPageUID INTEGER, BidAreaUID INTEGER, "
                "BidTypAreaUID INTEGER, BidAreaSelected INTEGER)");
    }

    long long floor_of(const ScratchDb &db, bool *ok = nullptr, std::string *error = nullptr)
    {
        Connection conn(db);
        long long max_uid = -1;
        std::string err;
        const bool result = ost_store::import_get_max_uid(conn.get(), max_uid, err);
        if (ok)
            *ok = result;
        if (error)
            *error = err;
        return max_uid;
    }

    bool assign_bid_no(const ScratchDb &db, ost_import::Row &bid_row, std::string &error)
    {
        Connection conn(db);
        return ost_store::import_assign_next_bid_no(conn.get(), bid_row, error);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-06: the UID floor (_get_max_uid)
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_uid_floor_includes_dangling_target_references
TEST(ImportFloor, Imp06_FloorIncludesDanglingTargetReferences)
{
    ScratchDb db("floor_dangling");
    db.must_all({"CREATE TABLE Bids (UID INTEGER)", "INSERT INTO Bids VALUES (1)",
                 "CREATE TABLE BidNotes (UID INTEGER, BidUID INTEGER)", "INSERT INTO BidNotes VALUES (10, 2)"});
    CHECK_EQ(floor_of(db), 2);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_uid_floor_uses_highest_uid_across_import_tables
TEST(ImportFloor, Imp06_FloorUsesTheHighestUidAcrossTheImportTables)
{
    ScratchDb db("floor_highest");
    db.must_all({"CREATE TABLE Bids (UID INTEGER)", "CREATE TABLE BidAreas (UID INTEGER)",
                 "CREATE TABLE CdnTypes (UID INTEGER)", "CREATE TABLE BidTakeoffs (UID INTEGER)",
                 "INSERT INTO Bids VALUES (4)", "INSERT INTO BidAreas VALUES (9)", "INSERT INTO BidAreas VALUES (50)",
                 "INSERT INTO CdnTypes VALUES (17)", "INSERT INTO BidTakeoffs VALUES (31)"});
    CHECK_EQ(floor_of(db), 50);

    ScratchDb empty("floor_empty");
    bool ok = false;
    CHECK_EQ(floor_of(empty, &ok), 0);
    CHECK(ok);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_floor_ignores_references_into_tables_the_import_never_writes
// (Bids.BidProjectUID points into BidProjects, which the import never writes: 500 does not count).
TEST(ImportFloor, Imp06_FloorIgnoresReferencesIntoTablesTheImportNeverWrites)
{
    ScratchDb db("floor_ignores");
    db.must_all({"CREATE TABLE Bids (UID INTEGER, BidProjectUID INTEGER)", "INSERT INTO Bids VALUES (1, 500)",
                 "CREATE TABLE BidNotes (UID INTEGER, BidUID INTEGER)", "INSERT INTO BidNotes VALUES (10, 2)"});
    CHECK_EQ(floor_of(db), 2);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_floor_issues_no_reference_query_for_absent_tables_or_columns
TEST(ImportFloor, Imp06_AbsentTablesAndColumnsAreSkipped)
{
    ScratchDb db("floor_absent");
    db.must_all({"CREATE TABLE Bids (UID INTEGER)", "INSERT INTO Bids VALUES (4)", "CREATE TABLE BidAreas (UID INTEGER)",
                 "INSERT INTO BidAreas VALUES (6)", "CREATE TABLE BidNotes (UID INTEGER)"});
    bool ok = false;
    CHECK_EQ(floor_of(db, &ok), 6);
    CHECK(ok);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_a_second_uid_column_of_the_same_child_table_is_scanned_too
TEST(ImportFloor, Imp06_ASecondUidColumnOfTheSameChildTableIsScannedToo)
{
    ScratchDb db("floor_second_column");
    db.must_all({"CREATE TABLE Bids (UID INTEGER)", "INSERT INTO Bids VALUES (1)",
                 "CREATE TABLE BidAreas (UID INTEGER, BidUID INTEGER, ParentUID INTEGER)",
                 "INSERT INTO BidAreas VALUES (2, 3, 900)"});
    CHECK_EQ(floor_of(db), 900);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_floor_does_not_swallow_a_non_numeric_uid
// (Windows lets the ValueError escape, the import fails; here the step reports failure).
TEST(ImportFloor, Imp06_ANonNumericUidFailsTheFloor)
{
    ScratchDb db("floor_non_numeric");
    db.must_all({"CREATE TABLE Bids (UID INTEGER)", "INSERT INTO Bids VALUES ('abc')"});
    bool ok = true;
    std::string error;
    floor_of(db, &ok, &error);
    CHECK_FALSE(ok);
    CHECK(error.find("abc") != std::string::npos);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_a_non_numeric_child_reference_value_is_not_swallowed
TEST(ImportFloor, Imp06_ANonNumericChildReferenceFailsTheFloor)
{
    ScratchDb db("floor_non_numeric_child");
    db.must_all({"CREATE TABLE Bids (UID INTEGER)", "CREATE TABLE BidNotes (UID INTEGER, BidUID TEXT)",
                 "INSERT INTO BidNotes VALUES (1, 'abc')"});
    bool ok = true;
    floor_of(db, &ok);
    CHECK_FALSE(ok);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_numeric_text_in_a_child_reference_column_counts_as_a_number
TEST(ImportFloor, Imp06_NumericTextInAChildReferenceColumnCountsAsANumber)
{
    ScratchDb db("floor_numeric_text");
    db.must_all({"CREATE TABLE Bids (UID INTEGER)", "INSERT INTO Bids VALUES (3)",
                 "CREATE TABLE BidNotes (UID INTEGER, BidUID TEXT)", "INSERT INTO BidNotes VALUES (1, '12')"});
    CHECK_EQ(floor_of(db), 12);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_the_uid_floor_scans_every_table_the_import_can_write
TEST(ImportFloor, Imp06_TheFloorScansEveryTableTheImportCanWrite)
{
    std::vector<std::string> tables = {"Bids", "CdnTypes", "JobStatuses"};
    for (const char *t : ost_schema::BID_SECTIONS)
        tables.push_back(t);
    tables.push_back("BidPages");
    for (const char *t : ost_schema::BID_TAIL_SECTIONS)
        tables.push_back(t);
    for (const char *t : ost_schema::PAGE_SECTIONS)
        tables.push_back(t);
    CHECK_EQ(tables.size(), 3u + 15u + 1u + 2u + 17u); // includes BidEmployees (15 bid sections)
    for (const std::string &table : tables)
    {
        SCOPED_CONTEXT("table " + table);
        ScratchDb db("floor_every_" + table);
        db.must_all({"CREATE TABLE [" + table + "] (UID INTEGER)", "INSERT INTO [" + table + "] VALUES (777)"});
        CHECK_EQ(floor_of(db), 777);
    }
}

// IMP-06: the iOS schema seeds the template layers at UIDs 1-4, so the floor of a fresh database is 4.
TEST(ImportFloor, Imp06_FreshIosDatabaseFloorIsTheTemplateLayerUids)
{
    ScratchDb db("floor_fresh_ios", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    CHECK_EQ(db.count("SELECT MAX(UID) FROM BidLayers"), 4);
    CHECK_EQ(floor_of(db), 4);
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-01: Settings.NextBidNo (_assign_next_bid_no)
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_initializes_missing_global_settings_once
TEST(ImportBidNumber, Imp01_MissingSettingsRowIsInitialisedOnce)
{
    ScratchDb db("bidno_missing_row");
    create_import_schema(db, {});
    ost_import::Row first, second;
    std::string error;
    CHECK(assign_bid_no(db, first, error));
    CHECK(assign_bid_no(db, second, error));
    CHECK_STR_EQ(first.at("BidNo"), std::string("1"));
    CHECK_STR_EQ(second.at("BidNo"), std::string("2"));
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("3"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_rejects_multiple_global_settings_rows
TEST(ImportBidNumber, Imp01_MultipleSettingsRowsAreRejectedAndUntouched)
{
    ScratchDb db("bidno_two_rows");
    create_import_schema(db, {"7", "12"});
    ost_import::Row bid_row;
    std::string error;
    CHECK_FALSE(assign_bid_no(db, bid_row, error));
    CHECK(error.find("Settings has multiple rows; expected at most one") != std::string::npos);
    CHECK_EQ(bid_row.count("BidNo"), 0u);
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings ORDER BY NextBidNo", 1), std::string("7;12"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_normalizes_zero_to_the_documented_default
TEST(ImportBidNumber, Imp01_ZeroIsNormalisedToOne)
{
    ScratchDb db("bidno_zero");
    create_import_schema(db, {"0"});
    ost_import::Row bid_row;
    std::string error;
    CHECK(assign_bid_no(db, bid_row, error));
    CHECK_STR_EQ(bid_row.at("BidNo"), std::string("1"));
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("2"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_rejects_nonnumeric_malformed_value
TEST(ImportBidNumber, Imp01_NonNumericValueIsRejectedAndUntouched)
{
    ScratchDb db("bidno_non_numeric");
    create_import_schema(db, {"'not-a-number'"});
    ost_import::Row bid_row;
    std::string error;
    CHECK_FALSE(assign_bid_no(db, bid_row, error));
    CHECK_EQ(bid_row.count("BidNo"), 0u);
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("not-a-number"));
}

// int(value) of a stored float: truncation toward zero (Windows normalize_next_bid_number: int(2.9) == 2, int(-2.9) == -2);
// a float outside the 64-bit range (nan/inf raise, 1e30 is a big integer SQLite cannot store) fails the import and leaves the
// row alone, instead of the undefined double -> long long conversion.
TEST(ImportBidNumber, Imp01_StoredFloatsTruncateAndOutOfRangeFloatsFail)
{
    {
        ScratchDb db("bidno_float_truncates");
        create_import_schema(db, {"2.9"});
        ost_import::Row bid_row;
        std::string error;
        CHECK(assign_bid_no(db, bid_row, error));
        CHECK_EQ(bid_row.count("BidNo"), 1u);
        CHECK_STR_EQ(bid_row["BidNo"], std::string("2"));
        CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("3"));
    }
    {
        ScratchDb db("bidno_float_negative");
        create_import_schema(db, {"-2.9"});
        ost_import::Row bid_row;
        std::string error;
        CHECK(assign_bid_no(db, bid_row, error));
        CHECK_STR_EQ(bid_row["BidNo"], std::string("-2"));
        CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("-1"));
    }
    for (const char *literal : {"1e30", "-1e30", "9223372036854775808.0", "1e999", "-1e999"})
    {
        SCOPED_CONTEXT(std::string("NextBidNo = ") + literal);
        ScratchDb db("bidno_float_out_of_range");
        create_import_schema(db, {literal});
        ost_import::Row bid_row;
        std::string error;
        CHECK_FALSE(assign_bid_no(db, bid_row, error));
        CHECK(!error.empty());
        CHECK_EQ(bid_row.count("BidNo"), 0u);
    }
}

// A second row is reported before a malformed first value (fetch_optional_global_settings_row runs first).
TEST(ImportBidNumber, Imp01_CardinalityIsCheckedBeforeTheValueIsRead)
{
    ScratchDb db("bidno_cardinality_first");
    create_import_schema(db, {"'not-a-number'", "5"});
    ost_import::Row bid_row;
    std::string error;
    CHECK_FALSE(assign_bid_no(db, bid_row, error));
    CHECK(error.find("Settings has multiple rows") != std::string::npos);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_does_not_swallow_sequence_write_failure
TEST(ImportBidNumber, Imp01_ASequenceWriteFailureIsReported)
{
    ScratchDb db("bidno_write_failure");
    create_import_schema(db);
    db.must("CREATE TRIGGER fail_settings_update BEFORE UPDATE ON Settings BEGIN SELECT RAISE(FAIL, 'sequence write failed'); END");
    ost_import::Row bid_row;
    std::string error;
    CHECK_FALSE(assign_bid_no(db, bid_row, error));
    CHECK(error.find("sequence write failed") != std::string::npos);
    CHECK_EQ(bid_row.count("BidNo"), 0u);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_rejects_missing_settings_table
TEST(ImportBidNumber, Imp01_MissingSettingsTableIsRejected)
{
    ScratchDb db("bidno_no_settings");
    create_import_schema(db);
    db.must("DROP TABLE Settings");
    ost_import::Row bid_row;
    std::string error;
    CHECK_FALSE(assign_bid_no(db, bid_row, error));
    CHECK(error.find("Settings table is unavailable") != std::string::npos);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_rejects_missing_next_bid_number_column
TEST(ImportBidNumber, Imp01_MissingNextBidNoColumnIsRejected)
{
    ScratchDb db("bidno_no_column");
    create_import_schema(db);
    db.must_all({"DROP TABLE Settings", "CREATE TABLE Settings (Name TEXT)"});
    ost_import::Row bid_row;
    std::string error;
    CHECK_FALSE(assign_bid_no(db, bid_row, error));
    CHECK(error.find("Settings.NextBidNo is unavailable") != std::string::npos);
}

// IMP-01 test plan: NextBidNo = 7 -> BidNo 7 and NextBidNo 8; the next call gives 8 and 9. NULL and '' give 1 then 2.
TEST(ImportBidNumber, Imp01_SequenceAdvancesByOneAndBlankStartsAtOne)
{
    {
        ScratchDb db("bidno_seven");
        create_import_schema(db, {"7"});
        ost_import::Row a, b;
        std::string error;
        CHECK(assign_bid_no(db, a, error));
        CHECK_STR_EQ(a.at("BidNo"), std::string("7"));
        CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("8"));
        CHECK(assign_bid_no(db, b, error));
        CHECK_STR_EQ(b.at("BidNo"), std::string("8"));
        CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("9"));
    }
    for (const char *blank : {"NULL", "''"})
    {
        SCOPED_CONTEXT(std::string("NextBidNo = ") + blank);
        ScratchDb db(std::string("bidno_blank_") + (blank[0] == 'N' ? "null" : "empty"));
        create_import_schema(db, {blank});
        ost_import::Row bid_row;
        std::string error;
        CHECK(assign_bid_no(db, bid_row, error));
        CHECK_STR_EQ(bid_row.at("BidNo"), std::string("1"));
        CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("2"));
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_bid_number_assignment_uses_exact_sql_and_closes_its_cursor
// (the outcome half: Settings.NextBidNo = 5 -> {"BidNo": "5"} and the stored value becomes 6).
TEST(ImportBidNumber, Imp01_BidRowGetsExactlyTheBidNoKey)
{
    ScratchDb db("bidno_exact");
    create_import_schema(db, {"5"});
    ost_import::Row bid_row;
    std::string error;
    CHECK(assign_bid_no(db, bid_row, error));
    CHECK_EQ(ost_itest::describe(bid_row), std::string("BidNo=5"));
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("6"));
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-05: typed value conversion (_convert_access_value)
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    ImportValue convert(const std::string &value, const std::string &type_name)
    {
        ImportValue out;
        CHECK(ost_store::import_convert_value(value, type_name, out));
        return out;
    }

    std::string show(const ImportValue &v)
    {
        switch (v.kind)
        {
        case ImportValue::Null:
            return "NULL";
        case ImportValue::Integer:
            return "int:" + std::to_string(v.integer);
        case ImportValue::Real:
            return "real:" + ost_test::detail::format_float(v.real);
        case ImportValue::Text:
            return "text:" + v.text;
        }
        return "?";
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_value_conversion_follows_access_column_types
// (the part that is not DATETIME/IMAGE conversion, which is IMP-20).
TEST(ImportValues, Imp05_NullAndTheWordNullAreNullForEveryColumnType)
{
    for (const char *type_name : {"yesno", "datetime", "longbinary", "integer", "text"})
    {
        SCOPED_CONTEXT(std::string("type ") + type_name);
        CHECK_EQ(show(convert("NULL", type_name)), std::string("NULL"));
    }
}

TEST(ImportValues, Imp05_YesNoColumnsStoreMinusOneOrZero)
{
    for (const char *truthy : {"True", "true", "-1", "1"})
    {
        SCOPED_CONTEXT(std::string("yesno ") + truthy);
        CHECK_EQ(show(convert(truthy, "yesno")), std::string("int:-1"));
    }
    for (const char *falsy : {"0", "False", "", "false", "TRUE", "yes", "2", " 1"})
    {
        SCOPED_CONTEXT(std::string("yesno '") + falsy + "'");
        CHECK_EQ(show(convert(falsy, "yesno")), std::string("int:0"));
    }
}

TEST(ImportValues, Imp05_NumericColumnsFollowTheWindowsTable)
{
    struct Case
    {
        const char *type_name;
        const char *value;
        const char *expected;
    };
    const Case cases[] = {
        {"integer", "7", "int:7"},     {"integer", "0", "int:0"},         {"integer", "-3", "int:-3"},
        {"double", "1.5", "real:1.5"}, {"double", "4", "int:4"},          {"integer", "", "NULL"},
        {"integer", "abc", "NULL"},    {"counter", "12", "int:12"},       {"varchar", "007", "text:007"},
        {"", "plain", "text:plain"},   {"integer", "1.5", "real:1.5"},    {"real", "2.25", "real:2.25"},
        {"integer", " 8 ", "int:8"},   {"integer", "1_0", "int:10"},      {"integer", "1e3", "NULL"},
        {"text", "5", "text:5"},       {"text", "", "text:"},             {"blob", "5", "text:5"},
        {"bigint", "9", "int:9"},      {"smallint", "-2", "int:-2"},      {"decimal", "3.", "real:3"},
        {"numeric", "x.y", "NULL"},    {"float", ".5", "real:0.5"},       {"currency", "0", "int:0"},
        {"single", "2", "int:2"},      {"byte", "255", "int:255"},        {"bit", "1", "int:1"},
        {"short", "", "NULL"},         {"long", "NULL", "NULL"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string(c.type_name) + " / '" + c.value + "'");
        CHECK_EQ(show(convert(c.value, c.type_name)), std::string(c.expected));
    }
}

TEST(ImportValues, Imp05_AnIntegerBeyond64BitsCannotBeStored)
{
    ImportValue out;
    CHECK_FALSE(ost_store::import_convert_value("99999999999999999999", "integer", out));
    CHECK(ost_store::import_convert_value("99999999999999999999", "text", out)); // text keeps it
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_raw_row_insert_filters_columns_and_omits_blank_autonumber
TEST(ImportValues, Imp05_RawRowInsertFiltersColumnsAndOmitsBlankAutonumber)
{
    ScratchDb db("raw_row_insert");
    db.must("CREATE TABLE Sample (UID COUNTER, Name TEXT, Count INTEGER, Flag YESNO)");
    Connection conn(db);
    const ost_store::ImportTableInfo info = ost_store::import_get_table_info(conn.get(), "Sample");
    CHECK_EQ(info.columns, (std::vector<std::string>{"UID", "Name", "Count", "Flag"}));
    CHECK_STR_EQ(info.types.at("UID"), std::string("counter"));
    CHECK_STR_EQ(info.types.at("Name"), std::string("text"));
    CHECK_STR_EQ(info.types.at("Count"), std::string("integer"));
    CHECK_STR_EQ(info.types.at("Flag"), std::string("yesno"));

    std::string error;
    for (const char *blank_uid : {"", "0", "NULL"})
    {
        CHECK(ost_store::import_insert_raw_row(
            conn.get(), "Sample",
            ost_itest::row({{"UID", blank_uid}, {"Name", std::string("blank-") + blank_uid}, {"Unknown", "x"}}), info, error));
    }
    CHECK(ost_store::import_insert_raw_row(
        conn.get(), "Sample", ost_itest::row({{"UID", "5"}, {"Name", "explicit"}, {"Count", "9"}, {"Flag", "True"}}), info,
        error));
    CHECK(ost_store::import_insert_raw_row(conn.get(), "Sample", ost_itest::row({{"Unknown", "only"}}), info, error));
    CHECK_EQ(db.rows("SELECT UID, Name, Count, Flag FROM Sample", 4),
             std::string("NULL|blank-|NULL|NULL;NULL|blank-0|NULL|NULL;NULL|blank-NULL|NULL|NULL;5|explicit|9|-1"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_only_autonumber_uid_columns_have_blank_values_left_to_the_engine
TEST(ImportValues, Imp05_OnlyAutonumberUidColumnsHaveBlankValuesLeftToTheEngine)
{
    ScratchDb db("raw_row_plain_uid");
    db.must("CREATE TABLE Plain (UID INTEGER, Name TEXT)");
    Connection conn(db);
    const ost_store::ImportTableInfo info = ost_store::import_get_table_info(conn.get(), "Plain");
    std::string error;
    CHECK(ost_store::import_insert_raw_row(conn.get(), "Plain", ost_itest::row({{"UID", "0"}, {"Name", "zero"}}), info, error));
    CHECK(ost_store::import_insert_raw_row(conn.get(), "Plain", ost_itest::row({{"UID", ""}, {"Name", "blank"}}), info, error));
    // a plain INTEGER column converts "0" -> 0 and "" -> NULL (the Windows numeric rule), it is not left to the engine
    CHECK_EQ(db.rows("SELECT UID, Name FROM Plain ORDER BY Name", 2), std::string("NULL|blank;0|zero"));
    ost_store::ImportTableInfo counter;
    counter.columns = {"UID", "Name"};
    counter.types = {{"UID", "counter"}, {"Name", "counter"}};
    CHECK_FALSE(ost_store::import_should_omit_autonumber_uid(ost_store::ImportTableInfo(), "UID", ""));
    CHECK(ost_store::import_should_omit_autonumber_uid(counter, "UID", ""));
    CHECK_FALSE(ost_store::import_should_omit_autonumber_uid(counter, "Name", ""));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_a_column_without_type_information_keeps_its_text_value
TEST(ImportValues, Imp05_AColumnWithoutTypeInformationKeepsItsTextValue)
{
    ScratchDb db("raw_row_untyped");
    db.must("CREATE TABLE Plain (A INTEGER, B TEXT)");
    Connection conn(db);
    ost_store::ImportTableInfo info;
    info.columns = {"A", "B"};
    info.types = {{"A", "integer"}};
    std::string error;
    CHECK(ost_store::import_insert_raw_row(conn.get(), "Plain", ost_itest::row({{"A", "5"}, {"B", "007"}}), info, error));
    CHECK_EQ(db.rows("SELECT A, B, typeof(B) FROM Plain", 3), std::string("5|007|text"));
}

// IMP-05 / IMP-M04 (text part): a text column keeps '' as the empty text (typeof = text), numeric columns turn '' into NULL.
TEST(ImportValues, Imp05_EmptyTextIsKeptAndEmptyNumbersAreNull)
{
    ScratchDb db("raw_row_empty");
    db.must("CREATE TABLE T (UID INTEGER PRIMARY KEY, JobName TEXT, Count INTEGER, Flag YESNO)");
    Connection conn(db);
    const ost_store::ImportTableInfo info = ost_store::import_get_table_info(conn.get(), "T");
    std::string error;
    CHECK(ost_store::import_insert_raw_row(
        conn.get(), "T", ost_itest::row({{"UID", "6"}, {"JobName", ""}, {"Count", ""}, {"Flag", ""}}), info, error));
    CHECK_EQ(db.rows("SELECT typeof(JobName), JobName, typeof(Count), typeof(Flag), Flag FROM T", 5),
             std::string("text||null|integer|0"));
}

// IMP-05: the iOS schema flattened the 85 YESNO columns to INTEGER; the importer still treats them as YESNO.
TEST(ImportValues, Imp05_YesNoColumnsOfTheIosSchemaReportTheYesNoType)
{
    ScratchDb db("yesno_ios_schema", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    Connection conn(db);
    const ost_store::ImportTableInfo layers = ost_store::import_get_table_info(conn.get(), "BidLayers");
    const ost_store::ImportTableInfo conditions = ost_store::import_get_table_info(conn.get(), "BidConditions");
    const ost_store::ImportTableInfo bids = ost_store::import_get_table_info(conn.get(), "Bids");
    CHECK_STR_EQ(layers.types.at("Show"), std::string("yesno"));
    CHECK_STR_EQ(layers.types.at("Sequence"), std::string("integer"));
    CHECK_STR_EQ(layers.types.at("UID"), std::string("integer"));
    CHECK(layers.primary_key.at("UID"));
    CHECK_STR_EQ(conditions.types.at("Backout"), std::string("yesno"));
    CHECK_STR_EQ(bids.types.at("IsAccepted"), std::string("yesno"));
    CHECK_EQ(ost_schema::YESNO_COLUMNS.size(), 85u);
    // every YESNO column that the iOS schema declares is reported as yesno
    for (const auto &entry : ost_schema::YESNO_COLUMNS)
    {
        const ost_store::ImportTableInfo info = ost_store::import_get_table_info(conn.get(), entry.first);
        if (info.has_column(entry.second))
            CHECK_STR_EQ(info.types.at(entry.second), std::string("yesno"));
    }
    // a UID INTEGER PRIMARY KEY column is the autonumber of the iOS schema
    CHECK(ost_store::import_should_omit_autonumber_uid(layers, "UID", "0"));
    CHECK(ost_store::import_should_omit_autonumber_uid(layers, "UID", "NULL"));
    CHECK_FALSE(ost_store::import_should_omit_autonumber_uid(layers, "UID", "7"));
    CHECK_FALSE(ost_store::import_should_omit_autonumber_uid(layers, "Sequence", "0"));
}

// A table the database does not have is described as empty: its rows have no known column and are skipped.
TEST(ImportValues, Imp08_UnknownTableAndColumnsAreSkippedNotFailed)
{
    ScratchDb db("raw_row_unknown");
    db.must("CREATE TABLE Known (UID INTEGER PRIMARY KEY, Name TEXT)");
    Connection conn(db);
    const ost_store::ImportTableInfo none = ost_store::import_get_table_info(conn.get(), "Missing");
    CHECK(none.columns.empty());
    std::string error;
    CHECK(ost_store::import_insert_raw_row(conn.get(), "Missing", ost_itest::row({{"UID", "1"}}), none, error));
    const ost_store::ImportTableInfo info = ost_store::import_get_table_info(conn.get(), "Known");
    CHECK(ost_store::import_insert_raw_row(conn.get(), "Known", ost_itest::row({{"UID", "1"}, {"Name", "a"}, {"Bogus", "x"}}), info, error));
    // attribute names compare exactly (Python set membership): 'name' is not the column 'Name'
    CHECK(ost_store::import_insert_raw_row(conn.get(), "Known", ost_itest::row({{"UID", "2"}, {"name", "b"}}), info, error));
    CHECK_EQ(db.rows("SELECT UID, Name FROM Known ORDER BY UID", 2), std::string("1|a;2|NULL"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_a_failed_insert_still_closes_its_cursor_and_propagates
// and ::test_write_failures_name_the_table_row_and_sorted_columns: the failure carries the Windows log line.
TEST(ImportValues, Imp08_AFailedInsertReportsTableRowUidAndSortedColumns)
{
    ScratchDb db("raw_row_failure");
    db.must_all({"CREATE TABLE BidAreas (UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT, Sequence INTEGER)",
                 "INSERT INTO BidAreas VALUES (30, 5, 'A', NULL)"});
    Connection conn(db);
    const ost_store::ImportTableInfo info = ost_store::import_get_table_info(conn.get(), "BidAreas");
    std::string error;
    CHECK_FALSE(ost_store::import_insert_raw_row(
        conn.get(), "BidAreas", ost_itest::row({{"Sequence", "1"}, {"UID", "30"}, {"Name", "B"}}), info, error));
    CHECK_STR_EQ(error, std::string("Failed inserting into [BidAreas], row_uid=30, columns=['Name', 'Sequence', 'UID'], "
                                    "error=UNIQUE constraint failed: BidAreas.UID"));
    // the first row is untouched (a plain INSERT, never INSERT OR REPLACE)
    CHECK_EQ(db.rows("SELECT UID, BidUID, Name FROM BidAreas", 3), std::string("30|5|A"));
}
