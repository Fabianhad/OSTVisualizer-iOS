// LP-24 (user decision: port merge_layers_for_bid): the layer list shows the bid's own layers plus the locked template layers,
// merged by lower-cased name with the bid's layer winning, sorted by Sequence only with stable ties (database order).
// Windows oracle: ost_visualizer/domain/entities/layer.py::merge_layers_for_bid, infrastructure/mdb/components/bid_data_reader.py
// _parse_bid_layers_for_sidebar (WHERE BidUID = ? OR (IsTemplate <> 0 AND IsLocked <> 0), no ORDER BY) and
// application/services/project_read_service.py::get_merged_bid_layers (a failure logs "Failed to load bid layers" and lists no
// layers), tests/domain/entities/test_layer.py (MergeLayersForBidTests, LayerEntityContractTests). LayerMergeOracleData.hpp holds
// the outcome of the real merge_layers_for_bid on 178 layer lists.
// "Database order" of the rows that tie is the order of their UIDs (the query reads ORDER BY UID: Access has no defined order).
#include "StoreTestSupport.hpp"

#include "LayerMergeOracleData.hpp"

#include "Domain/CoreLog.hpp"
#include "Domain/Layer.hpp"

#include <string>
#include <vector>

using ost_domain::BidLayer;
using ost_ptest::TempDb;

namespace
{
    BidLayer layer(const std::string &uid, const std::string &bid_uid, const std::string &name, bool show, int sequence,
                   bool is_template = false, bool is_locked = false)
    {
        BidLayer l;
        l.uid = uid;
        l.bid_uid = bid_uid;
        l.name = name;
        l.show = show;
        l.sequence = sequence;
        l.is_template = is_template;
        l.is_locked = is_locked;
        return l;
    }

    std::string uids_of(const std::vector<BidLayer> &layers)
    {
        std::string out;
        for (const BidLayer &l : layers)
            out += (out.empty() ? "" : ",") + l.uid;
        return out;
    }

    // "uid|bid|name|show|sequence|template|locked;..." (the oracle format) -> layers
    std::vector<BidLayer> parse_layers(const std::string &text)
    {
        std::vector<BidLayer> layers;
        size_t start = 0;
        while (start <= text.size())
        {
            size_t end = text.find(';', start);
            if (end == std::string::npos)
                end = text.size();
            const std::string part = text.substr(start, end - start);
            std::vector<std::string> field;
            size_t f = 0;
            while (true)
            {
                const size_t bar = part.find('|', f);
                if (bar == std::string::npos)
                {
                    field.push_back(part.substr(f));
                    break;
                }
                field.push_back(part.substr(f, bar - f));
                f = bar + 1;
            }
            REQUIRE_EQ(field.size(), static_cast<size_t>(7));
            layers.push_back(layer(field[0], field[1], field[2], field[3] == "1", std::stoi(field[4]), field[5] == "1", field[6] == "1"));
            start = end + 1;
        }
        return layers;
    }

    // Bid 10 and 20 in the 'Deleted Bids' project; the four template layers of create_schema are BidLayers 1..4:
    // Default (sequence 2), Annotation (1), Image (0), Comments (3), all IsTemplate and IsLocked, BidUID NULL.
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
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// merge_layers_for_bid
// ---------------------------------------------------------------------------------------------------------------

// Port of MergeLayersForBidTests::test_bid_override_wins_over_template_and_retains_original_object and
// test_empty_layers_produce_empty_result.
TEST(Lp24_MergeLayers, ABidLayerOverridesTheTemplateOfTheSameName)
{
    const BidLayer template_layer = layer("template", "0", "Annotation", true, 1, true);
    const BidLayer own = layer("own", "bid", "annotation", false, 3);
    const BidLayer image = layer("image", "0", "Image", true, 2, true);
    const std::vector<BidLayer> inputs = {own, template_layer, image};
    const std::vector<BidLayer> merged = ost_domain::merge_layers_for_bid(inputs);
    REQUIRE_EQ(merged.size(), static_cast<size_t>(2));
    CHECK_STR_EQ(merged[0].uid, "image");
    CHECK_STR_EQ(merged[1].uid, "own");
    CHECK_FALSE(merged[1].show); // the bid's row with all its fields, not the template's
    CHECK_EQ(merged[1].sequence, 3);
    CHECK_STR_EQ(uids_of(inputs), "own,template,image"); // the input is left alone
    CHECK(template_layer.show);

    CHECK(ost_domain::merge_layers_for_bid({}).empty());
}

// Port of LayerEntityContractTests::test_merge_prefers_the_later_sequence_for_equal_non_template_names.
TEST(Lp24_MergeLayers, OfSeveralLayersWithOneNameTheLaterSequenceWins)
{
    const BidLayer first = layer("a", "bid", "Same", true, 1);
    const BidLayer second = layer("b", "bid", "same", false, 2);
    const BidLayer template_layer = layer("t", "0", "SAME", true, 0, true);
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({second, template_layer, first})), "b");

    const std::vector<BidLayer> sorted_output = ost_domain::merge_layers_for_bid(
        {layer("x", "bid", "X", true, 9), layer("y", "bid", "Y", true, 3), layer("z", "0", "Z", true, 5, true)});
    CHECK_STR_EQ(uids_of(sorted_output), "y,z,x");
}

