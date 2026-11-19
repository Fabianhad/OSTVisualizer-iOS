// QTY-23: the Conditions list labels. Windows shows condition.name verbatim in the sidebar (elevation stripping is only
// used for the CSV export and hover text) and titles a condition-type group whose uid has no CdnTypes row "Unknown";
// iOS stripped everything after the last " @T", " @B", " @ " or " @" from the row title and titled that group
// "(unassigned)". Decision 2026-11-16: option (a), the full name and "Unknown" (strict Windows parity).
//
// Windows oracle: presentation/components/conditions_sidebar.py _create_condition_item (display_name = cond.name) and
// _build_cdn_type_groups (cdn_name_map[cdn_uid] = conds[0].cdn_type_name, "(unassigned)" without a uid);
// infrastructure/mdb/components/bid_data_reader.py:913 (cdn_type_name = "Unknown" when the CdnTypes row is missing);
// tests/presentation/components/test_conditions_sidebar.py::test_condition_sidebar_cdn_type_group_rows_are_bold.
//
// The label rules live in the Core condition tree (Domain/ConditionTree.hpp): the Condition node of build_condition_tree
// carries the name to show and the TypeGroup node the group title. ConditionsViewController.mm must show exactly those.
#include "TestHarness.hpp"

#include "Domain/CdnType.hpp"
#include "Domain/Condition.hpp"
#include "Domain/ConditionFolder.hpp"
#include "Domain/ConditionTree.hpp"

#include <string>
#include <unordered_map>
#include <vector>

using namespace ost_domain;

namespace
{
    Condition make_condition(const std::string &uid, const std::string &name, int ref_no, const std::string &type_uid)
    {
        Condition condition;
        condition.uid = uid;
        condition.name = name;
        condition.ref_no = ref_no;
        condition.cdn_type_uid = type_uid;
        return condition;
    }

    // "<depth>|<G or C>|<uid>|<name>" per node, group and condition nodes only (there are no folders in these tests)
    std::vector<std::string> describe(const std::vector<ConditionTreeNode> &nodes)
    {
        std::vector<std::string> lines;
        for (const ConditionTreeNode &node : nodes)
            lines.push_back(std::to_string(node.depth) + (node.kind == ConditionTreeNode::Kind::TypeGroup ? "|G|" : "|C|") +
                            node.uid + "|" + node.name);
        return lines;
    }
}

TEST(Qty23_ConditionLabels, EveryRowIsTitledWithTheFullConditionName)
{
    // test plan of QTY-23 / COND-NAME-01 / UI-NAME-01: no elevation-suffix stripping, whatever the marker or the value
    const std::vector<std::string> names = {
        "Wall @ Level 2",     "Fdn1 @T 10' 6\"", "Note @ later",  "Wall @T 12' 6\"", " @T 6\"",
        "Beam @B",            "Slab @",          "A @T 1 @B 2",   "Plain",           "Wall @T",
        "Wall @B 3' 1/2\"",   "Wall @ ",         "Wall@T 5",      "Wall @t 5",
    };
    std::unordered_map<std::string, Condition> conditions;
    for (size_t i = 0; i < names.size(); ++i)
        conditions[std::to_string(i + 1)] = make_condition(std::to_string(i + 1), names[i], static_cast<int>(i + 1), "");

    const std::vector<ConditionTreeNode> nodes = build_condition_tree(conditions, {}, {});
    REQUIRE_EQ(nodes.size(), names.size() + 1);
    CHECK(nodes[0].kind == ConditionTreeNode::Kind::TypeGroup);
    for (size_t i = 0; i < names.size(); ++i)
    {
        SCOPED_CONTEXT("name \"" + names[i] + "\"");
        CHECK(nodes[i + 1].kind == ConditionTreeNode::Kind::Condition);
        CHECK_STR_EQ(nodes[i + 1].name, names[i]);
    }
}

TEST(Qty23_ConditionLabels, ATypeGroupWhoseUidHasNoCdnTypeRowIsUnknown)
{
    std::unordered_map<std::string, Condition> conditions;
    conditions["1"] = make_condition("1", "Footing", 1, "T1");   // known type
    conditions["2"] = make_condition("2", "Orphan", 2, "T9");    // uid without a CdnTypes row
    conditions["3"] = make_condition("3", "No type", 3, "");     // no uid at all
    std::unordered_map<std::string, CdnType> types;
    types["T1"] = CdnType{"T1", "Foundations"};

    const std::vector<std::string> expected = {
        "0|G||(unassigned)", "1|C|3|No type", "0|G|T1|Foundations", "1|C|1|Footing", "0|G|T9|Unknown", "1|C|2|Orphan",
    };
    CHECK_EQ(describe(build_condition_tree(conditions, {}, types)), expected);
}

TEST(Qty23_ConditionLabels, TheUnknownAndUnassignedTitlesAreTheWindowsTexts)
{
    CHECK_STR_EQ(condition_group_unknown_title(), "Unknown");
    CHECK_STR_EQ(condition_group_unassigned_title(), "(unassigned)");
}

TEST(Qty23_ConditionLabels, ANamedTypeKeepsItsOwnNameEvenWhenItLooksLikeTheFallbackTexts)
{
    std::unordered_map<std::string, Condition> conditions;
    conditions["1"] = make_condition("1", "A", 1, "T1");
    std::unordered_map<std::string, CdnType> types;
    types["T1"] = CdnType{"T1", "Unknown"};
    const std::vector<ConditionTreeNode> nodes = build_condition_tree(conditions, {}, types);
    REQUIRE_EQ(nodes.size(), 2u);
    CHECK_STR_EQ(nodes[0].name, "Unknown");
    CHECK_EQ(nodes[0].uid, std::string("T1"));
}
