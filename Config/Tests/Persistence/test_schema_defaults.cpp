// DBP-33 (user decision: DEFAULT clauses only): the schema of a NEW database carries the 61 field defaults the Windows database
// creator applies (reference_schema_metadata.py FIELD_DEFAULTS, e.g. BidTakeoffs Count / GridOffsetX / GridOffsetY / GridRotation /
// IsNegativeQuantity / FontColor / NameFontColor = 0, Settings BackupNo = 2 / BackupPeriod = 2 / CompressPeriod = 3), and
// insert_takeoff writes the BidTakeoffs defaults explicitly so a database created before this change gets them too. NO foreign
// keys: the delete order and the import stay as they are.
// Windows oracle: ost_visualizer/infrastructure/mdb/reference_schema_metadata.py (the data of SchemaDefaultsOracleData.hpp),
// database_creator.py::_apply_reference_schema_metadata (Fields(field).DefaultValue = default) and
// tests/infrastructure/mdb/test_reference_schema_metadata.py::test_field_defaults_match_the_reference_exactly (61 defaults, only
// the three Settings backup values are not 0).
#include "StoreTestSupport.hpp"

#include "SchemaDefaultsOracleData.hpp"

#include <map>
#include <set>
#include <string>
#include <utility>

using ost_ptest::TempDb;

namespace
{
    // dflt_value of every column of every table that has one: "Table.Column" -> default text.
    std::map<std::string, std::string> declared_defaults(const TempDb &db)
    {
        std::map<std::string, std::string> out;
        ost_db::OstDatabase connection;
        REQUIRE(connection.open(db.path()));
        ost_db::Statement stmt = connection.prepare(
            "SELECT m.name, p.name, p.dflt_value FROM sqlite_master m, pragma_table_info(m.name) p "
            "WHERE m.type = 'table' AND m.name NOT LIKE 'sqlite_%' AND p.dflt_value IS NOT NULL");
        REQUIRE(stmt.valid());
        while (stmt.step())
            out[stmt.column_text(0) + "." + stmt.column_text(1)] = stmt.column_text(2);
        REQUIRE(!stmt.failed());
        return out;
    }

    // The BidTakeoffs defaults the decision names: Count, GridOffsetX / Y, GridRotation, FontColor and NameFontColor (NameFontColor is
    // a Windows default too, the decision text was corrected on 2026-11-19), plus IsNegativeQuantity, which insert_takeoff always
    // writes as a typed column.
    const char *const kTakeoffDefaultColumns[] = {"Count", "GridOffsetX", "GridOffsetY", "GridRotation", "IsNegativeQuantity",
                                                  "FontColor", "NameFontColor"};

    // "col=value col=value ..." of the typeof / value of each takeoff default column of the row with this UID.
    std::string takeoff_defaults_of(const TempDb &db, const std::string &uid)
    {
        std::string out;
        for (const char *column : kTakeoffDefaultColumns)
        {
            out += std::string(out.empty() ? "" : " ") + column + "=" +
                   db.text(std::string("SELECT ifnull(typeof([") + column + "]) || ':' || ifnull([" + column +
                           "], 'NULL'), 'none') FROM BidTakeoffs WHERE UID = " + uid);
        }
        return out;
    }

