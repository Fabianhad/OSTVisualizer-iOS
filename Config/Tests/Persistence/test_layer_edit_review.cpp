// Independent review additions for S2 layers (plan 5.3): cases the first tests let a lazy implementation survive, found by mutation thinking.
// Additions only; nothing of test_layer_edit_*.cpp was changed or weakened. The behaviours pinned here are the documented "hardening beyond Windows"
// of ProjectStore_Layers.hpp (a Sequence cell that is neither NULL nor an integer, a database without BidLayers) plus the exact values the first
// tests only implied.
//
// Survivors this file kills:
//   - an insert / delete that drops bid_sequences_are_integers: `Sequence + 1` / `Sequence - 1` of a TEXT or a REAL cell silently means something
//     else than the limit check assumed (SQLite adds to a REAL, and turns TEXT into 0 + 1), and the 2147483647 guard is then meaningless;
//   - a swap that reads a TEXT / REAL Sequence as 0 (column_int64 of such a cell) and writes the wrong values;
//   - a bid_sequences_are_integers that looks at every row of the table instead of the bid's rows (another bid's or a shared row's odd cell would
//     lock the bid out of its own layers);
//   - a reload_layers that fails (or keeps the stale lists) for a database without a BidLayers table, where load_project gives empty lists;
//   - a reload_layers that treats a BidLayers table without BidUID as an empty family instead of an unreadable one.
#include "LayerEditTestSupport.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

using ost_ltest::put_layer;
using ost_ltest::seed_m1;
using ost_ptest::TempDb;

namespace
{
    // Cells an INTEGER-affinity column can still hold: TEXT that is not a number and a REAL that is not a whole number (SQLite turns '7' and 2.0
    // into the integers 7 and 2 by affinity, so those are not odd).
    const char *const kOddSequences[] = {"'abc'", "1.5", "'1.5x'", "'1e999x'"};

    void expect_swap_refused(const TempDb &db, const std::string &a, const std::string &b)
    {
        const std::string before = db.snapshot_all();
        CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "10", a, b));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
}

// insert_layer: a bid with a row whose Sequence is TEXT or REAL refuses (no text: the generic failure), for any after_sequence, nothing is written
// (snapshot_all before and after), no half shift. The cell of ANOTHER bid, of a shared row (no bid) never blocks.
TEST(LayerEditReview, InsertRefusesABidWhoseSequenceCellIsNotAnInteger)
{
    int index = 0;
    for (const char *odd : kOddSequences)
    {
        SCOPED_CONTEXT(std::string("odd Sequence ") + odd);
        TempDb db("rev_ins_odd_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        seed_m1(db);
        put_layer(db, 108, "10", "Odd", "-1", odd);
        const std::string before = db.snapshot_all();
        for (const long long after : {0LL, 6LL, 100LL})
        {
            SCOPED_CONTEXT("after_sequence " + std::to_string(after));
            std::string error = "stale";
            CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "New", after, &error), "");
            CHECK_STR_EQ(error, "");
            CHECK_STR_EQ(db.snapshot_all(), before);
        }
    }
    TempDb db("rev_ins_odd_elsewhere");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 209, "20", "OddOther", "-1", "'abc'");
    put_layer(db, 9, "NULL", "OddShared", "-1", "1.5", "-1", "-1");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "New", 6), "210"); // MAX(UID) is 209
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "UID IN (107, 210)"), "(107,6);(210,7)");
    // and the bid with the odd cell is the one that refuses
    std::string error = "stale";
    const std::string before = db.snapshot_all();
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "20", "Attic", 2, &error), "");
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// delete_layers: the same cell refuses a delete of ANY layer of that bid, the odd row itself included (a renumber of a TEXT / REAL cell would not
// mean `Sequence - 1`); nothing changes. The odd cell of another bid or of a shared row does not block a delete of bid 10's layer.
TEST(LayerEditReview, DeleteRefusesABidWhoseSequenceCellIsNotAnInteger)
{
    int index = 0;
    for (const char *odd : kOddSequences)
    {
        SCOPED_CONTEXT(std::string("odd Sequence ") + odd);
        TempDb db("rev_del_odd_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        seed_m1(db);
        put_layer(db, 108, "10", "Odd", "-1", odd);
        const std::string before = db.snapshot_all();
        for (const char *uid : {"105", "107", "108"})
        {
            SCOPED_CONTEXT(std::string("layer ") + uid);
            CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {uid}));
            CHECK_STR_EQ(db.snapshot_all(), before);
        }
        CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {"105", "107"}));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
    TempDb db("rev_del_odd_elsewhere");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 209, "20", "OddOther", "-1", "'abc'");
    put_layer(db, 9, "NULL", "OddShared", "-1", "1.5", "-1", "-1");
    CHECK(ost_store::delete_layers(db.path(), "10", {"105"}));
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,0);(102,1);(103,2);(104,3);(106,4);(107,5)");
}