// Port of ...::test_a_bid_layer_beats_a_same_named_template_whatever_the_sequences_or_input_order and
// test_the_later_sequence_wins_for_equal_names_whatever_the_uid_order.
TEST(Lp24_MergeLayers, TheWinnerDoesNotDependOnTheInputOrder)
{
    const BidLayer template_layer = layer("t", "0", "X", true, 5, true);
    const BidLayer own = layer("o", "bid", "x", false, 1);
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({template_layer, own})), "o");
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({own, template_layer})), "o");

    const BidLayer low = layer("z", "bid", "same", true, 9);
    const BidLayer high = layer("a", "bid", "Same", false, 10);
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({low, high})), "a");
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({high, low})), "a");
}

// The item's test plan: layers a (sequence 1) and b (sequence 1): Windows keeps the database order within the tie, the old
// iOS rule sorted them by name. Here the first row is the one named "b".
TEST(Lp24_MergeLayers, EqualSequencesKeepTheOrderOfTheRows)
{
    const BidLayer b = layer("70", "7", "b", true, 1);
    const BidLayer a = layer("71", "7", "a", true, 1);
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({b, a})), "70,71");
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({a, b})), "71,70");
}

// A key keeps the place of its FIRST appearance (a Python dict: reassigning a key keeps its position), then the stable sort by
// sequence breaks ties by that place: the template 'Default' (sequence 5, listed first) is replaced by the bid's 'Default' and
// stays ahead of 'Zed' (also sequence 5).
TEST(Lp24_MergeLayers, AReplacedLayerKeepsThePlaceOfTheOneItReplaced)
{
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({layer("1", "0", "Default", true, 5, true, true),
                                                           layer("2", "0", "Zed", true, 5, true, true),
                                                           layer("50", "7", "Default", true, 5)})),
                 "50,2");
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({layer("1", "0", "Zed", true, 5, true, true),
                                                           layer("2", "0", "Default", true, 5, true, true),
                                                           layer("50", "7", "Default", true, 5)})),
                 "1,50");
}

// Python str.lower() decides which names are equal: full Unicode lower-casing, not ASCII.
TEST(Lp24_MergeLayers, NamesAreComparedWithPythonLower)
{
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({layer("1", "7", "\xC3\x9C" "n\xC3\xAF", true, 1), layer("2", "7", "\xC3\xBCN\xC3\x8F", true, 2)})), "2");
    // 'STRASSE' is not 'Straße' for lower() (only casefold() folds the sharp s): two layers
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({layer("1", "7", "STRASSE", true, 1), layer("2", "7", "Stra\xC3\x9F" "e", true, 2)})),
                 "1,2");
    // no strip(): a padded name is another name
    CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid({layer("1", "7", "pad", true, 1), layer("2", "7", " pad", true, 2)})), "1,2");
}

