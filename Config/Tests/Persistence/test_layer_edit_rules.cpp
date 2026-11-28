// M1 layers (plan 5.3), the RULES: ost_domain::LayerEdit (Core/Domain/LayerEdit.hpp): the name check, the New Layer position, which rows can
// be edited, the move plan, the delete plan with its Windows texts, and the 32-bit guard. Pure Core, no database.
//
// Windows oracle (49a2d59). The UI rules live only in the Windows dialogs, so the ports are the tests of the dialogs and the sidebar, with
// the same inputs and expected texts:
//   tests/presentation/dialogs/test_layers_dialog.py  :265 (rename rejects duplicate and empty names), :818 (a built-in uid is an ordinary
//       bid layer), :824 (a mixed selection sends only the deletable layers)
//   tests/presentation/components/test_layers_sidebar.py  :222 (the first layer, "  First  " is stored "First", after_sequence 0), :248
//       (a new layer goes after the selected layer's sequence, else after the highest; blank and a duplicate are refused), :316 (rename trims,
//       blank reverts), :334 (duplicate warns, locked / template rows are not editable, an equal name is ignored)
//   tests/integration/visibility/test_layer_delete_usage.py :38 and tests/integration/conditions/test_nested_layer_editor.py :74 (the usage
//       set decides at action time: in use = a warning without a question, unused = the question)
// Executed read-only with python -B -I (the real Windows functions, nothing written into the Windows tree):
//   ost_visualizer/domain/entities/layer.py normalize_layer_name / is_comments_layer_name on the strings of kNameOracle / kCommentsOracle;
//   ost_visualizer/presentation/utils/messagebox.py confirm_multi_delete with QMessageBox.question / warning replaced by recorders (the texts
//   and the returned rows of kDeleteOracle, messagebox.py:78-119).
// Deliberate differences from Windows (plan section 8, N-M1-13 / N-M1-19 / N-M1-20), each pinned by a test named for it: "comments" is always
// taken (LY-1), at most 50 UTF-16 units (LY-8), no control character or line break (LY-14), a shared neighbour disables a move (LY-6).
#include "LayerEditTestSupport.hpp"

#include "Domain/Layer.hpp"
#include "Domain/LayerEdit.hpp"

#include <climits>
#include <string>
#include <unordered_map>
#include <vector>

using ost_domain::BidLayer;
using ost_domain::Layer;
using ost_domain::LayerNameCheck;
using ost_domain::LayerNameVerdict;
using ost_ltest::contains;
using ost_ltest::kEmoji;
using ost_ltest::repeat;

namespace
{
    std::string verdict_text(LayerNameVerdict verdict)
    {
        switch (verdict)
        {
        case LayerNameVerdict::Accept:
            return "Accept";
        case LayerNameVerdict::Blank:
            return "Blank";
        case LayerNameVerdict::Unchanged:
            return "Unchanged";
        case LayerNameVerdict::Duplicate:
            return "Duplicate";
        case LayerNameVerdict::Reserved:
            return "Reserved";
        case LayerNameVerdict::TooLong:
            return "TooLong";
        case LayerNameVerdict::ControlCharacter:
            return "ControlCharacter";
        }
        return "?";
    }

    // The four fields of a LayerNameCheck in one line each, so that a failure points into the test.
#define EXPECT_CHECK(check_, verdict_, name_, title_, message_)             \
    do                                                                      \
    {                                                                       \
        const LayerNameCheck &ost_c_ = (check_);                            \
        CHECK_STR_EQ(verdict_text(ost_c_.verdict), (verdict_));             \
        CHECK_STR_EQ(ost_c_.name, (name_));                                 \
        CHECK_STR_EQ(ost_c_.title, (title_));                               \
        CHECK_STR_EQ(ost_c_.message, (message_));                           \
    } while (0)

    // A row of the layer list (Windows BidLayer(uid, bid_uid, name, show, sequence, is_template, is_locked)).
    BidLayer row(const std::string &uid, const std::string &name, int sequence, const std::string &bid = "bid-1", bool is_template = false,
                 bool is_locked = false)
    {
        BidLayer layer;
        layer.uid = uid;
        layer.bid_uid = bid;
        layer.name = name;
        layer.show = true;
        layer.sequence = sequence;
        layer.is_template = is_template;
        layer.is_locked = is_locked;
        return layer;
    }

    // A locked template row of no bid: a shared row (Windows _default_layer of test_layers_dialog.py:100).
    BidLayer shared(const std::string &uid, const std::string &name, int sequence)
    {
        return row(uid, name, sequence, "", true, true);
    }

    Layer own(const std::string &uid, const std::string &name, int sequence)
    {
        Layer layer;
        layer.uid = uid;
        layer.name = name;
        layer.visible = true;
        layer.sequence = sequence;
        return layer;
    }

    // test_layers_dialog.py:89-98 / test_layers_sidebar.py: layer-1 "Layer 1" (1) and layer-2 "Layer 2" (2) of bid-1.
    std::vector<BidLayer> two_layers()
    {
        return {row("layer-1", "Layer 1", 1), row("layer-2", "Layer 2", 2)};
    }

    const std::unordered_map<std::string, Layer> kNoOwn;

    LayerNameCheck create(const std::string &typed, const std::vector<BidLayer> &listed,
                          const std::unordered_map<std::string, Layer> &own_layers = kNoOwn)
    {
        return ost_domain::check_layer_name(typed, "", listed, own_layers, "");
    }

    LayerNameCheck rename(const std::string &typed, const std::string &stored, const std::string &uid, const std::vector<BidLayer> &listed,
                          const std::unordered_map<std::string, Layer> &own_layers = kNoOwn)
    {
        return ost_domain::check_layer_name(typed, stored, listed, own_layers, uid);
    }

    std::string unhex(const std::string &hex)
    {
        std::string out;
        for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
            out += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
        return out;
    }

    // The M1 device-pack list (plan 6.3) as the sheet shows it, in list order: three locked own copies, three own layers, one shared row.
    std::vector<BidLayer> sheet()
    {
        return {row("101", "Image", 0, "10", false, true), row("102", "Annotation", 1, "10", false, true),
                row("103", "Default", 2, "10", false, true), row("105", "Walls", 4, "10"),
                row("106", "Floors", 5, "10"),               row("107", "Unused", 6, "10"),
                shared("8", "Shared Template", 7)};
    }

    std::string join(const std::vector<ost_domain::LayerRef> &refs)
    {
        std::string out;
        for (const ost_domain::LayerRef &ref : refs)
            out += (out.empty() ? "" : ",") + ref.uid;
        return out;
    }

    std::string kind_text(ost_domain::LayerDeleteKind kind)
    {
        switch (kind)
        {
        case ost_domain::LayerDeleteKind::NothingToDelete:
            return "NothingToDelete";
        case ost_domain::LayerDeleteKind::OnlyInUse:
            return "OnlyInUse";
        case ost_domain::LayerDeleteKind::ConfirmMixed:
            return "ConfirmMixed";
        case ost_domain::LayerDeleteKind::Confirm:
            return "Confirm";
        }
        return "?";
    }
}

// ---------------------------------------------------------------------------------------------------------------
// texts and constants
// ---------------------------------------------------------------------------------------------------------------

