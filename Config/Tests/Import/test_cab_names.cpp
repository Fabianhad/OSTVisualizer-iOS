// IMP-15: the member-name safety rules of the OSP importer.
//
// Windows: ost_visualizer/infrastructure/mdb/importers/osp_importer.py _is_safe_cab_member_name (with _is_windows_device_name,
// _ends_in_dot_or_space and _windows_path_parts): a package holding any name that is absolute, rooted, UNC, an extended
// "\\?\" path, contains '..', has ':' in a component, a component ending in '.' or ' ', or a reserved device name (CON, PRN,
// AUX, NUL, CONIN$, CONOUT$, COM1-9, LPT1-9, also with the superscript digits) is rejected ("unsafe CAB member path")
// before anything is extracted. The decision of the user (IMP-15): iOS rejects the same archives, rules ported in full.
//
// Oracle: Fixtures/import_cab_name_oracle.txt holds names and what the Windows functions make of them (computed from the
// source text of osp_importer.py with Python 3.11.4; see the fixture header).
#include "TestHarness.hpp"

#include "Import/CabMemberName.hpp"

#include <sstream>
#include <string>
#include <vector>

using ost_import::is_safe_cab_member_name;
using ost_import::is_windows_device_name;
using ost_import::windows_path_parts;

namespace
{
    // Port of tests/infrastructure/mdb/importers/test_osp_importer.py::_RESERVED_DEVICE_MEMBER_NAMES
    const std::vector<std::string> RESERVED_DEVICE_MEMBER_NAMES = {
        "nul",
        "NUL.ost",
        "con.pdf",
        "COM1",
        "aux.ost",
        "Lpt9.txt",
        "prn",
        "PRN.OST",
        "com9.pdf",
        "lpt1",
        "nul.",
        "nul..",
        "nul .txt",
        "nul . .txt",
        "NUL.tar.gz",
        "con.d.pdf",
        "CONIN$",
        "conout$.txt",
        "COM\xc2\xb9"".pdf",
        "lpt\xc2\xb3""",
        "docs\\aux",
        "docs\\nul\\readme.pdf",
        "docs/con/readme.pdf",
        "nul\\readme.pdf",
        "TempImages!.tmp\\COM3.pdf",
        "TempImages!.tmp\\sub\\aux.pdf",
        "TempImages!.tmp\\nul\\plan.pdf",
        "TempImages!.tmp/Lpt2.tif",
    };

    // Port of tests/infrastructure/mdb/importers/test_osp_importer.py::_DRIVE_WITHOUT_ROOT_MEMBER_NAMES
    const std::vector<std::string> DRIVE_WITHOUT_ROOT_MEMBER_NAMES = {
        "//?/",
        "\\\\?\\",
        "/\\?/",
        "\\/?\\",
        "//?\\",
        "//./",
        "\\\\.\\",
        "//?/C:/x",
        "\\\\?\\C:\\x.pdf",
        "\\\\?\\UNC\\server\\share",
        "//server/share/x",
        "//server/share",
    };

    // Port of tests/infrastructure/mdb/importers/test_osp_importer.py::_TRAILING_DOT_OR_SPACE_MEMBER_NAMES
    const std::vector<std::string> TRAILING_DOT_OR_SPACE_MEMBER_NAMES = {
        "dir.\\plan.pdf",
        "dir \\plan.pdf",
        "file. ",
        "plan.pdf ",
        "plan.pdf.",
        "a..",
        "...",
        " ",
        "a\\ ",
        "a\\...\\x.pdf",
        "x\\a..\\y",
        ".. \\x",
        "a\\.. \\x.pdf",
        "Project.ost.",
        "Project.ost ",
        "TempImages!.tmp\\sheets.\\a.png",
        "TempImages!.tmp\\sheets \\a.png",
        "TempImages!.tmp\\a.png ",
        "TempImages!.tmp\\a.png.",
        "TempImages!.tmp/a. ",
        "dir./plan.pdf",
    };

