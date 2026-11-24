// UI decisions of the Conditions list and the folder manager (plan 5.1 and 5.2, stream S1): the rows of the folder manager and the picker, the
// "Next number" line of the create form, the title of "New Condition in <folder>", the refusal dialogs of a typed folder name and the alert
// of a refused or failed folder write. No database here except the data-safety line over the real writers: the Objective-C layer only converts
// types, shows these and calls the writers, so each decision is pinned here.
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/presentation/components/conditions_sidebar.py:423-513 (the tree the rows come from), :1455-1481 (the rename refusals)
//   ost_visualizer/application/dtos/create_condition_spec_dto.py:5-43 and infrastructure/mdb/components/condition_operations.py:42-66 (no ref_no in
//     the spec: the writer gives MAX(RefNo of the bid) + 1; register NC-1)
//   ost_visualizer/presentation/utils/messagebox.py:78-119 (the delete texts), condition_action_handler.py:1410-1412, :1453-1455, :1574-1575 (Windows
//     only logs a failed folder write: the iOS alert is register CF-7)
// The iOS-only texts (plan 4.14) are pinned verbatim here so that rewording one is a visible decision.
#include "ConditionFoldersTestSupport.hpp"

#include "Domain/ConditionCreate.hpp"
#include "Domain/ConditionFolderUi.hpp"
#include "Domain/ConditionTree.hpp"
#include "Persistence/ProjectStore/ProjectStore_ConditionFolderAlerts.hpp"

#include <string>
#include <vector>

using ost_domain::CdnType;
using ost_domain::Condition;
using ost_domain::ConditionFolder;
using ost_domain::ConditionFolderRow;
using ost_domain::DialogText;
using ost_domain::FolderDeleteItem;
using ost_domain::FolderNameCheck;
using ost_domain::FolderNameIssue;
using ost_store::ConditionFolderAction;
using ost_store::ConditionFolderRefusal;
using ost_store::ConditionFolderResult;

namespace
{
    using FolderMap = std::unordered_map<std::string, ConditionFolder>;
    using ConditionMap = std::unordered_map<std::string, Condition>;
    using TypeMap = std::unordered_map<std::string, CdnType>;

    void add_folder(FolderMap &folders, const std::string &uid, const std::string &name, const std::string &parent)
    {
        folders[uid] = ConditionFolder{uid, name, parent};
    }

    void add_condition(ConditionMap &conditions, const std::string &uid, const std::string &folder, int ref_no = 0, const std::string &cdn_type = "")
    {
        Condition condition;
        condition.uid = uid;
        condition.name = "c" + uid;
        condition.condition_folder_uid = folder;
        condition.ref_no = ref_no;
        condition.cdn_type_uid = cdn_type;
        conditions[uid] = condition;
    }

    std::string describe(const std::vector<ConditionFolderRow> &rows)
    {
        std::string out;
        for (const ConditionFolderRow &row : rows)
            out += row.uid + ":" + row.name + ":d" + std::to_string(row.depth) + ":n" + std::to_string(row.condition_count) + ";";
        return out;
    }

    std::string join(const std::string &a, const std::string &b) { return a + "|" + b; }

