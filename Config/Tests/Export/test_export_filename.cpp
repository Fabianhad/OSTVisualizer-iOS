// The suggested file name of an export (EXP-09): Core/Export/export_filename.*.
// Windows oracle: infrastructure/visualization_provider.py _prepare_export_filename and infrastructure/utils/filename.py
// sanitize_filename, run read-only (ExportFilenameOracleData.hpp: the Windows test cases for every extension plus 380
// seeded random cases).
#include "ExportTestSupport.hpp"
#include "ExportFilenameOracleData.hpp"
#include "Export/export_filename.hpp"

#include <stdexcept>

using ost_geometry::export_text::export_filename;
using ost_geometry::export_text::sanitize_filename;

namespace
{
    std::string rep(const std::string &unit, size_t count) { return export_filename_oracle::rep(unit, count); }
} // namespace

TEST(ExportFilename, SanitizeReplacesTheWindowsInvalidCharacters)
{
    // Port of tests/infrastructure/test_visualization_provider.py::ExportFilenameTests::
    // test_suggested_export_filename_replaces_windows_control_characters: [\x00-\x1f\\/:"*?<>|] become '_'.
    CHECK_STR_EQ(sanitize_filename("Bid\nOne"), "Bid_One");
    CHECK_STR_EQ(sanitize_filename("A1\tPlan"), "A1_Plan");
    CHECK_STR_EQ(sanitize_filename(std::string("B\0\x1f" "B", 4)), "B__B");
    CHECK_STR_EQ(sanitize_filename("P\r\x01P"), "P__P");
    CHECK_STR_EQ(sanitize_filename("Bid\\\"Q\""), "Bid__Q_");
    CHECK_STR_EQ(sanitize_filename("A/B:*?<>|"), "A_B______");
    // kept: space, no-break space, dot, dash, non-ASCII, DEL (0x7f is outside \x00-\x1f)
    CHECK_STR_EQ(sanitize_filename(" P\xC2\xA0 "), " P\xC2\xA0 ");
    CHECK_STR_EQ(sanitize_filename("B\xC3\xAD" "d - P\xC3\xA1ge.v2\x7f"), "B\xC3\xAD" "d - P\xC3\xA1ge.v2\x7f");
    CHECK_STR_EQ(sanitize_filename(""), "");
}

TEST(ExportFilename, TheWindowsCasesOfEveryExtension)
{
    // Port of tests/infrastructure/test_visualization_provider.py::ExportFilenameCompatibilityTests::
    // test_export_filename_consolidation_preserves_html_and_mesh_policy (the 11 cases, html and obj) and the control
    // character cases of ExportFilenameTests. The plan's corrections: ('Bid\nOne', ['A1\tPlan']) ends in '.pdf' only for
    // pdf, for obj it is 'Bid_One - A1_Plan.obj'.
    struct Case
    {
        std::string bid;
        std::vector<std::string> pages;
        std::string stem;
    };
    const std::vector<Case> cases = {
        {"Bid", {"Page"}, "Bid - Page"},
        {"Bid", {"One", "Two"}, "Bid - One + Two"},
        {"Bid", {"One", "Two", "Three"}, "Bid - One + Two + 1 more"},
        {"Bid", {"A/B:*?<>|"}, "Bid - A_B______"},
        {"Bid", {"A\\B\"C"}, "Bid - A_B_C"},
        {"Bid", {"Tab\tNew\nLine\x1f"}, "Bid - Tab_New_Line_"},
        {"Bid/Phase:*?<>|", {"Page"}, "Bid_Phase______ - Page"},
        {"", {"Page"}, " - Page"},
        {"Bid", {""}, "Bid - "},
        {"B\xC3\xAD" "d", {"P\xC3\xA1ge"}, "B\xC3\xAD" "d - P\xC3\xA1ge"},
        {"Bid", {"Same", "Same"}, "Bid - Same + Same"},
        {"Bid", {"A", "B", "C", "D", "E"}, "Bid - A + B + 3 more"},
        {"Bid\nOne", {"A1\tPlan"}, "Bid_One - A1_Plan"},
        {std::string("B\0\x1f" "B", 4), {"P\r\x01P"}, "B__B - P__P"},
        {"Bid\\\"Q\"", {" P\xC2\xA0 "}, "Bid__Q_ -  P\xC2\xA0 "},
    };
    for (const std::string ext : {"html", "obj", "dxf", "pdf"})
        for (const Case &c : cases)
        {
            SCOPED_CONTEXT("extension " + ext + ", stem " + c.stem);
            CHECK_STR_EQ(export_filename(ext, c.bid, c.pages), c.stem + "." + ext);
        }
}