// swap_layer_sequence: a TEXT or REAL Sequence on either row refuses (like NULL, LY-12): the two values are exchanged exactly or not at all.
TEST(LayerEditReview, SwapRefusesASequenceCellThatIsNotAnInteger)
{
    int index = 0;
    for (const char *odd : kOddSequences)
    {
        SCOPED_CONTEXT(std::string("odd Sequence ") + odd);
        TempDb db("rev_swap_odd_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        seed_m1(db);
        put_layer(db, 108, "10", "Odd", "-1", odd);
        expect_swap_refused(db, "105", "108");
        expect_swap_refused(db, "108", "105");
        expect_swap_refused(db, "101", "108");
    }
}

// reload_layers on a database that has NO BidLayers table: like load_project (a missing table is an empty family, not an unreadable database)
// both outputs are replaced by empty lists and the call succeeds; nothing is written.
TEST(LayerEditReview, ReloadWithoutABidLayersTableGivesEmptyListsLikeLoadProject)
{
    TempDb db("rev_reload_no_table");
    REQUIRE(db.schema_ok());
    ost_ltest::add_bids(db, {10});
    db.must("DROP TABLE BidLayers");
    const std::string before = db.snapshot_all();

    std::unordered_map<std::string, ost_domain::Layer> own;
    own["9"].uid = "9";
    own["9"].name = "Stale";
    std::vector<ost_domain::BidLayer> merged(2);
    merged[0].uid = "9";
    std::string error = "stale";
    CHECK(ost_store::reload_layers(db.path(), "10", own, merged, &error));
    CHECK_STR_EQ(error, "");
    CHECK(own.empty());
    CHECK(merged.empty());
    CHECK_STR_EQ(db.snapshot_all(), before);

    ost_data::ProjectData project;
    std::string load_error;
    if (ost_store::load_project(db.path(), "10", project, &load_error))
    {
        CHECK(project.layers.empty());
        CHECK(project.merged_layers.empty());
    }
}

// reload_layers: a BidLayers table that exists but cannot be read for the bid (no BidUID column) is a FAILED read, not an empty family: false, an
// error text, both outputs exactly as they were.
TEST(LayerEditReview, ReloadOfAnUnreadableBidLayersTableLeavesTheOutputsAlone)
{
    TempDb db("rev_reload_no_biduid");
    REQUIRE(db.schema_ok());
    ost_ltest::add_bids(db, {10});
    ost_ltest::rebuild_layers_without(db, "BidUID");

    std::unordered_map<std::string, ost_domain::Layer> own;
    own["9"].uid = "9";
    own["9"].name = "Kept";
    own["9"].sequence = 3;
    std::vector<ost_domain::BidLayer> merged(1);
    merged[0].uid = "9";
    merged[0].name = "Kept";
    merged[0].sequence = 3;
    const std::string own_before = ost_ltest::own_text(own);
    const std::string merged_before = ost_ltest::merged_text(merged);
    std::string error;
    CHECK_FALSE(ost_store::reload_layers(db.path(), "10", own, merged, &error));
    CHECK(!error.empty());
    CHECK_STR_EQ(ost_ltest::own_text(own), own_before);
    CHECK_STR_EQ(ost_ltest::merged_text(merged), merged_before);
}

// ---------------------------------------------------------------------------------------------------------------
// second review pass: contracts of ProjectStore_Layers.hpp and plan 4.4 item 2 that no test pinned
// ---------------------------------------------------------------------------------------------------------------

// ProjectStore_Layers.hpp (delete_layers): "An empty list is a success that writes nothing (the bid must still exist and be open for writes: a
// missing or locked bid refuses it too)". A delete_layers that answers true for an empty list before it looks at the bid would tell a screen
// that a write on a locked or missing bid worked.
TEST(LayerEditReview, AnEmptyDeleteListStillNeedsAnExistingOpenBid)
{
    TempDb db("rev_del_empty_bid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string before = db.snapshot_all();
    for (const char *bid : {"0", "-10", "abc", "", " 10", "1.0", "99", "99999999999999999999"})
    {
        SCOPED_CONTEXT(std::string("bid '") + bid + "'");
        CHECK_FALSE(ost_store::delete_layers(db.path(), bid, {}));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
    CHECK(ost_store::delete_layers(db.path(), "10", {}));
    CHECK(ost_store::delete_layers(db.path(), "010", {}));
    CHECK_STR_EQ(db.snapshot_all(), before);

    ost_ptest::seed_locked_bid(db, 10, 5);
    const std::string locked_before = db.snapshot_all();
    CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {}));
    CHECK_STR_EQ(db.snapshot_all(), locked_before);
    CHECK(ost_store::delete_layers(db.path(), "20", {})); // bid 20 has no status: open
    CHECK_STR_EQ(db.snapshot_all(), locked_before);
}

// Plan 4.4 item 2 order: ... the bid lock (4.1), then the name / value rules, then the limits. On a locked bid every refusal is the SILENT one: no
// user text from the name rules or the limits (a screen that showed "Layer Walls already exists." for a locked bid would hide the real reason, and
// the lock check must not depend on the typed text). Insert and rename, one case per rule and per limit.
TEST(LayerEditReview, ALockedBidRefusesBeforeTheNameRulesAndTheLimitsSpeak)
{
    TempDb db("rev_lock_first");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ptest::seed_locked_bid(db, 10, 5);
    const std::string before = db.snapshot_all();
    const std::vector<std::string> names = {"Walls",  "walls ", "Comments", "Image", ost_ltest::repeat("a", 51), "A\tB", "A\nB",
                                            std::string("A\xFF" "B"), "",     "   ",      "Fine name"};
    int index = 0;
    for (const std::string &name : names)
    {
        SCOPED_CONTEXT("name #" + std::to_string(index++));
        std::string error = "stale";
        CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", name, 6, &error), "");
        CHECK_STR_EQ(error, "");
        error = "stale";
        CHECK_FALSE(ost_store::rename_layer(db.path(), "10", "107", name, &error));
        CHECK_STR_EQ(error, "");
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
    // the limits: after_sequence past 2147483647 and a later row at the limit would speak on an open bid ("no sequence number is left")
    for (const long long after : {2147483647LL, -4000000000LL})
    {
        SCOPED_CONTEXT("after_sequence " + std::to_string(after));
        std::string error = "stale";
        CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Limit", after, &error), "");
        CHECK_STR_EQ(error, "");
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
    // the UID limit as well ("no identity is left"): MAX(UID) = 2147483647
    put_layer(db, 2147483647LL, "10", "Big", "-1", "7");
    const std::string with_big = db.snapshot_all();
    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Limit", 7, &error), "");
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(db.snapshot_all(), with_big);
}

// A swap whose two rows hold EQUAL sequences changes nothing and is a success on an open bid (LY-12); on a locked bid it is still refused (the
// lock is not skipped just because nothing would be written: the caller must not be told that a write on a locked bid worked).
TEST(LayerEditReview, ALockedBidRefusesEvenASwapThatWouldChangeNothing)
{
    TempDb db("rev_swap_equal_locked");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 108, "10", "Tie", "-1", "4"); // the same sequence as Walls
    CHECK(ost_store::swap_layer_sequence(db.path(), "10", "105", "108")); // open bid: success, nothing visible
    ost_ptest::seed_locked_bid(db, 10, 5);
    expect_swap_refused(db, "105", "108");
}

// ---------------------------------------------------------------------------------------------------------------
// third review pass (reviewer label layers-r): a lock READ that fails, UIDs beyond 32 bit, reads on a locked bid
// ---------------------------------------------------------------------------------------------------------------

// Plan 4.1 (fail closed): a lock that cannot be READ refuses every writer. Bid 10 has the OPEN status 4 (Locked 0), but the JobStatuses table has
// lost its Locked column, so bid_is_locked answers "the read failed" (false, locked = true). The earlier lock tests only used a locked status or
// an unresolved one, which a writer that treats a failed read as "open" would also refuse. insert / rename / delete / swap: nothing is written;
// the control run after the column is back writes.
TEST(LayerEditReview, ALockThatCannotBeReadRefusesEveryWriter)
{
    TempDb db("rev_lock_unreadable");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ptest::seed_open_bid(db, 10, 4);
    db.must("ALTER TABLE JobStatuses RENAME COLUMN Locked TO LockedGone");
    const std::string before = db.snapshot_all();

    std::string error = "stale";
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Roofing", 6, &error), "");
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(db.snapshot_all(), before);
    error = "stale";
    CHECK_FALSE(ost_store::rename_layer(db.path(), "10", "107", "Spare", &error));
    CHECK_STR_EQ(error, "");
    CHECK_STR_EQ(db.snapshot_all(), before);
    CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {"107"}));
    CHECK_STR_EQ(db.snapshot_all(), before);
    CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {}));
    CHECK_STR_EQ(db.snapshot_all(), before);
    CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "10", "105", "106"));
    CHECK_STR_EQ(db.snapshot_all(), before);

    db.must("ALTER TABLE JobStatuses RENAME COLUMN LockedGone TO Locked");
    CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "Roofing", 6), "203"); // control: the same call on a readable open bid
}

