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

// ---------------------------------------------------------------------------------------------------------------------
// COND-ALLOC-01 (job 6): the copy may not get a number past 2147483647. Windows 49a2d59,
// condition_operations.py _allocate_condition_identities :48-62 (ConditionNumberOverflowError), called by duplicate_conditions :109-113
// AFTER the ownership check (:103-105) and BEFORE any insert. The strings and the numbers are the output of that function run in memory
// with a stub cursor (the f-string of the refusal is condition_operations.py :57-62), the structure of the fixture is the Windows ConditionNumberAllocationLimitTests
// (test_condition_operations.py :1344: bid 1 = rows 10 and 11, bid 2 = row 20). iOS duplicates ONE condition per call (the only allocator
// of condition numbers; there is no New Condition, no paste, no copy to another bid, no renumber): Windows' "two duplicates at
// 2147483646 fail" is pinned in Core (test_condition_form.cpp, refuse_new_condition_numbers(2147483646, 2)) and here as a second duplicate.
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
    constexpr const char *kAddOneText =
        "Cannot add 1 condition: the new condition numbers would reach 2147483648, past 2147483647, the highest condition number.";

    // seed_duplicate_world (bid 1: 10 -> RefNo 1, 11 -> RefNo 2; bid 2 exists) with condition 11 at `highest` and a condition 20 of
    // bid 2 at `highest` too. The copy of a condition of bid 1 gets the UID 21 (MAX(UID) 20 + 1).
    void seed_limit_world(const TempDb &db, long long highest)
    {
        seed_duplicate_world(db);
        db.must("UPDATE BidConditions SET RefNo = " + std::to_string(highest) + " WHERE UID = 11");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (20, 2, " + std::to_string(highest) +
                ", 'other', 0)");
    }

    std::string above_limit_text(const std::string &last_ref_no)
    {
        return "Cannot add 1 condition: the new condition numbers would reach " + last_ref_no +
               ", past 2147483647, the highest condition number.";
    }
}

// Windows test_the_last_free_number_can_still_be_used: the boundary is exclusive (2147483647 itself is a number that can be stored).
TEST(CondAlloc01_DuplicateLimit, TheLastFreeNumberCanStillBeUsed)
{
    TempDb db("alloc_last_free");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483646LL);

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "21");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 21"), 2147483647LL);
    CHECK_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 21"), "integer");
    CHECK_EQ(db.count("SELECT MAX(RefNo) FROM BidConditions WHERE BidUID = 1"), 2147483647LL);
    // nothing else moved: the other numbers are as they were seeded
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 10"), 1);
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 11"), 2147483646LL);
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 20"), 2147483646LL);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 4);
}

// Windows test_adding_past_the_highest_number_raises_and_writes_nothing ("duplicate"): one duplicate at 2147483647 would be 2147483648
// (iOS read it through column_int and stored -2147483648 in the copy, reporting success). Every table is compared before and after.
TEST(CondAlloc01_DuplicateLimit, ADuplicateAtTheHighestNumberIsRefusedAndWritesNothing)
{
    TempDb db("alloc_refused");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483647LL);
    const std::string before = db.snapshot_all();

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, kAddOneText); // X = 2147483647 + 1 = 2147483648
    CHECK_EQ(db.snapshot_all(), before); // no row, no number, no table changed
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 3);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID > 20"), 0); // no new UID
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE RefNo < 0"), 0); // the old wrap value -2147483648 never appears

    // the source does not matter (the highest number is the bid's), and a call without the error channel refuses the same way
    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "11", &error), "");
    CHECK_STR_EQ(error, kAddOneText);
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "");
    CHECK_EQ(db.snapshot_all(), before);
}

// The refusal reads and writes nothing that persists: no UID is consumed, so once a number is free the next copy gets the UID the refused
// one would have had.
TEST(CondAlloc01_DuplicateLimit, ARefusalConsumesNoUidAndTheNextDuplicateGetsTheSameOne)
{
    TempDb db("alloc_no_uid");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483647LL);

    std::string error;
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, kAddOneText);

    db.must("UPDATE BidConditions SET RefNo = 5 WHERE UID = 11");
    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "21");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 21"), 6);
}

