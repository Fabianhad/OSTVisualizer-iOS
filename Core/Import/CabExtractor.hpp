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
        std::string name;
        uint32_t uncompressed_size;
        uint16_t folder_index;
        uint32_t offset_in_folder;
    };

    std::vector<CabEntry> list_cab(const char *data, size_t length);

    enum class CabStatus
    {
        Ok,
        NotACabinet, // no MSCF signature
        NotFound,    // the cabinet has no member with that name
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
    // any corruption in the member's folder is a failure (status != Ok).
    CabExtractResult extract_cab_member(const char *data, size_t length,
                                        const std::string &filename);

    // extract_cab_member that reports failure as an EMPTY vector. A zero-length member is indistinguishable from a
    // failure here: callers that must tell them apart (the OSP import) use extract_cab_member.
    std::vector<char> extract_cab_file(const char *data, size_t length,
                                       const std::string &filename);

}
