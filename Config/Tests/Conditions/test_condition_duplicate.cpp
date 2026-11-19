// COND-DUP-01 (alias DBP-13): a duplicated condition gets a fresh GUID and the next reference number, keeps every other
// column verbatim, and the source condition must belong to the bid.
//
// Windows: ost_visualizer/infrastructure/mdb/components/condition_operations.py duplicate_conditions / _new_ost_guid.
// iOS duplicates one condition per call (Windows takes a list), so the batch tests are ported as consecutive calls.
// The UID of the copy (MAX(UID) + 1 raised above dangling references, Windows _next_uid_preserving_references) belongs to
// DBP-18 and is not tested here.
#include "ConditionsTestSupport.hpp"

#include <regex>
#include <set>

using ost_ctest::TempDb;

namespace
{
    // The iOS-schema counterpart of ConditionDuplicateContractTests._db: bid 1 has
    //   (UID 10, GUID '{OLD}', RefNo 1, 'Wall', Type 2, Notes 'memo', layer 30, folder 40, type 7, UOM/Quantity 2/5/8 and 1/4/20)
    //   (UID 11, '{TWO}', RefNo 2, 'Slab', Type 1)
    void seed_duplicate_world(const TempDb &db)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (1, 1, 'Bid one', 1, 0)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (2, 1, 'Bid two', 2, 1)");
        db.must("INSERT INTO BidLayers (UID, BidUID, Name, Show, Sequence) VALUES (30, 1, 'L30', -1, 1)");
        db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (40, 1, 'F40')");
        db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type, Notes, BidLayerUID, "
                "BidConditionFolderUID, CdnTypeUID, UOM1, UOM2, UOM3, Quantity1, Quantity2, Quantity3) "
                "VALUES (10, 1, '{OLD}', 1, 'Wall', 2, 'memo', 30, 40, 7, 2, 5, 8, 1, 4, 20)");
        db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) "
                "VALUES (11, 1, '{TWO}', 2, 'Slab', 1)");
    }

    bool is_ost_guid(const std::string &guid)
    {
        static const std::regex pattern("^\\{[0-9A-F]{8}(-[0-9A-F]{4}){3}-[0-9A-F]{12}\\}$");
        return std::regex_match(guid, pattern);
    }
}

// Port of ...::ConditionDuplicateContractTests::test_duplicates_get_fresh_identities_and_keep_every_other_value
// (duplicate 10 then 11 -> UIDs 12 and 13, RefNo 3 and 4, GUIDs of the Windows format that differ from each other and
// from their sources, every other column copied verbatim) and the DBP-13 / COND-DUP-01 test plans.
TEST(ConditionDuplicate, CopiesGetFreshIdentitiesAndKeepEveryOtherValue)
{
    TempDb db("dup_identity");
    REQUIRE(db.schema_ok());
    seed_duplicate_world(db);
    const std::string source10 = db.row("BidConditions", "10", {"UID", "GUID", "RefNo"});
    const std::string source11 = db.row("BidConditions", "11", {"UID", "GUID", "RefNo"});

    const std::string first = ost_store::duplicate_condition(db.path(), "1", "10");
    const std::string second = ost_store::duplicate_condition(db.path(), "1", "11");
    CHECK_STR_EQ(first, "12");
    CHECK_STR_EQ(second, "13");

    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 12"), 3);
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 13"), 4);
    CHECK_EQ(db.text("SELECT Name || '|' || Type || '|' || CAST(Notes AS TEXT) || '|' || BidLayerUID || '|' || "
                     "BidConditionFolderUID || '|' || CdnTypeUID FROM BidConditions WHERE UID = 12"),
             "Wall|2|memo|30|40|7");
    CHECK_EQ(db.text("SELECT Name FROM BidConditions WHERE UID = 13"), "Slab");
    // every other column of the copy equals its source
    CHECK_EQ(db.row("BidConditions", "12", {"UID", "GUID", "RefNo"}), source10);
    CHECK_EQ(db.row("BidConditions", "13", {"UID", "GUID", "RefNo"}), source11);

    const std::string guid12 = db.text("SELECT GUID FROM BidConditions WHERE UID = 12");
    const std::string guid13 = db.text("SELECT GUID FROM BidConditions WHERE UID = 13");
    CHECK(is_ost_guid(guid12));
    CHECK(is_ost_guid(guid13));
    CHECK_NE(guid12, guid13);
    CHECK_NE(guid12, "{OLD}");
    CHECK_NE(guid13, "{TWO}");
    // the sources are untouched
    CHECK_EQ(db.list("SELECT GUID FROM BidConditions WHERE UID IN (10, 11) ORDER BY UID"), "{OLD},{TWO}");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 4);
}