// The texts of section 2.11 of the research map, exact, with their Windows lines.
TEST(LayerEditTexts, TheWindowsTextsAreExact)
{
    CHECK_STR_EQ(ost_domain::kDuplicateLayerTitle, "Duplicate Layer");           // layers_dialog.py:405, :435; layers_sidebar.py:338, :350
    CHECK_STR_EQ(ost_domain::duplicate_layer_message("layer 1"), "Layer layer 1 already exists."); // test_layers_dialog.py:265
    CHECK_STR_EQ(ost_domain::duplicate_layer_message("Walls"), "Layer Walls already exists.");
    CHECK_STR_EQ(ost_domain::duplicate_layer_message("He said \"hi\""), "Layer He said \"hi\" already exists.");
    CHECK_STR_EQ(ost_domain::kNewLayerTitle, "New Layer");                       // layers_dialog.py:469, :473
    CHECK_STR_EQ(ost_domain::kNewLayerFailed, "Failed to create layer.");        // layers_dialog.py:473
    CHECK_STR_EQ(ost_domain::kRenameLayerTitle, "Rename Layer");                 // layers_dialog.py:427
    CHECK_STR_EQ(ost_domain::kRenameLayerFailed, "Failed to rename layer.");     // layers_dialog.py:417
    CHECK_STR_EQ(ost_domain::kMoveLayerTitle, "Move Layer");                     // layers_dialog.py:654
    CHECK_STR_EQ(ost_domain::kMoveLayerFailed, "Failed to move layer.");         // layers_dialog.py:646
    CHECK_STR_EQ(ost_domain::kDeleteLayerTitle, "Delete Layer");                 // layers_dialog.py:506, :508, :542, :544
    CHECK_STR_EQ(ost_domain::kDeleteLayerFailed, "Failed to delete layer.");     // layers_dialog.py:544
    CHECK_STR_EQ(ost_domain::kLayerUsageUnreadable, "Failed to validate layer usage."); // layers_dialog.py:506
    CHECK_STR_EQ(ost_domain::kLayersDeletedRefreshFailed, "Layers were deleted, but the refresh failed."); // layers_dialog.py:540
    CHECK_STR_EQ(ost_domain::kRefreshErrorTitle, "Refresh Error");               // ui_event_coordinator.py:6584
    CHECK_STR_EQ(ost_domain::kLayerCreatedRefreshFailed,
                 "The layer was created, but the layer list could not be refreshed. Reopen the database to see the new layer."); // :6585-6587
}

// The Access limits (database_creator.py:306-314: UID COUNTER, Sequence INTEGER, Name VARCHAR(50)); the iOS texts of the refusals.
TEST(LayerEditTexts, TheLimitsAreTheAccessLongAndFiftyUnits)
{
    CHECK_EQ(ost_domain::kMaxLayerSequence, 2147483647LL);
    CHECK_EQ(ost_domain::kMinLayerSequence, -2147483648LL);
    CHECK_EQ(ost_domain::kMaxLayerUid, 2147483647LL);
    CHECK_EQ(ost_domain::kMaxLayerNameUnits, static_cast<std::size_t>(50));
    CHECK(contains(ost_domain::kLayerNameTooLongHint, "50"));
    CHECK(contains(ost_domain::kLayerSequenceLimitText, "2147483647"));
    CHECK_STR_EQ(ost_domain::kLayerSequenceLimitText, "Cannot add a layer: no sequence number is left below 2147483647.");
}

// ---------------------------------------------------------------------------------------------------------------
// can_modify / can_delete
// ---------------------------------------------------------------------------------------------------------------

// layers_dialog.py:285-291 (SELECT mode) and layers_sidebar.py:230-231: own row, not template, not locked. test_layers_sidebar.py:334 (locked
// and template rows are not editable); test_layers_dialog.py:818 (a layer with the built-in uid "1" is an ordinary bid layer in bid mode).
TEST(LayerEditCapability, OnlyAnOwnUnlockedNonTemplateRowCanBeModifiedOrDeleted)
{
    struct Case
    {
        bool is_template, is_locked, expected;
    };
    const Case cases[] = {{false, false, true}, {true, false, false}, {false, true, false}, {true, true, false}};
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("template=") + (c.is_template ? "1" : "0") + " locked=" + (c.is_locked ? "1" : "0"));
        const BidLayer layer = row("7", "Layer", 1, "bid-1", c.is_template, c.is_locked);
        CHECK_EQ(ost_domain::can_modify_bid_layer(layer), c.expected);
        CHECK_EQ(ost_domain::can_delete_bid_layer(layer), c.expected);
    }
    // test_layers_dialog.py:818: BidLayer("1", "bid-1", "Bid layer", True, 1) is deletable in bid mode
    CHECK(ost_domain::can_delete_bid_layer(row("1", "Bid layer", 1, "bid-1")));
    CHECK(ost_domain::can_modify_bid_layer(row("1", "Bid layer", 1, "bid-1")));
    // a shared row (locked template of no bid) and the bid's own locked copy are neither
    CHECK_FALSE(ost_domain::can_modify_bid_layer(shared("8", "Shared Template", 7)));
    CHECK_FALSE(ost_domain::can_delete_bid_layer(shared("8", "Shared Template", 7)));
}

// ---------------------------------------------------------------------------------------------------------------
// check_layer_name: the Windows ports
// ---------------------------------------------------------------------------------------------------------------

// test_layers_dialog.py:265 test_layers_dialog_rename_rejects_duplicate_and_empty_names_without_write: layers "Layer 1" (layer-1) and
// "Layer 2" (layer-2); rename layer-2 to "layer 1" warns Duplicate Layer / "Layer layer 1 already exists." (layers_dialog.py:403-407); "   "
// reverts silently (:398-400). Neither writes.
TEST(LayerEditName, Windows_DialogRenameRejectsDuplicateAndEmptyNames)
{
    const std::vector<BidLayer> layers = two_layers();
    EXPECT_CHECK(rename("layer 1", "Layer 2", "layer-2", layers), "Duplicate", "layer 1", "Duplicate Layer", "Layer layer 1 already exists.");
    EXPECT_CHECK(rename("   ", "Layer 2", "layer-2", layers), "Blank", "", "", "");
}

// test_layers_sidebar.py:222 test_layers_sidebar_can_add_first_layer_after_last_layer_disappears: the first layer of an empty list; "  First  "
// is added as "First" with after_sequence 0 (layers_sidebar.py:337-345 strips; :425-429 / get_max_sequence is 0 for an empty list).
TEST(LayerEditName, Windows_SidebarFirstLayerIsTrimmedAndGoesAfterSequenceZero)
{
    EXPECT_CHECK(create("  First  ", {}), "Accept", "First", "", "");
    CHECK_EQ(ost_domain::new_layer_after_sequence({}, ""), 0LL);
}

// test_layers_sidebar.py:248 test_layers_sidebar_new_layer_is_added_after_selected_layer_unless_blank_or_duplicate: layers "Layer 1" (1) and
// "Layer 2" (2), layer-1 selected: after_sequence 1; "   " is refused silently, " layer 2 " warns Duplicate Layer (the stripped name in the
// text), "Layer 1b" is added.
TEST(LayerEditName, Windows_SidebarNewLayerBlankAndDuplicateAreRefused)
{
    const std::vector<BidLayer> layers = two_layers();
    CHECK_EQ(ost_domain::new_layer_after_sequence(layers, "layer-1"), 1LL);
    EXPECT_CHECK(create("   ", layers), "Blank", "", "", "");
    EXPECT_CHECK(create(" layer 2 ", layers), "Duplicate", "layer 2", "Duplicate Layer", "Layer layer 2 already exists.");
    EXPECT_CHECK(create("Layer 1b", layers), "Accept", "Layer 1b", "", "");
}

