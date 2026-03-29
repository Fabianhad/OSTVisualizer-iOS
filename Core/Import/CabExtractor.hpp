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

    std::vector<char> extract_cab_file(const char *data, size_t length,
                                       const std::string &filename);

}
