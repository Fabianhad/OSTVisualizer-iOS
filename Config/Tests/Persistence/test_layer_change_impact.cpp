// M1 layers UI (plan 5.3 and 4.11): ost_domain::layer_rename_preserves_rendering (Core/Domain/LayerChangeImpact.hpp), the decision the Layers
// sheet takes after a rename: may the page background and the meshes stay as they are, or must they be drawn again?
//
// Windows oracle (49a2d59): ost_visualizer/application/layer_change_impact.py:12-34 layer_rename_preserves_rendering, pinned by
// tests/application/test_layer_change_impact.py (7 tests: :11 test_only_ordinary_names_preserve_rendering, :39
// test_no_rename_empty_or_duplicate_identity_cannot_claim_narrow_impact, :52 test_rename_collision_changing_effective_membership_is_conservative,
// :60 test_every_retained_layer_must_preserve_non_name_state, :84 test_empty_or_duplicate_layer_lists_never_preserve_rendering, :103
// test_a_rename_alongside_an_unchanged_layer_preserves_rendering_and_other_uids_do_not, :135
// test_a_case_only_or_whitespace_only_rename_still_counts_as_a_rename). Every expected value of the extra table (kOracle) was produced by
// executing the real Windows function read-only with `python -B -I` (layer_rename_preserves_rendering on BidLayer("1", "8", "Walls", True, 1)
// with each change below, forward and reversed, plus the list cases); the Windows file:line of the function is above.
//
// The one place iOS needs it: ViewController+LayerEditing.mm asks it after a rename (before = the merged list before the write, after = the merged
// list the reload returned) and draws the page and the meshes again only when it answers false (plan 4.11).
#include "LayerEditTestSupport.hpp"

#include "Domain/Layer.hpp"
#include "Domain/LayerChangeImpact.hpp"

#include <string>
#include <utility>
#include <vector>

using ost_domain::BidLayer;
using ost_domain::layer_rename_preserves_rendering;

namespace
{
    // BidLayer(uid, bid_uid, name, show, sequence, is_template, is_locked) of the Windows tests.
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

    BidLayer walls()
    {
        return layer("1", "8", "Walls", true, 1);
    }

    BidLayer floors()
    {
        return layer("2", "8", "Floors", false, 2);
    }

    BidLayer renamed(BidLayer l, const std::string &name)
    {
        l.name = name;
        return l;
    }
}

// test_layer_change_impact.py:11. The ten change sets, each forward and reversed, and the two empty lists.
TEST(LayerChangeImpact, OnlyOrdinaryNamesPreserveRendering)
{
    const BidLayer old = walls();
    struct Case
    {
        const char *what;
        BidLayer changed;
        bool expected;
    };
    BidLayer shown = renamed(old, "Partitions");
    shown.show = false;
    BidLayer moved = renamed(old, "Partitions");
    moved.sequence = 2;
    BidLayer locked = renamed(old, "Partitions");
    locked.is_locked = true;
    BidLayer templ = renamed(old, "Partitions");
    templ.is_template = true;
    BidLayer other_uid = renamed(old, "Partitions");
    other_uid.uid = "2";
    BidLayer other_bid = renamed(old, "Partitions");
    other_bid.bid_uid = "other-bid";
    const std::vector<Case> cases = {
        {"Partitions", renamed(old, "Partitions"), true},
        {"Image", renamed(old, "Image"), false},
        {" ANNOTATION ", renamed(old, " ANNOTATION "), false},
        {"comments", renamed(old, "comments"), false},
        {"Partitions + show", shown, false},
        {"Partitions + sequence", moved, false},
        {"Partitions + locked", locked, false},
        {"Partitions + template", templ, false},
        {"Partitions + uid", other_uid, false},
        {"Partitions + bid", other_bid, false},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("change: ") + c.what);
        CHECK_EQ(layer_rename_preserves_rendering({old}, {c.changed}), c.expected);
        CHECK_EQ(layer_rename_preserves_rendering({c.changed}, {old}), c.expected);
    }
    CHECK_FALSE(layer_rename_preserves_rendering({}, {old}));
    CHECK_FALSE(layer_rename_preserves_rendering({old}, {}));
}