// test_layers_sidebar.py:316 test_layers_sidebar_inline_rename_rejects_blank_and_trims_name: "Layer 1" (layer-1): "   " reverts (nothing is
// written), "  Renamed Layer  " is renamed to "Renamed Layer".
TEST(LayerEditName, Windows_SidebarRenameTrimsAndBlankReverts)
{
    const std::vector<BidLayer> layers = {row("layer-1", "Layer 1", 1)};
    EXPECT_CHECK(rename("   ", "Layer 1", "layer-1", layers), "Blank", "", "", "");
    EXPECT_CHECK(rename("  Renamed Layer  ", "Layer 1", "layer-1", layers), "Accept", "Renamed Layer", "", "");
}

// test_layers_sidebar.py:334 test_layers_sidebar_inline_rename_rejects_duplicates_and_ignores_locked_rows: rows "Layer 1" (1), "Layer 2" (2),
// a locked "Locked" (3), a template "Template" (4). Row 1: " layer 2 " warns Duplicate Layer and reverts; "Layer 1" (its own name) does
// nothing. The locked and template rows are not editable (layers_sidebar.py:230-231, :306-307).
TEST(LayerEditName, Windows_SidebarRenameDuplicateAndEqualNameAndLockedRows)
{
    const BidLayer locked = row("layer-3", "Locked", 3, "bid-1", false, true);
    const BidLayer tmpl = row("layer-4", "Template", 4, "bid-1", true, false);
    const std::vector<BidLayer> layers = {row("layer-1", "Layer 1", 1), row("layer-2", "Layer 2", 2), locked, tmpl};
    EXPECT_CHECK(rename(" layer 2 ", "Layer 1", "layer-1", layers), "Duplicate", "layer 2", "Duplicate Layer", "Layer layer 2 already exists.");
    EXPECT_CHECK(rename("Layer 1", "Layer 1", "layer-1", layers), "Unchanged", "Layer 1", "", "");
    CHECK(ost_domain::can_modify_bid_layer(layers[0]));
    CHECK_FALSE(ost_domain::can_modify_bid_layer(locked));
    CHECK_FALSE(ost_domain::can_modify_bid_layer(tmpl));
}

// ---------------------------------------------------------------------------------------------------------------
// check_layer_name: Windows semantics beyond the ported tests
// ---------------------------------------------------------------------------------------------------------------

// layers_dialog.py:397-400 strip() is Python's (Unicode whitespace, the oracle of the strip rows is str.strip()): a blank name is silent.
TEST(LayerEditName, EveryKindOfBlankIsSilent)
{
    const std::vector<BidLayer> layers = two_layers();
    const char *blanks[] = {"", " ", "   ", "\t", "\n", "\t\n", " \r\n ", "\xC2\xA0",                  // NBSP
                            "\xE2\x80\x83",                                                             // em space
                            "\xE3\x80\x80", "\xC2\x85", "\xE2\x80\xA8\xE2\x80\xA9", "\x0B\x0C", "\x1C\x1D\x1E\x1F"}; // ideographic space, NEL, LS PS, VT FF, FS GS RS US
    int index = 0;
    for (const char *blank : blanks)
    {
        SCOPED_CONTEXT(std::string("blank #") + std::to_string(index++));
        EXPECT_CHECK(create(blank, layers), "Blank", "", "", "");
        EXPECT_CHECK(rename(blank, "Layer 1", "layer-1", layers), "Blank", "", "", "");
    }
}

// Python strip() takes the whitespace characters off both ends and nothing else (python -B -I: str.strip() of these strings): a tab, a line
// break, NEL, LS / PS, FS..US and the no-break space around a name are stripped, a NUL, a DEL, a zero-width space, a byte order mark and the
// Mongolian vowel separator are not whitespace and stay (the first two are control characters and refused below).
TEST(LayerEditName, StripRemovesWhitespaceAtBothEndsOnly)
{
    const std::vector<BidLayer> layers;
    EXPECT_CHECK(create(" \tWalls\t ", layers), "Accept", "Walls", "", "");
    EXPECT_CHECK(create("\x1C" "Walls", layers), "Accept", "Walls", "", "");
    EXPECT_CHECK(create("\xC2\x85" "Walls\xC2\x85", layers), "Accept", "Walls", "", "");
    EXPECT_CHECK(create("\xE2\x80\xA8" "Walls\xE2\x80\xA9", layers), "Accept", "Walls", "", "");
    EXPECT_CHECK(create("\x0B" "Walls\x0C", layers), "Accept", "Walls", "", "");
    EXPECT_CHECK(create("\xE3\x80\x80" "Walls\xC2\xA0", layers), "Accept", "Walls", "", "");
    EXPECT_CHECK(create("\xE2\x80\x8B" "Walls", layers), "Accept", "\xE2\x80\x8B" "Walls", "", "");   // zero-width space is not whitespace
    EXPECT_CHECK(create("\xEF\xBB\xBF" "Walls", layers), "Accept", "\xEF\xBB\xBF" "Walls", "", "");   // byte order mark neither
    EXPECT_CHECK(create("\xE1\xA0\x8E" "Walls", layers), "Accept", "\xE1\xA0\x8E" "Walls", "", "");   // Mongolian vowel separator neither
    EXPECT_CHECK(create("Two  Words", layers), "Accept", "Two  Words", "", "");                       // inner spaces are kept
}

// layers_dialog.py:401-402 / layers_sidebar.py:323-324: `new_name == layer.name` is the EXACT stored name, tested after the strip: the same
// text (also typed with spaces around) does nothing.
TEST(LayerEditName, TheSameNameAfterTheStripIsUnchanged)
{
    const std::vector<BidLayer> layers = two_layers();
    EXPECT_CHECK(rename("Layer 2", "Layer 2", "layer-2", layers), "Unchanged", "Layer 2", "", "");
    EXPECT_CHECK(rename("  Layer 2  ", "Layer 2", "layer-2", layers), "Unchanged", "Layer 2", "", "");
}

// The comparison is exact and case sensitive: a change of letter case alone is NOT unchanged and the layer's own row never counts as a
// duplicate (layers_dialog.py:403-404 `existing.uid != layer.uid`), so it is written.
TEST(LayerEditName, ACaseOnlyChangeOfTheOwnNameIsAccepted)
{
    const std::vector<BidLayer> layers = two_layers();
    EXPECT_CHECK(rename("layer 2", "Layer 2", "layer-2", layers), "Accept", "layer 2", "", "");
    EXPECT_CHECK(rename("LAYER 2", "Layer 2", "layer-2", layers), "Accept", "LAYER 2", "", "");
}

// A stored name with surrounding spaces ("Walls ") is not equal to the stripped typed name: it is rewritten stripped (Windows `new_name ==
// layer.name`, new_name stripped, layer.name as stored).
TEST(LayerEditName, AStoredNameWithSpacesIsRewrittenStripped)
{
    const std::vector<BidLayer> layers = {row("5", "Walls ", 1)};
    EXPECT_CHECK(rename("Walls", "Walls ", "5", layers), "Accept", "Walls", "", "");
}