    std::string dialog_text(const std::optional<DialogText> &dialog)
    {
        return dialog ? join(dialog->title, dialog->message) : std::string("<none>");
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the rows of the folder manager and of the "Move to Folder" picker
// ---------------------------------------------------------------------------------------------------------------------------------

// conditions_sidebar.py:423-513: nested folders, siblings sorted by name in code point order ("Floors" before "Walls"), a sub-folder directly after its
// parent; the count is the conditions placed DIRECTLY in the folder: a condition of a sub-folder, a condition at the top level, one with an unknown
// folder uid ("ghost") and one with the text "0" count nowhere.
TEST(ConditionFolderRows, NestedFoldersInTreeOrderWithDirectCounts)
{
    FolderMap folders;
    add_folder(folders, "1", "Walls", "");
    add_folder(folders, "2", "Interior", "1");
    add_folder(folders, "3", "Floors", "");
    add_folder(folders, "4", "Empty", "3");
    ConditionMap conditions;
    add_condition(conditions, "10", "1");
    add_condition(conditions, "11", "2");
    add_condition(conditions, "12", "2");
    add_condition(conditions, "13", "");
    add_condition(conditions, "14", "ghost");
    add_condition(conditions, "15", "0");
    const std::vector<ConditionFolderRow> rows = ost_domain::condition_folder_rows(folders, conditions, TypeMap{});
    CHECK_STR_EQ(describe(rows), "3:Floors:d0:n0;4:Empty:d1:n0;1:Walls:d0:n1;2:Interior:d1:n2;");
}

// No folders: no rows, whatever the conditions are.
TEST(ConditionFolderRows, NoFoldersNoRows)
{
    ConditionMap conditions;
    add_condition(conditions, "10", "");
    add_condition(conditions, "11", "5");
    CHECK(ost_domain::condition_folder_rows(FolderMap{}, conditions, TypeMap{}).empty());
    CHECK(ost_domain::condition_folder_rows(FolderMap{}, ConditionMap{}, TypeMap{}).empty());
}

// A folder whose parent is not a folder of the bid ("ghost", "0") is a top-level folder (conditions_sidebar.py:469-475), like in the tree.
TEST(ConditionFolderRows, AFolderWithAnUnknownParentIsTopLevel)
{
    FolderMap folders;
    add_folder(folders, "1", "A", "ghost");
    add_folder(folders, "2", "B", "0");
    add_folder(folders, "3", "C", "1");
    const std::vector<ConditionFolderRow> rows = ost_domain::condition_folder_rows(folders, ConditionMap{}, TypeMap{});
    CHECK_STR_EQ(describe(rows), "1:A:d0:n0;3:C:d1:n0;2:B:d0:n0;");
}

// Equal names keep database read order (the numeric uid), whatever the order of the map is.
TEST(ConditionFolderRows, EqualNamesKeepDatabaseReadOrder)
{
    FolderMap folders;
    add_folder(folders, "10", "Same", "");
    add_folder(folders, "9", "Same", "");
    add_folder(folders, "100", "Same", "");
    const std::vector<ConditionFolderRow> rows = ost_domain::condition_folder_rows(folders, ConditionMap{}, TypeMap{});
    CHECK_STR_EQ(describe(rows), "9:Same:d0:n0;10:Same:d0:n0;100:Same:d0:n0;");
}

// The rows are exactly the Folder nodes of the Conditions tree (same order, same depth): the list and the manager can never disagree.
TEST(ConditionFolderRows, AgreeWithTheFolderNodesOfTheTree)
{
    FolderMap folders;
    add_folder(folders, "1", "b", "");
    add_folder(folders, "2", "a", "1");
    add_folder(folders, "3", "c", "1");
    add_folder(folders, "4", "A", "");
    add_folder(folders, "5", "z", "3");
    ConditionMap conditions;
    add_condition(conditions, "10", "5", 1, "7");
    add_condition(conditions, "11", "2", 2);
    add_condition(conditions, "12", "", 3, "7");
    TypeMap types;
    types["7"] = CdnType{"7", "Walls"};
    const std::vector<ost_domain::ConditionTreeNode> nodes = ost_domain::build_condition_tree(conditions, folders, types);
    std::vector<std::string> expected;
    for (const ost_domain::ConditionTreeNode &node : nodes)
        if (node.kind == ost_domain::ConditionTreeNode::Kind::Folder)
            expected.push_back(node.uid + ":" + node.name + ":d" + std::to_string(node.depth));
    const std::vector<ConditionFolderRow> rows = ost_domain::condition_folder_rows(folders, conditions, types);
    REQUIRE_EQ(rows.size(), expected.size());
    for (std::size_t i = 0; i < rows.size(); ++i)
        CHECK_STR_EQ(rows[i].uid + ":" + rows[i].name + ":d" + std::to_string(rows[i].depth), expected[i]);
}

// CF-10: a chain of 8000 nested folders (Windows reads it) gives 8000 rows, one level deeper each, without recursion; the condition in the deepest
// folder is counted there.
TEST(ConditionFolderRows, AChainOf8000FoldersGivesOneRowPerLevel)
{
    FolderMap folders;
    for (int i = 1; i <= 8000; ++i)
        add_folder(folders, std::to_string(i), "f" + std::to_string(i), i == 1 ? std::string() : std::to_string(i - 1));
    ConditionMap conditions;
    add_condition(conditions, "1", "8000");
    const std::vector<ConditionFolderRow> rows = ost_domain::condition_folder_rows(folders, conditions, TypeMap{});
    REQUIRE_EQ(rows.size(), static_cast<std::size_t>(8000));
    CHECK_EQ(rows.front().depth, 0);
    CHECK_STR_EQ(rows.front().uid, "1");
    CHECK_EQ(rows.back().depth, 7999);
    CHECK_STR_EQ(rows.back().uid, "8000");
    CHECK_EQ(rows.back().condition_count, 1);
    CHECK_EQ(rows[4000].condition_count, 0);
}

// The count line of a row.
TEST(ConditionFolderRows, TheCountText)
{
    CHECK_STR_EQ(ost_domain::condition_folder_count_text(0), "No conditions");
    CHECK_STR_EQ(ost_domain::condition_folder_count_text(1), "1 condition");
    CHECK_STR_EQ(ost_domain::condition_folder_count_text(2), "2 conditions");
    CHECK_STR_EQ(ost_domain::condition_folder_count_text(1000), "1000 conditions");
    CHECK_STR_EQ(ost_domain::condition_folder_count_text(-3), "No conditions");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// New Condition: the title of the long-press item and the "Next number" line
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(NewConditionTexts, TheLongPressItemNamesTheFolder)
{
    CHECK_STR_EQ(ost_domain::new_condition_action_title("", ""), "New Condition");
    CHECK_STR_EQ(ost_domain::new_condition_action_title("", "ignored at the top level"), "New Condition");
    CHECK_STR_EQ(ost_domain::new_condition_action_title("5", "Walls"), "New Condition in Walls");
    CHECK_STR_EQ(ost_domain::new_condition_action_title("5", ""), "New Condition in Folder");
    CHECK_STR_EQ(ost_domain::new_condition_action_title("5", "Gr\xC3\xBC" "n \xF0\x9F\x8F\xA0"), "New Condition in Gr\xC3\xBC" "n \xF0\x9F\x8F\xA0");
}

// N-M1-07 / NC-1: the number the writer will give: the highest + 1 (MAX(RefNo) + 1, condition_operations.py:42-66), 1 for a bid without numbers.
TEST(NewConditionTexts, NextNumberIsTheHighestPlusOne)
{
    ConditionMap conditions;
    CHECK_STR_EQ(ost_domain::next_condition_number_text(conditions), "Next number: 1");
    add_condition(conditions, "1", "", 1);
    add_condition(conditions, "2", "", 2);
    add_condition(conditions, "3", "", 5); // a gap is never filled
    CHECK_STR_EQ(ost_domain::next_condition_number_text(conditions), "Next number: 6");
}

TEST(NewConditionTexts, NumbersWithoutValueDoNotCount)
{
    ConditionMap zero;
    add_condition(zero, "1", "", 0);
    add_condition(zero, "2", "", 0);
    CHECK_STR_EQ(ost_domain::next_condition_number_text(zero), "Next number: 1");
    ConditionMap negative;
    add_condition(negative, "1", "", -5);
    CHECK_STR_EQ(ost_domain::next_condition_number_text(negative), "Next number: 1");
    add_condition(negative, "2", "", 3);
    CHECK_STR_EQ(ost_domain::next_condition_number_text(negative), "Next number: 4");
}

// The highest possible number: the line is computed in 64 bit (it must not wrap to a negative int); the writer refuses that create with the
// Windows text (register NC-9), the form only shows the number.
TEST(NewConditionTexts, NextNumberDoesNotOverflow)
{
    ConditionMap conditions;
    add_condition(conditions, "1", "", 2147483647);
    CHECK_STR_EQ(ost_domain::next_condition_number_text(conditions), "Next number: 2147483648");
    add_condition(conditions, "2", "", 2147483646);
    CHECK_STR_EQ(ost_domain::next_condition_number_text(conditions), "Next number: 2147483648");
}

// The line equals what the writer assigns: the first create on a bid with numbers 1, 2 and 5 gets 6.
TEST(NewConditionTexts, NextNumberEqualsTheNumberTheWriterAssigns)
{
    ost_ctest::TempDb db("folder_ui_next_number");
    REQUIRE(db.schema_ok());
    ost_ftest::seed_bids(db);
    ConditionMap conditions;
    for (const int ref : {1, 2, 5})
    {
        ost_ftest::add_condition(db, 100 + ref, 1, "", ref);
        add_condition(conditions, std::to_string(100 + ref), "", ref);
    }
    ost_domain::CreateConditionSpec spec;
    spec.name = "New";
    std::string error;
    const std::string uid = ost_store::insert_condition(db.path(), "1", spec, &error);
    REQUIRE(!uid.empty());
    CHECK_STR_EQ(ost_domain::next_condition_number_text(conditions),
                 "Next number: " + db.text("SELECT RefNo FROM BidConditions WHERE UID = " + uid));
}

// condition_action_handler.py:764-773: the condition is in the database but the list could not be read again: the Windows warning, verbatim.
TEST(NewConditionTexts, TheRefreshWarningIsTheWindowsOne)
{
    CHECK_STR_EQ(ost_domain::kConditionCreatedRefreshFailedTitle, "Refresh Error");
    CHECK_STR_EQ(ost_domain::kConditionCreatedRefreshFailedText,
                 "The condition was created, but the conditions list could not be refreshed. Reopen the database to see the new condition.");
}

// Review of S1 (register CF-8, the safe behaviour reports what was committed): a folder write that went through but whose list could not be read
// again says that the change was saved (the generic "Failed to load" text says nothing about the write). iOS-only text, pinned so that rewording
// it is a visible decision; the title is the one of the Windows create warning.
TEST(NewConditionTexts, TheRefreshWarningOfAFolderWriteSaysTheChangeWasSaved)
{
    CHECK_STR_EQ(ost_domain::kFolderChangeSavedRefreshFailedTitle, "Refresh Error");
    CHECK_STR_EQ(ost_domain::kFolderChangeSavedRefreshFailedTitle, ost_domain::kConditionCreatedRefreshFailedTitle);
    CHECK_STR_EQ(ost_domain::kFolderChangeSavedRefreshFailedText,
                 "The folder change was saved, but the list could not be refreshed. Reopen the bid to see it.");
}

// Review of S1: the iOS-only texts of the create form's discard alert (Windows asks "Save changes before closing?" and forces the dirty flag,
// register NC-3; the iOS form closes an untouched form silently and asks only for an edited one). Pinned so that rewording is a visible decision.
TEST(NewConditionTexts, TheDiscardAlertOfTheCreateForm)
{
    CHECK_STR_EQ(ost_domain::kDiscardNewConditionTitle, "Unsaved Changes");
    CHECK_STR_EQ(ost_domain::kDiscardNewConditionMessage, "Discard this new condition?");
    CHECK_STR_EQ(ost_domain::kKeepEditingTitle, "Keep Editing");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the folder a condition is listed under (the checkmark of the "Move to Folder" picker)
// ---------------------------------------------------------------------------------------------------------------------------------

// conditions_sidebar.py:461-467: a condition whose folder uid is not a folder of the bid sits at the top level, so the picker checks "(No folder)".
TEST(ConditionListedFolder, AFolderOfTheBidOrTheTopLevel)
{
    FolderMap folders;
    add_folder(folders, "1", "Walls", "");
    ConditionMap conditions;
    add_condition(conditions, "10", "1");
    add_condition(conditions, "11", "");
    add_condition(conditions, "12", "ghost");
    add_condition(conditions, "13", "0");
    CHECK_STR_EQ(ost_domain::condition_listed_folder_uid(folders, conditions["10"]), "1");
    CHECK_STR_EQ(ost_domain::condition_listed_folder_uid(folders, conditions["11"]), "");
    CHECK_STR_EQ(ost_domain::condition_listed_folder_uid(folders, conditions["12"]), "");
    CHECK_STR_EQ(ost_domain::condition_listed_folder_uid(folders, conditions["13"]), "");
    CHECK_STR_EQ(ost_domain::condition_listed_folder_uid(FolderMap{}, conditions["10"]), "");
}

// It is the folder the Conditions tree puts the condition in: the tree and the picker agree for every condition.
TEST(ConditionListedFolder, AgreesWithTheConditionsTree)
{
    FolderMap folders;
    add_folder(folders, "1", "Walls", "");
    add_folder(folders, "2", "Interior", "1");
    ConditionMap conditions;
    add_condition(conditions, "10", "1", 1);
    add_condition(conditions, "11", "2", 2);
    add_condition(conditions, "12", "ghost", 3);
    add_condition(conditions, "13", "", 4);
    const std::vector<ost_domain::ConditionTreeNode> nodes = ost_domain::build_condition_tree(conditions, folders, TypeMap{});
    std::size_t seen = 0;
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        if (nodes[i].kind != ost_domain::ConditionTreeNode::Kind::Condition)
            continue;
        ++seen;
        SCOPED_CONTEXT(nodes[i].uid);
        CHECK_STR_EQ(ost_domain::condition_listed_folder_uid(folders, conditions[nodes[i].uid]), ost_domain::folder_context_uid(nodes, i));
    }
    CHECK_EQ(seen, static_cast<std::size_t>(4));
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the refusal dialog of a typed folder name
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(ConditionFolderNameDialog, EveryIssueHasItsDialogOrIsSilent)
{
    FolderNameCheck check;
    check.issue = FolderNameIssue::None;
    CHECK_STR_EQ(dialog_text(ost_domain::condition_folder_name_refusal_dialog(check)), "<none>");
    check.issue = FolderNameIssue::Blank;
    CHECK_STR_EQ(dialog_text(ost_domain::condition_folder_name_refusal_dialog(check)), "<none>"); // Windows reverts silently (:1466-1469)

    check.issue = FolderNameIssue::InvalidText;
    CHECK_STR_EQ(dialog_text(ost_domain::condition_folder_name_refusal_dialog(check)),
                 "Invalid Folder Name|The folder name contains characters that cannot be saved.");

    check.issue = FolderNameIssue::TooLong;
    check.dialog = DialogText{"Invalid Folder Name", "Folder name must not be longer than 50 characters."};
    CHECK_STR_EQ(dialog_text(ost_domain::condition_folder_name_refusal_dialog(check)),
                 "Invalid Folder Name|Folder name must not be longer than 50 characters.");

    check.issue = FolderNameIssue::DuplicateSibling;
    check.dialog = DialogText{"Duplicate Folder", "Folder Other already exists."};
    CHECK_STR_EQ(dialog_text(ost_domain::condition_folder_name_refusal_dialog(check)), "Duplicate Folder|Folder Other already exists.");
}

// The same through the real name check: what a user types in the New Folder / Rename alert.
TEST(ConditionFolderNameDialog, TypedNamesThroughTheNameCheck)
{
    FolderMap folders;
    add_folder(folders, "1", "Other", "");
    auto dialog_of = [&folders](const std::string &typed)
    { return dialog_text(ost_domain::condition_folder_name_refusal_dialog(ost_domain::check_condition_folder_name(folders, "", "", typed))); };
    CHECK_STR_EQ(dialog_of("Fine"), "<none>");
    CHECK_STR_EQ(dialog_of("   "), "<none>");
    CHECK_STR_EQ(dialog_of(""), "<none>");
    CHECK_STR_EQ(dialog_of("other"), "Duplicate Folder|Folder other already exists."); // the Windows text, with the stripped typed name
    CHECK_STR_EQ(dialog_of("  OTHER "), "Duplicate Folder|Folder OTHER already exists.");
    CHECK_STR_EQ(dialog_of(std::string(51, 'x')), "Invalid Folder Name|Folder name must not be longer than 50 characters.");
    CHECK_STR_EQ(dialog_of(std::string(50, 'x')), "<none>");
    CHECK_STR_EQ(dialog_of(std::string("a\0b", 3)), "Invalid Folder Name|The folder name contains characters that cannot be saved.");
    CHECK_STR_EQ(dialog_of("bad\xFF"), "Invalid Folder Name|The folder name contains characters that cannot be saved.");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the alert of a refused or failed folder write (register CF-7: Windows only logs)
// ---------------------------------------------------------------------------------------------------------------------------------

namespace
{
    ConditionFolderResult refused(ConditionFolderRefusal refusal, const std::string &message = std::string(),
                                  std::vector<std::string> blocked = {})
    {
        ConditionFolderResult result;
        result.ok = false;
        result.refusal = refusal;
        result.message = message;
        result.blocked = std::move(blocked);
        return result;
    }

    std::string failure(ConditionFolderAction action, const ConditionFolderResult &result, const std::vector<FolderDeleteItem> &items = {})
    {
        return dialog_text(ost_store::condition_folder_failure_dialog(action, result, items));
    }

    const ConditionFolderAction kActions[] = {ConditionFolderAction::Create, ConditionFolderAction::Rename, ConditionFolderAction::Delete,
                                              ConditionFolderAction::Move};
    const char *kHint = "The database may be locked by another application.";
}

// A successful write has no alert, and neither has a blank name (the screens do not write one; Windows is silent).
TEST(ConditionFolderFailure, ASuccessAndABlankNameAreSilent)
{
    for (const ConditionFolderAction action : kActions)
    {
        ConditionFolderResult ok;
        ok.ok = true;
        CHECK_STR_EQ(failure(action, ok), "<none>");
        CHECK_STR_EQ(failure(action, refused(ConditionFolderRefusal::BlankName)), "<none>");
    }
}

TEST(ConditionFolderFailure, ALockedBidSaysSo)
{
    for (const ConditionFolderAction action : kActions)
        CHECK_STR_EQ(failure(action, refused(ConditionFolderRefusal::BidLocked)), "Bid Locked|This bid is locked.");
}

// The Windows warning of a sibling with the name (conditions_sidebar.py:1472-1479) when the store refuses it (a name that was free when the
// screen checked); the too-long and invalid-name texts.
TEST(ConditionFolderFailure, TheNameRefusalsKeepTheirTexts)
{
    CHECK_STR_EQ(failure(ConditionFolderAction::Create, refused(ConditionFolderRefusal::DuplicateName, "Folder Walls already exists.")),
                 "Duplicate Folder|Folder Walls already exists.");
    CHECK_STR_EQ(failure(ConditionFolderAction::Rename, refused(ConditionFolderRefusal::DuplicateName, "Folder Walls already exists.")),
                 "Duplicate Folder|Folder Walls already exists.");
    CHECK_STR_EQ(failure(ConditionFolderAction::Create, refused(ConditionFolderRefusal::NameTooLong, "Folder name must not be longer than 50 characters.")),
                 "Invalid Folder Name|Folder name must not be longer than 50 characters.");
    CHECK_STR_EQ(failure(ConditionFolderAction::Rename, refused(ConditionFolderRefusal::InvalidName)),
                 "Invalid Folder Name|The folder name contains characters that cannot be saved.");
}

// The refusals that carry a text of their own: the title of the action and that text.
TEST(ConditionFolderFailure, TheLimitAndSchemaTextsUnderTheTitleOfTheAction)
{
    const std::string windows = "This OST database does not support Condition folder hierarchy persistence.";
    CHECK_STR_EQ(failure(ConditionFolderAction::Create, refused(ConditionFolderRefusal::HierarchyNotSupported, windows)), "New Folder|" + windows);
    const std::string limit = "Cannot add a folder: no identity is left below 2147483647.";
    CHECK_STR_EQ(failure(ConditionFolderAction::Create, refused(ConditionFolderRefusal::UidExhausted, limit)), "New Folder|" + limit);
}

// Review of S1 (mutant F4: a body that always uses result.message survived): a limit or schema refusal that carries NO text is the generic failure of
// the action (the title of the action, the iOS wording and the database-locked hint), never an alert with an empty message. The text of the store
// is used only when the store gave one.
TEST(ConditionFolderFailure, ALimitOrSchemaRefusalWithoutATextIsTheGenericFailureOfTheAction)
{
    for (const ConditionFolderRefusal refusal : {ConditionFolderRefusal::UidExhausted, ConditionFolderRefusal::HierarchyNotSupported})
    {
        SCOPED_CONTEXT(ost_ftest::refusal_name(refusal));
        CHECK_STR_EQ(failure(ConditionFolderAction::Create, refused(refusal)), std::string("New Folder|Failed to create folder. ") + kHint);
        CHECK_STR_EQ(failure(ConditionFolderAction::Rename, refused(refusal)), std::string("Rename Folder|Failed to rename folder. ") + kHint);
        CHECK_STR_EQ(failure(ConditionFolderAction::Move, refused(refusal)), std::string("Move to Folder|Failed to move the condition. ") + kHint);
        CHECK_STR_EQ(failure(ConditionFolderAction::Delete, refused(refusal), {{"A", "1"}}), std::string("Delete Folder|Failed to delete folder. ") + kHint);
    }
}

// Review of S1 (hardening next to mutant F4): a duplicate-name or too-long-name refusal that carries NO text is the generic failure of the action,
// never an alert with a title and an empty body. The real writers always set the text, so this only pins the fallback.
TEST(ConditionFolderFailure, ANameRefusalWithoutATextIsTheGenericFailureOfTheAction)
{
    for (const ConditionFolderRefusal refusal : {ConditionFolderRefusal::DuplicateName, ConditionFolderRefusal::NameTooLong})
    {
        SCOPED_CONTEXT(ost_ftest::refusal_name(refusal));
        CHECK_STR_EQ(failure(ConditionFolderAction::Create, refused(refusal)), std::string("New Folder|Failed to create folder. ") + kHint);
        CHECK_STR_EQ(failure(ConditionFolderAction::Rename, refused(refusal)), std::string("Rename Folder|Failed to rename folder. ") + kHint);
        CHECK_STR_EQ(failure(ConditionFolderAction::Move, refused(refusal)), std::string("Move to Folder|Failed to move the condition. ") + kHint);
        CHECK_STR_EQ(failure(ConditionFolderAction::Delete, refused(refusal), {{"A", "1"}}), std::string("Delete Folder|Failed to delete folder. ") + kHint);
    }
}

// Review of S1: the title of the entry alert of an action ("New Folder", "Rename Folder") and the title of its failure alert are ONE constant, so the
// two cannot drift apart; the empty-state line of the folder screen is pinned with them (iOS-only texts, plan 4.14).
TEST(ConditionFolderFailure, TheEntryAlertAndTheFailureAlertOfAnActionShareTheirTitle)
{
    CHECK_STR_EQ(ost_domain::kNewFolderTitle, "New Folder");
    CHECK_STR_EQ(ost_domain::kRenameFolderTitle, "Rename Folder");
    CHECK_STR_EQ(ost_domain::kNoFoldersText, "No folders. Tap + to add one.");
    const std::optional<DialogText> create =
        ost_store::condition_folder_failure_dialog(ConditionFolderAction::Create, refused(ConditionFolderRefusal::WriteFailed), {});
    REQUIRE(create.has_value());
    CHECK_STR_EQ(create->title, ost_domain::kNewFolderTitle);
    const std::optional<DialogText> rename =
        ost_store::condition_folder_failure_dialog(ConditionFolderAction::Rename, refused(ConditionFolderRefusal::WriteFailed), {});
    REQUIRE(rename.has_value());
    CHECK_STR_EQ(rename->title, ost_domain::kRenameFolderTitle);
}

// Windows messagebox.py:84-105 / project_write_service.py:946-953: the blocked folders are named, one line each; the folder that holds the
// condition may be a sub-folder (CF-11: the text names the selected folder anyway).
TEST(ConditionFolderFailure, AFolderInUseIsNamedWithTheWindowsText)
{
    const std::vector<FolderDeleteItem> items = {{"Floors", "1"}, {"Walls", "2"}, {"Roofs", "3"}};
    CHECK_STR_EQ(failure(ConditionFolderAction::Delete, refused(ConditionFolderRefusal::InUse, "", {"2"}), items),
                 "Delete Folder|Cannot delete \"Walls\" because it is in use.");
    CHECK_STR_EQ(failure(ConditionFolderAction::Delete, refused(ConditionFolderRefusal::InUse, "", {"3", "1"}), items),
                 "Delete Folder|Cannot delete \"Floors\" because it is in use.\nCannot delete \"Roofs\" because it is in use.");
    // the order of the items (the request), not of `blocked`, decides the lines, like Windows (the items are walked)
    CHECK_STR_EQ(failure(ConditionFolderAction::Delete, refused(ConditionFolderRefusal::InUse, "", {"1", "2", "3"}), items),
                 "Delete Folder|Cannot delete \"Floors\" because it is in use.\nCannot delete \"Walls\" because it is in use.\n"
                 "Cannot delete \"Roofs\" because it is in use.");
}

// An InUse the screen cannot name (no item of the request is blocked) is the generic text, never an empty alert.
TEST(ConditionFolderFailure, AnInUseThatCannotBeNamedIsTheGenericText)
{
    CHECK_STR_EQ(failure(ConditionFolderAction::Delete, refused(ConditionFolderRefusal::InUse, "", {"9"}), {{"Walls", "2"}}),
                 std::string("Delete Folder|Failed to delete folder. ") + kHint);
    CHECK_STR_EQ(failure(ConditionFolderAction::Delete, refused(ConditionFolderRefusal::InUse), {}),
                 std::string("Delete Folder|Failed to delete folder. ") + kHint);
}

// Every refusal that has no text of its own: the iOS wording with the database-locked hint (the existing BrowserFailure pattern).
TEST(ConditionFolderFailure, EveryOtherRefusalIsTheGenericFailureOfTheAction)
{
    const ConditionFolderRefusal generic[] = {ConditionFolderRefusal::BidNotFound,    ConditionFolderRefusal::ParentNotFound,
                                              ConditionFolderRefusal::ParentOfAnotherBid, ConditionFolderRefusal::FolderNotFound,
                                              ConditionFolderRefusal::FolderOfAnotherBid, ConditionFolderRefusal::ConditionNotFound,
                                              ConditionFolderRefusal::ConditionOfAnotherBid, ConditionFolderRefusal::InvalidUid,
                                              ConditionFolderRefusal::SchemaUnsupported,  ConditionFolderRefusal::WriteFailed};
    for (const ConditionFolderRefusal refusal : generic)
    {
        SCOPED_CONTEXT(ost_ftest::refusal_name(refusal));
        CHECK_STR_EQ(failure(ConditionFolderAction::Create, refused(refusal)), std::string("New Folder|Failed to create folder. ") + kHint);
        CHECK_STR_EQ(failure(ConditionFolderAction::Rename, refused(refusal)), std::string("Rename Folder|Failed to rename folder. ") + kHint);
        CHECK_STR_EQ(failure(ConditionFolderAction::Delete, refused(refusal), {{"A", "1"}}),
                     std::string("Delete Folder|Failed to delete folder. ") + kHint);
        CHECK_STR_EQ(failure(ConditionFolderAction::Delete, refused(refusal), {{"A", "1"}, {"B", "2"}}),
                     std::string("Delete Folder|Failed to delete folders. ") + kHint);
        CHECK_STR_EQ(failure(ConditionFolderAction::Move, refused(refusal)), std::string("Move to Folder|Failed to move the condition. ") + kHint);
    }
}

// No refusal of the store, for any action, ends in an empty alert: a refusal that is added later and falls through shows up here as an empty title or
// message. (Blank is the one silent refusal.)
TEST(ConditionFolderFailure, NoRefusalGivesAnEmptyAlert)
{
    for (int value = 0; value <= static_cast<int>(ConditionFolderRefusal::WriteFailed); ++value)
    {
        const ConditionFolderRefusal refusal = static_cast<ConditionFolderRefusal>(value);
        if (refusal == ConditionFolderRefusal::None || refusal == ConditionFolderRefusal::BlankName)
            continue;
        // a refusal with a text of the store and one without (review of S1: an empty text must not give an empty body either)
        for (const char *text : {"text", ""})
            for (const ConditionFolderAction action : kActions)
            {
                SCOPED_CONTEXT(std::string(ost_ftest::refusal_name(refusal)) + (*text ? " with text" : " without text"));
                const std::optional<DialogText> dialog = ost_store::condition_folder_failure_dialog(action, refused(refusal, text), {{"A", "1"}});
                REQUIRE(dialog.has_value());
                CHECK(!dialog->title.empty());
                CHECK(!dialog->message.empty());
            }
    }
}

// The whole chain over a real database: a duplicate name gives the Windows warning with the name the writer reports, a blank name is silent, a
// folder that is not there is the generic text and a locked bid gives the lock alert.
TEST(ConditionFolderFailure, TheRealWritersResultsGiveTheExpectedAlerts)
{
    ost_ctest::TempDb db("folder_ui_chain");
    REQUIRE(db.schema_ok());
    ost_ftest::seed_bids(db);
    const ConditionFolderResult first = ost_store::create_condition_folder(db.path(), "1", "Walls", "");
    REQUIRE(first.ok);
    const ConditionFolderResult second = ost_store::create_condition_folder(db.path(), "1", "walls", "");
    REQUIRE(!second.ok);
    CHECK_STR_EQ(failure(ConditionFolderAction::Create, second), "Duplicate Folder|Folder walls already exists.");
    const ConditionFolderResult blank = ost_store::create_condition_folder(db.path(), "1", "   ", "");
    CHECK_STR_EQ(failure(ConditionFolderAction::Create, blank), "<none>");
    const ConditionFolderResult ghost = ost_store::rename_condition_folder(db.path(), "1", "999", "X");
    CHECK_STR_EQ(failure(ConditionFolderAction::Rename, ghost), std::string("Rename Folder|Failed to rename folder. ") + kHint);
    ost_ftest::lock_bid(db, 1);
    const ConditionFolderResult locked = ost_store::create_condition_folder(db.path(), "1", "Roofs", "");
    REQUIRE(!locked.ok);
    CHECK_STR_EQ(failure(ConditionFolderAction::Create, locked), "Bid Locked|This bid is locked.");
}
