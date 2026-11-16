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
