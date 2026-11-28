// IMP-14 (package structure), IMP-16 (image references and path rewrite), IMP-12 (image staging that never overwrites) and IMP-M02 (a
// failing write aborts the import) of the OSP import: Core/Import/OspPackage.hpp.
// Windows oracle: ost_visualizer/infrastructure/mdb/importers/osp_importer.py and tests/infrastructure/mdb/importers/test_osp_importer.py.
// Expected values that no Windows test pins were computed by running the Windows helpers read-only (python -B): the messages of
// _inspect_package, the results of _resolve_image_member / _is_supported_image_path / _image_destination_relative_path and the digests.
#include "OspTestSupport.hpp"

#include "Import/CabMemberName.hpp"
#include "Domain/FailureText.hpp"
#include "Import/Digest.hpp"
#include "Import/ImportDiagnostics.hpp"

#include <string>
#include <vector>

using ost_import::ImageMemberIndex;
using ost_import::OspPackage;
using ost_osptest::CountingFs;
using ost_osptest::LogCapture;
using ost_osptest::TempTree;

namespace
{
    std::string inspect_error(const std::vector<std::string> &names)
    {
        OspPackage package;
        std::string error;
        if (ost_import::inspect_osp_package(names, package, error))
            return "<accepted>";
        return error;
    }

    std::string resolve(const std::string &reference, const ImageMemberIndex &index)
    {
        std::string member;
        return ost_import::resolve_image_member(reference, index, member) ? member : "<none>";
    }

    bool starts_with(const std::string &text, const std::string &prefix)
    {
        return text.compare(0, prefix.size(), prefix) == 0;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-14: package structure
// ---------------------------------------------------------------------------------------------------------------

// Port of test_osp_import_requires_exactly_one_top_level_ost_member ([] / two / only nested: "exactly one top-level .ost file; found N"),
// test_osp_import_rejects_unsupported_members_under_temp_images_root, test_osp_import_rejects_case_insensitive_duplicate_members,
// test_package_rejections_use_exact_messages and the item test plan (a CAB with only 'not-a-project.txt'). Messages from the Windows run.
TEST(Imp14_Package, RejectionsUseTheWindowsMessages)
{
    CHECK_STR_EQ(inspect_error({}), "archive must contain exactly one top-level .ost file; found 0");
    CHECK_STR_EQ(inspect_error({"First.ost", "Second.ost"}), "archive must contain exactly one top-level .ost file; found 2");
    CHECK_STR_EQ(inspect_error({"nested\\Project.ost"}), "archive must contain exactly one top-level .ost file; found 0");
    CHECK_STR_EQ(inspect_error({"not-a-project.txt"}), "archive must contain exactly one top-level .ost file; found 0");
    CHECK_STR_EQ(inspect_error({"a.ost", "b.OST"}), "archive must contain exactly one top-level .ost file; found 2");
    CHECK_STR_EQ(inspect_error({"x\\y.ost"}), "archive must contain exactly one top-level .ost file; found 0");

    CHECK_STR_EQ(inspect_error({"Project.ost", "TempImages!.tmp/notes.txt"}),
                 "unsupported member under TempImages!.tmp: 'TempImages!.tmp\\\\notes.txt'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "tempimages!.TMP"}), "unsupported member under TempImages!.tmp: 'tempimages!.TMP'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "TempImages!.tmp\\a.tif", "TempImages!.tmp\\b.TIFF", "TempImages!.tmp\\c.png"}),
                 "unsupported member under TempImages!.tmp: 'TempImages!.tmp\\\\c.png'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "TempImages!.tmp\\it's.txt"}),
                 "unsupported member under TempImages!.tmp: \"TempImages!.tmp\\\\it's.txt\"");

    CHECK_STR_EQ(inspect_error({"Project.ost", "TempImages!.tmp/sheet.pdf", "tempimages!.TMP/SHEET.PDF"}),
                 "duplicate or conflicting CAB member: 'tempimages!.TMP\\\\SHEET.PDF'");
}

// Port of test_casefold_semantics_are_used_for_member_matching_and_staging_keys: sharp s and SS are the same member, the long s makes
// TempImage\u017f!.tmp a different folder name that casefolds to TempImages!.tmp.
TEST(Imp14_Package, MembersAreComparedCasefolded)
{
    CHECK_STR_EQ(inspect_error({"Project.ost", "TempImages!.tmp\\Stra\xC3\x9F"
                                               "e.pdf",
                                "TempImages!.tmp\\STRASSE.pdf"}),
                 "duplicate or conflicting CAB member: 'TempImages!.tmp\\\\STRASSE.pdf'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "TempImage\xC5\xBF!.tmp\\notes.txt"}),
                 "unsupported member under TempImages!.tmp: 'TempImage\xC5\xBF!.tmp\\\\notes.txt'");
}

// Port of test_osp_import_rejects_unsafe_cab_member_paths_before_extraction, test_real_archives_with_these_member_names_are_rejected_unextracted
// and the reserved-name tests: the unsafe-name message quotes the name as the cabinet lists it.
TEST(Imp14_Package, UnsafeNamesAreRejectedFirst)
{
    CHECK_STR_EQ(inspect_error({"Project.ost", "payload.ost:stream"}), "unsafe CAB member path: 'payload.ost:stream'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "."}), "unsafe CAB member path: '.'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "../x.pdf"}), "unsafe CAB member path: '../x.pdf'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "a/.."}), "unsafe CAB member path: 'a/..'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "CON.pdf"}), "unsafe CAB member path: 'CON.pdf'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "Plan."}), "unsafe CAB member path: 'Plan.'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "//server/share/x"}), "unsafe CAB member path: '//server/share/x'");
    CHECK_STR_EQ(inspect_error({"Project.ost", "\\\\server\\share\\payload.ost"}),
                 "unsafe CAB member path: '\\\\\\\\server\\\\share\\\\payload.ost'");
    // the first problem in member order wins, also before a missing .ost is noticed
    CHECK_STR_EQ(inspect_error({"CON.pdf"}), "unsafe CAB member path: 'CON.pdf'");
}