// DBP-13 plan: BidConditions (7, bid 1, GUID 'source-guid', RefNo 1): the copy's GUID matches the Windows format and
// differs from the source, RefNo = MAX(RefNo of the bid) + 1.
TEST(ConditionDuplicate, TheCopyOfASourceWithAnArbitraryGuidGetsAWindowsFormatGuid)
{
    TempDb db("dup_guid_format");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
    db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name) VALUES (7, 1, 'source-guid', 1, 'Source')");

    const std::string uid = ost_store::duplicate_condition(db.path(), "1", "7");
    REQUIRE(!uid.empty());
    const std::string guid = db.text("SELECT GUID FROM BidConditions WHERE UID = " + uid);
    CHECK(is_ost_guid(guid));
    CHECK_NE(guid, "source-guid");
    CHECK_EQ(db.text("SELECT GUID FROM BidConditions WHERE UID = 7"), "source-guid");
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = " + uid), 2);
}

// Every duplicate gets its own GUID: many copies of one source never repeat a GUID and never reuse the source's. The
// generator is a random v4 UUID in the Windows format (version nibble 4, variant 8..B), like Python's uuid4().
TEST(ConditionDuplicate, ManyCopiesNeverRepeatAGuid)
{
    TempDb db("dup_many_guids");
    REQUIRE(db.schema_ok());
    seed_duplicate_world(db);

    std::set<std::string> guids = {"{OLD}", "{TWO}"};
    for (int i = 0; i < 40; ++i)
    {
        SCOPED_CONTEXT("copy " + std::to_string(i));
        const std::string uid = ost_store::duplicate_condition(db.path(), "1", "10");
        REQUIRE(!uid.empty());
        const std::string guid = db.text("SELECT GUID FROM BidConditions WHERE UID = " + uid);
        REQUIRE(is_ost_guid(guid));
        CHECK_EQ(guid.size(), size_t(38));
        CHECK_EQ(guid[15], '4');
        CHECK(std::string("89AB").find(guid[20]) != std::string::npos);
        CHECK(guids.insert(guid).second);
        CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = " + uid), 3 + i);
    }
}

// Port of ...::test_a_null_binary_value_is_copied_as_null
TEST(ConditionDuplicate, ANullNotesValueIsCopiedAsNull)
{
    TempDb db("dup_null_notes");
    REQUIRE(db.schema_ok());
    seed_duplicate_world(db);

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "11"), "12");
    CHECK_EQ(db.text("SELECT typeof(Notes) FROM BidConditions WHERE UID = 12"), "null");
}

// A blob in a column is copied as the same blob (the copy is a verbatim copy, not a text round trip).
TEST(ConditionDuplicate, ABinaryNotesValueIsCopiedByteForByte)
{
    TempDb db("dup_blob_notes");
    REQUIRE(db.schema_ok());
    seed_duplicate_world(db);
    db.must("UPDATE BidConditions SET Notes = X'41004200FF' WHERE UID = 11");

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "11"), "12");
    CHECK_EQ(db.text("SELECT typeof(Notes) || ':' || hex(Notes) FROM BidConditions WHERE UID = 12"), "blob:41004200FF");
}