// layers_dialog.py:293-298 _find_layer_by_name: strip().lower() against EVERY listed row: the bid's own layers, its locked copies of the template
// layers (Image, Annotation, Default) and the shared template rows. The text names the stripped typed name.
TEST(LayerEditName, DuplicateByCaseAndSpacesAgainstEveryListedRow)
{
    const std::vector<BidLayer> listed = sheet();
    EXPECT_CHECK(create("walls", listed), "Duplicate", "walls", "Duplicate Layer", "Layer walls already exists.");
    EXPECT_CHECK(create(" WALLS ", listed), "Duplicate", "WALLS", "Duplicate Layer", "Layer WALLS already exists.");
    EXPECT_CHECK(create("floors\t", listed), "Duplicate", "floors", "Duplicate Layer", "Layer floors already exists.");
    // a locked copy of a template layer
    EXPECT_CHECK(create("Image", listed), "Duplicate", "Image", "Duplicate Layer", "Layer Image already exists.");
    EXPECT_CHECK(create(" image ", listed), "Duplicate", "image", "Duplicate Layer", "Layer image already exists.");
    EXPECT_CHECK(create("ANNOTATION", listed), "Duplicate", "ANNOTATION", "Duplicate Layer", "Layer ANNOTATION already exists.");
    // a shared template row
    EXPECT_CHECK(create("shared template", listed), "Duplicate", "shared template", "Duplicate Layer", "Layer shared template already exists.");
    // rename: another row's name, the row itself excluded
    EXPECT_CHECK(rename("unused", "Walls", "105", listed), "Duplicate", "unused", "Duplicate Layer", "Layer unused already exists.");
    EXPECT_CHECK(rename("Image", "Walls", "105", listed), "Duplicate", "Image", "Duplicate Layer", "Layer Image already exists.");
    // near misses are not duplicates
    EXPECT_CHECK(create("Walls 2", listed), "Accept", "Walls 2", "", "");
    EXPECT_CHECK(create("Wall", listed), "Accept", "Wall", "", "");
    EXPECT_CHECK(create("Wa lls", listed), "Accept", "Wa lls", "", "");
}

// "If a bid has neither its own nor a shared row named Image, a user-created layer named 'Image' becomes the Image layer" (research 2.7): the
// duplicate rule only sees listed rows, so the name is accepted when none is listed.
TEST(LayerEditName, ImageIsAcceptedWhenNoImageRowIsListed)
{
    EXPECT_CHECK(create("Image", two_layers()), "Accept", "Image", "", "");
}

// The duplicate key is strip().lower() with PYTHON's lower() (Unicode, full case mapping, final sigma): the pairs of kNameOracle come from
// executing normalize_layer_name of layer.py:9-10 (python -B -I); `same` is `normalize_layer_name(a) == normalize_layer_name(b)`.
TEST(LayerEditName, DuplicateKeyMatchesWindowsOnUnicode)
{
    struct Row
    {
        const char *a, *b, *normalized_a, *normalized_b; // UTF-8 as hex
        bool same;
    };
    const Row kNameOracle[] = {
        {"77616c6c73", "57616c6c73", "77616c6c73", "77616c6c73", true},
        {"2057616c6c7320", "77616c6c73", "77616c6c73", "77616c6c73", true},
        {"c389434f4c45", "c3a9636f6c65", "c3a9636f6c65", "c3a9636f6c65", true},
        {"53747261c39f65", "53545241535345", "73747261c39f65", "73747261737365", false},
        {"53545241535345", "73747261737365", "73747261737365", "73747261737365", true},
        {"cea3ce91cea3", "cf83ceb1cf82", "cf83ceb1cf82", "cf83ceb1cf82", true},
        {"c4b0", "69cc87", "69cc87", "69cc87", true},
        {"c4b0", "69", "69cc87", "69", false},
        {"c2a057616c6c73e28083", "77616c6c73", "77616c6c73", "77616c6c73", true},
        {"57616c6c73", "57616c6c732032", "77616c6c73", "77616c6c732032", false},
        {"c785", "c786", "c786", "c786", true},
        {"e1ba9e", "c39f", "c39f", "c39f", true},
        {"e284aa", "6b", "6b", "6b", true},
        {"c4b07374616e62756c", "69cc877374616e62756c", "69cc877374616e62756c", "69cc877374616e62756c", true},
        {"e280a857616c6c73", "77616c6c73", "77616c6c73", "77616c6c73", true},
        {"5761206c6c73", "57616c6c73", "7761206c6c73", "77616c6c73", false},
        {"57414c4c5309", "77616c6c73", "77616c6c73", "77616c6c73", true},
        {"ce91ce92ce93", "ceb1ceb2ceb3", "ceb1ceb2ceb3", "ceb1ceb2ceb3", true},
    };
    int index = 0;
    for (const Row &r : kNameOracle)
    {
        SCOPED_CONTEXT("oracle row " + std::to_string(index++));
        const std::string a = unhex(r.a), b = unhex(r.b);
        CHECK_STR_EQ(ost_domain::normalize_layer_name(a), unhex(r.normalized_a));
        CHECK_STR_EQ(ost_domain::normalize_layer_name(b), unhex(r.normalized_b));
        // a typed `a` against a listed row named `b` (and the reverse): a duplicate exactly when the keys are equal
        const std::vector<BidLayer> listed_b = {row("1", b, 1)};
        const std::vector<BidLayer> listed_a = {row("1", a, 1)};
        const std::string expected = r.same ? "Duplicate" : "Accept";
        CHECK_STR_EQ(verdict_text(create(a, listed_b).verdict), expected);
        CHECK_STR_EQ(verdict_text(create(b, listed_a).verdict), expected);
    }
}

// LY-1 (layers_dialog.py:293-298, :433-436; project_read_service.py:50-53): Windows never lists the Comments row, so its duplicate check lets
// "Comments" through, which creates an invisible, undeletable layer. iOS: "comments" in any spelling is always taken, and the text is the
// duplicate text (N-M1-13). kCommentsOracle: is_comments_layer_name of layer.py:13-14 on the same strings.
TEST(LayerEditName, CommentsIsAlwaysTakenInEverySpelling)
{
    struct Row
    {
        const char *typed; // UTF-8 as hex
        bool is_comments;
    };
    const Row kCommentsOracle[] = {
        {"20434f4d4d454e545320", true},   {"436f6d6d656e7473", true},   {"c2a0436f6d6d656e7473e28083", true}, {"636f6d6d656e74", false},
        {"436f6d6d656e74732032", false},  {"434f4d4d454e54530a", true}, {"e280a8636f6d6d656e7473", true},    {"636f4d4d656e7473e38080", true},
    };
    // the listed rows never hold a Comments row (merge + drop), so the rule cannot come from them
    const std::vector<BidLayer> listed = sheet();
    int index = 0;
    for (const Row &r : kCommentsOracle)
    {
        SCOPED_CONTEXT("oracle row " + std::to_string(index++));
        const std::string typed = unhex(r.typed);
        CHECK_EQ(ost_domain::is_comments_layer_name(typed), r.is_comments);
        const LayerNameCheck c = create(typed, listed);
        if (r.is_comments)
        {
            CHECK_STR_EQ(verdict_text(c.verdict), "Reserved");
            CHECK_STR_EQ(c.title, "Duplicate Layer");
            CHECK_STR_EQ(c.message, "Layer " + c.name + " already exists.");
            CHECK_EQ(ost_domain::normalize_layer_name(c.name), std::string("comments"));
            // a rename is refused the same way
            CHECK_STR_EQ(verdict_text(rename(typed, "Walls", "105", listed).verdict), "Reserved");
        }
        else
        {
            CHECK_STR_EQ(verdict_text(c.verdict), "Accept");
            CHECK_STR_EQ(c.title, "");
        }
    }
    EXPECT_CHECK(create(" COMMENTS ", listed), "Reserved", "COMMENTS", "Duplicate Layer", "Layer COMMENTS already exists.");
    EXPECT_CHECK(create("Comments", listed), "Reserved", "Comments", "Duplicate Layer", "Layer Comments already exists.");
    // not the Comments layer
    EXPECT_CHECK(create("Comments 2", listed), "Accept", "Comments 2", "", "");
    EXPECT_CHECK(create("My Comments", listed), "Accept", "My Comments", "", "");
    EXPECT_CHECK(create("Comment", listed), "Accept", "Comment", "", "");
}

