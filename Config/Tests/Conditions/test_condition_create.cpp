// NEWCOND-02 (plan 5.1, M1, tests first): the store writer of New Condition, ost_store::insert_condition and the allocator
// ost_store::allocate_condition_identities (ProjectStore_ConditionCreate.hpp).
//
// Windows 49a2d59 (source of truth): infrastructure/mdb/components/condition_operations.py :254-350 insert_condition, :39-66 the allocator and the
// condition-number guard (:56-62), identity_allocation.py :5-47 (the UID), infrastructure/database/bid_owned_identity.py :105-202 (uids and ownership),
// infrastructure/mdb/mdb_writer.py :221-274 (required / optional columns); the tests ported below are
// tests/infrastructure/mdb/components/test_condition_operations.py (:407, :446, :487, :968, :1013, :1028, :1044, :1055, :1080, :1360, :2133, :2197) and
// tests/application/services/test_project_write_service.py (:1355, :19668). The Windows tests run on a hand-made sqlite schema; they are ported on the iOS
// schema (TempDb) where the table matters little, and on hand-made tables (TempDb without schema) where the Windows test is about the columns. ONE
// adaptation applies to every hand-made Bids table: it gets the column JobStatusUID (NULL), because the bid lock guard (plan 4.1) fails closed when it cannot
// read the status, and Windows has no such guard in its insert.
// Data-safety cases of the plan (5.1 tests-first plan, research section 4): refusals with the whole database compared before and after (snapshot_all),
// rollbacks with a failing INSERT (TempDb::block), duplicate and blank names, the name limit in UTF-16 units, non-finite numbers, a locked bid, foreign-bid
// ids, not-found ids, the 2147483647 limits of the UID and of the condition number, failed reads, and a round trip through the loader.
//
// The store functions are STUBS until the implementation lands: this file is written to FAIL first (the red run is recorded). Every refusal test ends with
// a positive control (a valid create right afterwards gets the UID and the number the refusal must not have consumed), so that a stub that only refuses
// cannot pass it.
#include "ConditionsTestSupport.hpp"

#include "Domain/ConditionCreate.hpp"
#include "Domain/ConditionFormRules.hpp"
#include "Domain/ConditionStyleChange.hpp"
#include "Domain/CoreLog.hpp"

#include <climits>
#include <cmath>
#include <limits>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <vector>

using ost_ctest::TempDb;
using ost_domain::CreateConditionSpec;

namespace
{
    constexpr const char *kNoIdentityText = "Cannot add a condition: no identity is left below 2147483647.";
    constexpr const char *kNameTooLongText = "Condition name must not be longer than 255 characters.";

    // The Windows text of ConditionNumberOverflowError (condition_operations.py:57-62): "condition" for 1, "conditions" otherwise, X = the last new number.
    std::string overflow_text(long long count, const std::string &last)
    {
        return "Cannot add " + std::to_string(count) + " condition" + (count == 1 ? "" : "s") + ": the new condition numbers would reach " + last +
               ", past 2147483647, the highest condition number.";
    }

    bool is_ost_guid(const std::string &guid)
    {
        static const std::regex pattern("^\\{[0-9A-F]{8}(-[0-9A-F]{4}){3}-[0-9A-F]{12}\\}$");
        return std::regex_match(guid, pattern);
    }

    std::string repeat(const std::string &unit, std::size_t count)
    {
        std::string out;
        for (std::size_t i = 0; i < count; ++i)
            out += unit;
        return out;
    }

    // ---- the iOS-schema world -------------------------------------------------------------------------------------------------------------------
    // bid 1 (imperial) and bid 2 (metric); own layers 30 (bid 1) and 31 (bid 2); folders 40 (bid 1) and 41 (bid 2); the condition type 7. The shared
    // template layers (no bid) come from create_schema.
    void seed_world(const TempDb &db)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (1, 1, 'Bid one', 1, 0)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (2, 1, 'Bid two', 2, 1)");
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (30, 1, 'L30', -1, 1)");
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (31, 2, 'L31', -1, 1)");
        db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (40, 1, 'F40')");
        db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (41, 2, 'F41')");
        db.must("INSERT INTO CdnTypes (UID, Name) VALUES (7, 'Concrete')");
    }

    // The Windows ConditionNumberAllocationLimitTests._db (:1344-1352): bid 1 holds 10 (number 1) and 11 (number `highest`), bid 2 holds 20 (number `highest`).
    void seed_limit_world(const TempDb &db, long long highest)
    {
        seed_world(db);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 1, 'c', 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (11, 1, " + std::to_string(highest) + ", 'c', 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (20, 2, " + std::to_string(highest) + ", 'c', 0)");
    }

    CreateConditionSpec valid(const std::string &name = "Wall")
    {
        CreateConditionSpec spec;
        spec.name = name;
        spec.condition_type = 1;
        return spec;
    }

    // The Windows _FULL_SPEC (test_condition_operations.py:881-919): every field set, type 3, cdn "7", layer "30", folder "40", notes "memo".
    CreateConditionSpec windows_full_spec()
    {
        CreateConditionSpec s;
        s.name = "Wall";
        s.condition_type = 3;
        s.backout = true;
        s.cdn_type_uid = "7";
        s.layer_uid = "30";
        s.height = 1.5;
        s.width = 2.5;
        s.depth = 3.5;
        s.thickness = 4.5;
        s.rise = 5.5;
        s.run = 6.5;
        s.display_size = 7.5;
        s.shape = 2;
        s.pattern = 3;
        s.spacing = 8.5;
        s.color_fill = 9;
        s.uom1 = 1;
        s.uom2 = 2;
        s.uom3 = 3;
        s.calc_type1 = 4;
        s.calc_type2 = 5;
        s.calc_type3 = 6;
        s.notes = "memo";
        s.drop_run = true;
        s.drop_value = 9.5;
        s.round_quantity = true;
        s.round_up = 10.5;
        s.trim = true;
        s.is_curved_segment = true;
        s.grid = true;
        s.grid_size1 = 11.5;
        s.grid_size2 = 12.5;
        s.gap = 13.5;
        s.display_dimension = true;
        s.display_name = true;
        s.display_grid_while_drawing = true;
        s.folder_uid = "40";
        return s;
    }

    // ---- reading the result ---------------------------------------------------------------------------------------------------------------------
    // "storage_class:value" of one cell of one condition ("integer:1", "real:1.5", "text:Wall", "null:NULL"): exact, type-aware.
    std::string typed(const TempDb &db, const std::string &uid, const std::string &column)
    {
        return db.text("SELECT typeof([" + column + "]) || ':' || quote([" + column + "]) FROM BidConditions WHERE UID = " + uid);
    }

    double real_of(const TempDb &db, const std::string &column, const std::string &uid)
    {
        ost_db::OstDatabase connection;
        if (!connection.open(db.path()))
            return std::nan("");
        ost_db::Statement stmt = connection.prepare("SELECT [" + column + "] FROM BidConditions WHERE UID = " + uid);
        if (!stmt.valid() || !stmt.step() || stmt.column_type(0) == ost_db::Statement::TypeNull)
            return std::nan("");
        return stmt.column_double(0);
    }

    // "quote(c1) quote(c2) ..." of one row, for a whole-row comparison with NULLs visible.
    std::string quoted_row(const TempDb &db, const std::string &uid, const std::vector<std::string> &columns)
    {
        std::string expr;
        for (const std::string &column : columns)
            expr += (expr.empty() ? "" : " || ' ' || ") + std::string("quote([") + column + "])";
        return db.text("SELECT " + expr + " FROM BidConditions WHERE UID = " + uid);
    }

    // Every table except `table`, whole and exact: proves that a success changes ONE table only.
    std::string snapshot_except(const TempDb &db, const std::string &table)
    {
        std::vector<std::string> tables;
        const std::string names = db.list("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' AND name <> '" + table +
                                          "' ORDER BY name");
        std::string current;
        for (const char c : names + ",")
        {
            if (c == ',')
            {
                if (!current.empty())
                    tables.push_back(current);
                current.clear();
            }
            else
                current += c;
        }
        return db.snapshot(tables);
    }

    // A second connection can take the write lock at once: the refused call left no transaction open.
    bool no_write_lock_left(const TempDb &db)
    {
        ost_db::OstDatabase other;
        if (!other.open(db.path()))
            return false;
        const bool began = other.begin_transaction();
        other.rollback();
        return began;
    }

    // ---- the two kinds of check -------------------------------------------------------------------------------------------------------------------
    // One refused call: nothing returned, the error as given (nullopt: not pinned), no write lock left behind. The caller compares snapshot_all around it
    // (expect_refused) or around a whole group of calls (refusal_call alone).
    void refusal_call(const TempDb &db, const std::string &bid, const CreateConditionSpec &spec, const std::optional<std::string> &want_error)
    {
        std::string error = "stale";
        CHECK_STR_EQ(ost_store::insert_condition(db.path(), bid, spec, &error), "");
        if (want_error)
            CHECK_STR_EQ(error, *want_error);
        CHECK(no_write_lock_left(db));
        CHECK_STR_EQ(ost_store::insert_condition(db.path(), bid, spec), ""); // the same refusal without the error channel
    }

    void expect_refused(const TempDb &db, const std::string &bid, const CreateConditionSpec &spec, const std::optional<std::string> &want_error = std::string())
    {
        const std::string before = db.snapshot_all();
        refusal_call(db, bid, spec, want_error);
        CHECK_EQ(db.snapshot_all(), before); // not one row, number, UID or table changed
    }

    // A create that must work: the UID and the number it must get (the positive control after a refusal: nothing was consumed).
    void expect_created(const TempDb &db, const std::string &bid, const CreateConditionSpec &spec, const std::string &uid, long long ref_no,
                        bool with_guid = true)
    {
        std::string error = "stale";
        const std::string got = ost_store::insert_condition(db.path(), bid, spec, &error);
        CHECK_STR_EQ(got, uid);
        CHECK_STR_EQ(error, "");
        if (!got.empty())
        {
            CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = " + got), ref_no);
            CHECK_EQ(db.count("SELECT BidUID FROM BidConditions WHERE UID = " + got), std::stoll(bid));
            if (with_guid)
                CHECK(is_ost_guid(db.text("SELECT GUID FROM BidConditions WHERE UID = " + got)));
        }
    }

    // Collects ost_log records while it lives.
    class LogCapture
    {
    public:
        LogCapture()
        {
            ost_log::set_sink([this](ost_log::Level, const std::string &message, const std::string &exception_text)
                              { messages.push_back(exception_text.empty() ? message : message + " " + exception_text); });
        }
        ~LogCapture() { ost_log::set_sink(ost_log::Sink()); }
        LogCapture(const LogCapture &) = delete;
        LogCapture &operator=(const LogCapture &) = delete;
        std::vector<std::string> messages;
    };

    // A hand-made BidConditions of the Windows tests, with the one adaptation above (Bids gets JobStatusUID).
    void minimal_world(const TempDb &db, const std::string &condition_columns)
    {
        db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
        db.must("INSERT INTO Bids (UID) VALUES (1)");
        db.must("CREATE TABLE BidConditions (" + condition_columns + ")");
    }
}

// =====================================================================================================================
// Ports of the Windows tests (test_condition_operations.py)
// =====================================================================================================================

// Port of ConditionOperationsPersistenceTests.test_condition_insert_rejects_cross_bid_relationship_before_insert (:407-444): bids 1 and 2, layers 7 (bid
// 1) and 8 (bid 2), folders 9 (bid 1) and 10 (bid 2); the specs (layer, folder) = (8, 9), (7, 10), (8, None) each return None and write 0 rows.
// (Windows logs a MissingBidOwnedUidError; iOS: the generic failure, empty error text.)
TEST(CondCreateWindows, CrossBidRelationshipsAreRefusedBeforeTheInsert)
{
    const char *cases[][2] = {{"8", "9"}, {"7", "10"}, {"8", ""}};
    int index = 0;
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT(std::string("layer ") + c[0] + ", folder '" + c[1] + "'");
        TempDb db("cc_xbid_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid one', 1)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Bid two', 2)");
        db.must("INSERT INTO BidLayers (UID, BidUID) VALUES (7, 1)");
        db.must("INSERT INTO BidLayers (UID, BidUID) VALUES (8, 2)");
        db.must("INSERT INTO BidConditionFolders (UID, BidUID) VALUES (9, 1)");
        db.must("INSERT INTO BidConditionFolders (UID, BidUID) VALUES (10, 2)");
        CreateConditionSpec spec = valid("Invalid");
        spec.layer_uid = c[0];
        spec.folder_uid = c[1];
        expect_refused(db, "1", spec);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 0);
        expect_created(db, "1", valid(), "1", 1); // nothing was consumed
    }
}