// test_layer_change_impact.py:39
TEST(LayerChangeImpact, NoRenameEmptyOrDuplicateIdentityCannotClaimNarrowImpact)
{
    const BidLayer old = walls();
    const BidLayer r = renamed(old, "Partitions");
    CHECK_FALSE(layer_rename_preserves_rendering({}, {}));
    CHECK_FALSE(layer_rename_preserves_rendering({old}, {old}));
    CHECK_FALSE(layer_rename_preserves_rendering({old, old}, {r}));
    CHECK_FALSE(layer_rename_preserves_rendering({old}, {r, r}));
    CHECK(layer_rename_preserves_rendering({old}, {r}));
}

// test_layer_change_impact.py:52: two layers collapse into one by the merge (names are compared lower-cased), so the uid sets differ.
TEST(LayerChangeImpact, RenameCollisionChangingEffectiveMembershipIsConservative)
{
    const BidLayer old = walls();
    const BidLayer sibling = floors();
    const std::vector<BidLayer> before = ost_domain::merge_layers_for_bid({old, sibling});
    const std::vector<BidLayer> after = ost_domain::merge_layers_for_bid({renamed(old, "Floors"), sibling});
    CHECK_EQ(before.size(), static_cast<std::size_t>(2));
    CHECK_EQ(after.size(), static_cast<std::size_t>(1));
    CHECK_FALSE(layer_rename_preserves_rendering(before, after));
}

// test_layer_change_impact.py:60: every layer that stays must keep everything but its name; the order of the lists does not matter.
TEST(LayerChangeImpact, EveryRetainedLayerMustPreserveNonNameState)
{
    const BidLayer first = walls();
    const BidLayer second = floors();
    const BidLayer r = renamed(first, "Partitions");
    const std::vector<std::vector<BidLayer>> befores = {{first, second}, {second, first}};
    for (const std::vector<BidLayer> &before : befores)
    {
        CHECK(layer_rename_preserves_rendering(before, {second, r}));
        BidLayer second_shown = second;
        second_shown.show = true;
        CHECK_FALSE(layer_rename_preserves_rendering(before, {r, second_shown}));
        CHECK_FALSE(layer_rename_preserves_rendering(before, {r, renamed(second, "Image")}));
    }
}

// test_layer_change_impact.py:84
TEST(LayerChangeImpact, EmptyOrDuplicateLayerListsNeverPreserveRendering)
{
    const BidLayer l = walls();
    const BidLayer r = renamed(l, "Partitions");
    CHECK_FALSE(layer_rename_preserves_rendering({}, {}));
    CHECK_FALSE(layer_rename_preserves_rendering({l}, {}));
    CHECK_FALSE(layer_rename_preserves_rendering({}, {l}));
    CHECK_FALSE(layer_rename_preserves_rendering({l, l}, {r, r}));
    CHECK_FALSE(layer_rename_preserves_rendering({l}, {r, r}));
    CHECK_FALSE(layer_rename_preserves_rendering({l, l}, {r}));
    CHECK_FALSE(layer_rename_preserves_rendering({l}, {l}));
    CHECK(layer_rename_preserves_rendering({l}, {r}));
}

// test_layer_change_impact.py:103
TEST(LayerChangeImpact, RenameAlongsideAnUnchangedLayerPreservesRenderingAndOtherUidsDoNot)
{
    const BidLayer w = walls();
    BidLayer f = floors();
    f.show = true;
    CHECK(layer_rename_preserves_rendering({w, f}, {renamed(w, "Partitions"), f}));
    CHECK_FALSE(layer_rename_preserves_rendering({w, f}, {renamed(w, "Partitions")}));
    CHECK_FALSE(layer_rename_preserves_rendering({w}, {renamed(f, "Partitions")}));
}

