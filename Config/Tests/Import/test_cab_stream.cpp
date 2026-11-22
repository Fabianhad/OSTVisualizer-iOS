// IMP-M03: single pass CAB extraction that streams the members to a sink / to disk.
//
// Windows: cpp_extensions/src/cab/cab_compressor.cpp extract_cab drives the Windows cabinet library (FDICopy) over the
// cabinet ONCE and writes every member straight to its file; memory does not grow with the package. The iOS importer used to
// inflate the whole folder of a member into one vector again for every member (an OSP with N drawings in one folder was
// decoded N + 1 times and held in RAM each time). The decision of the user (IMP-M03): a single pass extractor that streams to
// disk. extract_cab / extract_cab_to_directory are that extractor; extract_cab_member (the old per-member extractor) stays as
// the reference the new results are compared with.
//
// What the Windows library does, measured with a native probe that drives the real FDI exactly like cab_compressor.cpp
// (members are extracted in file table order; "FDI decodes" below means the CFDATA blocks it reads and verifies):
//   - it decodes the folder from its first block up to the block that holds the last byte of the last member it extracts, and
//     no further: a flipped byte in a later block is never noticed (member of 100 bytes in a folder of 5 blocks: only block 1
//     counts, a member that ends at 20001 needs block 2, a member that ends exactly at a block boundary does not need the next
//     one). Stored and MSZIP folders behave the same.
//   - a zero-length member is created empty and decodes nothing, wherever its offset points (even beyond the folder).
//   - a table that lists the members in another order than their bytes (descending offsets), overlapping members, gaps and
//     interleaved folders extract correctly; the bytes of a member are folder[offset, offset + size).
//   - a member whose bytes run past the end of its folder fails the extraction after the bytes up to the end were written.
//   - a member of a folder that does not exist (also 0xFFFD / 0xFFFF) is skipped silently.
// Two behaviours of the Windows library that are NOT reproduced because they are accidents: with MSZIP blocks of another
// uncompressed size than 32768 (a cabinet no real writer produces) its output is garbage, and a member that runs past the end
// of a folder whose only block is shorter than 32768 gets bytes that do not exist. Here a block is decoded as the format says
// and such a member fails (the fuzz that compared this reader with the real library on thousands of cabinets lists both).
// Test inputs are built here (CabTestSupport.hpp build_cab_folders) and from the two makecab fixtures.
#include "TestHarness.hpp"

#include "CabTestSupport.hpp"
#include "ImportTestSupport.hpp"
#include "Import/CabExtractor.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using ost_import::CabEntry;
using ost_import::CabMemberAction;
using ost_import::CabStatus;
using ost_itest::CabBuildOptions;
using ost_itest::CabMember;
using ost_itest::FolderCabOptions;
using ost_itest::FolderMember;

namespace
{
    struct Delivered
    {
        std::string name;
        std::string bytes;
        size_t writes = 0;
        size_t largest_piece = 0;
        bool ended = false;
    };

    // Records what the extractor hands over.
    class Collector : public ost_import::CabMemberSink
    {
    public:
        std::vector<Delivered> members;
        std::map<std::string, CabMemberAction> actions; // by member name; default Extract
        bool fail_write = false;
        bool fail_end = false;

        CabMemberAction begin_member(const CabEntry &entry) override
        {
            members.push_back(Delivered());
            members.back().name = entry.name;
            const auto it = actions.find(entry.name);
            return it == actions.end() ? CabMemberAction::Extract : it->second;
        }
        bool write(const char *bytes, size_t length) override
        {
            Delivered &d = members.back();
            d.bytes.append(bytes, length);
            ++d.writes;
            d.largest_piece = std::max(d.largest_piece, length);
            return !fail_write;
        }
        bool end_member() override
        {
            members.back().ended = true;
            return !fail_end;
        }

        const Delivered *find(const std::string &name) const
        {
            for (const Delivered &d : members)
                if (d.name == name)
                    return &d;
            return nullptr;
        }
    };

    ost_import::CabExtractResult run(const std::string &cab, Collector &collector)
    {
        return ost_import::extract_cab(cab.data(), cab.size(), collector);
    }

    std::string legacy_member(const std::string &cab, const std::string &name, CabStatus *status = nullptr)
    {
        const ost_import::CabExtractResult result = ost_import::extract_cab_member(cab.data(), cab.size(), name);
        if (status)
            *status = result.status;
        return std::string(result.data.begin(), result.data.end());
    }

    std::string read_fixture(const char *name)
    {
        std::string bytes;
        if (!ost_test::read_file(ost_test::fixture_path(name), bytes))
            FAIL_TEST(std::string("cannot read fixture ") + name);
        return bytes;
    }

    // The bytes the file table entry `index` of a built cabinet points at (what FDI extracts for it).
    std::string expected_bytes(const std::vector<FolderMember> &members, size_t index)
    {
        std::vector<std::string> folders(8);
        std::vector<size_t> real_offset(members.size());
        for (size_t i = 0; i < members.size(); ++i)
        {
            real_offset[i] = folders[members[i].folder].size();
            folders[members[i].folder] += members[i].content;
        }
        const FolderMember &m = members[index];
        const size_t offset = m.declared_offset >= 0 ? static_cast<size_t>(m.declared_offset) : real_offset[index];
        const size_t size = m.declared_size >= 0 ? static_cast<size_t>(m.declared_size) : m.content.size();
        const std::string &folder = folders[m.folder];
        if (size == 0)
            return std::string();
        REQUIRE(offset + size <= folder.size());
        return folder.substr(offset, size);
    }

    unsigned long long blocks_before() { return ost_import::cab_blocks_decoded(); }
    unsigned long long blocks_since(unsigned long long before) { return ost_import::cab_blocks_decoded() - before; }

    // A scratch directory below the system temp dir, removed again.
    class TempDir
    {
    public:
        explicit TempDir(const std::string &name)
            : path_(std::filesystem::u8path(ost_test::temp_path("cabdir_" + ost_itest::process_token() + "_" + name)))
        {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
            std::filesystem::create_directories(path_, ec);
        }
        ~TempDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
        }
        TempDir(const TempDir &) = delete;
        TempDir &operator=(const TempDir &) = delete;
        std::string utf8() const { return path_.u8string(); }
        std::filesystem::path file(const std::string &relative_utf8) const { return path_ / std::filesystem::u8path(relative_utf8); }
        bool read(const std::string &relative_utf8, std::string &out) const
        {
            std::ifstream in(file(relative_utf8), std::ios::binary);
            if (!in)
                return false;
            out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            return true;
        }
        size_t file_count() const
        {
            size_t count = 0;
            std::error_code ec;
            for (const auto &entry : std::filesystem::recursive_directory_iterator(path_, ec))
                if (entry.is_regular_file(ec))
                    ++count;
            return count;
        }
        size_t entry_count() const
        {
            size_t count = 0;
            std::error_code ec;
            for (const auto &entry : std::filesystem::recursive_directory_iterator(path_, ec))
            {
                (void)entry;
                ++count;
            }
            return count;
        }

