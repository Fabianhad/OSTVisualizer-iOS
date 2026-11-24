// M1 layers (plan 5.3), DATA SAFETY of ost_store::delete_layers: ownership and guards (only the OPEN bid's own unlocked, non-template rows), each of the
// 11 reference tables cleared in turn (that table only; other layers' and other bids' rows intact), a failed clear in every table rolls the whole
// delete back, failed renumber / DELETE / COMMIT, the renumbering rules, the all-or-nothing batch, the bid lock, a failed read.
//
// Windows: layer_operations.py:153-217 delete_layer (clear BidLayerUID NULL in LAYER_REFERENCE_TABLES unscoped, DELETE, renumber the later rows of the
// same bid), the ports are in test_layer_edit_store.cpp (:145, :194, :1064, :1091, :1116, :1147, :1180, :1185, :2832). Deliberate differences: LY-4 (a
// batch is one transaction, all or nothing: Windows commits the layers deleted before a refused one, project_write_service.py:6689-6725), LY-5 (any
// failed clear aborts and rolls back), LY-9 (the open bid). The store keeps the Windows contract of clearing and deleting an in-use layer: the REFUSAL
// is the usage read plus the delete plan (test_layer_edit_usage.cpp). The world is seed_m1 (LayerEditTestSupport.hpp).
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
using ost_ltest::put_reference;
using ost_ltest::reference_rows;
using ost_ltest::reference_tables;
using ost_ltest::seed_m1;
using ost_ltest::sequences_text;
using ost_ltest::table_text;
using ost_ptest::TempDb;

namespace
{
    // A refusal: false and the whole database byte-identical.
    void expect_refused(const TempDb &db, const std::string &bid, const std::vector<std::string> &uids)
    {
        const std::string before = db.snapshot_all();
        CHECK_FALSE(ost_store::delete_layers(db.path(), bid, uids));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }

    // The M1 layer rows without the three own layers' rows that a test deletes / renumbers.
    const char *kWalls = "(105,10,'Walls',-1,4,0,0)";
    const char *kFloors = "(106,10,'Floors',-1,5,0,0)";
    const char *kUnused = "(107,10,'Unused',-1,6,0,0)";
}

// ---------------------------------------------------------------------------------------------------------------
// the references: each of the 11 tables in turn
// ---------------------------------------------------------------------------------------------------------------

// layer_operations.py:184-194: the layer's reference is set NULL in THIS table (BidLayerUID = ?); another layer of the same bid, another bid's
// row and an already empty reference keep their values; no other reference table changes. One table at a time, for all 11 (the deleted layer is
// 107 'Unused', which the seed does not reference).
TEST(LayerEditDelete, AReferenceInEachOfTheElevenTablesIsClearedInThatTableOnly)
{
    for (const std::string &table : reference_tables())
    {
        SCOPED_CONTEXT(std::string("") + table);
        TempDb db("del_ref_" + table);
        REQUIRE(db.schema_ok());
        seed_m1(db);
        put_reference(db, table, 900, "10", "107"); // the deleted layer's reference
        put_reference(db, table, 901, "10", "106"); // another layer of the same bid
        put_reference(db, table, 902, "20", "201"); // another bid's row
        put_reference(db, table, 903, "20", "NULL"); // already empty
        const std::vector<std::string> before = reference_rows(db);
        const std::string other_tables = ost_ltest::snapshot_except(db, ost_ltest::layer_family_tables());

        CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
        CHECK_STR_EQ(table_text(db, table, "UID, BidLayerUID", "UID >= 900"), "(900,NULL);(901,106);(902,201);(903,NULL)");
        CHECK_EQ(db.count("SELECT COUNT(*) FROM [" + table + "] WHERE BidLayerUID = 107"), 0);
        const std::vector<std::string> after = reference_rows(db);
        REQUIRE_EQ(after.size(), before.size());
        for (std::size_t i = 0; i < after.size(); ++i)
        {
            if (reference_tables()[i] == table)
                continue;
            SCOPED_CONTEXT("other table " + reference_tables()[i]);
            CHECK_STR_EQ(after[i], before[i]);
        }
        CHECK_STR_EQ(layers_text(db), edited(kM1Layers, {{std::string(";") + kUnused, ""}}));
        CHECK_STR_EQ(ost_ltest::snapshot_except(db, ost_ltest::layer_family_tables()), other_tables);
    }
}

