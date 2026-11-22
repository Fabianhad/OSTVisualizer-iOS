// Independent review additions for S0 (plan 4.2): cases the first UID tests let a lazy implementation survive, found by mutation thinking.
// Additions only (moved out of test_bid_lock_review.cpp so that the UID limit helper and the bid lock land in separate commits). The UID
// cases are limit arithmetic (no Windows oracle: the limit is a deliberate safe deviation, plan 4.2).
//
// Survivors this file kills:
//   - the optional no_identity_left flag of next_uid_within_access_limit: set on every refusal, never reset, or never set;
//   - the no-identity text set for every refusal of the helper instead of the limit refusal only;
//   - duplicate_condition when the number guard and the UID limit are both exhausted (Windows allocates the UIDs first and then raises
//     the NUMBER error, condition_operations.py:42-62: the number text wins).
#include "StoreTestSupport.hpp"

#include "Domain/ConditionFormRules.hpp"
#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore_Integrity.hpp"

#include <climits>
#include <optional>
#include <string>

using ost_ptest::TempDb;

// ---------------------------------------------------------------------------------------------------------------
// the UID limit helper: the optional `no_identity_left` flag and the shared text
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    struct Flagged
    {
        bool ok = true;
        long long first = 777;
        bool no_identity_left = true; // pre-set to the value the helper must always overwrite
    };

    Flagged allocate_flagged(const TempDb &db, const char *table, long long count, bool preset)
    {
        Flagged out;
        out.no_identity_left = preset;
        ost_db::OstDatabase connection;
        if (!connection.open(db.path()) || !connection.begin_transaction())
        {
            out.ok = false;
            return out;
        }
        out.ok = ost_store::integrity::next_uid_within_access_limit(connection, table, count, out.first, &out.no_identity_left);
        connection.rollback();
        return out;
    }

    void seed_condition_uid(const TempDb &db, long long highest)
    {
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
        db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (10, 1, 'g10', 1, 'Ten', 0)");
        db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (" + std::to_string(highest) +
                ", 1, 'gh', 2, 'High', 0)");
    }
} // namespace

// plan 4.2 / ProjectStore_Integrity.hpp: `no_identity_left` is true ONLY when the refusal is the limit; a success, a count below one and
// a failed statement leave it false (the caller shows its generic text then), and it is always written, whatever it held before.
TEST(UidLimitReview, TheNoIdentityFlagIsTrueOnlyForTheLimitRefusal)
{
    TempDb db("limit_rev_flag");
    REQUIRE(db.schema_ok());
    seed_condition_uid(db, 2147483646LL);
    for (const bool preset : {false, true})
    {
        SCOPED_CONTEXT(preset ? "flag pre-set to true" : "flag pre-set to false");
        const Flagged allowed = allocate_flagged(db, "BidConditions", 1, preset);
        CHECK(allowed.ok);
        CHECK_EQ(allowed.first, 2147483647LL);
        CHECK_FALSE(allowed.no_identity_left);

        const Flagged past = allocate_flagged(db, "BidConditions", 2, preset);
        CHECK_FALSE(past.ok);
        CHECK_EQ(past.first, 0LL);
        CHECK(past.no_identity_left);

        const Flagged huge = allocate_flagged(db, "BidConditions", LLONG_MAX, preset);
        CHECK_FALSE(huge.ok);
        CHECK(huge.no_identity_left); // a count no range can hold is the limit too

        for (const long long count : {0LL, -1LL, LLONG_MIN})
        {
            SCOPED_CONTEXT("count " + std::to_string(count));
            const Flagged bad = allocate_flagged(db, "BidConditions", count, preset);
            CHECK_FALSE(bad.ok);
            CHECK_EQ(bad.first, 0LL);
            CHECK_FALSE(bad.no_identity_left);
        }

        const Flagged missing = allocate_flagged(db, "NoSuchTable", 1, preset);
        CHECK_FALSE(missing.ok);
        CHECK_EQ(missing.first, 0LL);
        CHECK_FALSE(missing.no_identity_left);
    }
}

// The refusal text every writer shows (plan 4.2 / 4.14): "Cannot add a <noun>: no identity is left below 2147483647."
TEST(UidLimitReview, TheNoIdentityTextNamesTheNounAndTheLimit)
{
    CHECK_STR_EQ(ost_store::integrity::no_identity_left_text("condition"), "Cannot add a condition: no identity is left below 2147483647.");
    CHECK_STR_EQ(ost_store::integrity::no_identity_left_text("folder"), "Cannot add a folder: no identity is left below 2147483647.");
    CHECK_STR_EQ(ost_store::integrity::no_identity_left_text("layer"), "Cannot add a layer: no identity is left below 2147483647.");
    CHECK_EQ(ost_store::integrity::kAccessLongMax, 2147483647LL);
}

// Windows _allocate_condition_identities (condition_operations.py:42-62) allocates the UIDs first and raises the NUMBER error after it:
// a copy that has neither a free number nor a free identity shows the Windows number text (the job 6 guard stays as it is), not the new
// identity text, and writes nothing.
TEST(UidLimitReview, ACopyWithNoNumberAndNoIdentityShowsTheNumberText)
{
    TempDb db("limit_rev_both");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
    db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (10, 1, 'g10', 1, 'Ten', 0)");
    db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (2147483647, 1, 'gh', 2147483647, 'High', 0)");
    const std::string before = db.snapshot_all();
    const std::optional<std::string> number_text = ost_domain::refuse_new_condition_numbers(2147483647LL, 1);
    REQUIRE(number_text.has_value());

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, *number_text);
    CHECK_EQ(db.snapshot_all(), before);

    // and the identity text appears as soon as only the identity is exhausted (the number has room: the high row's number is lowered)
    db.must("UPDATE BidConditions SET RefNo = 5 WHERE UID = 2147483647");
    const std::string number_free = db.snapshot_all();
    error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, "Cannot add a condition: no identity is left below 2147483647.");
    CHECK_EQ(db.snapshot_all(), number_free);
}

// ProjectStore.hpp (duplicate_condition) and ProjectStore_Integrity.hpp (next_uid_within_access_limit): the "no identity is left below
// 2147483647" text is for the LIMIT refusal only; "every other failure leaves `error` empty (the caller shows the generic text)". A highest
// UID of 2^63 - 1 leaves nothing above it, which next_uid_preserving_references reports like a failed statement: the flag stays false and
// the copy fails with an empty `error`, on a row of ANOTHER bid (the ownership check and the number guard pass). A store that sets the text
// for every refusal of the helper survives the other tests, which only ask the helper for the flag.
TEST(UidLimitReview, AnUnusableMaxUidIsTheGenericFailureNotTheNoIdentityText)
{
    TempDb db("limit_rev_generic");
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (1, 1, 'Bid', 1)");
    db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo) VALUES (2, 1, 'Other', 2)");
    db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (10, 1, 'g10', 1, 'Ten', 0)");
    db.must("INSERT INTO BidConditions (UID, BidUID, GUID, RefNo, Name, Type) VALUES (9223372036854775807, 2, 'gm', 1, 'Max', 0)");
    const std::string before = db.snapshot_all();

    for (const bool preset : {false, true})
    {
        const Flagged unusable = allocate_flagged(db, "BidConditions", 1, preset);
        CHECK_FALSE(unusable.ok);
        CHECK_EQ(unusable.first, 0LL);
        CHECK_FALSE(unusable.no_identity_left);
    }

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::duplicate_condition(db.path(), "1", "10", &error), "");
    CHECK_STR_EQ(error, "");
    CHECK_EQ(db.snapshot_all(), before);
}