// Port of test_osp_import_accepts_nested_legacy_visualizer_layout and test_package_inspection_normalizes_member_names_and_indexes_images.
TEST(Imp14_Package, TheLegacyNestedLayoutAndNormalisedNames)
{
    OspPackage package;
    std::string error;
    REQUIRE(ost_import::inspect_osp_package({"Project.ost", "TempImages!.tmp/generated-folder/sheet.pdf"}, package, error));
    CHECK_STR_EQ(package.ost_member_name, "Project.ost");
    REQUIRE_EQ(package.image_members_by_path.size(), 1u);
    CHECK_STR_EQ(package.image_members_by_path[0].first, "tempimages!.tmp\\generated-folder\\sheet.pdf");
    CHECK_STR_EQ(package.image_members_by_path[0].second, "TempImages!.tmp\\generated-folder\\sheet.pdf");

    REQUIRE(ost_import::inspect_osp_package({"Plan.OST", "TempImages!.tmp/Sub/Sheet.PDF", "notes\\readme.txt", "Other.tif"}, package, error));
    CHECK_EQ(package.member_names.size(), 4u);
    CHECK_STR_EQ(package.member_names[0], "Plan.OST");
    CHECK_STR_EQ(package.member_names[1], "TempImages!.tmp\\Sub\\Sheet.PDF");
    CHECK_STR_EQ(package.member_names[2], "notes\\readme.txt");
    CHECK_STR_EQ(package.member_names[3], "Other.tif");
    CHECK_STR_EQ(package.ost_member_name, "Plan.OST");
    REQUIRE_EQ(package.image_members_by_path.size(), 1u);
    CHECK_STR_EQ(package.image_members_by_path[0].first, "tempimages!.tmp\\sub\\sheet.pdf");
    CHECK_STR_EQ(package.image_members_by_path[0].second, "TempImages!.tmp\\Sub\\Sheet.PDF");

    // names with quotes are fine outside TempImages!.tmp
    REQUIRE(ost_import::inspect_osp_package({"Project.ost", "it's.pdf", "a\"b.pdf"}, package, error));
    CHECK(package.image_members_by_path.empty());
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-16: image references
// ---------------------------------------------------------------------------------------------------------------

// Port of test_image_references_match_the_deepest_member_suffix (+ the extra references of the Windows run).
TEST(Imp16_References, TheDeepestMemberSuffixWins)
{
    const ImageMemberIndex members =
        ost_osptest::image_members({"TempImages!.tmp\\b.pdf", "TempImages!.tmp\\a\\b.pdf", "TempImages!.tmp\\x\\y\\z.pdf"});
    struct Case
    {
        const char *reference;
        const char *expected;
    };
    const Case cases[] = {
        {"a\\b.pdf", "TempImages!.tmp\\a\\b.pdf"},
        {"C:\\plans\\a\\b.pdf", "TempImages!.tmp\\a\\b.pdf"},
        {"C:/plans/other/b.pdf", "TempImages!.tmp\\b.pdf"},
        {"z.pdf", "<none>"},
        {"y\\z.pdf", "<none>"},
        {"x\\y\\z.pdf", "TempImages!.tmp\\x\\y\\z.pdf"},
        {"TempImages!.tmp\\A\\B.PDF", "TempImages!.tmp\\a\\b.pdf"},
        {"TempImages!.tmp\\missing.pdf", "<none>"},
        // from the Windows run
        {"b.pdf", "TempImages!.tmp\\b.pdf"},
        {"A/B.pdf", "TempImages!.tmp\\a\\b.pdf"},
        {"C:\\B.PDF", "TempImages!.tmp\\b.pdf"},
        {"\\b.pdf", "TempImages!.tmp\\b.pdf"},
        {"C:b.pdf", "TempImages!.tmp\\b.pdf"},
        {"tempimages!.tmp\\b.pdf", "TempImages!.tmp\\b.pdf"},
        {"TempImages!.tmp/x/y/z.pdf", "TempImages!.tmp\\x\\y\\z.pdf"},
        {"q:\\x\\y\\z.pdf", "TempImages!.tmp\\x\\y\\z.pdf"},
        {"x/y/Z.PDF", "TempImages!.tmp\\x\\y\\z.pdf"},
        {"/x/y/z.pdf", "TempImages!.tmp\\x\\y\\z.pdf"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.reference);
        CHECK_STR_EQ(resolve(c.reference, members), c.expected);
    }
}

// Port of test_osp_image_member_resolution_prefers_deepest_match_in_any_member_order and
// test_explicit_package_root_references_never_fall_back_to_a_flat_basename.
TEST(Imp16_References, TheDeepestMatchIsIndependentOfTheMemberOrder)
{
    const std::string flat = "TempImages!.tmp\\A701-D.pdf";
    const std::string mid = "TempImages!.tmp\\drawings\\A701-D.pdf";
    const std::string deep = "TempImages!.tmp\\estimates\\project\\drawings\\A701-D.pdf";
    const std::string reference = "Q:\\estimates\\project\\drawings\\A701-D.pdf";
    const std::vector<std::vector<std::string>> orders = {{flat, mid, deep}, {deep, mid, flat}, {mid, deep, flat}};
    for (const auto &order : orders)
    {
        CHECK_STR_EQ(resolve(reference, ost_osptest::image_members(order)), deep);
    }
    CHECK_STR_EQ(resolve("C:\\other\\drawings\\A701-D.pdf", ost_osptest::image_members({deep, flat, mid})), mid);
    CHECK_STR_EQ(resolve("C:\\other\\A701-D.pdf", ost_osptest::image_members({deep, mid})), "<none>");

    const ImageMemberIndex only_flat = ost_osptest::image_members({"TempImages!.tmp\\b.pdf"});
    CHECK_STR_EQ(resolve("TempImages!.tmp\\b.pdf", only_flat), "TempImages!.tmp\\b.pdf");
    CHECK_STR_EQ(resolve("TempImages!.tmp\\other\\b.pdf", only_flat), "<none>");
    CHECK_STR_EQ(resolve("tempimages!.TMP\\deep\\er\\b.pdf", only_flat), "<none>");
}

// Port of test_osp_import_does_not_fall_back_to_nested_basename: a flat reference never matches a nested member by its basename.
TEST(Imp16_References, NoBasenameFallbackIntoNestedMembers)
{
    const ImageMemberIndex nested = ost_osptest::image_members({"TempImages!.tmp\\first\\sheet.pdf"});
    CHECK_STR_EQ(resolve("C:\\unrelated\\sheet.pdf", nested), "<none>");
    CHECK_STR_EQ(resolve("sheet.pdf", nested), "<none>");
    CHECK_STR_EQ(resolve("first\\sheet.pdf", nested), "TempImages!.tmp\\first\\sheet.pdf");
}

// Port of test_casefold_semantics_are_used_for_member_matching_and_staging_keys (the resolver part).
TEST(Imp16_References, MatchingIsCasefolded)
{
    CHECK_STR_EQ(resolve("C:\\x\\Stra\xC3\x9F"
                         "e.pdf",
                         ost_osptest::image_members({"TempImages!.tmp\\STRASSE.pdf"})),
                 "TempImages!.tmp\\STRASSE.pdf");
    CHECK_STR_EQ(resolve("C:\\x\\STRASSE.pdf", ost_osptest::image_members({"TempImages!.tmp\\Stra\xC3\x9F"
                                                                           "e.pdf"})),
                 "TempImages!.tmp\\Stra\xC3\x9F"
                 "e.pdf");
}

// Port of test_supported_image_paths_depend_on_the_final_component_suffix (+ the Windows run for further spellings).
TEST(Imp16_References, OnlyPdfAndTifExtensionsOfTheFinalComponentCount)
{
    struct Case
    {
        const char *path;
        bool expected;
    };
    const Case cases[] = {
        {"C:\\plans\\a.pdf", true}, {"C:/plans/a.PDF", true}, {"a.tif", true},    {"a.TIFF", true},         {"a.png", false},
        {"a", false},               {"C:\\plans\\.pdf", false}, {"C:/plans/.pdf", false}, {"dir.pdf\\a", false}, {"dir.pdf/a", false},
        {"a.pdf ", false},          {"a.pdf.", false},          {"x.pdf\\", true},        {"C:\\", false},       {"", false},
        {".", false},               {"..pdf", true},            {"a..pdf", true},         {"a.PdF", true},       {"C:x.pdf", true},
        {"a.t\xC4\xB0"
         "f",
         false}, // U+0130: its lower-case form is not an ASCII i
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.path);
        CHECK_EQ(ost_import::is_supported_image_path(c.path), c.expected);
    }
}

// Port of test_image_reference_collection_checks_each_attribute_independently.
TEST(Imp16_References, EachAttributeIsCheckedOnItsOwn)
{
    const ImageMemberIndex members = ost_osptest::image_members({"TempImages!.tmp\\over.pdf", "TempImages!.tmp\\img.pdf"});
    ost_import::ImageReferenceScan scan;
    std::string parse_error;

    // an empty ImagePath does not stop the OverlayImagePath
    {
        LogCapture log;
        const std::string xml = ost_osptest::page_xml({{"ImagePath", ""}, {"OverlayImagePath", "C:\\x\\over.pdf"}});
        REQUIRE(ost_import::collect_image_references(xml.data(), xml.size(), members, scan, parse_error));
        REQUIRE_EQ(scan.references.size(), 1u);
        CHECK_STR_EQ(scan.references[0].original_path, "C:\\x\\over.pdf");
        CHECK_STR_EQ(scan.references[0].member_name, "TempImages!.tmp\\over.pdf");
        CHECK(log.records.empty());
    }
    // an unsupported ImagePath is skipped silently (no warning), the OverlayImagePath still resolves
    {
        LogCapture log;
        const std::string xml = ost_osptest::page_xml({{"ImagePath", "C:\\x\\notes.txt"}, {"OverlayImagePath", "C:\\x\\over.pdf"}});
        REQUIRE(ost_import::collect_image_references(xml.data(), xml.size(), members, scan, parse_error));
        REQUIRE_EQ(scan.references.size(), 1u);
        CHECK_STR_EQ(scan.references[0].original_path, "C:\\x\\over.pdf");
        CHECK(log.records.empty());
        CHECK_EQ(scan.unresolved_count, 0u);
    }
    // a missing ImagePath is one warning with the exact text, the OverlayImagePath still resolves
    {
        LogCapture log;
        const std::string xml = ost_osptest::page_xml({{"ImagePath", "C:\\x\\gone.pdf"}, {"OverlayImagePath", "C:\\x\\over.pdf"}});
        REQUIRE(ost_import::collect_image_references(xml.data(), xml.size(), members, scan, parse_error));
        REQUIRE_EQ(scan.references.size(), 1u);
        CHECK_STR_EQ(scan.references[0].original_path, "C:\\x\\over.pdf");
        CHECK_EQ(scan.unresolved_count, 1u);
        CHECK_STR_EQ(scan.first_unresolved, "C:\\x\\gone.pdf");
        REQUIRE_EQ(log.records.size(), 1u);
        CHECK(log.records[0].level == ost_log::Level::Warning);
        CHECK_STR_EQ(log.records[0].message,
                     "OSP import could not resolve 1 referenced image(s) in the package. Import will continue and preserve those "
                     "original paths. First missing source path: C:\\x\\gone.pdf");
    }
}

