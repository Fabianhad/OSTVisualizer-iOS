// LP-18 (the Comments layer is not listed and not touched by Show All / Hide All), DBP-35 (Show All / Hide All are one atomic
// write) and LP-24 (a) (Show All / Hide All never write a shared row: the store skips it).
//
// Windows oracle:
//  - ost_visualizer/domain/entities/layer.py normalize_layer_name / is_comments_layer_name (str(name or "").strip().lower() ==
//    "comments"), application/services/project_read_service.py get_merged_bid_layers (merge_layers_for_bid, then every layer for
//    which is_comments_layer_name is true is dropped) and tests/application/services/test_project_read_service.py::
//    test_merged_bid_layers_hide_comments_layer_from_project_ui (the list [Default, IMAGE override, Custom] for the source
//    [Custom, "  CoMmEnTs  ", IMAGE override, Default, Image]);
//  - presentation/coordinators/ui_event_coordinator.py update_all_layers_visibility_deferred: the layers are the non-shared ones
//    of the sidebar list (no Comments), changed_layer_uids = those whose visibility differs from the requested one, and nothing
//    is written when none differs;
//  - infrastructure/mdb/components/layer_operations.py update_all_layers_show(db, bid, show, layer_uids): every uid must name a
//    layer of the bid or a locked template layer, otherwise nothing is written (any ineligible uid refuses the whole call); then
//    the uids whose BidUID is not the bid (the shared template rows) are dropped, the call succeeds without a write when none
//    remains, and the rest is one UPDATE ... WHERE [BidUID] = bid, all or nothing (the whole call runs in one _connection);
//    tests/infrastructure/mdb/components/test_layer_operations.py (test_bulk_layer_visibility_skips_a_captured_shared_layer_and_
//    leaves_uncaptured_ones, test_bulk_layer_visibility_writes_the_bids_own_locked_copies_but_no_shared_row,
//    test_bulk_layer_visibility_rejects_ineligible_uid_before_any_write).
#include "StoreTestSupport.hpp"

#include "Domain/Layer.hpp"

#include <string>
#include <unordered_map>
#include <vector>

using ost_domain::BidLayer;
using ost_domain::Layer;
using ost_ptest::TempDb;

namespace
{
    BidLayer listed(const std::string &uid, const std::string &bid_uid, const std::string &name, bool show, int sequence)
    {
        BidLayer l;
        l.uid = uid;
        l.bid_uid = bid_uid;
        l.name = name;
        l.show = show;
        l.sequence = sequence;
        return l;
    }

    Layer live(const std::string &uid, const std::string &name, bool visible, int sequence)
    {
        Layer l;
        l.uid = uid;
        l.name = name;
        l.visible = visible;
        l.sequence = sequence;
        return l;
    }

    std::string join(const std::vector<std::string> &items)
    {
        std::string out;
        for (const std::string &item : items)
            out += (out.empty() ? "" : ",") + item;
        return out;
    }

    void seed_bids(const TempDb &db)
    {
        REQUIRE(db.schema_ok());
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (10, 1, 'Ten', 10, 0)");
        db.must("INSERT INTO Bids (UID, BidProjectUID, JobName, BidNo, MeasureBase) VALUES (20, 1, 'Twenty', 20, 0)");
    }

    void add_layer(const TempDb &db, int uid, int bid, const std::string &name, int show, int sequence)
    {
        db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (" + std::to_string(uid) +
                ", " + std::to_string(bid) + ", 0, " + ost_ptest::quoted(name) + ", " + std::to_string(show) + ", 0, " +
                std::to_string(sequence) + ")");
    }

    ost_data::ProjectData load(const TempDb &db, const std::string &bid = "10")
    {
        ost_data::ProjectData project;
        std::string error;
        if (!ost_store::load_project(db.path(), bid, project, &error))
            FAIL_TEST("load failed: " + error);
        return project;
    }

    std::string names_of(const std::vector<BidLayer> &layers)
    {
        std::string out;
        for (const BidLayer &l : layers)
            out += (out.empty() ? "" : ",") + l.name;
        return out;
    }

    // A locked template layer that belongs to no bid: a shared row, listed for every bid and never written by Show All / Hide All.
    BidLayer shared_row(const std::string &uid, const std::string &name, bool show, int sequence)
    {
        BidLayer l = listed(uid, "", name, show, sequence);
        l.is_template = true;
        l.is_locked = true;
        return l;
    }