// The bid's own layers (ProjectData::layers) include the hidden Comments row and any row the merge shadowed (two own rows whose names
// differ only in case collapse to one listed row): the check also sees them. Stricter than Windows (which sees the listed rows only), never looser.
TEST(LayerEditName, TheBidsOwnLayersCountEvenWhenTheyAreNotListed)
{
    std::unordered_map<std::string, Layer> own_layers;
    own_layers["9"] = own("9", "Floors", 3);
    own_layers["10"] = own("10", " Hidden ", 4);
    EXPECT_CHECK(create("floors", {}, own_layers), "Duplicate", "floors", "Duplicate Layer", "Layer floors already exists.");
    EXPECT_CHECK(create("hidden", {}, own_layers), "Duplicate", "hidden", "Duplicate Layer", "Layer hidden already exists.");
    // the row being renamed is excluded from that map too
    EXPECT_CHECK(rename("FLOORS", "Floors", "9", {}, own_layers), "Accept", "FLOORS", "", "");
    EXPECT_CHECK(rename("hidden", "Floors", "9", {}, own_layers), "Duplicate", "hidden", "Duplicate Layer", "Layer hidden already exists.");
    EXPECT_CHECK(create("Roofing", {}, own_layers), "Accept", "Roofing", "", "");
}

// N-M1-13 / LY-8: Name is VARCHAR(50), the count is UTF-16 units (an emoji is 2): 50 units pass, 51 do not; the limit is on the STRIPPED name;
// the text is the generic failure text, a line break and the hint. A rename says "Rename Layer".
TEST(LayerEditName, TheLengthLimitCountsUtf16UnitsOfTheStrippedName)
{
    const std::vector<BidLayer> layers = two_layers();
    const std::string fifty = repeat("a", 50);
    const std::string fifty_one = repeat("a", 51);
    EXPECT_CHECK(create(fifty, layers), "Accept", fifty, "", "");
    EXPECT_CHECK(create("  " + fifty + "  ", layers), "Accept", fifty, "", "");                     // spaces do not count
    EXPECT_CHECK(create(repeat(kEmoji, 25), layers), "Accept", repeat(kEmoji, 25), "", "");          // 25 emoji = 50 units
    const std::string hint = std::string("\n") + ost_domain::kLayerNameTooLongHint;
    EXPECT_CHECK(create(fifty_one, layers), "TooLong", fifty_one, "New Layer", "Failed to create layer." + hint);
    EXPECT_CHECK(create(repeat(kEmoji, 26), layers), "TooLong", repeat(kEmoji, 26), "New Layer", "Failed to create layer." + hint);
    EXPECT_CHECK(create(repeat("\xC3\xA9", 51), layers), "TooLong", repeat("\xC3\xA9", 51), "New Layer", "Failed to create layer." + hint);
    EXPECT_CHECK(rename(fifty_one, "Layer 1", "layer-1", layers), "TooLong", fifty_one, "Rename Layer", "Failed to rename layer." + hint);
    EXPECT_CHECK(rename(repeat(kEmoji, 26), "Layer 1", "layer-1", layers), "TooLong", repeat(kEmoji, 26), "Rename Layer",
                 "Failed to rename layer." + hint);
    // 49 ASCII + one emoji = 51 units
    EXPECT_CHECK(create(repeat("a", 49) + kEmoji, layers), "TooLong", repeat("a", 49) + kEmoji, "New Layer", "Failed to create layer." + hint);
    EXPECT_CHECK(create(repeat("a", 48) + kEmoji, layers), "Accept", repeat("a", 48) + kEmoji, "", "");
}

// LY-14 (layers_dialog.py:397, :430: any character, also a pasted line break): control characters (Unicode Cc), the line breaks of Python's
// splitlines (U+2028, U+2029 too), a NUL and invalid UTF-8 are refused with the generic failure text and their own hint; ones at the ends are
// whitespace and stripped first (StripRemovesWhitespaceAtBothEndsOnly).
TEST(LayerEditName, ControlCharactersAndLineBreaksAreRefused)
{
    const std::vector<BidLayer> layers = two_layers();
    const std::string hint = std::string("\n") + ost_domain::kLayerNameControlHint;
    const std::string bad[] = {
        "A\tB",                          // tab inside
        "A\nB",                          // line feed
        "A\rB",                          // carriage return
        std::string("A\0B", 3),          // NUL
        "A\x01" "B",                     // SOH
        "A\x7F" "B",                     // DEL
        "A\xC2\x85" "B",                 // NEL (C1)
        "A\xC2\x9F" "B",                 // C1 end
        "A\xE2\x80\xA8" "B",             // LINE SEPARATOR
        "A\xE2\x80\xA9" "B",             // PARAGRAPH SEPARATOR
        "A\x0B" "B",                     // vertical tab
        "A\x0C" "B",                     // form feed
        "A\x1C" "B",                     // FS
        "A\xFF" "B",                     // invalid UTF-8
        "A\xC0\x80" "B",                 // overlong NUL
        "A\xED\xA0\x80" "B",             // a surrogate encoded in UTF-8
        "Walls\x01",                     // a control character at the end is not whitespace
    };
    int index = 0;
    for (const std::string &typed : bad)
    {
        SCOPED_CONTEXT("name #" + std::to_string(index++));
        EXPECT_CHECK(create(typed, layers), "ControlCharacter", typed, "New Layer", "Failed to create layer." + hint);
        EXPECT_CHECK(rename(typed, "Layer 1", "layer-1", layers), "ControlCharacter", typed, "Rename Layer", "Failed to rename layer." + hint);
    }
}

// Quotes, semicolons and every other printable character are fine: the store binds the name, it never concatenates it into SQL.
TEST(LayerEditName, PrintableCharactersAreAccepted)
{
    const std::vector<BidLayer> layers = two_layers();
    const char *good[] = {"O'Brien \"A\" \xC3\xA9; DROP", "Walls; DROP TABLE BidLayers;--", "50% / 100% [x] {y} <z>",
                          ("\xE6\xA3\x9F"
                           "\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC"),
                          "a", "1", "-1", "NULL", "0"};
    for (const char *typed : good)
    {
        SCOPED_CONTEXT(std::string("") + typed);
        EXPECT_CHECK(create(typed, layers), "Accept", typed, "", "");
    }
}

// The order of the rules: blank is silent before everything, an unchanged name is silent before the iOS refusals (a name an old import stored
// with 60 characters can be "renamed" to itself without a message), the refusals of the characters and the length come before the duplicate
// test, Comments before the duplicate test.
TEST(LayerEditName, TheRulesApplyInTheDocumentedOrder)
{
    const std::string sixty = repeat("a", 60);
    const std::vector<BidLayer> listed = {row("1", sixty, 1), row("2", "Layer 2", 2)};
    EXPECT_CHECK(rename(sixty, sixty, "1", listed), "Unchanged", sixty, "", "");
    // too long AND equal to another row: too long wins (the name could never be stored anyway)
    EXPECT_CHECK(rename(sixty, "Layer 2", "2", listed), "TooLong", sixty, "Rename Layer",
                 std::string("Failed to rename layer.\n") + ost_domain::kLayerNameTooLongHint);
    // control character AND too long: the control character is reported
    EXPECT_CHECK(create(sixty + "\n" + sixty, listed), "ControlCharacter", sixty + "\n" + sixty, "New Layer",
                 std::string("Failed to create layer.\n") + ost_domain::kLayerNameControlHint);
}