// Port of test_only_page_rows_contribute_image_references_and_rewrites (the collection part): an element that is not a BidPage never
// contributes, a BidPage counts wherever it is in the document (the Windows code walks root.iter("BidPage")).
TEST(Imp16_References, OnlyBidPageElementsContribute)
{
    const ImageMemberIndex members = ost_osptest::image_members({"TempImages!.tmp\\plan.pdf"});
    ost_import::ImageReferenceScan scan;
    std::string parse_error;
    {
        LogCapture log;
        const std::string xml = "<Root><Other ImagePath='C:\\x\\plan.pdf'/><BidPage ImagePath='C:\\x\\plan.pdf'/></Root>";
        REQUIRE(ost_import::collect_image_references(xml.data(), xml.size(), members, scan, parse_error));
        REQUIRE_EQ(scan.references.size(), 1u);
        CHECK_STR_EQ(scan.references[0].original_path, "C:\\x\\plan.pdf");
        CHECK(log.records.empty());
    }
    {
        LogCapture log;
        const std::string xml = "<Root><Other ImagePath='C:\\x\\plan.pdf'/></Root>";
        REQUIRE(ost_import::collect_image_references(xml.data(), xml.size(), members, scan, parse_error));
        CHECK(scan.references.empty());
        CHECK(log.records.empty());
    }
    // the root itself and a nested BidPage count too (Element.iter includes the element it is called on)
    {
        const std::string xml = "<BidPage ImagePath='C:\\r\\plan.pdf'><X><BidPage OverlayImagePath='D:\\s\\plan.pdf'/></X></BidPage>";
        REQUIRE(ost_import::collect_image_references(xml.data(), xml.size(), members, scan, parse_error));
        REQUIRE_EQ(scan.references.size(), 2u);
        CHECK_STR_EQ(scan.references[0].original_path, "C:\\r\\plan.pdf");
        CHECK_STR_EQ(scan.references[1].original_path, "D:\\s\\plan.pdf");
    }
}

// The same original path is one reference however often it appears; the distinct missing paths are counted once each, the first one is named.
TEST(Imp16_References, DistinctPathsAreCountedOnce)
{
    const ImageMemberIndex members = ost_osptest::image_members({"TempImages!.tmp\\plan.pdf"});
    LogCapture log;
    ost_import::ImageReferenceScan scan;
    std::string parse_error;
    const std::string xml =
        "<R><BidPage ImagePath='C:\\a\\plan.pdf' OverlayImagePath='C:\\a\\plan.pdf'/>"
        "<BidPage ImagePath='C:\\a\\plan.pdf'/><BidPage ImagePath='C:\\m\\one.pdf'/><BidPage ImagePath='C:\\m\\two.pdf' "
        "OverlayImagePath='C:\\m\\one.pdf'/></R>";
    REQUIRE(ost_import::collect_image_references(xml.data(), xml.size(), members, scan, parse_error));
    CHECK_EQ(scan.references.size(), 1u);
    CHECK_EQ(scan.unresolved_count, 2u);
    CHECK_STR_EQ(scan.first_unresolved, "C:\\m\\one.pdf");
    REQUIRE_EQ(log.records.size(), 1u);
    CHECK(starts_with(log.records[0].message, "OSP import could not resolve 2 referenced image(s) in the package."));
}

// Port of test_corrupt_embedded_ost_message_names_archive_and_parse_error (the parse part): ET.parse of "<XML_ROOT>" fails with
// "no element found: line L, column C".
TEST(Imp16_References, AMalformedFileIsAParseError)
{
    ost_import::ImageReferenceScan scan;
    std::string parse_error;
    const std::string xml = "<XML_ROOT>";
    CHECK_FALSE(ost_import::collect_image_references(xml.data(), xml.size(), ost_osptest::image_members({}), scan, parse_error));
    CHECK(starts_with(parse_error, "no element found: line "));
}