    // The uids a Show All / Hide All writes for the bid "10" (the layers of these tests belong to bid 10), joined with ','.
    std::string planned(const std::vector<BidLayer> &list, const std::unordered_map<std::string, Layer> &own, bool show)
    {
        return join(ost_domain::plan_bulk_layer_visibility(list, "10", own, show).uids);
    }

    std::string planned(const ost_data::ProjectData &project, bool show)
    {
        return join(ost_domain::plan_bulk_layer_visibility(project.merged_layers, project.bid.uid, project.layers, show).uids);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// LP-18: the Comments layer
// ---------------------------------------------------------------------------------------------------------------

// normalize_layer_name / is_comments_layer_name: strip() and lower() (Python: Unicode whitespace, Unicode lower-casing).
TEST(Lp18_CommentsLayer, TheNameIsRecognisedLikeWindows)
{
    CHECK_STR_EQ(ost_domain::normalize_layer_name("  CoMmEnTs  "), "comments");
    CHECK(ost_domain::is_comments_layer_name("Comments"));
    CHECK(ost_domain::is_comments_layer_name("comments"));
    CHECK(ost_domain::is_comments_layer_name("  CoMmEnTs  "));
    CHECK(ost_domain::is_comments_layer_name("\tCOMMENTS\n"));
    CHECK(ost_domain::is_comments_layer_name("\xC2\xA0" "Comments" "\xE2\x80\x83")); // no-break space and em space strip too
    CHECK_FALSE(ost_domain::is_comments_layer_name("Comment"));
    CHECK_FALSE(ost_domain::is_comments_layer_name("Comments 2"));
    CHECK_FALSE(ost_domain::is_comments_layer_name("My Comments"));
    CHECK_FALSE(ost_domain::is_comments_layer_name(""));
}

// Port of test_merged_bid_layers_hide_comments_layer_from_project_ui: source [Custom (own, 6), "  CoMmEnTs  " (template, 3),
// IMAGE (own, 4), Default (template, 2), Image (template, 0)] lists Default, the IMAGE override and Custom.
TEST(Lp18_CommentsLayer, TheLayerListNeverContainsTheCommentsLayer)
{
    TempDb db("lp18_windows_case");
    seed_bids(db);
    db.must("DELETE FROM BidLayers WHERE IsTemplate <> 0");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (81, NULL, -1, 'Image', -1, -1, 0)");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (82, NULL, -1, 'Default', -1, -1, 2)");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (83, NULL, -1, '  CoMmEnTs  ', -1, -1, 3)");
    add_layer(db, 85, 10, "IMAGE", -1, 4);
    add_layer(db, 86, 10, "Custom", -1, 6);
    const ost_data::ProjectData project = load(db);
    CHECK_STR_EQ(names_of(project.merged_layers), "Default,IMAGE,Custom");
}

// The bid's own Comments row is hidden from the list too (the list is the merged one), but the bid's layers keep it: conditions
// and pages may still refer to it.
TEST(Lp18_CommentsLayer, ABidsOwnCommentsRowIsNotListedButStaysAmongItsLayers)
{
    TempDb db("lp18_own_comments");
    seed_bids(db);
    db.must("DELETE FROM BidLayers WHERE IsTemplate <> 0");
    add_layer(db, 50, 10, "Default", -1, 2);
    add_layer(db, 53, 10, "Comments", -1, 3);
    add_layer(db, 54, 10, "Custom", -1, 6);
    const ost_data::ProjectData project = load(db);
    CHECK_STR_EQ(names_of(project.merged_layers), "Default,Custom");
    CHECK_EQ(project.layers.size(), static_cast<size_t>(3));
    CHECK(project.layers.count("53") == 1);
}

// ---------------------------------------------------------------------------------------------------------------
// Show All / Hide All: which layers are written
// ---------------------------------------------------------------------------------------------------------------

// The layers whose visibility differs from the requested one, in list order; nothing when none differs (Windows writes nothing
// then).
TEST(Dbp35_BulkVisibilitySelection, OnlyTheLayersThatDifferAreChanged)
{
    const std::vector<BidLayer> list = {listed("1", "10", "A", true, 0), listed("2", "10", "B", false, 1),
                                        listed("3", "10", "C", false, 2), listed("4", "10", "D", true, 3)};
    const std::unordered_map<std::string, Layer> own = {{"1", live("1", "A", true, 0)},
                                                        {"2", live("2", "B", false, 1)},
                                                        {"3", live("3", "C", false, 2)},
                                                        {"4", live("4", "D", true, 3)}};
    CHECK_STR_EQ(planned(list, own, true), "2,3");
    CHECK_STR_EQ(planned(list, own, false), "1,4");

    std::unordered_map<std::string, Layer> all_on = own;
    for (auto &entry : all_on)
        entry.second.visible = true;
    const ost_domain::BulkLayerPlan nothing = ost_domain::plan_bulk_layer_visibility(list, "10", all_on, true);
    CHECK(nothing.outcome == ost_domain::BulkLayerOutcome::AlreadyThere);
    CHECK(nothing.uids.empty());
}

// The current visibility of a layer is the live one (a toggle made after the load), not the Show of the list entry.
TEST(Dbp35_BulkVisibilitySelection, TheLiveVisibilityDecidesNotTheLoadedShow)
{
    const std::vector<BidLayer> list = {listed("1", "10", "A", true, 0), listed("2", "10", "B", true, 1)};
    const std::unordered_map<std::string, Layer> own = {{"1", live("1", "A", false, 0)}, {"2", live("2", "B", true, 1)}};
    CHECK_STR_EQ(planned(list, own, true), "1");
    CHECK_STR_EQ(planned(list, own, false), "2");
}

// LP-18: the Comments layer is not in the list, so it is never changed even though the bid has such a row; a layer that is only
// listed (a locked template layer of no bid: a shared row, LP-24 (a)) and a layer of the bid that is not listed (merged away)
// are left alone too.
TEST(Dbp35_BulkVisibilitySelection, CommentsTemplateOnlyAndMergedAwayLayersAreLeftAlone)
{
    const std::vector<BidLayer> list = {listed("1", "10", "Default", true, 0), shared_row("7", "Template only", true, 1)};
    const std::unordered_map<std::string, Layer> own = {{"1", live("1", "Default", true, 0)},
                                                        {"3", live("3", "Comments", true, 3)},
                                                        {"5", live("5", "merged away", true, 4)}};
    CHECK_STR_EQ(planned(list, own, false), "1");
}

// End to end on a loaded bid: Hide All selects the bid's own layers of the list and never the Comments row.
TEST(Dbp35_BulkVisibilitySelection, AProjectsHideAllLeavesTheCommentsRowAlone)
{
    TempDb db("lp18_hide_all");
    seed_bids(db);
    db.must("DELETE FROM BidLayers WHERE IsTemplate <> 0");
    add_layer(db, 50, 10, "Default", -1, 2);
    add_layer(db, 51, 10, "Image", -1, 0);
    add_layer(db, 53, 10, "Comments", -1, 3);
    add_layer(db, 54, 10, "Custom", 0, 6);
    const ost_data::ProjectData project = load(db);
    CHECK_STR_EQ(planned(project, false), "51,50"); // list order (Image 0, Default 2); Custom is hidden already, Comments is never listed
    CHECK_STR_EQ(planned(project, true), "54");
}

// ---------------------------------------------------------------------------------------------------------------
// DBP-35: Show All / Hide All are one atomic write
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    void seed_layers(const TempDb &db)
    {
        seed_bids(db);
        add_layer(db, 50, 10, "A", -1, 1);
        add_layer(db, 51, 10, "B", -1, 2);
        add_layer(db, 52, 10, "C", -1, 3);
        add_layer(db, 60, 20, "Other bid", -1, 1);
    }
}

