// M1 layers (plan 5.3), DATA SAFETY of ost_store::swap_layer_sequence (Move Up / Move Down): the exact exchange, a == b, unknown / malformed uids, cross-bid
// pairs, the shared template row as either side, NULL and equal sequences, the bid's own locked copies as a neighbour, the failure points (the second
// UPDATE, the COMMIT), the bid lock, a failed read, and the invariant that a swap never changes the multiset of sequence values.
//
// Windows: layer_operations.py:432-465 swap_layer_sequence (no lock or template filter, the scope check "one bid", a == b / a missing row / a NULL
// Sequence give False); the ports are in test_layer_edit_store.cpp (:311, :1238, :1243, :1257, :1262). Deliberate differences: LY-6 (a shared neighbour is
// refused, the bid's own locked copies are allowed), LY-9 (the open bid), LY-12 (kept: equal Sequences succeed and nothing visibly moves, a NULL Sequence
// refuses), LY-13 (uid "007" is 7: Windows' seq_map lookup by the argument text makes swap return False for it). The world is seed_m1.
#include "LayerEditTestSupport.hpp"

#include <cstdio>
#include <string>
#include <vector>

using ost_ltest::add_bids;
using ost_ltest::block_when;
using ost_ltest::edited;
using ost_ltest::kM1Layers;
using ost_ltest::layers_text;
using ost_ltest::put_layer;
using ost_ltest::seed_m1;
using ost_ltest::sequences_text;
using ost_ptest::TempDb;

namespace
{
    // A refusal: false and the whole database byte-identical.
    void expect_refused(const TempDb &db, const std::string &bid, const std::string &a, const std::string &b)
    {
        const std::string before = db.snapshot_all();
        CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), bid, a, b));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }

    const char *kWalls = "(105,10,'Walls',-1,4,0,0)";
    const char *kFloors = "(106,10,'Floors',-1,5,0,0)";
    const char *kDefault = "(103,10,'Default',-1,2,0,-1)";

    // The multiset of the bid's sequence values: "0,1,2,3,4,5,6".
    std::string sequence_set(const TempDb &db, const std::string &bid)
    {
        return db.text("SELECT group_concat(Sequence) FROM (SELECT Sequence FROM BidLayers WHERE BidUID = " + bid + " ORDER BY Sequence)");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the exchange
// ---------------------------------------------------------------------------------------------------------------

// layer_operations.py:455-464: the two Sequence values are exchanged; nothing else in the row or the database changes.
TEST(LayerEditSwap, TwoOwnLayersExchangeTheirSequences)
{
    TempDb db("swap_basic");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string other = ost_ltest::snapshot_except(db, {"BidLayers"});
    CHECK(ost_store::swap_layer_sequence(db.path(), "10", "105", "106"));
    CHECK_STR_EQ(layers_text(db), edited(kM1Layers, {{kWalls, "(105,10,'Walls',-1,5,0,0)"}, {kFloors, "(106,10,'Floors',-1,4,0,0)"}}));
    CHECK_STR_EQ(ost_ltest::snapshot_except(db, {"BidLayers"}), other);
    // swapping back restores the seed
    CHECK(ost_store::swap_layer_sequence(db.path(), "10", "106", "105"));
    CHECK_STR_EQ(layers_text(db), kM1Layers);
}

// D6 / N-M1-19 / LY-6: the bid's OWN locked copies (Default 103, Image 101) are allowed as the neighbour, on either side: their sequence is rewritten.
TEST(LayerEditSwap, ALockedOwnCopyAsTheNeighbourIsAllowed)
{
    {
        TempDb db("swap_locked_b");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        CHECK(ost_store::swap_layer_sequence(db.path(), "10", "105", "103"));
        CHECK_STR_EQ(layers_text(db), edited(kM1Layers, {{kWalls, "(105,10,'Walls',-1,2,0,0)"}, {kDefault, "(103,10,'Default',-1,4,0,-1)"}}));
    }
    {
        TempDb db("swap_locked_a");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        CHECK(ost_store::swap_layer_sequence(db.path(), "10", "103", "105"));
        CHECK_STR_EQ(layers_text(db), edited(kM1Layers, {{kWalls, "(105,10,'Walls',-1,2,0,0)"}, {kDefault, "(103,10,'Default',-1,4,0,-1)"}}));
    }
    {
        TempDb db("swap_image");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        CHECK(ost_store::swap_layer_sequence(db.path(), "10", "101", "105"));
        CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,4);(102,1);(103,2);(104,3);(105,0);(106,5);(107,6)");
    }
}