// 178 lists: hand written ones (the Windows tests, the tie example, a bid with / without its template copies, replaced
// positions) and 160 random ones with mixed-case, accented, sigma and dotted-i names.
TEST(Lp24_MergeLayers, EveryWindowsOracleCaseGivesTheWindowsList)
{
    int index = 0;
    for (const ost_ptest::LayerMergeCase &c : ost_ptest::kLayerMergeOracleCases)
    {
        SCOPED_CONTEXT("oracle case " + std::to_string(index++) + ": " + c.layers);
        CHECK_STR_EQ(uids_of(ost_domain::merge_layers_for_bid(parse_layers(c.layers))), c.expected);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// load_project: the layer list
// ---------------------------------------------------------------------------------------------------------------

// Test plan: a bid without its own 'Default' row but with the template one: the template layer is listed.
TEST(Lp24_LoadedLayerList, ABidWithoutItsOwnRowsListsTheTemplateLayers)
{
    TempDb db("lp24_templates_only");
    seed_bids(db);
    const ost_data::ProjectData project = load(db);
    // Image (0), Annotation (1), Default (2); the Comments template layer (3) is never listed (LP-18)
    CHECK_STR_EQ(uids_of(project.merged_layers), "3,2,1");
    REQUIRE_EQ(project.merged_layers.size(), static_cast<size_t>(3));
    CHECK_STR_EQ(project.merged_layers[2].name, "Default");
    CHECK(project.merged_layers[2].is_template);
    CHECK(project.merged_layers[2].is_locked);
    CHECK(project.merged_layers[2].show); // Show = -1
    CHECK_STR_EQ(project.merged_layers[2].bid_uid, "");
    CHECK_EQ(project.merged_layers[2].sequence, 2);
    // the bid's own layers (what its conditions and pages refer to) stay the BidUID rows only
    CHECK(project.layers.empty());
}

// A bid that copied the four templates (what create_bid does): its rows win by name and the list is in sequence order.
TEST(Lp24_LoadedLayerList, TheBidsCopiesOfTheTemplatesWin)
{
    TempDb db("lp24_copies");
    seed_bids(db);
    add_layer(db, 50, 10, "Default", -1, 2);
    add_layer(db, 51, 10, "Annotation", 0, 1);
    add_layer(db, 52, 10, "Image", -1, 0);
    add_layer(db, 53, 10, "Comments", -1, 3);
    const ost_data::ProjectData project = load(db);
    // the bid's own Comments row (53) is not listed either (LP-18) but stays one of the bid's layers
    CHECK_STR_EQ(uids_of(project.merged_layers), "52,51,50");
    REQUIRE_EQ(project.merged_layers.size(), static_cast<size_t>(3));
    CHECK_STR_EQ(project.merged_layers[1].bid_uid, "10");
    CHECK_FALSE(project.merged_layers[1].show); // Annotation of the bid: Show = 0
    CHECK_FALSE(project.merged_layers[1].is_template);
    CHECK_EQ(project.layers.size(), static_cast<size_t>(4));
}

// Mixed: only 'Annotation' and 'Image' are the bid's own rows, 'Default' and 'Comments' come from the templates.
TEST(Lp24_LoadedLayerList, OwnRowsAndTemplateLayersAreMerged)
{
    TempDb db("lp24_mixed");
    seed_bids(db);
    add_layer(db, 50, 10, "annotation", 0, 1);
    add_layer(db, 52, 10, "IMAGE", -1, 0);
    const ost_data::ProjectData project = load(db);
    CHECK_STR_EQ(uids_of(project.merged_layers), "52,50,1"); // the 'Comments' template (4) is not listed (LP-18)
}

// Test plan: layers a (sequence 1), b (sequence 1) - the list keeps the row (UID) order of the tie, it is not sorted by name.
TEST(Lp24_LoadedLayerList, EqualSequencesKeepTheDatabaseOrder)
{
    TempDb db("lp24_ties");
    seed_bids(db);
    db.must("DELETE FROM BidLayers WHERE IsTemplate <> 0");
    add_layer(db, 70, 10, "b", -1, 1);
    add_layer(db, 71, 10, "a", -1, 1);
    const ost_data::ProjectData project = load(db);
    CHECK_STR_EQ(uids_of(project.merged_layers), "70,71");
}

// Case-insensitive duplicates of the bid's own rows are merged in the list (the later sequence wins) but both stay in layers
// (a condition may refer to either UID).
TEST(Lp24_LoadedLayerList, DuplicateNamesAreOneEntryOfTheList)
{
    TempDb db("lp24_duplicates");
    seed_bids(db);
    db.must("DELETE FROM BidLayers WHERE IsTemplate <> 0");
    add_layer(db, 60, 10, "Floors", -1, 1);
    add_layer(db, 61, 10, "floors", 0, 2);
    const ost_data::ProjectData project = load(db);
    CHECK_STR_EQ(uids_of(project.merged_layers), "61");
    CHECK_EQ(project.layers.size(), static_cast<size_t>(2));
}

// Only LOCKED TEMPLATE layers join the list: an unlocked template, a locked layer that is no template and the layers of another
// bid are not listed (WHERE BidUID = ? OR (IsTemplate <> 0 AND IsLocked <> 0)).
TEST(Lp24_LoadedLayerList, OnlyLockedTemplateLayersAndTheBidsOwnAreListed)
{
    TempDb db("lp24_filter");
    seed_bids(db);
    db.must("DELETE FROM BidLayers WHERE IsTemplate <> 0");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (80, NULL, -1, 'Locked template', -1, -1, 1)");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (81, NULL, -1, 'Unlocked template', -1, 0, 2)");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (82, NULL, 0, 'Locked not template', -1, -1, 3)");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (83, NULL, NULL, 'Null flags', -1, NULL, 4)");
    add_layer(db, 84, 20, "Of the other bid", -1, 5);
    add_layer(db, 85, 10, "Own", -1, 6);
    const ost_data::ProjectData project = load(db);
    CHECK_STR_EQ(uids_of(project.merged_layers), "80,85");
    CHECK_EQ(project.layers.size(), static_cast<size_t>(1));
}

