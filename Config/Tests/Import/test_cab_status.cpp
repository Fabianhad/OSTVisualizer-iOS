// IMP-13: extract_cab_member reports WHY an extraction failed, so the OSP import can fail with an extraction error (Windows:
// "Cannot import OSP <name>: the OSP archive could not be extracted") instead of treating an empty result as success, and so
// that an empty member is not confused with a failure.
//
// Windows: tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_extraction_failure_fails_import_and_cleans_temp_files
// and ::test_extraction_failure_message_names_the_cause_and_archive (extract_cab false -> the import fails and nothing is imported).
#include "TestHarness.hpp"

#include "CabTestSupport.hpp"
#include "Import/CabExtractor.hpp"

using ost_import::CabStatus;
using ost_itest::CabBuildOptions;
using ost_itest::CabMember;

namespace
{
    ost_import::CabExtractResult member(const std::string &cab, const std::string &name)
    {
        return ost_import::extract_cab_member(cab.data(), cab.size(), name);
    }
}

TEST(ImportCabStatus, Imp13_AGoodMemberIsOkAndKeepsItsBytes)
{
    CabBuildOptions options;
    options.mszip = true;
    options.checksums = true;
    const std::vector<CabMember> members = {{"Project.ost", "<XML_ROOT />"}, {"TempImages!.tmp\\p.pdf", "%PDF-1.4"}};
    const std::string cab = ost_itest::build_cab(members, options);
    const ost_import::CabExtractResult result = member(cab, "Project.ost");
    CHECK(result.status == CabStatus::Ok);
    CHECK_STR_EQ(std::string(result.data.begin(), result.data.end()), std::string("<XML_ROOT />"));
    CHECK(result.detail.empty());
}

// A zero-length member is a success with no data; the extract_cab_file wrapper cannot tell it from a failure, this can.
TEST(ImportCabStatus, Imp13_AnEmptyMemberIsOkNotAFailure)
{
    const std::string cab = ost_itest::build_cab({{"empty.txt", ""}, {"full.txt", "data"}}, CabBuildOptions());
    const ost_import::CabExtractResult result = member(cab, "empty.txt");
    CHECK(result.status == CabStatus::Ok);
    CHECK(result.data.empty());
    CHECK(member(cab, "full.txt").status == CabStatus::Ok);
}

TEST(ImportCabStatus, Imp13_FailuresAreReportedWithTheirKind)
{
    const std::string cab = ost_itest::build_cab({{"a.bin", "abc"}}, CabBuildOptions());
    CHECK(member(cab, "missing.bin").status == CabStatus::NotFound);
    CHECK(member("this is not a cabinet", "a.bin").status == CabStatus::NotACabinet);
    CHECK(member("", "a.bin").status == CabStatus::NotACabinet);
}

TEST(ImportCabStatus, Imp13_CorruptionIsCorruptAndSaysWhich)
{
    CabBuildOptions options;
    options.mszip = true;
    options.checksums = true;
    options.block_size = 4000;
    options.corrupt_block = 1;
    options.corrupt_mode = CabBuildOptions::FlipPayloadByte;
    const std::vector<CabMember> members = {{"a.bin", ost_itest::noise(1, 6000)}};
    const std::string cab = ost_itest::build_cab(members, options);
    const ost_import::CabExtractResult result = member(cab, "a.bin");
    CHECK(result.status == CabStatus::Corrupt);
    CHECK(result.data.empty());
    CHECK(result.detail.find("block 2 of 2") != std::string::npos);
    CHECK(result.detail.find("checksum") != std::string::npos);

    options.checksums = false;
    options.corrupt_mode = CabBuildOptions::DeflateBytes;
    options.corrupt_block = 0;
    const std::string no_checksum = ost_itest::build_cab(members, options);
    const ost_import::CabExtractResult inflate = member(no_checksum, "a.bin");
    CHECK(inflate.status == CabStatus::Corrupt);
    CHECK(inflate.detail.find("block 1 of 2") != std::string::npos);
}

// Compression types other than stored and MSZIP are not supported (IMP-17 is a separate decision): they fail loudly now,
// where the old reader broke out of the loop and returned a short buffer.
TEST(ImportCabStatus, Imp13_AnUnsupportedCompressionTypeIsAFailureNotAPartialResult)
{
    std::string cab = ost_itest::build_cab({{"a.bin", "abc"}}, CabBuildOptions());
    // folder entry sits right after the 36 byte header: typeCompress at +6; 3 = LZX
    cab[36 + 6] = 3;
    const ost_import::CabExtractResult result = member(cab, "a.bin");
    CHECK(result.status == CabStatus::Corrupt);
    CHECK(result.detail.find("unsupported compression type") != std::string::npos);
}

