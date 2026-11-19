// UND-06: the undo of a delete puts the row back with ALL its columns, not only the ten typed ones.
//
// Windows: presentation/handlers/plan_view_action_handler.py on_elements_deleted (saved_takeoff_extras via get_takeoff_extras
// -> InsertTakeoffSpec.raw_extras), infrastructure/mdb/components/bid_data_reader.py _parse_bid_takeoffs_for_bid (the extras of a
// takeoff: every column that is not typed) and takeoff_operations.py insert_takeoffs lines 534-550 (the extras are written
// except typed and undeclared columns; the bid-scoped references are dropped on a cross-bid insert; the GUID is regenerated).
// iOS keeps the uid of a restored row (Windows allocates a new one: the history is LIFO, so the difference is harmless).
#include "SettingsTestSupport.hpp"

#include "Domain/Takeoff.hpp"

#include <string>
#include <vector>

using ost_domain::Takeoff;
using ost_domain::TakeoffExtraValue;
using ost_stest::TempDb;

namespace
{
    // Port of tests/infrastructure/mdb/components/test_takeoff_operations.py::_GUID_PATTERN
    bool looks_like_guid(const std::string &text)
    {
        // ^\{[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}\}$
        if (text.size() != 38 || text.front() != '{' || text.back() != '}')
            return false;
        for (std::size_t i = 1; i + 1 < text.size(); ++i)
        {
            const bool dash = (i == 9 || i == 14 || i == 19 || i == 24);
            const char c = text[i];
            if (dash ? c != '-' : !((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')))
                return false;
        }
        return true;
    }

    // Every column of BidTakeoffs except GUID (a restore regenerates it) as quote()d text, so a type change shows.
    const char *kColumns[] = {"UID", "BidUID", "BidConditionUID", "BidZoneUID", "BidPageUID", "BidAreaUID",
                              "BidTypAreaUID", "ParentUID", "No", "Quantity", "Count", "Rotation", "Position",
                              "GridOffsetX", "GridOffsetY", "GridRotation", "IsNegativeQuantity", "FontName",
                              "FontColor", "FontSize", "FontBold", "FontItalic", "FontUnderline",
                              "TypGroupTakeoffUID", "TypPageTakeoffUID", "TakeoffModified", "TypGroupUID",
                              "TypGroupMarkerUID", "FlipX", "FlipY", "NameFontName", "NameFontColor",
                              "NameFontSize", "NameFontBold", "NameFontItalic", "NameFontUnderline", "Curve"};

    std::string row_without_guid(const TempDb &db, long long uid)
    {
        std::string select = "SELECT ";
        bool first = true;
        for (const char *column : kColumns)
        {
            select += std::string(first ? "" : " || '|' || ") + "quote([" + column + "])";
            first = false;
        }
        return db.text(select + " FROM BidTakeoffs WHERE UID = " + std::to_string(uid));
    }

    // Takeoff 700 of bid 10 with a value in every column (the stored form the importer and the app write).
    void insert_full_row(const TempDb &db)
    {
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidZoneUID, BidPageUID, BidAreaUID, BidTypAreaUID, "
                "ParentUID, No, Quantity, Count, Rotation, Position, GridOffsetX, GridOffsetY, GridRotation, "
                "IsNegativeQuantity, FontName, FontColor, FontSize, FontBold, FontItalic, FontUnderline, "
                "TypGroupTakeoffUID, TypPageTakeoffUID, TakeoffModified, TypGroupUID, TypGroupMarkerUID, FlipX, FlipY, GUID, "
                "NameFontName, NameFontColor, NameFontSize, NameFontBold, NameFontItalic, NameFontUnderline, Curve) "
                "VALUES (700, 10, 30, 5, 100, 60, 6, 0, 7, 12.5, 3, 0.5, '1.0;2.0;3.0;4.0\n', 1.5, -2.25, 0.125, 1, "
                "'Arial', 255, -12, 1, 0, 1, 8, 9, 1, 4, 3, 1, 0, '{AAAA}', 'Verdana', 65280, 10, 0, 1, 0, 2)");
    }

    // The typed fields of row 700, as the in-memory takeoff holds them.
    Takeoff typed_takeoff_700()
    {
        Takeoff t;
        t.uid = "700";
        t.condition_uid = "30";
        t.page_uid = "100";
        t.area_uid = "60";
        t.position = {1.0, 2.0, 3.0, 4.0};
        t.rotation = 0.5;
        t.curve = 2;
        t.parent_uid = "0";
        t.is_negative = true;
        return t;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the snapshot
// ---------------------------------------------------------------------------------------------------------------

// bid_data_reader._parse_bid_takeoffs_for_bid: takeoff_extras[uid] = {every column that is not one of the ten typed ones}.
TEST(TakeoffRestore, TheSnapshotHoldsEveryColumnButTheTypedOnes)
{
    TempDb db("snapshot");
    REQUIRE(db.schema_ok());
    db.seed_world();
    insert_full_row(db);

    ost_domain::TakeoffExtras extras;
    REQUIRE(ost_store::read_takeoff_extras(db.path(), "10", "700", extras));
    CHECK_EQ(extras.columns.size(), static_cast<size_t>(28)); // 38 columns of the table - 10 typed ones
    for (const char *typed : {"UID", "BidUID", "BidConditionUID", "BidPageUID", "BidAreaUID", "Position", "Rotation",
                              "Curve", "ParentUID", "IsNegativeQuantity"})
        CHECK_EQ(extras.columns.count(typed), static_cast<size_t>(0));
    CHECK_FALSE(extras.source_bid_uid.has_value());

    // storage classes and values as stored
    REQUIRE_EQ(extras.columns.count("No"), static_cast<size_t>(1));
    CHECK(extras.columns["No"].kind == TakeoffExtraValue::Kind::Integer);
    CHECK_EQ(extras.columns["No"].integer, 7LL);
    CHECK(extras.columns["Quantity"].kind == TakeoffExtraValue::Kind::Real);
    CHECK_EQ(extras.columns["Quantity"].real, 12.5);
    CHECK_EQ(extras.columns["GridOffsetX"].real, 1.5);
    CHECK_EQ(extras.columns["GridOffsetY"].real, -2.25);
    CHECK(extras.columns["FontName"].kind == TakeoffExtraValue::Kind::Text);
    CHECK_EQ(extras.columns["FontName"].text, std::string("Arial"));
    CHECK_EQ(extras.columns["FontSize"].integer, -12LL);
    CHECK_EQ(extras.columns["GUID"].text, std::string("{AAAA}"));
    CHECK_EQ(extras.columns["BidZoneUID"].integer, 5LL);
}

TEST(TakeoffRestore, ANullColumnIsCapturedAsNull)
{
    TempDb db("snapshot_null");
    REQUIRE(db.schema_ok());
    db.seed_world();
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position, FontName) "
            "VALUES (701, 10, 30, 100, '1.0;2.0\n', 'Arial')");
    ost_domain::TakeoffExtras extras;
    REQUIRE(ost_store::read_takeoff_extras(db.path(), "10", "701", extras));
    REQUIRE_EQ(extras.columns.count("No"), static_cast<size_t>(1));
    CHECK(extras.columns["No"].kind == TakeoffExtraValue::Kind::Null);
    CHECK(extras.columns["FontName"].kind == TakeoffExtraValue::Kind::Text);
}

TEST(TakeoffRestore, TheSnapshotRefusesAMissingTakeoffOrAnotherBidsTakeoff)
{
    TempDb db("snapshot_refuse");
    REQUIRE(db.schema_ok());
    db.seed_world();
    insert_full_row(db);
    ost_domain::TakeoffExtras extras;
    extras.columns["Stale"] = TakeoffExtraValue::make_integer(1);
    CHECK_FALSE(ost_store::read_takeoff_extras(db.path(), "10", "999", extras));
    CHECK(extras.empty());
    extras.columns["Stale"] = TakeoffExtraValue::make_integer(1);
    CHECK_FALSE(ost_store::read_takeoff_extras(db.path(), "11", "700", extras)); // row 700 belongs to bid 10
    CHECK(extras.empty());
    CHECK_FALSE(ost_store::read_takeoff_extras(db.path(), "10", "abc", extras));
    CHECK_FALSE(ost_store::read_takeoff_extras(db.path(), "x", "700", extras));
}

// ---------------------------------------------------------------------------------------------------------------
// the restore
// ---------------------------------------------------------------------------------------------------------------

// Test plan of UND-06: insert a row with No=7, Count=3, GridOffsetX=1.5, FontName='Arial', GUID='{AAAA}' (and a value in every
// other column), capture the full row, delete_takeoff, then restore. Every column except the GUID equals the original (iOS
// used to restore only the typed columns: Quantity, Count, No, GridOffsetX/Y, GridRotation, Font* and the rest came back NULL).
TEST(TakeoffRestore, DeleteThenRestoreBringsEveryColumnBack)
{
    TempDb db("restore_all");
    REQUIRE(db.schema_ok());
    db.seed_world();
    insert_full_row(db);
    const std::string original = row_without_guid(db, 700);
    CHECK_EQ(original.find("NULL"), std::string::npos); // a value in every column: the helper reads the whole row
    CHECK_STR_EQ(db.text("SELECT quote(No) || quote(Count) || quote(GridOffsetX) || quote(FontName) FROM BidTakeoffs "
                         "WHERE UID = 700"),
                 "73.01.5'Arial'");

    Takeoff takeoff = typed_takeoff_700();
    REQUIRE(ost_store::read_takeoff_extras(db.path(), "10", "700", takeoff.extras));
    REQUIRE(ost_store::delete_takeoff(db.path(), "700"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = 700"), 0);

    CHECK_EQ(ost_store::insert_takeoff(db.path(), "10", takeoff), std::string("700"));
    CHECK_STR_EQ(row_without_guid(db, 700), original);
    CHECK_STR_EQ(db.takeoff_cell(700, "No"), "7");
    CHECK_STR_EQ(db.takeoff_cell(700, "Count"), "3.0");
    CHECK_STR_EQ(db.takeoff_cell(700, "GridOffsetX"), "1.5");
    CHECK_STR_EQ(db.takeoff_cell(700, "FontName"), "'Arial'");
    // the GUID is regenerated, never copied
    const std::string guid = db.text("SELECT GUID FROM BidTakeoffs WHERE UID = 700");
    CHECK(looks_like_guid(guid));
    CHECK_NE(guid, std::string("{AAAA}"));
}

// A takeoff with an empty snapshot (a new takeoff, or an undo that has no snapshot) is written exactly as before: the typed
// columns only, everything else NULL.
TEST(TakeoffRestore, WithoutExtrasOnlyTheTypedColumnsAreWritten)
{
    TempDb db("restore_none");
    REQUIRE(db.schema_ok());
    db.seed_world();
    Takeoff takeoff = typed_takeoff_700();
    CHECK_EQ(ost_store::insert_takeoff(db.path(), "10", takeoff), std::string("700"));
    CHECK_STR_EQ(db.takeoff_cell(700, "No"), "NULL");
    CHECK_STR_EQ(db.takeoff_cell(700, "GUID"), "NULL");
    CHECK_STR_EQ(db.takeoff_cell(700, "FontName"), "NULL");
    CHECK_STR_EQ(db.takeoff_cell(700, "Rotation"), "0.5");
    CHECK_STR_EQ(db.takeoff_cell(700, "Curve"), "2");
}

namespace
{
    // Port of tests/infrastructure/mdb/components/test_takeoff_operations.py::_EXTRAS (the bid-scoped references and the
    // typed / undeclared columns the writer must handle). iOS integer values; 'NotAColumn' is declared by no table.
    ost_domain::TakeoffExtras windows_extras()
    {
        ost_domain::TakeoffExtras extras;
        auto &c = extras.columns;
        c["UID"] = TakeoffExtraValue::make_integer(999);
        c["NotAColumn"] = TakeoffExtraValue::make_integer(1);
        c["BidUID"] = TakeoffExtraValue::make_integer(2);
        c["BidConditionUID"] = TakeoffExtraValue::make_integer(6);
        c["BidPageUID"] = TakeoffExtraValue::make_integer(9);
        c["BidAreaUID"] = TakeoffExtraValue::make_integer(31);
        c["Position"] = TakeoffExtraValue::make_text("x");
        c["Rotation"] = TakeoffExtraValue::make_real(9.0);
        c["Curve"] = TakeoffExtraValue::make_integer(5);
        c["ParentUID"] = TakeoffExtraValue::make_integer(123);
        c["IsNegativeQuantity"] = TakeoffExtraValue::make_integer(1);
        c["GUID"] = TakeoffExtraValue::make_text("{OLD-GUID}");
        c["BidZoneUID"] = TakeoffExtraValue::make_integer(77);
        c["TypGroupUID"] = TakeoffExtraValue::make_integer(5);
        c["BidTypAreaUID"] = TakeoffExtraValue::make_integer(6);
        c["TypGroupTakeoffUID"] = TakeoffExtraValue::make_integer(7);
        c["TypPageTakeoffUID"] = TakeoffExtraValue::make_integer(8);
        c["TypGroupMarkerUID"] = TakeoffExtraValue::make_integer(9);
        c["FontName"] = TakeoffExtraValue::make_text("Arial");
        return extras;
    }

    Takeoff plain_takeoff()
    {
        Takeoff t;
        t.condition_uid = "30";
        t.page_uid = "100";
        t.position = {0.0, 0.0, 1.0, 1.0};
        return t;
    }
}

// Port of tests/infrastructure/mdb/components/test_takeoff_operations.py::
// test_raw_extras_keep_declared_columns_and_regenerate_the_guid: two takeoffs inserted with the same extras (typed columns and
// an undeclared column among them, no source bid / the same bid as the target). The typed columns keep the typed values, the
// declared extras are written, the GUID is regenerated (valid, and different for each row).
TEST(TakeoffRestore, ExtrasKeepDeclaredColumnsAndRegenerateTheGuid)
{
    const std::vector<std::optional<std::string>> sources = {std::nullopt, std::string("10")};
    for (const auto &source : sources)
    {
        SCOPED_CONTEXT(std::string("source bid ") + (source ? *source : std::string("none")));
        TempDb db("restore_windows");
        REQUIRE(db.schema_ok());
        db.seed_world();
        db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) "
                "VALUES (7, 10, 30, 100, '0.0;0.0\n')");
        // (The Windows test also pins the allocated uids 8 and 9; iOS allocates them one insert at a time, and the extras
        // of the first row reference takeoff 9 (TypGroupMarkerUID), so only the first uid, MAX(UID) + 1, is pinned here.)
        std::vector<long long> new_uids;
        for (int i = 0; i < 2; ++i)
        {
            Takeoff takeoff = plain_takeoff();
            takeoff.extras = windows_extras();
            takeoff.extras.source_bid_uid = source;
            const std::string uid = ost_store::insert_takeoff(db.path(), "10", takeoff);
            REQUIRE(!uid.empty());
            new_uids.push_back(std::stoll(uid));
        }
        CHECK_EQ(new_uids[0], 8LL);
        REQUIRE(new_uids[0] != new_uids[1]);
        std::vector<std::string> guids;
        for (long long uid : new_uids)
        {
            SCOPED_CONTEXT("uid " + std::to_string(uid));
            // typed columns: the typed values win
            CHECK_STR_EQ(db.takeoff_cell(uid, "BidUID"), "10");
            CHECK_STR_EQ(db.takeoff_cell(uid, "BidConditionUID"), "30");
            CHECK_STR_EQ(db.takeoff_cell(uid, "BidPageUID"), "100");
            CHECK_STR_EQ(db.takeoff_cell(uid, "BidAreaUID"), "NULL");
            CHECK_STR_EQ(db.takeoff_cell(uid, "Position"), "'0.0;0.0;1.0;1.0\n'");
            CHECK_STR_EQ(db.takeoff_cell(uid, "Rotation"), "0.0");
            CHECK_STR_EQ(db.takeoff_cell(uid, "Curve"), "-1");
            CHECK_STR_EQ(db.takeoff_cell(uid, "ParentUID"), "0");
            CHECK_STR_EQ(db.takeoff_cell(uid, "IsNegativeQuantity"), "0");
            // declared extras are kept, same bid
            CHECK_STR_EQ(db.takeoff_cell(uid, "BidZoneUID"), "77");
            CHECK_STR_EQ(db.takeoff_cell(uid, "TypGroupUID"), "5");
            CHECK_STR_EQ(db.takeoff_cell(uid, "BidTypAreaUID"), "6");
            CHECK_STR_EQ(db.takeoff_cell(uid, "TypGroupTakeoffUID"), "7");
            CHECK_STR_EQ(db.takeoff_cell(uid, "TypPageTakeoffUID"), "8");
            CHECK_STR_EQ(db.takeoff_cell(uid, "TypGroupMarkerUID"), "9");
            CHECK_STR_EQ(db.takeoff_cell(uid, "FontName"), "'Arial'");
            guids.push_back(db.text("SELECT GUID FROM BidTakeoffs WHERE UID = " + std::to_string(uid)));
        }
        REQUIRE_EQ(guids.size(), static_cast<size_t>(2));
        CHECK_NE(guids[0], guids[1]);
        for (const std::string &guid : guids)
            CHECK(looks_like_guid(guid));
    }
}

// Port of tests/infrastructure/mdb/components/test_takeoff_operations.py::
// test_raw_extras_from_another_bid_drop_bid_scoped_references_only: extras that come from ANOTHER bid lose the bid-scoped
// references (BidZoneUID, TypGroupUID, BidTypAreaUID, TypGroupTakeoffUID, TypPageTakeoffUID, TypGroupMarkerUID become NULL)
// and keep everything else; the GUID is regenerated.
TEST(TakeoffRestore, ExtrasFromAnotherBidDropTheBidScopedReferencesOnly)
{
    TempDb db("restore_cross_bid");
    REQUIRE(db.schema_ok());
    db.seed_world();
    Takeoff takeoff = plain_takeoff();
    takeoff.extras = windows_extras();
    takeoff.extras.source_bid_uid = std::string("11");
    REQUIRE_EQ(ost_store::insert_takeoff(db.path(), "10", takeoff), std::string("1"));
    for (const char *column : {"BidZoneUID", "TypGroupUID", "BidTypAreaUID", "TypGroupTakeoffUID", "TypPageTakeoffUID",
                               "TypGroupMarkerUID"})
        CHECK_STR_EQ(db.takeoff_cell(1, column), "NULL");
    CHECK_STR_EQ(db.takeoff_cell(1, "FontName"), "'Arial'");
    CHECK(looks_like_guid(db.text("SELECT GUID FROM BidTakeoffs WHERE UID = 1")));
}

// A restore writes the typed values for the columns the snapshot does not hold and never leaves the row half written: a
// restore that fails (here: the page is not one of the bid) writes nothing, extras included.
TEST(TakeoffRestore, ARefusedRestoreWritesNothing)
{
    TempDb db("restore_refused");
    REQUIRE(db.schema_ok());
    db.seed_world();
    Takeoff takeoff = typed_takeoff_700();
    takeoff.page_uid = "101"; // a page of bid 11
    takeoff.extras = windows_extras();
    CHECK_EQ(ost_store::insert_takeoff(db.path(), "10", takeoff), std::string());
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 0);
}

// An extra whose storage class is Null, Integer, Real or Text comes back with that storage class.
TEST(TakeoffRestore, TheStorageClassesOfTheExtrasSurvive)
{
    TempDb db("restore_types");
    REQUIRE(db.schema_ok());
    db.seed_world();
    Takeoff takeoff = typed_takeoff_700();
    takeoff.extras.columns["No"] = TakeoffExtraValue::make_integer(7);
    takeoff.extras.columns["Quantity"] = TakeoffExtraValue::make_real(12.5);
    takeoff.extras.columns["FontName"] = TakeoffExtraValue::make_text("");
    takeoff.extras.columns["NameFontName"] = TakeoffExtraValue::make_null();
    CHECK_EQ(ost_store::insert_takeoff(db.path(), "10", takeoff), std::string("700"));
    CHECK_STR_EQ(db.text("SELECT typeof(No) FROM BidTakeoffs WHERE UID = 700"), "integer");
    CHECK_STR_EQ(db.text("SELECT typeof(Quantity) FROM BidTakeoffs WHERE UID = 700"), "real");
    CHECK_STR_EQ(db.text("SELECT typeof(FontName) FROM BidTakeoffs WHERE UID = 700"), "text");
    CHECK_STR_EQ(db.text("SELECT typeof(NameFontName) FROM BidTakeoffs WHERE UID = 700"), "null");
    CHECK_STR_EQ(db.text("SELECT quote(Quantity) FROM BidTakeoffs WHERE UID = 700"), "12.5");
}