// The Windows clear is not scoped to the bid (`UPDATE [t] SET [BidLayerUID]=NULL WHERE [BidLayerUID]=?`, layer_operations.py:190-194): a row of ANOTHER
// bid that points at the deleted layer (a cross-bid reference, bad data) is cleared too, so no dangling reference to a deleted UID is left.
TEST(LayerEditDelete, TheClearIsNotScopedToTheBid)
{
    TempDb db("del_cross_bid_ref");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_reference(db, "BidZones", 900, "20", "107");
    put_reference(db, "BidTexts", 900, "NULL", "107");
    CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
    CHECK_STR_EQ(table_text(db, "BidZones", "UID, BidLayerUID"), "(900,NULL)");
    CHECK_STR_EQ(table_text(db, "BidTexts", "UID, BidLayerUID"), "(900,NULL)");
}

// The store keeps the Windows contract: it deletes an IN-USE layer too and clears its references (the condition loses its layer). The refusal is the
// usage read plus the delete plan, which a screen runs first (test_layer_edit_usage.cpp, plan 4.4 item 4): condition 301 uses Walls, annotation 501
// uses Floors.
TEST(LayerEditDelete, TheStoreKeepsTheWindowsContractForAnInUseLayer)
{
    TempDb db("del_in_use");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::delete_layers(db.path(), "10", {"105", "106"}));
    CHECK_STR_EQ(table_text(db, "BidConditions", "UID, BidLayerUID"), "(301,NULL);(302,201)");
    CHECK_STR_EQ(table_text(db, "BidAnnotationRects", "UID, BidLayerUID"), "(501,NULL)");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID IN (105, 106)"), 0);
}

// layer_operations.py:184-188 skips a missing table or a table without the column: a database where one of the 11 tables is gone still deletes.
TEST(LayerEditDelete, AMissingReferenceTableIsSkipped)
{
    TempDb db("del_missing_table");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    db.must("DROP TABLE BidCallOuts");
    CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 107"), 0);
}

// Nothing outside BidLayers and the 11 reference tables is touched by a delete (every other table byte-identical).
TEST(LayerEditDelete, NothingOutsideTheLayerFamilyChanges)
{
    TempDb db("del_only_family");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string other = ost_ltest::snapshot_except(db, ost_ltest::layer_family_tables());
    CHECK(ost_store::delete_layers(db.path(), "10", {"105", "107"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID IN (105, 107)"), 0); // the delete really happened
    CHECK_STR_EQ(ost_ltest::snapshot_except(db, ost_ltest::layer_family_tables()), other);
}

// ---------------------------------------------------------------------------------------------------------------
// failure points: the whole delete is rolled back
// ---------------------------------------------------------------------------------------------------------------

// LY-5 (layer_operations.py:189-203 logs and goes on when a clear fails; the production driver makes every error fatal, so the delete aborts):
// a failure of the clear in ANY of the 11 tables rolls back everything, the layer, the tables cleared before it, the renumbering. Walls (105)
// is deleted; every table holds a reference to it.
TEST(LayerEditDelete, AFailedClearInAnyOfTheElevenTablesRollsTheWholeDeleteBack)
{
    TempDb db("del_clear_fails");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    int uid = 900;
    for (const std::string &table : reference_tables())
        put_reference(db, table, uid++, "10", "105");
    const std::string before = db.snapshot_all();
    for (const std::string &table : reference_tables())
    {
        SCOPED_CONTEXT("clear blocked in " + table);
        const std::string blocker = db.block(table, "UPDATE");
        CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {"105"}));
        CHECK_STR_EQ(db.snapshot_all(), before);
        db.unblock(blocker);
    }
    // the control run: unblocked, the same call works
    CHECK(ost_store::delete_layers(db.path(), "10", {"105"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 105"), 0);
}

// The renumbering fails (BEFORE UPDATE on BidLayers; the later rows 106 and 107 would move), the DELETE fails, the COMMIT fails: each rolls back
// the clearing of the references and the delete.
TEST(LayerEditDelete, AFailedRenumberDeleteOrCommitRollsEverythingBack)
{
    {
        TempDb db("del_fail_renumber");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        const std::string before = db.snapshot_all();
        db.block("BidLayers", "UPDATE");
        CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {"105"})); // clears 301, deletes, then the renumber fails
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
    {
        TempDb db("del_fail_delete");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        const std::string before = db.snapshot_all();
        db.block("BidLayers", "DELETE");
        CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {"105"}));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
    {
        TempDb db("del_fail_commit");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        ost_ltest::make_commit_fail(db, "BidLayers", "DELETE");
        const std::string before = db.snapshot_all();
        CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {"105"}));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
}

// LY-4: two deletes where the SECOND fails later (BEFORE DELETE on layer 106 only): the first one is rolled back with it (Windows would commit the
// layers deleted before a refused one).
TEST(LayerEditDelete, ABatchWhoseSecondDeleteFailsIsRolledBackAsAWhole)
{
    TempDb db("del_second_fails");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string before = db.snapshot_all();
    const std::string blocker = block_when(db, "BidLayers", "DELETE", "OLD.UID = 106");
    CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {"105", "106"}));
    CHECK_STR_EQ(db.snapshot_all(), before);
    CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {"106", "105"}));
    CHECK_STR_EQ(db.snapshot_all(), before);
    db.unblock(blocker);
    CHECK(ost_store::delete_layers(db.path(), "10", {"105", "106"}));
}