// test_layer_change_impact.py:135: the rename is detected on the raw names, so a change of letter case or of surrounding spaces of an ordinary
// name is a rename (and preserves rendering), not a no-op.
TEST(LayerChangeImpact, CaseOnlyOrWhitespaceOnlyRenameStillCountsAsARename)
{
    const BidLayer old = walls();
    for (const char *name : {"WALLS", " Walls ", "walls"})
    {
        SCOPED_CONTEXT(std::string("name: '") + name + "'");
        CHECK(layer_rename_preserves_rendering({old}, {renamed(old, name)}));
        CHECK(layer_rename_preserves_rendering({renamed(old, name)}, {old}));
    }
}

// Extra oracle rows (executed with python -B -I against the real Windows function; the inputs are the same as the Windows tests use):
// the role names in every spelling Python's strip().lower() folds (padding, non-breaking spaces, tabs and line breaks), a swap of the list order,
// two plain renames at once, and the cases of the iOS sheet (a rename that touches a role name on either side).
TEST(LayerChangeImpact, OracleRows)
{
    const BidLayer old = walls();
    struct Row
    {
        const char *what;
        std::vector<BidLayer> before, after;
        bool expected; // layer_rename_preserves_rendering(before, after) of the Windows function
    };
    const BidLayer r = renamed(old, "Partitions");
    const BidLayer sib = floors();
    const std::vector<Row> rows = {
        {"role Image with trailing space", {old}, {renamed(old, "Image ")}, false},
        {"role IMAGE with leading space", {old}, {renamed(old, " IMAGE")}, false},
        {"role Comments", {old}, {renamed(old, "Comments")}, false},
        {"role Annotation", {old}, {renamed(old, "Annotation")}, false},
        {"role behind non-breaking spaces", {old}, {renamed(old, "\xC2\xA0Image\xC2\xA0")}, false},
        {"role behind a tab and a line break", {old}, {renamed(old, "\tcomments\n")}, false},
        {"role as the OLD name", {renamed(old, "Image")}, {r}, false},
        {"two ordinary renames", {old, sib}, {r, renamed(sib, "Ceilings")}, true},
        {"two renames, one becomes a role", {old, sib}, {r, renamed(sib, "image")}, false},
        {"two renames, one was a role", {old, renamed(sib, "Comments")}, {r, renamed(sib, "Ceilings")}, false},
        {"only the list order changes besides the rename", {old, sib}, {sib, r}, true},
        {"retained layer keeps its state", {old, sib}, {r, sib}, true},
        {"a uid missing after", {old, sib}, {r}, false},
        {"a different uid after", {old}, {renamed(sib, "Partitions")}, false},
        {"nothing renamed", {old, sib}, {old, sib}, false},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(std::string("row: ") + row.what);
        CHECK_EQ(layer_rename_preserves_rendering(row.before, row.after), row.expected);
    }
}

// The iOS reading of the same decision on the lists the sheet really has (merged list, sorted by sequence): renaming the sheet's own plain layer
// preserves rendering, and renaming it to a role name (a bid without an Image row lets a user layer named "Image" become the Image layer, which the
// page background follows) does not.
TEST(LayerChangeImpact, RenameOfAPlainLayerOnTheDevicePackListPreservesRendering)
{
    const std::vector<BidLayer> before = {layer("101", "10", "Image", true, 0, false, true), layer("102", "10", "Annotation", true, 1, false, true),
                                          layer("105", "10", "Walls", true, 4),              layer("106", "10", "Floors", true, 5),
                                          layer("8", "", "Shared Template", true, 7, true, true)};
    std::vector<BidLayer> after = before;
    after[2].name = "Spare";
    CHECK(layer_rename_preserves_rendering(before, after));
    after[2].name = "image";
    CHECK_FALSE(layer_rename_preserves_rendering(before, after));
    after[2].name = "Spare";
    after[3].show = false; // a visibility change in the same reload is not "only a rename"
    CHECK_FALSE(layer_rename_preserves_rendering(before, after));
}