    // Port of tests/infrastructure/mdb/importers/test_osp_importer.py::_PLAIN_DOT_AND_SPACE_MEMBER_NAMES
    const std::vector<std::string> PLAIN_DOT_AND_SPACE_MEMBER_NAMES = {
        "level 1\\plan v2.pdf",
        "TempImages!.tmp\\sheets\\a.b.png",
        "TempImages!.tmp\\sheets\\a.b.pdf",
        "TempImages!.tmp\\a b.pdf",
        "TempImages!.tmp/sub folder/a.b.c.tif",
        "a.b",
        " lead.pdf",
        ".hidden\\a.pdf",
        "x\\.y\\z.pdf",
        "Bid Trans v2.xml",
        "a b\\c d\\e f.pdf",
        "..X/a.pdf",
        "ends-with-x.pdf",
    };

    std::string from_hex(const std::string &hex)
    {
        std::string out;
        for (size_t i = 0; i + 1 < hex.size(); i += 2)
            out += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
        return out;
    }

}

// Port of tests/infrastructure/mdb/importers/test_osp_importer.py::test_only_non_empty_relative_cab_member_names_are_safe
// (the str cases; None, 5 and bytes cannot be passed to the C++ function).
TEST(ImportCabNames, Imp15_OnlyNonEmptyRelativeNamesAreSafe)
{
    for (const char *unsafe : {"", "/abs.png", "C:\\x.png", "..\\x"})
    {
        SCOPED_CONTEXT(std::string() + unsafe);
        CHECK_FALSE(is_safe_cab_member_name(unsafe));
    }
    CHECK(is_safe_cab_member_name("TempImages!.tmp/plan.png"));
}

// Port of ::test_cab_member_name_safety_rules_return_exact_booleans
TEST(ImportCabNames, Imp15_SafetyRulesGiveTheWindowsAnswers)
{
    for (const char *unsafe : {"", "/abs.png", "\\abs.png", "C:\\x.png", "C:x.png", "..\\x", "../x", "ok/../x.pdf", "ok\\..\\x.pdf", "a.ost:stream",
                               ".", "\\\\server\\share\\x.pdf"})
    {
        SCOPED_CONTEXT(std::string() + unsafe);
        CHECK_FALSE(is_safe_cab_member_name(unsafe));
    }
    for (const char *allowed : {"Project.ost", "TempImages!.tmp\\sheet.pdf", "TempImages!.tmp/sheet.pdf", "ok/./file.pdf", "name with spaces.pdf"})
    {
        SCOPED_CONTEXT(std::string() + allowed);
        CHECK(is_safe_cab_member_name(allowed));
    }
    CHECK_EQ(windows_path_parts("a/./b\\c"), (std::vector<std::string>{"a", "b", "c"}));
}

// Port of ::test_path_normalisation_replaces_only_separators
TEST(ImportCabNames, Imp15_NormalisationReplacesOnlySeparators)
{
    CHECK(is_safe_cab_member_name("..X/a.pdf"));
    CHECK(is_safe_cab_member_name("a/..X/b.pdf"));
    CHECK_FALSE(is_safe_cab_member_name("\\\\server\\share"));
    CHECK_EQ(windows_path_parts("a/MAX/b"), (std::vector<std::string>{"a", "MAX", "b"}));
}

// Port of ::test_reserved_device_names_are_rejected_in_every_component
TEST(ImportCabNames, Imp15_ReservedDeviceNamesAreRejectedInEveryComponent)
{
    for (const std::string &name : RESERVED_DEVICE_MEMBER_NAMES)
    {
        SCOPED_CONTEXT(std::string() + name);
        CHECK_FALSE(is_safe_cab_member_name(name));
    }
}

// Port of ::test_ordinary_names_that_only_resemble_device_names_stay_safe
TEST(ImportCabNames, Imp15_NamesThatOnlyResembleDeviceNamesStaySafe)
{
    for (const char *name : {"nulls.ost", "console.pdf", "com10.pdf", "lpt.txt", "comm1", "com0.pdf", "lpt0", "com.pdf", "com1x.pdf", "xnul.pdf", "a.nul",
                             "a.con.pdf", "nu.l", "conin.pdf", "docs\\auxiliary\\readme.pdf", "TempImages!.tmp\\com10.pdf",
                             "TempImages!.tmp\\sub\\console.pdf", "Project.ost"})
    {
        SCOPED_CONTEXT(std::string() + name);
        CHECK(is_safe_cab_member_name(name));
    }
}

