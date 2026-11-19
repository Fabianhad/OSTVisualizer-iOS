// The pickers and orders of the condition screens (ConditionChoices.hpp): COND-CDNPICK-01 (the type picker sorts
// case-insensitively), COND-LAYER-01 (the layers offered), COND-SHAPE-01 (the shape picker order), COND-NAV-01 (Edit Previous /
// Next walk the list order), COND-PLACE-01 and COND-M07 (what happens to the placement tool).
//
// Windows oracle: presentation/dialogs/condition_types_dialog.py:152 (sorted by name.lower()),
// presentation/dialogs/edit_condition_dialog.py (_SHAPE_ITEMS, the layer list), domain/entities/layer.py merge_layers_for_bid,
// presentation/components/conditions_sidebar.py collect_ordered_condition_uids, presentation/coordinators/
// placement_coordinator.py enter, presentation/handlers/condition_action_handler.py _finish_condition_duplicate.
#include "TestHarness.hpp"

#include "Domain/ConditionChoices.hpp"
#include "Domain/ConditionTree.hpp"

#include <string>
#include <vector>

using namespace ost_domain;

namespace
{
    std::unordered_map<std::string, CdnType> types_of(const std::vector<std::pair<std::string, std::string>> &rows)
    {
        std::unordered_map<std::string, CdnType> types;
        for (const auto &row : rows)
            types[row.first] = CdnType{row.first, row.second};
        return types;
    }