// Sequential stand-in for "two duplicates at 2147483646 fail" (iOS copies one condition per request): the first copy takes the last free
// number, the next ones are refused (X = 2147483648) and leave the database as the first copy left it.
TEST(CondAlloc01_DuplicateLimit, TheDuplicateAfterTheLastFreeNumberIsRefused)
{
    TempDb db("alloc_two");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483646LL);

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "21");
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 21"), 2147483647LL);
    const std::string after_first = db.snapshot_all();

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, kAddOneText);
    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "21", &error), ""); // the copy itself holds the highest number
    CHECK_STR_EQ(error, kAddOneText);
    CHECK_EQ(db.snapshot_all(), after_first);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 4);
}

// Windows `WHERE [BidUID] = ?` (:51): only the numbers of the condition's own bid count, in both directions.
TEST(CondAlloc01_DuplicateLimit, OnlyTheNumbersOfTheOwnBidCount)
{
    TempDb db("alloc_own_bid");
    REQUIRE(db.schema_ok());
    seed_duplicate_world(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (20, 2, 2147483647, 'other', 0)");

    // bid 2 sits at the limit, bid 1 does not: its copy is numbered after bid 1's own highest number (2)
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "21");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 21"), 3);

    // a condition of bid 2 itself is refused, and nothing changes
    const std::string snapshot = db.snapshot_all();
    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "2", "20", &error), "");
    CHECK_STR_EQ(error, kAddOneText);
    CHECK_EQ(db.snapshot_all(), snapshot);
}

// MAX ignores NULL: a bid whose only numbers are NULL starts at 1 (Windows None -> 1, oracle), and a NULL next to a real number is no
// number. The source keeps its NULL. Neither case refuses.
TEST(CondAlloc01_DuplicateLimit, NullNumbersAreIgnoredAndABidWithoutNumbersStartsAtOne)
{
    {
        SCOPED_CONTEXT("only NULL numbers");
        TempDb db("alloc_null_only");
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name) VALUES (7, 1, NULL, 'Source')");

        std::string error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "7", &error), "8");
        CHECK_STR_EQ(error, "");
        CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 8"), 1);
        CHECK_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 8"), "integer");
        CHECK_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 7"), "null");
    }
    {
        SCOPED_CONTEXT("a NULL next to a number");
        TempDb db("alloc_null_mixed");
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name) VALUES (7, 1, NULL, 'Null')");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name) VALUES (8, 1, 5, 'Five')");

        std::string error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "7", &error), "9");
        CHECK_STR_EQ(error, "");
        CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 9"), 6);
        CHECK_EQ(db.text("SELECT typeof(RefNo) FROM BidConditions WHERE UID = 7"), "null");
    }
    {
        SCOPED_CONTEXT("a highest number of 0");
        TempDb db("alloc_zero");
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
        db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name) VALUES (7, 1, 0, 'Zero')");

        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "7"), "8");
        CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 8"), 1); // highest 0 + 1, the same as a bid without numbers
    }
}

