// IMP-13: the CAB reader must extract byte-identically or fail; it must never return shifted or partial bytes.
//
// Windows: cpp_extensions/src/cab/cab_compressor.cpp extracts through the Windows cabinet library (FDICopy). Any decode or
// corruption error makes extract_cab return false, the import then fails with 'the OSP archive could not be extracted' and
// nothing is imported (tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_extraction_failure_fails_import_and_cleans_temp_files,
// ::test_extraction_failure_message_names_the_cause_and_archive). The legacy OSP fixtures are uncompressed with CFDATA
// checksum 0 (tests/helpers/import_workflow.py::_write_legacy_ansi_cab, used by tests/integration/native/test_osp_cab.py).
// This file only uses the original extract_cab_file API (failure = empty vector).
#include "TestHarness.hpp"

#include "CabTestSupport.hpp"
#include "Import/CabExtractor.hpp"

#include <string>
#include <vector>

using ost_itest::CabBuildOptions;
using ost_itest::CabMember;

namespace
{
    std::string extract(const std::string &cab, const std::string &member)
    {
        const std::vector<char> bytes = ost_import::extract_cab_file(cab.data(), cab.size(), member);
        return std::string(bytes.begin(), bytes.end());
    }

    std::string ost_xml()
    {
        return "<XML_ROOT><Bid UID=\"1\" JobName=\"Imported\"/>" + ost_itest::lcg_text(7, 40000) + "</XML_ROOT>";
    }

    std::vector<CabMember> ost_and_pdf()
    {
        return {{"Project.ost", ost_xml()}, {"TempImages!.tmp\\plan.pdf", "%PDF-1.4\n" + ost_itest::noise(3, 5000)}};
    }

    const char *kPdf = "TempImages!.tmp/plan.pdf"; // the reader reports backslashes as '/'
}

// Port of tests/integration/native/test_osp_cab.py::test_osp_imports_legacy_ansi_members_from_unicode_archive_path (shape of
// the archive): uncompressed, one block, CFDATA checksum 0. Both members must come out byte-identical.
TEST(ImportCab, Imp13_LegacyStoredCabWithZeroChecksumStillExtracts)
{
    const std::vector<CabMember> members = {{"Project.ost", "<XML_ROOT />"}, {"TempImages!.tmp\\A0-0.0.pdf", "%PDF-1.4 fixture"}};
    const std::string cab = ost_itest::build_cab(members, CabBuildOptions());
    CHECK_STR_EQ(extract(cab, "Project.ost"), std::string("<XML_ROOT />"));
    CHECK_STR_EQ(extract(cab, "TempImages!.tmp/A0-0.0.pdf"), std::string("%PDF-1.4 fixture"));
}

// IMP-13 test plan: a two-block MSZIP CAB (block 2 uses block 1 as its dictionary) with an OST then a PDF extracts both
// byte-identically, with and without CFDATA checksums.
TEST(ImportCab, Imp13_TwoBlockMszipCabExtractsBothMembersByteIdentically)
{
    for (bool checksums : {false, true})
    {
        SCOPED_CONTEXT(std::string("checksums ") + (checksums ? "on" : "off"));
        CabBuildOptions options;
        options.mszip = true;
        options.checksums = checksums;
        const std::vector<CabMember> members = ost_and_pdf();
        const std::string cab = ost_itest::build_cab(members, options);
        CHECK(extract(cab, "Project.ost") == members[0].content);
        CHECK(extract(cab, kPdf) == members[1].content);
    }
}

TEST(ImportCab, Imp13_ManySmallBlocksAndStoredBlocksExtract)
{
    for (bool mszip : {false, true})
    {
        for (bool checksums : {false, true})
        {
            SCOPED_CONTEXT(std::string(mszip ? "mszip" : "stored") + (checksums ? " + checksums" : ""));
            CabBuildOptions options;
            options.mszip = mszip;
            options.checksums = checksums;
            options.block_size = 1000;
            const std::vector<CabMember> members = {{"a.bin", ost_itest::noise(1, 2500)}, {"b.txt", ost_itest::lcg_text(5, 3333)}};
            const std::string cab = ost_itest::build_cab(members, options);
            CHECK(extract(cab, "a.bin") == members[0].content);
            CHECK(extract(cab, "b.txt") == members[1].content);
        }
    }
}