// The layers are written together: all of them change and the other rows are untouched.
TEST(Dbp35_SetLayersVisibility, ListedLayersAreAllWritten)
{
    TempDb db("bulk_write");
    seed_layers(db);
    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"50", "52"}, false));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 50 AND Show = 0"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 51 AND Show = -1"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 52 AND Show = 0"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 60 AND Show = -1"), 1);
    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"50", "52"}, true));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 50 AND Show = -1"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 52 AND Show = -1"), 1);
}

// DBP-35 test plan: a failure on the second row leaves the first one unwritten (a BEFORE UPDATE trigger aborts the update of
// layer 51); without the trigger the same call writes both.
TEST(Dbp35_SetLayersVisibility, AFailureOnTheSecondRowRollsBackTheFirst)
{
    TempDb db("bulk_atomic");
    seed_layers(db);
    db.must("CREATE TRIGGER boom BEFORE UPDATE ON BidLayers WHEN NEW.UID = 51 BEGIN SELECT RAISE(ABORT, 'boom'); END");
    const std::string before = db.snapshot({"BidLayers"});
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"50", "51", "52"}, false));
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
    db.must("DROP TRIGGER boom");
    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"50", "51", "52"}, false));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE BidUID = 10 AND Show = 0"), 3);
}

// Windows update_all_layers_show: every uid must name a layer of the bid (or a locked template layer), otherwise NOTHING is
// written: a layer of another bid, a missing one, a malformed uid, an unlocked template layer.
TEST(Dbp35_SetLayersVisibility, AnyRefusedUidRefusesTheWholeBatch)
{
    TempDb db("bulk_refused");
    seed_layers(db);
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (80, NULL, -1, 'Unlocked template', -1, 0, 9)");
    const std::string before = db.snapshot({"BidLayers"});
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"50", "60"}, false));   // another bid's layer
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"50", "9999"}, false)); // missing
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"50", "abc"}, false));  // malformed
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"50", ""}, false));
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"50", "80"}, false));   // template layer that is not locked
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
}