// Port of ...::test_condition_insert_persists_spec_with_next_identity_and_scoped_owners (:446-485): conditions (UID 3, bid 1, RefNo 5) and (UID 4, bid 2,
// RefNo 40); a spec "Wall", type 2, backout, layer 7, folder 9, notes "hello", width 3.5 for bid 1 returns "5"; the row is (UID 5, BidUID 1, RefNo 6, "Wall",
// 2, Backout -1, layer 7, folder 9, notes, CdnTypeUID NULL, Width 3.5) with a GUID of the Windows format. The UID is the MAX over ALL bids (4) + 1, the
// number is the bid's own MAX (5) + 1. Notes are TEXT on iOS (plan 4.5; Windows writes a BLOB).
TEST(CondCreateWindows, PersistsTheSpecWithTheNextIdentityAndScopedOwners)
{
    TempDb db("cc_next_identity");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid one', 1)");
    db.must("INSERT INTO BidLayers (UID, BidUID) VALUES (7, 1)");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID) VALUES (9, 1)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo) VALUES (3, 1, 5)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo) VALUES (4, 2, 40)");
    CreateConditionSpec spec;
    spec.name = "Wall";
    spec.condition_type = 2;
    spec.backout = true;
    spec.layer_uid = "7";
    spec.folder_uid = "9";
    spec.notes = "hello";
    spec.width = 3.5;
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", spec, &error), "5");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(quoted_row(db, "5", {"UID", "BidUID", "RefNo", "Name", "Type", "Backout", "BidLayerUID", "BidConditionFolderUID", "Notes", "CdnTypeUID", "Width"}),
             "5 1 6 'Wall' 2 -1 7 9 'hello' NULL 3.5");
    CHECK(is_ost_guid(db.text("SELECT GUID FROM BidConditions WHERE UID = 5")));
    // the other rows are untouched
    CHECK_EQ(db.text("SELECT quote(RefNo) FROM BidConditions WHERE UID = 3"), "5");
    CHECK_EQ(db.text("SELECT quote(RefNo) FROM BidConditions WHERE UID = 4"), "40");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 3);
}

// Port of ...::test_condition_insert_defaults_root_layer_folder_and_empty_notes_to_null (:487-507): CreateConditionSpec(name "Plain", type 1) for an empty
// bid returns "1" with (RefNo 1, Backout 0, layer NULL, folder NULL, Notes NULL).
TEST(CondCreateWindows, DefaultsRootLayerFolderAndEmptyNotesToNull)
{
    TempDb db("cc_plain_defaults");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid one', 1)");
    CreateConditionSpec spec;
    spec.name = "Plain";
    spec.condition_type = 1;
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", spec), "1");
    CHECK_EQ(quoted_row(db, "1", {"RefNo", "Backout", "BidLayerUID", "BidConditionFolderUID", "Notes"}), "1 0 NULL NULL NULL");
}

// Port of ConditionInsertContractTests.test_every_spec_field_lands_in_its_own_column_and_the_fixed_columns_take_their_defaults (:968-1011) on the iOS schema
// (with the condition type 7 the Windows test uses, which the iOS insert checks for existence: NC-8): the call returns "1"; the fixed columns are (UID 1,
// BidUID 1, RefNo 1, IsTemplate 0, Curve 0, SnapToGrid 0, ManualLength 0, MatAmount 0, LabAmount 0, SubAmount 0, FontName "Arial", FontSize -16, FontBold 0,
// FontItalic 0, ShowTakeoff -1); the GUID has the Windows format; Backout -1; every other spec field is in its own column (ids as integers, flags as 1 / 0).
TEST(CondCreateWindows, EverySpecFieldLandsInItsOwnColumnAndTheFixedColumnsTakeTheirDefaults)
{
    TempDb db("cc_every_field");
    REQUIRE(db.schema_ok());
    seed_world(db);
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", windows_full_spec(), &error), "1");
    CHECK_STR_EQ(error, "");

    // the fixed columns, in the order of the Windows test
    const struct
    {
        const char *column;
        const char *value;
    } fixed[] = {
        {"UID", "integer:1"},       {"BidUID", "integer:1"},   {"RefNo", "integer:1"},      {"IsTemplate", "integer:0"}, {"Curve", "integer:0"},
        {"SnapToGrid", "integer:0"}, {"ManualLength", "integer:0"}, {"FontName", "text:'Arial'"}, {"FontSize", "integer:-16"}, {"FontBold", "integer:0"},
        {"FontItalic", "integer:0"}, {"ShowTakeoff", "integer:-1"}, {"Backout", "integer:-1"},
    };
    for (const auto &c : fixed)
        CHECK_EQ(typed(db, "1", c.column), c.value);
    for (const char *column : {"MatAmount", "LabAmount", "SubAmount"})
        CHECK_EQ(real_of(db, column, "1"), 0.0); // a REAL column in the iOS schema: 0 or 0.0, both are zero
    CHECK(is_ost_guid(db.text("SELECT GUID FROM BidConditions WHERE UID = 1")));

    // Notes: Windows encode_text_blob("memo"); iOS stores TEXT (plan 4.5)
    CHECK_EQ(typed(db, "1", "Notes"), "text:'memo'");
    CHECK_EQ(typed(db, "1", "Name"), "text:'Wall'");

    // every other field equals its spec value (:1002-1011)
    const struct
    {
        const char *column;
        const char *value;
    } integers[] = {
        {"Type", "integer:3"},         {"CdnTypeUID", "integer:7"},   {"BidLayerUID", "integer:30"}, {"BidConditionFolderUID", "integer:40"}, {"Shape", "integer:2"},
        {"Pattern", "integer:3"},      {"ColorFill", "integer:9"},    {"UOM1", "integer:1"},         {"UOM2", "integer:2"},                   {"UOM3", "integer:3"},
        {"Quantity1", "integer:4"},    {"Quantity2", "integer:5"},    {"Quantity3", "integer:6"},    {"DropRun", "integer:1"},                {"RoundQuantity", "integer:1"},
        {"Trim", "integer:1"},         {"IsCurvedSegment", "integer:1"}, {"Grid", "integer:1"},      {"DisplayDimension", "integer:1"},       {"DisplayName", "integer:1"},
        {"DisplayGridWhileDrawing", "integer:1"},
    };
    for (const auto &c : integers)
        CHECK_EQ(typed(db, "1", c.column), c.value);
    const struct
    {
        const char *column;
        double value;
    } reals[] = {
        {"Height", 1.5}, {"Width", 2.5},      {"Depth", 3.5},      {"Thickness", 4.5},  {"Rise", 5.5},      {"Run", 6.5},  {"DisplaySize", 7.5},
        {"Spacing", 8.5}, {"DropValue", 9.5}, {"RoundUp", 10.5},   {"GridSize1", 11.5}, {"GridSize2", 12.5}, {"Gap", 13.5},
    };
    for (const auto &c : reals)
    {
        SCOPED_CONTEXT(c.column);
        CHECK_EQ(real_of(db, c.column, "1"), c.value);
    }
}

// Port of ...::test_a_default_spec_writes_no_backout_and_no_notes_and_numbers_conditions_in_sequence (:1013-1026): two default specs get the UIDs "1" and "2",
// the numbers 1 and 2, Backout 0 and NULL notes, cdn type, layer and folder.
TEST(CondCreateWindows, ADefaultSpecWritesNoBackoutNoNotesAndNumbersConditionsInSequence)
{
    TempDb db("cc_default_sequence");
    REQUIRE(db.schema_ok());
    seed_world(db);
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", CreateConditionSpec()), "1");
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", CreateConditionSpec()), "2");
    for (const char *uid : {"1", "2"})
        CHECK_EQ(quoted_row(db, uid, {"Backout", "Notes", "CdnTypeUID", "BidLayerUID", "BidConditionFolderUID"}), "0 NULL NULL NULL NULL");
    CHECK_EQ(db.list("SELECT UID || ':' || RefNo FROM BidConditions ORDER BY UID"), "1:1,2:2");
}

// Port of ...::test_text_reference_numbers_are_counted_from_their_integer_value (:1028-1042): a RefNo column of NO declared type holding the text '5' gives the
// new number 6 and the new UID 6 (MAX(UID) 5 + 1). (Hand-made tables; Bids gets JobStatusUID, see the file comment.)
TEST(CondCreateWindows, TextReferenceNumbersAreCountedFromTheirIntegerValue)
{
    TempDb db("cc_text_refno", false);
    minimal_world(db, "UID INTEGER, BidUID INTEGER, RefNo, Name TEXT, Type INTEGER");
    db.must("INSERT INTO BidConditions VALUES (5, 1, '5', 'old', 0)");
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", CreateConditionSpec()), "6");
    CHECK_EQ(db.list("SELECT RefNo FROM BidConditions WHERE UID = 6"), "6");
}

// Port of ...::test_missing_optional_columns_are_reported_under_the_insert_label (:1044-1053): a BidConditions with only UID, BidUID, Name and Type still takes
// the insert ("1"); every optional column that is missing is skipped and reported under the label "insert_condition" (Windows `skips`; iOS: ost_log::warning
// records that name the label and the column).
TEST(CondCreateWindows, MissingOptionalColumnsAreReportedUnderTheInsertLabel)
{
    TempDb db("cc_missing_optional", false);
    minimal_world(db, "UID INTEGER, BidUID INTEGER, Name TEXT, Type INTEGER");
    LogCapture log;
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", CreateConditionSpec(), &error), "1");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.text("SELECT UID || ':' || BidUID || ':' || Name || ':' || Type FROM BidConditions"), "1:1:Untitled:0");
    bool height_reported = false;
    bool label_everywhere = true;
    int reports = 0;
    for (const std::string &message : log.messages)
    {
        if (message.find("insert_condition") == std::string::npos)
            continue;
        ++reports;
        height_reported = height_reported || message.find("Height") != std::string::npos;
    }
    CHECK(reports > 0);
    CHECK(height_reported);
    // every record that mentions a skipped column names the label (the Windows test: the set of operations is exactly {"insert_condition"})
    for (const std::string &message : log.messages)
        if (message.find("Height") != std::string::npos || message.find("Notes") != std::string::npos || message.find("DisplaySize") != std::string::npos)
            label_everywhere = label_everywhere && message.find("insert_condition") != std::string::npos;
    CHECK(label_everywhere);
}

// Port of ...::test_the_insert_needs_its_identity_name_and_type_columns (:1055-1078) and the "insert" rows of ConditionGuardMessageTests.
// test_each_operation_names_the_identity_column_it_cannot_find (:2133-2153; Windows names the missing column in a strict-mode message, iOS has the generic
// failure): without UID, BidUID, Name or Type the insert fails and writes nothing. The control: with all four it works.
TEST(CondCreateWindows, TheInsertNeedsItsIdentityNameAndTypeColumns)
{
    {
        TempDb db("cc_needs_columns_all", false);
        minimal_world(db, "UID INTEGER, BidUID INTEGER, Name TEXT, Type INTEGER");
        CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", CreateConditionSpec()), "1");
    }
    for (const char *dropped : {"UID", "BidUID", "Name", "Type"})
    {
        SCOPED_CONTEXT(std::string("without ") + dropped);
        std::string columns;
        for (const char *column : {"UID INTEGER", "BidUID INTEGER", "Name TEXT", "Type INTEGER"})
            if (std::string(column).rfind(std::string(dropped) + " ", 0) != 0)
                columns += (columns.empty() ? "" : ", ") + std::string(column);
        TempDb db(std::string("cc_needs_columns_") + dropped, false);
        minimal_world(db, columns);
        const std::string before = db.snapshot_all();
        std::string error = "stale";
        CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", CreateConditionSpec(), &error), "");
        CHECK_STR_EQ(error, "");
        CHECK_EQ(db.snapshot_all(), before);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 0);
    }
}

// Port of ...::test_a_layer_or_folder_of_another_bid_is_refused (:1080-1095): layer 31 and folder 41 belong to bid 2: both specs are refused, 0 rows.
TEST(CondCreateWindows, ALayerOrFolderOfAnotherBidIsRefused)
{
    TempDb db("cc_other_bid");
    REQUIRE(db.schema_ok());
    seed_world(db);
    CreateConditionSpec layer_spec;
    layer_spec.layer_uid = "31";
    CreateConditionSpec folder_spec;
    folder_spec.folder_uid = "41";
    const std::string before = db.snapshot_all();
    refusal_call(db, "1", layer_spec, std::string());
    refusal_call(db, "1", folder_spec, std::string());
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 0);
    expect_created(db, "1", CreateConditionSpec(), "1", 1);
}

// Port of ConditionEdgeContractTests.test_an_insert_for_a_bid_that_does_not_exist_is_refused_with_the_owner_named (:2197-2211): bid 99 has no Bids row: refused,
// 0 rows (Windows logs "Bids has no row for UID 99"; iOS: the generic failure). A bid that only conditions point at (no Bids row) is no bid either.
TEST(CondCreateWindows, ABidThatDoesNotExistIsRefused)
{
    TempDb db("cc_missing_bid");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (7, 3, 1, 'Orphan', 0)");
    const std::string before = db.snapshot_all();
    refusal_call(db, "99", CreateConditionSpec(), std::string());
    refusal_call(db, "3", CreateConditionSpec(), std::string());
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 1);
    expect_created(db, "1", CreateConditionSpec(), "8", 1);
}