// After a failed delete the database is usable: no lock and no half transaction is left.
TEST(LayerEditDelete, AFailedDeleteLeavesTheDatabaseUsable)
{
    TempDb db("del_after_failure");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    const std::string blocker = db.block("BidLayers", "DELETE");
    CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {"107"}));
    db.unblock(blocker);
    CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
    CHECK_STR_EQ(layers_text(db), edited(kM1Layers, {{std::string(";") + kUnused, ""}}));
}

// ---------------------------------------------------------------------------------------------------------------
// renumbering
// ---------------------------------------------------------------------------------------------------------------

// layer_operations.py:205-216: only the LATER rows of the SAME bid move down by one (Walls 4: Floors 5 -> 4, Unused 6 -> 5); the locked copies below,
// the other bid's rows and the shared rows never move.
TEST(LayerEditDelete, OnlyTheLaterRowsOfTheSameBidAreRenumbered)
{
    TempDb db("del_renumber");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::delete_layers(db.path(), "10", {"105"}));
    CHECK_STR_EQ(layers_text(db), edited(kM1Layers, {{std::string(kWalls) + ";", ""},
                                                     {kFloors, "(106,10,'Floors',-1,4,0,0)"},
                                                     {kUnused, "(107,10,'Unused',-1,5,0,0)"}}));
}

// The last layer (no later row), and a deleted layer with NO sequence (no renumber at all, layer_operations.py:205-216 `deleted_seq is not None`).
TEST(LayerEditDelete, TheLastLayerAndALayerWithoutASequenceRenumberNothing)
{
    TempDb db("del_no_renumber");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
    CHECK_STR_EQ(layers_text(db), edited(kM1Layers, {{std::string(";") + kUnused, ""}}));
    put_layer(db, 108, "10", "NoSequence", "-1", "NULL");
    const std::string before = ost_ltest::sequences_text(db);
    CHECK(ost_store::delete_layers(db.path(), "10", {"108"}));
    CHECK_STR_EQ(ost_ltest::sequences_text(db), edited(before, {{";(108,NULL)", ""}}));
}

// A row with NULL Sequence does not move when an earlier layer is deleted (`Sequence > ?` is not true for NULL).
TEST(LayerEditDelete, ARowWithoutASequenceIsNotRenumbered)
{
    TempDb db("del_null_stays");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 108, "10", "NoSequence", "-1", "NULL");
    CHECK(ost_store::delete_layers(db.path(), "10", {"105"}));
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,0);(102,1);(103,2);(104,3);(106,4);(107,5);(108,NULL)");
}