// LY-13: the uids are canonical numbers: "0105" is 105 (Windows' seq_map is keyed by str(UID) but looked up by the argument text, so "007" made swap
// return False while the other operations accepted it).
TEST(LayerEditSwap, ALeadingZeroUidIsTheSameLayer)
{
    TempDb db("swap_leading_zero");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::swap_layer_sequence(db.path(), "10", "0105", "0106"));
    CHECK_STR_EQ(sequences_text(db, "UID IN (105, 106)"), "(105,5);(106,4)");
    CHECK(ost_store::swap_layer_sequence(db.path(), "010", "105", "106"));
    CHECK_STR_EQ(sequences_text(db, "UID IN (105, 106)"), "(105,4);(106,5)");
}

// layer_operations.py:448-449: a == b is False (also written as "105" and "0105": the same layer).
TEST(LayerEditSwap, ALayerWithItselfIsRefusedInEverySpelling)
{
    TempDb db("swap_self");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    expect_refused(db, "10", "105", "105");
    expect_refused(db, "10", "105", "0105");
    expect_refused(db, "10", "101", "101");
}

TEST(LayerEditSwap, AnUnknownOrMalformedUidIsRefusedOnEitherSide)
{
    TempDb db("swap_bad_uid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *uid : {"999", "0", "-5", "abc", "", " 105", "105 ", "1.0", "+105", "99999999999999999999", "105;106"})
    {
        SCOPED_CONTEXT(std::string("uid '") + uid + "'");
        expect_refused(db, "10", uid, "106");
        expect_refused(db, "10", "106", uid);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// ownership
// ---------------------------------------------------------------------------------------------------------------

// LY-9 / the Windows scope check: a pair with a row of ANOTHER bid is refused whatever the side; the bid that owns both can swap them.
TEST(LayerEditSwap, AForeignLayerIsRefusedOnEitherSide)
{
    TempDb db("swap_foreign");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    expect_refused(db, "10", "105", "201");
    expect_refused(db, "10", "201", "105");
    expect_refused(db, "20", "105", "106"); // both rows exist, neither is bid 20's
    expect_refused(db, "20", "105", "201");
    CHECK(ost_store::swap_layer_sequence(db.path(), "20", "201", "202"));
    CHECK_STR_EQ(sequences_text(db, "BidUID = 20"), "(201,2);(202,1)");
}

// LY-6: a shared template row (no bid: the schema's rows 1-4 and 'Shared Template' 8) is refused on either side: Windows raises the raw scope error
// "BidLayers mutation targets do not belong to one authoritative Bid."; nothing is written.
TEST(LayerEditSwap, ASharedTemplateRowIsRefusedOnEitherSide)
{
    TempDb db("swap_shared");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *uid : {"1", "2", "3", "4", "8"})
    {
        SCOPED_CONTEXT(std::string("shared row ") + uid);
        expect_refused(db, "10", "107", uid);
        expect_refused(db, "10", uid, "107");
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the sequence values
// ---------------------------------------------------------------------------------------------------------------

// LY-12 (layer_operations.py:451-454): a NULL Sequence on either row refuses (the list shows it as 0, Move says "Failed to move layer.").
TEST(LayerEditSwap, ANullSequenceOnEitherRowIsRefused)
{
    TempDb db("swap_null");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 108, "10", "NoSequence", "-1", "NULL");
    expect_refused(db, "10", "105", "108");
    expect_refused(db, "10", "108", "105");
}

// LY-12 (pinned by Windows): equal Sequences succeed and nothing visibly moves. A real swap first, so the no-op is judged against a changed state.
TEST(LayerEditSwap, EqualSequencesSucceedAndChangeNothing)
{
    TempDb db("swap_equal");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 108, "10", "Tie", "-1", "4"); // the same sequence as Walls
    CHECK(ost_store::swap_layer_sequence(db.path(), "10", "106", "107"));
    CHECK_STR_EQ(sequences_text(db, "UID IN (106, 107)"), "(106,6);(107,5)");
    const std::string before = db.snapshot_all();
    CHECK(ost_store::swap_layer_sequence(db.path(), "10", "105", "108"));
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// The values at the limits swap like any other (nothing is added to them): 2147483647 and -2147483648 exchange exactly, and a value beyond 32 bit that
// SQLite stores swaps too (no truncation through int).
TEST(LayerEditSwap, ExtremeSequencesAreExchangedWithoutTruncation)
{
    TempDb db("swap_extreme");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 108, "10", "Max", "-1", "2147483647");
    put_layer(db, 109, "10", "Min", "-1", "-2147483648");
    put_layer(db, 110, "10", "Big", "-1", "3000000000");
    CHECK(ost_store::swap_layer_sequence(db.path(), "10", "108", "109"));
    CHECK_STR_EQ(sequences_text(db, "UID IN (108, 109)"), "(108,-2147483648);(109,2147483647)");
    CHECK(ost_store::swap_layer_sequence(db.path(), "10", "108", "110"));
    CHECK_STR_EQ(sequences_text(db, "UID IN (108, 109, 110)"), "(108,3000000000);(109,2147483647);(110,-2147483648)");
}

// A swap never changes the SET of sequence values of the bid (it only exchanges two of them), also with ties and gaps: a chain of swaps keeps it.
TEST(LayerEditSwap, TheSetOfSequenceValuesStaysTheSameAfterAnySwap)
{
    TempDb db("swap_invariant");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 108, "10", "Tie", "-1", "4");
    put_layer(db, 109, "10", "Gap", "-1", "40");
    const std::string set_before = sequence_set(db, "10");
    const std::string other_bid = sequences_text(db, "BidUID = 20");
    const char *pairs[][2] = {{"105", "106"}, {"106", "107"}, {"101", "105"}, {"103", "107"}, {"108", "109"}, {"105", "108"}, {"102", "104"}, {"109", "101"}};
    for (const auto &pair : pairs)
    {
        SCOPED_CONTEXT(std::string("swap ") + pair[0] + "," + pair[1]);
        const std::string a_before = db.text(std::string("SELECT Sequence FROM BidLayers WHERE UID = ") + pair[0]);
        const std::string b_before = db.text(std::string("SELECT Sequence FROM BidLayers WHERE UID = ") + pair[1]);
        CHECK(ost_store::swap_layer_sequence(db.path(), "10", pair[0], pair[1]));
        // the two values really were exchanged (the set below would also survive a swap that did nothing)
        CHECK_STR_EQ(db.text(std::string("SELECT Sequence FROM BidLayers WHERE UID = ") + pair[0]), b_before);
        CHECK_STR_EQ(db.text(std::string("SELECT Sequence FROM BidLayers WHERE UID = ") + pair[1]), a_before);
        CHECK_STR_EQ(sequence_set(db, "10"), set_before);
        CHECK_STR_EQ(sequences_text(db, "BidUID = 20"), other_bid);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// failure points: the first UPDATE is rolled back with the second
// ---------------------------------------------------------------------------------------------------------------

// The second UPDATE fails: in whichever order the store writes the two rows, one of these two blockers is the second statement, so the first
// UPDATE must be rolled back (Windows: two UPDATEs in one transaction, layer_operations.py:455-464).
TEST(LayerEditSwap, AFailureOfTheSecondUpdateRollsTheFirstBack)
{
    for (const char *blocked : {"105", "106"})
    {
        SCOPED_CONTEXT(std::string("update of ") + blocked + " blocked");
        TempDb db(std::string("swap_second_fails_") + blocked);
        REQUIRE(db.schema_ok());
        seed_m1(db);
        const std::string before = db.snapshot_all();
        const std::string blocker = block_when(db, "BidLayers", "UPDATE", std::string("NEW.UID = ") + blocked);
        CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "10", "105", "106"));
        CHECK_STR_EQ(db.snapshot_all(), before);
        db.unblock(blocker);
        CHECK(ost_store::swap_layer_sequence(db.path(), "10", "105", "106")); // control: unblocked it works
    }
    TempDb db("swap_all_updates_fail");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string before = db.snapshot_all();
    db.block("BidLayers", "UPDATE");
    CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "10", "105", "106"));
    CHECK_STR_EQ(db.snapshot_all(), before);
}