    std::vector<std::string> names_of(const std::vector<std::string> &uids, const std::unordered_map<std::string, CdnType> &types)
    {
        std::vector<std::string> names;
        for (const std::string &uid : uids)
            names.push_back(types.at(uid).name);
        return names;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-CDNPICK-01
// ---------------------------------------------------------------------------------------------------------------------

// Test plan: Types ['zebra','Apple','apple','Banana'] -> ['Apple','apple','Banana','zebra'] (case-insensitive, equal-fold names
// keep input order); std::sort by raw name gave Apple, Banana, apple, zebra.
TEST(CondCdnPick01_TypePicker, TheTypesSortByLowerCasedNameAndEqualNamesKeepReadOrder)
{
    const auto types = types_of({{"1", "zebra"}, {"2", "Apple"}, {"3", "apple"}, {"4", "Banana"}});
    const std::vector<std::string> uids = sorted_cdn_type_uids(types);
    REQUIRE_EQ(uids.size(), 4u);
    const std::vector<std::string> expected = {"Apple", "apple", "Banana", "zebra"};
    CHECK(names_of(uids, types) == expected);
    CHECK((uids == std::vector<std::string>{"2", "3", "4", "1"}));
}

// the tie order is the database read order (ascending numeric uid), not the order of the unordered map or the text order
TEST(CondCdnPick01_TypePicker, EqualLowerCasedNamesFollowTheNumericUidOrder)
{
    const auto types = types_of({{"10", "apple"}, {"9", "Apple"}, {"100", "APPLE"}, {"2", "apple"}});
    const std::vector<std::string> uids = sorted_cdn_type_uids(types);
    CHECK((uids == std::vector<std::string>{"2", "9", "10", "100"}));
}

// Python str.lower() (full Unicode lower-casing) compared by code point: "Éclair".lower() is "éclair" (U+00E9), which sorts
// after every ASCII name: ["eclair", "zulu", "éclair", "étude"]
TEST(CondCdnPick01_TypePicker, NonAsciiNamesAreLowerCasedLikePython)
{
    const auto types = types_of({{"1", "\xC3\x89" "clair"}, {"2", "eclair"}, {"3", "Zulu"}, {"4", "\xC3\x89" "tude"}});
    const std::vector<std::string> uids = sorted_cdn_type_uids(types);
    CHECK((uids == std::vector<std::string>{"2", "3", "1", "4"}));
}

TEST(CondCdnPick01_TypePicker, NoTypesGiveNoUids)
{
    CHECK(sorted_cdn_type_uids({}).empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-LAYER-01
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
    BidLayer listed(const std::string &uid, const std::string &name, int sequence, bool is_template = false)
    {
        BidLayer layer;
        layer.uid = uid;
        layer.name = name;
        layer.sequence = sequence;
        layer.is_template = is_template;
        return layer;
    }

    Layer bid_layer(const std::string &uid, const std::string &name, int sequence)
    {
        Layer layer;
        layer.uid = uid;
        layer.name = name;
        layer.sequence = sequence;
        return layer;
    }
}

// Test plan: With layers {Image seq0, Annotation seq1, Default seq2, Comments seq3, Walls seq4}: picker list == [Image,
// Annotation, Default, Walls] in sequence order (iOS offered [Default, Walls])
TEST(CondLayer01_LayerChoices, EveryLayerButCommentsInSequenceOrder)
{
    const std::vector<BidLayer> merged = {listed("1", "Image", 0), listed("2", "Annotation", 1), listed("3", "Default", 2),
                                          listed("4", "Comments", 3), listed("5", "Walls", 4)};
    const std::unordered_map<std::string, Layer> own = {{"1", bid_layer("1", "Image", 0)},
                                                        {"2", bid_layer("2", "Annotation", 1)},
                                                        {"3", bid_layer("3", "Default", 2)},
                                                        {"4", bid_layer("4", "Comments", 3)},
                                                        {"5", bid_layer("5", "Walls", 4)}};
    const std::vector<std::string> uids = condition_layer_choices(merged, own);
    CHECK((uids == std::vector<std::string>{"1", "2", "3", "5"}));
}

// the order is the one of the merged list (sequence), not the name order and not the unordered map order
TEST(CondLayer01_LayerChoices, TheOrderIsTheSequenceOrderOfTheMergedList)
{
    const std::vector<BidLayer> merged = {listed("7", "Zebra", 0), listed("3", "Default", 1), listed("9", "Alpha", 2)};
    const std::unordered_map<std::string, Layer> own = {{"7", bid_layer("7", "Zebra", 0)},
                                                        {"3", bid_layer("3", "Default", 1)},
                                                        {"9", bid_layer("9", "Alpha", 2)}};
    CHECK((condition_layer_choices(merged, own) == std::vector<std::string>{"7", "3", "9"}));
}

// the Comments layer is recognised like Windows does: trimmed and case-insensitive
TEST(CondLayer01_LayerChoices, TheCommentsLayerIsRecognisedIgnoringCaseAndSpaces)
{
    const std::vector<BidLayer> merged = {listed("1", " COMMENTS ", 0), listed("2", "Default", 1)};
    const std::unordered_map<std::string, Layer> own = {{"1", bid_layer("1", " COMMENTS ", 0)}, {"2", bid_layer("2", "Default", 1)}};
    CHECK((condition_layer_choices(merged, own) == std::vector<std::string>{"2"}));
}

// a locked template layer of no bid is listed but cannot be assigned: only the bid's own layers are offered
TEST(CondLayer01_LayerChoices, ATemplateOnlyEntryIsNotOffered)
{
    const std::vector<BidLayer> merged = {listed("t1", "Template Layer", 0, true), listed("3", "Default", 1)};
    const std::unordered_map<std::string, Layer> own = {{"3", bid_layer("3", "Default", 1)}};
    CHECK((condition_layer_choices(merged, own) == std::vector<std::string>{"3"}));
}

TEST(CondLayer01_LayerChoices, NothingListedOffersNothing)
{
    CHECK(condition_layer_choices({}, {}).empty());
    const std::unordered_map<std::string, Layer> own = {{"3", bid_layer("3", "Default", 1)}};
    CHECK(condition_layer_choices({}, own).empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-SHAPE-01
// ---------------------------------------------------------------------------------------------------------------------

// Test plan: shape option list == [(0,'Square'),(1,'Circle'),(2,'Triangle'),(3,'Rectangle'),(13,'Ellipse'),(12,'Diamond'),
// (14,'Hexagon'),(15,'Octagon')]
TEST(CondShape01_ShapePicker, TheWindowsOrderPutsEllipseBeforeDiamond)
{
    const std::vector<ShapeChoice> &choices = condition_shape_choices();
    const std::pair<int, const char *> expected[] = {{0, "Square"},  {1, "Circle"},  {2, "Triangle"}, {3, "Rectangle"},
                                                     {13, "Ellipse"}, {12, "Diamond"}, {14, "Hexagon"},  {15, "Octagon"}};
    REQUIRE_EQ(choices.size(), 8u);
    for (std::size_t i = 0; i < choices.size(); ++i)
    {
        SCOPED_CONTEXT("entry " + std::to_string(i));
        CHECK_EQ(static_cast<int>(choices[i].shape), expected[i].first);
        CHECK_STR_EQ(choices[i].label, expected[i].second);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-NAV-01
// ---------------------------------------------------------------------------------------------------------------------

namespace
{
    Condition condition(const std::string &uid, const std::string &name, int ref_no, const std::string &cdn_type_uid = "",
                        const std::string &folder_uid = "")
    {
        Condition c;
        c.uid = uid;
        c.name = name;
        c.ref_no = ref_no;
        c.cdn_type_uid = cdn_type_uid;
        c.condition_folder_uid = folder_uid;
        return c;
    }
}

// Test plan: conditions {c1 ref 2 'B' type Z, c2 ref 1 'C' type A, c3 ref 1 'A' type Z}: Windows order = type A [c2], type Z
// [c3 ref1, c1 ref2] -> [c2, c3, c1]; iOS walked the name order [c3, c1, c2]
TEST(CondNav01_EditNavigation, PreviousAndNextFollowTheListOrder)
{
    std::unordered_map<std::string, Condition> conditions;
    conditions["1"] = condition("1", "B", 2, "20");
    conditions["2"] = condition("2", "C", 1, "10");
    conditions["3"] = condition("3", "A", 1, "20");
    const auto types = types_of({{"10", "A"}, {"20", "Z"}});
    const std::vector<std::string> uids = condition_uids_in_list_order(conditions, {}, types);
    CHECK((uids == std::vector<std::string>{"2", "3", "1"}));
}

// the order is the flattened pre-order of the tree the list shows: folders (with their type groups) and the conditions at the
// root in name order, every condition once
TEST(CondNav01_EditNavigation, TheOrderIsTheFlattenedTree)
{
    std::unordered_map<std::string, Condition> conditions;
    conditions["1"] = condition("1", "in folder, ref 2", 2, "", "100");
    conditions["2"] = condition("2", "in folder, ref 1", 1, "", "100");
    conditions["3"] = condition("3", "at the root", 5);
    conditions["4"] = condition("4", "dangling folder", 3, "", "999");
    std::unordered_map<std::string, ConditionFolder> folders;
    folders["100"] = ConditionFolder{"100", "Folder", ""};
    const auto types = types_of({});

    const std::vector<std::string> uids = condition_uids_in_list_order(conditions, folders, types);
    std::vector<std::string> expected;
    for (const ConditionTreeNode &node : build_condition_tree(conditions, folders, types))
        if (node.kind == ConditionTreeNode::Kind::Condition)
            expected.push_back(node.uid);
    CHECK(uids == expected);
    CHECK_EQ(uids.size(), 4u);
}

TEST(CondNav01_EditNavigation, NoConditionsGiveAnEmptyList)
{
    CHECK(condition_uids_in_list_order({}, {}, {}).empty());
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-PLACE-01 / COND-M07
// ---------------------------------------------------------------------------------------------------------------------

// A condition on a hidden layer is not placeable: selecting it must not enter place mode (PlacementCoordinator.enter forces the
// placement tool off); a visible condition is entered.
TEST(CondPlace01_Placement, AHiddenLayerConditionDoesNotEnterPlacement)
{
    CHECK(placement_for_selected_condition(true) == PlacementAction::Enter);
    CHECK(placement_for_selected_condition(false) == PlacementAction::Exit);
}

// test_duplicate_placement.py::test_duplicate_uses_condition_state_projected_before_active_placement: in the 2D takeoff view
// the duplicate enters placement; test_duplicate_outside_2d_view_keeps_placement_and_only_highlights: outside it the current
// placement is kept
TEST(CondM07_Placement, ADuplicateEntersPlacementOnlyInThe2DView)
{
    CHECK(placement_after_duplicate(true, true) == PlacementAction::Enter);
    CHECK(placement_after_duplicate(false, true) == PlacementAction::Keep);
    CHECK(placement_after_duplicate(false, false) == PlacementAction::Keep);
    // enter() refuses an unplaceable condition and leaves the placement tool
    CHECK(placement_after_duplicate(true, false) == PlacementAction::Exit);
}

// ---------------------------------------------------------------------------------------------------------------------
// COND-PLACE-01 / LP-17: the placement tool is suspended when its layer is hidden and restored when it is shown again
// (UIEventCoordinator._active_layer_tool_snapshot, _suspend_active_layer_tool, _restore_suspended_layer_tool)
// ---------------------------------------------------------------------------------------------------------------------
namespace
{
    Condition placed_condition(const std::string &uid, const std::string &layer_uid, ConditionType type = ConditionType::Area)
    {
        Condition condition;
        condition.uid = uid;
        condition.layer_uid = layer_uid;
        condition.condition_type = type;
        return condition;
    }

    // The state the app is in after the suspension left the placement tool: back in the view mode, the tool revision it had,
    // the same bid, the placed condition still there and visible, the 2D takeoff view open.
    PlacementRestoreState state_after_suspension(const SuspendedPlacement &suspended, const Condition *condition)
    {
        PlacementRestoreState now;
        now.bid_uid = suspended.bid_uid;
        now.tool_in_view_mode = true;
        now.tool_revision = suspended.tool_revision;
        now.condition = condition;
        now.condition_visible = true;
        now.takeoff_2d_view_active = true;
        return now;
    }

    SuspendedPlacement suspended_on_l1()
    {
        const Condition condition = placed_condition("c-1", "L1");
        std::optional<SuspendedPlacement> suspended = suspended_placement_for_layer_hide("bid-1", condition, std::string("L1"));
        REQUIRE(suspended.has_value());
        suspended->tool_revision = 4;
        return *suspended;
    }
}

// test_snapshot_captures_every_placed_condition_identity (single placed condition) and
// test_snapshot_layer_filter_must_match_a_condition_layer: hiding the layer of the placed condition suspends the placement and
// remembers the condition, its layer, its style and the bid; hiding any other layer does not
TEST(CondPlace01_LayerHide, HidingTheLayerOfThePlacedConditionSuspendsThePlacement)
{
    const Condition condition = placed_condition("c-1", "L1", ConditionType::Linear);
    const std::optional<SuspendedPlacement> suspended =
        suspended_placement_for_layer_hide("bid-1", condition, std::string("L1"));
    REQUIRE(suspended.has_value());
    CHECK_EQ(suspended->condition_uid, std::string("c-1"));
    CHECK_EQ(suspended->layer_uid, std::string("L1"));
    CHECK_EQ(suspended->bid_uid, std::string("bid-1"));
    CHECK(suspended->condition_type == ConditionType::Linear);

    CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", condition, std::string("L2")).has_value());
    CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", condition, std::string("")).has_value());
}

// test_snapshot_without_layer_filter_uses_condition_layer: Hide All names no layer, the suspension is for the layer of the
// condition
TEST(CondPlace01_LayerHide, HideAllSuspendsThePlacementOnTheLayerOfTheCondition)
{
    const Condition condition = placed_condition("c-1", "L9");
    const std::optional<SuspendedPlacement> suspended = suspended_placement_for_layer_hide("bid-1", condition, std::nullopt);
    REQUIRE(suspended.has_value());
    CHECK_EQ(suspended->layer_uid, std::string("L9"));
}

// test_snapshot_refuses_when_no_placed_condition_has_a_layer: a condition on no layer is never suspended (it is not hidden by a
// layer)
TEST(CondPlace01_LayerHide, AConditionOnNoLayerIsNeverSuspended)
{
    const Condition condition = placed_condition("c-1", "");
    CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", condition, std::nullopt).has_value());
    CHECK_FALSE(suspended_placement_for_layer_hide("bid-1", condition, std::string("L1")).has_value());
}

// test_matching_layer_or_no_layer_restores_and_clears_the_suspension: the layer shown again, or Show All, puts the placement
// back
TEST(CondPlace01_LayerShow, TheLayerShownAgainOrShowAllRestoresThePlacement)
{
    const SuspendedPlacement suspended = suspended_on_l1();
    const Condition condition = placed_condition("c-1", "L1");
    const PlacementRestoreState now = state_after_suspension(suspended, &condition);
    CHECK(restore_of_suspended_placement(suspended, std::string("L1"), now) == SuspendedPlacementOutcome::Restore);
    CHECK(restore_of_suspended_placement(suspended, std::nullopt, now) == SuspendedPlacementOutcome::Restore);
}

// test_other_layer_keeps_the_suspended_tool_untouched: showing another layer neither restores nor drops it
TEST(CondPlace01_LayerShow, AnotherLayerShownKeepsTheSuspension)
{
    const SuspendedPlacement suspended = suspended_on_l1();
    const Condition condition = placed_condition("c-1", "L1");
    const PlacementRestoreState now = state_after_suspension(suspended, &condition);
    CHECK(restore_of_suspended_placement(suspended, std::string("L2"), now) == SuspendedPlacementOutcome::Keep);
    // even when everything else changed: the layer comes first
    PlacementRestoreState changed = now;
    changed.bid_uid = "bid-2";
    CHECK(restore_of_suspended_placement(suspended, std::string("L2"), changed) == SuspendedPlacementOutcome::Keep);
}

// test_changed_cursor_mode_or_tool_revision_drops_the_suspension: a tool picked since the suspension (another mode, or any tool
// change that bumped the revision) drops the suspension, the placement is not forced back over it
TEST(CondPlace01_LayerShow, AToolChosenSinceTheSuspensionDropsIt)
{
    const SuspendedPlacement suspended = suspended_on_l1();
    const Condition condition = placed_condition("c-1", "L1");
    PlacementRestoreState now = state_after_suspension(suspended, &condition);
    now.tool_in_view_mode = false;
    CHECK(restore_of_suspended_placement(suspended, std::string("L1"), now) == SuspendedPlacementOutcome::Drop);

    now = state_after_suspension(suspended, &condition);
    now.tool_revision = suspended.tool_revision + 1;
    CHECK(restore_of_suspended_placement(suspended, std::string("L1"), now) == SuspendedPlacementOutcome::Drop);
    CHECK(restore_of_suspended_placement(suspended, std::nullopt, now) == SuspendedPlacementOutcome::Drop);
}

// test_other_bid_or_replaced_bid_owner_drops_the_suspension
TEST(CondPlace01_LayerShow, AnotherBidDropsTheSuspension)
{
    const SuspendedPlacement suspended = suspended_on_l1();
    const Condition condition = placed_condition("c-1", "L1");
    PlacementRestoreState now = state_after_suspension(suspended, &condition);
    now.bid_uid = "bid-2";
    CHECK(restore_of_suspended_placement(suspended, std::string("L1"), now) == SuspendedPlacementOutcome::Drop);
}

// test_placement_requires_every_captured_condition_to_be_unchanged (removed, retyped) and
// test_hidden_active_condition_is_not_re_entered_even_if_captured_ones_are_ok: a condition that is gone, of another style, on
// another layer or still hidden is not placed again
TEST(CondPlace01_LayerShow, AChangedOrHiddenConditionIsNotPlacedAgain)
{
    const SuspendedPlacement suspended = suspended_on_l1();

    PlacementRestoreState removed = state_after_suspension(suspended, nullptr);
    CHECK(restore_of_suspended_placement(suspended, std::string("L1"), removed) == SuspendedPlacementOutcome::Drop);

    const Condition retyped = placed_condition("c-1", "L1", ConditionType::Linear);
    CHECK(restore_of_suspended_placement(suspended, std::string("L1"), state_after_suspension(suspended, &retyped)) ==
          SuspendedPlacementOutcome::Drop);

    const Condition moved = placed_condition("c-1", "L2");
    CHECK(restore_of_suspended_placement(suspended, std::string("L1"), state_after_suspension(suspended, &moved)) ==
          SuspendedPlacementOutcome::Drop);

    const Condition condition = placed_condition("c-1", "L1");
    PlacementRestoreState hidden = state_after_suspension(suspended, &condition);
    hidden.condition_visible = false;
    CHECK(restore_of_suspended_placement(suspended, std::string("L1"), hidden) == SuspendedPlacementOutcome::Drop);
}

// test_placement_requires_2d_view_and_place_permission (the 2D view half): placement is only put back in the 2D takeoff view
TEST(CondPlace01_LayerShow, PlacementIsOnlyRestoredInThe2DView)
{
    const SuspendedPlacement suspended = suspended_on_l1();
    const Condition condition = placed_condition("c-1", "L1");
    PlacementRestoreState now = state_after_suspension(suspended, &condition);
    now.takeoff_2d_view_active = false;
    CHECK(restore_of_suspended_placement(suspended, std::string("L1"), now) == SuspendedPlacementOutcome::Drop);
}