// ---------------------------------------------------------------------------------------------------------------
// new_layer_after_sequence
// ---------------------------------------------------------------------------------------------------------------

// test_layers_sidebar.py:248 second case: layers 1 (seq 1) and 2 (seq 5), nothing selected: after_sequence 5 (the highest).
TEST(LayerEditPosition, Windows_NoSelectionGoesAfterTheHighestSequence)
{
    const std::vector<BidLayer> layers = {row("layer-1", "Layer 1", 1), row("layer-2", "Layer 2", 5)};
    CHECK_EQ(ost_domain::new_layer_after_sequence(layers, ""), 5LL);
    CHECK_EQ(ost_domain::new_layer_after_sequence(layers, "layer-1"), 1LL);
    CHECK_EQ(ost_domain::new_layer_after_sequence(layers, "layer-2"), 5LL);
}

// layers_dialog.py:299-306 _max_sequence: max(layer.sequence), 0 for an empty list; the highest is the maximum whatever the list order.
TEST(LayerEditPosition, TheHighestSequenceIsTheMaximumWhateverTheOrder)
{
    CHECK_EQ(ost_domain::new_layer_after_sequence({}, ""), 0LL);
    CHECK_EQ(ost_domain::new_layer_after_sequence({row("a", "A", 5), row("b", "B", 1), row("c", "C", 3)}, ""), 5LL);
    CHECK_EQ(ost_domain::new_layer_after_sequence({row("a", "A", 1), row("b", "B", 9), row("c", "C", 3)}, ""), 9LL);
    // negative sequences: the maximum is negative (Python max), not 0
    CHECK_EQ(ost_domain::new_layer_after_sequence({row("a", "A", -3), row("b", "B", -1)}, ""), -1LL);
    // ties
    CHECK_EQ(ost_domain::new_layer_after_sequence({row("a", "A", 4), row("b", "B", 4)}, ""), 4LL);
}

// A selected uid that is not in the list (or empty) counts as no selection (Windows selected_layer is None).
TEST(LayerEditPosition, AnUnknownSelectionCountsAsNoSelection)
{
    const std::vector<BidLayer> layers = {row("a", "A", 2), row("b", "B", 7)};
    CHECK_EQ(ost_domain::new_layer_after_sequence(layers, "zzz"), 7LL);
    CHECK_EQ(ost_domain::new_layer_after_sequence(layers, ""), 7LL);
}

// Any listed row can be the selected one, a locked copy and a shared row included (Windows selects any row; "New Layer Below" on a locked
// copy is fine: the new row is the bid's own).
TEST(LayerEditPosition, ALockedOrSharedRowCanBeTheSelectedRow)
{
    const std::vector<BidLayer> listed = sheet();
    CHECK_EQ(ost_domain::new_layer_after_sequence(listed, "101"), 0LL); // Image, locked copy
    CHECK_EQ(ost_domain::new_layer_after_sequence(listed, "103"), 2LL);
    CHECK_EQ(ost_domain::new_layer_after_sequence(listed, "8"), 7LL);   // the shared row
    CHECK_EQ(ost_domain::new_layer_after_sequence(listed, ""), 7LL);    // highest = the shared row's 7
}

// The device-pack list without the shared row: Image 0, Annotation 1, Default 2 (the hidden Comments row 3 is not listed): the highest listed
// sequence is 2, so a new layer gets 3 and the hidden Comments row moves to 4 (research 2.3).
TEST(LayerEditPosition, TheBuiltInCopiesGiveTheDocumentedExample)
{
    const std::vector<BidLayer> listed = {row("101", "Image", 0, "10", false, true), row("102", "Annotation", 1, "10", false, true),
                                          row("103", "Default", 2, "10", false, true)};
    CHECK_EQ(ost_domain::new_layer_after_sequence(listed, ""), 2LL);
}

// Pure and overflow free: the sequences reach the Access maximum without any +1 here (the store adds it and refuses past the limit).
TEST(LayerEditPosition, TheMaximumSequenceIsReturnedWithoutOverflow)
{
    CHECK_EQ(ost_domain::new_layer_after_sequence({row("a", "A", INT_MAX), row("b", "B", 1)}, ""), 2147483647LL);
    CHECK_EQ(ost_domain::new_layer_after_sequence({row("a", "A", INT_MAX)}, "a"), 2147483647LL);
    CHECK_EQ(ost_domain::new_layer_after_sequence({row("a", "A", INT_MIN), row("b", "B", INT_MIN)}, ""), static_cast<long long>(INT_MIN));
}

// ---------------------------------------------------------------------------------------------------------------
// plan_layer_move
// ---------------------------------------------------------------------------------------------------------------

#define EXPECT_MOVE_ALLOWED(plan, neighbor)                       \
    do                                                            \
    {                                                             \
        const ost_domain::LayerMovePlan &ost_p_ = (plan);         \
        CHECK(ost_p_.allowed);                                    \
        CHECK_STR_EQ(ost_p_.neighbor_uid, (neighbor));            \
        CHECK_STR_EQ(ost_p_.refusal, "");                         \
    } while (0)

#define EXPECT_MOVE_REFUSED(plan)                                 \
    do                                                            \
    {                                                             \
        const ost_domain::LayerMovePlan &ost_p_ = (plan);         \
        CHECK_FALSE(ost_p_.allowed);                              \
        CHECK_STR_EQ(ost_p_.neighbor_uid, "");                    \
        CHECK_NE(ost_p_.refusal, std::string(""));                \
    } while (0)

// layers_dialog.py:628-636: neighbour = the listed row at row + direction. test_layers_dialog.py:409 (two layers "1" and "2": Move Down on "1").
TEST(LayerEditMove, TheNeighbourIsTheAdjacentListedRow)
{
    const std::vector<BidLayer> two = {row("1", "Original", 1), row("2", "Second", 2)};
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(two, "bid-1", "1", +1), "2");
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(two, "bid-1", "2", -1), "1");
    const std::vector<BidLayer> listed = sheet();
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(listed, "10", "106", -1), "105");
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(listed, "10", "106", +1), "107");
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(listed, "10", "105", +1), "106");
}

// layers_dialog.py:634 `target < 0 or target >= len(self._layers)`: the ends of the list have no neighbour; a single row has none either way.
TEST(LayerEditMove, TheEndsOfTheListAndASingleRowCannotMove)
{
    const std::vector<BidLayer> three = {row("1", "A", 1), row("2", "B", 2), row("3", "C", 3)};
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(three, "bid-1", "1", -1));
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(three, "bid-1", "3", +1));
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(three, "bid-1", "1", +1), "2");
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(three, "bid-1", "3", -1), "2");
    const std::vector<BidLayer> one = {row("1", "A", 1)};
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(one, "bid-1", "1", -1));
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(one, "bid-1", "1", +1));
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move({}, "bid-1", "1", +1));
}