// The bid must exist (require_existing_unique_bid_owned_uid_matches(Bids)), even for a list that is empty.
TEST(Dbp35_SetLayersVisibility, TheBidMustExist)
{
    TempDb db("bulk_bid");
    seed_layers(db);
    const std::string before = db.snapshot({"BidLayers"});
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "99", {"50"}, false));
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "99", {}, false));
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "abc", {"50"}, false));
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
}

// An empty list is a success that writes nothing (`if not normalized_uids: return True`); a uid listed twice counts once.
TEST(Dbp35_SetLayersVisibility, AnEmptyListSucceedsAndRepeatedUidsCountOnce)
{
    TempDb db("bulk_empty");
    seed_layers(db);
    const std::string before = db.snapshot({"BidLayers"});
    CHECK(ost_store::set_layers_visibility(db.path(), "10", {}, false));
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"50", "50", "50"}, false));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 50 AND Show = 0"), 1);
}

// LP-24 (a): a locked template layer is an eligible uid of every bid's batch (BidUID = ? OR (IsTemplate AND IsLocked), so the call
// is not refused) but it is a shared row, and Show All / Hide All never write one: only the rows with BidUID = bid are updated.
// create_schema's four template layers are 1 to 4 (BidUID NULL, shown).
TEST(Dbp35_SetLayersVisibility, SharedTemplateRowsAreSkipped)
{
    TempDb db("bulk_template");
    seed_layers(db);
    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"1", "50"}, false));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 1 AND Show = -1"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 50 AND Show = 0"), 1);
}

// Windows test_bulk_layer_visibility_skips_a_captured_shared_layer_and_leaves_uncaptured_ones (bid 7 there, 10 here): rows
// (100, no bid, shared), (101, bid), (102, bid), (201, other bid); [100, 101] hides 101 only, [100] alone succeeds and changes
// nothing.
TEST(Dbp35_SetLayersVisibility, OnlySharedUidsSucceedAndWriteNothing)
{
    TempDb db("bulk_only_shared");
    seed_bids(db);
    db.must("DELETE FROM BidLayers");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (100, NULL, -1, 'Shared', -1, -1, 0)");
    add_layer(db, 101, 10, "Captured", -1, 1);
    add_layer(db, 102, 10, "Uncaptured", -1, 2);
    add_layer(db, 201, 20, "Other bid", -1, 1);

    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"100", "101"}, false));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || '=' || Show FROM BidLayers ORDER BY UID"), "100=-1,101=0,102=-1,201=-1");

    const std::string before = db.snapshot({"BidLayers"});
    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"100"}, false));
    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"100"}, true));
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
}