TEST(ExportFilename, TheLongNameFallbacksOfTheWindowsTests)
{
    // Port of ExportFilenameCompatibilityTests::test_export_filename_consolidation_preserves_long_name_fallbacks and
    // test_export_filename_length_boundary_is_255_characters. The corrections of the EXP-09 plan: ('Bid', ['P'*300]) gives
    // 'P'*247 + '....html' for html but 'P'*248 + '....obj' for obj.
    const std::string long_bid = rep("B", 300);
    CHECK_STR_EQ(export_filename("html", long_bid, {"Page"}), rep("B", 240) + "... - Page.html");
    CHECK_STR_EQ(export_filename("obj", long_bid, {"Page"}), rep("B", 241) + "... - Page.obj");
    const std::string long_page = rep("P", 300);
    CHECK_STR_EQ(export_filename("html", "Bid", {long_page}), rep("P", 247) + "....html");
    CHECK_STR_EQ(export_filename("obj", "Bid", {long_page}), rep("P", 248) + "....obj");
    CHECK_STR_EQ(export_filename("html", "Bid", {long_page, long_page}), "Export_2_pages.html");
    CHECK_STR_EQ(export_filename("obj", "Bid", {long_page, long_page, long_page}), "Export_3_pages.obj");

    const std::string suffix = " - Page.html";
    const std::string exact_bid = rep("B", 255 - suffix.size());
    CHECK_STR_EQ(export_filename("html", exact_bid, {"Page"}), exact_bid + suffix);
    const std::string over_bid = rep("B", 256 - suffix.size());
    const std::string truncated = export_filename("html", over_bid, {"Page"});
    CHECK_EQ(truncated.size(), static_cast<size_t>(255));
    CHECK_STR_EQ(truncated, rep("B", over_bid.size() - 1 - 3) + "..." + suffix);
    CHECK_STR_EQ(export_filename("html", rep("B", 10), {long_page}), rep("P", 247) + "....html");
}

TEST(ExportFilename, TheLengthIsCountedInCodePointsNotBytes)
{
    // Python len() counts code points: 300 two-byte 'e acute' or four-byte emoji characters shorten like 300 ASCII ones
    // (the plan: "Count code points, not UTF-16 units or bytes").
    const std::string eacute = "\xC3\xA9";
    CHECK_STR_EQ(export_filename("obj", rep(eacute, 300), {"Page"}), rep(eacute, 241) + "... - Page.obj");
    const std::string emoji = "\xF0\x9F\x98\x80";
    CHECK_STR_EQ(export_filename("obj", rep(emoji, 300), {"Page"}), rep(emoji, 241) + "... - Page.obj");
    CHECK_STR_EQ(export_filename("obj", "Bid", {rep(emoji, 300)}), rep(emoji, 248) + "....obj");
    // 255 characters of two bytes each (510 bytes) are within the limit and kept whole
    const size_t room = 255 - std::string("Bid - .obj").size();
    CHECK_STR_EQ(export_filename("obj", "Bid", {rep(eacute, room)}), "Bid - " + rep(eacute, room) + ".obj");
}

TEST(ExportFilename, MatchesTheWindowsFunctionOnEveryOracleCase)
{
    // 452 cases generated by the real Windows _prepare_export_filename: the test cases above for html / obj / dxf / pdf and
    // 380 seeded random bid and page names (invalid characters, control characters, non-ASCII, a character outside the BMP,
    // lengths around the limit, one to four pages).
    size_t checked = 0;
    for (const auto &row : export_filename_oracle::rows())
    {
        SCOPED_CONTEXT("case " + std::to_string(checked) + " extension " + row.extension);
        CHECK_STR_EQ(export_filename(row.extension, row.bid_name, row.page_names), row.expected);
        ++checked;
    }
    CHECK(checked > 400);
}

TEST(ExportFilename, WithoutAPageThereIsNoName)
{
    // Windows never asks for a name without a page (NO_DATA first); an empty list is a caller error, not a guessed name.
    bool threw = false;
    try
    {
        export_filename("obj", "Bid", {});
    }
    catch (const std::invalid_argument &)
    {
        threw = true;
    }
    CHECK(threw);
}