TEST(ImportCabStatus, Imp13_AMemberPastTheEndOfItsFolderIsCorrupt)
{
    std::string cab = ost_itest::build_cab({{"a.bin", "abcdef"}}, CabBuildOptions());
    // cbFile of the member (first field of the CFFILE entry at 36 + 8) grows beyond the 6 bytes the folder holds
    cab[36 + 8] = 40;
    CHECK(member(cab, "a.bin").status == CabStatus::Corrupt);
    // and a size that would wrap around 32 bits (offset 0xFFFFFFFF.. + size) must not be mistaken for in-range
    std::string wrap = ost_itest::build_cab({{"a.bin", "abcdef"}}, CabBuildOptions());
    wrap[36 + 8 + 4] = static_cast<char>(0xFF);
    wrap[36 + 8 + 5] = static_cast<char>(0xFF);
    wrap[36 + 8 + 6] = static_cast<char>(0xFF);
    wrap[36 + 8 + 7] = static_cast<char>(0xFF); // uoffFolderStart = 0xFFFFFFFF
    wrap[36 + 8] = 7;                          // cbFile = 7 -> 0xFFFFFFFF + 7 wraps to 6 in 32-bit arithmetic
    CHECK(member(wrap, "a.bin").status == CabStatus::Corrupt);
}

// ---------------------------------------------------------------------------------------------------------------
// verify_cab: the whole cabinet must be sound (Windows FDICopy extracts every member in one pass)
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    // Written by Microsoft's makecab.exe with ".New Folder" between the two files: folder 1 = plans.ost (two CFDATA blocks,
    // data at 104), folder 2 = sheet.pdf (one block, data at 25601); plans.ost = lcg_text(1, 40000), sheet.pdf = lcg_text(2, 2000).
    // Verified with expand.exe -F:* (the Windows cabinet library, FDI): the intact cabinet extracts both files; a flipped
    // byte in folder 1 fails with no file written, and a flipped byte in folder 2 fails although plans.ost (folder 1) had
    // already come out intact (1 file written, exit code 1).
    std::string two_folder_cab()
    {
        std::string cab;
        if (!ost_test::read_file(ost_test::fixture_path("makecab_two_folders.cab"), cab))
            FAIL_TEST("cannot read makecab_two_folders.cab");
        return cab;
    }
}

TEST(ImportCabStatus, Imp13_VerifyAcceptsAnIntactCabinetOfEveryShape)
{
    // makecab, two folders
    std::string cab = two_folder_cab();
    ost_import::CabExtractResult result = ost_import::verify_cab(cab.data(), cab.size());
    CHECK(result.status == CabStatus::Ok);
    CHECK(result.data.empty());
    const ost_import::CabExtractResult plans = member(cab, "plans.ost");
    const ost_import::CabExtractResult sheet = member(cab, "sheet.pdf");
    CHECK(plans.status == CabStatus::Ok);
    CHECK(sheet.status == CabStatus::Ok);
    CHECK(std::string(plans.data.begin(), plans.data.end()) == ost_itest::lcg_text(1, 40000));
    CHECK(std::string(sheet.data.begin(), sheet.data.end()) == ost_itest::lcg_text(2, 2000));
    // makecab, one folder, real checksums
    REQUIRE(ost_test::read_file(ost_test::fixture_path("makecab_mszip.cab"), cab));
    CHECK(ost_import::verify_cab(cab.data(), cab.size()).status == CabStatus::Ok);
    // stored, checksum 0 (the legacy shape), MSZIP with and without checksums, several blocks, an empty member
    for (bool mszip : {false, true})
    {
        for (bool checksums : {false, true})
        {
            SCOPED_CONTEXT(std::string(mszip ? "mszip" : "stored") + (checksums ? " + checksums" : ""));
            CabBuildOptions options;
            options.mszip = mszip;
            options.checksums = checksums;
            options.block_size = 1000;
            const std::string built = ost_itest::build_cab({{"a.bin", ost_itest::noise(1, 2500)}, {"empty.txt", ""}}, options);
            CHECK(ost_import::verify_cab(built.data(), built.size()).status == CabStatus::Ok);
        }
    }
}

// The reason verify_cab exists: a member that is intact is not enough, any corruption in the cabinet fails the Windows import.
TEST(ImportCabStatus, Imp13_VerifyFailsOnCorruptionInAFolderThatHoldsAnUnrelatedMember)
{
    const std::string good = two_folder_cab();
    std::string folder2 = good;
    folder2[25601 + 8 + 5] ^= static_cast<char>(0xFF);
    // plans.ost lives in folder 1 and still extracts on its own ...
    CHECK(member(folder2, "plans.ost").status == CabStatus::Ok);
    CHECK(member(folder2, "sheet.pdf").status == CabStatus::Corrupt);
    // ... but the cabinet as a whole is damaged, like FDICopy says
    ost_import::CabExtractResult result = ost_import::verify_cab(folder2.data(), folder2.size());
    CHECK(result.status == CabStatus::Corrupt);
    CHECK(result.data.empty());
    CHECK(!result.detail.empty());

    std::string folder1 = good;
    folder1[104 + 8 + 5] ^= static_cast<char>(0xFF);
    CHECK(ost_import::verify_cab(folder1.data(), folder1.size()).status == CabStatus::Corrupt);
}