// =====================================================================================================================
// The condition-number limit (Windows ConditionNumberAllocationLimitTests, :1344-1416)
// =====================================================================================================================

// Port of ...::test_adding_past_the_highest_number_raises_and_writes_nothing, the "new condition" row (:1388-1395, :1397-1404): bid 1 holds 2147483647 (and bid 2
// does too): the insert raises ConditionNumberOverflowError with the exact text and the rows are unchanged. iOS: "" and the text in the error channel; and
// test_project_write_service.py:19668 (the overflow becomes the user message, no uid). The expected text is the Windows f-string run in memory
// (condition_operations.py:57-62, count 1, last 2147483648).
TEST(CondCreateLimits, ANewConditionPastTheHighestNumberIsRefusedAndWritesNothing)
{
    TempDb db("cc_limit_refused");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483647LL);
    const std::string rows_before = db.snapshot_all();
    expect_refused(db, "1", CreateConditionSpec(), overflow_text(1, "2147483648"));
    CHECK_EQ(db.snapshot_all(), rows_before);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 3);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID > 20"), 0); // no new UID
    expect_refused(db, "1", valid("Another"), overflow_text(1, "2147483648")); // the spec does not matter
    // the error channel is optional: refusing without it is the same refusal
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid()), "");
    CHECK_EQ(db.snapshot_all(), rows_before);
    // once a number is free the next create gets the UID the refused ones must not have used
    db.must("UPDATE BidConditions SET RefNo = 5 WHERE UID = 11");
    expect_created(db, "1", valid(), "21", 6);
}

// Windows ...::test_the_last_free_number_can_still_be_used (:1406-1416), written for Duplicate there; the same boundary for a new condition: with 2147483646
// the new condition gets 2147483647 (exclusive limit), the next one is refused with X = 2147483648.
TEST(CondCreateLimits, TheLastFreeNumberCanStillBeUsed)
{
    TempDb db("cc_limit_last_free");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483646LL);
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid(), &error), "21");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 21"), 2147483647LL);
    CHECK_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 21"), "integer");
    CHECK_EQ(db.count("SELECT MAX(RefNo) FROM BidConditions WHERE BidUID = 1"), 2147483647LL);
    // nothing else moved
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 10"), 1);
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 11"), 2147483646LL);
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 20"), 2147483646LL);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 4);
    // the NEXT create is past the limit
    expect_refused(db, "1", valid(), overflow_text(1, "2147483648"));
}

// A number beyond what an Access Long holds (an older iOS build could store 2147483648 through the update shift): the highest number is read as 64 bit and
// the new condition is refused with X = highest + 1 printed exactly, unsigned for LLONG_MAX (the Windows ints do not wrap). Windows text per case run in memory.
TEST(CondCreateLimits, ANumberAlreadyAboveTheLimitBlocksToo)
{
    const struct
    {
        long long highest;
        const char *last;
    } cases[] = {
        {2147483648LL, "2147483649"},
        {3000000000LL, "3000000001"},
        {9223372036854775807LL, "9223372036854775808"},
    };
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT("highest " + std::to_string(c.highest));
        TempDb db("cc_limit_above_" + std::to_string(c.highest));
        REQUIRE(db.schema_ok());
        seed_limit_world(db, c.highest);
        expect_refused(db, "1", valid(), overflow_text(1, c.last));
        db.must("UPDATE BidConditions SET RefNo = 2 WHERE UID = 11");
        expect_created(db, "1", valid(), "21", 3);
    }
}

// Windows `WHERE [BidUID] = ?` (:51): only the numbers of the condition's own bid count: bid 2 at the limit does not block bid 1 (its new condition is numbered
// after bid 1's own highest number, 2), and a create in bid 2 itself is refused.
TEST(CondCreateLimits, OnlyTheNumbersOfTheOwnBidCount)
{
    TempDb db("cc_limit_own_bid");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 1, 'c', 0)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (11, 1, 2, 'c', 0)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (20, 2, 2147483647, 'c', 0)");
    expect_created(db, "1", valid(), "21", 3);
    expect_refused(db, "2", valid(), overflow_text(1, "2147483648"));
}

// MAX ignores NULL: a bid whose numbers are all NULL, or that has no condition, starts at 1; a NULL next to a number is no number; a highest number of 0 gives 1
// (Windows: `int(max_ref) + 1 if max_ref is not None else 1`, run in memory: NULL only -> 1, NULL next to 5 -> 6, 0 -> 1).
TEST(CondCreateLimits, NullNumbersAreIgnoredAndABidWithoutNumbersStartsAtOne)
{
    {
        SCOPED_CONTEXT("only NULL numbers");
        TempDb db("cc_null_only");
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, NULL, 'c', 0)");
        expect_created(db, "1", valid(), "11", 1);
        CHECK_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 10"), "null"); // the source keeps its NULL
    }
    {
        SCOPED_CONTEXT("a NULL next to a number");
        TempDb db("cc_null_mixed");
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, NULL, 'c', 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (11, 1, 5, 'c', 0)");
        expect_created(db, "1", valid(), "12", 6);
    }
    {
        SCOPED_CONTEXT("a highest number of 0");
        TempDb db("cc_zero_number");
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 0, 'c', 0)");
        expect_created(db, "1", valid(), "11", 1);
    }
}

// The ownership check comes BEFORE the allocation (Windows insert_condition :262-279 precede :280): a foreign or unknown layer / folder, an unknown bid or a
// bad uid at a bid that is full is the generic failure (empty error text), never the overflow text, and so is a name that would also be too long.
TEST(CondCreateLimits, OwnershipComesBeforeTheNumberLimitAndBeforeTheNameRules)
{
    TempDb db("cc_limit_order");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483647LL);
    const std::string before = db.snapshot_all();

    CreateConditionSpec foreign_layer = valid();
    foreign_layer.layer_uid = "31";
    refusal_call(db, "1", foreign_layer, std::string());

    CreateConditionSpec unknown_folder = valid();
    unknown_folder.folder_uid = "99";
    refusal_call(db, "1", unknown_folder, std::string());

    CreateConditionSpec bad_layer_and_long_name = valid(repeat("a", 300));
    bad_layer_and_long_name.layer_uid = "abc";
    refusal_call(db, "1", bad_layer_and_long_name, std::string());

    refusal_call(db, "99", valid(), std::string()); // an unknown bid
    refusal_call(db, "abc", valid(), std::string());
    CHECK_EQ(db.snapshot_all(), before);
    db.must("UPDATE BidConditions SET RefNo = 1 WHERE UID IN (11, 20)");
    expect_created(db, "1", valid(), "21", 2);
}

// A schema without the number column has no number to run out of (Windows :49, :55: max_ref stays None): no check, no error, whatever else the table holds.
TEST(CondCreateLimits, ASchemaWithoutTheNumberColumnNeverHitsTheLimit)
{
    TempDb db("cc_no_refno", false);
    minimal_world(db, "UID INTEGER, BidUID INTEGER, Name TEXT, Type INTEGER");
    db.must("INSERT INTO BidConditions VALUES (10, 1, 'Wall', 0)");
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid(), &error), "11");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.list("SELECT UID || ':' || BidUID || ':' || Name FROM BidConditions ORDER BY UID"), "10:1:Wall,11:1:Wall");
}

// The error channel is optional and always reset (the duplicate_condition contract): a success and a generic failure clear it, a refusal sets it, and every
// call works without it. (Windows: only the overflow reaches the user as a text, project_write_service.py:722-736; every other failure is "Failed to create
// condition.".)
TEST(CondCreateLimits, TheErrorChannelIsClearedAndOptional)
{
    TempDb db("cc_error_channel");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483645LL);
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid(), &error), "21"); // number 2147483646
    CHECK_STR_EQ(error, "");
    error = "stale";
    CreateConditionSpec foreign;
    foreign.layer_uid = "31";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", foreign, &error), ""); // generic failure
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid()), "22"); // without the channel: number 2147483647
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 22"), 2147483647LL);
    error = "stale";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid(), &error), "");
    CHECK_STR_EQ(error, overflow_text(1, "2147483648"));
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid()), ""); // a refusal without the channel
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 5);
}

// =====================================================================================================================
// The UID limit (plan 4.2, N-M1-04, register NC-9: Windows guards only the number) and the UID allocation
// =====================================================================================================================

// MAX(UID) of BidConditions at 2147483647: the next UID would pass an Access Long, so the create is refused before any write with the "no identity" text; one
// below the limit the new condition gets 2147483647 and the one after that is refused.
TEST(CondCreateUidLimit, ANewConditionWhoseUidWouldPassTheLimitIsRefused)
{
    TempDb db("cc_uid_limit");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (2147483647, 1, 1, 'High', 0)");
    expect_refused(db, "1", valid(), kNoIdentityText);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 1);

    db.must("UPDATE BidConditions SET UID = 2147483646");
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid(), &error), "2147483647"); // the last identity
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 2147483647"), 2);
    expect_refused(db, "1", valid(), kNoIdentityText);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 2);
}

// A UID above 32 bits is refused, never wrapped (4294967296 would be 0 as a uint32, 4294967295 + 1 would be negative as an int32).
TEST(CondCreateUidLimit, AUidAbove32BitsIsRefusedNotWrapped)
{
    for (const long long highest : {2147483648LL, 4294967295LL, 4294967296LL, 5000000000LL})
    {
        SCOPED_CONTEXT("highest UID " + std::to_string(highest));
        TempDb db("cc_uid_wrap_" + std::to_string(highest));
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (" + std::to_string(highest) + ", 1, 1, 'High', 0)");
        expect_refused(db, "1", valid(), kNoIdentityText);
        db.must("DELETE FROM BidConditions");
        expect_created(db, "1", valid(), "1", 1);
    }
}

// The rows that REFERENCE BidConditions count as much as its own rows (identity_allocation.py:10-39; plan 4.2): a dangling reference at 2147483647 in any of the
// six columns leaves no identity, one at 2147483646 leaves exactly one (the new UID is 2147483647), one beyond 32 bits leaves none.
TEST(CondCreateUidLimit, ReferencesCountTowardsTheLimit)
{
    const struct
    {
        const char *table;
        const char *column;
    } references[] = {
        {"BidConditionUser", "ConditionUID"}, {"BidLaborActivity", "BidConditionUID"}, {"BidTakeoffTotals", "BidConditionUID"},
        {"BidTakeoffs", "BidConditionUID"},   {"BidTypGroupViews", "BidConditionUID"}, {"BidTypicalGroupTotals", "BidConditionUID"},
    };
    int index = 0;
    for (const auto &reference : references)
        for (const long long referenced : {2147483646LL, 2147483647LL, 4294967296LL})
        {
            SCOPED_CONTEXT(std::string(reference.table) + "." + reference.column + " = " + std::to_string(referenced));
            TempDb db("cc_uid_ref_" + std::to_string(index++));
            REQUIRE(db.schema_ok());
            seed_world(db);
            db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (5, 1, 1, 'c', 0)");
            if (std::string(reference.table) == "BidTakeoffs")
                db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, Position) VALUES (70, 1, " + std::to_string(referenced) + ", '0;0;1;1')");
            else
                db.must(std::string("INSERT INTO ") + reference.table + " (UID, " + reference.column + ") VALUES (70, " + std::to_string(referenced) + ")");
            if (referenced == 2147483646LL)
            {
                expect_created(db, "1", valid(), "2147483647", 2);
                expect_refused(db, "1", valid(), kNoIdentityText);
            }
            else
            {
                expect_refused(db, "1", valid(), kNoIdentityText);
                db.must("DELETE FROM " + std::string(reference.table));
                expect_created(db, "1", valid(), "6", 2);
            }
        }
}

// Port of Windows identity_allocation.py:5-47 (the UID is MAX(UID) of the table over ALL bids + 1, raised above the largest value of every column that references
// the table; NULL, '', 0 and '0' references are ignored), as the Windows oracle run in memory through insert_condition for the six referencing columns of
// BID_RELATIONSHIPS: a reference of 8 with a condition 7 gives 9, 7 / 0 / NULL / '' give 8, 100 gives 101 (DBP-18 pattern, tests/.../test_condition_operations.py
// DBP-18 plan "BidTakeoffTotals(1, 8) with condition 7 gives 9").
TEST(CondCreateUidLimit, TheUidIsRaisedAboveEveryDanglingReference)
{
    const struct
    {
        const char *table;
        const char *column;
    } references[] = {
        {"BidConditionUser", "ConditionUID"}, {"BidLaborActivity", "BidConditionUID"}, {"BidTakeoffTotals", "BidConditionUID"},
        {"BidTakeoffs", "BidConditionUID"},   {"BidTypGroupViews", "BidConditionUID"}, {"BidTypicalGroupTotals", "BidConditionUID"},
    };
    const struct
    {
        const char *value;
        const char *uid;
    } values[] = {{"8", "9"}, {"7", "8"}, {"0", "8"}, {"NULL", "8"}, {"''", "8"}, {"100", "101"}};
    int index = 0;
    for (const auto &reference : references)
        for (const auto &v : values)
        {
            SCOPED_CONTEXT(std::string(reference.table) + "." + reference.column + " = " + v.value);
            TempDb db("cc_uid_dangling_" + std::to_string(index++));
            REQUIRE(db.schema_ok());
            seed_world(db);
            db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (7, 1, 1, 'c', 0)");
            if (std::string(reference.table) == "BidTakeoffs")
                db.must(std::string("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, Position) VALUES (70, 1, ") + v.value + ", '0;0;1;1')");
            else
                db.must(std::string("INSERT INTO ") + reference.table + " (UID, " + reference.column + ") VALUES (70, " + v.value + ")");
            expect_created(db, "1", valid(), v.uid, 2);
        }
}