// Port of ::test_drive_without_root_and_extended_prefixes_are_never_safe
TEST(ImportCabNames, Imp15_ExtendedPrefixesAndDrivesWithoutRootAreNeverSafe)
{
    for (const std::string &name : DRIVE_WITHOUT_ROOT_MEMBER_NAMES)
    {
        SCOPED_CONTEXT(std::string() + name);
        CHECK_FALSE(is_safe_cab_member_name(name));
    }
}

// Port of ::test_components_ending_in_a_dot_or_a_space_are_never_safe
TEST(ImportCabNames, Imp15_ComponentsEndingInADotOrASpaceAreNeverSafe)
{
    for (const std::string &name : TRAILING_DOT_OR_SPACE_MEMBER_NAMES)
    {
        SCOPED_CONTEXT(std::string() + name);
        CHECK_FALSE(is_safe_cab_member_name(name));
    }
}

// Port of ::test_names_with_inner_dots_and_spaces_stay_safe
TEST(ImportCabNames, Imp15_InnerDotsAndSpacesStaySafe)
{
    for (const std::string &name : PLAIN_DOT_AND_SPACE_MEMBER_NAMES)
    {
        SCOPED_CONTEXT(std::string() + name);
        CHECK(is_safe_cab_member_name(name));
    }
}

// Port of ::test_every_numbered_device_name_and_only_those_are_reserved
TEST(ImportCabNames, Imp15_EveryNumberedDeviceNameAndOnlyThoseAreReserved)
{
    const std::vector<std::string> digits = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "\xC2\xB9", "\xC2\xB2", "\xC2\xB3"};
    for (const char *device : {"com", "lpt", "COM", "LPT", "Com", "Lpt"})
    {
        for (const std::string &digit : digits)
        {
            for (const std::string &name : {std::string(device) + digit, std::string(device) + digit + ".pdf"})
            {
                SCOPED_CONTEXT(std::string() + name);
                CHECK_FALSE(is_safe_cab_member_name(name));
            }
        }
        for (const char *suffix : {"0", "x", "10", "1a", "\xE2\x81\xB4", "A"})
        {
            const std::string name = std::string(device) + suffix + ".pdf";
            SCOPED_CONTEXT(std::string() + name);
            CHECK(is_safe_cab_member_name(name));
        }
    }
    for (const char *name : {"con", "prn", "aux", "nul", "conin$", "conout$"})
    {
        std::string upper = name;
        for (char &c : upper)
            c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        std::string title = name;
        title[0] = static_cast<char>(title[0] - 'a' + 'A');
        for (const std::string &variant : {std::string(name), upper, title, std::string(name) + ".txt"})
        {
            SCOPED_CONTEXT(std::string() + variant);
            CHECK_FALSE(is_safe_cab_member_name(variant));
        }
    }
    for (const char *name : {"conx", "nulX", "nul\t.txt", "nul\t", "aux_", "pr", "xcon"})
    {
        SCOPED_CONTEXT(std::string() + name);
        CHECK(is_safe_cab_member_name(name));
    }
}

// Port of ::test_only_spaces_and_the_extension_are_ignored_for_device_names
TEST(ImportCabNames, Imp15_OnlySpacesAndTheExtensionAreIgnoredForDeviceNames)
{
    for (const char *part : {"nul", "nul ", "nul   ", "nul .txt", "NUL.a.b", "Com1 .x"})
    {
        SCOPED_CONTEXT(std::string() + part);
        CHECK(is_windows_device_name(part));
    }
    for (const char *part : {" nul", "nul\t", "nul\xC2\xA0.txt", "nulX .txt", ".nul", "a.nul", "", "nul1"})
    {
        SCOPED_CONTEXT(std::string() + part);
        CHECK_FALSE(is_windows_device_name(part));
    }
}