// The 2147483647 limit of the UID (plan 4.2) on rows that are already beyond it (an imported database, a hand edit): MAX(UID) of 2147483648, 3000000000,
// 4294967296 (which a 32-bit read would see as 0) and 9223372036854775807 (the 63-bit end: no UID is left above it) refuse the insert before any
// write, with the "no identity" text where the helper can tell the limit from a failed statement, and the rows stay exactly as they were (nothing
// wraps, nothing is shifted). The other writers still address such a row (64-bit uid parse and bind): rename, swap and delete work on it.
TEST(LayerEditReview, UidsBeyond32BitRefuseTheInsertAndTheOtherWritersStillWork)
{
    const char *const uids[] = {"2147483648", "3000000000", "4294967296", "9223372036854775807"};
    int index = 0;
    for (const char *uid : uids)
    {
        SCOPED_CONTEXT(std::string("MAX(UID) = ") + uid);
        TempDb db("rev_uid_big_" + std::to_string(index++));
        REQUIRE(db.schema_ok());
        seed_m1(db);
        put_layer(db, std::stoll(uid), "10", "Huge", "-1", "7");
        const std::string before = db.snapshot_all();
        for (const long long after : {7LL, 0LL})
        {
            std::string error = "stale";
            CHECK_STR_EQ(ost_store::insert_layer(db.path(), "10", "New", after, &error), "");
            CHECK(error.empty() || error == "Cannot add a layer: no identity is left below 2147483647.");
            CHECK_STR_EQ(db.snapshot_all(), before);
        }
        CHECK(ost_store::rename_layer(db.path(), "10", uid, "Huge 2"));
        CHECK_STR_EQ(db.text(std::string("SELECT Name FROM BidLayers WHERE UID = ") + uid), "Huge 2");
        CHECK(ost_store::swap_layer_sequence(db.path(), "10", uid, "107"));
        CHECK_STR_EQ(ost_ltest::sequences_text(db, std::string("UID IN (107, ") + uid + ")"),
                     std::string("(107,7);(") + uid + ",6)");
        CHECK(ost_store::delete_layers(db.path(), "10", {uid}));
        CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM BidLayers WHERE UID = ") + uid), 0);
    }
}