// The UID is the MAX over ALL bids of the table, not of the bid (Windows :446 test: conditions 3 and 4 of two bids give 5): a condition of another bid with a
// high UID moves the new UID, a refused call consumes none.
TEST(CondCreateUidLimit, TheUidIsTheMaximumOverAllBidsPlusOne)
{
    TempDb db("cc_uid_all_bids");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (3, 1, 1, 'c', 0)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (900, 2, 1, 'c', 0)");
    CreateConditionSpec foreign;
    foreign.layer_uid = "31";
    expect_refused(db, "1", foreign);
    expect_created(db, "1", valid(), "901", 2);
    expect_created(db, "2", valid(), "902", 2);
}

// =====================================================================================================================
// Refusals before any write: uids, ownership, the condition type
// =====================================================================================================================

// The bid uid is read like Python int() (condition_operations.py:261 `int(bid_uid)`; the same reading as update_condition and duplicate_condition): "0", a
// negative number, "abc", "", "1.5", "1x" and a number beyond 64 bits name no bid; surrounding blanks, a sign and leading zeros are the bid 1.
TEST(CondCreateRefusals, BidUidsAreReadLikePythonInt)
{
    TempDb db("cc_bid_text");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();
    for (const char *bad : {"0", "-1", "abc", "", "1.5", "x1", "1x", "0x1", "99999999999999999999", "1 2"})
    {
        SCOPED_CONTEXT(std::string("bid uid '") + bad + "'");
        refusal_call(db, bad, valid(), std::string());
    }
    CHECK_EQ(db.snapshot_all(), before);
    // the accepted spellings all mean bid 1
    expect_created(db, " 1", valid(), "1", 1);
    expect_created(db, "1 ", valid(), "2", 2);
    expect_created(db, "+1", valid(), "3", 3);
    expect_created(db, "01", valid(), "4", 4);
    expect_created(db, "\t1\n", valid(), "5", 5);
}

// New mutation-check case (fixer, not a Windows port): a bid uid is a positive number even when the table holds a row with uid 0 or a negative uid (Access
// AutoNumbers start at 1, the writer never addresses such a row): the generic failure, nothing written; the bid 1 still takes the condition.
TEST(CondCreateRefusals, ABidRowWithAZeroOrNegativeUidIsNeverWrittenTo)
{
    TempDb db("cc_bid_zero");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (0, 1, 'Bid zero', 3, 0)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (-5, 1, 'Bid minus five', 4, 0)");
    const std::string before = db.snapshot_all();
    for (const char *bad : {"0", "-5"})
    {
        SCOPED_CONTEXT(std::string("bid uid '") + bad + "'");
        refusal_call(db, bad, valid(), std::string());
    }
    CHECK_EQ(db.snapshot_all(), before);
    expect_created(db, "1", valid(), "1", 1);
}

// New mutation-check case (fixer, not a Windows port): a bid uid beyond 64 bits is refused, it never wraps around to a bid that exists. Python int()
// has no such limit and the Windows bid lookup would simply find no row; here "18446744073709551617" is 2^64 + 1 (a wrapped read gives bid 1),
// "36893488147419103233" is 2 * 2^64 + 1 and "-18446744073709551615" is minus (2^64 - 1) (a wrapped, negated read gives bid 1 as well).
TEST(CondCreateRefusals, ABidUidBeyond64BitsNeverWrapsAroundToAnExistingBid)
{
    TempDb db("cc_bid_wrap");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();
    for (const char *bad : {"18446744073709551617", "36893488147419103233", "-18446744073709551615", "+18446744073709551617", " 18446744073709551617 "})
    {
        SCOPED_CONTEXT(std::string("bid uid '") + bad + "'");
        refusal_call(db, bad, valid(), std::string());
    }
    CHECK_EQ(db.snapshot_all(), before);
    expect_created(db, "1", valid(), "1", 1);
}

// A layer uid is ASCII digits only and greater than zero (bid_owned_identity.py:105-117 require_valid_unique_bid_owned_uids: "0", "abc", "-5", " 30", "+30",
// "1.0" are malformed), the row must exist (:186-189) and belong to THIS bid (:190-201: another bid's row, a shared template row with BidUID NULL and a template
// row of another bid are refused). The bid's OWN locked copy of a template layer is accepted, and so is "030" (int("030") = 30).
TEST(CondCreateRefusals, ALayerMustBeOneOfTheBidsOwn)
{
    TempDb db("cc_layer_owner");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (32, 1, -1, 'Default', -1, -1, 2)"); // the bid's own locked copy
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (33, 2, -1, 'Default', -1, -1, 2)"); // another bid's
    const std::string shared = db.text("SELECT UID FROM BidLayers WHERE BidUID IS NULL AND Name = 'Default'");
    REQUIRE(shared != "<none>");
    const std::string before = db.snapshot_all();
    for (const std::string &bad : std::vector<std::string>{"0", "abc", "-5", " 30", "30 ", "3 0", "+30", "1.0", "99", "31", "33", shared})
    {
        SCOPED_CONTEXT("layer uid '" + bad + "'");
        CreateConditionSpec spec = valid();
        spec.layer_uid = bad;
        refusal_call(db, "1", spec, std::string());
    }
    CHECK_EQ(db.snapshot_all(), before);
    // accepted: the bid's own layer, its own locked copy, a leading zero
    for (const char *good : {"30", "32", "030"})
    {
        SCOPED_CONTEXT(std::string("layer uid '") + good + "'");
        CreateConditionSpec spec = valid();
        spec.layer_uid = good;
        const std::string uid = ost_store::insert_condition(db.path(), "1", spec);
        REQUIRE(!uid.empty());
        CHECK_EQ(db.count("SELECT BidLayerUID FROM BidConditions WHERE UID = " + uid), std::stoll(std::string(good)));
    }
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 3);
    // no layer at all is NULL (an empty uid is "no layer", not a malformed one)
    const std::string uid = ost_store::insert_condition(db.path(), "1", valid());
    REQUIRE(!uid.empty());
    CHECK_EQ(db.text("SELECT typeof(BidLayerUID) FROM BidConditions WHERE UID = " + uid), "null");
}

// The same for a folder: digits only, positive, an existing row of THIS bid; "040" is folder 40; an empty uid is the top level (NULL).
TEST(CondCreateRefusals, AFolderMustBeOneOfTheBidsOwn)
{
    TempDb db("cc_folder_owner");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();
    for (const char *bad : {"0", "abc", "-5", " 40", "40 ", "+40", "1.0", "99", "41"})
    {
        SCOPED_CONTEXT(std::string("folder uid '") + bad + "'");
        CreateConditionSpec spec = valid();
        spec.folder_uid = bad;
        refusal_call(db, "1", spec, std::string());
    }
    CHECK_EQ(db.snapshot_all(), before);
    for (const char *good : {"40", "040"})
    {
        SCOPED_CONTEXT(std::string("folder uid '") + good + "'");
        CreateConditionSpec spec = valid();
        spec.folder_uid = good;
        const std::string uid = ost_store::insert_condition(db.path(), "1", spec);
        REQUIRE(!uid.empty());
        CHECK_EQ(db.count("SELECT BidConditionFolderUID FROM BidConditions WHERE UID = " + uid), 40);
    }
    const std::string uid = ost_store::insert_condition(db.path(), "1", valid());
    REQUIRE(!uid.empty());
    CHECK_EQ(db.text("SELECT typeof(BidConditionFolderUID) FROM BidConditions WHERE UID = " + uid), "null");
}

// NC-8 (Windows condition_operations.py:326-334 `int(val) if val else None` checks no existence; the Windows test uses type 7 with no CdnTypes table): iOS
// refuses a condition type that is not a CdnTypes row (the 1:1 port seeds row 7). "abc" (Windows int() raises: generic failure), "0", a negative number and a
// number that is no row are refused; an empty uid is NULL.
TEST(CondCreateRefusals, AConditionTypeMustBeACdnTypesRow)
{
    TempDb db("cc_cdn_type");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();
    for (const char *bad : {"8", "abc", "0", "-7", "7.5", "7x", "99999999999999999999"})
    {
        SCOPED_CONTEXT(std::string("cdn type uid '") + bad + "'");
        CreateConditionSpec spec = valid();
        spec.cdn_type_uid = bad;
        refusal_call(db, "1", spec, std::string());
    }
    CHECK_EQ(db.snapshot_all(), before);
    for (const char *good : {"7", "007"})
    {
        SCOPED_CONTEXT(std::string("cdn type uid '") + good + "'");
        CreateConditionSpec spec = valid();
        spec.cdn_type_uid = good;
        const std::string uid = ost_store::insert_condition(db.path(), "1", spec);
        REQUIRE(!uid.empty());
        CHECK_EQ(db.count("SELECT CdnTypeUID FROM BidConditions WHERE UID = " + uid), 7);
    }
    const std::string uid = ost_store::insert_condition(db.path(), "1", valid());
    REQUIRE(!uid.empty());
    CHECK_EQ(db.text("SELECT typeof(CdnTypeUID) FROM BidConditions WHERE UID = " + uid), "null");
}

// The style numbers 0..3 are stored as given, and so is any other number the builder can make (the Windows handler test creates type 99,
// test_condition_action_handler.py:7192, and the writer has no table of types: condition_operations.py:326-334).
TEST(CondCreateRefusals, TheConditionTypeIsStoredAsGiven)
{
    TempDb db("cc_types");
    REQUIRE(db.schema_ok());
    seed_world(db);
    long long expected_uid = 1;
    for (const int type : {0, 1, 2, 3, 99, -1})
    {
        SCOPED_CONTEXT("type " + std::to_string(type));
        CreateConditionSpec spec = valid();
        spec.condition_type = type;
        CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", spec), std::to_string(expected_uid));
        CHECK_EQ(typed(db, std::to_string(expected_uid), "Type"), "integer:" + std::to_string(type));
        ++expected_uid;
    }
}

// A failed read is a refusal, never a write and never a crash: a missing table or column the insert has to read. Windows: every exception inside the connection
// is logged and None is returned (condition_operations.py:346-350). Each case also proves that the insert works when the unreadable table is not needed.
TEST(CondCreateRefusals, AMissingTableThatTheInsertReadsIsARefusal)
{
    {
        SCOPED_CONTEXT("no BidConditions");
        TempDb db("cc_fail_conditions");
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("DROP TABLE BidConditions");
        const std::string before = db.snapshot_all();
        refusal_call(db, "1", valid(), std::string());
        CHECK_EQ(db.snapshot_all(), before);
    }
    {
        SCOPED_CONTEXT("no Bids");
        TempDb db("cc_fail_bids");
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("DROP TABLE Bids");
        const std::string before = db.snapshot_all();
        refusal_call(db, "1", valid(), std::string());
        CHECK_EQ(db.snapshot_all(), before);
    }
    {
        SCOPED_CONTEXT("no BidLayers: a layer is refused, no layer is fine");
        TempDb db("cc_fail_layers");
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("DROP TABLE BidLayers");
        CreateConditionSpec with_layer = valid();
        with_layer.layer_uid = "30";
        expect_refused(db, "1", with_layer);
        expect_created(db, "1", valid(), "1", 1);
    }
    {
        SCOPED_CONTEXT("no BidConditionFolders: a folder is refused, no folder is fine");
        TempDb db("cc_fail_folders");
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("DROP TABLE BidConditionFolders");
        CreateConditionSpec with_folder = valid();
        with_folder.folder_uid = "40";
        expect_refused(db, "1", with_folder);
        expect_created(db, "1", valid(), "1", 1);
    }
    {
        SCOPED_CONTEXT("no CdnTypes: a condition type is refused, none is fine");
        TempDb db("cc_fail_cdn");
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("DROP TABLE CdnTypes");
        CreateConditionSpec with_type = valid();
        with_type.cdn_type_uid = "7";
        expect_refused(db, "1", with_type);
        expect_created(db, "1", valid(), "1", 1);
    }
}

// =====================================================================================================================
// Names and numbers the store refuses (iOS-new, N-M1-13, register NC-4, NC-5, NC-10)
// =====================================================================================================================