// SQLite can hold a number an Access Long cannot (an older iOS build wrote 2147483648 through the update shift): the highest number is
// read as 64 bit and a number past the limit blocks as well, with X = highest + 1 printed exactly (Windows ints do not wrap).
TEST(CondAlloc01_DuplicateLimit, ANumberAlreadyAboveTheLimitBlocksToo)
{
    struct Case
    {
        long long highest;
        const char *last_ref_no;
    };
    const Case cases[] = {
        {2147483648LL, "2147483649"},
        {3000000000LL, "3000000001"},
        {9223372036854775807LL, "9223372036854775808"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("highest ") + std::to_string(c.highest));
        TempDb db("alloc_above_" + std::to_string(c.highest));
        REQUIRE(db.schema_ok());
        seed_limit_world(db, c.highest);
        const std::string before = db.snapshot_all();

        std::string error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
        CHECK_STR_EQ(error, above_limit_text(c.last_ref_no));
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// The ownership check comes BEFORE the allocation (Windows :103-105 vs :109): an unknown or foreign condition at a full bid is the generic
// failure (empty error, the generic alert text), never the overflow text. A failing INSERT is generic too and rolls the read back.
TEST(CondAlloc01_DuplicateLimit, AnUnknownOrForeignConditionAtTheLimitIsTheGenericFailure)
{
    {
        TempDb db("alloc_generic");
        REQUIRE(db.schema_ok());
        seed_limit_world(db, 2147483647LL);
        const std::string before = db.snapshot_all();

        std::string error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "99", &error), ""); // unknown
        CHECK_STR_EQ(error, "");
        error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "20", &error), ""); // a condition of bid 2
        CHECK_STR_EQ(error, "");
        error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "2", "10", &error), ""); // named under the wrong bid, which is full too
        CHECK_STR_EQ(error, "");
        error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "abc", &error), ""); // not a uid
        CHECK_STR_EQ(error, "");
        CHECK_EQ(db.snapshot_all(), before);
    }
    {
        SCOPED_CONTEXT("a failing INSERT below the limit");
        TempDb db("alloc_generic_insert");
        REQUIRE(db.schema_ok());
        seed_limit_world(db, 2147483646LL);
        const std::string trigger = db.block("BidConditions", "INSERT");
        const std::string before = db.snapshot_all();

        std::string error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
        CHECK_STR_EQ(error, ""); // the generic text, not a refusal
        CHECK_EQ(db.snapshot_all(), before);

        db.unblock(trigger);
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "21");
        CHECK_STR_EQ(error, "");
    }
    {
        // AT the limit the refusal comes BEFORE any write (Windows :56-62 precede the INSERT): with an INSERT that would fail, the caller
        // still gets the overflow text, not the generic failure. A refusal raised only after the INSERT has run (and rolled back) would
        // give the empty error here; found by the independent mutation re-check (mutant: guard after the INSERT).
        SCOPED_CONTEXT("a failing INSERT at the limit");
        TempDb db("alloc_before_insert");
        REQUIRE(db.schema_ok());
        seed_limit_world(db, 2147483647LL);
        const std::string trigger = db.block("BidConditions", "INSERT");
        const std::string before = db.snapshot_all();

        std::string error = "stale";
        CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
        CHECK_STR_EQ(error, kAddOneText); // X = 2147483647 + 1 = 2147483648
        CHECK_EQ(db.snapshot_all(), before);
        db.unblock(trigger);
    }
}

// A schema without the number column has no number to run out of (Windows :48-49, :55: max_ref stays None): no check, no error.
TEST(CondAlloc01_DuplicateLimit, ASchemaWithoutTheNumberColumnNeverHitsTheLimit)
{
    TempDb db("alloc_no_column", false);
    db.must("CREATE TABLE Bids (UID INTEGER)");
    db.must("INSERT INTO Bids VALUES (1)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, Name TEXT)");
    db.must("INSERT INTO BidConditions VALUES (10, 1, 'Wall')");

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "11");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.list("SELECT UID || ':' || BidUID || ':' || Name FROM BidConditions ORDER BY UID"), "10:1:Wall,11:1:Wall");
}

// The error channel is optional and always reset: a success and a generic failure clear it, a refusal sets it, and a call without it
// works in all three cases.
TEST(CondAlloc01_DuplicateLimit, TheErrorChannelIsClearedAndOptional)
{
    TempDb db("alloc_channel");
    REQUIRE(db.schema_ok());
    seed_limit_world(db, 2147483645LL);

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "21"); // takes 2147483646
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 21"), 2147483646LL);

    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "99", &error), ""); // generic failure
    CHECK_STR_EQ(error, "");

    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), "22"); // without the channel: takes 2147483647
    CHECK_EQ(db.count("SELECT RefNo FROM BidConditions WHERE UID = 22"), 2147483647LL);

    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), ""); // refusal
    CHECK_STR_EQ(error, kAddOneText);
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10"), ""); // refusal without the channel
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 5);
}