// The reference: a cabinet written by Microsoft's own makecab.exe (MSZIP, two CFDATA blocks, real non-zero checksums
// 0x18594755 and 0x9b158db0). plans.ost = lcg_text(1, 40000), sheet.pdf = lcg_text(2, 2000) (scratchpad make_cab_fixture.py).
TEST(ImportCab, Imp13_CabinetWrittenByMakecabExtractsWithItsRealChecksums)
{
    std::string cab;
    REQUIRE(ost_test::read_file(ost_test::fixture_path("makecab_mszip.cab"), cab));
    CHECK(extract(cab, "plans.ost") == ost_itest::lcg_text(1, 40000));
    CHECK(extract(cab, "sheet.pdf") == ost_itest::lcg_text(2, 2000));
    CHECK(extract(cab, "SHEET.PDF") == ost_itest::lcg_text(2, 2000)); // member names compare case-insensitively
}

TEST(ImportCab, Imp13_MakecabCabinetWithOneFlippedByteFailsNeverReturningShiftedBytes)
{
    std::string cab;
    REQUIRE(ost_test::read_file(ost_test::fixture_path("makecab_mszip.cab"), cab));
    // flip one byte in the middle of the first CFDATA payload (offset 96 + 8 + 20000 is inside block 1)
    std::string damaged = cab;
    damaged[96 + 8 + 20000] ^= 0x01;
    CHECK(extract(damaged, "plans.ost").empty());
    CHECK(extract(damaged, "sheet.pdf").empty());
    // and one in the second block
    damaged = cab;
    damaged[cab.size() - 100] ^= 0x01;
    CHECK(extract(damaged, "plans.ost").empty());
    CHECK(extract(damaged, "sheet.pdf").empty());
}

// IMP-13 test plan: corrupt block 1's deflate bytes (checksum 0): extract must fail (empty), never return shifted bytes. The
// old reader skipped the bad block and returned the bytes of block 2 in its place.
TEST(ImportCab, Imp13_CorruptDeflateDataFailsInsteadOfShiftingTheFollowingBytes)
{
    CabBuildOptions options;
    options.mszip = true;
    options.corrupt_block = 0;
    options.corrupt_mode = CabBuildOptions::DeflateBytes;
    const std::vector<CabMember> members = ost_and_pdf();
    const std::string cab = ost_itest::build_cab(members, options);
    CHECK(extract(cab, "Project.ost").empty());
    CHECK(extract(cab, kPdf).empty());
}

// Layout where dropping the damaged block 0 leaves a buffer that is still long enough for the second member: the old reader
// then returned 2000 WRONG bytes (the bytes that follow the member) instead of failing.
TEST(ImportCab, Imp13_ASkippedBadBlockUsedToHandOutTheWrongBytesOfALaterMember)
{
    CabBuildOptions options;
    options.mszip = true;
    options.block_size = 1000;
    options.corrupt_block = 0;
    options.corrupt_mode = CabBuildOptions::DeflateBytes;
    // incompressible content: the blocks do not refer back into each other, so the later blocks still inflate on their own
    const std::vector<CabMember> members = {{"a.bin", ost_itest::noise(1, 1000)},
                                            {"b.bin", ost_itest::noise(2, 2000)},
                                            {"c.bin", ost_itest::noise(3, 1000)}};
    const std::string cab = ost_itest::build_cab(members, options);
    CHECK(extract(cab, "a.bin").empty());
    CHECK(extract(cab, "b.bin").empty());
    CHECK(extract(cab, "c.bin").empty());
    // the same archive undamaged extracts all three
    options.corrupt_block = -1;
    const std::string good = ost_itest::build_cab(members, options);
    for (const CabMember &m : members)
        CHECK(extract(good, m.name) == m.content);
}

// IMP-13 test plan: flip one byte of a CFDATA payload with a non-zero checksum: the checksum is verified and the extraction fails.
TEST(ImportCab, Imp13_AFlippedPayloadByteIsCaughtByTheCfdataChecksum)
{
    for (bool mszip : {false, true})
    {
        SCOPED_CONTEXT(mszip ? "mszip" : "stored");
        CabBuildOptions options;
        options.mszip = mszip;
        options.checksums = true;
        options.block_size = 4000;
        options.corrupt_block = 1;
        options.corrupt_mode = CabBuildOptions::FlipPayloadByte;
        const std::vector<CabMember> members = {{"a.bin", ost_itest::noise(1, 6000)}, {"b.bin", ost_itest::noise(2, 6000)}};
        const std::string cab = ost_itest::build_cab(members, options);
        CHECK(extract(cab, "a.bin").empty()); // even the member that lies in the intact block 0 fails (all or nothing)
        CHECK(extract(cab, "b.bin").empty());
    }
}

