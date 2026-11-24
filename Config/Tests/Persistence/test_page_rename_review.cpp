// Rename Page, tests added by the INDEPENDENT REVIEWER (they only add; nothing of test_page_rename_rules / _store / _memory.cpp is changed).
// Each test kills a mutant that the existing tests survive. The Windows answers that matter are re-derived in memory with python -B -I
// (str.strip() / str.__eq__ on the same texts); nothing is written into the Windows tree.
//
//   1. strip works on CODE POINTS: names whose last byte is 0x85 or 0xA0 (the last byte of U+0105, U+00E0, U+3005, U+1085 ...) are not
//      touched, while the real NBSP / NEL around them are removed (Python: 'Pią'.strip() == 'Pią', '\xa0ą　'.strip()
//      == 'ą'). A byte-wise strip of 0x85 / 0xA0 would cut a letter in two and store text that is not valid UTF-8.
//   2. the compare of the unchanged shortcut is an exact code point compare (Python ==): 'e' + U+0301 is not U+00E9.
//   3. the lock and the ownership checks run INSIDE the transaction: a second connection holds the write lock, changes the lock status /
//      the owner of the page / deletes the page and commits while rename_page waits at BEGIN IMMEDIATE; the rename must see the COMMITTED
//      state. A function that reads the lock or the owner before it has the write lock (or through a second connection) would write.
//   4. the order of the refusals of plan 4.4 item 2: ownership first, then the lock (observable through the reason text).
//   5. exactly one row: only the rows changed by the UPDATE itself count, not the rows a trigger changes (sqlite3_changes, not
//      sqlite3_total_changes).
//   6. RP-1 at the store: a stored name with surrounding spaces is rewritten stripped, and a case-only change is written.
#include "StoreTestSupport.hpp"

#include "Domain/PageRules.hpp"
#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore_PageRename.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

using ost_domain::PageRenameDecision;
using ost_ptest::TempDb;

namespace
{
    const char *decision_name(PageRenameDecision decision)
    {
        switch (decision)
        {
        case PageRenameDecision::Refused:
            return "Refused";
        case PageRenameDecision::TooLong:
            return "TooLong";
        case PageRenameDecision::Invalid:
            return "Invalid";
        case PageRenameDecision::Unchanged:
            return "Unchanged";
        case PageRenameDecision::Write:
            return "Write";
        }
        return "<not a decision>";
    }

    void expect_plan(const std::string &entered, const std::string &stored, PageRenameDecision decision, const std::string &new_name)
    {
        const ost_domain::PageRenamePlan plan = ost_domain::decide_page_rename(entered, stored);
        CHECK_EQ(std::string(decision_name(plan.decision)) + ":" + plan.new_name,
                 std::string(decision_name(decision)) + ":" + new_name);
    }

    std::string lower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    std::string page_name(const TempDb &db, const std::string &uid)
    {
        return db.text("SELECT Name FROM BidPages WHERE UID = " + uid);
    }