// NC-5: Windows accepts an empty name in the writer (the dialog refuses it, `name.strip()`, edit_condition_dialog.py:1432-1436). The store refuses an empty or
// blank name (Python strip: every Unicode white space; the names come from the same `name.strip()` run as in test_condition_create_form.cpp) with the generic
// failure and writes nothing; U+200B, U+FEFF and U+180E are not white space and are accepted; a name is stored AS GIVEN (the form strips it, the writer of
// Windows does not: condition_operations.py:326-334).
TEST(CondCreateNames, ABlankNameIsRefusedAndAnyOtherNameIsStoredAsGiven)
{
    TempDb db("cc_blank_names");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();
    const std::vector<std::string> blank = {"",           " ",           "  \t ",      "\n",         "\r\n",       "\xC2\xA0",   "\xE2\x80\x83",
                                            "\xE3\x80\x80", "\xE2\x80\xA8", "\xE2\x80\xA9", "\x1C",     "\x1D",       "\x1E",       "\x1F",
                                            "\xC2\x85",   "\x0B",        "\x0C"};
    for (const std::string &name : blank)
    {
        SCOPED_CONTEXT("blank name of " + std::to_string(name.size()) + " bytes, first " + std::to_string(name.empty() ? 0 : static_cast<unsigned char>(name[0])));
        refusal_call(db, "1", valid(name), std::string());
    }
    CHECK_EQ(db.snapshot_all(), before);
    long long uid = 1;
    for (const std::string &name : std::vector<std::string>{"\xE2\x80\x8B", "\xEF\xBB\xBF", "\xE1\xA0\x8E", " a ", "\xC2\xA0" "a" "\xC2\xA0", "  Wall  ", "Wall\n2"})
    {
        SCOPED_CONTEXT("name of " + std::to_string(name.size()) + " bytes");
        CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid(name)), std::to_string(uid));
        CHECK_STR_EQ(db.text("SELECT Name FROM BidConditions WHERE UID = " + std::to_string(uid)), name);
        ++uid;
    }
}

// NC-4 (Name VARCHAR(255), database_creator.py:166; Windows checks no length, an over-long name fails in Access with no hint): at most 255 UTF-16 code
// units, counted on the stored name INCLUDING the " @T value" suffix: 127 emoji are 254 units and are accepted, 128 emoji are 256 units (128 characters) and are
// refused; 255 accented letters (510 bytes) are accepted. The refusal carries the text and writes nothing.
TEST(CondCreateNames, TheNameLimitIs255Utf16Units)
{
    const std::string emoji = "\xF0\x9F\x98\x80"; // U+1F600: 4 bytes, 2 units
    const std::string e_acute = "\xC3\xA9";        // 2 bytes, 1 unit
    const struct
    {
        const char *label;
        std::string name;
        bool accepted;
    } cases[] = {
        {"255 ASCII", repeat("a", 255), true},
        {"256 ASCII", repeat("a", 256), false},
        {"255 accented letters (510 bytes)", repeat(e_acute, 255), true},
        {"256 accented letters", repeat(e_acute, 256), false},
        {"127 emoji (254 units)", repeat(emoji, 127), true},
        {"127 emoji + a (255 units)", repeat(emoji, 127) + "a", true},
        {"128 emoji (256 units, 128 characters)", repeat(emoji, 128), false},
        {"248 + ' @T 10'' (255 units, the suffix counts)", repeat("a", 248) + " @T 10'", true},
        {"249 + ' @T 10'' (256 units)", repeat("a", 249) + " @T 10'", false},
    };
    int index = 0;
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT(c.label);
        TempDb db("cc_name_limit_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        seed_world(db);
        if (c.accepted)
        {
            CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid(c.name)), "1");
            CHECK(db.text("SELECT Name FROM BidConditions WHERE UID = 1") == c.name);
        }
        else
        {
            expect_refused(db, "1", valid(c.name), std::string(kNameTooLongText));
            CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 0);
            expect_created(db, "1", valid("Short"), "1", 1);
        }
    }
}

// Text that is not valid UTF-8 is never written as a name (utf16_length: nullopt): the generic failure, nothing written.
TEST(CondCreateNames, ANameThatIsNotValidUtf8IsRefused)
{
    TempDb db("cc_name_utf8");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();
    for (const char *name : {"\xC3\x28", "\xED\xA0\x80", "\xC0\x80", "\xF4\x90\x80\x80", "abc\xFF", "\xE2\x82", "ok \xF0\x9F\x98"})
    {
        SCOPED_CONTEXT(std::string("name bytes starting ") + std::to_string(static_cast<unsigned char>(name[0])));
        refusal_call(db, "1", valid(name), std::string());
    }
    CHECK_EQ(db.snapshot_all(), before);
    expect_created(db, "1", valid("Fine"), "1", 1);
}

// New data-safety case (implementer, not a Windows port): a NUL inside the name or the notes would be cut by the C string the loader reads back; the
// generic failure and nothing written (the error channel stays empty), then a name without it is written.
TEST(CondCreateNames, ANulInTheNameOrTheNotesIsRefusedAndWritesNothing)
{
    TempDb db("cc_name_nul");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();
    for (const std::string &name : {std::string("Wa\0ll", 5), std::string("\0", 1), std::string("Wall\0", 5)})
    {
        SCOPED_CONTEXT("name of " + std::to_string(name.size()) + " bytes with a NUL");
        refusal_call(db, "1", valid(name), std::string());
    }
    {
        SCOPED_CONTEXT("notes with a NUL");
        CreateConditionSpec spec = valid("Wall");
        spec.notes = std::string("a\0b", 3);
        refusal_call(db, "1", spec, std::string());
    }
    CHECK_EQ(db.snapshot_all(), before);
    expect_created(db, "1", valid("Fine"), "1", 1);
}

// NC-10 (Windows: finiteness is checked by the dialog only, edit_condition_dialog.py:1411-1426; the writer passes NaN / inf on and SQLite stores NaN as NULL
// silently): a NaN or infinite number in any real field is refused with the generic failure and nothing is written; a huge FINITE number is stored exactly.
TEST(CondCreateNames, ANonFiniteNumberIsRefusedAndAHugeFiniteOneIsStored)
{
    TempDb db("cc_non_finite");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const struct
    {
        const char *column;
        double CreateConditionSpec::*field;
    } fields[] = {
        {"Height", &CreateConditionSpec::height},          {"Width", &CreateConditionSpec::width},           {"Depth", &CreateConditionSpec::depth},
        {"Thickness", &CreateConditionSpec::thickness},    {"Rise", &CreateConditionSpec::rise},             {"Run", &CreateConditionSpec::run},
        {"DisplaySize", &CreateConditionSpec::display_size}, {"Spacing", &CreateConditionSpec::spacing},     {"DropValue", &CreateConditionSpec::drop_value},
        {"RoundUp", &CreateConditionSpec::round_up},       {"GridSize1", &CreateConditionSpec::grid_size1},  {"GridSize2", &CreateConditionSpec::grid_size2},
        {"Gap", &CreateConditionSpec::gap},
    };
    const double non_finite[] = {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                                 -std::numeric_limits<double>::infinity()};
    const std::string before = db.snapshot_all();
    for (const auto &f : fields)
        for (const double bad : non_finite)
        {
            SCOPED_CONTEXT(std::string(f.column) + " = " + std::to_string(bad));
            CreateConditionSpec spec = valid();
            spec.*(f.field) = bad;
            refusal_call(db, "1", spec, std::string());
        }
    for (const double bad : non_finite)
    {
        SCOPED_CONTEXT("ConnectTolerance = " + std::to_string(bad));
        CreateConditionSpec spec = valid();
        spec.connect_tolerance = bad;
        refusal_call(db, "1", spec, std::string());
    }
    CHECK_EQ(db.snapshot_all(), before);
    long long uid = 1;
    for (const auto &f : fields)
    {
        SCOPED_CONTEXT(std::string(f.column) + " = 1e308");
        CreateConditionSpec spec = valid();
        spec.*(f.field) = 1e308;
        CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", spec), std::to_string(uid));
        CHECK_EQ(real_of(db, f.column, std::to_string(uid)), 1e308);
        ++uid;
    }
}

// =====================================================================================================================
// Duplicate names, two calls, what a success writes
// =====================================================================================================================

// NC-11 (Windows conditions_sidebar.py:883-896 refuses a duplicate name only on an inline rename; New and Duplicate create equal names): two creates with the same
// name are two rows with different UID, number and GUID. A double tap that reached the store twice would make two rows (the UI guards it, device check M1-03).
TEST(CondCreateNames, TwoCreatesWithTheSameNameAreTwoRows)
{
    TempDb db("cc_same_name");
    REQUIRE(db.schema_ok());
    seed_world(db);
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid("Wall")), "1");
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid("Wall")), "2");
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid("wall")), "3");
    CHECK_EQ(db.list("SELECT UID || ':' || RefNo || ':' || Name FROM BidConditions ORDER BY UID"), "1:1:Wall,2:2:Wall,3:3:wall");
    CHECK_NE(db.text("SELECT GUID FROM BidConditions WHERE UID = 1"), db.text("SELECT GUID FROM BidConditions WHERE UID = 2"));
}

// Port of ConditionDuplicate.ManyCopiesNeverRepeatAGuid for the insert: 40 creates never repeat a GUID; each is a random v4 UUID in the Windows format
// ("{" + str(uuid.uuid4()).upper() + "}": version nibble 4, variant 8..B).
TEST(CondCreateNames, ManyCreatesNeverRepeatAGuid)
{
    TempDb db("cc_many_guids");
    REQUIRE(db.schema_ok());
    seed_world(db);
    std::set<std::string> guids;
    for (int i = 0; i < 40; ++i)
    {
        SCOPED_CONTEXT("create " + std::to_string(i));
        const std::string uid = ost_store::insert_condition(db.path(), "1", valid("Same name"));
        REQUIRE_EQ(uid, std::to_string(i + 1));
        const std::string guid = db.text("SELECT GUID FROM BidConditions WHERE UID = " + uid);
        REQUIRE(is_ost_guid(guid));
        CHECK_EQ(guid.size(), size_t(38));
        CHECK_EQ(guid[15], '4');
        CHECK(std::string("89AB").find(guid[20]) != std::string::npos);
        CHECK(guids.insert(guid).second);
        CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = " + uid), i + 1);
    }
}

// Windows condition_operations.py:330-331 `encode_text_blob(val) if val else None`: notes are NULL when empty; iOS stores the TEXT (plan 4.5) byte for byte,
// including line breaks and non-ASCII text.
TEST(CondCreateNames, NotesAreTextAndEmptyNotesAreNull)
{
    TempDb db("cc_notes");
    REQUIRE(db.schema_ok());
    seed_world(db);
    CreateConditionSpec with_notes = valid();
    with_notes.notes = "line 1\nline 2\r\n\xE2\x82\xAC 5 \xF0\x9F\x98\x80 tab\t end";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", with_notes), "1");
    CHECK_EQ(db.text("SELECT typeof(Notes) FROM BidConditions WHERE UID = 1"), "text");
    CHECK(db.text("SELECT Notes FROM BidConditions WHERE UID = 1") == with_notes.notes);
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid()), "2");
    CHECK_EQ(db.text("SELECT typeof(Notes) FROM BidConditions WHERE UID = 2"), "null");
    CreateConditionSpec big = valid();
    big.notes = repeat("0123456789", 20000);
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", big), "3");
    CHECK_EQ(db.count("SELECT length(Notes) FROM BidConditions WHERE UID = 3"), 200000);
}

// Windows condition_operations.py:283-334 writes ONE row of ONE table (plan 4.4: nothing else is touched): after a success every other table of the database
// is byte for byte what it was, no takeoff, page, layer, folder or total included.
TEST(CondCreateNames, ASuccessChangesOnlyBidConditions)
{
    TempDb db("cc_only_conditions");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 1, 'Old', 0)");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (500, 1, 'Page')");
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (900, 1, 10, 500, '0;0;1;1')");
    const std::string others_before = snapshot_except(db, "BidConditions");
    const std::string conditions_before = db.dump("BidConditions");
    CreateConditionSpec spec = windows_full_spec();
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", spec), "11");
    CHECK_EQ(snapshot_except(db, "BidConditions"), others_before);
    CHECK_NE(db.dump("BidConditions"), conditions_before);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 2);
    CHECK_EQ(db.text("SELECT Name FROM BidConditions WHERE UID = 10"), "Old"); // the old row is untouched
}

// =====================================================================================================================
// The bid lock (plan 4.1, N-M1-01, register NC-12): Windows refuses silently on the local path (project_write_service.py:701-704)
// =====================================================================================================================
// What error text a locked-bid refusal carries is not pinned (the UI shows "This bid is locked." through the shared helper before the form opens; plan
// 4.4 item 2 only says the writer refuses): every case is checked by the refusal with the database unchanged AND a positive control after the lock is lifted.

namespace
{
    void seed_status(const TempDb &db, long long uid, const std::string &locked_sql, const std::string &name = "Status")
    {
        db.must("INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (" + std::to_string(uid) + ", '" + name + "', " + locked_sql + ", " +
                std::to_string(uid) + ")");
    }

    void set_bid_status(const TempDb &db, long long bid_uid, const std::string &status_sql)
    {
        db.must("UPDATE Bids SET JobStatusUID = " + status_sql + " WHERE UID = " + std::to_string(bid_uid));
    }
}