    // A database whose BidTakeoffs table has no DEFAULT clause, like every database created before this change. `drop_columns`
    // are left out of the copy (a table without them).
    void make_legacy_takeoff_table(const TempDb &db, const std::string &drop_columns = "")
    {
        std::string columns;
        {
            ost_db::OstDatabase connection;
            REQUIRE(connection.open(db.path()));
            ost_db::Statement info = connection.prepare("SELECT name FROM pragma_table_info('BidTakeoffs')");
            REQUIRE(info.valid());
            while (info.step())
            {
                const std::string name = info.column_text(0);
                if (drop_columns.find("," + name + ",") != std::string::npos)
                    continue;
                columns += (columns.empty() ? "" : ", ") + name;
            }
        }
        db.must("ALTER TABLE BidTakeoffs RENAME TO BidTakeoffs_new");
        db.must("CREATE TABLE BidTakeoffs AS SELECT " + columns + " FROM BidTakeoffs_new");
        db.must("DROP TABLE BidTakeoffs_new");
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// the schema of a new database
// ---------------------------------------------------------------------------------------------------------------

// Test plan: PRAGMA table_info(BidTakeoffs).dflt_value for Count / GridOffsetX / GridOffsetY / GridRotation /
// IsNegativeQuantity / FontColor / NameFontColor is '0'.
TEST(Dbp33_SchemaDefaults, BidTakeoffsHasTheWindowsDefaults)
{
    TempDb db("schema_defaults_takeoffs");
    REQUIRE(db.schema_ok());
    for (const char *column : kTakeoffDefaultColumns)
    {
        SCOPED_CONTEXT(std::string("BidTakeoffs.") + column);
        CHECK_STR_EQ(db.text(std::string("SELECT dflt_value FROM pragma_table_info('BidTakeoffs') WHERE name = '") + column + "'"),
                     "0");
    }
}

// All 61 entries of the Windows FIELD_DEFAULTS, and nothing else: no column of the schema has a default Windows does not
// have (the Windows model test pins that the defaults are exactly FIELD_DEFAULTS).
TEST(Dbp33_SchemaDefaults, EveryReferenceFieldDefaultIsThereAndNoOtherColumnHasOne)
{
    TempDb db("schema_defaults_all");
    REQUIRE(db.schema_ok());
    const std::map<std::string, std::string> declared = declared_defaults(db);

    std::map<std::string, std::string> expected;
    for (const ost_ptest::FieldDefault &field : ost_ptest::kReferenceFieldDefaults)
        expected[std::string(field.table) + "." + field.column] = field.value;
    CHECK_EQ(expected.size(), static_cast<size_t>(61));

    for (const auto &pair : expected)
    {
        SCOPED_CONTEXT(pair.first);
        auto found = declared.find(pair.first);
        REQUIRE(found != declared.end());
        CHECK_STR_EQ(found->second, pair.second);
    }
    for (const auto &pair : declared)
    {
        SCOPED_CONTEXT("unexpected default " + pair.first + " = " + pair.second);
        CHECK(expected.count(pair.first) == 1);
    }
    CHECK_EQ(declared.size(), static_cast<size_t>(61));
}

// Port of test_field_defaults_pin_the_non_zero_backup_settings.
TEST(Dbp33_SchemaDefaults, OnlyTheSettingsBackupValuesAreNotZero)
{
    TempDb db("schema_defaults_settings");
    REQUIRE(db.schema_ok());
    std::set<std::string> non_zero;
    for (const auto &pair : declared_defaults(db))
        if (pair.second != "0")
            non_zero.insert(pair.first + "=" + pair.second);
    CHECK(non_zero == (std::set<std::string>{"Settings.BackupNo=2", "Settings.BackupPeriod=2", "Settings.CompressPeriod=3"}));
}

// Test plan: inserting a row with only the required columns then yields the defaults.
TEST(Dbp33_SchemaDefaults, ARowInsertedWithOnlyTheRequiredColumnsGetsTheDefaults)
{
    TempDb db("schema_defaults_insert");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (1, 10, 30, 100, '1;2')");
    CHECK_STR_EQ(takeoff_defaults_of(db, "1"),
                 "Count=real:0.0 GridOffsetX=real:0.0 GridOffsetY=real:0.0 GridRotation=real:0.0 IsNegativeQuantity=integer:0 "
                 "FontColor=integer:0 NameFontColor=integer:0");
    // a column that has no Windows default stays NULL (FontSize, GUID, Curve ...)
    CHECK_STR_EQ(db.text("SELECT ifnull(FontSize, 'NULL') || ifnull(GUID, 'NULL') FROM BidTakeoffs WHERE UID = 1"), "NULLNULL");

    db.must("INSERT INTO Settings (UID) VALUES (99)");
    CHECK_STR_EQ(db.text("SELECT BackupNo || ',' || BackupPeriod || ',' || CompressPeriod || ',' || QuantitiesInLegend || ',' || "
                         "HoursPerDay || ',' || StartWeekOn || ',' || GridCountMethod || ',' || IgnoreBidAreas || ',' || "
                         "SendImageFiles FROM Settings WHERE UID = 99"),
                 "2,2,3,0,0,0,0,0,0");
    db.must("INSERT INTO BidConditions (UID, BidUID, Name, Type) VALUES (30, 10, 'C', 0)");
    CHECK_EQ(db.count("SELECT Spacing FROM BidConditions WHERE UID = 30"), 0);
    CHECK_STR_EQ(db.text("SELECT typeof(Spacing) FROM BidConditions WHERE UID = 30"), "real");
}

// An explicit NULL stays NULL: a DEFAULT only fills a column that the INSERT does not name (the import keeps the NULLs of an OST file).
TEST(Dbp33_SchemaDefaults, AnExplicitNullIsNotReplacedByTheDefault)
{
    TempDb db("schema_defaults_explicit_null");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position, Count, FontColor) "
            "VALUES (1, 10, 30, 100, '1;2', NULL, NULL)");
    CHECK_STR_EQ(db.text("SELECT ifnull(Count, 'NULL') || ',' || ifnull(FontColor, 'NULL') || ',' || GridRotation FROM BidTakeoffs WHERE UID = 1"),
                 "NULL,NULL,0.0");
}

// Decision: NO foreign keys. No table of the schema declares one (Windows' 83 relationships stay out), foreign_keys enforcement is
// off, so the delete order and INSERT OR REPLACE are untouched.
TEST(Dbp33_SchemaDefaults, NoTableDeclaresAForeignKey)
{
    TempDb db("schema_defaults_no_fk");
    REQUIRE(db.schema_ok());
    CHECK_EQ(db.count("SELECT count(*) FROM sqlite_master m, pragma_foreign_key_list(m.name) f WHERE m.type = 'table'"), 0);
    // a takeoff of a condition that does not exist is still accepted by the schema (the loader refuses it, DBP-31)
    CHECK(db.exec("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (1, 10, 777, 888, '1;2')"));
}