TEST(ImportCabStatus, Imp13_VerifyReportsTheSameKindsAsExtract)
{
    CHECK(ost_import::verify_cab("", 0).status == CabStatus::NotACabinet);
    const std::string not_cab = "this is not a cabinet";
    CHECK(ost_import::verify_cab(not_cab.data(), not_cab.size()).status == CabStatus::NotACabinet);

    CabBuildOptions options;
    options.mszip = true;
    options.checksums = true;
    options.block_size = 4000;
    options.corrupt_block = 1;
    options.corrupt_mode = CabBuildOptions::FlipPayloadByte;
    const std::string cab = ost_itest::build_cab({{"a.bin", ost_itest::noise(1, 6000)}}, options);
    const ost_import::CabExtractResult result = ost_import::verify_cab(cab.data(), cab.size());
    CHECK(result.status == CabStatus::Corrupt);
    CHECK(result.detail.find("block 2 of 2") != std::string::npos);

    // truncated, a member past the end of its folder, an unsupported compression type
    const std::string whole = ost_itest::build_cab({{"a.bin", "abcdef"}, {"b.bin", "ghi"}}, CabBuildOptions());
    CHECK(ost_import::verify_cab(whole.data(), whole.size() - 3).status == CabStatus::Corrupt);
    std::string past_end = whole;
    past_end[36 + 8] = 40; // cbFile of the first member
    CHECK(ost_import::verify_cab(past_end.data(), past_end.size()).status == CabStatus::Corrupt);
    std::string lzx = whole;
    lzx[36 + 6] = 3;
    CHECK(ost_import::verify_cab(lzx.data(), lzx.size()).status == CabStatus::Corrupt);
}

// Checked against FDI (expand.exe -F:*): a cabinet whose header claims a different total size, or that has bytes after the
// last block, is still extracted.
TEST(ImportCabStatus, Imp13_VerifyIgnoresTheDeclaredCabinetSizeAndTrailingBytes)
{
    std::string cab = two_folder_cab();
    const std::string original = cab;
    cab += "JUNKJUNK";
    CHECK(ost_import::verify_cab(cab.data(), cab.size()).status == CabStatus::Ok);
    cab = original;
    cab[8] = static_cast<char>(cab[8] + 100); // cbCabinet
    CHECK(ost_import::verify_cab(cab.data(), cab.size()).status == CabStatus::Ok);
    CHECK(member(cab, "plans.ost").status == CabStatus::Ok);
}

// The Windows cabinet library only reads cabinets of format version 1.3 (versionMajor 1, versionMinor 3 at header offsets 25 and
// 24): expand.exe -F:* (FDI) refused every other version in a probe (minor 0, 1, 2, 4, 5, 0x13, 0x83, 255 and major 0, 2, 3, 0x81,
// 255 all fail, 1.3 extracts). The old reader never looked at the version, so a damaged version field was imported as if intact.
TEST(ImportCabStatus, Imp13_OnlyCabinetVersion1Point3IsAccepted)
{
    const std::string good = ost_itest::build_cab({{"a.bin", "abcdef"}}, CabBuildOptions());
    REQUIRE_EQ(static_cast<int>(good[24]), 3);
    REQUIRE_EQ(static_cast<int>(good[25]), 1);
    CHECK(member(good, "a.bin").status == CabStatus::Ok);
    CHECK(ost_import::verify_cab(good.data(), good.size()).status == CabStatus::Ok);
    CHECK_EQ(ost_import::list_cab(good.data(), good.size()).size(), 1u);

    const int versions[][2] = {{2, 1}, {4, 1}, {5, 1}, {0x13, 1}, {0, 1}, {0x83, 1}, {255, 1}, {3, 0}, {3, 2}, {3, 3}, {3, 0x81}, {3, 255}, {0, 0}};
    for (const auto &version : versions)
    {
        SCOPED_CONTEXT("version " + std::to_string(version[1]) + "." + std::to_string(version[0]));
        std::string cab = good;
        cab[24] = static_cast<char>(version[0]);
        cab[25] = static_cast<char>(version[1]);
        CHECK(member(cab, "a.bin").status == CabStatus::NotACabinet);
        CHECK(ost_import::verify_cab(cab.data(), cab.size()).status == CabStatus::NotACabinet);
        CHECK(ost_import::list_cab(cab.data(), cab.size()).empty());
        CHECK(ost_import::extract_cab_file(cab.data(), cab.size(), "a.bin").empty());
    }
}