// Port of test_image_paths_are_rewritten_only_for_copied_references (on the parsed page rows): a map that names no path leaves every row
// as it is; the references that were copied are replaced, an empty value is never touched, "Other" rows do not exist in this model.
TEST(Imp16_Rewrite, OnlyCopiedReferencesAreRewritten)
{
    using ost_import::Row;
    ost_import::RowList rows = {Row{{"ImagePath", "C:\\a.png"}, {"OverlayImagePath", "C:\\b.png"}}, Row{{"ImagePath", ""}}, Row{},
                                Row{{"ImagePath", "C:\\other.png"}}};
    const ost_import::RowList original = rows;
    CHECK_FALSE(ost_import::rewrite_image_paths(rows, {{"C:\\zzz.png", "D:\\x.png"}}));
    CHECK(rows == original);

    CHECK(ost_import::rewrite_image_paths(
        rows, {{"C:\\a.png", "D:\\one.png"}, {"C:\\b.png", "D:\\two.png"}, {"", "D:\\blank.png"}}));
    CHECK_STR_EQ(rows[0]["ImagePath"], "D:\\one.png");
    CHECK_STR_EQ(rows[0]["OverlayImagePath"], "D:\\two.png");
    CHECK_STR_EQ(rows[1]["ImagePath"], "");
    CHECK_EQ(rows[1].count("OverlayImagePath"), 0u);
    CHECK(rows[2].empty());
    CHECK_STR_EQ(rows[3]["ImagePath"], "C:\\other.png");
    CHECK_EQ(rows[3].count("OverlayImagePath"), 0u);
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-12: where an image is staged
// ---------------------------------------------------------------------------------------------------------------

// Port of test_staged_image_locations_are_flat_or_addressed_by_member_name (+ test_casefold_semantics..., the Windows run for the digests):
// a member directly below TempImages!.tmp is "<name>", anything deeper is "Images/<sha1(casefolded member name)[:16]>/<name>".
TEST(Imp12_Destination, FlatOrAddressedByMemberName)
{
    struct Case
    {
        const char *member;
        const char *expected;
    };
    const Case cases[] = {
        {"TempImages!.tmp\\plan.pdf", "plan.pdf"},
        {"TempImages!.tmp\\Sheets\\Plan.pdf", "Images/44bda7d1a07736ab/Plan.pdf"},
        {"TempImages!.tmp\\Sub\\Stra\xC3\x9F"
         "e.pdf",
         "Images/c606eb2e9116f351/Stra\xC3\x9F"
         "e.pdf"},
        {"TempImages!.tmp\\estimates\\project\\drawings\\A701-D.pdf", "Images/cf1ca6ac4eb064be/A701-D.pdf"},
        {"tempimages!.TMP\\a\\b.pdf", "Images/3ec4044999b8ac05/b.pdf"},
        {"TempImages!.tmp\\a\\b\\c\\d.tif", "Images/8378fc43fb29af61/d.tif"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.member);
        std::string relative;
        std::string error;
        REQUIRE(ost_import::image_destination_relative_path(c.member, relative, error));
        CHECK_STR_EQ(relative, c.expected);
    }
}

// Port of test_staged_image_names_reject_reserved_device_names / test_a_reserved_image_destination_is_never_copied (messages from the Windows run).
TEST(Imp12_Destination, ReservedDeviceNamesAreRefused)
{
    struct Case
    {
        const char *member;
        const char *message;
    };
    const Case cases[] = {
        {"TempImages!.tmp\\COM3.pdf", "image staging name is a reserved device name: 'TempImages!.tmp\\\\COM3.pdf'"},
        {"TempImages!.tmp\\sub\\aux.pdf", "image staging name is a reserved device name: 'TempImages!.tmp\\\\sub\\\\aux.pdf'"},
        {"TempImages!.tmp\\nul\\plan.pdf", "image staging name is a reserved device name: 'TempImages!.tmp\\\\nul\\\\plan.pdf'"},
        {"TempImages!.tmp\\Lpt2.tif", "image staging name is a reserved device name: 'TempImages!.tmp\\\\Lpt2.tif'"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.member);
        std::string relative;
        std::string error;
        CHECK_FALSE(ost_import::image_destination_relative_path(c.member, relative, error));
        CHECK_STR_EQ(error, c.message);
    }
}

// Port of test_conflicting_image_names_get_a_content_digest_suffix_that_grows.
TEST(Imp12_NonConflicting, TheDigestSuffixGrowsThenGivesUp)
{
    TempTree tree("nonconflicting");
    ost_import::StdOspFileSystem fs;
    tree.write("source.png", "new image");
    const std::string digest = ost_import::digest::sha256_hex("new image");
    CHECK_EQ(digest.size(), 64u);
    const std::string source = tree.path("source.png");
    const std::string destination = tree.path("plan.png");
    std::string chosen;
    std::string error;

    // nothing there: the destination itself
    REQUIRE(ost_import::non_conflicting_image_destination(fs, source, destination, chosen, error));
    CHECK_STR_EQ(chosen, destination);

    tree.write("plan.png", "old");
    std::vector<std::string> names;
    const size_t lengths[] = {16, 32, 64};
    for (const size_t length : lengths)
    {
        REQUIRE(ost_import::non_conflicting_image_destination(fs, source, destination, chosen, error));
        const std::string expected = "plan-" + digest.substr(0, length) + ".png";
        CHECK_STR_EQ(chosen, tree.path(expected));
        names.push_back(expected);
        tree.write(expected, "different " + std::to_string(length));
    }
    // all three taken by other content: the Windows error
    CHECK_FALSE(ost_import::non_conflicting_image_destination(fs, source, destination, chosen, error));
    CHECK_STR_EQ(error, "image staging content-address collision for 'plan.png'");

    // a candidate that already holds the same bytes is reused
    tree.write(names[1], "new image");
    REQUIRE(ost_import::non_conflicting_image_destination(fs, source, destination, chosen, error));
    CHECK_STR_EQ(chosen, tree.path(names[1]));

    // and so is the destination itself when it holds the same bytes
    tree.write("plan.png", "new image");
    REQUIRE(ost_import::non_conflicting_image_destination(fs, source, destination, chosen, error));
    CHECK_STR_EQ(chosen, destination);
}

// Port of test_staging_names_taken_by_directories_are_never_reused.
TEST(Imp12_NonConflicting, ANameTakenByADirectoryIsNeverReused)
{
    TempTree tree("directories");
    ost_import::StdOspFileSystem fs;
    tree.write("source.png", "new image");
    const std::string digest = ost_import::digest::sha256_hex("new image");
    tree.make_directory("plan.png");
    std::string chosen;
    std::string error;
    REQUIRE(ost_import::non_conflicting_image_destination(fs, tree.path("source.png"), tree.path("plan.png"), chosen, error));
    CHECK_STR_EQ(chosen, tree.path("plan-" + digest.substr(0, 16) + ".png"));
    tree.make_directory("plan-" + digest.substr(0, 16) + ".png");
    REQUIRE(ost_import::non_conflicting_image_destination(fs, tree.path("source.png"), tree.path("plan.png"), chosen, error));
    CHECK_STR_EQ(chosen, tree.path("plan-" + digest.substr(0, 32) + ".png"));
}

// Port of test_a_destination_that_is_not_a_regular_file_counts_as_different_content (Windows b9902e6). The source is EMPTY on purpose:
// on NTFS a directory reports st_size 0, so a size comparison alone would call an empty file and a directory "the same" and then fail
// opening the directory. Windows answers "different" first, because the second path is not a regular file; iOS does the same with
// OspFileSystem::is_file (never with luck: StdOspFileSystem::file_size of a directory is an error that would fail the comparison).
// Pin: iOS already behaved like this, the test is not a red-first test. The digest literal is the SHA-256 of the empty string (printed by
// the Core digest and by Python hashlib.sha256(b'').hexdigest(): e3b0c442 98fc1c14 9afbf4c8 996fb924 27ae41e4 649b934c a495991b 7852b855).
TEST(Imp12_NonConflicting, AnEmptySourceAndADirectoryDestinationCountAsDifferentContent)
{
    TempTree tree("empty_source_directory");
    ost_import::StdOspFileSystem fs;
    tree.write("source.png", "");
    tree.make_directory("plan.png");
    const std::string digest = ost_import::digest::sha256_hex("");
    CHECK_STR_EQ(digest, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // the comparison itself: no error, and a directory is never the same content
    bool same = true;
    CHECK(ost_import::files_have_same_content(fs, tree.path("source.png"), tree.path("plan.png"), same));
    CHECK_FALSE(same);

    // the first candidate is the 16 digit name; the directory is not reused
    const std::string first_candidate = "plan-e3b0c44298fc1c14.png";
    CHECK_STR_EQ(first_candidate, "plan-" + digest.substr(0, 16) + ".png");
    std::string chosen;
    std::string error;
    REQUIRE(ost_import::non_conflicting_image_destination(fs, tree.path("source.png"), tree.path("plan.png"), chosen, error));
    CHECK_STR_EQ(chosen, tree.path(first_candidate));
    CHECK(error.empty());

    // that candidate taken by a directory too: the 32 digit name
    tree.make_directory(first_candidate);
    REQUIRE(ost_import::non_conflicting_image_destination(fs, tree.path("source.png"), tree.path("plan.png"), chosen, error));
    CHECK_STR_EQ(chosen, tree.path("plan-e3b0c44298fc1c149afbf4c8996fb924.png"));
    CHECK_STR_EQ(chosen, tree.path("plan-" + digest.substr(0, 32) + ".png"));

    // choosing created nothing: the destination is still a directory and the folder holds exactly the three entries
    CHECK(tree.is_directory("plan.png"));
    CHECK(tree.is_directory(first_candidate));
    CHECK(tree.entries_in(".") == (std::vector<std::string>{first_candidate, "plan.png", "source.png"}));
    CHECK(tree.files_below(".") == (std::vector<std::string>{"source.png"}));

    // all three names taken by directories: the Windows content-address collision error
    tree.make_directory("plan-" + digest.substr(0, 32) + ".png");
    tree.make_directory("plan-" + digest + ".png");
    CHECK_FALSE(ost_import::non_conflicting_image_destination(fs, tree.path("source.png"), tree.path("plan.png"), chosen, error));
    CHECK_STR_EQ(error, "image staging content-address collision for 'plan.png'");
}

// Direct pins of files_have_same_content (Windows _files_have_same_content: `if not second.is_file(): return False` runs before any size
// or open). A directory or a missing path as the second argument is "different" and not an error, whatever the sizes: an empty source
// (a directory reports size 0 on NTFS), a 9 byte source and a 4096 byte source (4096 is the size a directory reports on most file
// systems). Controls: the same bytes in two regular files are "same", so the pins are not vacuous. Pin: iOS already behaved like this.
TEST(Imp12_NonConflicting, ASecondPathThatIsNotARegularFileIsAlwaysDifferentContent)
{
    TempTree tree("not_regular");
    ost_import::StdOspFileSystem fs;
    tree.write("empty.bin", "");
    tree.write("nine.bin", "new image");
    tree.write("page.bin", std::string(4096, '\x07'));
    tree.make_directory("folder");
    tree.write("folder/inner.bin", "new image");

    auto compare = [&](const char *a, const char *b, bool &same)
    {
        same = true; // the function must overwrite it
        return ost_import::files_have_same_content(fs, tree.path(a), tree.path(b), same);
    };
    bool same = true;

    // a regular file against a directory: different, success, for equal and for different sizes
    for (const char *source : {"empty.bin", "nine.bin", "page.bin"})
    {
        SCOPED_CONTEXT(std::string("source ") + source);
        CHECK(compare(source, "folder", same));
        CHECK_FALSE(same);
    }
    // the directory is not looked into: a file of the same bytes inside it changes nothing
    CHECK(compare("nine.bin", "folder", same));
    CHECK_FALSE(same);

    // a regular file against a path that does not exist: different, success (Windows is_file() is False for it too)
    for (const char *source : {"empty.bin", "nine.bin", "page.bin"})
    {
        SCOPED_CONTEXT(std::string("source ") + source);
        CHECK(compare(source, "missing.bin", same));
        CHECK_FALSE(same);
        CHECK(compare(source, "missing/inner.bin", same));
        CHECK_FALSE(same);
    }

    // controls: the same bytes in a regular file are the same content, other bytes or another size are not
    tree.write("empty2.bin", "");
    tree.write("nine2.bin", "new image");
    tree.write("ten.bin", "new image!");
    CHECK(compare("empty.bin", "empty2.bin", same));
    CHECK(same);
    CHECK(compare("nine.bin", "nine2.bin", same));
    CHECK(same);
    CHECK(compare("nine.bin", "ten.bin", same));
    CHECK_FALSE(same);
    CHECK(compare("nine.bin", "folder/inner.bin", same));
    CHECK(same);

    // nothing was created or changed by the comparisons
    CHECK(tree.is_directory("folder"));
    CHECK_FALSE(tree.exists("missing.bin"));
    CHECK_FALSE(tree.exists("missing"));
}

// Port of test_large_files_are_compared_and_hashed_across_chunk_boundaries and test_value_comparisons_return_exact_booleans.
TEST(Imp12_NonConflicting, FilesAreComparedAcrossChunkBoundaries)
{
    TempTree tree("chunks");
    ost_import::StdOspFileSystem fs;
    const size_t chunk = 1024 * 1024;
    const std::string base(2 * chunk + 10, '\x01');
    tree.write("first.bin", base);
    tree.write("copy.bin", base);
    std::string last = base;
    last.back() = '\x02';
    tree.write("last.bin", last);
    std::string mid = base;
    mid[chunk + 5] = '\x02';
    tree.write("mid.bin", mid);
    tree.write("short.bin", base.substr(0, base.size() - 1));
    tree.write("exact.bin", std::string(chunk, '\x03'));
    tree.write("exact2.bin", std::string(chunk, '\x03'));
    tree.write("empty.bin", "");
    tree.write("empty2.bin", "");

    auto same = [&](const char *a, const char *b)
    {
        bool result = false;
        REQUIRE(ost_import::files_have_same_content(fs, tree.path(a), tree.path(b), result));
        return result;
    };
    CHECK(same("first.bin", "copy.bin"));
    CHECK_FALSE(same("first.bin", "last.bin"));
    CHECK_FALSE(same("first.bin", "mid.bin"));
    CHECK_FALSE(same("first.bin", "short.bin"));
    CHECK(same("exact.bin", "exact2.bin"));
    CHECK(same("empty.bin", "empty2.bin"));

    tree.write("base.bin", "abcdef");
    tree.write("same.bin", "abcdef");
    tree.write("other_size.bin", "abcde");
    tree.write("other_bytes.bin", "abcdeX");
    CHECK(same("base.bin", "same.bin"));
    CHECK_FALSE(same("base.bin", "other_size.bin"));
    CHECK_FALSE(same("base.bin", "other_bytes.bin"));
}

namespace
{
    ost_import::ImageReference ref(const char *original, const char *member)
    {
        return ost_import::ImageReference{original, member};
    }
}

// Port of test_osp_import_does_not_overwrite_different_existing_image (item test plan 1): the existing drawing stays, the new file is
// sheet-<sha256('new drawing')[:16]>.pdf and the original path is mapped to it.
TEST(Imp12_Copy, ADifferentExistingImageIsNeverOverwritten)
{
    TempTree tree("different");
    ost_import::StdOspFileSystem fs;
    tree.write("extract/TempImages!.tmp/sheet.pdf", "new drawing");
    tree.write("dest/sheet.pdf", "existing drawing");
    std::vector<std::pair<std::string, std::string>> copied;
    std::string error;
    REQUIRE(ost_import::copy_referenced_images(fs, tree.path("extract"), tree.path("dest"),
                                               {ref("C:\\plans\\sheet.pdf", "TempImages!.tmp\\sheet.pdf")}, copied, error));
    CHECK_STR_EQ(tree.read("dest/sheet.pdf"), "existing drawing");
    const std::string expected_name = "sheet-f9ee0dc8058e14f8.pdf"; // sha256('new drawing')[:16] (hashlib)
    CHECK_STR_EQ(ost_import::digest::sha256_hex("new drawing").substr(0, 16), "f9ee0dc8058e14f8");
    CHECK_STR_EQ(tree.read("dest/" + expected_name), "new drawing");
    CHECK_EQ(tree.entries_in("dest").size(), 2u);
    REQUIRE_EQ(copied.size(), 1u);
    CHECK_STR_EQ(copied[0].first, "C:\\plans\\sheet.pdf");
    CHECK_STR_EQ(copied[0].second, tree.path("dest/" + expected_name));
}

// Port of test_osp_import_reuses_identical_existing_image (item test plan 2): no new file, the path points at the existing file.
TEST(Imp12_Copy, AnIdenticalExistingImageIsReused)
{
    TempTree tree("identical");
    ost_import::StdOspFileSystem fs;
    tree.write("extract/TempImages!.tmp/sheet.pdf", "same drawing");
    tree.write("dest/sheet.pdf", "same drawing");
    std::vector<std::pair<std::string, std::string>> copied;
    std::string error;
    REQUIRE(ost_import::copy_referenced_images(fs, tree.path("extract"), tree.path("dest"),
                                               {ref("C:\\plans\\sheet.pdf", "TempImages!.tmp\\sheet.pdf")}, copied, error));
    CHECK_EQ(tree.entries_in("dest").size(), 1u);
    CHECK_STR_EQ(tree.read("dest/sheet.pdf"), "same drawing");
    REQUIRE_EQ(copied.size(), 1u);
    CHECK_STR_EQ(copied[0].second, tree.path("dest/sheet.pdf"));
}

// Port of test_osp_import_resolves_most_specific_nested_member_without_overwrite (item test plan 3): the flat member is staged as
// dest/A701-D.pdf, the nested one below dest/Images/<sha1 of the casefolded member name>/, each reference mapped to its own file.
TEST(Imp12_Copy, FlatAndNestedMembersWithTheSameBasenameDoNotCollide)
{
    TempTree tree("nested");
    ost_import::StdOspFileSystem fs;
    const std::string flat = "TempImages!.tmp\\A701-D.pdf";
    const std::string nested = "TempImages!.tmp\\estimates\\project\\drawings\\A701-D.pdf";
    tree.write("extract/TempImages!.tmp/A701-D.pdf", "generated");
    tree.write("extract/TempImages!.tmp/estimates/project/drawings/A701-D.pdf", "original");
    std::vector<std::pair<std::string, std::string>> copied;
    std::string error;
    REQUIRE(ost_import::copy_referenced_images(fs, tree.path("extract"), tree.path("dest"),
                                               {ref("C:\\OST\\Project\\A701-D.pdf", flat.c_str()),
                                                ref("Q:\\estimates\\project\\drawings\\A701-D.pdf", nested.c_str())},
                                               copied, error));
    CHECK_STR_EQ(tree.read("dest/A701-D.pdf"), "generated");
    CHECK_STR_EQ(tree.read("dest/Images/cf1ca6ac4eb064be/A701-D.pdf"), "original"); // sha1 from the Windows run
    REQUIRE_EQ(copied.size(), 2u);
    CHECK_STR_EQ(copied[0].second, tree.path("dest/A701-D.pdf"));
    CHECK_STR_EQ(copied[1].second, tree.path("dest/Images/cf1ca6ac4eb064be/A701-D.pdf"));
}

// Port of test_one_packaged_member_referenced_twice_is_copied_once_for_both_paths.
TEST(Imp12_Copy, OneMemberReferencedTwiceIsCopiedOnce)
{
    TempTree tree("twice");
    CountingFs fs;
    tree.write("extract/TempImages!.tmp/plan.pdf", "plan");
    std::vector<std::pair<std::string, std::string>> copied;
    std::string error;
    REQUIRE(ost_import::copy_referenced_images(
        fs, tree.path("extract"), tree.path("dest"),
        {ref("C:\\plans\\plan.pdf", "TempImages!.tmp\\plan.pdf"), ref("D:\\other\\plan.pdf", "TempImages!.tmp\\plan.pdf")}, copied, error));
    CHECK_EQ(fs.copies, 1);
    REQUIRE_EQ(copied.size(), 2u);
    CHECK_STR_EQ(copied[0].second, tree.path("dest/plan.pdf"));
    CHECK_STR_EQ(copied[1].second, tree.path("dest/plan.pdf"));
    CHECK_EQ(tree.entries_in("dest").size(), 1u);
}

// Port of test_staging_helper_shares_one_copy_between_casefold_equal_members (sharp s and SS).
TEST(Imp12_Copy, CasefoldEqualMembersShareOneCopy)
{
    TempTree tree("casefold");
    ost_import::StdOspFileSystem fs;
    tree.write("extract/TempImages!.tmp/Sub/STRASSE.pdf", "alpha");
    tree.write("extract/TempImages!.tmp/Sub/Stra\xC3\x9F"
               "e.pdf",
               "alpha");
    std::vector<std::pair<std::string, std::string>> copied;
    std::string error;
    REQUIRE(ost_import::copy_referenced_images(fs, tree.path("extract"), tree.path("dest"),
                                               {ref("a.pdf", "TempImages!.tmp\\Sub\\STRASSE.pdf"),
                                                ref("b.pdf", "TempImages!.tmp\\Sub\\Stra\xC3\x9F"
                                                             "e.pdf")},
                                               copied, error));
    REQUIRE_EQ(copied.size(), 2u);
    CHECK_STR_EQ(copied[0].second, copied[1].second);
    const std::vector<std::string> files = tree.files_below("dest");
    REQUIRE_EQ(files.size(), 1u);
    CHECK(files[0].size() >= 11 && files[0].substr(files[0].size() - 11) == "STRASSE.pdf");
}

// The Windows collision rule (test_distinct_members_sharing_a_staging_name_are_never_merged, same-content branch), reached through the real
// destination function: m2 is named like the digest-suffixed copy of m1, so both end at the same file and are not merged.
TEST(Imp12_Copy, TwoDifferentMembersEndingAtTheSameFileAreACollision)
{
    TempTree tree("collision");
    ost_import::StdOspFileSystem fs;
    const std::string digest16 = ost_import::digest::sha256_hex("alpha").substr(0, 16);
    tree.write("extract/TempImages!.tmp/plan.pdf", "alpha");
    tree.write("extract/TempImages!.tmp/plan-" + digest16 + ".pdf", "alpha");
    tree.write("dest/plan.pdf", "existing"); // m1 must take the digest name
    std::vector<std::pair<std::string, std::string>> copied;
    std::string error;
    CHECK_FALSE(ost_import::copy_referenced_images(
        fs, tree.path("extract"), tree.path("dest"),
        {ref("a.pdf", "TempImages!.tmp\\plan.pdf"), ref("b.pdf", ("TempImages!.tmp\\plan-" + digest16 + ".pdf").c_str())}, copied, error));
    CHECK_STR_EQ(error, "image staging destination collision between 'TempImages!.tmp\\\\plan-" + digest16 +
                            ".pdf' and 'tempimages!.tmp\\\\plan.pdf'");
}

// Port of test_distinct_members_sharing_a_staging_name_are_never_merged (different content branch): the second member gets its own digest name.
TEST(Imp12_Copy, TwoMembersWithDifferentContentGetTheirOwnNames)
{
    TempTree tree("distinct");
    ost_import::StdOspFileSystem fs;
    const std::string digest16 = ost_import::digest::sha256_hex("alpha").substr(0, 16);
    const std::string bravo16 = ost_import::digest::sha256_hex("bravo").substr(0, 16);
    tree.write("extract/TempImages!.tmp/plan.pdf", "alpha");
    tree.write("extract/TempImages!.tmp/plan-" + digest16 + ".pdf", "bravo");
    tree.write("dest/plan.pdf", "existing");
    std::vector<std::pair<std::string, std::string>> copied;
    std::string error;
    REQUIRE(ost_import::copy_referenced_images(
        fs, tree.path("extract"), tree.path("dest"),
        {ref("a.pdf", "TempImages!.tmp\\plan.pdf"), ref("b.pdf", ("TempImages!.tmp\\plan-" + digest16 + ".pdf").c_str())}, copied, error));
    REQUIRE_EQ(copied.size(), 2u);
    CHECK_STR_EQ(copied[0].second, tree.path("dest/plan-" + digest16 + ".pdf"));
    CHECK_STR_EQ(copied[1].second, tree.path("dest/plan-" + digest16 + "-" + bravo16 + ".pdf"));
    CHECK_STR_EQ(tree.read("dest/plan-" + digest16 + ".pdf"), "alpha");
    CHECK_STR_EQ(tree.read("dest/plan-" + digest16 + "-" + bravo16 + ".pdf"), "bravo");
    CHECK_STR_EQ(tree.read("dest/plan.pdf"), "existing");
}

// Port of test_a_directory_in_place_of_a_packaged_image_is_not_extracted and test_missing_image_member_is_detected_before_any_image_is_copied:
// every needed member must be an extracted FILE before the first copy; the folder of the destination is not even created.
TEST(Imp12_Copy, EveryMemberMustBeExtractedBeforeTheFirstCopy)
{
    TempTree tree("notextracted");
    CountingFs fs;
    tree.make_directory("extract/TempImages!.tmp/plan.pdf"); // a directory where the image should be
    std::vector<std::pair<std::string, std::string>> copied;
    std::string error;
    CHECK_FALSE(ost_import::copy_referenced_images(fs, tree.path("extract"), tree.path("dest"),
                                                   {ref("a.pdf", "TempImages!.tmp\\plan.pdf")}, copied, error));
    CHECK_STR_EQ(error, "image member was not extracted: TempImages!.tmp\\plan.pdf");
    CHECK_FALSE(tree.exists("dest"));

    tree.write("extract/TempImages!.tmp/one.pdf", "one");
    CHECK_FALSE(ost_import::copy_referenced_images(
        fs, tree.path("extract"), tree.path("dest"),
        {ref("a.pdf", "TempImages!.tmp\\one.pdf"), ref("b.pdf", "TempImages!.tmp\\gone.pdf")}, copied, error));
    CHECK_STR_EQ(error, "image member was not extracted: TempImages!.tmp\\gone.pdf");
    CHECK_EQ(fs.copies, 0);
    CHECK_FALSE(tree.exists("dest"));
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-M02: a failing write aborts the import
// ---------------------------------------------------------------------------------------------------------------

// Port of test_a_copy_failure_does_not_rewrite_the_embedded_ost (item test plan): copy 2 of 2 fails ("disk full"), the calls were 2, the
// function reports failure and hands back no mapping to rewrite with.
TEST(ImpM02_StagingFailure, ACopyFailureAbortsTheStaging)
{
    TempTree tree("diskfull");
    CountingFs fs;
    fs.fail_on_copy = 2;
    tree.write("extract/TempImages!.tmp/one.pdf", "one");
    tree.write("extract/TempImages!.tmp/two.pdf", "two");
    std::vector<std::pair<std::string, std::string>> copied;
    std::string error;
    CHECK_FALSE(ost_import::copy_referenced_images(
        fs, tree.path("extract"), tree.path("dest"),
        {ref("C:\\plans\\one.pdf", "TempImages!.tmp\\one.pdf"), ref("C:\\plans\\two.pdf", "TempImages!.tmp\\two.pdf")}, copied, error));
    CHECK_EQ(fs.copies, 2);
    CHECK(starts_with(error, "cannot copy the packaged image TempImages!.tmp\\two.pdf to "));
}

// ---------------------------------------------------------------------------------------------------------------
// The whole package
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    struct Flow
    {
        TempTree tree;
        ost_import::StdOspFileSystem real_fs;
        explicit Flow(const std::string &name) : tree(name) {}

        // Runs stage_osp_package on `package` with the temp folder `tmp` and the images root `images` below the tree.
        bool run(const std::string &package, ost_import::OspStageResult &result, ost_import::OspFileSystem *fs = nullptr)
        {
            return ost_import::stage_osp_package(package.data(), package.size(), tree.path("tmp"), tree.path("images"),
                                                 fs ? *fs : static_cast<ost_import::OspFileSystem &>(real_fs), result);
        }
    };
}

// Port of test_real_archive_stages_images_rewrites_paths_and_removes_the_extraction and test_osp_import_resolves_page_and_overlay_images_from_flat_members:
// the images go to <images root>/<ost name>/, the pages' paths point at the staged files, the extraction folder is gone.
TEST(OspFlow, StagesImagesRewritesPathsAndRemovesTheExtraction)
{
    Flow flow("flow_ok");
    const std::string xml = ost_osptest::page_xml({{"ImagePath", "C:\\plans\\page.pdf"}, {"OverlayImagePath", "TempImages!.tmp\\overlay.tif"}});
    const std::string package = ost_osptest::make_osp({{"Project.ost", xml},
                                                       {"TempImages!.tmp\\page.pdf", "page"},
                                                       {"TempImages!.tmp\\overlay.tif", "overlay"}});
    ost_import::OspStageResult result;
    REQUIRE(flow.run(package, result));
    CHECK_STR_EQ(result.package_name, "Project");
    CHECK_EQ(result.staged_image_count, 2u);
    CHECK_STR_EQ(flow.tree.read("images/Project/page.pdf"), "page");
    CHECK_STR_EQ(flow.tree.read("images/Project/overlay.tif"), "overlay");
    CHECK_STR_EQ(ost_osptest::page_value(result.raw, 0, "ImagePath"), flow.tree.path("images/Project/page.pdf"));
    CHECK_STR_EQ(ost_osptest::page_value(result.raw, 0, "OverlayImagePath"), flow.tree.path("images/Project/overlay.tif"));
    CHECK_STR_EQ(result.raw.bid_row["UID"], "1");
    // the extraction folder (below tmp) was removed, nothing else was left there
    CHECK(flow.tree.entries_in("tmp").empty());
}

// A temporary folder that cannot be created is a device failure: stage_osp_package reports it with the shared constant, writes nothing, and
// the classifier shows it like every other local failure of the file (Windows swallows it inside import_osp: the "damaged" sentence).
namespace
{
    class NoTempFolderFs : public ost_import::StdOspFileSystem
    {
    public:
        bool make_temp_directory(const std::string &, const std::string &, std::string &) override { return false; }
    };
}

TEST(OspFlow, ATemporaryFolderThatCannotBeCreatedFailsTheImportWithTheSharedDiagnostic)
{
    Flow flow("flow_no_temp");
    NoTempFolderFs fs;
    const std::string package = ost_osptest::make_osp({{"Project.ost", ost_osptest::page_xml({})}});
    ost_import::OspStageResult result;
    CHECK_FALSE(flow.run(package, result, &fs));
    CHECK_STR_EQ(result.error, ost_import::kOspTempFolderError);
    CHECK_FALSE(flow.tree.exists("images"));
    CHECK(ost_domain::classify_import_failure(result.error) == ost_domain::ImportFailureKind::Damaged);
}

// Item test plan of IMP-12 end to end: an existing different drawing stays, the new one gets the digest name, the path points at it.
TEST(OspFlow, AnExistingDrawingOfAnotherProjectIsNotOverwritten)
{
    Flow flow("flow_existing");
    flow.tree.write("images/Project/sheet.pdf", "existing drawing");
    const std::string package = ost_osptest::make_osp(
        {{"Project.ost", ost_osptest::page_xml({{"ImagePath", "C:\\plans\\sheet.pdf"}})}, {"TempImages!.tmp\\sheet.pdf", "new drawing"}});
    ost_import::OspStageResult result;
    REQUIRE(flow.run(package, result));
    CHECK_STR_EQ(flow.tree.read("images/Project/sheet.pdf"), "existing drawing");
    CHECK_STR_EQ(flow.tree.read("images/Project/sheet-f9ee0dc8058e14f8.pdf"), "new drawing");
    CHECK_STR_EQ(ost_osptest::page_value(result.raw, 0, "ImagePath"), flow.tree.path("images/Project/sheet-f9ee0dc8058e14f8.pdf"));
}

// IMP-12 item test plan 3 end to end: a flat member and a nested one with the same basename.
TEST(OspFlow, AFlatAndANestedImageWithTheSameBasenameAreBothKept)
{
    Flow flow("flow_nested");
    const std::string xml = ost_osptest::page_xml(
        {{"ImagePath", "C:\\OST\\Project\\A701-D.pdf"}, {"OverlayImagePath", "Q:\\estimates\\project\\drawings\\A701-D.pdf"}});
    const std::string package = ost_osptest::make_osp({{"Project.ost", xml},
                                                       {"TempImages!.tmp\\A701-D.pdf", "generated"},
                                                       {"TempImages!.tmp\\estimates\\project\\drawings\\A701-D.pdf", "original"}});
    ost_import::OspStageResult result;
    REQUIRE(flow.run(package, result));
    CHECK_STR_EQ(flow.tree.read("images/Project/A701-D.pdf"), "generated");
    CHECK_STR_EQ(flow.tree.read("images/Project/Images/cf1ca6ac4eb064be/A701-D.pdf"), "original");
    CHECK_STR_EQ(ost_osptest::page_value(result.raw, 0, "ImagePath"), flow.tree.path("images/Project/A701-D.pdf"));
    CHECK_STR_EQ(ost_osptest::page_value(result.raw, 0, "OverlayImagePath"), flow.tree.path("images/Project/Images/cf1ca6ac4eb064be/A701-D.pdf"));
}

// IMP-16 plan: a page whose image is not in the package imports with the value unchanged and one warning; nothing is staged.
// Port of test_osp_import_preserves_missing_image_reference_and_continues and test_osp_import_does_not_fall_back_to_nested_basename.
TEST(OspFlow, AMissingImageKeepsItsOriginalPath)
{
    Flow flow("flow_missing");
    LogCapture log;
    const std::string package = ost_osptest::make_osp(
        {{"Project.ost", ost_osptest::page_xml({{"ImagePath", "C:\\old\\folder\\missing.pdf"}})}, {"TempImages!.tmp\\first\\sheet.pdf", "nested"}});
    ost_import::OspStageResult result;
    REQUIRE(flow.run(package, result));
    CHECK_STR_EQ(ost_osptest::page_value(result.raw, 0, "ImagePath"), "C:\\old\\folder\\missing.pdf");
    CHECK_EQ(result.staged_image_count, 0u);
    CHECK_FALSE(flow.tree.exists("images"));
    REQUIRE_EQ(log.count(ost_log::Level::Warning), 1u);
    CHECK(starts_with(log.records[0].message, "OSP import could not resolve 1 referenced image(s) in the package."));
}

// Only references with an image extension are touched; a .png reference is neither staged nor rewritten (IMP-16 plan).
TEST(OspFlow, AReferenceWithoutAnImageExtensionIsLeftAlone)
{
    Flow flow("flow_png");
    const std::string package = ost_osptest::make_osp(
        {{"Project.ost", ost_osptest::page_xml({{"ImagePath", "C:\\a.png"}, {"OverlayImagePath", "C:\\b.png"}})}, {"TempImages!.tmp\\a.png2.pdf", "x"}});
    ost_import::OspStageResult result;
    LogCapture log;
    REQUIRE(flow.run(package, result));
    CHECK_STR_EQ(ost_osptest::page_value(result.raw, 0, "ImagePath"), "C:\\a.png");
    CHECK_STR_EQ(ost_osptest::page_value(result.raw, 0, "OverlayImagePath"), "C:\\b.png");
    CHECK(log.records.empty());
    CHECK_FALSE(flow.tree.exists("images"));
}

// IMP-14 end to end: a rejected package writes nothing and creates no temporary folder.
TEST(OspFlow, ARejectedPackageWritesNothing)
{
    Flow flow("flow_rejected");
    struct Case
    {
        std::vector<std::pair<std::string, std::string>> members;
        const char *error;
    };
    const std::string xml = ost_osptest::page_xml({{"ImagePath", "C:\\plans\\sheet.pdf"}});
    const Case cases[] = {
        {{{"First.ost", xml}, {"Second.ost", xml}}, "archive must contain exactly one top-level .ost file; found 2"},
        {{{"nested\\Project.ost", xml}}, "archive must contain exactly one top-level .ost file; found 0"},
        {{{"not-a-project.txt", "x"}}, "archive must contain exactly one top-level .ost file; found 0"},
        {{{"Project.ost", xml}, {"TempImages!.tmp\\notes.txt", "x"}}, "unsupported member under TempImages!.tmp: 'TempImages!.tmp\\\\notes.txt'"},
        {{{"Project.ost", xml}, {"TempImages!.tmp\\sheet.pdf", "a"}, {"tempimages!.TMP\\SHEET.PDF", "b"}},
         "duplicate or conflicting CAB member: 'tempimages!.TMP\\\\SHEET.PDF'"},
        {{{"Project.ost", xml}, {"payload.ost:stream", "x"}}, "unsafe CAB member path: 'payload.ost:stream'"},
        {{{"Project.ost", xml}, {"CON.pdf", "x"}}, "unsafe CAB member path: 'CON.pdf'"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.error);
        ost_import::OspStageResult result;
        CHECK_FALSE(flow.run(ost_osptest::make_osp(c.members), result));
        CHECK_STR_EQ(result.error, c.error);
        CHECK_FALSE(flow.tree.exists("tmp"));
        CHECK_FALSE(flow.tree.exists("images"));
    }
}

// Not a cabinet at all: the message of the iOS screen ("Invalid OSP file format"), nothing created.
TEST(OspFlow, NotACabinetIsInvalid)
{
    Flow flow("flow_garbage");
    ost_import::OspStageResult result;
    CHECK_FALSE(flow.run("this is not a cabinet", result));
    CHECK_STR_EQ(result.error, "Invalid OSP file format");
    CHECK_FALSE(flow.tree.exists("tmp"));
}

// Port of test_osp_import_rejects_corrupt_embedded_ost_and_cleans_temp_files: the embedded OST is not well-formed, nothing is staged, the
// extraction folder is gone.
TEST(OspFlow, ACorruptEmbeddedOstStagesNothing)
{
    Flow flow("flow_corrupt");
    const std::string package =
        ost_osptest::make_osp({{"Project.ost", "<XML_ROOT>"}, {"TempImages!.tmp\\sheet.pdf", "x"}});
    ost_import::OspStageResult result;
    CHECK_FALSE(flow.run(package, result));
    CHECK(starts_with(result.error, "embedded OST data is corrupt: no element found: line "));
    CHECK_FALSE(flow.tree.exists("images"));
    CHECK(flow.tree.entries_in("tmp").empty());
}

// Port of test_osp_import_extraction_failure_fails_import_and_cleans_temp_files / IMP-13: a damaged block fails the whole extraction, nothing
// is imported and nothing is staged.
TEST(OspFlow, ADamagedArchiveFailsTheExtraction)
{
    Flow flow("flow_damaged");
    std::vector<ost_itest::CabMember> members;
    ost_itest::CabMember ost;
    ost.name = "Project.ost";
    ost.content = ost_osptest::page_xml({{"ImagePath", "C:\\plans\\sheet.pdf"}});
    members.push_back(ost);
    ost_itest::CabMember image;
    image.name = "TempImages!.tmp\\sheet.pdf";
    image.content = ost_itest::lcg_text(7, 5000);
    members.push_back(image);
    ost_itest::CabBuildOptions options;
    options.mszip = true;
    options.checksums = true;
    options.corrupt_block = 0;
    options.corrupt_mode = ost_itest::CabBuildOptions::FlipPayloadByte;
    const std::string package = ost_itest::build_cab(members, options);
    LogCapture log;
    ost_import::OspStageResult result;
    CHECK_FALSE(flow.run(package, result));
    CHECK_STR_EQ(result.error, "The OSP archive could not be extracted");
    CHECK_FALSE(flow.tree.exists("images"));
    CHECK(flow.tree.entries_in("tmp").empty());
}

// IMP-M02 end to end (item test plan): the second of two image copies fails: the result is a failure, nothing is parsed (no rows to
// import), the first image stays staged (as on Windows), the extraction folder is gone.
TEST(OspFlow, AFailingCopyAbortsTheImport)
{
    Flow flow("flow_diskfull");
    CountingFs fs;
    fs.fail_on_copy = 2;
    const std::string xml = ost_osptest::page_xml({{"ImagePath", "C:\\plans\\one.pdf"}, {"OverlayImagePath", "C:\\plans\\two.pdf"}});
    const std::string package = ost_osptest::make_osp(
        {{"Project.ost", xml}, {"TempImages!.tmp\\one.pdf", "one"}, {"TempImages!.tmp\\two.pdf", "two"}});
    ost_import::OspStageResult result;
    CHECK_FALSE(flow.run(package, result, &fs));
    CHECK_EQ(fs.copies, 2);
    CHECK(result.error.find("TempImages!.tmp\\two.pdf") != std::string::npos);
    CHECK(result.raw.empty()); // nothing to import
    CHECK(flow.tree.entries_in("tmp").empty());
}

// A folder that cannot be created for the staged images (a plain file where the project folder should be) aborts too.
TEST(OspFlow, AnUncreatableImageFolderAbortsTheImport)
{
    Flow flow("flow_nofolder");
    flow.tree.write("images/Project", "a file where the folder should be");
    const std::string package = ost_osptest::make_osp(
        {{"Project.ost", ost_osptest::page_xml({{"ImagePath", "C:\\plans\\sheet.pdf"}})}, {"TempImages!.tmp\\sheet.pdf", "x"}});
    ost_import::OspStageResult result;
    CHECK_FALSE(flow.run(package, result));
    CHECK_FALSE(result.error.empty());
    CHECK(result.raw.empty());
    CHECK(flow.tree.entries_in("tmp").empty());
}

// An embedded OST without a Bid element: Windows stages the images first (the image step runs before the importer parses the Bid) and then
// the import fails with "No Bid element found in OST file".
TEST(OspFlow, AnOstWithoutABidFailsAfterTheImageStep)
{
    Flow flow("flow_nobid");
    const std::string package = ost_osptest::make_osp(
        {{"Project.ost", "<XML_ROOT><BidPage ImagePath=\"C:\\plans\\sheet.pdf\"/></XML_ROOT>"}, {"TempImages!.tmp\\sheet.pdf", "x"}});
    ost_import::OspStageResult result;
    CHECK_FALSE(flow.run(package, result));
    CHECK_STR_EQ(result.error, "No Bid element found in OST file");
    CHECK_STR_EQ(flow.tree.read("images/Project/sheet.pdf"), "x");
    CHECK(flow.tree.entries_in("tmp").empty());
}

// The package name is the stem of the one top-level .ost (Windows ost_path.stem), whatever its case.
TEST(OspFlow, ThePackageNameIsTheOstStem)
{
    Flow flow("flow_stem");
    const std::string package = ost_osptest::make_osp({{"My.Plan.OST", ost_osptest::page_xml({{"ImagePath", "C:\\x\\sheet.pdf"}})},
                                                       {"TempImages!.tmp\\sheet.pdf", "x"}});
    ost_import::OspStageResult result;
    REQUIRE(flow.run(package, result));
    CHECK_STR_EQ(result.package_name, "My.Plan");
    CHECK_STR_EQ(flow.tree.read("images/My.Plan/sheet.pdf"), "x");
}
