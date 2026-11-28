// A stored quantity the condition style does not offer (ConditionQuantityChoices.hpp), parity pass 3 package 4 (Windows d1c430a).
//
// Windows oracle: ost_visualizer/presentation/dialogs/edit_condition_dialog.py _show_stored_quantity (a stored calc_type that is not
// in the quantity combo's items is added as a disabled item EDIT_CONDITION_DIALOG_CHOICE_UNKNOWN_QUANTITY = "Unknown ({calc_type})"
// and shown), tests/presentation/dialogs/test_edit_condition_dialog.py test_a_stored_quantity_the_type_does_not_offer_is_shown_as_
// unknown (unknown type 7 and Count, calc 11: "Unknown (11)", data 11, disabled), ..._can_be_cleared_with_none and
// test_a_stored_quantity_the_type_does_not_offer_survives_a_notes_only_save (the save rules are pinned in test_style_change.cpp:
// CondM08_UnknownStyle).
#include "TestHarness.hpp"

#include "Domain/ConditionQuantityChoices.hpp"
#include "Domain/UomService.hpp"

#include <optional>
#include <string>

using namespace ost_domain;

TEST(CondQtyChoices, AQuantityTheStyleDoesNotOfferIsUnknown)
{
    // the two Windows cases: a type outside 0..3 (7) and Count, both with calc 11 (an Area calculation)
    const std::optional<int> unknown_type = unknown_quantity_calc_type(static_cast<ConditionType>(7), 11);
    REQUIRE(unknown_type.has_value());
    CHECK_EQ(*unknown_type, 11);
    const std::optional<int> unknown_count = unknown_quantity_calc_type(ConditionType::Count, 11);
    REQUIRE(unknown_count.has_value());
    CHECK_EQ(*unknown_count, 11);
    // the same rule for the other styles
    CHECK(unknown_quantity_calc_type(ConditionType::Linear, 11).has_value());
    CHECK(unknown_quantity_calc_type(ConditionType::Area, 1).has_value());
    CHECK(unknown_quantity_calc_type(ConditionType::Attachment, 11).has_value());
    CHECK(unknown_quantity_calc_type(ConditionType::Area, 999).has_value());
}

TEST(CondQtyChoices, AnOfferedQuantityOrNoneIsNotUnknown)
{
    CHECK_FALSE(unknown_quantity_calc_type(ConditionType::Count, 0).has_value());
    CHECK_FALSE(unknown_quantity_calc_type(ConditionType::Count, CALC_COUNT).has_value());
    CHECK_FALSE(unknown_quantity_calc_type(ConditionType::Attachment, CALC_COUNT).has_value());
    CHECK_FALSE(unknown_quantity_calc_type(ConditionType::Linear, 1).has_value());
    CHECK_FALSE(unknown_quantity_calc_type(ConditionType::Area, 11).has_value());
    // an unsupported type is offered the count options (get_quantity_options_for_type)
    CHECK_FALSE(unknown_quantity_calc_type(static_cast<ConditionType>(7), CALC_COUNT).has_value());
    // every option of every style is offered
    for (ConditionType style : {ConditionType::Linear, ConditionType::Area, ConditionType::Count, ConditionType::Attachment,
                                static_cast<ConditionType>(7), static_cast<ConditionType>(-1)})
        for (const auto &option : get_quantity_options_for_type(style))
            CHECK_FALSE(unknown_quantity_calc_type(style, option.first).has_value());
}

TEST(CondQtyChoices, TheEntryTextIsUnknownWithTheStoredNumber)
{
    CHECK_STR_EQ(unknown_quantity_label(11), "Unknown (11)");
    CHECK_STR_EQ(unknown_quantity_label(999), "Unknown (999)");
    CHECK_STR_EQ(unknown_quantity_label(-3), "Unknown (-3)");
}

// the label of the Results row: none, the option's label, or the unknown entry (the old row showed "(none)" for a stored quantity
// it could not name)
TEST(CondQtyChoices, TheRowLabelNamesTheQuantity)
{
    CHECK_STR_EQ(condition_quantity_label(ConditionType::Count, 0), "(none)");
    CHECK_STR_EQ(condition_quantity_label(ConditionType::Count, CALC_COUNT), "Count");
    CHECK_STR_EQ(condition_quantity_label(ConditionType::Area, 11), "Area");
    CHECK_STR_EQ(condition_quantity_label(ConditionType::Linear, 1), "Length");
    CHECK_STR_EQ(condition_quantity_label(ConditionType::Count, 11), "Unknown (11)");
    CHECK_STR_EQ(condition_quantity_label(static_cast<ConditionType>(7), 11), "Unknown (11)");
}