// layers_dialog.py:634 `not self._can_modify(layer)`: a locked copy, a template row and a shared row cannot be moved themselves.
TEST(LayerEditMove, ALockedTemplateOrSharedRowCannotBeMovedItself)
{
    const std::vector<BidLayer> listed = sheet();
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(listed, "10", "102", +1)); // locked copy with an own neighbour
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(listed, "10", "102", -1));
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(listed, "10", "101", +1));
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(listed, "10", "8", -1));   // the shared row
    const std::vector<BidLayer> template_row = {row("1", "A", 1), row("2", "T", 2, "bid-1", true, false), row("3", "C", 3)};
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(template_row, "bid-1", "2", -1));
}

// N-M1-19 / LY-6 (layer_operations.py:432-465 has no filter, layers_dialog.py:634 checks the selected row only: a shared neighbour raises the
// raw scope error "BidLayers mutation targets do not belong to one authoritative bid."): a SHARED neighbour disables the move, a locked copy
// of the bid's own as the neighbour is allowed (its sequence is rewritten, D6).
TEST(LayerEditMove, ASharedNeighbourDisablesTheMoveButALockedOwnCopyDoesNot)
{
    const std::vector<BidLayer> listed = sheet();
    // Unused (last own row) down: the next listed row is the shared "Shared Template": refused
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(listed, "10", "107", +1));
    // Walls up: the neighbour is the bid's own locked copy "Default": allowed
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(listed, "10", "105", -1), "103");
    // a shared row ABOVE an own row: moving up into it is refused too
    const std::vector<BidLayer> shared_first = {shared("8", "Shared Template", 0), row("105", "Walls", 1, "10")};
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(shared_first, "10", "105", -1));
    // the shared row of ANOTHER bid's locked template (bid uid differs) is shared as well
    const std::vector<BidLayer> foreign = {row("9", "Foreign", 0, "20", true, true), row("105", "Walls", 1, "10")};
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(foreign, "10", "105", -1));
}

// The neighbour is the next row of the LIST, not the next sequence value; ties and gaps stay as they are (register LY-12).
TEST(LayerEditMove, TheNeighbourFollowsTheListOrderNotTheSequenceValue)
{
    const std::vector<BidLayer> unordered = {row("a", "A", 5), row("b", "B", 1), row("c", "C", 3)};
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(unordered, "bid-1", "b", -1), "a");
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(unordered, "bid-1", "b", +1), "c");
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(unordered, "bid-1", "a", +1), "b");
    const std::vector<BidLayer> tie = {row("a", "A", 1), row("b", "B", 1)};
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(tie, "bid-1", "a", +1), "b");
    EXPECT_MOVE_ALLOWED(ost_domain::plan_layer_move(tie, "bid-1", "b", -1), "a");
}

// An unknown uid, a direction other than -1 / +1 and a row of another bid are refused (defensive: the screens never ask).
TEST(LayerEditMove, UnknownRowsBadDirectionsAndForeignRowsAreRefused)
{
    const std::vector<BidLayer> three = {row("1", "A", 1), row("2", "B", 2), row("3", "C", 3)};
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(three, "bid-1", "99", +1));
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(three, "bid-1", "", +1));
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(three, "bid-1", "2", 0));
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(three, "bid-1", "2", 2));
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(three, "bid-1", "2", -2));
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(three, "bid-1", "2", INT_MAX));
    // an unlocked row whose bid is not the open bid: not movable
    const std::vector<BidLayer> foreign = {row("1", "A", 1, "bid-1"), row("2", "B", 2, "bid-2"), row("3", "C", 3, "bid-1")};
    EXPECT_MOVE_REFUSED(ost_domain::plan_layer_move(foreign, "bid-1", "2", -1));
}

// ---------------------------------------------------------------------------------------------------------------
// plan_layer_delete
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    // The oracle of messagebox.py:78-119 confirm_multi_delete(parent, "Delete Layer", items, used): which dialog it showed (question = a
    // confirmation with Yes / No and No as the default; warning = a message and no deletion), its text, and the rows it returns when the answer
    // is Yes. Executed with python -B -I and QMessageBox.question / warning replaced by recorders (the "item(s)" texts were recorded at 49a2d59;
    // parity-sync-2 hand-edited them to the singular / plural texts of d26893c, messagebox.py:295-330: count 1 = "item", any other count "items").
    struct DeleteOracleCase
    {
        const char *label;
        std::vector<std::pair<std::string, std::string>> items; // (name, uid) in the order passed to Windows
        std::set<std::string> used;
        const char *kind;      // Confirm / ConfirmMixed / OnlyInUse
        const char *message;   // the text of the dialog
        const char *deletable; // uids of the rows returned on Yes, joined by ','
        const char *in_use;    // uids refused, joined by ','
    };

    std::vector<DeleteOracleCase> delete_oracle()
    {
        return {
            {"plain one", {{"Walls", "5"}}, {}, "Confirm", "Delete 1 item?", "5", ""},
            {"only in use", {{"Walls", "5"}}, {"5"}, "OnlyInUse", "Cannot delete \"Walls\" because it is in use.", "", "5"},
            {"mixed",
             {{"Walls", "5"}, {"Floors", "6"}, {"Unused", "7"}},
             {"5", "6"},
             "ConfirmMixed",
             "Cannot delete \"Walls\" because it is in use.\nCannot delete \"Floors\" because it is in use.\n\nDelete the other 1 item?",
             "7",
             "5,6"},
            {"plain three", {{"A", "1"}, {"B", "2"}, {"C", "3"}}, {}, "Confirm", "Delete 3 items?", "1,2,3", ""},
            {"two in use",
             {{"Walls", "5"}, {"Floors", "6"}},
             {"5", "6"},
             "OnlyInUse",
             "Cannot delete \"Walls\" because it is in use.\nCannot delete \"Floors\" because it is in use.",
             "",
             "5,6"},
            {"quotes", {{"He said \"hi\"", "9"}}, {"9"}, "OnlyInUse", "Cannot delete \"He said \"hi\"\" because it is in use.", "", "9"},
            {"mixed one in use two free",
             {{"Walls", "5"}, {"Floors", "6"}, {"Unused", "7"}},
             {"6"},
             "ConfirmMixed",
             "Cannot delete \"Floors\" because it is in use.\n\nDelete the other 2 items?",
             "5,7",
             "6"},
            {"not in used set", {{"Walls", "5"}}, {"6"}, "Confirm", "Delete 1 item?", "5", ""},
        };
    }
}

// messagebox.py:78-119 on the rows of kDeleteOracle: every selected row is an own unlocked layer; the kind, the text, the rows to delete and the
// rows refused are Windows'.
TEST(LayerEditDeletePlan, Windows_TheDecisionTableMatchesConfirmMultiDelete)
{
    for (const DeleteOracleCase &c : delete_oracle())
    {
        SCOPED_CONTEXT(c.label);
        std::vector<BidLayer> listed;
        std::vector<std::string> selected;
        int sequence = 1;
        for (const auto &item : c.items)
        {
            listed.push_back(row(item.second, item.first, sequence++, "bid-1"));
            selected.push_back(item.second);
        }
        const ost_domain::LayerDeletePlan plan = ost_domain::plan_layer_delete(listed, selected, c.used);
        CHECK_STR_EQ(kind_text(plan.kind), c.kind);
        CHECK_STR_EQ(plan.title, "Delete Layer");
        CHECK_STR_EQ(plan.message, c.message);
        CHECK_STR_EQ(join(plan.deletable), c.deletable);
        CHECK_STR_EQ(join(plan.in_use), c.in_use);
    }
}