// A bid whose job status is Locked takes no new condition: refused, not one table changes (the control: once the status is Open the same create works and gets the
// first UID and number).
TEST(CondCreateLock, ALockedBidIsRefusedAndNothingIsWritten)
{
    TempDb db("cc_locked");
    REQUIRE(db.schema_ok());
    seed_world(db);
    seed_status(db, 5, "-1", "Locked");
    set_bid_status(db, 1, "5");
    expect_refused(db, "1", valid(), std::nullopt);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 0);
    db.must("UPDATE JobStatuses SET Locked = 0 WHERE UID = 5");
    expect_created(db, "1", valid(), "1", 1);
}

// The lock is the lock of the bid of the write: bid 2 locked does not stop bid 1, and bid 2 itself is refused.
TEST(CondCreateLock, OnlyTheLockOfTheBidOfTheWriteCounts)
{
    TempDb db("cc_locked_other");
    REQUIRE(db.schema_ok());
    seed_world(db);
    seed_status(db, 5, "-1", "Locked");
    set_bid_status(db, 2, "5");
    expect_created(db, "1", valid(), "1", 1);
    expect_refused(db, "2", valid(), std::nullopt);
    db.must("UPDATE JobStatuses SET Locked = 0 WHERE UID = 5");
    expect_created(db, "2", valid(), "2", 1);
}

// The Windows reading of the Locked flag (settings_reader.py:68-73: set unless its stored text is "0", "False" or ""; an Access Yes is -1): -1, 1, 2 and NULL lock
// the bid, 0, 'False' and '' do not. (The guard itself is pinned in test_bid_lock.cpp; this pins that the writer uses it.)
TEST(CondCreateLock, TheLockedFlagIsReadLikeWindows)
{
    const struct
    {
        const char *locked_sql;
        bool locked;
    } cases[] = {{"-1", true}, {"1", true}, {"2", true}, {"NULL", true}, {"0", false}, {"'False'", false}, {"''", false}};
    int index = 0;
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT(std::string("Locked = ") + c.locked_sql);
        TempDb db("cc_lock_flag_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        seed_world(db);
        seed_status(db, 5, c.locked_sql);
        set_bid_status(db, 1, "5");
        if (c.locked)
        {
            expect_refused(db, "1", valid(), std::nullopt);
            db.must("UPDATE JobStatuses SET Locked = 0 WHERE UID = 5");
        }
        expect_created(db, "1", valid(), "1", 1);
    }
}

// No job status is no lock (cover_sheet.py:15-23: None or "" -> False), a status of another name than Locked is open, and a status that no JobStatuses row names is
// treated as LOCKED (project_read_service.py:81-94, fail closed: the row is missing).
TEST(CondCreateLock, NoStatusIsOpenAndAnUnresolvedStatusIsLocked)
{
    {
        TempDb db("cc_lock_none");
        REQUIRE(db.schema_ok());
        seed_world(db);
        expect_created(db, "1", valid(), "1", 1); // JobStatusUID is NULL
    }
    {
        TempDb db("cc_lock_open");
        REQUIRE(db.schema_ok());
        seed_world(db);
        seed_status(db, 4, "0", "Open");
        set_bid_status(db, 1, "4");
        expect_created(db, "1", valid(), "1", 1);
    }
    {
        TempDb db("cc_lock_unresolved");
        REQUIRE(db.schema_ok());
        seed_world(db);
        seed_status(db, 4, "0", "Open");
        set_bid_status(db, 1, "99"); // no JobStatuses row 99
        expect_refused(db, "1", valid(), std::nullopt);
        set_bid_status(db, 1, "4");
        expect_created(db, "1", valid(), "1", 1);
    }
}

// Fail closed on a failed read (settings_reader.py:361-370: duplicate JobStatuses UIDs or any failed read give an empty list = unresolved = locked): duplicate status
// UIDs anywhere in the table, and a bid with a status but no JobStatuses table, are refused; a bid WITHOUT a status needs no table at all.
TEST(CondCreateLock, AFailedStatusReadIsLocked)
{
    {
        SCOPED_CONTEXT("duplicate JobStatuses UIDs");
        TempDb db("cc_lock_duplicate", false);
        db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
        db.must("INSERT INTO Bids VALUES (1, 4)");
        db.must("CREATE TABLE JobStatuses (UID INTEGER, Name TEXT, Locked, Sequence INTEGER)");
        db.must("INSERT INTO JobStatuses VALUES (4, 'Open', 0, 1)");
        db.must("INSERT INTO JobStatuses VALUES (4, 'Open again', 0, 2)");
        db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, RefNo INTEGER, Name TEXT, Type INTEGER)");
        expect_refused(db, "1", valid(), std::nullopt);
        db.must("DELETE FROM JobStatuses WHERE Sequence = 2");
        expect_created(db, "1", valid(), "1", 1, false);
    }
    {
        SCOPED_CONTEXT("a status but no JobStatuses table");
        TempDb db("cc_lock_no_table", false);
        db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
        db.must("INSERT INTO Bids VALUES (1, 4)");
        db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, RefNo INTEGER, Name TEXT, Type INTEGER)");
        expect_refused(db, "1", valid(), std::nullopt);
        db.must("UPDATE Bids SET JobStatusUID = NULL");
        expect_created(db, "1", valid(), "1", 1, false); // no status: no table is read
    }
    {
        SCOPED_CONTEXT("no Bids.JobStatusUID column");
        TempDb db("cc_lock_no_column", false);
        db.must("CREATE TABLE Bids (UID INTEGER)");
        db.must("INSERT INTO Bids VALUES (1)");
        db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, RefNo INTEGER, Name TEXT, Type INTEGER)");
        expect_refused(db, "1", valid(), std::nullopt);
        db.must("ALTER TABLE Bids ADD COLUMN JobStatusUID");
        expect_created(db, "1", valid(), "1", 1, false);
    }
}

// =====================================================================================================================
// Rollbacks (TempDb::block, snapshot_all)
// =====================================================================================================================

// The insert is one transaction: a failing INSERT leaves nothing behind (no row, no other table touched) and reports the generic failure (empty error text), and
// the next call works once the failure is gone and gets the UID and the number the failed one must not have consumed. (Windows: _connection rolls back on any
// exception, mdb_writer.py:63-105.)
TEST(CondCreateRollback, AFailedInsertLeavesNothingBehind)
{
    TempDb db("cc_rollback");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string trigger = db.block("BidConditions", "INSERT");
    expect_refused(db, "1", windows_full_spec(), std::string());
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 0);
    db.unblock(trigger);
    expect_created(db, "1", valid(), "1", 1);
}

// AT the number limit the refusal comes BEFORE any write (Windows :56-62 precede the INSERT): with an INSERT that would fail the caller still gets the overflow
// text, not the generic failure (the lesson of the mutation re-check of duplicate_condition: a guard placed after the INSERT would give the empty error here).
TEST(CondCreateRollback, TheNumberLimitComesBeforeTheInsert)
{
    TempDb db("cc_rollback_limit");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483647LL);
    const std::string trigger = db.block("BidConditions", "INSERT");
    expect_refused(db, "1", valid(), overflow_text(1, "2147483648"));
    db.unblock(trigger);
    db.must("UPDATE BidConditions SET RefNo = 3 WHERE UID IN (11, 20)");
    expect_created(db, "1", valid(), "21", 4);
}

// The same for the UID limit: the "no identity" text comes before the INSERT.
TEST(CondCreateRollback, TheUidLimitComesBeforeTheInsert)
{
    TempDb db("cc_rollback_uid");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (2147483647, 1, 1, 'High', 0)");
    const std::string trigger = db.block("BidConditions", "INSERT");
    expect_refused(db, "1", valid(), kNoIdentityText);
    db.unblock(trigger);
    db.must("DELETE FROM BidConditions");
    expect_created(db, "1", valid(), "1", 1);
}

// A refusal that was decided inside the transaction (the overflow rolls back an EMPTY transaction, BEGIN IMMEDIATE was taken) leaves no lock behind: a second
// connection can write straight afterwards (refusal_call proves it for every refusal of this file; here with a real write by the second connection).
TEST(CondCreateRollback, ARefusalLeavesNoTransactionOpen)
{
    TempDb db("cc_no_lock_left");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483647LL);
    CreateConditionSpec foreign;
    foreign.layer_uid = "31";
    for (const CreateConditionSpec &spec : {valid(), foreign, valid(repeat("a", 256)), valid("")})
    {
        std::string error;
        CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", spec, &error), "");
        ost_db::OstDatabase other;
        REQUIRE(other.open(db.path()));
        REQUIRE(other.begin_transaction());
        CHECK(other.exec("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (99, 2, 1, 'direct', 0)"));
        CHECK(other.commit());
        db.must("DELETE FROM BidConditions WHERE UID = 99");
    }
    // the positive control: once a number is free the same call writes the row (a stub that only refuses cannot pass this test)
    db.must("UPDATE BidConditions SET RefNo = 1 WHERE UID IN (11, 20)");
    expect_created(db, "1", valid(), "21", 2);
}

// =====================================================================================================================
// What the row looks like to Windows and to the loader
// =====================================================================================================================

// The Windows column set and nothing more (plan 5.1 tests-first plan; research section 4 item 16): the columns Windows never writes stay NULL after an iOS
// create (ColorLine, ExternalID, OpenExternalID, DirectQuantity1-3, ExcelCell1-3, TypGroupUID, BFperLF, SnapToLinearTolerance and, for a spec that does not set
// them, the three iOS-only columns), the columns it writes are never NULL (the nullable references and the notes aside). A guard for the M3 round trip.
TEST(CondCreateShape, OnlyTheWindowsColumnSetIsWritten)
{
    TempDb db("cc_shape");
    REQUIRE(db.schema_ok());
    seed_world(db);
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", windows_full_spec()), "1");
    for (const char *column : {"ColorLine", "ExternalID", "OpenExternalID", "DirectQuantity1", "DirectQuantity2", "DirectQuantity3", "ExcelCell1", "ExcelCell2",
                               "ExcelCell3", "TypGroupUID", "BFperLF", "SnapToLinearTolerance", "Connect", "ConnectTolerance", "SnapToLinear"})
    {
        SCOPED_CONTEXT(std::string("column ") + column);
        CHECK_EQ(db.text(std::string("SELECT typeof([") + column + "]) FROM BidConditions WHERE UID = 1"), "null");
    }
    for (const char *column : {"UID", "BidUID", "GUID", "RefNo", "IsTemplate", "Curve", "SnapToGrid", "ManualLength", "MatAmount", "LabAmount", "SubAmount",
                               "FontName", "FontSize", "FontBold", "FontItalic", "ShowTakeoff", "Backout", "Name", "Type", "CdnTypeUID", "BidLayerUID", "Height",
                               "Width", "Depth", "Thickness", "Rise", "Run", "DisplaySize", "Shape", "Pattern", "Spacing", "ColorFill", "UOM1", "UOM2", "UOM3",
                               "Quantity1", "Quantity2", "Quantity3", "Notes", "DropRun", "DropValue", "RoundQuantity", "RoundUp", "Trim", "IsCurvedSegment",
                               "Grid", "GridSize1", "GridSize2", "Gap", "DisplayDimension", "DisplayName", "DisplayGridWhileDrawing", "BidConditionFolderUID"})
    {
        SCOPED_CONTEXT(std::string("column ") + column);
        CHECK_NE(db.text(std::string("SELECT typeof([") + column + "]) FROM BidConditions WHERE UID = 1"), "null");
    }
}

// N-M1-12 (D9): the three iOS-only columns are written when the spec sets them (build_create_spec sets them for every style), as an integer flag, a real and an
// integer; a spec that leaves them unset leaves NULL (the test above).
TEST(CondCreateShape, TheIosOnlyColumnsAreWrittenWhenTheSpecSetsThem)
{
    TempDb db("cc_shape_ios");
    REQUIRE(db.schema_ok());
    seed_world(db);
    CreateConditionSpec on = valid("On");
    on.connect = true;
    on.connect_tolerance = 6.0;
    on.snap_to_linear = -1;
    CreateConditionSpec off = valid("Off");
    off.connect = false;
    off.connect_tolerance = 2.5;
    off.snap_to_linear = 1;
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", on), "1");
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", off), "2");
    CHECK_EQ(quoted_row(db, "1", {"Connect", "ConnectTolerance", "SnapToLinear"}), "1 6.0 -1");
    CHECK_EQ(quoted_row(db, "2", {"Connect", "ConnectTolerance", "SnapToLinear"}), "0 2.5 1");
}

