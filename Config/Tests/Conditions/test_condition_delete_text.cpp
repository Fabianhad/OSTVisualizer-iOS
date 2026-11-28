// The Delete Condition confirmation text (ConditionDeleteText.hpp), parity pass 3 package 2 (Windows d1c430a).
//
// Windows oracle: ost_visualizer/presentation/texts.py MESSAGEBOX_LABEL_DELETING_THIS_CONDITION_WILL_DELETE_ALL and
// MESSAGEBOX_LABEL_YOU_ARE_DELETING_CONDITIONS, utils/messagebox.py confirm_delete_conditions (singular for one condition, the
// plural sentence with the count otherwise; the title is MESSAGEBOX_TITLE_DELETE = 'Delete "{name}"?'), tests/presentation/utils/
// test_messagebox.py test_one_condition_is_asked_about_with_the_singular_sentence and
// test_several_conditions_are_asked_about_with_the_plural_sentence.
#include "TestHarness.hpp"

#include "Domain/ConditionDeleteText.hpp"

#include <string>

using namespace ost_domain;

TEST(ConditionDeleteText, OneConditionIsAskedAboutWithTheSingularSentence)
{
    CHECK_STR_EQ(condition_delete_message(1),
                 "Deleting this condition will delete all its takeoffs on all pages within this bid.\n\n"
                 "This cannot be undone.\n\nAre you sure you want to delete this condition?");
}

TEST(ConditionDeleteText, SeveralConditionsAreAskedAboutWithThePluralSentenceAndTheCount)
{
    CHECK_STR_EQ(condition_delete_message(3),
                 "You are deleting 3 conditions. Deleting a condition will delete all its takeoffs on all pages within this "
                 "bid.\n\nThis cannot be undone.\n\nAre you sure you want to delete this condition?");
    CHECK_STR_EQ(condition_delete_message(2),
                 "You are deleting 2 conditions. Deleting a condition will delete all its takeoffs on all pages within this "
                 "bid.\n\nThis cannot be undone.\n\nAre you sure you want to delete this condition?");
}

// Windows tests only len(names) == 1; nothing else is singular
TEST(ConditionDeleteText, OnlyExactlyOneConditionIsSingular)
{
    CHECK(condition_delete_message(0).find("You are deleting 0 conditions.") == 0);
    CHECK(condition_delete_message(12).find("You are deleting 12 conditions.") == 0);
    CHECK(condition_delete_message(1).find("Deleting this condition") == 0);
}

TEST(ConditionDeleteText, TheTitleQuotesTheNameAndIsUnchanged)
{
    CHECK_STR_EQ(condition_delete_title("Footing"), "Delete \"Footing\"?");
    CHECK_STR_EQ(condition_delete_title(""), "Delete \"\"?");
}