TEST(LayerEditSwap, AFailedCommitLeavesTheSequencesAlone)
{
    TempDb db("swap_fail_commit");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    ost_ltest::make_commit_fail(db, "BidLayers", "UPDATE");
    const std::string before = db.snapshot_all();
    CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "10", "105", "106"));
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// ---------------------------------------------------------------------------------------------------------------
// the bid
// ---------------------------------------------------------------------------------------------------------------

TEST(LayerEditSwap, AMalformedOrMissingBidIsRefused)
{
    TempDb db("swap_bad_bid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *bid : {"0", "-10", "abc", "", " 10", "10 ", "1.0", "99", "99999999999999999999"})
    {
        SCOPED_CONTEXT(std::string("bid '") + bid + "'");
        expect_refused(db, bid, "105", "106");
    }
}

// plan 4.1: the lock is re-checked inside the transaction; an unresolved status is locked; another bid's lock does not matter.
TEST(LayerEditSwap, ALockedBidIsRefused)
{
    {
        TempDb db("swap_locked");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        ost_ptest::seed_locked_bid(db, 10, 5);
        expect_refused(db, "10", "105", "106");
        CHECK(ost_store::swap_layer_sequence(db.path(), "20", "201", "202"));
    }
    {
        TempDb db("swap_unresolved");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        ost_ptest::seed_bid_with_status(db, 10, "77");
        expect_refused(db, "10", "105", "106");
    }
    {
        TempDb db("swap_open");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        ost_ptest::seed_open_bid(db, 10, 4);
        ost_ptest::seed_locked_bid(db, 20, 5);
        CHECK(ost_store::swap_layer_sequence(db.path(), "10", "105", "106"));
    }
}

// D13: a table without Sequence or UID is refused; a database that cannot be read is refused without a throw.
TEST(LayerEditSwap, AMissingColumnAndAFailedReadAreRefusals)
{
    for (const char *column : {"UID", "Sequence"})
    {
        SCOPED_CONTEXT(std::string("") + column);
        TempDb db(std::string("swap_no_") + column);
        REQUIRE(db.schema_ok());
        add_bids(db, {10});
        ost_ltest::rebuild_layers_without(db, column);
        const std::string before = db.snapshot_all();
        CHECK_FALSE(ost_store::swap_layer_sequence(db.path(), "10", "1", "2"));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
    CHECK_FALSE(ost_store::swap_layer_sequence(ost_ltest::unopenable_db_path(), "10", "105", "106"));
    const std::string garbage = ost_ltest::garbage_db_path("swap");
    CHECK_FALSE(ost_store::swap_layer_sequence(garbage, "10", "105", "106"));
    std::remove(garbage.c_str());
}