// The whole chain Core -> store for every style: the spec build_create_spec makes for an untouched form of each style is written (all four, both systems), the row has
// the iOS-only columns set and the values of the type row, and a reload reads them back (a metric bid: through the loader's unit normalisation, which keeps the
// metric units the metric rows already have).
TEST(CondCreateShape, ABuiltSpecOfEveryStyleIsWrittenAndReadBack)
{
    for (const bool metric : {false, true})
        for (int type = 0; type <= 3; ++type)
        {
            SCOPED_CONTEXT("type " + std::to_string(type) + (metric ? " metric" : " imperial"));
            TempDb db("cc_chain_" + std::to_string(type) + (metric ? "m" : "i"));
            REQUIRE(db.schema_ok());
            seed_world(db);
            const std::string bid = metric ? "2" : "1";
            ost_domain::ConditionChanges changes;
            changes.values.condition_type = static_cast<ost_domain::ConditionType>(type);
            changes.values.name = "Chain";
            changes.mark(ost_domain::ConditionField::ConditionType);
            changes.mark(ost_domain::ConditionField::Name);
            const std::string layer = metric ? "31" : "30";
            const CreateConditionSpec spec =
                ost_domain::build_create_spec(ost_domain::new_condition_form(layer, metric), changes, metric ? "41" : "40", layer, metric);
            CHECK_STR_EQ(ost_store::insert_condition(db.path(), bid, spec), "1");
            CHECK_EQ(typed(db, "1", "Type"), "integer:" + std::to_string(type));
            CHECK_EQ(typed(db, "1", "Name"), "text:'Chain'");
            CHECK_EQ(db.text("SELECT typeof(Connect) || typeof(ConnectTolerance) || typeof(SnapToLinear) FROM BidConditions WHERE UID = 1"), "integerrealinteger");
            CHECK_EQ(db.count("SELECT BidLayerUID FROM BidConditions WHERE UID = 1"), std::stoll(layer));
            CHECK_EQ(db.count("SELECT BidConditionFolderUID FROM BidConditions WHERE UID = 1"), metric ? 41 : 40);

            ost_data::ProjectData project;
            std::string load_error;
            REQUIRE(ost_store::load_project(db.path(), bid, project, &load_error));
            const auto it = project.conditions.find("1");
            REQUIRE(it != project.conditions.end());
            const ost_domain::Condition &c = it->second;
            CHECK_EQ(static_cast<int>(c.condition_type), type);
            CHECK_EQ(c.thickness, spec.thickness);
            CHECK_EQ(c.spacing, spec.spacing);
            CHECK_EQ(c.calc_type1, spec.calc_type1);
            CHECK_EQ(c.backout, spec.backout);
            CHECK_EQ(c.display_grid_while_drawing, spec.display_grid_while_drawing);
            CHECK_EQ(static_cast<int>(c.shape), spec.shape);
            REQUIRE(spec.connect.has_value());
            CHECK_EQ(c.connect, *spec.connect);
            CHECK_EQ(c.connect_tolerance, 6.0);
        }
}

// CONTROL (passes before the implementation: it writes the row with SQL, not with insert_condition): the loader of a METRIC bid turns the imperial unit 2 (feet of
// length) of a Linear condition into the metric unit 13 and reads a metric bid's own unit 13 unchanged; an imperial bid keeps 2. The tests below rely on it.
TEST(CondCreateRoundTrip, ControlTheLoaderNormalisesTheUnitsOfAMetricBid)
{
    TempDb db("cc_control_uom");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, UOM1, Quantity1) VALUES (1, 1, 1, 'imperial', 0, 2, 1)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, UOM1, Quantity1) VALUES (2, 2, 1, 'metric from feet', 0, 2, 1)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type, UOM1, Quantity1) VALUES (3, 2, 2, 'metric own', 0, 13, 1)");
    ost_data::ProjectData imperial = ost_store::load_project(db.path(), "1");
    ost_data::ProjectData metric = ost_store::load_project(db.path(), "2");
    REQUIRE(!imperial.empty());
    REQUIRE(!metric.empty());
    CHECK_EQ(imperial.conditions.at("1").uom1, 2);
    CHECK_EQ(metric.conditions.at("2").uom1, 13);
    CHECK_EQ(metric.conditions.at("3").uom1, 13);
}

namespace
{
    // What the loader must read for the row of windows_full_spec() with trim / curved segment off and a display size of 120, written as condition 11 of bid 1 with
    // the number 5 (the form never saves Trim and Curved Segment together, and allows 10..500 percent).
    void expect_loaded_like_the_full_spec(const ost_data::ProjectData &project)
    {
        REQUIRE_EQ(project.conditions.count("11"), size_t(1));
        const ost_domain::Condition &c = project.conditions.at("11");
        CHECK_EQ(c.ref_no, 5);
        CHECK_STR_EQ(c.name, "Wall");
        CHECK_EQ(static_cast<int>(c.condition_type), 3);
        CHECK_STR_EQ(c.layer_uid, "30");
        CHECK_STR_EQ(c.condition_folder_uid, "40");
        CHECK_STR_EQ(c.cdn_type_uid, "7");
        CHECK_STR_EQ(c.cdn_type_name, "Concrete");
        CHECK_STR_EQ(c.notes, "memo");
        CHECK(c.backout);
        CHECK_EQ(c.height, 1.5);
        CHECK_EQ(c.width, 2.5);
        CHECK_EQ(c.depth, 3.5);
        CHECK_EQ(c.thickness, 4.5);
        CHECK_EQ(c.rise, 5.5);
        CHECK_EQ(c.run, 6.5);
        CHECK_EQ(c.display_size, 120.0);
        CHECK_EQ(static_cast<int>(c.shape), 2);
        CHECK_EQ(static_cast<int>(c.pattern), 3);
        CHECK_EQ(c.spacing, 8.5);
        CHECK_EQ(c.color_fill, 9);
        CHECK_EQ(c.calc_type1, 4);
        CHECK_EQ(c.calc_type2, 5);
        CHECK_EQ(c.calc_type3, 6);
        CHECK(c.drop_run);
        CHECK_EQ(c.drop_value, 9.5);
        CHECK(c.round_quantity);
        CHECK_EQ(c.round_up, 10.5);
        CHECK_FALSE(c.trim);
        CHECK_FALSE(c.is_curved_segment);
        CHECK(c.grid);
        CHECK_EQ(c.grid_size1, 11.5);
        CHECK_EQ(c.grid_size2, 12.5);
        CHECK_EQ(c.gap, 13.5);
        CHECK(c.display_dimension);
        CHECK(c.display_name);
        CHECK(c.display_grid_while_drawing);
        CHECK_EQ(project.conditions.size(), size_t(2));
        CHECK_STR_EQ(project.conditions.at("10").name, "Old"); // the other condition is as it was
    }

    void seed_round_trip_world(const TempDb &db)
    {
        seed_world(db);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 4, 'Old', 0)");
    }
}

// CONTROL (passes before the implementation: the row is written with SQL, not with insert_condition): the loader reads a full row of the Windows column set as
// expect_loaded_like_the_full_spec says, so the next test differs from this one only in the writer.
TEST(CondCreateRoundTrip, ControlTheLoaderReadsAFullRowWrittenBySql)
{
    TempDb db("cc_control_full_row");
    REQUIRE(db.schema_ok());
    seed_round_trip_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type, CdnTypeUID, BidLayerUID, BidConditionFolderUID, Height, Width, Depth, Thickness, "
            "Rise, Run, DisplaySize, Shape, Pattern, Spacing, ColorFill, UOM1, UOM2, UOM3, Quantity1, Quantity2, Quantity3, Notes, DropRun, DropValue, "
            "RoundQuantity, RoundUp, Trim, IsCurvedSegment, Grid, GridSize1, GridSize2, Gap, DisplayDimension, DisplayName, DisplayGridWhileDrawing, Backout) "
            "VALUES (11, 1, '{00000000-0000-4000-8000-000000000000}', 5, 'Wall', 3, 7, 30, 40, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 120, 2, 3, 8.5, 9, 1, 2, 3, 4, 5, 6, "
            "'memo', 1, 9.5, 1, 10.5, 0, 0, 1, 11.5, 12.5, 13.5, 1, 1, 1, -1)");
    ost_data::ProjectData project;
    std::string error;
    REQUIRE(ost_store::load_project(db.path(), "1", project, &error));
    expect_loaded_like_the_full_spec(project);
}

// A new condition loads through ost_store::load_project with exactly the values of its spec (research section 4 item 15): the identity, the number, the name,
// the type, the layer, the folder, the condition type and its name, the notes and every number and flag.
TEST(CondCreateRoundTrip, TheNewConditionLoadsWithExactlyTheSpecValues)
{
    TempDb db("cc_round_trip");
    REQUIRE(db.schema_ok());
    seed_round_trip_world(db);
    CreateConditionSpec spec = windows_full_spec();
    spec.trim = false; // the form never saves both; a loader normalises them: keep this round trip about the plain columns
    spec.is_curved_segment = false;
    spec.display_size = 120.0; // the form allows 10..500 percent
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", spec), "11");
    ost_data::ProjectData project;
    std::string error;
    REQUIRE(ost_store::load_project(db.path(), "1", project, &error));
    expect_loaded_like_the_full_spec(project);
}

// is_top and z_value are not columns: the loader derives them from the NAME the form built (COND-ELEV-01): "Wall @B 10' 3\"" is a bottom elevation of 123
// inches, "Wall @T 10' 6\"" a top one of 126, a plain name has none. ("10'" alone is no elevation text: the loader reads feet only together with inches, so
// "Wall @T 10'" is (0.0, not top).) The values are the output of the Windows extract_z_value_from_name (infrastructure/parsers/position_parser.py:21-31), run
// read-only: (123.0, False), (126.0, True), (0.0, False) for "Wall @T 10'" and for "Wall".
TEST(CondCreateRoundTrip, TheElevationIsDerivedFromTheName)
{
    TempDb db("cc_round_elevation");
    REQUIRE(db.schema_ok());
    seed_world(db);
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid("Wall @B 10' 3\"")), "1");
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid("Wall @T 10' 6\"")), "2");
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", valid("Wall")), "3");
    ost_data::ProjectData project = ost_store::load_project(db.path(), "1");
    REQUIRE(!project.empty());
    CHECK_EQ(project.conditions.at("1").z_value, 123.0);
    CHECK_FALSE(project.conditions.at("1").is_top);
    CHECK_EQ(project.conditions.at("2").z_value, 126.0);
    CHECK(project.conditions.at("2").is_top);
    CHECK_EQ(project.conditions.at("3").z_value, 0.0);
    CHECK_FALSE(project.conditions.at("3").is_top);
    CHECK_STR_EQ(project.conditions.at("1").name, "Wall @B 10' 3\"");
}

// A Linear condition created in a METRIC bid with the metric unit 13 reads back as 13, and in an imperial bid with the unit 2 as 2 (the loader's normalisation,
// see the control above): what the form shows is what a reload shows.
TEST(CondCreateRoundTrip, TheUnitsOfANewConditionSurviveTheLoader)
{
    TempDb db("cc_round_uom");
    REQUIRE(db.schema_ok());
    seed_world(db);
    CreateConditionSpec metric_spec = valid("Metric");
    metric_spec.condition_type = 0;
    metric_spec.calc_type1 = 1;
    metric_spec.uom1 = 13;
    CreateConditionSpec imperial_spec = valid("Imperial");
    imperial_spec.condition_type = 0;
    imperial_spec.calc_type1 = 1;
    imperial_spec.uom1 = 2;
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "2", metric_spec), "1");
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", imperial_spec), "2");
    CHECK_EQ(db.count("SELECT UOM1 FROM BidConditions WHERE UID = 1"), 13);
    CHECK_EQ(db.count("SELECT UOM1 FROM BidConditions WHERE UID = 2"), 2);
    const ost_data::ProjectData metric_project = ost_store::load_project(db.path(), "2");
    const ost_data::ProjectData imperial_project = ost_store::load_project(db.path(), "1");
    REQUIRE(!metric_project.empty());
    REQUIRE(!imperial_project.empty());
    CHECK_EQ(metric_project.conditions.at("1").uom1, 13);
    CHECK_EQ(imperial_project.conditions.at("2").uom1, 2);
}

// Port of test_project_write_service.py:1355 (test_condition_create_result_keeps_uid_when_refresh_fails: the write worked, the reload did not: the uid is kept)
// and research section 4 item 12: a bid whose folder graph is cyclic cannot be loaded (Windows refuses to read it), but the store does not read the graph:
// the insert into a folder of the cycle succeeds and returns its uid; the load then fails with the message it gave before.
TEST(CondCreateRoundTrip, TheUidIsKeptWhenOnlyTheReloadFails)
{
    TempDb db("cc_reload_fails");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, ParentUID, Name) VALUES (60, 1, 61, 'A')");
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, ParentUID, Name) VALUES (61, 1, 60, 'B')");
    ost_data::ProjectData project;
    std::string error_before;
    REQUIRE_EQ(ost_store::load_project(db.path(), "1", project, &error_before), false);
    CHECK(project.empty());
    CHECK(!error_before.empty());

    CreateConditionSpec spec = valid("In the cycle");
    spec.folder_uid = "60";
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_condition(db.path(), "1", spec, &error), "1");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT BidConditionFolderUID FROM BidConditions WHERE UID = 1"), 60);
    std::string error_after;
    REQUIRE_EQ(ost_store::load_project(db.path(), "1", project, &error_after), false);
    CHECK_STR_EQ(error_after, error_before);
}

