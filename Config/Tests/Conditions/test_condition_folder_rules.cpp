// CF-1, CF-3, CF-5, CF-6, CF-10, CF-11, CF-12 (plan 5.2): the pure rules of the condition folders: the name rule (strip, blank, length,
// sibling duplicates), the rename decision, the parent of a new folder, THE in-use rule of the delete (the service rule, not the Access
// layer's) and the texts of the delete confirmation. No database here: tests/Conditions/test_condition_folder_write.cpp and
// test_condition_folder_delete.cpp run the same rules through the store.
//
// Windows sources (HEAD 49a2d59):
//   ost_visualizer/presentation/components/conditions_sidebar.py:1455-1497 (rename rule), :1499-1513 and :989-1001 (the parent of a new folder)
//   ost_visualizer/application/services/project_write_service.py:855-864, :936-953 (the in-use rule)
//   ost_visualizer/presentation/handlers/condition_action_handler.py:1514-1575 (delete flow), :1392 (the default name)
//   ost_visualizer/presentation/utils/messagebox.py:78-119 confirm_multi_delete
// The oracle rows (ConditionFolderOracleData.hpp) were produced by running the real Windows code (see that header).
#include "ConditionFolderOracleData.hpp"
#include "ConditionFoldersTestSupport.hpp"

#include "Domain/ConditionCreate.hpp"
#include "Domain/ConditionTree.hpp"
#include "Import/TextRules.hpp"

#include <chrono>
#include <set>

using ost_domain::Condition;
using ost_domain::ConditionFolder;
using ost_domain::FolderNameCheck;
using ost_domain::FolderNameIssue;
using ost_domain::FolderRenameDecision;

namespace
{
    using FolderMap = std::unordered_map<std::string, ConditionFolder>;
    using ConditionMap = std::unordered_map<std::string, Condition>;

    FolderMap folders_of(const std::vector<ost_ftest::OracleFolder> &rows)
    {
        FolderMap folders;
        for (const ost_ftest::OracleFolder &row : rows)
            folders[row.uid] = ConditionFolder{row.uid, row.name, row.parent};
        return folders;
    }

    ConditionMap conditions_of(const std::vector<std::pair<const char *, const char *>> &rows)
    {
        ConditionMap conditions;
        for (const auto &row : rows)
        {
            Condition condition;
            condition.uid = row.first;
            condition.name = row.first;
            condition.condition_folder_uid = row.second;
            conditions[condition.uid] = condition;
        }
        return conditions;
    }

    std::vector<std::string> strings_of(const std::vector<const char *> &items)
    {
        return std::vector<std::string>(items.begin(), items.end());
    }

    std::string strip(const std::string &text) { return ost_import::text_rules::py_strip(text); }

    const char *issue_name(FolderNameIssue issue)
    {
        switch (issue)
        {
        case FolderNameIssue::None: return "None";
        case FolderNameIssue::Blank: return "Blank";
        case FolderNameIssue::InvalidText: return "InvalidText";
        case FolderNameIssue::TooLong: return "TooLong";
        case FolderNameIssue::DuplicateSibling: return "DuplicateSibling";
        }
        return "?";
    }

    const char *decision_name(FolderRenameDecision decision)
    {
        switch (decision)
        {
        case FolderRenameDecision::Refused: return "Refused";
        case FolderRenameDecision::Unchanged: return "Unchanged";
        case FolderRenameDecision::Write: return "Write";
        }
        return "?";
    }

    // The rename decision of `folder_uid` to `typed`, with the outputs pre-set to values no implementation returns (a function that does not
    // write its outputs fails the checks that follow).
    struct Rename
    {
        FolderRenameDecision decision = FolderRenameDecision::Write;
        std::string new_name = "<stale new_name>";
        FolderNameCheck check;
    };

    Rename rename_of(const FolderMap &folders, const std::string &folder_uid, const std::string &typed)
    {
        Rename out;
        out.check.issue = FolderNameIssue::DuplicateSibling;
        out.check.name = "<stale check.name>";
        out.check.dialog = ost_domain::DialogText{"<stale title>", "<stale message>"};
        out.decision = ost_domain::decide_condition_folder_rename(folders, folder_uid, typed, out.new_name, &out.check);
        return out;
    }

    // `count` repetitions of a UTF-8 text.
    std::string repeat(const std::string &unit, std::size_t count)
    {
        std::string out;
        for (std::size_t i = 0; i < count; ++i)
            out += unit;
        return out;
    }

    const char *kEmoji = "\xF0\x9F\x98\x80"; // U+1F600: 4 UTF-8 bytes, 2 UTF-16 units

    const std::string kTooLongTitle = "Invalid Folder Name";
    const std::string kTooLongMessage = "Folder name must not be longer than 50 characters.";

    // Everything Python's str.strip() removes (str.isspace(), verified with Python 3.11) and a few look-alikes it keeps.
    const std::vector<std::string> &blank_texts()
    {
        static const std::vector<std::string> texts = {
            "", " ", "   ", "\t", "\n", "\r", "\x0b", "\x0c", "\x1c", "\x1d", "\x1e", "\x1f", "\xC2\x85", "\xC2\xA0", "\xE1\x9A\x80",
            "\xE2\x80\x80", "\xE2\x80\x83", "\xE2\x80\x8A", "\xE2\x80\xA8", "\xE2\x80\xA9", "\xE2\x80\xAF", "\xE2\x81\x9F", "\xE3\x80\x80",
            " \t\r\n\x1c\x1d\x1e\x1f\xC2\x85\xE3\x80\x80 "};
        return texts;
    }