    // Runs rename_page on a second thread while `other` (an open connection of the caller that holds BEGIN IMMEDIATE and has already
    // made its changes) keeps the write lock for ~300 ms, then commits `other`. rename_page waits at BEGIN IMMEDIATE (busy timeout 5 s)
    // and must read the lock, the owner and the page AFTER it has the lock, i.e. from the state `other` committed. Returns the result of
    // rename_page; `error` is its reason. The check macros are only used on the calling thread, after the join.
    bool rename_while_the_database_changes(const TempDb &db, ost_db::OstDatabase &other, const std::string &bid, const std::string &page,
                                           const std::string &name, std::string &error)
    {
        bool result = true;
        std::thread renamer([&]() { result = ost_store::rename_page(db.path(), bid, page, name, &error); });
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        const bool committed = other.commit();
        renamer.join();
        CHECK(committed);
        return result;
    }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// 1. strip works on code points
// ---------------------------------------------------------------------------------------------------------------

// Python: 'Pią'.strip(), 'Caf\xe0'.strip(), 'ą'.strip(), '\xe0'.strip(), '々'.strip(), 'ႅ'.strip(), 'ƅ'.strip() and
// '\xc0'.strip() are unchanged and not blank; '\xa0Caf\xe0\xa0'.strip() == 'Caf\xe0'; '\xa0ą　'.strip() == 'ą';
// '\x85\xe0\x85'.strip() == '\xe0'. The bytes: U+0105 = C4 85, U+00E0 = C3 A0, U+3005 = E3 80 85, U+1085 = E1 82 85, U+0185 = C6 85.
TEST(PageRenameReview, Strip_NeverCutsACharacterThatEndsInABytePythonCallsWhitespace)
{
    const std::vector<std::string> kept = {
        "Pi\xC4\x85",          // U+0105: last byte 0x85
        "Caf\xC3\xA0",         // U+00E0: last byte 0xA0
        "\xC4\x85",            // only U+0105: not blank
        "\xC3\xA0",            // only U+00E0: not blank
        "\xE3\x80\x85",        // U+3005 IDEOGRAPHIC ITERATION MARK (not whitespace)
        "\xE1\x82\x85",        // U+1085
        "\xC6\x85",            // U+0185
        "\xC3\x80",            // U+00C0
        "A\xC3\xA0" "B",       // inside
    };
    for (std::size_t i = 0; i < kept.size(); ++i)
    {
        SCOPED_CONTEXT("kept #" + std::to_string(i) + " " + kept[i]);
        CHECK_EQ(ost_domain::normalize_page_name(kept[i]), kept[i]);
        expect_plan(kept[i], "Old", PageRenameDecision::Write, kept[i]);
        expect_plan(kept[i], kept[i], PageRenameDecision::Unchanged, kept[i]);
    }
    // the real whitespace around them goes
    expect_plan("\xC2\xA0" "Caf\xC3\xA0" "\xC2\xA0", "Old", PageRenameDecision::Write, "Caf\xC3\xA0");
    expect_plan("\xC2\xA0\xC4\x85\xE3\x80\x80", "Old", PageRenameDecision::Write, "\xC4\x85");
    expect_plan("\xC2\x85\xC3\xA0\xC2\x85", "Old", PageRenameDecision::Write, "\xC3\xA0");
    expect_plan("\xE3\x80\x85 ", "\xE3\x80\x85", PageRenameDecision::Unchanged, "\xE3\x80\x85");
}

// The same names through the store, byte for byte (a name whose letter was cut in two would be refused as invalid UTF-8 or stored wrong).
TEST(PageRenameReview, Store_NamesThatEndInAContinuationByteAreStoredExactly)
{
    TempDb db("rename_review_continuation");
    ost_ptest::seed_two_bids(db);
    const std::vector<std::string> names = {"Pi\xC4\x85", "Caf\xC3\xA0", "\xC3\xA0", "\xE3\x80\x85", "\xE1\x82\x85", "\xC6\x85"};
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        SCOPED_CONTEXT("name #" + std::to_string(i));
        const ost_domain::PageRenamePlan plan = ost_domain::decide_page_rename(names[i], "P100");
        CHECK_EQ(std::string(decision_name(plan.decision)), std::string("Write"));
        std::string error;
        CHECK(ost_store::rename_page(db.path(), "10", "100", plan.new_name, &error));
        CHECK_EQ(page_name(db, "100"), names[i]);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// 2. the unchanged compare is exact
// ---------------------------------------------------------------------------------------------------------------

// Python: 'é' == '\xe9' is False, and 'A' == 'A​' is False: no Unicode normalisation and no invisible-character folding in the
// compare (an NSString isEqualToString: would fold canonical equivalents, this rule must not).
TEST(PageRenameReview, Unchanged_TheCompareIsExactNoNormalisationNoFolding)
{
    expect_plan("e\xCC\x81", "\xC3\xA9", PageRenameDecision::Write, "e\xCC\x81");
    expect_plan("\xC3\xA9", "e\xCC\x81", PageRenameDecision::Write, "\xC3\xA9");
    expect_plan("A", "A\xE2\x80\x8B", PageRenameDecision::Write, "A");
    expect_plan("A\xE2\x80\x8B", "A", PageRenameDecision::Write, "A\xE2\x80\x8B");
    expect_plan("\xC3\xA9", "\xC3\xA9", PageRenameDecision::Unchanged, "\xC3\xA9"); // control
}

// ---------------------------------------------------------------------------------------------------------------
// 3. the lock and the ownership are read INSIDE the transaction
// ---------------------------------------------------------------------------------------------------------------

// The bid is open when rename_page starts, and a committed change makes it Locked while rename_page waits for the write lock: refused.
TEST(PageRenameReview, InsideTheTransaction_ALockCommittedWhileWaitingIsSeen)
{
    TempDb db("rename_review_lock_seen");
    ost_ptest::seed_two_bids(db);
    ost_ptest::seed_open_bid(db, 10, 4);
    ost_ptest::seed_job_status(db, 5, "-1", "Locked");
    ost_db::OstDatabase other;
    REQUIRE(other.open(db.path()));
    REQUIRE(other.begin_transaction());
    REQUIRE(other.exec("UPDATE Bids SET JobStatusUID = 5 WHERE UID = 10"));
    std::string error = "stale";
    const bool renamed = rename_while_the_database_changes(db, other, "10", "100", "Renamed", error);
    CHECK_FALSE(renamed);
    CHECK_EQ(page_name(db, "100"), std::string("P100"));
    CHECK(lower(error).find("locked") != std::string::npos);
    CHECK_EQ(db.text("SELECT JobStatusUID FROM Bids WHERE UID = 10"), std::string("5")); // only `other`'s change happened
}

// The bid is Locked when rename_page starts and a committed change unlocks it while it waits: the rename sees the unlocked state and writes
// (so the function does not simply refuse whatever it saw first).
TEST(PageRenameReview, InsideTheTransaction_AnUnlockCommittedWhileWaitingIsSeen)
{
    TempDb db("rename_review_unlock_seen");
    ost_ptest::seed_two_bids(db);
    ost_ptest::seed_locked_bid(db, 10, 5);
    ost_db::OstDatabase other;
    REQUIRE(other.open(db.path()));
    REQUIRE(other.begin_transaction());
    REQUIRE(other.exec("UPDATE Bids SET JobStatusUID = NULL WHERE UID = 10"));
    std::string error = "stale";
    const bool renamed = rename_while_the_database_changes(db, other, "10", "100", "Renamed", error);
    CHECK(renamed);
    CHECK_EQ(error, std::string());
    CHECK_EQ(page_name(db, "100"), std::string("Renamed"));
    CHECK_EQ(page_name(db, "101"), std::string("P101"));
}

// The page changes owner while rename_page waits: the ownership is read after the lock, so a page that now belongs to another bid is refused
// and keeps its name (and its new owner).
TEST(PageRenameReview, InsideTheTransaction_AnOwnerChangeCommittedWhileWaitingIsSeen)
{
    TempDb db("rename_review_owner_seen");
    ost_ptest::seed_two_bids(db);
    ost_db::OstDatabase other;
    REQUIRE(other.open(db.path()));
    REQUIRE(other.begin_transaction());
    REQUIRE(other.exec("UPDATE BidPages SET BidUID = 20 WHERE UID = 100"));
    std::string error = "stale";
    const bool renamed = rename_while_the_database_changes(db, other, "10", "100", "Renamed", error);
    CHECK_FALSE(renamed);
    CHECK(!error.empty());
    CHECK_EQ(page_name(db, "100"), std::string("P100"));
    CHECK_EQ(db.count("SELECT BidUID FROM BidPages WHERE UID = 100"), 20);
}

// The page is deleted while rename_page waits (the Windows race of RP-8, here through the ownership read): refused, nothing created.
TEST(PageRenameReview, InsideTheTransaction_ADeleteCommittedWhileWaitingIsSeen)
{
    TempDb db("rename_review_delete_seen");
    ost_ptest::seed_two_bids(db);
    ost_db::OstDatabase other;
    REQUIRE(other.open(db.path()));
    REQUIRE(other.begin_transaction());
    REQUIRE(other.exec("DELETE FROM BidPages WHERE UID = 100"));
    std::string error = "stale";
    const bool renamed = rename_while_the_database_changes(db, other, "10", "100", "Renamed", error);
    CHECK_FALSE(renamed);
    CHECK(!error.empty());
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages WHERE UID = 100"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 2);
    CHECK_EQ(page_name(db, "101"), std::string("P101"));
}

// Control of the helper itself: nothing changes while rename_page waits, so it renames (a helper that always made the call fail would hide
// a broken wait).
TEST(PageRenameReview, InsideTheTransaction_ControlNothingChangedWhileWaiting)
{
    TempDb db("rename_review_wait_control");
    ost_ptest::seed_two_bids(db);
    ost_db::OstDatabase other;
    REQUIRE(other.open(db.path()));
    REQUIRE(other.begin_transaction());
    REQUIRE(other.exec("UPDATE BidPages SET Sequence = 9 WHERE UID = 101"));
    std::string error = "stale";
    CHECK(rename_while_the_database_changes(db, other, "10", "100", "Renamed", error));
    CHECK_EQ(error, std::string());
    CHECK_EQ(page_name(db, "100"), std::string("Renamed"));
    CHECK_EQ(db.count("SELECT Sequence FROM BidPages WHERE UID = 101"), 9);
}

// ---------------------------------------------------------------------------------------------------------------
// 4. the order of the refusals: ownership first, then the lock
// ---------------------------------------------------------------------------------------------------------------

// Plan 4.4 item 2: "rows belong to the OPEN bid" comes BEFORE "bid lock". On a locked bid a page that is not the bid's is refused as a
// page of another bid (its reason does not say "locked": the lock of a bid says nothing about another bid's page), while a page of the bid
// is refused with the lock reason.
TEST(PageRenameReview, Order_OwnershipIsCheckedBeforeTheLock)
{
    TempDb db("rename_review_order");
    ost_ptest::seed_two_bids(db);
    ost_ptest::seed_locked_bid(db, 10, 5);
    std::string error;
    CHECK_FALSE(ost_store::rename_page(db.path(), "10", "200", "Renamed", &error)); // page of bid 20
    CHECK(!error.empty());
    CHECK(lower(error).find("locked") == std::string::npos);
    CHECK_FALSE(ost_store::rename_page(db.path(), "10", "99", "Renamed", &error)); // no such page
    CHECK(lower(error).find("locked") == std::string::npos);
    CHECK_FALSE(ost_store::rename_page(db.path(), "10", "100", "Renamed", &error)); // the bid's own page
    CHECK(lower(error).find("locked") != std::string::npos);
    // the lock is read before the name rule: a bad name on a locked bid is reported as the lock
    CHECK_FALSE(ost_store::rename_page(db.path(), "10", "100", "", &error));
    CHECK(lower(error).find("locked") != std::string::npos);
    CHECK_EQ(page_name(db, "100"), std::string("P100"));
    CHECK_EQ(page_name(db, "200"), std::string("P200"));
}

// ---------------------------------------------------------------------------------------------------------------
// 5. exactly one row: the rows a trigger changes do not count
// ---------------------------------------------------------------------------------------------------------------

// An AFTER UPDATE trigger of the database changes another row (and a third): the UPDATE of the page itself changed exactly one row, so the
// rename succeeds (sqlite3_total_changes() would say 2 or 3 and refuse a valid rename), and the trigger's own change is part of the same
// committed transaction.
TEST(PageRenameReview, ExactlyOneRow_RowsChangedByATriggerDoNotCount)
{
    TempDb db("rename_review_trigger_rows");
    ost_ptest::seed_two_bids(db);
    db.must("CREATE TRIGGER touch_rows AFTER UPDATE ON BidPages BEGIN "
            "UPDATE Bids SET JobName = 'touched' WHERE UID = NEW.BidUID; "
            "UPDATE BidConditions SET Name = 'touched too' WHERE UID = 30; END");
    std::string error = "stale";
    CHECK(ost_store::rename_page(db.path(), "10", "100", "Renamed", &error));
    CHECK_EQ(error, std::string());
    CHECK_EQ(page_name(db, "100"), std::string("Renamed"));
    CHECK_EQ(page_name(db, "101"), std::string("P101"));
    CHECK_EQ(db.text("SELECT JobName FROM Bids WHERE UID = 10"), std::string("touched"));
    CHECK_EQ(db.text("SELECT Name FROM BidConditions WHERE UID = 30"), std::string("touched too"));
    CHECK_EQ(db.text("SELECT JobName FROM Bids WHERE UID = 20"), std::string("Job twenty"));
}

// ---------------------------------------------------------------------------------------------------------------
// 6. RP-1 at the store
// ---------------------------------------------------------------------------------------------------------------

// A stored name with surrounding spaces (the Cover Sheet or an import can leave one): typing 'A' (or '  A  ') is a Write of 'A' (RP-1:
// the stripped text is compared with the UNSTRIPPED stored name), and the store takes it; a case-only change is a write as well. The rule
// and the store agree: what the rule says Write, the store writes.
TEST(PageRenameReview, Rp1_AStoredNameWithSpacesIsRewrittenStrippedAndACaseChangeIsWritten)
{
    TempDb db("rename_review_rp1");
    ost_ptest::seed_two_bids(db);
    db.must("UPDATE BidPages SET Name = '  A101  ' WHERE UID = 100");
    for (const char *typed : {"A101", "  A101  ", "\tA101\n"})
    {
        SCOPED_CONTEXT(std::string("typed '") + typed + "'");
        db.must("UPDATE BidPages SET Name = '  A101  ' WHERE UID = 100");
        const ost_domain::PageRenamePlan plan = ost_domain::decide_page_rename(typed, page_name(db, "100"));
        CHECK_EQ(std::string(decision_name(plan.decision)), std::string("Write"));
        CHECK_EQ(plan.new_name, std::string("A101"));
        CHECK(ost_store::rename_page(db.path(), "10", "100", plan.new_name));
        CHECK_EQ(page_name(db, "100"), std::string("A101"));
    }
    // the stored name is already clean: the same typed text is Unchanged and a case change is a Write
    expect_plan("A101", "A101", PageRenameDecision::Unchanged, "A101");
    const ost_domain::PageRenamePlan lowered = ost_domain::decide_page_rename("a101", page_name(db, "100"));
    CHECK_EQ(std::string(decision_name(lowered.decision)), std::string("Write"));
    CHECK(ost_store::rename_page(db.path(), "10", "100", lowered.new_name));
    CHECK_EQ(page_name(db, "100"), std::string("a101"));
    CHECK_EQ(page_name(db, "101"), std::string("P101"));
}

// ---------------------------------------------------------------------------------------------------------------
// 7. a lock read that FAILS refuses (fail closed); it never falls back to "unlocked"
// ---------------------------------------------------------------------------------------------------------------

// ost_store::integrity::bid_is_locked answers FALSE (the read itself failed: a statement that cannot be prepared) in two ways, and only then
// does the store take the "could not be read" branch (a dropped JobStatuses table or an unknown status is a different path: the status is
// unresolved and the bid reads LOCKED with a successful read). Case 1: the bid has a status and the status table lost its Locked column.
// Case 2: Bids lost its JobStatusUID column (every bid). Both must be refused with the database identical; a store that treated a failed
// read as "not locked" would write. Controls: in case 1 a bid WITHOUT a status never reads the status table and still renames; in case 2
// putting the column back renames.
TEST(PageRenameReview, Lock_AReadThatFailsRefusesAndNeverFallsBackToUnlocked)
{
    {
        TempDb db("rename_review_lock_read_fails_status");
        ost_ptest::seed_two_bids(db);
        ost_ptest::seed_open_bid(db, 10, 4);
        db.must("ALTER TABLE JobStatuses DROP COLUMN Locked");
        const std::string before = db.snapshot_all();
        std::string error = "stale";
        CHECK_FALSE(ost_store::rename_page(db.path(), "10", "100", "Renamed", &error));
        CHECK(error != "stale");
        CHECK(lower(error).find("read") != std::string::npos); // the failed-read branch, not the unresolved-status one
        CHECK_EQ(db.snapshot_all(), before);
        CHECK_EQ(page_name(db, "100"), std::string("P100"));
        CHECK(ost_store::rename_page(db.path(), "20", "200", "Renamed")); // control: no status, nothing read
        CHECK_EQ(page_name(db, "200"), std::string("Renamed"));
    }
    {
        TempDb db("rename_review_lock_read_fails_bids");
        ost_ptest::seed_two_bids(db);
        db.must("ALTER TABLE Bids DROP COLUMN JobStatusUID");
        const std::string before = db.snapshot_all();
        std::string error = "stale";
        CHECK_FALSE(ost_store::rename_page(db.path(), "10", "100", "Renamed", &error));
        CHECK_FALSE(ost_store::rename_page(db.path(), "20", "200", "Renamed"));
        CHECK(error != "stale");
        CHECK(lower(error).find("read") != std::string::npos);
        CHECK_EQ(db.snapshot_all(), before);
        db.must("ALTER TABLE Bids ADD COLUMN JobStatusUID");
        CHECK(ost_store::rename_page(db.path(), "10", "100", "Renamed")); // control
        CHECK_EQ(page_name(db, "100"), std::string("Renamed"));
    }
}