// =====================================================================================================================
// allocate_condition_identities(count): the allocator M2's cross-bid paste reuses (plan 4.2, 5.1)
// =====================================================================================================================

namespace
{
    struct Allocation
    {
        bool ok = false;
        std::vector<ost_store::ConditionIdentity> ids;
        std::string error = "stale";
    };

    // The allocator on a connection of its own inside a transaction (the writers' place), rolled back afterwards.
    Allocation allocate(const TempDb &db, long long bid, long long count, bool in_transaction = true)
    {
        Allocation out;
        ost_db::OstDatabase connection;
        if (!connection.open(db.path()))
            return out;
        if (in_transaction && !connection.begin_transaction())
            return out;
        out.ids.resize(2); // the contract: replaced, whatever it held
        out.ok = ost_store::allocate_condition_identities(connection, bid, count, out.ids, &out.error);
        if (in_transaction)
            connection.rollback();
        return out;
    }

    std::string uids_of(const Allocation &a)
    {
        std::string out;
        for (const auto &id : a.ids)
            out += (out.empty() ? "" : ",") + std::to_string(id.uid);
        return out;
    }

    std::string refs_of(const Allocation &a)
    {
        std::string out;
        for (const auto &id : a.ids)
            out += (out.empty() ? "" : ",") + std::to_string(id.ref_no);
        return out;
    }

    void expect_allocation_refused(const Allocation &a, const std::string &want_error)
    {
        CHECK_FALSE(a.ok);
        CHECK(a.ids.empty());
        CHECK_STR_EQ(a.error, want_error);
    }
}

// Port of the Windows allocator for count 1 and 3 (condition_operations.py:42-66), the oracle run in memory over the sqlite schema: bid 1 holds condition 10 with
// number 5: one identity is (UID 11, number 6), three are the UIDs 11, 12, 13 and the numbers 6, 7, 8, each with its own GUID of the Windows format.
TEST(CondCreateAlloc, ConsecutiveUidsNumbersAndDistinctGuids)
{
    TempDb db("alloc_basic");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 5, 'c', 0)");
    const Allocation one = allocate(db, 1, 1);
    REQUIRE(one.ok);
    CHECK_STR_EQ(one.error, "");
    CHECK_STR_EQ(uids_of(one), "11");
    CHECK_STR_EQ(refs_of(one), "6");
    CHECK(is_ost_guid(one.ids[0].guid));
    const Allocation three = allocate(db, 1, 3);
    REQUIRE(three.ok);
    CHECK_STR_EQ(three.error, "");
    CHECK_STR_EQ(uids_of(three), "11,12,13");
    CHECK_STR_EQ(refs_of(three), "6,7,8");
    std::set<std::string> guids;
    for (const auto &id : three.ids)
    {
        CHECK(is_ost_guid(id.guid));
        guids.insert(id.guid);
    }
    CHECK_EQ(guids.size(), size_t(3));
    // two calls give the same answer: the allocator reserves nothing
    CHECK_STR_EQ(uids_of(allocate(db, 1, 3)), "11,12,13");
}

// The numbers and UIDs follow the same rules as insert_condition: the UID is the MAX over all bids raised above every reference, the number is the bid's own MAX,
// NULL numbers are ignored, a bid without numbers (or without the column) starts at 1.
TEST(CondCreateAlloc, TheSameRulesAsTheInsert)
{
    TempDb db("alloc_rules");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (3, 1, 5, 'c', 0)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (4, 2, 40, 'c', 0)");
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (5, 1, NULL, 'c', 0)");
    const Allocation own = allocate(db, 1, 2);
    CHECK_STR_EQ(uids_of(own), "6,7");
    CHECK_STR_EQ(refs_of(own), "6,7");
    const Allocation other = allocate(db, 2, 2);
    CHECK_STR_EQ(uids_of(other), "6,7");
    CHECK_STR_EQ(refs_of(other), "41,42");
    // a dangling reference above MAX(UID) raises the first UID (DBP-18)
    db.must("INSERT INTO BidTakeoffTotals (UID, BidConditionUID) VALUES (1, 50)");
    CHECK_STR_EQ(uids_of(allocate(db, 1, 3)), "51,52,53");
    CHECK_STR_EQ(refs_of(allocate(db, 1, 3)), "6,7,8");
    // a bid without conditions starts at 1
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (3, 1, 'Bid three', 3)");
    CHECK_STR_EQ(refs_of(allocate(db, 3, 2)), "1,2");
}

// Windows, the three last-free-number rows of the oracle run in memory (count 3 at 2147483644 -> 2147483645..2147483647; count 2 at 2147483645 ->
// 2147483646, 2147483647): the last free numbers can be allocated together.
TEST(CondCreateAlloc, TheLastFreeNumbersCanBeAllocated)
{
    {
        TempDb db("alloc_last_three");
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 2147483644, 'c', 0)");
        const Allocation a = allocate(db, 1, 3);
        REQUIRE(a.ok);
        CHECK_STR_EQ(refs_of(a), "2147483645,2147483646,2147483647");
        CHECK_STR_EQ(uids_of(a), "11,12,13");
    }
    {
        TempDb db("alloc_last_two");
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 2147483645, 'c', 0)");
        const Allocation a = allocate(db, 1, 2);
        REQUIRE(a.ok);
        CHECK_STR_EQ(refs_of(a), "2147483646,2147483647");
    }
}

// Port of ...::test_adding_past_the_highest_number_raises_and_writes_nothing, the "duplicate two" row (:1376-1381, text with "2 conditions") and plan 5.1
// ("allocate_condition_identities(3) on a bid whose highest number is 2147483645 is refused whole with nothing written"): the whole set is refused when its LAST
// number passes 2147483647, with the Windows text for that count and that last number (the oracle run in memory: (2, 2147483646) -> reach 2147483648; (3,
// 2147483645) -> 2147483648; (3, 2147483646) -> 2147483649; (1, 2147483647) -> 2147483648; (1, 2147483648) -> 2147483649; (1, LLONG_MAX) ->
// 9223372036854775808).
TEST(CondCreateAlloc, ASetWhoseLastNumberPassesTheHighestIsRefusedWhole)
{
    const struct
    {
        long long count;
        long long highest;
        const char *last;
    } cases[] = {
        {2, 2147483646LL, "2147483648"}, {3, 2147483645LL, "2147483648"}, {3, 2147483646LL, "2147483649"},
        {1, 2147483647LL, "2147483648"}, {1, 2147483648LL, "2147483649"}, {1, 9223372036854775807LL, "9223372036854775808"},
    };
    int index = 0;
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT("count " + std::to_string(c.count) + ", highest " + std::to_string(c.highest));
        TempDb db("alloc_refused_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        seed_world(db);
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, " + std::to_string(c.highest) + ", 'c', 0)");
        const std::string before = db.snapshot_all();
        expect_allocation_refused(allocate(db, 1, c.count), overflow_text(c.count, c.last));
        expect_allocation_refused(allocate(db, 1, c.count, false), overflow_text(c.count, c.last)); // outside a transaction too
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// The UID limit refuses the whole set as well: MAX(UID) 2147483645 leaves 2147483646 and 2147483647, so two identities are fine and three are not (the no-identity
// text); a reference at 2147483647 leaves none.
TEST(CondCreateAlloc, ASetWhoseLastUidPassesTheLimitIsRefusedWhole)
{
    TempDb db("alloc_uid_limit");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (2147483645, 1, 1, 'c', 0)");
    const Allocation two = allocate(db, 1, 2);
    REQUIRE(two.ok);
    CHECK_STR_EQ(uids_of(two), "2147483646,2147483647");
    const std::string before = db.snapshot_all();
    expect_allocation_refused(allocate(db, 1, 3), kNoIdentityText);
    CHECK_EQ(db.snapshot_all(), before);
    db.must("INSERT INTO BidTakeoffTotals (UID, BidConditionUID) VALUES (1, 2147483647)");
    expect_allocation_refused(allocate(db, 1, 1), kNoIdentityText);
}

// A count below one is a caller bug: refused with no text (nothing to reserve). A count that no limit allows (beyond 32 bits, LLONG_MAX) is refused WITHOUT being
// tried: the allocator checks both limits before it builds one identity (a loop over such a count would never end, a reserve() of it would throw).
TEST(CondCreateAlloc, ANonsenseCountIsRefusedWithoutBeingTried)
{
    TempDb db("alloc_count");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 1, 'c', 0)");
    for (const long long count : {0LL, -1LL, -2147483647LL, std::numeric_limits<long long>::min()})
    {
        SCOPED_CONTEXT("count " + std::to_string(count));
        expect_allocation_refused(allocate(db, 1, count), "");
    }
    for (const long long count : {4294967296LL, 4294967297LL, 9223372036854775806LL, std::numeric_limits<long long>::max()})
    {
        SCOPED_CONTEXT("count " + std::to_string(count));
        const Allocation a = allocate(db, 1, count);
        CHECK_FALSE(a.ok);
        CHECK(a.ids.empty());
    }
    const Allocation fine = allocate(db, 1, 1);
    REQUIRE(fine.ok);
    CHECK_STR_EQ(uids_of(fine), "11");
}

// New mutation-check case (fixer, not a Windows port): the cap of one call is one million identities (the plan has no use for more than a paste of a few
// thousand conditions); the first count above it is refused before one identity is built, with no text and with the database untouched. (The success side of
// the cap is not run: it would build a million identities.)
TEST(CondCreateAlloc, TheFirstCountAboveOneMillionIsRefusedWithoutBeingTried)
{
    TempDb db("alloc_cap");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 1, 'c', 0)");
    const std::string before = db.snapshot_all();
    for (const long long count : {1000001LL, 1000002LL, 2147483647LL})
    {
        SCOPED_CONTEXT("count " + std::to_string(count));
        expect_allocation_refused(allocate(db, 1, count), "");
    }
    CHECK_EQ(db.snapshot_all(), before);
    const Allocation fine = allocate(db, 1, 3);
    REQUIRE(fine.ok);
    CHECK_STR_EQ(uids_of(fine), "11,12,13");
}

// The allocator only READS and sees what the caller's own transaction has written (it is meant to run inside the transaction that inserts the rows): after the
// caller inserted condition 50 (number 9) on the same connection, the next identities are 51, 52 and 10, 11; and a call outside any transaction leaves the whole
// database as it was.
TEST(CondCreateAlloc, ItReadsTheCallersTransactionAndWritesNothing)
{
    TempDb db("alloc_in_transaction");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (10, 1, 5, 'c', 0)");
    {
        ost_db::OstDatabase connection;
        REQUIRE(connection.open(db.path()));
        REQUIRE(connection.begin_transaction());
        REQUIRE(connection.exec("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (50, 1, 9, 'uncommitted', 0)"));
        std::vector<ost_store::ConditionIdentity> ids;
        std::string error = "stale";
        REQUIRE(ost_store::allocate_condition_identities(connection, 1, 2, ids, &error));
        CHECK_STR_EQ(error, "");
        REQUIRE_EQ(ids.size(), size_t(2));
        CHECK_EQ(ids[0].uid, 51);
        CHECK_EQ(ids[1].uid, 52);
        CHECK_EQ(ids[0].ref_no, 10);
        CHECK_EQ(ids[1].ref_no, 11);
        connection.rollback();
    }
    const std::string before = db.snapshot_all();
    const Allocation outside = allocate(db, 1, 3, false);
    REQUIRE(outside.ok);
    CHECK_STR_EQ(uids_of(outside), "11,12,13");
    CHECK_EQ(db.snapshot_all(), before);
    CHECK(no_write_lock_left(db));
}

// An unreadable table or a missing identity column is a failure with no text, never a crash and never a partial answer (Windows _require_write_columns, :44).
TEST(CondCreateAlloc, AnUnreadableTableOrColumnIsAGenericFailure)
{
    {
        SCOPED_CONTEXT("no BidConditions");
        TempDb db("alloc_no_table");
        REQUIRE(db.schema_ok());
        db.must("DROP TABLE BidConditions");
        expect_allocation_refused(allocate(db, 1, 1), "");
    }
    for (const char *columns : {"BidUID INTEGER, RefNo INTEGER", "UID INTEGER, RefNo INTEGER"})
    {
        SCOPED_CONTEXT(std::string("columns ") + columns);
        TempDb db("alloc_missing_identity_column", false);
        db.must(std::string("CREATE TABLE BidConditions (") + columns + ")");
        expect_allocation_refused(allocate(db, 1, 1), "");
    }
    {
        SCOPED_CONTEXT("no number column: the numbers start at 1");
        TempDb db("alloc_no_refno", false);
        db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER)");
        db.must("INSERT INTO BidConditions VALUES (10, 1)");
        const Allocation a = allocate(db, 1, 2);
        REQUIRE(a.ok);
        CHECK_STR_EQ(uids_of(a), "11,12");
        CHECK_STR_EQ(refs_of(a), "1,2");
    }
}