// A block that lost its "CK" signature is corrupt (the old reader skipped it).
TEST(ImportCab, Imp13_AnMszipBlockWithoutTheCkSignatureFails)
{
    CabBuildOptions options;
    options.mszip = true;
    const std::vector<CabMember> members = ost_and_pdf();
    std::string cab = ost_itest::build_cab(members, options);
    // first CFDATA block starts at data offset; header (36) + folder (8) + file table
    size_t file_table = 0;
    for (const CabMember &m : members)
        file_table += 16 + m.name.size() + 1;
    const size_t first_block = 36 + 8 + file_table;
    REQUIRE(cab[first_block + 8] == 'C' && cab[first_block + 9] == 'K');
    cab[first_block + 8] = 'X';
    CHECK(extract(cab, "Project.ost").empty());
    CHECK(extract(cab, kPdf).empty());
}

// A stored folder cut inside its last block fails for every member (FDICopy fails the whole cabinet), instead of "just
// breaking the loop" and handing out whatever bytes arrived.
TEST(ImportCab, Imp13_ATruncatedCabinetFailsForEveryMember)
{
    CabBuildOptions options;
    options.block_size = 1000;
    const std::vector<CabMember> members = {{"first.bin", ost_itest::noise(1, 800)}, {"second.bin", ost_itest::noise(2, 1800)}};
    const std::string cab = ost_itest::build_cab(members, options);
    CHECK(extract(cab, "first.bin") == members[0].content); // intact cabinet
    const std::string truncated = cab.substr(0, cab.size() - 300);
    CHECK(extract(truncated, "second.bin").empty());
    CHECK(extract(truncated, "first.bin").empty());
}

// "There is no check that produced bytes equal cbUncomp": a block whose header declares more bytes than it holds fails.
TEST(ImportCab, Imp13_ABlockWhoseSizeDiffersFromItsHeaderFails)
{
    for (bool mszip : {false, true})
    {
        SCOPED_CONTEXT(mszip ? "mszip" : "stored");
        CabBuildOptions options;
        options.mszip = mszip;
        const std::vector<CabMember> members = {{"a.bin", ost_itest::lcg_text(1, 3000)}};
        std::string cab = ost_itest::build_cab(members, options);
        const size_t first_block = 36 + 8 + (16 + members[0].name.size() + 1);
        cab[first_block + 6] = static_cast<char>(cab[first_block + 6] + 1); // cbUncomp + 1
        CHECK(extract(cab, "a.bin").empty());
    }
}

// MSZIP data that stops early (a truncated deflate stream) fails.
TEST(ImportCab, Imp13_ATruncatedDeflateStreamFails)
{
    CabBuildOptions options;
    options.mszip = true;
    const std::vector<CabMember> members = {{"a.bin", ost_itest::lcg_text(1, 5000)}};
    std::string cab = ost_itest::build_cab(members, options);
    const size_t first_block = 36 + 8 + (16 + members[0].name.size() + 1);
    // shrink cbData by 40 bytes and cut them from the file: a deflate stream without its tail
    const uint16_t cb_data = static_cast<uint8_t>(cab[first_block + 4]) | (static_cast<uint8_t>(cab[first_block + 5]) << 8);
    REQUIRE(cb_data > 100);
    const uint16_t shorter = static_cast<uint16_t>(cb_data - 40);
    cab[first_block + 4] = static_cast<char>(shorter & 0xFF);
    cab[first_block + 5] = static_cast<char>(shorter >> 8);
    cab.erase(first_block + 8 + shorter, 40);
    CHECK(extract(cab, "a.bin").empty());
}

// Windows' own legacy cabinets keep working when a member sits behind a damaged one in ANOTHER cabinet: only the folder that is
// read matters, but within the folder nothing is skipped (covered above). Unknown members and non-cabinets are plain failures.
TEST(ImportCab, Imp13_UnknownMembersAndNonCabinetsReturnNothing)
{
    const std::string cab = ost_itest::build_cab({{"a.bin", "abc"}}, CabBuildOptions());
    CHECK(extract(cab, "b.bin").empty());
    CHECK(extract("not a cabinet at all, definitely not one", "a.bin").empty());
    CHECK(extract("", "a.bin").empty());
}