// The only layer of a bid: its row goes, nothing else exists to renumber.
TEST(LayerEditDelete, TheOnlyLayerOfABidCanBeDeleted)
{
    TempDb db("del_only_layer");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    add_bids(db, {30});
    put_layer(db, 150, "30", "Alone", "-1", "1");
    CHECK(ost_store::delete_layers(db.path(), "30", {"150"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE BidUID = 30"), 0);
    CHECK_STR_EQ(layers_text(db), kM1Layers);
}

// The limits: a layer at Sequence 2147483647 deletes (no later row); a later row at 2147483647 moves DOWN to 2147483646 (the direction has no
// overflow); gaps and ties are not repaired, they just move down by one.
TEST(LayerEditDelete, SequencesAtTheLimitGapsAndTiesAreHandledExactly)
{
    {
        TempDb db("del_seq_max");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        put_layer(db, 108, "10", "Max", "-1", "2147483647");
        CHECK(ost_store::delete_layers(db.path(), "10", {"108"}));
        CHECK_STR_EQ(layers_text(db), kM1Layers);
    }
    {
        TempDb db("del_seq_later_max");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        put_layer(db, 108, "10", "Max", "-1", "2147483647");
        CHECK(ost_store::delete_layers(db.path(), "10", {"105"}));
        CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,0);(102,1);(103,2);(104,3);(106,4);(107,5);(108,2147483646)");
    }
    {
        TempDb db("del_gaps_ties");
        REQUIRE(db.schema_ok());
        ost_ltest::clear_layers(db);
        add_bids(db, {10});
        put_layer(db, 1, "10", "A", "-1", "2");
        put_layer(db, 2, "10", "B", "-1", "10");
        put_layer(db, 3, "10", "C", "-1", "10");
        put_layer(db, 4, "10", "D", "-1", "2");
        CHECK(ost_store::delete_layers(db.path(), "10", {"1"}));
        CHECK_STR_EQ(sequences_text(db), "(2,9);(3,9);(4,2)"); // the tie at 2 does not move (not greater), the gap stays a gap
    }
}

// ---------------------------------------------------------------------------------------------------------------
// ownership and guards
// ---------------------------------------------------------------------------------------------------------------

TEST(LayerEditDelete, AnUnknownOrMalformedLayerUidIsRefused)
{
    TempDb db("del_bad_uid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *uid : {"999", "0", "-5", "abc", "", " 107", "107 ", "1.0", "+107", "99999999999999999999", "107;105"})
    {
        SCOPED_CONTEXT(std::string("uid '") + uid + "'");
        expect_refused(db, "10", {uid});
    }
}

TEST(LayerEditDelete, ALeadingZeroUidIsTheSameLayer)
{
    TempDb db("del_leading_zero");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::delete_layers(db.path(), "10", {"0107"}));
    CHECK_STR_EQ(layers_text(db), edited(kM1Layers, {{std::string(";") + kUnused, ""}}));
}

// LY-9: a layer of ANOTHER bid, a shared template row and the bid's own locked copies are refused (layer_operations.py:158 checks "one bid", the guard
// at :168-172 IsTemplate = 0 AND IsLocked = 0).
TEST(LayerEditDelete, ForeignSharedAndLockedRowsAreRefused)
{
    TempDb db("del_guards");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *uid : {"201", "202", "1", "2", "3", "4", "8", "101", "102", "103", "104"})
    {
        SCOPED_CONTEXT(std::string("layer ") + uid);
        expect_refused(db, "10", {uid});
    }
    expect_refused(db, "20", {"105"}); // the bid that does not own it
    CHECK(ost_store::delete_layers(db.path(), "20", {"202"}));
}

// A template flag, a NULL flag (fail closed: `= 0` does not match NULL, LY-10) and any nonzero flag refuse the delete.
TEST(LayerEditDelete, TemplateNullAndNonzeroFlagsAreRefused)
{
    TempDb db("del_flags");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    put_layer(db, 110, "10", "NullTemplate", "-1", "8", "NULL", "0");
    put_layer(db, 111, "10", "NullLocked", "-1", "9", "0", "NULL");
    put_layer(db, 112, "10", "NullBoth", "-1", "10", "NULL", "NULL");
    put_layer(db, 113, "10", "OwnTemplate", "-1", "11", "-1", "0");
    put_layer(db, 114, "10", "PositiveLocked", "-1", "12", "0", "1");
    put_layer(db, 115, "10", "PositiveTemplate", "-1", "13", "1", "0");
    for (const char *uid : {"110", "111", "112", "113", "114", "115"})
    {
        SCOPED_CONTEXT(std::string("row ") + uid);
        expect_refused(db, "10", {uid});
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the batch (LY-4: one transaction, all or nothing)
// ---------------------------------------------------------------------------------------------------------------

// A repeated uid counts once: Walls twice renumbers the later rows once (Floors 4, Unused 5), not twice.
TEST(LayerEditDelete, ARepeatedUidIsDeletedOnce)
{
    TempDb db("del_dedupe");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::delete_layers(db.path(), "10", {"105", "105", "0105"}));
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,0);(102,1);(103,2);(104,3);(106,4);(107,5)");
}

// Two layers in one call renumber like two single deletes (each uid reads the sequence it has at that moment, layer_operations.py:181): Walls and
// Floors out, Unused ends at 4, in either order.
TEST(LayerEditDelete, ABatchRenumbersLikeSingleDeletes)
{
    for (const std::vector<std::string> &order : {std::vector<std::string>{"105", "106"}, std::vector<std::string>{"106", "105"}})
    {
        SCOPED_CONTEXT(order[0] + "," + order[1]);
        TempDb db("del_batch_" + order[0]);
        REQUIRE(db.schema_ok());
        seed_m1(db);
        CHECK(ost_store::delete_layers(db.path(), "10", order));
        CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,0);(102,1);(103,2);(104,3);(107,4)");
    }
    TempDb db("del_batch_all");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::delete_layers(db.path(), "10", {"107", "105", "106"}));
    CHECK_STR_EQ(ost_ltest::sequences_text(db, "BidUID = 10"), "(101,0);(102,1);(103,2);(104,3)");
    CHECK_STR_EQ(layers_text(db, "BidUID = 20"), "(201,20,'Other',-1,1,0,0);(202,20,'Zebra',-1,2,0,0)");
}