// Port of ...::test_unknown_or_foreign_conditions_block_the_duplicate
TEST(ConditionDuplicate, UnknownOrForeignConditionsBlockTheDuplicate)
{
    TempDb db("dup_foreign");
    REQUIRE(db.schema_ok());
    seed_duplicate_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (20, 2, 1, 'f', 0)");
    const std::string before = db.dump("BidConditions");

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "99"), ""); // unknown
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "20"), ""); // a condition of bid 2
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "2", "10"), ""); // named under the wrong bid
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "abc"), "");
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "x", "10"), "");
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", ""), "");
    CHECK_EQ(db.dump("BidConditions"), before);
}

// Windows hands the condition uids of duplicate_conditions on as TEXT to require_valid_unique_bid_owned_uids, which accepts
// ASCII decimal digits only; the bid is turned into an integer first (int(bid_uid)), so blanks and a sign pass there.
// Computed by running the Windows code (python -B, RealContractOps on the Windows duplicate fixture with condition 10):
//   condition ' 10' -> [], '10 ' -> [], '+10' -> [], '010' -> ['11'];  bid ' 1' -> ['11'], '+1' -> ['11'].
TEST(ConditionDuplicate, AConditionUidMustBePlainDigitsButTheBidUidMayCarryBlanksOrASign)
{
    TempDb db("dup_uid_text");
    REQUIRE(db.schema_ok());
    seed_duplicate_world(db);
    const std::string before = db.dump("BidConditions");

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", " 10"), "");
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10 "), "");
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "+10"), "");
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "1 0"), "");
    CHECK_EQ(db.dump("BidConditions"), before);

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "010"), "12");
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), " 1", "10"), "13");
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "+1", "10"), "14");
}

// Port of ConditionOperationsPersistenceTests.test_condition_duplicate_rejects_cross_bid_batch_before_insert (the same
// rule for a single condition): a bid that has no Bids row cannot own a duplicate.
TEST(ConditionDuplicate, ABidWithoutARowBlocksTheDuplicate)
{
    TempDb db("dup_orphan_bid", false);
    db.must("CREATE TABLE Bids (UID INTEGER)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, GUID TEXT, RefNo INTEGER, Name TEXT)");
    db.must("INSERT INTO BidConditions VALUES (7, 99, 'g', 1, 'Orphan')");

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "99", "7"), "");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 1);
}

// Port of ...::test_a_missing_optional_column_is_reported_under_the_duplicate_label: a schema without GUID and RefNo
// still duplicates (those two are replaced only when the columns exist).
TEST(ConditionDuplicate, ASchemaWithoutGuidAndRefNoStillDuplicates)
{
    TempDb db("dup_minimal_schema", false);
    db.must("CREATE TABLE Bids (UID INTEGER)");
    db.must("INSERT INTO Bids VALUES (1)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, Name TEXT)");
    db.must("INSERT INTO BidConditions VALUES (10, 1, 'Wall')");

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "11");
    CHECK_EQ(db.list("SELECT UID || ':' || BidUID || ':' || Name FROM BidConditions ORDER BY UID"),
             "10:1:Wall,11:1:Wall");
}

// Port of ...::test_the_duplicate_needs_the_identity_columns
TEST(ConditionDuplicate, TheDuplicateNeedsTheIdentityColumns)
{
    for (const char *kept : {"BidUID", "UID"})
    {
        SCOPED_CONTEXT(std::string("only column ") + kept);
        TempDb db(std::string("dup_identity_column_") + kept, false);
        db.must(std::string("CREATE TABLE BidConditions (") + kept + " INTEGER)");
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "");
    }
}

// The copy is one transaction: a failing INSERT leaves nothing behind (and reports ""), and the next call works.
TEST(ConditionDuplicate, AFailedInsertLeavesNothingBehind)
{
    TempDb db("dup_failure");
    REQUIRE(db.schema_ok());
    seed_duplicate_world(db);
    const std::string trigger = db.block("BidConditions", "INSERT");
    const std::string before = db.dump("BidConditions");

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "");
    CHECK_EQ(db.dump("BidConditions"), before);

    db.unblock(trigger);
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "12");
}