    private:
        std::filesystem::path path_;
    };

    std::string text(size_t seed, size_t size) { return ost_itest::lcg_text(static_cast<uint32_t>(seed), size); }
}

// ---------------------------------------------------------------------------------------------------------------
// the new extractor against the old one
// ---------------------------------------------------------------------------------------------------------------

// The reference cabinets written by makecab.exe (MSZIP; the second one has two folders): every member comes out byte-identical
// to what the per-member extractor returned, in the order of the file table.
TEST(ImportCabStream, ImpM03_TheMakecabFixturesComeOutByteIdenticallyToThePerMemberExtractor)
{
    struct Fixture
    {
        const char *file;
        std::vector<std::pair<std::string, std::string>> members;
    };
    const std::vector<Fixture> fixtures = {
        {"makecab_mszip.cab", {{"plans.ost", text(1, 40000)}, {"sheet.pdf", text(2, 2000)}}},
        {"makecab_two_folders.cab", {{"plans.ost", text(1, 40000)}, {"sheet.pdf", text(2, 2000)}}},
    };
    for (const Fixture &fixture : fixtures)
    {
        SCOPED_CONTEXT(fixture.file);
        const std::string cab = read_fixture(fixture.file);
        Collector collector;
        const ost_import::CabExtractResult result = run(cab, collector);
        CHECK(result.status == CabStatus::Ok);
        REQUIRE_EQ(collector.members.size(), fixture.members.size());
        const std::vector<CabEntry> entries = ost_import::list_cab(cab.data(), cab.size());
        REQUIRE_EQ(entries.size(), fixture.members.size());
        for (size_t i = 0; i < fixture.members.size(); ++i)
        {
            SCOPED_CONTEXT("member " + fixture.members[i].first);
            CHECK_STR_EQ(collector.members[i].name, entries[i].name);
            CHECK_STR_EQ(collector.members[i].name, fixture.members[i].first);
            CHECK(collector.members[i].ended);
            CHECK(collector.members[i].bytes == fixture.members[i].second);
            CHECK(collector.members[i].bytes == legacy_member(cab, collector.members[i].name));
        }
    }
}