// ---------------------------------------------------------------------------------------------------------------
// insert_takeoff writes the Windows defaults explicitly
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    constexpr const char *kAllZero =
        "Count=real:0.0 GridOffsetX=real:0.0 GridOffsetY=real:0.0 GridRotation=real:0.0 IsNegativeQuantity=integer:0 "
        "FontColor=integer:0 NameFontColor=integer:0";
}

// A database created before this change has no DEFAULT clause: insert_takeoff must still store 0 (Windows: the Access defaults).
TEST(Dbp33_InsertTakeoffDefaults, ADatabaseWithoutDefaultClausesStillGetsTheWindowsValues)
{
    TempDb db("schema_defaults_legacy_insert");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db);
    make_legacy_takeoff_table(db);
    CHECK_EQ(db.count("SELECT count(*) FROM pragma_table_info('BidTakeoffs') WHERE dflt_value IS NOT NULL"), 0);

    const std::string uid = ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("30", "100"));
    REQUIRE(!uid.empty());
    CHECK_STR_EQ(takeoff_defaults_of(db, uid), kAllZero);
}

TEST(Dbp33_InsertTakeoffDefaults, ANewDatabaseGetsThemToo)
{
    TempDb db("schema_defaults_new_insert");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db);
    const std::string uid = ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("30", "100"));
    REQUIRE(!uid.empty());
    CHECK_STR_EQ(takeoff_defaults_of(db, uid), kAllZero);
}

// The values the plan view sets stay: a negative takeoff keeps IsNegativeQuantity = 1 (the default only fills the rest).
TEST(Dbp33_InsertTakeoffDefaults, TheTypedColumnsAreNotTouched)
{
    TempDb db("schema_defaults_typed");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db);
    make_legacy_takeoff_table(db);
    ost_domain::Takeoff takeoff = ost_ptest::make_takeoff("30", "100");
    takeoff.is_negative = true;
    takeoff.rotation = 0.5;
    const std::string uid = ost_store::insert_takeoff(db.path(), "10", takeoff);
    REQUIRE(!uid.empty());
    CHECK_STR_EQ(db.text("SELECT IsNegativeQuantity || ',' || Rotation || ',' || Position FROM BidTakeoffs WHERE UID = " + uid), "1,0.5,0.0;0.0;1.0;1.0\n");
    CHECK_STR_EQ(db.text("SELECT typeof(Count) || ',' || Count FROM BidTakeoffs WHERE UID = " + uid), "real,0.0");
}

// The undo path re-inserts a row with the columns it had (Windows raw_extras): an extra - even a NULL - wins over the default,
// a column that is not in the extras gets the default.
TEST(Dbp33_InsertTakeoffDefaults, CapturedExtrasWinOverTheDefaults)
{
    TempDb db("schema_defaults_extras");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db);
    ost_domain::Takeoff takeoff = ost_ptest::make_takeoff("30", "100");
    takeoff.extras.columns["Count"] = ost_domain::TakeoffExtraValue::make_null();
    takeoff.extras.columns["FontColor"] = ost_domain::TakeoffExtraValue::make_integer(255);
    takeoff.extras.columns["GridOffsetX"] = ost_domain::TakeoffExtraValue::make_real(2.5);
    const std::string uid = ost_store::insert_takeoff(db.path(), "10", takeoff);
    REQUIRE(!uid.empty());
    CHECK_STR_EQ(takeoff_defaults_of(db, uid),
                 "Count=null:NULL GridOffsetX=real:2.5 GridOffsetY=real:0.0 GridRotation=real:0.0 IsNegativeQuantity=integer:0 "
                 "FontColor=integer:255 NameFontColor=integer:0");
}

// A table that lacks a defaulted column (a database from an older schema) still takes the insert: only declared columns are written.
TEST(Dbp33_InsertTakeoffDefaults, AColumnTheTableLacksIsNotWritten)
{
    TempDb db("schema_defaults_missing_columns");
    REQUIRE(db.schema_ok());
    ost_ptest::seed_two_bids(db);
    make_legacy_takeoff_table(db, ",Count,NameFontColor,GridRotation,");
    CHECK_EQ(db.count("SELECT count(*) FROM pragma_table_info('BidTakeoffs') WHERE name IN ('Count', 'NameFontColor', 'GridRotation')"), 0);

    const std::string uid = ost_store::insert_takeoff(db.path(), "10", ost_ptest::make_takeoff("30", "100"));
    REQUIRE(!uid.empty());
    CHECK_STR_EQ(db.text("SELECT typeof(GridOffsetX) || ':' || GridOffsetX || ' ' || typeof(GridOffsetY) || ':' || GridOffsetY || ' ' || "
                         "typeof(FontColor) || ':' || FontColor FROM BidTakeoffs WHERE UID = " + uid),
                 "real:0.0 real:0.0 integer:0");
}