// Windows test_bulk_layer_visibility_writes_the_bids_own_locked_copies_but_no_shared_row (bid 7 there, 10 here): shared 1 and 2,
// the bid's own locked copy 13, its plain layer 14 and another bid's locked copy 23. Hide All of all five hides 13 and 14 only;
// Show All of the bid's two brings them back.
TEST(Dbp35_SetLayersVisibility, OwnLockedCopiesAreWrittenSharedRowsAreNot)
{
    TempDb db("bulk_own_copies");
    seed_bids(db);
    db.must("DELETE FROM BidLayers");
    const char *rows[] = {"(1, NULL, -1, 'Image', -1, -1, 0)", "(2, NULL, -1, 'Default', -1, -1, 1)",
                          "(13, 10, -1, 'Image', -1, -1, 0)",  "(14, 10, 0, 'Walls', -1, 0, 2)",
                          "(23, 20, -1, 'Image', -1, -1, 0)"};
    for (const char *row : rows)
        db.must(std::string("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES ") + row);

    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"1", "2", "13", "14", "23"}, false));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || '=' || Show FROM BidLayers ORDER BY UID"), "1=-1,2=-1,13=0,14=0,23=-1");
    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"13", "14"}, true));
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || '=' || Show FROM BidLayers ORDER BY UID"), "1=-1,2=-1,13=-1,14=-1,23=-1");
}

// Another bid's locked template row is an eligible uid (a locked template layer of any bid is), so it is skipped, not refused:
// the call succeeds, the bid's own layer changes and the row of bid 20 stays.
TEST(Dbp35_SetLayersVisibility, AnotherBidsLockedTemplateRowIsSkippedNotRefused)
{
    TempDb db("bulk_other_locked");
    seed_layers(db);
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (61, 20, -1, 'Locked copy', -1, -1, 5)");
    CHECK(ost_store::set_layers_visibility(db.path(), "10", {"50", "61"}, false));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 50 AND Show = 0"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE UID = 61 AND Show = -1"), 1);
}

// The eligibility check still runs over every uid first: one uid that is neither the bid's nor a locked template row (a plain
// layer of another bid, an unlocked template row, a missing one) refuses the whole call even when the other uids are shared
// rows or the bid's own, and nothing is written.
TEST(Dbp35_SetLayersVisibility, IneligibleUidStillRefusesTheWholeBatchEvenWithSharedUids)
{
    TempDb db("bulk_ineligible_shared");
    seed_layers(db);
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (80, NULL, -1, 'Unlocked template', -1, 0, 9)");
    const std::string before = db.snapshot({"BidLayers"});
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"1", "50", "60"}, false));   // another bid's plain layer
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"1", "80"}, false));         // unlocked template row
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"1", "2", "9999"}, false));  // missing
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"1", "2", "60"}, true));
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
}

// The eligibility filter is "BidUID = ? OR (IsTemplate <> 0 AND IsLocked <> 0)" (layer_operations.py update_all_layers_show :301 with
// template_filter :274): IsLocked alone does not make a row eligible. A locked row that is no template row and is not the bid's own
// (BidUID NULL, or another bid's) refuses the whole call, as Windows' eligible_uid_rows != chunk does, and nothing is written - the
// own row 50 in the same batch stays shown. Since the call drops the eligible rows that are not the bid's own instead of writing
// them, a filter that let such a row through would turn this refusal into a silent success.
TEST(Dbp35_SetLayersVisibility, ALockedRowThatIsNoTemplateRowIsIneligibleAndRefusesTheBatch)
{
    TempDb db("bulk_locked_plain");
    seed_layers(db);
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (81, NULL, 0, 'Locked plain', -1, -1, 9)");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (82, 20, 0, 'Locked plain other bid', -1, -1, 9)");
    const std::string before = db.snapshot({"BidLayers"});
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"50", "81"}, false)); // locked, no bid, not a template row
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"50", "82"}, false)); // locked, another bid's, not a template row
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"81"}, true));
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"82"}, true));
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
}

// A statement failure of the update itself (read-only table) fails the call and keeps the old values.
TEST(Dbp35_SetLayersVisibility, ANonWritableTableFailsTheCall)
{
    TempDb db("bulk_blocked");
    seed_layers(db);
    db.must("CREATE TRIGGER blocked BEFORE UPDATE ON BidLayers BEGIN SELECT RAISE(ABORT, 'blocked'); END");
    const std::string before = db.snapshot({"BidLayers"});
    CHECK_FALSE(ost_store::set_layers_visibility(db.path(), "10", {"50"}, false));
    CHECK_EQ(db.snapshot({"BidLayers"}), before);
}