// Python's str.upper() maps the dotless i U+0131 to 'I': "CON\u0131N$" is CONIN$ for the Windows rules (found with the
// oracle; every other non-ASCII character whose upper-case form is ASCII, e.g. U+017F, U+00DF, the ligatures U+FB00..FB06,
// cannot form a device name).
TEST(ImportCabNames, Imp15_UpperCasingFollowsPython)
{
    CHECK(is_windows_device_name("CON\xC4\xB1N$"));
    CHECK(is_windows_device_name("con\xC4\xB1n$.txt"));
    CHECK_FALSE(is_windows_device_name("con\xC4\xB0n$")); // U+0130 (dotted capital I) stays as it is
    CHECK_FALSE(is_windows_device_name("\xC4\xB1"));
    CHECK_FALSE(is_windows_device_name("CON\xC5\xBF")); // long s
}

// Port of tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_rejects_unsafe_cab_member_paths_before_extraction
// (the names tests/helpers/import_workflow.py hostile_cab_member_names builds: absolute, rooted, relative, relative with
// forward slashes) and of ::test_package_inspection_rejects_these_names_with_the_exact_message.
TEST(ImportCabNames, Imp15_HostilePathsAreRejected)
{
    for (const char *name : {"C:\\Users\\x\\outside\\payload.ost", "\\Users\\x\\outside\\payload.ost", "..\\..\\outside\\payload.ost",
                             "../../outside/payload.ost", "\\\\server\\share\\payload.ost", "payload.ost:stream", ".", "//server/share/payload.ost",
                             "CON.pdf", "Plan.", "a/..", "../x.pdf"})
    {
        SCOPED_CONTEXT(std::string() + name);
        CHECK_FALSE(is_safe_cab_member_name(name));
    }
}

// The real names of the Windows ANSI-member fixture (tests/integration/native/test_osp_cab.py): safe.
TEST(ImportCabNames, Imp15_TheNamesOfTheLegacyAnsiFixtureAreSafe)
{
    CHECK(is_safe_cab_member_name("26-061 412 \xE2\x80\x93 Corporate Lot K, KS.ost"));
    CHECK(is_safe_cab_member_name("TempImages!.tmp/1 Estimates through 1-12-2015/2026/26-061 412 \xE2\x80\x93 Corporate Lot K, KS/01. Drawings/A0-0.pdf"));
    CHECK(is_safe_cab_member_name("TempImages!.tmp\\1 Estimates through 1-12-2015\\2026\\A0-0.pdf"));
}

// The oracle: every name of the fixture, the Windows answer and its path components.
TEST(ImportCabNames, Imp15_TheRulesMatchTheWindowsFunctionsOnEveryOracleName)
{
    std::string text;
    REQUIRE(ost_test::read_file(ost_test::fixture_path("import_cab_name_oracle.txt"), text));
    std::istringstream stream(text);
    std::string line;
    int number = 0;
    size_t checked = 0, safe_count = 0;
    while (std::getline(stream, line))
    {
        ++number;
        if (line.empty() || line[0] == '#')
            continue;
        const size_t t1 = line.find('\t');
        const size_t t2 = line.find('\t', t1 + 1);
        REQUIRE(t1 != std::string::npos && t2 != std::string::npos);
        const std::string name = from_hex(line.substr(0, t1));
        const bool safe = line.substr(t1 + 1, t2 - t1 - 1) == "1";
        std::vector<std::string> parts;
        const std::string parts_field = line.substr(t2 + 1);
        if (parts_field != "-")
        {
            size_t start = 0;
            while (start <= parts_field.size())
            {
                size_t comma = parts_field.find(',', start);
                if (comma == std::string::npos)
                    comma = parts_field.size();
                parts.push_back(from_hex(parts_field.substr(start, comma - start)));
                start = comma + 1;
            }
        }
        SCOPED_CONTEXT("fixture line " + std::to_string(number) + " [" + name + "]");
        CHECK_EQ(is_safe_cab_member_name(name), safe);
        CHECK_EQ(windows_path_parts(name), parts);
        ++checked;
        if (safe)
            ++safe_count;
    }
    CHECK(checked > 2000u);
    CHECK(safe_count > 400u);
    CHECK(checked - safe_count > 400u);
}