// Reads are not writes: reload_layers on a LOCKED bid works and gives exactly the lists load_project gives (the Layers sheet still opens and shows
// a locked bid; only the writers refuse). A reload that asked the lock guard would leave the sheet empty on every locked bid.
TEST(LayerEditReview, ReloadReadsALockedBidLikeAnyOther)
{
    TempDb db("rev_reload_locked");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ptest::seed_locked_bid(db, 10, 5);
    std::unordered_map<std::string, ost_domain::Layer> own;
    std::vector<ost_domain::BidLayer> merged;
    std::string error = "stale";
    CHECK(ost_store::reload_layers(db.path(), "10", own, merged, &error));
    CHECK_STR_EQ(error, "");
    CHECK(!merged.empty());
    CHECK(!own.empty());
    ost_data::ProjectData project;
    std::string load_error;
    REQUIRE(ost_store::load_project(db.path(), "10", project, &load_error));
    CHECK_STR_EQ(ost_ltest::own_text(own), ost_ltest::own_text(project.layers));
    CHECK_STR_EQ(ost_ltest::merged_text(merged), ost_ltest::merged_text(project.merged_layers));
}

// LY-4b (deviation, safe side): Windows _unique_nonempty_uids (project_write_service.py:6884-6892, `if uid and uid not in seen`) silently drops an
// empty uid and deletes the rest. Here an empty or malformed element refuses the WHOLE call and writes nothing: a list with such an element is a
// bug of the caller, and a partial delete would hide it. The same call without the bad element works.
TEST(LayerEditReview, AnEmptyOrMalformedUidAmongValidOnesRefusesTheWholeDelete)
{
    TempDb db("rev_delete_empty_uid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string before = db.snapshot_all();
    const std::vector<std::vector<std::string>> lists = {
        {"107", ""}, {"", "107"}, {"107", "abc", "106"}, {"107", "-1"}, {"107", "106 "}, {"107", "0"}};
    int index = 0;
    for (const std::vector<std::string> &list : lists)
    {
        SCOPED_CONTEXT("list " + std::to_string(index++));
        CHECK_FALSE(ost_store::delete_layers(db.path(), "10", list));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
    CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 107"), 0);
}

// plan_layer_move: the neighbour must be a row of the OPEN bid. The merged list never holds a row of another bid that is not a shared template, but
// the plan must not rely on that: a listed neighbour of another bid is refused whatever its template / locked flags are (the shared-template rule
// alone, IsTemplate and IsLocked and another bid, would let an unflagged foreign neighbour through). The same list with a neighbour of the open bid
// is allowed, so the refusal is about the bid and not about the position.
TEST(LayerEditReview, ANeighbourOfAnotherBidIsRefusedWhateverItsFlags)
{
    auto make = [](const char *uid, const char *bid, int sequence, bool is_template, bool is_locked)
    {
        ost_domain::BidLayer layer;
        layer.uid = uid;
        layer.bid_uid = bid;
        layer.name = std::string("Layer ") + uid;
        layer.show = true;
        layer.sequence = sequence;
        layer.is_template = is_template;
        layer.is_locked = is_locked;
        return layer;
    };
    for (const bool is_template : {false, true})
    {
        for (const bool is_locked : {false, true})
        {
            SCOPED_CONTEXT(std::string("template ") + (is_template ? "1" : "0") + " locked " + (is_locked ? "1" : "0"));
            const std::vector<ost_domain::BidLayer> below = {make("1", "10", 1, false, false), make("2", "20", 2, is_template, is_locked)};
            const ost_domain::LayerMovePlan down = ost_domain::plan_layer_move(below, "10", "1", +1);
            CHECK_FALSE(down.allowed);
            CHECK_STR_EQ(down.neighbor_uid, "");
            CHECK(!down.refusal.empty());
            const std::vector<ost_domain::BidLayer> above = {make("2", "20", 1, is_template, is_locked), make("1", "10", 2, false, false)};
            const ost_domain::LayerMovePlan up = ost_domain::plan_layer_move(above, "10", "1", -1);
            CHECK_FALSE(up.allowed);
            CHECK_STR_EQ(up.neighbor_uid, "");
            CHECK(!up.refusal.empty());
        }
    }
    const std::vector<ost_domain::BidLayer> own_neighbour = {make("1", "10", 1, false, false), make("2", "10", 2, false, false)};
    const ost_domain::LayerMovePlan allowed = ost_domain::plan_layer_move(own_neighbour, "10", "1", +1);
    CHECK(allowed.allowed);
    CHECK_STR_EQ(allowed.neighbor_uid, "2");
}

// reload_layers judges the UIDs of the bid's OWN layers first, exactly like load_project (a layer UID that is not a positive integer fails the read
// with the Windows text, the outputs stay untouched). A shared row with a bad UID that comes earlier in the table must not change WHICH problem is
// reported: without the own-rows check the merged read would name the shared row (-5) instead of the bid's own row (0).
TEST(LayerEditReview, ReloadFailsLikeLoadProjectForABidWhoseOwnLayerUidIsMalformed)
{
    TempDb db("rev_reload_bad_uid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, -5, "NULL", "Broken shared", "-1", "8", "-1", "-1"); // a shared row with a non-positive uid, first in rowid order
    put_layer(db, 0, "10", "Broken own", "-1", "9");                   // an own row with uid 0
    ost_data::ProjectData project;
    std::string load_error;
    CHECK_FALSE(ost_store::load_project(db.path(), "10", project, &load_error));
    CHECK(load_error.find("malformed UID 0") != std::string::npos);

    std::unordered_map<std::string, ost_domain::Layer> own;
    own["9"].uid = "9";
    std::vector<ost_domain::BidLayer> merged(1);
    merged[0].uid = "9";
    std::string error;
    CHECK_FALSE(ost_store::reload_layers(db.path(), "10", own, merged, &error));
    CHECK_STR_EQ(error, load_error);
    CHECK_EQ(own.size(), static_cast<std::size_t>(1)); // untouched
    CHECK_EQ(merged.size(), static_cast<std::size_t>(1));
}

// plan_layer_move: an empty uid means "no layer selected" and is refused even when a listed row happens to carry an empty uid (the store never
// produces one, the plan does not rely on that).
TEST(LayerEditReview, AnEmptyUidIsNeverMovableEvenWhenARowCarriesIt)
{
    auto make = [](const char *uid, int sequence)
    {
        ost_domain::BidLayer layer;
        layer.uid = uid;
        layer.bid_uid = "10";
        layer.name = std::string("Layer ") + uid;
        layer.show = true;
        layer.sequence = sequence;
        return layer;
    };
    const std::vector<ost_domain::BidLayer> listed = {make("", 1), make("2", 2), make("3", 3)};
    for (const int direction : {-1, +1})
    {
        SCOPED_CONTEXT("direction " + std::to_string(direction));
        const ost_domain::LayerMovePlan plan = ost_domain::plan_layer_move(listed, "10", "", direction);
        CHECK_FALSE(plan.allowed);
        CHECK_STR_EQ(plan.neighbor_uid, "");
        CHECK(!plan.refusal.empty());
    }
    CHECK(ost_domain::plan_layer_move(listed, "10", "2", +1).allowed); // the same list moves a row that has a uid
}