    const std::vector<std::string> &not_blank_texts()
    {
        static const std::vector<std::string> texts = {"\xE2\x80\x8B" /* ZWSP */, "\xE1\xA0\x8E" /* MVS */, "\xEF\xBB\xBF" /* BOM */,
                                                       "\xC2\xAD" /* soft hyphen */, "\xE2\x80\x8C" /* ZWNJ */, "\xE2\x81\xA0" /* word joiner */, "."};
        return texts;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// constants and texts
// ---------------------------------------------------------------------------------------------------------------------------------

// condition_action_handler.py:1392 / :1408 (the name of a new folder), database_creator.py:142 (VARCHAR(50)), messagebox.py:78 title,
// condition_folder_operations.py:26-30 (the hierarchy text); the iOS line is the plan's 4.14 text.
TEST(ConditionFolderRules, TheConstantsAreTheWindowsTextsAndTheIosLine)
{
    CHECK_STR_EQ(ost_domain::kDefaultConditionFolderName, "New Folder");
    CHECK_EQ(ost_domain::kMaxConditionFolderNameUnits, size_t(50));
    CHECK_STR_EQ(ost_domain::kConditionFolderDeleteTitle, "Delete Folder");
    CHECK_STR_EQ(ost_domain::kConditionFolderSubfoldersMoveText, "Folders inside it move to the top level.");
    CHECK_STR_EQ(ost_domain::kConditionFolderHierarchyNotSupportedText,
                 "This OST database does not support Condition folder hierarchy persistence.");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the parent a folder is listed under (conditions_sidebar.py:469-475)
// ---------------------------------------------------------------------------------------------------------------------------------

// `parent = folder.parent_uid if folder.parent_uid in self._folders else None`: empty, "0" and a parent that is not a folder of the bid all list
// the folder at the root; a known parent is kept.
TEST(ConditionFolderRules, AFolderIsListedAtTheRootUnlessItsParentIsAFolderOfTheBid)
{
    const FolderMap folders = {{"1", {"1", "Root", ""}},       {"2", {"2", "Kid", "1"}},      {"3", {"3", "Zero", "0"}},
                               {"4", {"4", "Dangling", "99"}}, {"5", {"5", "Blank", ""}},     {"6", {"6", "Deep", "2"}}};
    const std::vector<std::pair<std::string, std::string>> expected = {{"1", ""}, {"2", "1"}, {"3", ""}, {"4", ""}, {"5", ""}, {"6", "2"}};
    for (const auto &row : expected)
    {
        SCOPED_CONTEXT("folder " + row.first);
        CHECK_STR_EQ(ost_domain::effective_condition_folder_parent(folders, folders.at(row.first)), row.second);
    }
    // a folder whose parent is another bid's folder is not in `folders` (one bid): root
    const FolderMap one_bid = {{"1", {"1", "A", "7"}}};
    CHECK_STR_EQ(ost_domain::effective_condition_folder_parent(one_bid, one_bid.at("1")), "");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the rename rule (conditions_sidebar.py:1455-1497)
// ---------------------------------------------------------------------------------------------------------------------------------

// Port of tests/presentation/components/test_conditions_sidebar.py:1246 test_condition_folder_rename_emits_trimmed_name_and_keeps_label_authoritative
// (folders f1 'Original', f2 'Other'; typed "  Renamed  " -> ("f1", "Renamed"); "   " -> nothing; "Original" -> nothing; "other" -> exactly one
// warning "Duplicate Folder"), asserted on the decision of decide_condition_folder_rename.
TEST(ConditionFolderRules, TheSidebarRenameTestOfWindowsOnTheDecision)
{
    const FolderMap folders = {{"f1", {"f1", "Original", ""}}, {"f2", {"f2", "Other", ""}}};

    const Rename trimmed = rename_of(folders, "f1", "  Renamed  ");
    CHECK_STR_EQ(decision_name(trimmed.decision), "Write");
    CHECK_STR_EQ(trimmed.new_name, "Renamed");
    CHECK_STR_EQ(issue_name(trimmed.check.issue), "None");
    CHECK_STR_EQ(trimmed.check.name, "Renamed");
    CHECK_STR_EQ(trimmed.check.dialog.title, "");
    CHECK_STR_EQ(trimmed.check.dialog.message, "");

    const Rename blank = rename_of(folders, "f1", "   ");
    CHECK_STR_EQ(decision_name(blank.decision), "Refused");
    CHECK_STR_EQ(blank.new_name, "");
    CHECK_STR_EQ(issue_name(blank.check.issue), "Blank");
    CHECK_STR_EQ(blank.check.dialog.title, ""); // silent: Windows reverts the label and shows nothing
    CHECK_STR_EQ(blank.check.dialog.message, "");

    const Rename unchanged = rename_of(folders, "f1", "Original");
    CHECK_STR_EQ(decision_name(unchanged.decision), "Unchanged");
    CHECK_STR_EQ(unchanged.new_name, "Original");
    CHECK_STR_EQ(issue_name(unchanged.check.issue), "None");

    const Rename duplicate = rename_of(folders, "f1", "other");
    CHECK_STR_EQ(decision_name(duplicate.decision), "Refused");
    CHECK_STR_EQ(issue_name(duplicate.check.issue), "DuplicateSibling");
    CHECK_STR_EQ(duplicate.new_name, "other");
    CHECK_STR_EQ(duplicate.check.dialog.title, "Duplicate Folder");
    CHECK_STR_EQ(duplicate.check.dialog.message, "Folder other already exists.");
}

// Every row of the oracle: the real Windows sidebar driven headless (strip, blank, unchanged, sibling duplicates with Python's lower(), a folder
// shown at the root because its parent is dangling or "0", names in other scripts, Final_Sigma, U+0130, U+0085, tab and line feed inside a name).
TEST(ConditionFolderRules, TheRenameRuleMatchesTheRealWindowsSidebarOnEveryOracleRow)
{
    int index = 0;
    for (const ost_ftest::OracleRenameCase &row : ost_ftest::rename_cases())
    {
        SCOPED_CONTEXT("oracle row " + std::to_string(index++) + " group " + row.group + " typed " + ost_test::detail::quote(row.typed));
        const FolderMap folders = folders_of(row.folders);
        const Rename got = rename_of(folders, row.folder_uid, row.typed);
        const std::string stripped = strip(row.typed);
        CHECK_STR_EQ(got.new_name, stripped);
        if (row.outcome == 'W')
        {
            CHECK_STR_EQ(decision_name(got.decision), "Write");
            CHECK_STR_EQ(got.new_name, row.text1);
            CHECK_STR_EQ(issue_name(got.check.issue), "None");
        }
        else if (row.outcome == 'S')
        {
            // silent: a blank text is refused (Blank), the stored name again is unchanged
            if (stripped.empty())
            {
                CHECK_STR_EQ(decision_name(got.decision), "Refused");
                CHECK_STR_EQ(issue_name(got.check.issue), "Blank");
            }
            else
            {
                CHECK_STR_EQ(decision_name(got.decision), "Unchanged");
                CHECK_STR_EQ(issue_name(got.check.issue), "None");
                CHECK_STR_EQ(stripped, folders.at(row.folder_uid).name);
            }
        }
        else
        {
            CHECK_STR_EQ(decision_name(got.decision), "Refused");
            CHECK_STR_EQ(issue_name(got.check.issue), "DuplicateSibling");
            CHECK_STR_EQ(got.check.dialog.title, row.text1);
            CHECK_STR_EQ(got.check.dialog.message, row.text2);
        }
    }
}

// The same rule for a NEW folder (plan N-M1-15: the sibling rule also at create; exclude_uid is ""): the typed text is stripped, a blank one is
// refused silently, a sibling with the same stripped lower-cased name (same effective parent) is the Windows warning.
TEST(ConditionFolderRules, ANewFolderFollowsTheSameNameRule)
{
    const FolderMap folders = {{"p", {"p", "Parent", ""}},   {"k", {"k", "Kid", "p"}},       {"q", {"q", "Other parent", ""}},
                               {"d", {"d", "Dangling", "ghost"}}, {"z", {"z", "Zero", "0"}}};
    struct Row
    {
        const char *parent;
        const char *typed;
        FolderNameIssue issue;
        const char *name;
    };
    const Row rows[] = {
        {"", "  Walls  ", FolderNameIssue::None, "Walls"},
        {"", "   ", FolderNameIssue::Blank, ""},
        {"", "parent", FolderNameIssue::DuplicateSibling, "parent"},        // the root sibling 'Parent'
        {"", "DANGLING ", FolderNameIssue::DuplicateSibling, "DANGLING"},    // a folder shown at the root because its parent is dangling
        {"", "zero", FolderNameIssue::DuplicateSibling, "zero"},             // parent "0" lists it at the root too
        {"", "kid", FolderNameIssue::None, "kid"},                           // 'Kid' is a child of 'p', not a root sibling
        {"p", "KID", FolderNameIssue::DuplicateSibling, "KID"},
        {"p", "Parent", FolderNameIssue::None, "Parent"},                    // the same name as the PARENT is fine: only siblings count
        {"q", "kid", FolderNameIssue::None, "kid"},                          // another parent may repeat a name
        {"ghost", "dangling", FolderNameIssue::DuplicateSibling, "dangling"}, // a parent that is no folder counts as the root
        {"0", "Walls", FolderNameIssue::None, "Walls"},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(std::string("parent '") + row.parent + "' typed " + ost_test::detail::quote(row.typed));
        FolderNameCheck got = ost_domain::check_condition_folder_name(folders, row.parent, "", row.typed);
        CHECK_STR_EQ(issue_name(got.issue), issue_name(row.issue));
        CHECK_STR_EQ(got.name, row.name);
        if (row.issue == FolderNameIssue::DuplicateSibling)
        {
            CHECK_STR_EQ(got.dialog.title, "Duplicate Folder");
            CHECK_STR_EQ(got.dialog.message, std::string("Folder ") + row.name + " already exists.");
        }
    }
}

// conditions_sidebar.py:1493 `if sibling is item: continue`: a folder is not its own sibling, so a case-only change is allowed.
TEST(ConditionFolderRules, AFolderIsNotItsOwnSiblingSoACaseOnlyChangeIsAllowed)
{
    const FolderMap folders = {{"f1", {"f1", "walls", ""}}, {"f2", {"f2", "Floors", ""}}};
    const Rename got = rename_of(folders, "f1", "Walls");
    CHECK_STR_EQ(decision_name(got.decision), "Write");
    CHECK_STR_EQ(got.new_name, "Walls");

    FolderNameCheck as_new = ost_domain::check_condition_folder_name(folders, "", "", "Walls");
    CHECK_STR_EQ(issue_name(as_new.issue), "DuplicateSibling"); // the same text for a NEW folder collides with 'walls'
    FolderNameCheck excluded = ost_domain::check_condition_folder_name(folders, "", "f1", "Walls");
    CHECK_STR_EQ(issue_name(excluded.issue), "None");
}

// Python's strip() removes more than NSCharacterSet whitespace (U+001C..U+001F, U+0085 and every str.isspace() character): all of them make a
// name blank; the look-alikes that Python keeps (zero-width space, BOM, soft hyphen ...) are a name.
TEST(ConditionFolderRules, BlankMeansWhatPythonStripRemoves)
{
    const FolderMap folders = {{"f1", {"f1", "Original", ""}}};
    for (const std::string &text : blank_texts())
    {
        SCOPED_CONTEXT("blank " + ost_test::detail::quote(text));
        const FolderNameCheck created = ost_domain::check_condition_folder_name(folders, "", "", text);
        CHECK_STR_EQ(issue_name(created.issue), "Blank");
        CHECK_STR_EQ(created.name, "");
        const Rename renamed = rename_of(folders, "f1", text);
        CHECK_STR_EQ(decision_name(renamed.decision), "Refused");
        CHECK_STR_EQ(issue_name(renamed.check.issue), "Blank");
    }
    for (const std::string &text : not_blank_texts())
    {
        SCOPED_CONTEXT("not blank " + ost_test::detail::quote(text));
        const FolderNameCheck created = ost_domain::check_condition_folder_name(folders, "", "", text);
        CHECK_STR_EQ(issue_name(created.issue), "None");
        CHECK_STR_EQ(created.name, text);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the length rule (CF-6, N-M1-13: 50 UTF-16 units; Windows checks nothing, the column is VARCHAR(50))
// ---------------------------------------------------------------------------------------------------------------------------------

// 50 units are fine, 51 are refused with the iOS text; the units are UTF-16 code units (an emoji takes two, an accented letter and a combining
// sequence are counted by their code points) and the strip comes first (the spaces around do not count).
TEST(ConditionFolderRules, TheLimitIsFiftyUtf16UnitsOfTheStrippedName)
{
    const FolderMap folders;
    struct Row
    {
        std::string typed;
        bool ok;
    };
    const std::vector<Row> rows = {
        {repeat("A", 50), true},
        {repeat("A", 51), false},
        {" " + repeat("A", 50) + "  ", true},
        {"  " + repeat("A", 51) + " ", false},
        {repeat(kEmoji, 25), true},                                     // 50 units in 100 bytes
        {repeat(kEmoji, 26), false},                                    // 52 units
        {repeat(kEmoji, 24) + "aa", true},                              // 48 + 2
        {repeat(kEmoji, 24) + "aaa", false},                            // 48 + 3 = 51
        {repeat(kEmoji, 25) + "a", false},                              // 50 + 1
        {repeat("\xC3\xA9", 50), true},                                 // 50 x U+00E9: 100 bytes, 50 units
        {repeat("\xC3\xA9", 51), false},
        {repeat("e\xCC\x81", 25), true},                                // e + U+0301 counts 2 units
        {repeat("e\xCC\x81", 26), false},
        {repeat("\xE4\xB8\xAD", 50), true},                             // 50 CJK characters (3 bytes, 1 unit each)
        {repeat("\xE4\xB8\xAD", 51), false},
    };
    int index = 0;
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("row " + std::to_string(index++) + " (" + std::to_string(row.typed.size()) + " bytes)");
        const FolderNameCheck got = ost_domain::check_condition_folder_name(folders, "", "", row.typed);
        CHECK_STR_EQ(issue_name(got.issue), row.ok ? "None" : "TooLong");
        CHECK_STR_EQ(got.name, strip(row.typed));
        if (!row.ok)
        {
            CHECK_STR_EQ(got.dialog.title, kTooLongTitle);
            CHECK_STR_EQ(got.dialog.message, kTooLongMessage);
        }
    }
}

// The limit also holds for a rename; a stored name that is already longer than the limit (an imported file) is not flagged when the typed
// text is that name again (Unchanged: no write, no dialog), and any other long text is TooLong.
TEST(ConditionFolderRules, ARenameIsLimitedToFiftyUnitsButAnUnchangedLongNameIsSilent)
{
    const std::string longer = repeat("L", 60);
    const FolderMap folders = {{"f1", {"f1", longer, ""}}, {"f2", {"f2", "Short", ""}}};

    const Rename same = rename_of(folders, "f1", longer);
    CHECK_STR_EQ(decision_name(same.decision), "Unchanged");
    CHECK_STR_EQ(issue_name(same.check.issue), "None");

    const Rename other_long = rename_of(folders, "f2", repeat("M", 51));
    CHECK_STR_EQ(decision_name(other_long.decision), "Refused");
    CHECK_STR_EQ(issue_name(other_long.check.issue), "TooLong");
    CHECK_STR_EQ(other_long.check.dialog.title, kTooLongTitle);
    CHECK_STR_EQ(other_long.check.dialog.message, kTooLongMessage);

    const Rename exactly_fifty = rename_of(folders, "f2", repeat("M", 50));
    CHECK_STR_EQ(decision_name(exactly_fifty.decision), "Write");
}

// The order of the checks: blank, invalid text, too long, duplicate. A legacy sibling whose name is longer than the limit, typed again for a new
// folder, is TooLong (not a duplicate).
TEST(ConditionFolderRules, TheChecksRunInTheOrderBlankInvalidTooLongDuplicate)
{
    const std::string longer = repeat("L", 60);
    const FolderMap folders = {{"f1", {"f1", longer, ""}}};
    CHECK_STR_EQ(issue_name(ost_domain::check_condition_folder_name(folders, "", "", longer).issue), "TooLong");
    CHECK_STR_EQ(issue_name(ost_domain::check_condition_folder_name(folders, "", "", "  \xFF  ").issue), "InvalidText");
    CHECK_STR_EQ(issue_name(ost_domain::check_condition_folder_name(folders, "", "", "   ").issue), "Blank");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// text that cannot be a name (TextLimits.hpp: invalid UTF-8 is refused, never repaired; a NUL cuts a C string)
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(ConditionFolderRules, InvalidUtf8AndNulAreRefusedNotRepaired)
{
    const FolderMap folders;
    const std::vector<std::string> bad = {
        std::string("\xC0\x80"),            // overlong NUL
        std::string("\xED\xA0\x80"),        // a UTF-16 surrogate encoded in UTF-8
        std::string("\xF4\x90\x80\x80"),    // above U+10FFFF
        std::string("A\xFF"),               // a byte that is never UTF-8
        std::string("\xE2\x82"),            // truncated
        std::string("\x80"),                // a stray continuation byte
        std::string("A\0B", 3),             // an embedded NUL
        std::string("\0", 1),               // NUL alone is not blank (strip keeps it)
        std::string(" \0 ", 3),
    };
    int index = 0;
    for (const std::string &text : bad)
    {
        SCOPED_CONTEXT("bad text " + std::to_string(index++) + " " + ost_test::detail::quote(text));
        const FolderNameCheck got = ost_domain::check_condition_folder_name(folders, "", "", text);
        CHECK_STR_EQ(issue_name(got.issue), "InvalidText");
        const FolderMap one = {{"f1", {"f1", "Original", ""}}};
        const Rename renamed = rename_of(one, "f1", text);
        CHECK_STR_EQ(decision_name(renamed.decision), "Refused");
        CHECK_STR_EQ(issue_name(renamed.check.issue), "InvalidText");
    }
    // valid text that looks unusual is a name: tab and line break inside a name are kept (Windows checks nothing but the strip)
    const FolderNameCheck inner = ost_domain::check_condition_folder_name(folders, "", "", " a\tb\nc ");
    CHECK_STR_EQ(issue_name(inner.issue), "None");
    CHECK_STR_EQ(inner.name, "a\tb\nc");
}

// A folder that does not exist cannot be renamed: refused silently, the outputs are written (never left as they were).
TEST(ConditionFolderRules, RenamingAFolderThatIsNotThereIsRefusedSilently)
{
    const FolderMap folders = {{"f1", {"f1", "Original", ""}}};
    const Rename got = rename_of(folders, "ghost", "Whatever");
    CHECK_STR_EQ(decision_name(got.decision), "Refused");
    CHECK_STR_EQ(got.new_name, "");
    CHECK_STR_EQ(issue_name(got.check.issue), "None");
    CHECK_STR_EQ(got.check.name, "");
    CHECK_STR_EQ(got.check.dialog.title, "");
    CHECK_STR_EQ(got.check.dialog.message, "");
    // without a `check` pointer the function still decides
    std::string new_name = "<stale>";
    CHECK_STR_EQ(decision_name(ost_domain::decide_condition_folder_rename(folders, "f1", " X ", new_name)), "Write");
    CHECK_STR_EQ(new_name, "X");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the parent of a new folder or condition (conditions_sidebar.py:989-1001, :1499-1513)
// ---------------------------------------------------------------------------------------------------------------------------------

// Port of tests/presentation/components/test_conditions_sidebar.py:1006-1068 (test_condition_context_new_folder_uses_clicked_target_and_root_is_top_level
// and test_condition_context_new_keeps_right_clicked_folder_target): the CLICKED target decides, not the current item at trigger time; the root item
// means the top level ("" ); the tests expect ["f1", ""] for the folder and the condition creation.
// ONE rule, ost_domain::folder_context_uid(nodes, index) of ConditionCreate.hpp (the Conditions list calls it for every section of the tree it
// shows; the map-and-uid overload that used to live in ConditionFolderRules.hpp was the same rule for a caller that had already found the folder
// and was removed, S1 UI). Every expectation of the old test is kept, asked of the tree the screen really has: a folder row is its own context, a
// type group or a condition is the folder it sits in, a root node and a condition whose folder uid is not a folder of the bid ("ghost", "0") sit at
// the top level (conditions_sidebar.py:462-467).
TEST(ConditionFolderRules, TheClickedTargetDecidesTheParentFolder)
{
    const FolderMap folders = {{"f1", {"f1", "First", ""}}, {"f2", {"f2", "Second", ""}}};
    ConditionMap conditions = conditions_of({{"in1", "f1"}, {"in2", "f2"}, {"root", ""}, {"ghost", "ghost"}, {"zero", "0"}});
    const std::vector<ost_domain::ConditionTreeNode> nodes = ost_domain::build_condition_tree(conditions, folders, {});

    auto context_of = [&nodes](ost_domain::ConditionTreeNode::Kind kind, const std::string &uid)
    {
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].kind == kind && nodes[i].uid == uid)
                return ost_domain::folder_context_uid(nodes, i);
        return std::string("<not in the tree>");
    };
    using Kind = ost_domain::ConditionTreeNode::Kind;
    CHECK_STR_EQ(context_of(Kind::Folder, "f1"), "f1"); // a click on the folder f1 (menu built while f2 was current)
    CHECK_STR_EQ(context_of(Kind::Folder, "f2"), "f2");
    // a condition or a type group inside f1 passes the folder it sits in
    CHECK_STR_EQ(context_of(Kind::Condition, "in1"), "f1");
    CHECK_STR_EQ(context_of(Kind::Condition, "in2"), "f2");
    // the root item: the top level; a condition without a folder, and one whose folder uid is not a folder of the bid, sit at the root
    CHECK_STR_EQ(context_of(Kind::Condition, "root"), "");
    CHECK_STR_EQ(context_of(Kind::Condition, "ghost"), "");
    CHECK_STR_EQ(context_of(Kind::Condition, "zero"), "");
    // a type group is the folder it sits in: the group of the conditions of f1 says f1, the group at the top level (ghost, zero, root) says ""
    std::size_t groups = 0;
    for (std::size_t i = 0; i + 1 < nodes.size(); ++i)
        if (nodes[i].kind == Kind::TypeGroup)
        {
            ++groups;
            CHECK_STR_EQ(ost_domain::folder_context_uid(nodes, i), ost_domain::folder_context_uid(nodes, i + 1)); // its first condition
            CHECK_STR_EQ(ost_domain::folder_context_uid(nodes, i), nodes[i].depth == 0 ? "" : nodes[i - 1].uid);
        }
    CHECK_EQ(groups, static_cast<std::size_t>(3));
    // no folders at all: the top level everywhere
    const std::vector<ost_domain::ConditionTreeNode> flat = ost_domain::build_condition_tree(conditions, FolderMap{}, {});
    for (std::size_t i = 0; i < flat.size(); ++i)
        CHECK_STR_EQ(ost_domain::folder_context_uid(flat, i), "");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// THE in-use rule of the delete (project_write_service.py:936-953; register CF-1)
// ---------------------------------------------------------------------------------------------------------------------------------

// Port of tests/application/services/test_project_write_service.py:16520-16552 test_folder_usage_walks_every_ancestor_and_ignores_unknown_folders
// (the expected lists are the results of the real Windows function, see the oracle header): folders r > c > g, one condition.
TEST(ConditionFolderRules, FolderUsageWalksEveryAncestorAndIgnoresUnknownFolders)
{
    const FolderMap folders = {{"r", {"r", "Root", ""}}, {"c", {"c", "Child", "r"}}, {"g", {"g", "Grandchild", "c"}}};
    auto blocked = [&folders](const char *condition_folder, std::vector<std::string> requested)
    {
        ConditionMap conditions;
        Condition condition;
        condition.uid = "10";
        condition.condition_folder_uid = condition_folder;
        conditions["10"] = condition;
        return ost_domain::plan_condition_folder_delete(folders, conditions, requested).blocked;
    };
    using V = std::vector<std::string>;
    // a Condition in the grandchild blocks it and every ancestor up to the root
    CHECK(blocked("g", {"r"}) == V({"r"}));
    CHECK(blocked("g", {"c"}) == V({"c"}));
    CHECK(blocked("g", {"g"}) == V({"g"}));
    CHECK(blocked("g", {"g", "r", "c"}) == V({"g", "r", "c"}));
    // descendants of the Condition's own folder are not affected
    CHECK(blocked("c", {"g"}) == V());
    CHECK(blocked("c", {"r", "g"}) == V({"r"}));
    CHECK(blocked("r", {"c", "g"}) == V());
    // a Condition without a folder blocks nothing
    CHECK(blocked("", {"r", "c", "g"}) == V());
    // a Condition whose folder is not in the folder map blocks nothing, even when that same uid is requested
    CHECK(blocked("zz", {"zz", "r"}) == V());
}

// Port of tests/application/services/test_project_write_service.py:19220-19236 test_a_blocked_condition_folder_deletion_names_the_folder_in_use:
// F1 'Walls', F2 'Floors' (child of F1), condition 10 in the unknown folder 'missing', condition 11 in F2, request [F1, F2, F3] -> blocked [F1, F2]
// (the first condition names a folder that is not in the Bid and must not stop the scan).
TEST(ConditionFolderRules, ABlockedDeletionNamesTheFoldersInUse)
{
    const FolderMap folders = {{"F1", {"F1", "Walls", ""}}, {"F2", {"F2", "Floors", "F1"}}};
    ConditionMap conditions;
    Condition c10;
    c10.uid = "10";
    c10.condition_folder_uid = "missing";
    Condition c11;
    c11.uid = "11";
    c11.condition_folder_uid = "F2";
    conditions["10"] = c10;
    conditions["11"] = c11;

    const ost_domain::ConditionFolderDeletePlan plan = ost_domain::plan_condition_folder_delete(folders, conditions, {"F1", "F2", "F3"});
    CHECK(plan.blocked == std::vector<std::string>({"F1", "F2"}));
    CHECK(plan.requested == std::vector<std::string>({"F1", "F2", "F3"}));
    CHECK(plan.deletable == std::vector<std::string>({"F3"}));
    CHECK(plan.unknown_folder); // F3 is not a folder of the bid: the handler does nothing at all (condition_action_handler.py:1526-1530)
}

// Port of tests/application/services/test_project_write_service.py:16494-16518 test_blocked_folder_delete_reports_the_whole_failure_result:
// f1 'Walls' holds condition 10 -> blocked ["f1"], nothing deletable (the service result has no deletable folder and the blocked uids).
TEST(ConditionFolderRules, AllBlockedLeavesNothingToDelete)
{
    const FolderMap folders = {{"f1", {"f1", "Walls", ""}}};
    ConditionMap conditions;
    Condition condition;
    condition.uid = "10";
    condition.condition_folder_uid = "f1";
    conditions["10"] = condition;

    const ost_domain::ConditionFolderDeletePlan plan = ost_domain::plan_condition_folder_delete(folders, conditions, {"f1"});
    CHECK(plan.requested == std::vector<std::string>({"f1"}));
    CHECK(plan.blocked == std::vector<std::string>({"f1"}));
    CHECK(plan.deletable.empty());
    CHECK_FALSE(plan.unknown_folder);
}

// Port of tests/application/services/test_project_write_service.py:1541 test_condition_folder_delete_result_blocks_in_use_folder: folder-1 holds
// cond-1 -> the failure reason is "in use", blocked_uids ["folder-1"] and the delete use case is never called (nothing deletable).
TEST(ConditionFolderRules, AFolderThatHoldsAConditionIsBlockedAndNothingIsDeleted)
{
    const FolderMap folders = {{"folder-1", {"folder-1", "Folder", ""}}};
    ConditionMap conditions;
    Condition condition;
    condition.uid = "cond-1";
    condition.condition_folder_uid = "folder-1";
    conditions["cond-1"] = condition;

    const ost_domain::ConditionFolderDeletePlan plan = ost_domain::plan_condition_folder_delete(folders, conditions, {"folder-1"});
    CHECK(plan.blocked == std::vector<std::string>({"folder-1"}));
    CHECK(plan.deletable.empty());
    CHECK_FALSE(plan.unknown_folder);
}

// Port of tests/application/services/test_project_write_service.py:1565 test_condition_folder_delete_result_allows_unused_folder: no condition at all ->
// folder-1 is deletable (the use case is called with ["folder-1"]) and nothing is blocked.
TEST(ConditionFolderRules, AnUnusedFolderIsDeletable)
{
    const FolderMap folders = {{"folder-1", {"folder-1", "Folder", ""}}};

    const ost_domain::ConditionFolderDeletePlan plan = ost_domain::plan_condition_folder_delete(folders, {}, {"folder-1"});
    CHECK(plan.requested == std::vector<std::string>({"folder-1"}));
    CHECK(plan.blocked.empty());
    CHECK(plan.deletable == std::vector<std::string>({"folder-1"}));
    CHECK_FALSE(plan.unknown_folder);
    CHECK_FALSE(plan.subfolders_move);
}

// Port of tests/application/services/test_project_write_service.py:16465 test_none_inputs_mean_no_uids_or_no_changes (the folder line): an empty
// request validates to empty lists.
TEST(ConditionFolderRules, AnEmptyRequestHasNothingBlockedOrDeletable)
{
    const FolderMap folders = {{"f1", {"f1", "Walls", ""}}};
    const ost_domain::ConditionFolderDeletePlan plan = ost_domain::plan_condition_folder_delete(folders, {}, {});
    CHECK(plan.requested.empty());
    CHECK(plan.blocked.empty());
    CHECK(plan.deletable.empty());
    CHECK_FALSE(plan.unknown_folder);
    CHECK_FALSE(plan.subfolders_move);
}

// Every oracle row of the real Windows function (_condition_folder_uids_in_use) incl. the rows of the two tests above, two conditions in two
// branches, a dangling parent and a parent "0": `blocked` is the Windows list, `deletable` is the request without the blocked ones in request order,
// `requested` is the request as given. (A duplicated request uid appears as often in `blocked` as Windows lists it: ['g','g','r'].)
TEST(ConditionFolderRules, TheInUseRuleMatchesTheRealWindowsFunctionOnEveryOracleRow)
{
    int index = 0;
    for (const ost_ftest::OracleInUseCase &row : ost_ftest::in_use_cases())
    {
        SCOPED_CONTEXT("oracle row " + std::to_string(index++) + " " + row.label + " request [" + ost_ftest::join(strings_of(row.requested)) + "]");
        const FolderMap folders = folders_of(row.folders);
        const ost_domain::ConditionFolderDeletePlan plan =
            ost_domain::plan_condition_folder_delete(folders, conditions_of(row.conditions), strings_of(row.requested));
        CHECK_STR_EQ(ost_ftest::join(plan.requested), ost_ftest::join(strings_of(row.requested)));
        CHECK_STR_EQ(ost_ftest::join(plan.blocked), ost_ftest::join(strings_of(row.blocked)));

        std::vector<std::string> deletable;
        const std::set<std::string> blocked(row.blocked.begin(), row.blocked.end());
        bool unknown = false;
        for (const char *uid : row.requested)
        {
            if (!blocked.count(uid))
                deletable.push_back(uid);
            unknown = unknown || folders.count(uid) == 0;
        }
        CHECK_STR_EQ(ost_ftest::join(plan.deletable), ost_ftest::join(deletable));
        CHECK_EQ(plan.unknown_folder, unknown);
    }
}

// Nothing may loop on a ParentUID cycle (Windows' `while current:` would never end; the load refuses such a bid, but the rule is a pure function):
// a<->b with a condition in a blocks both, a folder that is its own parent blocks itself, and a cycle that does not hold the condition is no problem.
TEST(ConditionFolderRules, ACycleOfFoldersCannotLoopTheWalk)
{
    const FolderMap folders = {{"a", {"a", "A", "b"}}, {"b", {"b", "B", "a"}}, {"s", {"s", "Self", "s"}}, {"x", {"x", "X", ""}}};
    auto plan_for = [&folders](const char *condition_folder, std::vector<std::string> requested)
    {
        ConditionMap conditions;
        Condition condition;
        condition.uid = "10";
        condition.condition_folder_uid = condition_folder;
        conditions["10"] = condition;
        return ost_domain::plan_condition_folder_delete(folders, conditions, requested);
    };
    CHECK(plan_for("a", {"a", "b", "x"}).blocked == std::vector<std::string>({"a", "b"}));
    CHECK(plan_for("b", {"b", "a"}).blocked == std::vector<std::string>({"b", "a"}));
    CHECK(plan_for("s", {"s", "x"}).blocked == std::vector<std::string>({"s"}));
    CHECK(plan_for("x", {"a", "b", "s", "x"}).blocked == std::vector<std::string>({"x"}));
}

// The iOS line of the confirmation: a surviving sub-folder of a folder that is deleted moves to the top level. `subfolders_move` is true only then:
// not when the sub-folder is deleted too, not when it is the folder that is deleted, not when the parent is blocked (nothing is deleted), and
// a folder shown at the root because its parent is dangling is nobody's sub-folder.
TEST(ConditionFolderRules, TheSubfoldersMoveFlagIsTrueOnlyWhenASubfolderSurvivesItsDeletedParent)
{
    const FolderMap folders = {{"p", {"p", "Parent", ""}},   {"k", {"k", "Kid", "p"}},       {"g", {"g", "Grandkid", "k"}},
                               {"e", {"e", "Empty", ""}},    {"d", {"d", "Dangling", "p9"}}, {"q", {"q", "Other", ""}}};
    auto flag = [&folders](std::vector<std::string> requested, const ConditionMap &conditions = {})
    { return ost_domain::plan_condition_folder_delete(folders, conditions, requested).subfolders_move; };

    CHECK(flag({"p"}));                // k survives and moves to the top level
    CHECK(flag({"k"}));                // g survives
    CHECK_FALSE(flag({"p", "k", "g"})); // everything below goes too
    CHECK(flag({"p", "k"}));           // g survives under the deleted k
    CHECK_FALSE(flag({"g"}));          // no sub-folder
    CHECK_FALSE(flag({"e"}));
    CHECK_FALSE(flag({"d"}));
    CHECK_FALSE(flag({"q", "e"}));
    CHECK_FALSE(flag({}));
    // a condition in g blocks g, k and p: nothing is deletable, so nothing moves
    ConditionMap conditions;
    Condition condition;
    condition.uid = "10";
    condition.condition_folder_uid = "g";
    conditions["10"] = condition;
    CHECK_FALSE(flag({"p"}, conditions));
    CHECK(flag({"p", "e"}, ConditionMap{}));
    // a condition in g blocks p, k and g, but the deletable 'e' has no sub-folder
    CHECK_FALSE(flag({"p", "e"}, conditions));
}

// A chain of 8000 folders (the Windows reader test tests/infrastructure/database/test_bid_owned_identity.py:493-499 reads such a chain): the walk is
// iterative (a recursion would overflow the stack) and linear: one condition in the deepest folder blocks all 8000 folders, in request order.
TEST(ConditionFolderRules, TheInUseWalkHandlesAChainOfEightThousandFolders)
{
    FolderMap folders;
    std::vector<std::string> requested;
    for (int uid = 1; uid <= 8000; ++uid)
    {
        folders[std::to_string(uid)] = ConditionFolder{std::to_string(uid), "F" + std::to_string(uid), uid == 1 ? "" : std::to_string(uid - 1)};
        requested.push_back(std::to_string(8001 - uid)); // request order 8000 .. 1
    }
    ConditionMap conditions;
    Condition condition;
    condition.uid = "10";
    condition.condition_folder_uid = "8000";
    conditions["10"] = condition;

    const auto started = std::chrono::steady_clock::now();
    const ost_domain::ConditionFolderDeletePlan plan = ost_domain::plan_condition_folder_delete(folders, conditions, requested);
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    CHECK(plan.blocked == requested);
    CHECK(plan.deletable.empty());
    CHECK_FALSE(plan.unknown_folder);
    CHECK(elapsed_ms < 1500);

    // and a condition in the middle blocks the folders from 1 to 4000 only
    conditions["10"].condition_folder_uid = "4000";
    const ost_domain::ConditionFolderDeletePlan half = ost_domain::plan_condition_folder_delete(folders, conditions, requested);
    REQUIRE_EQ(half.blocked.size(), size_t(4000));
    CHECK_STR_EQ(half.blocked.front(), "4000");
    CHECK_STR_EQ(half.blocked.back(), "1");
    CHECK_EQ(half.deletable.size(), size_t(4000));
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the texts of the delete confirmation (messagebox.py:78-119; register CF-3, CF-11)
// ---------------------------------------------------------------------------------------------------------------------------------

// Every oracle row of the real Windows confirm_multi_delete (QMessageBox recorded): title "Delete Folder"; "Delete {n} item(s)?"; the
// `Cannot delete "{name}" because it is in use.` lines, a blank line and "Delete the other {n} item(s)?"; the warning when everything is blocked;
// the items a Yes deletes.
TEST(ConditionFolderRules, TheConfirmationTextsMatchTheRealWindowsDialogOnEveryOracleRow)
{
    int index = 0;
    for (const ost_ftest::OracleConfirmCase &row : ost_ftest::confirm_cases())
    {
        SCOPED_CONTEXT("oracle row " + std::to_string(index++) + " text " + ost_test::detail::quote(row.text));
        std::vector<ost_domain::FolderDeleteItem> items;
        for (const auto &item : row.items)
            items.push_back(ost_domain::FolderDeleteItem{item.first, item.second});
        const ost_domain::FolderDeleteConfirmation got = ost_domain::condition_folder_delete_confirmation(items, strings_of(row.used), false);
        CHECK_STR_EQ(got.dialog.title, row.title);
        CHECK_STR_EQ(got.dialog.message, row.text);
        CHECK(got.kind == (row.kind == 'Q' ? ost_domain::FolderDeleteDialogKind::Ask : ost_domain::FolderDeleteDialogKind::Warn));
        CHECK_STR_EQ(ost_ftest::join(got.deletable), ost_ftest::join(strings_of(row.returned)));
    }
}

// The iOS line (N-M1-16, CF-3): the confirmation says that the sub-folders move to the top level. It is appended as a new paragraph to a question
// and changes nothing else; a warning (everything blocked, nothing is deleted) does not get it.
TEST(ConditionFolderRules, TheIosLineIsAppendedToAQuestionOnly)
{
    const std::string line = "Folders inside it move to the top level.";
    using Item = ost_domain::FolderDeleteItem;
    {
        const auto got = ost_domain::condition_folder_delete_confirmation({Item{"Walls", "f1"}}, {}, true);
        CHECK_STR_EQ(got.dialog.title, "Delete Folder");
        CHECK_STR_EQ(got.dialog.message, "Delete 1 item(s)?\n\n" + line);
        CHECK(got.kind == ost_domain::FolderDeleteDialogKind::Ask);
        CHECK(got.deletable == std::vector<std::string>({"f1"}));
    }
    {
        const auto got = ost_domain::condition_folder_delete_confirmation({Item{"Walls", "f1"}, Item{"Floors", "f2"}}, {"f1"}, true);
        CHECK_STR_EQ(got.dialog.message, "Cannot delete \"Walls\" because it is in use.\n\nDelete the other 1 item(s)?\n\n" + line);
        CHECK(got.kind == ost_domain::FolderDeleteDialogKind::Ask);
        CHECK(got.deletable == std::vector<std::string>({"f2"}));
    }
    {
        const auto got = ost_domain::condition_folder_delete_confirmation({Item{"Walls", "f1"}}, {"f1"}, true);
        CHECK_STR_EQ(got.dialog.message, "Cannot delete \"Walls\" because it is in use.");
        CHECK(got.kind == ost_domain::FolderDeleteDialogKind::Warn);
        CHECK(got.deletable.empty());
    }
    {
        const auto got = ost_domain::condition_folder_delete_confirmation({Item{"Walls", "f1"}}, {}, false);
        CHECK_STR_EQ(got.dialog.message, "Delete 1 item(s)?"); // without the flag exactly the Windows text
    }
}

// CF-11 (messagebox.py:84-105, project_write_service.py:946-953): the in-use line names the SELECTED folder even when only a descendant holds the
// condition. Plan -> confirmation: Root is blocked because its grandchild holds the condition, the text names "Root".
TEST(ConditionFolderRules, TheInUseLineNamesTheSelectedFolderEvenWhenADescendantHoldsTheCondition)
{
    const FolderMap folders = {{"r", {"r", "Root", ""}}, {"c", {"c", "Child", "r"}}, {"g", {"g", "Grandchild", "c"}}, {"o", {"o", "Other", ""}}};
    ConditionMap conditions;
    Condition condition;
    condition.uid = "10";
    condition.condition_folder_uid = "g";
    conditions["10"] = condition;
    const ost_domain::ConditionFolderDeletePlan plan = ost_domain::plan_condition_folder_delete(folders, conditions, {"r", "o"});
    CHECK(plan.blocked == std::vector<std::string>({"r"}));
    CHECK(plan.deletable == std::vector<std::string>({"o"}));
    const auto got = ost_domain::condition_folder_delete_confirmation({{"Root", "r"}, {"Other", "o"}}, plan.blocked, plan.subfolders_move);
    CHECK_STR_EQ(got.dialog.message, "Cannot delete \"Root\" because it is in use.\n\nDelete the other 1 item(s)?");
    CHECK(got.deletable == std::vector<std::string>({"o"}));
}

// Mutation check of the fix run (a requested uid that is no folder). A folder listed at the root because its parent uid is dangling ("p9") or
// the text "0" is nobody's sub-folder: a request for that unknown uid (unknown_folder is true, the screens do nothing with it) must not raise the
// iOS "Folders inside it move to the top level." flag, while a request for a real parent still does.
TEST(ConditionFolderRules, ARequestedUidThatIsNoFolderIsNobodysParentForTheSubfoldersFlag)
{
    const FolderMap folders = {{"p", {"p", "Parent", ""}}, {"k", {"k", "Kid", "p"}}, {"d", {"d", "Dangling", "p9"}}, {"z", {"z", "Zero", "0"}}};
    auto plan_for = [&folders](std::vector<std::string> requested) { return ost_domain::plan_condition_folder_delete(folders, {}, requested); };

    CHECK(plan_for({"p"}).subfolders_move); // k survives under the deleted p
    for (const std::vector<std::string> &requested : {std::vector<std::string>{"p9"}, std::vector<std::string>{"0"}, std::vector<std::string>{"p9", "0"}})
    {
        SCOPED_CONTEXT("request " + ost_ftest::join(requested));
        const ost_domain::ConditionFolderDeletePlan plan = plan_for(requested);
        CHECK_FALSE(plan.subfolders_move);
        CHECK(plan.unknown_folder);
        CHECK(plan.blocked.empty());
        CHECK(plan.deletable == requested);
    }
}

// Mutation check of the fix run (blank before unchanged). conditions_sidebar.py:1466-1471: a blank text is checked BEFORE "the name the editor
// started with", so a folder whose stored name is empty (a foreign file) renamed to blank spaces is a silent refusal (Blank), never "Unchanged".
TEST(ConditionFolderRules, ABlankTextOverAnEmptyStoredNameIsStillBlankNotUnchanged)
{
    const FolderMap folders = {{"e", {"e", "", ""}}, {"f", {"f", "Walls", ""}}};
    for (const std::string &typed : std::vector<std::string>{"", " ", "   ", "\t\n", "\xC2\xA0"})
    {
        SCOPED_CONTEXT("typed " + ost_test::detail::quote(typed));
        const Rename got = rename_of(folders, "e", typed);
        CHECK_STR_EQ(decision_name(got.decision), "Refused");
        CHECK_STR_EQ(issue_name(got.check.issue), "Blank");
        CHECK_STR_EQ(got.new_name, "");
        CHECK_STR_EQ(got.check.name, "");
    }
    // the same text over a stored name that has letters is refused as Blank too, and a real name is written
    CHECK_STR_EQ(decision_name(rename_of(folders, "f", "   ").decision), "Refused");
    CHECK_STR_EQ(decision_name(rename_of(folders, "e", "Floors").decision), "Write");
}

// Mutation check of the fix run (parent "0"). The Windows reader reads a ParentUID of NULL, 0, "0" or "" as no parent (test_bid_data_reader.py:358),
// so "0" means the root even when some folder has the uid "0" (a damaged file: the loader refuses non-positive uids, the store reads raw rows).
// The folder below is listed at the root, not under that folder, and it is a sibling of the root folders.
TEST(ConditionFolderRules, AParentUidZeroMeansTheRootEvenWhenAFolderHasTheUidZero)
{
    const FolderMap folders = {{"0", {"0", "Zero", ""}}, {"a", {"a", "Kid of zero", "0"}}, {"b", {"b", "Under a", "a"}}};
    CHECK_STR_EQ(ost_domain::effective_condition_folder_parent(folders, folders.at("a")), "");
    CHECK_STR_EQ(ost_domain::effective_condition_folder_parent(folders, folders.at("b")), "a");

    // "Zero" and "Kid of zero" are both root folders: the name rule sees them as siblings of a new root folder ...
    CHECK_STR_EQ(issue_name(ost_domain::check_condition_folder_name(folders, "", "", "kid of ZERO").issue), "DuplicateSibling");
    CHECK_STR_EQ(issue_name(ost_domain::check_condition_folder_name(folders, "0", "", "zero").issue), "DuplicateSibling");
    // ... and a folder under "a" is not.
    CHECK_STR_EQ(issue_name(ost_domain::check_condition_folder_name(folders, "a", "", "zero").issue), "None");
}