// One refused uid (a locked copy, an unknown uid, another bid's layer, a shared row, a malformed uid) makes the WHOLE call refuse and nothing is
// deleted, in any position of the list (Windows breaks at the first False and commits the earlier deletes, LY-4).
TEST(LayerEditDelete, OneRefusedUidRefusesTheWholeBatch)
{
    TempDb db("del_batch_refused");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *bad : {"101", "999", "201", "8", "abc", "", "0"})
    {
        SCOPED_CONTEXT(std::string("with '") + bad + "'");
        expect_refused(db, "10", {"107", bad});
        expect_refused(db, "10", {bad, "107"});
        expect_refused(db, "10", {"105", bad, "107"});
    }
}

// An empty list is a success that writes nothing (like set_layers_visibility). A real delete first, so the no-op is judged against a changed state.
TEST(LayerEditDelete, AnEmptyListIsASuccessThatWritesNothing)
{
    TempDb db("del_empty");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 107"), 0);
    const std::string before = db.snapshot_all();
    CHECK(ost_store::delete_layers(db.path(), "10", {}));
    CHECK_STR_EQ(db.snapshot_all(), before);
}

// ---------------------------------------------------------------------------------------------------------------
// the bid
// ---------------------------------------------------------------------------------------------------------------

TEST(LayerEditDelete, AMalformedOrMissingBidIsRefused)
{
    TempDb db("del_bad_bid");
    REQUIRE(db.schema_ok());
    seed_m1(db);
    for (const char *bid : {"0", "-10", "abc", "", " 10", "10 ", "1.0", "99", "99999999999999999999"})
    {
        SCOPED_CONTEXT(std::string("bid '") + bid + "'");
        expect_refused(db, bid, {"107"});
    }
    CHECK(ost_store::delete_layers(db.path(), "010", {"107"}));
}

// plan 4.1: the lock is re-checked inside the transaction; an unresolved status is locked; another bid's lock does not matter.
TEST(LayerEditDelete, ALockedBidIsRefused)
{
    {
        TempDb db("del_locked");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        ost_ptest::seed_locked_bid(db, 10, 5);
        expect_refused(db, "10", {"107"});
        expect_refused(db, "10", {"105", "106", "107"});
        CHECK(ost_store::delete_layers(db.path(), "20", {"202"})); // bid 20 has no status: unlocked
    }
    {
        TempDb db("del_unresolved");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        ost_ptest::seed_bid_with_status(db, 10, "77");
        expect_refused(db, "10", {"107"});
    }
    {
        TempDb db("del_open");
        REQUIRE(db.schema_ok());
        seed_m1(db);
        ost_ptest::seed_open_bid(db, 10, 4);
        ost_ptest::seed_locked_bid(db, 20, 5);
        CHECK(ost_store::delete_layers(db.path(), "10", {"107"}));
    }
}

// D13: a table without UID is refused; a database that cannot be read is refused without a throw.
TEST(LayerEditDelete, AMissingColumnAndAFailedReadAreRefusals)
{
    {
        TempDb db("del_no_uid");
        REQUIRE(db.schema_ok());
        add_bids(db, {10});
        ost_ltest::rebuild_layers_without(db, "UID");
        const std::string before = db.snapshot_all();
        CHECK_FALSE(ost_store::delete_layers(db.path(), "10", {"1"}));
        CHECK_STR_EQ(db.snapshot_all(), before);
    }
    CHECK_FALSE(ost_store::delete_layers(ost_ltest::unopenable_db_path(), "10", {"107"}));
    const std::string garbage = ost_ltest::garbage_db_path("delete");
    CHECK_FALSE(ost_store::delete_layers(garbage, "10", {"107"}));
    std::remove(garbage.c_str());
}