// The names travel with the rows (the refusal lines use them): LayerRef{uid, name}.
TEST(LayerEditDeletePlan, TheRowsKeepTheirNames)
{
    const std::vector<BidLayer> listed = {row("5", "Walls", 1), row("6", "Floors", 2), row("7", "Unused", 3)};
    const ost_domain::LayerDeletePlan plan = ost_domain::plan_layer_delete(listed, {"5", "6", "7"}, {"6"});
    REQUIRE_EQ(plan.deletable.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(plan.in_use.size(), static_cast<std::size_t>(1));
    CHECK_STR_EQ(plan.deletable[0].uid, "5");
    CHECK_STR_EQ(plan.deletable[0].name, "Walls");
    CHECK_STR_EQ(plan.deletable[1].uid, "7");
    CHECK_STR_EQ(plan.deletable[1].name, "Unused");
    CHECK_STR_EQ(plan.in_use[0].uid, "6");
    CHECK_STR_EQ(plan.in_use[0].name, "Floors");
}

// test_layers_dialog.py:824 test_a_mixed_selection_enables_delete_but_only_sends_the_deletable_layers (layers_dialog.py:490-496): only the rows
// that _can_delete go on; a locked copy, a shared row and an unknown uid are dropped without a message; with nothing deletable left nothing
// happens at all (the dialog returns before the question, :495-496).
TEST(LayerEditDeletePlan, Windows_OnlyTheDeletableRowsOfAMixedSelectionGoOn)
{
    const std::vector<BidLayer> listed = sheet();
    // locked copy Default (103), own Walls (105), shared row (8), unknown uid, Unused (107)
    const ost_domain::LayerDeletePlan mixed = ost_domain::plan_layer_delete(listed, {"103", "105", "8", "999", "107"}, {});
    CHECK_STR_EQ(kind_text(mixed.kind), "Confirm");
    CHECK_STR_EQ(join(mixed.deletable), "105,107");
    CHECK_STR_EQ(mixed.message, "Delete 2 items?");
    // only rows that cannot be deleted: nothing, silently (test_layers_dialog.py:848 only_built_in_layers_selected_never_reaches_the_confirmation)
    const ost_domain::LayerDeletePlan none = ost_domain::plan_layer_delete(listed, {"101", "8", "999"}, {});
    CHECK_STR_EQ(kind_text(none.kind), "NothingToDelete");
    CHECK_STR_EQ(none.title, "");
    CHECK_STR_EQ(none.message, "");
    CHECK_EQ(none.deletable.size(), static_cast<std::size_t>(0));
    CHECK_EQ(none.in_use.size(), static_cast<std::size_t>(0));
    // an empty selection is nothing as well
    CHECK_STR_EQ(kind_text(ost_domain::plan_layer_delete(listed, {}, {}).kind), "NothingToDelete");
    // a locked row that is also "in use" is not deletable and is not reported as in use either
    const ost_domain::LayerDeletePlan locked_used = ost_domain::plan_layer_delete(listed, {"101"}, {"101"});
    CHECK_STR_EQ(kind_text(locked_used.kind), "NothingToDelete");
}

// test_layer_delete_usage.py:38 and test_nested_layer_editor.py:74: the usage is read when the action starts, so the same layer is a question
// when it is unused and a warning when it is used: the plan is a pure function of the set it is given.
TEST(LayerEditDeletePlan, Windows_ActionTimeUsageDecidesEveryTime)
{
    const std::vector<BidLayer> listed = {row("layer-1", "Custom", 1, "bid-1")};
    // (test_nested_layer_editor.py:74) content on another page was removed after the editor opened: unused now -> the question
    const ost_domain::LayerDeletePlan unused = ost_domain::plan_layer_delete(listed, {"layer-1"}, {});
    CHECK_STR_EQ(kind_text(unused.kind), "Confirm");
    CHECK_STR_EQ(unused.message, "Delete 1 item?");
    // new authoritative content makes the same layer used again -> the warning, no question
    const ost_domain::LayerDeletePlan used = ost_domain::plan_layer_delete(listed, {"layer-1"}, {"layer-1"});
    CHECK_STR_EQ(kind_text(used.kind), "OnlyInUse");
    CHECK_STR_EQ(used.message, "Cannot delete \"Custom\" because it is in use.");
    CHECK_EQ(used.deletable.size(), static_cast<std::size_t>(0));
}

// Each uid counts once and the order of the selection is kept; a used-uid text that is not a selected row changes nothing.
TEST(LayerEditDeletePlan, ARepeatedUidCountsOnceAndTheSelectionOrderIsKept)
{
    const std::vector<BidLayer> listed = {row("5", "Walls", 1), row("6", "Floors", 2), row("7", "Unused", 3)};
    const ost_domain::LayerDeletePlan plan = ost_domain::plan_layer_delete(listed, {"7", "5", "7", "5"}, {"999", "abc"});
    CHECK_STR_EQ(kind_text(plan.kind), "Confirm");
    CHECK_STR_EQ(join(plan.deletable), "7,5");
    CHECK_STR_EQ(plan.message, "Delete 2 items?");
}

// The in-use test is on the uid TEXT (messagebox.py:84-85 `str(uid) in used_uids`): "5" is not "05".
TEST(LayerEditDeletePlan, TheUsedSetIsComparedAsText)
{
    const std::vector<BidLayer> listed = {row("5", "Walls", 1)};
    CHECK_STR_EQ(kind_text(ost_domain::plan_layer_delete(listed, {"5"}, {"05"}).kind), "Confirm");
    CHECK_STR_EQ(kind_text(ost_domain::plan_layer_delete(listed, {"5"}, {"5"}).kind), "OnlyInUse");
}

// ---------------------------------------------------------------------------------------------------------------
// layer_sequence_would_overflow
// ---------------------------------------------------------------------------------------------------------------

// LY-7: no Windows test (the Access driver fails with a raw text). The guard refuses BEFORE anything is shifted: the new row's sequence
// after_sequence + 1 must be in [-2147483648, 2147483647] and a later row (Sequence > after_sequence, highest_later_sequence, -1 = none) must
// still fit after +1. 64-bit safe: LLONG_MAX and LLONG_MIN do not overflow the check itself.
TEST(LayerEditLimits, TheSequenceGuardRefusesBeforeAnythingIsShifted)
{
    struct Case
    {
        long long after, highest_later;
        bool overflow;
    };
    const Case cases[] = {
        {0, -1, false},
        {2147483645LL, -1, false},
        {2147483646LL, -1, false},              // the new row gets 2147483647: allowed
        {2147483647LL, -1, true},               // the new row would get 2147483648
        {2147483648LL, -1, true},
        {2147483645LL, 2147483646LL, false},    // the later row shifts to 2147483647: allowed
        {2147483645LL, 2147483647LL, true},     // the later row would shift to 2147483648
        {2147483645LL, 3000000000LL, true},     // a stored value beyond 32 bit
        {5, 2147483646LL, false},
        {5, 2147483647LL, true},
        {0, 6, false},
        {-1, 0, false},
        {-2147483649LL, -1, false},             // the new row gets -2147483648: allowed
        {-2147483650LL, -1, true},              // below the Access Long
        {LLONG_MAX, -1, true},
        {LLONG_MIN, -1, true},
        {LLONG_MAX, LLONG_MAX, true},
        {0, LLONG_MAX, true},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT("after=" + std::to_string(c.after) + " highest_later=" + std::to_string(c.highest_later));
        CHECK_EQ(ost_domain::layer_sequence_would_overflow(c.after, c.highest_later), c.overflow);
    }
}