// The same for generated cabinets of every shape the earlier tests use: stored / MSZIP, with and without checksums, one block
// or many, empty members.
TEST(ImportCabStream, ImpM03_GeneratedCabinetsComeOutByteIdenticallyToThePerMemberExtractor)
{
    for (bool mszip : {false, true})
    {
        for (bool checksums : {false, true})
        {
            for (size_t block_size : {size_t(1000), size_t(32768)})
            {
                SCOPED_CONTEXT(std::string(mszip ? "mszip" : "stored") + (checksums ? " + checksums" : "") + ", block " + std::to_string(block_size));
                CabBuildOptions options;
                options.mszip = mszip;
                options.checksums = checksums;
                options.block_size = block_size;
                const std::vector<CabMember> members = {{"Project.ost", text(7, 40000)},
                                                        {"TempImages!.tmp\\a.pdf", "%PDF-1.4\n" + ost_itest::noise(3, 5000)},
                                                        {"empty.txt", ""},
                                                        {"TempImages!.tmp\\b.pdf", ost_itest::noise(4, 70001)}};
                const std::string cab = ost_itest::build_cab(members, options);
                Collector collector;
                CHECK(run(cab, collector).status == CabStatus::Ok);
                REQUIRE_EQ(collector.members.size(), members.size());
                for (size_t i = 0; i < members.size(); ++i)
                {
                    SCOPED_CONTEXT("member " + members[i].name);
                    CHECK(collector.members[i].ended);
                    CHECK(collector.members[i].bytes == members[i].content);
                    std::string normalised = members[i].name;
                    for (char &c : normalised)
                        if (c == '\\')
                            c = '/';
                    CHECK_STR_EQ(collector.members[i].name, normalised);
                    CabStatus status = CabStatus::NotACabinet;
                    const std::string old = legacy_member(cab, normalised, &status);
                    CHECK(status == CabStatus::Ok);
                    CHECK(old == collector.members[i].bytes);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// one pass, bounded memory
// ---------------------------------------------------------------------------------------------------------------

// Port of the IMP-M03 test plan: a multi-block MSZIP CAB with an OST and two PDFs returns all three byte-identically from ONE
// decompression pass (counted with cab_blocks_decoded), matching the per-member extractor, which decodes the folder again for
// every member.
TEST(ImportCabStream, ImpM03_AFolderIsDecodedOnceNotOncePerMember)
{
    CabBuildOptions options;
    options.mszip = true;
    options.checksums = true;
    options.block_size = 20000;
    const std::vector<CabMember> members = {{"Project.ost", text(7, 40000)},
                                            {"TempImages!.tmp\\a.pdf", "%PDF-1.4\n" + ost_itest::noise(3, 5000)},
                                            {"TempImages!.tmp\\b.pdf", "%PDF-1.4\n" + ost_itest::noise(4, 30000)}};
    const std::string cab = ost_itest::build_cab(members, options);
    const size_t total = members[0].content.size() + members[1].content.size() + members[2].content.size();
    const unsigned long long folder_blocks = (total + options.block_size - 1) / options.block_size;
    REQUIRE_EQ(folder_blocks, 4ull);

    unsigned long long before = blocks_before();
    Collector collector;
    CHECK(run(cab, collector).status == CabStatus::Ok);
    CHECK_EQ(blocks_since(before), folder_blocks); // every block decoded exactly once
    REQUIRE_EQ(collector.members.size(), 3u);
    for (size_t i = 0; i < 3; ++i)
        CHECK(collector.members[i].bytes == members[i].content);

    // what the per-member extractor costs for the same three members (the behaviour this item removes)
    before = blocks_before();
    for (const Delivered &d : collector.members)
        CHECK(legacy_member(cab, d.name) == d.bytes);
    CHECK_EQ(blocks_since(before), 3 * folder_blocks);
}

// The two-folder makecab cabinet: three CFDATA blocks in all (plans.ost 2 + sheet.pdf 1), each decoded once; verify_cab is the
// same single pass.
TEST(ImportCabStream, ImpM03_EveryBlockOfARealCabinetIsDecodedExactlyOnce)
{
    const std::string cab = read_fixture("makecab_two_folders.cab");
    unsigned long long before = blocks_before();
    Collector collector;
    CHECK(run(cab, collector).status == CabStatus::Ok);
    CHECK_EQ(blocks_since(before), 3ull);

    before = blocks_before();
    CHECK(ost_import::verify_cab(cab.data(), cab.size()).status == CabStatus::Ok);
    CHECK_EQ(blocks_since(before), 3ull);

    const std::string one_folder = read_fixture("makecab_mszip.cab");
    before = blocks_before();
    Collector second;
    CHECK(run(one_folder, second).status == CabStatus::Ok);
    CHECK_EQ(blocks_since(before), 2ull); // 42000 bytes in all: sheet.pdf starts inside the second block
}

// Memory does not grow with the package: a member comes out in pieces of at most one CFDATA block, only the current block is
// held. (The old extractor returned a member as ONE vector after inflating the whole folder.)
TEST(ImportCabStream, ImpM03_MembersAreHandedOverInPiecesOfAtMostOneBlock)
{
    for (bool mszip : {false, true})
    {
        SCOPED_CONTEXT(mszip ? "mszip" : "stored");
        CabBuildOptions options;
        options.mszip = mszip;
        options.checksums = true;
        options.block_size = 32768;
        const std::vector<CabMember> members = {{"big.pdf", ost_itest::noise(9, 300000)}, {"small.txt", "tail"}};
        const std::string cab = ost_itest::build_cab(members, options);
        Collector collector;
        CHECK(run(cab, collector).status == CabStatus::Ok);
        REQUIRE_EQ(collector.members.size(), 2u);
        CHECK(collector.members[0].bytes == members[0].content);
        CHECK(collector.members[0].writes >= 9u);
        CHECK(collector.members[0].largest_piece <= 32768u);
        CHECK(collector.members[1].bytes == members[1].content);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// what the Windows library decodes, and what it does with odd file tables
// ---------------------------------------------------------------------------------------------------------------

// Measured with the real FDI: the blocks up to the one holding the last byte of the last extracted member are decoded (and
// verified), no later ones. A cabinet of 5 blocks of 20000 bytes, one member of the given size at offset 0.
TEST(ImportCabStream, ImpM03_OnlyTheBlocksTheMembersNeedAreDecodedAndChecked)
{
    struct Case
    {
        int64_t size;
        unsigned long long blocks; // decoded
    };
    const Case cases[] = {{0, 0}, {1, 1}, {100, 1}, {19999, 1}, {20000, 1}, {20001, 2}, {40000, 2}, {40001, 3}, {99999, 5}, {100000, 5}};
    for (bool mszip : {false, true})
    {
        for (const Case &c : cases)
        {
            SCOPED_CONTEXT(std::string(mszip ? "mszip" : "stored") + " member size " + std::to_string(c.size));
            FolderCabOptions options;
            options.mszip = {mszip};
            options.block_size = 20000;
            FolderMember member{"Project.ost", ost_itest::noise(5, 100000)};
            member.declared_size = c.size;
            const ost_itest::FolderCab built = ost_itest::build_cab_folders({member}, options);
            REQUIRE_EQ(built.block_offsets[0].size(), 5u);

            const unsigned long long before = blocks_before();
            Collector collector;
            CHECK(run(built.bytes, collector).status == CabStatus::Ok);
            CHECK_EQ(blocks_since(before), c.blocks);

            for (int victim = 0; victim < 5; ++victim)
            {
                SCOPED_CONTEXT("flipped byte in block " + std::to_string(victim + 1));
                FolderCabOptions damaged = options;
                damaged.corrupt_folder = 0;
                damaged.corrupt_block = victim;
                const ost_itest::FolderCab bad = ost_itest::build_cab_folders({member}, damaged);
                Collector second;
                const bool needed = static_cast<unsigned long long>(victim) < c.blocks;
                const ost_import::CabExtractResult result = run(bad.bytes, second);
                CHECK(result.status == (needed ? CabStatus::Corrupt : CabStatus::Ok));
                const ost_import::CabExtractResult verified = ost_import::verify_cab(bad.bytes.data(), bad.bytes.size());
                CHECK(verified.status == (needed ? CabStatus::Corrupt : CabStatus::Ok));
            }
        }
    }
}

// A member that starts behind corrupt blocks needs every block before it (the decoder has to run through them).
TEST(ImportCabStream, ImpM03_ABlockBeforeAMembersFirstByteIsStillCheckedLikeWindowsDoes)
{
    for (bool mszip : {false, true})
    {
        for (int victim = 0; victim < 5; ++victim)
        {
            SCOPED_CONTEXT(std::string(mszip ? "mszip" : "stored") + ", flipped byte in block " + std::to_string(victim + 1));
            FolderCabOptions options;
            options.mszip = {mszip};
            options.block_size = 20000;
            options.corrupt_folder = 0;
            options.corrupt_block = victim;
            FolderMember member{"late.bin", ost_itest::noise(6, 100000)};
            member.declared_offset = 60000; // block 4
            member.declared_size = 100;
            const ost_itest::FolderCab built = ost_itest::build_cab_folders({member}, options);
            Collector collector;
            const ost_import::CabExtractResult result = run(built.bytes, collector);
            CHECK(result.status == (victim <= 3 ? CabStatus::Corrupt : CabStatus::Ok)); // blocks 1..4 are decoded, block 5 is not
        }
    }
}

// Table order, gaps, overlaps and interleaved folders (all extracted correctly by FDI): every member equals folder[offset,
// offset + size), and the new extractor agrees with the per-member extractor on every one of them.
TEST(ImportCabStream, ImpM03_AnyFileTableOrderGapOrOverlapComesOutRight)
{
    struct Layout
    {
        const char *label;
        std::vector<FolderMember> members;
        std::vector<size_t> table_order;
        size_t folders;
    };
    auto member = [](const std::string &name, size_t size, uint16_t folder, int64_t offset = -1, int64_t declared = -1)
    {
        FolderMember m{name, ost_itest::noise(static_cast<uint32_t>(name[0]) * 31 + folder, size)};
        m.folder = folder;
        m.declared_offset = offset;
        m.declared_size = declared;
        return m;
    };
    std::vector<Layout> layouts;
    layouts.push_back({"in order", {member("a", 1000, 0), member("b", 2000, 0)}, {}, 1});
    layouts.push_back({"table lists the bytes in descending order", {member("a", 1000, 0), member("b", 2000, 0), member("c", 500, 0)}, {2, 1, 0}, 1});
    layouts.push_back({"table lists the bytes in a shuffled order", {member("a", 7000, 0), member("b", 40000, 0), member("c", 500, 0), member("d", 25000, 0)}, {3, 0, 2, 1}, 1});
    layouts.push_back({"overlapping members", {member("a", 5000, 0), member("b", 1000, 0, 4000, 2000)}, {}, 1});
    layouts.push_back({"a member inside another one", {member("a", 5000, 0), member("b", 100, 0, 1000, 100)}, {}, 1});
    layouts.push_back({"the same bytes twice", {member("a", 5000, 0), member("b", 5000, 0, 0, 5000)}, {}, 1});
    layouts.push_back({"gaps between members", {member("a", 5000, 0, 100, 100), member("b", 5000, 0, 3000, 500)}, {}, 1});
    layouts.push_back({"interleaved folders", {member("a", 3000, 0), member("b", 3000, 1), member("c", 3000, 0), member("d", 3000, 1)}, {0, 1, 2, 3}, 2});
    layouts.push_back({"folders in reverse order", {member("a", 3000, 0), member("b", 3000, 1), member("c", 3000, 2)}, {2, 1, 0}, 3});
    layouts.push_back({"folder 1 only", {member("a", 3000, 1)}, {}, 2});
    for (bool mszip : {false, true})
    {
        for (size_t block_size : {size_t(1000), size_t(20000)})
        {
            for (const Layout &layout : layouts)
            {
                SCOPED_CONTEXT(std::string(layout.label) + (mszip ? ", mszip" : ", stored") + ", block " + std::to_string(block_size));
                FolderCabOptions options;
                options.mszip = std::vector<bool>(layout.folders, mszip);
                options.block_size = block_size;
                options.table_order = layout.table_order;
                const ost_itest::FolderCab built = ost_itest::build_cab_folders(layout.members, options);
                Collector collector;
                CHECK(run(built.bytes, collector).status == CabStatus::Ok);
                std::vector<size_t> order = layout.table_order;
                if (order.empty())
                    for (size_t i = 0; i < layout.members.size(); ++i)
                        order.push_back(i);
                REQUIRE_EQ(collector.members.size(), order.size());
                for (size_t position = 0; position < order.size(); ++position)
                {
                    const FolderMember &m = layout.members[order[position]];
                    SCOPED_CONTEXT("member " + m.name);
                    CHECK_STR_EQ(collector.members[position].name, m.name); // file table order
                    CHECK(collector.members[position].ended);
                    CHECK(collector.members[position].bytes == expected_bytes(layout.members, order[position]));
                    CabStatus status = CabStatus::NotACabinet;
                    CHECK(legacy_member(built.bytes, m.name, &status) == collector.members[position].bytes);
                    CHECK(status == CabStatus::Ok);
                }
            }
        }
    }
}

// Zero-length members are created empty and touch no block, wherever their offset points; they never fail the extraction.
TEST(ImportCabStream, ImpM03_ZeroLengthMembersDecodeNothingAndNeverFail)
{
    FolderCabOptions options;
    options.block_size = 1000;
    options.mszip = {true};
    FolderMember a{"a", ost_itest::noise(1, 3000)};
    FolderMember start{"z-start", ""};
    start.declared_offset = 0;
    FolderMember middle{"z-middle", ""};
    middle.declared_offset = 1500;
    FolderMember at_end{"z-end", ""};
    at_end.declared_offset = 3000;
    FolderMember beyond{"z-beyond", ""};
    beyond.declared_offset = 3001;
    FolderMember far_beyond{"z-far", ""};
    far_beyond.declared_offset = 4000000000LL;
    // ... also when the extracted size is declared as zero
    FolderMember shrunk{"z-shrunk", ost_itest::noise(2, 50)};
    shrunk.declared_size = 0;

    const ost_itest::FolderCab built = ost_itest::build_cab_folders({start, a, middle, at_end, beyond, far_beyond, shrunk}, options);
    Collector collector;
    CHECK(run(built.bytes, collector).status == CabStatus::Ok);
    REQUIRE_EQ(collector.members.size(), 7u);
    for (const Delivered &d : collector.members)
    {
        SCOPED_CONTEXT(d.name);
        CHECK(d.ended);
        if (d.name != "a")
            CHECK(d.bytes.empty());
    }

    // only zero-length members: no block is read at all (not even a damaged one), the compression type is never looked at
    FolderCabOptions damaged = options;
    damaged.corrupt_folder = 0;
    damaged.corrupt_block = 0;
    const ost_itest::FolderCab only_empty = ost_itest::build_cab_folders({start, middle, beyond}, damaged);
    std::string bytes = only_empty.bytes;
    bytes[36 + 6] = 3; // LZX
    const unsigned long long before = blocks_before();
    Collector second;
    CHECK(run(bytes, second).status == CabStatus::Ok);
    CHECK_EQ(blocks_since(before), 0ull);
    CHECK_EQ(second.members.size(), 3u);
}

// A member whose bytes run past the end of its folder fails the extraction; what lies before it was handed over, the member
// itself got its bytes up to the end and no end_member (FDI leaves a partial file).
TEST(ImportCabStream, ImpM03_AMemberPastTheEndOfItsFolderFailsAfterTheBytesThatExist)
{
    for (bool mszip : {false, true})
    {
        SCOPED_CONTEXT(mszip ? "mszip" : "stored");
        FolderCabOptions options;
        options.mszip = {mszip};
        options.block_size = 1000;
        FolderMember first{"first", ost_itest::noise(1, 100)};
        FolderMember last{"last", ost_itest::noise(2, 2900)};
        last.declared_offset = 2900;
        last.declared_size = 200; // folder holds 3000 bytes
        const ost_itest::FolderCab built = ost_itest::build_cab_folders({first, last}, options);
        Collector collector;
        const ost_import::CabExtractResult result = run(built.bytes, collector);
        CHECK(result.status == CabStatus::Corrupt);
        CHECK(result.detail.find("past the end") != std::string::npos);
        REQUIRE_EQ(collector.members.size(), 2u);
        CHECK(collector.members[0].ended);
        CHECK(collector.members[0].bytes == first.content);
        CHECK_FALSE(collector.members[1].ended);
        CHECK_EQ(collector.members[1].bytes.size(), 100u);
        CHECK(ost_import::verify_cab(built.bytes.data(), built.bytes.size()).status == CabStatus::Corrupt);
    }
}

// A member of a folder that does not exist is not reported (FDI skips it); 0xFFFD / 0xFFFE / 0xFFFF are the "continued from /
// to another cabinet" markers.
TEST(ImportCabStream, ImpM03_MembersOfFoldersThatDoNotExistAreSkipped)
{
    FolderCabOptions options;
    FolderMember a{"a", "alpha"};
    for (uint16_t folder : {uint16_t(5), uint16_t(0xFFFD), uint16_t(0xFFFE), uint16_t(0xFFFF)})
    {
        SCOPED_CONTEXT("folder index " + std::to_string(folder));
        const ost_itest::FolderCab good = ost_itest::build_cab_folders({a}, options);
        // member b = folder `folder`: patch the file table of a one-member cabinet to a second entry
        FolderMember b{"b", "beta"};
        const ost_itest::FolderCab two = ost_itest::build_cab_folders({a, b}, options);
        std::string bytes = two.bytes;
        // the second CFFILE entry: header (36) + 1 CFFOLDER (8) + first entry (16 + len("a") + 1) + iFolder at +8
        const size_t second_entry = 36 + 8 + 16 + 2;
        bytes[second_entry + 8] = static_cast<char>(folder & 0xFF);
        bytes[second_entry + 9] = static_cast<char>(folder >> 8);
        Collector collector;
        CHECK(run(bytes, collector).status == CabStatus::Ok);
        REQUIRE_EQ(collector.members.size(), 1u);
        CHECK_STR_EQ(collector.members[0].name, std::string("a"));
        CHECK(collector.members[0].bytes == "alpha");
        CHECK(ost_import::verify_cab(bytes.data(), bytes.size()).status == CabStatus::Ok);
        (void)good;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// failures
// ---------------------------------------------------------------------------------------------------------------

// Corruption in a block that a member needs fails the whole extraction, also in another folder than the one of the members
// that came out first (Windows: "a flipped byte in folder 2 fails although plans.ost had come out intact").
TEST(ImportCabStream, ImpM03_CorruptionAnywhereInTheBlocksInUseFailsTheWholeExtraction)
{
    const std::string good = read_fixture("makecab_two_folders.cab");
    std::string folder2 = good;
    folder2[25601 + 8 + 5] ^= static_cast<char>(0xFF);
    Collector collector;
    const ost_import::CabExtractResult result = run(folder2, collector);
    CHECK(result.status == CabStatus::Corrupt);
    CHECK(!result.detail.empty());
    const Delivered *plans = collector.find("plans.ost");
    REQUIRE(plans != nullptr);
    CHECK(plans->ended);
    CHECK(plans->bytes == text(1, 40000));

    std::string folder1 = good;
    folder1[104 + 8 + 5] ^= static_cast<char>(0xFF);
    Collector second;
    CHECK(run(folder1, second).status == CabStatus::Corrupt);
    CHECK_FALSE(second.members.empty() ? second.members[0].ended : false);

    CHECK(run("this is not a cabinet", second).status == CabStatus::NotACabinet);
    CHECK(run("", second).status == CabStatus::NotACabinet);
}

// The sink steers the extraction: Skip does not deliver a member but it is still decoded and checked, Abort stops, a failing
// write or end stops (the caller cleans up what it already wrote).
TEST(ImportCabStream, ImpM03_TheSinkCanSkipAbortOrFail)
{
    CabBuildOptions options;
    options.mszip = true;
    options.checksums = true;
    options.block_size = 4000;
    const std::vector<CabMember> members = {{"a.bin", ost_itest::noise(1, 6000)}, {"b.bin", ost_itest::noise(2, 6000)}, {"c.bin", "tail"}};
    const std::string cab = ost_itest::build_cab(members, options);

    Collector skipping;
    skipping.actions["b.bin"] = CabMemberAction::Skip;
    CHECK(run(cab, skipping).status == CabStatus::Ok);
    REQUIRE_EQ(skipping.members.size(), 3u);
    CHECK(skipping.members[0].ended);
    CHECK_FALSE(skipping.members[1].ended);
    CHECK(skipping.members[1].bytes.empty());
    CHECK(skipping.members[2].bytes == "tail");

    // a skipped member lying in a damaged block still fails the extraction
    CabBuildOptions damaged = options;
    damaged.corrupt_block = 2; // block 3 holds the end of a.bin and the start of b.bin (offsets 8000..11999)
    damaged.corrupt_mode = CabBuildOptions::FlipPayloadByte;
    const std::string bad = ost_itest::build_cab(members, damaged);
    Collector skipping_bad;
    skipping_bad.actions["b.bin"] = CabMemberAction::Skip;
    skipping_bad.actions["a.bin"] = CabMemberAction::Skip;
    CHECK(run(bad, skipping_bad).status == CabStatus::Corrupt);

    Collector aborting;
    aborting.actions["b.bin"] = CabMemberAction::Abort;
    const ost_import::CabExtractResult aborted = run(cab, aborting);
    CHECK(aborted.status == CabStatus::Corrupt);
    CHECK(aborted.detail.find("aborted") != std::string::npos);
    CHECK_EQ(aborting.members.size(), 2u);

    Collector failing_write;
    failing_write.fail_write = true;
    const ost_import::CabExtractResult write_failed = run(cab, failing_write);
    CHECK(write_failed.status == CabStatus::Corrupt);
    CHECK(write_failed.detail.find("write failed") != std::string::npos);
    CHECK_EQ(failing_write.members.size(), 1u);

    Collector failing_end;
    failing_end.fail_end = true;
    CHECK(run(cab, failing_end).status == CabStatus::Corrupt);
    CHECK_EQ(failing_end.members.size(), 1u);
}

// verify_cab is the extraction with every member discarded: the same decode extent and the same verdicts.
TEST(ImportCabStream, ImpM03_VerifyCabDecodesTheSameBlocksAsAnExtraction)
{
    FolderCabOptions options;
    options.block_size = 20000;
    options.mszip = {true};
    FolderMember member{"Project.ost", ost_itest::noise(5, 100000)};
    member.declared_size = 30000; // blocks 1 and 2
    options.corrupt_folder = 0;
    options.corrupt_block = 4; // not decoded
    const ost_itest::FolderCab trailing = ost_itest::build_cab_folders({member}, options);
    unsigned long long before = blocks_before();
    CHECK(ost_import::verify_cab(trailing.bytes.data(), trailing.bytes.size()).status == CabStatus::Ok);
    CHECK_EQ(blocks_since(before), 2ull);
    options.corrupt_block = 1; // decoded
    const ost_itest::FolderCab damaged = ost_itest::build_cab_folders({member}, options);
    CHECK(ost_import::verify_cab(damaged.bytes.data(), damaged.bytes.size()).status == CabStatus::Corrupt);
}

// Measured with FDI: a cabinet that lists no member does not need its folder table (a header promising 2 folders, cut right
// after the header, extracts "OK 0"); as soon as there is a member the folder table has to be complete.
TEST(ImportCabStream, ImpM03_ACabinetWithoutMembersDoesNotNeedItsFolderTable)
{
    FolderCabOptions options;
    const ost_itest::FolderCab empty = ost_itest::build_cab_folders({}, options);
    std::string cut = empty.bytes.substr(0, 36); // the header of a cabinet with 0 files
    cut[26] = 2;                                 // cFolders = 2, but there is no folder record at all
    cut[27] = 0;
    Collector collector;
    CHECK(run(cut, collector).status == CabStatus::Ok);
    CHECK(collector.members.empty());
    CHECK(ost_import::verify_cab(cut.data(), cut.size()).status == CabStatus::Ok);

    const ost_itest::FolderCab one = ost_itest::build_cab_folders({FolderMember{"a", "alpha"}}, options);
    const std::string broken = one.bytes.substr(0, 40); // the header and half of the one folder record
    Collector second;
    CHECK(run(broken, second).status == CabStatus::Corrupt);
}

// The CFDATA checksum covers cbData, cbUncomp, the per-block reserve bytes and the payload (MS-CAB). Measured with FDI: a
// cabinet whose checksum includes the reserve extracts for every reserve size (1..255, odd sizes too) and one whose checksum
// leaves it out fails. The old reader checked it the other way round (it left the reserve out), so it rejected valid cabinets
// that have a per-block reserve and accepted ones the Windows library refuses.
TEST(ImportCabStream, ImpM03_TheCfdataChecksumCoversThePerBlockReserve)
{
    const std::vector<CabMember> members = {{"Project.ost", "<XML_ROOT />"}, {"TempImages!.tmp\\a.pdf", "%PDF-1.4 fixture"}};
    for (unsigned reserve : {1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u, 15u, 17u, 255u})
    {
        SCOPED_CONTEXT("reserve " + std::to_string(reserve));
        ost_itest::HeaderAreas areas;
        areas.flags = 0x0004;
        areas.data_reserve = static_cast<uint8_t>(reserve);
        const std::string good = ost_itest::build_cab_with_header_areas(members, areas);
        Collector collector;
        CHECK(run(good, collector).status == CabStatus::Ok);
        REQUIRE_EQ(collector.members.size(), 2u);
        CHECK_STR_EQ(collector.members[0].bytes, std::string("<XML_ROOT />"));
        CHECK_STR_EQ(collector.members[1].bytes, std::string("%PDF-1.4 fixture"));
        CHECK(ost_import::verify_cab(good.data(), good.size()).status == CabStatus::Ok);

        areas.checksum_covers_reserve = false;
        const std::string wrong = ost_itest::build_cab_with_header_areas(members, areas);
        Collector second;
        const ost_import::CabExtractResult result = run(wrong, second);
        // a reserve whose bytes happen not to change the checksum (every word cancels) is not a detectable difference
        if (result.status != CabStatus::Ok)
            CHECK(result.detail.find("checksum") != std::string::npos);
    }
    // the reserve bytes of the test builder are 0x5A: for 4 bytes the checksum differs, so the wrong one must fail
    ost_itest::HeaderAreas four;
    four.flags = 0x0004;
    four.data_reserve = 4;
    four.checksum_covers_reserve = false;
    const std::string wrong = ost_itest::build_cab_with_header_areas(members, four);
    Collector third;
    CHECK(run(wrong, third).status == CabStatus::Corrupt);
}

// Cabinets that use the header areas legitimately extract: previous / next cabinet and disk names, a header reserve, a folder
// reserve (all measured with FDI); a cabinet or disk name of 256 bytes and a header reserve above 60000 bytes do not.
TEST(ImportCabStream, ImpM03_HeaderAreasAreReadLikeTheWindowsLibraryReadsThem)
{
    const std::vector<CabMember> members = {{"a.txt", "hello world"}};
    auto extracts = [&](const ost_itest::HeaderAreas &areas, CabStatus expected)
    {
        const std::string cab = ost_itest::build_cab_with_header_areas(members, areas);
        Collector collector;
        const ost_import::CabExtractResult result = run(cab, collector);
        CHECK(result.status == expected);
        if (expected == CabStatus::Ok)
        {
            REQUIRE_EQ(collector.members.size(), 1u);
            CHECK_STR_EQ(collector.members[0].bytes, std::string("hello world"));
        }
        CHECK_EQ(ost_import::list_cab(cab.data(), cab.size()).size(), expected == CabStatus::Ok ? 1u : 0u);
    };
    ost_itest::HeaderAreas areas;
    areas.flags = 0x0001;
    areas.previous_cabinet = "prev.cab";
    areas.previous_disk = "Disk 1";
    extracts(areas, CabStatus::Ok);
    areas = ost_itest::HeaderAreas();
    areas.flags = 0x0002;
    areas.next_cabinet = "next.cab";
    areas.next_disk = "Disk 2";
    extracts(areas, CabStatus::Ok);
    areas.flags = 0x0003;
    areas.previous_cabinet = "prev.cab";
    areas.previous_disk = "Disk 1";
    extracts(areas, CabStatus::Ok);
    areas = ost_itest::HeaderAreas();
    areas.flags = 0x0005;
    areas.previous_cabinet = "prev.cab";
    areas.previous_disk = "Disk 1";
    areas.header_reserve = 10;
    areas.folder_reserve = 6;
    areas.data_reserve = 8;
    extracts(areas, CabStatus::Ok);

    // names: 255 bytes pass, 256 fail (cabinet, disk, previous, next)
    for (size_t length : {size_t(255), size_t(256)})
    {
        const CabStatus expected = length == 255 ? CabStatus::Ok : CabStatus::Corrupt;
        SCOPED_CONTEXT("name length " + std::to_string(length));
        ost_itest::HeaderAreas names;
        names.flags = 0x0001;
        names.previous_cabinet = std::string(length, 'p');
        names.previous_disk = "d";
        extracts(names, expected);
        names.previous_cabinet = "p";
        names.previous_disk = std::string(length, 'd');
        extracts(names, expected);
        names = ost_itest::HeaderAreas();
        names.flags = 0x0002;
        names.next_cabinet = std::string(length, 'n');
        names.next_disk = "d";
        extracts(names, expected);
        names.next_cabinet = "n";
        names.next_disk = std::string(length, 'd');
        extracts(names, expected);
    }
    // the header reserve: 60000 passes, 60001 is not a cabinet
    for (size_t reserve : {size_t(0), size_t(255), size_t(60000)})
    {
        ost_itest::HeaderAreas big;
        big.flags = 0x0004;
        big.header_reserve = reserve;
        extracts(big, CabStatus::Ok);
    }
    for (size_t reserve : {size_t(60001), size_t(65535)})
    {
        ost_itest::HeaderAreas big;
        big.flags = 0x0004;
        big.header_reserve = reserve;
        const std::string cab = ost_itest::build_cab_with_header_areas(members, big);
        Collector collector;
        CHECK(run(cab, collector).status == CabStatus::NotACabinet);
        CHECK(ost_import::list_cab(cab.data(), cab.size()).empty());
    }
}

// A block whose deflate stream holds more data than its cbUncomp declares is corrupt: the Windows library fails it with a
// decompression error (found by differential fuzzing after a flipped cbUncomp; before, the block was cut at cbUncomp and
// accepted). A stream that ends exactly at cbUncomp is fine.
TEST(ImportCabStream, ImpM03_AnMszipBlockWithMoreDataThanItDeclaresFails)
{
    FolderCabOptions options;
    options.mszip = {true};
    options.checksums = false; // the checksum would catch the changed header first
    options.block_size = 32768;
    FolderMember member{"Project.ost", text(3, 1000)};
    const ost_itest::FolderCab built = ost_itest::build_cab_folders({member}, options);
    Collector intact;
    CHECK(run(built.bytes, intact).status == CabStatus::Ok);
    CHECK(intact.members[0].bytes == member.content);

    // cbUncomp (the 16 bit value at +6 of the CFDATA header) cut from 1000 to 999 while the stream still holds 1000 bytes; the
    // member is declared with 999 bytes so that it lies inside the shortened folder
    member.declared_size = 999;
    const ost_itest::FolderCab shortened = ost_itest::build_cab_folders({member}, options);
    std::string damaged = shortened.bytes;
    const size_t header = shortened.block_offsets[0][0];
    REQUIRE_EQ(static_cast<unsigned char>(damaged[header + 6]), 1000u & 0xFF);
    damaged[header + 6] = static_cast<char>(999 & 0xFF);
    damaged[header + 7] = static_cast<char>(999 >> 8);
    Collector collector;
    const ost_import::CabExtractResult result = run(damaged, collector);
    CHECK(result.status == CabStatus::Corrupt);
    CHECK(result.detail.find("more than") != std::string::npos);
    CHECK(ost_import::verify_cab(damaged.data(), damaged.size()).status == CabStatus::Corrupt);
    CHECK(ost_import::extract_cab_member(damaged.data(), damaged.size(), "Project.ost").status == CabStatus::Corrupt);
}

// ---------------------------------------------------------------------------------------------------------------
// to disk
// ---------------------------------------------------------------------------------------------------------------

// The Windows flow in Core: every member below the directory, sub directories created, members written as they are decoded.
TEST(ImportCabStream, ImpM03_ExtractToDirectoryWritesEveryMemberStraightToItsFile)
{
    CabBuildOptions options;
    options.mszip = true;
    options.checksums = true;
    options.block_size = 20000;
    const std::vector<CabMember> members = {{"Project.ost", text(7, 40000)},
                                            {"TempImages!.tmp\\1 Estimates\\2026\\A0-0.pdf", "%PDF-1.4\n" + ost_itest::noise(3, 50000)},
                                            {"TempImages!.tmp/sheets/B1.pdf", "%PDF-1.4\n" + ost_itest::noise(4, 1234)},
                                            {"TempImages!.tmp\\empty.pdf", ""}};
    const std::string cab = ost_itest::build_cab(members, options);
    TempDir dir("stream");
    const unsigned long long before = blocks_before();
    const ost_import::CabExtractResult result = ost_import::extract_cab_to_directory(cab.data(), cab.size(), dir.utf8());
    CHECK(result.status == CabStatus::Ok);
    CHECK_EQ(blocks_since(before), (members[0].content.size() + members[1].content.size() + members[2].content.size() + 19999) / 20000);
    const char *paths[] = {"Project.ost", "TempImages!.tmp/1 Estimates/2026/A0-0.pdf", "TempImages!.tmp/sheets/B1.pdf", "TempImages!.tmp/empty.pdf"};
    for (size_t i = 0; i < members.size(); ++i)
    {
        SCOPED_CONTEXT(std::string("file ") + paths[i]);
        std::string bytes;
        REQUIRE(dir.read(paths[i], bytes));
        CHECK(bytes == members[i].content);
    }
    CHECK_EQ(dir.file_count(), 4u);

    // an existing file is overwritten
    {
        std::ofstream stale(dir.file("Project.ost"), std::ios::binary | std::ios::trunc);
        stale << "old and longer than the new content, which is overwritten completely" << std::string(100000, 'x');
    }
    CHECK(ost_import::extract_cab_to_directory(cab.data(), cab.size(), dir.utf8()).status == CabStatus::Ok);
    std::string bytes;
    REQUIRE(dir.read("Project.ost", bytes));
    CHECK(bytes == members[0].content);
}

// Port of tests/integration/native/test_osp_cab.py::test_native_cab_round_trip_preserves_utf8_paths_and_member_names: a member
// 'TempImages!.tmp\source <en dash> drawing.pdf' (UTF-8, flagged _A_NAME_IS_UTF) lands below the directory under that name.
TEST(ImportCabStream, ImpM03_Imp18_AUtf8MemberNameLandsUnderItsUnicodePath)
{
    CabMember member{"TempImages!.tmp\\source \xE2\x80\x93 drawing.pdf", "%PDF-1.4 unicode"};
    member.attribs = 0x80;
    const std::string cab = ost_itest::build_cab({member}, CabBuildOptions());
    TempDir dir("utf8name");
    CHECK(ost_import::extract_cab_to_directory(cab.data(), cab.size(), dir.utf8()).status == CabStatus::Ok);
    std::string bytes;
    REQUIRE(dir.read("TempImages!.tmp/source \xE2\x80\x93 drawing.pdf", bytes));
    CHECK_STR_EQ(bytes, std::string("%PDF-1.4 unicode"));
    CHECK_EQ(dir.file_count(), 1u);
}

// Port of ::test_osp_imports_legacy_ansi_members_from_unicode_archive_path: the legacy OST writes the member names in the
// ANSI code page (cp1252, no UTF flag): the en dash is the single byte 0x96. The files must land under the UTF-8 name.
TEST(ImportCabStream, ImpM03_Imp18_ALegacyAnsiMemberLandsUnderTheDecodedUtf8Path)
{
    const std::string project = "26-061 412 \x96 Corporate Lot K, KS"; // cp1252
    const std::string utf8_project = "26-061 412 \xE2\x80\x93 Corporate Lot K, KS";
    const std::vector<CabMember> members = {
        {project + ".ost", "<XML_ROOT />"},
        {"TempImages!.tmp\\1 Estimates through 1-12-2015\\2026\\" + project + "\\01. Drawings\\A0-0.0.pdf", "%PDF-1.4 fixture"}};
    const std::string cab = ost_itest::build_cab(members, CabBuildOptions());
    TempDir dir("legacy_ansi");
    const ost_import::CabExtractResult result = ost_import::extract_cab_to_directory(cab.data(), cab.size(), dir.utf8());
    CHECK(result.status == CabStatus::Ok);
    std::string ost, pdf;
    REQUIRE(dir.read(utf8_project + ".ost", ost));
    REQUIRE(dir.read("TempImages!.tmp/1 Estimates through 1-12-2015/2026/" + utf8_project + "/01. Drawings/A0-0.0.pdf", pdf));
    CHECK_STR_EQ(ost, std::string("<XML_ROOT />"));
    CHECK_STR_EQ(pdf, std::string("%PDF-1.4 fixture"));
    CHECK_EQ(dir.file_count(), 2u);
}

// IMP-15: the names are checked BEFORE anything is extracted. One unsafe name anywhere (also the last one) rejects the package
// and nothing is written; a member that is rejected never creates a directory either.
// Port of tests/infrastructure/mdb/importers/test_osp_importer.py::test_osp_import_rejects_unsafe_cab_member_paths_before_extraction.
TEST(ImportCabStream, ImpM03_Imp15_AnUnsafeMemberNameRejectsThePackageBeforeAnythingIsWritten)
{
    const char *unsafe[] = {"payload.ost:stream", ".", "//server/share/payload.ost", "CON.pdf", "Plan.", "a/..", "../x.pdf",
                            "C:\\Users\\x\\outside\\payload.ost", "\\Users\\x\\outside\\payload.ost", "..\\..\\outside\\payload.ost",
                            "\\\\?\\C:\\x.pdf", "//?/x", "dir \\plan.pdf", "TempImages!.tmp\\COM3.pdf", "TempImages!.tmp\\sheets.\\a.png", "", "nul"};
    for (const char *bad : unsafe)
    {
        SCOPED_CONTEXT(std::string("unsafe member '") + bad + "'");
        const std::string cab = ost_itest::build_cab({{"Project.ost", "<XML_ROOT />"}, {"TempImages!.tmp\\ok\\a.pdf", "%PDF"}, {bad, "payload"}}, CabBuildOptions());
        TempDir dir("unsafe");
        const unsigned long long before = blocks_before();
        const ost_import::CabExtractResult result = ost_import::extract_cab_to_directory(cab.data(), cab.size(), dir.utf8());
        CHECK(result.status == CabStatus::UnsafeName);
        // the Windows text: f"unsafe CAB member path: {raw_name!r}" with the name as the cabinet stores it (backslashes kept and
        // doubled by repr; these names have no quote characters)
        std::string quoted;
        for (const char *c = bad; *c; ++c)
        {
            if (*c == '\\')
                quoted += '\\';
            quoted += *c;
        }
        CHECK_EQ(result.detail, "unsafe CAB member path: '" + quoted + "'");
        CHECK_EQ(dir.entry_count(), 0u); // no file, no directory
        CHECK_EQ(blocks_since(before), 0ull);
    }
    // the same names first in the table
    const std::string first = ost_itest::build_cab({{"CON.pdf", "x"}, {"Project.ost", "<XML_ROOT />"}}, CabBuildOptions());
    TempDir dir("unsafe_first");
    CHECK(ost_import::extract_cab_to_directory(first.data(), first.size(), dir.utf8()).status == CabStatus::UnsafeName);
    CHECK_EQ(dir.entry_count(), 0u);
}

// Names that merely look like the unsafe ones are fine.
TEST(ImportCabStream, ImpM03_Imp15_OrdinaryNamesAreExtracted)
{
    const std::vector<CabMember> members = {{"nulls.ost", "a"}, {"console.pdf", "b"}, {"com10.pdf", "c"}, {"TempImages!.tmp\\a.b.png", "d"}, {"..X\\e.pdf", "e"}};
    const std::string cab = ost_itest::build_cab(members, CabBuildOptions());
    TempDir dir("ordinary");
    CHECK(ost_import::extract_cab_to_directory(cab.data(), cab.size(), dir.utf8()).status == CabStatus::Ok);
    CHECK_EQ(dir.file_count(), 5u);
    std::string bytes;
    REQUIRE(dir.read("..X/e.pdf", bytes));
    CHECK_STR_EQ(bytes, std::string("e"));
}

// A path that cannot be created or written is a failed extraction (Windows: extract_cab false): a member below a plain file,
// a member that is a directory.
TEST(ImportCabStream, ImpM03_APathThatCannotBeWrittenFailsTheExtraction)
{
    {
        const std::string cab = ost_itest::build_cab({{"a", "file"}, {"a\\b.pdf", "below a file"}}, CabBuildOptions());
        TempDir dir("below_file");
        CHECK(ost_import::extract_cab_to_directory(cab.data(), cab.size(), dir.utf8()).status == CabStatus::Corrupt);
    }
    {
        const std::string cab = ost_itest::build_cab({{"a\\b.pdf", "x"}, {"a", "file"}}, CabBuildOptions());
        TempDir dir("is_directory");
        CHECK(ost_import::extract_cab_to_directory(cab.data(), cab.size(), dir.utf8()).status == CabStatus::Corrupt);
    }
    {
        const std::string cab = ost_itest::build_cab({{"Project.ost", "x"}}, CabBuildOptions());
        TempDir dir("missing_parent");
        std::string missing = dir.utf8() + "/no/such/parent";
        // the directory itself is created like the members' sub directories
        CHECK(ost_import::extract_cab_to_directory(cab.data(), cab.size(), missing).status == CabStatus::Ok);
        std::string bytes;
        std::ifstream in(std::filesystem::u8path(missing) / "Project.ost", std::ios::binary);
        CHECK(static_cast<bool>(in));
    }
}
