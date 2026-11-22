// Layer: Import (pure C++)
// Minimal CAB (Microsoft Cabinet) archive reader.
// Supports MSZIP compression using zlib.
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace ost_import
{

    struct CabEntry
    {
        // The member name as UTF-8 with '/' as separator. CAB names are stored as bytes: UTF-8 when the CFFILE attribute
        // _A_NAME_IS_UTF (0x80) is set, otherwise in the legacy ANSI code page, read as cp1252 (the Windows importer
        // decodes them the same way, cab_compressor.cpp decode_cab_member, with the machine ANSI code page).
        std::string name;
        // The same decoded name with the separators as the cabinet stores them (the spelling the Windows listing returns, and
        // the one its error messages quote); only for messages, use `name` everywhere else.
        std::string stored_name;
        uint32_t uncompressed_size;
        uint16_t folder_index;
        uint32_t offset_in_folder;
        uint16_t attribs = 0;
    };

    // The members of a cabinet in table order, like the Windows listing (cab_compressor.cpp list_cab): only members of a
    // folder that exists are reported (the library skips the others). EMPTY when it is not a cabinet, when the member table
    // is cut short, or when a member name cannot be decoded (UTF-8 flagged but not valid UTF-8) or is 256 bytes or longer
    // (the Windows cabinet library rejects all of those).
    std::vector<CabEntry> list_cab(const char *data, size_t length);

    enum class CabStatus
    {
        Ok,
        NotACabinet, // no MSCF signature, or a format version other than 1.3 (the only one the Windows library reads)
        NotFound,    // the cabinet has no member with that name
        UnsafeName,  // extract_cab_to_directory: a member name is unsafe on a Windows file system (CabMemberName.hpp)
        Corrupt,     // truncated header/block, wrong CFDATA checksum, a block that does not inflate or whose size differs
                     // from its header, a member past the end of its folder, a compression type that is neither stored
                     // nor MSZIP (IMP-17 owns the question whether more types are needed)
    };

    struct CabExtractResult
    {
        CabStatus status = CabStatus::NotACabinet;
        std::vector<char> data; // the member's bytes when status == Ok (an empty member is Ok with no data)
        std::string detail;     // why it failed
    };

    // Extracts one member. Like the Windows cabinet library (FDICopy) it never returns partial or shifted bytes:
    // any corruption in the member's folder is a failure (status != Ok). This is the older per-member extractor: it inflates
    // the whole folder of the member into one buffer for every call (IMP-M03), which is too much memory for a big package;
    // the OSP import is meant to use extract_cab_to_directory below. It stays as the reference the single pass is tested
    // against.
    CabExtractResult extract_cab_member(const char *data, size_t length,
                                        const std::string &filename);

    // Windows FDICopy semantics (cpp_extensions/src/cab/cab_compressor.cpp extract_cab): the Windows importer extracts
    // EVERY member in one pass and any corruption in the blocks that pass reads fails the whole extraction, even in a member
    // the import never uses (checked with expand.exe and a native FDI probe: a flipped byte in the second folder of a
    // two-folder cabinet fails the extraction although the member of the first folder had come out intact). The library
    // reads a folder only up to the block that holds the last byte of the last member it extracts, so damage in later blocks
    // is not noticed. verify_cab is extract_cab with every member discarded: it decodes exactly those blocks, checks that
    // every member lies inside its folder and reports the first problem (status != Ok, `detail` says which). `data` is
    // always empty.
    CabExtractResult verify_cab(const char *data, size_t length);

    // ---- single pass extraction (IMP-M03) --------------------------------------------------------------------------
    //
    // The Windows importer extracts the whole cabinet in ONE pass of the cabinet library (FDICopy) straight to files:
    // memory does not grow with the package. extract_cab_member / extract_cab_file instead decode the whole folder of
    // the member into one buffer for every call (kept as the reference the single pass is tested against). extract_cab
    // walks the file table in order and decodes every CFDATA block it needs once, handing the bytes of each member to a
    // sink in pieces of at most one block; only the current block is held. A table that lists the members out of order
    // (descending offsets, overlap, folders interleaved) makes it read a folder again from its first block, like the
    // Windows library does.

    enum class CabMemberAction
    {
        Extract, // deliver the bytes of this member (write / end_member)
        Skip,    // do not deliver this member (its bytes are still decoded and checked: the folder is read in one pass)
        Abort    // stop now: extract_cab returns status Corrupt with detail "aborted by the sink"
    };

    class CabMemberSink
    {
    public:
        virtual ~CabMemberSink() = default;
        // Called for every member in cabinet table order (members of a folder that does not exist are not reported, like the
        // Windows library does).
        virtual CabMemberAction begin_member(const CabEntry &entry) = 0;
        // The next bytes of the member being extracted; false aborts (extract_cab then fails with "write failed").
        virtual bool write(const char *bytes, size_t length) = 0;
        // After the last byte of the member; false aborts.
        virtual bool end_member() = 0;
    };

    // Extracts like the Windows library (FDICopy): any corruption in the blocks the members need fails the extraction, also
    // in a member the sink skipped (Skip only withholds the bytes; verify_cab is this call with a sink that skips
    // everything); blocks after the last byte of the last member of a folder are never read. A zero-length member is
    // reported (begin_member / end_member) and touches no block. status Ok means every member lay inside its folder and
    // every extracted member was delivered completely. On any other status the sink may already have received bytes (the
    // caller discards them); a member that runs past the end of its folder got the bytes that exist and no end_member.
    CabExtractResult extract_cab(const char *data, size_t length, CabMemberSink &sink);

    // The Windows flow in Core (osp_importer._extracted_ost): writes every member below `directory` (UTF-8 path; it and the
    // member sub directories are created, files are overwritten), member by member as the cabinet is decoded. Every name is
    // judged first (is_safe_cab_member_name: status UnsafeName, `detail` = "unsafe CAB member path: '<name>'", nothing is
    // created or written); a path that cannot be created or written (a member below a plain file, a member name that is a
    // directory) is Corrupt. On a failure the files written so far stay (the caller removes its temporary directory, as
    // the Windows importer does).
    CabExtractResult extract_cab_to_directory(const char *data, size_t length, const std::string &directory);

    // Diagnostic: how many CFDATA blocks have been decoded (inflated or copied) by this process so far. A single pass
    // over a cabinet adds exactly its number of blocks.
    unsigned long long cab_blocks_decoded();

    // extract_cab_member that reports failure as an EMPTY vector. A zero-length member is indistinguishable from a
    // failure here: callers that must tell them apart (the OSP import) use extract_cab_member.
    std::vector<char> extract_cab_file(const char *data, size_t length,
                                       const std::string &filename);

}