// Show is -1 or 1 for visible (Windows row.Show in (1, -1)); anything else hides.
TEST(Lp24_LoadedLayerList, ShowIsReadLikeWindows)
{
    TempDb db("lp24_show");
    seed_bids(db);
    db.must("DELETE FROM BidLayers WHERE IsTemplate <> 0");
    add_layer(db, 60, 10, "minus one", -1, 1);
    add_layer(db, 61, 10, "one", 1, 2);
    add_layer(db, 62, 10, "zero", 0, 3);
    add_layer(db, 63, 10, "two", 2, 4);
    const ost_data::ProjectData project = load(db);
    REQUIRE_EQ(project.merged_layers.size(), static_cast<size_t>(4));
    CHECK(project.merged_layers[0].show);
    CHECK(project.merged_layers[1].show);
    CHECK_FALSE(project.merged_layers[2].show);
    CHECK_FALSE(project.merged_layers[3].show);
}

// Windows get_merged_bid_layers: when the list cannot be read (a UID problem of a template row) it logs "Failed to load bid layers"
// and lists NO layers; the bid itself still opens (only the bid's own rows fail the open, DBP-31).
TEST(Lp24_LoadedLayerList, AnUnreadableListIsEmptyAndLogsAWarningButTheBidOpens)
{
    TempDb db("lp24_unreadable");
    seed_bids(db);
    db.must("ALTER TABLE BidLayers RENAME TO BidLayers_pk");
    db.must("CREATE TABLE BidLayers AS SELECT * FROM BidLayers_pk");
    db.must("DROP TABLE BidLayers_pk");
    db.must("INSERT INTO BidLayers (UID, BidUID, IsTemplate, Name, Show, IsLocked, Sequence) VALUES (1, NULL, -1, 'Duplicate template uid', -1, -1, 9)");
    add_layer(db, 60, 10, "Own", -1, 1);

    std::vector<std::string> warnings;
    ost_log::set_sink([&warnings](ost_log::Level level, const std::string &message, const std::string &)
                      {
                          if (level == ost_log::Level::Warning)
                              warnings.push_back(message);
                      });
    const ost_data::ProjectData project = load(db);
    ost_log::set_sink({});

    CHECK(project.merged_layers.empty());
    CHECK_EQ(project.layers.size(), static_cast<size_t>(1));
    REQUIRE_EQ(warnings.size(), static_cast<size_t>(1));
    CHECK_STR_EQ(warnings[0], "Failed to load bid layers");
}

// A database without the BidLayers table has no layer list (Windows: optional table).
TEST(Lp24_LoadedLayerList, WithoutTheLayerTableTheListIsEmpty)
{
    TempDb db("lp24_no_table");
    seed_bids(db);
    db.must("DROP TABLE BidLayers");
    const ost_data::ProjectData project = load(db);
    CHECK(project.merged_layers.empty());
    CHECK(project.layers.empty());
}
