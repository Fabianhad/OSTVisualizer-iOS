// Layer: Import (pure C++)
// Minimal CAB (Microsoft Cabinet) archive reader with MSZIP decompression.
// Reference: Microsoft Cabinet File Format specification.
#include "CabExtractor.hpp"
#include <zlib.h>
#include <cstring>
#include <algorithm>

namespace ost_import
{

    static uint16_t read_u16(const char *p)
    {
        const unsigned char *u = reinterpret_cast<const unsigned char *>(p);
        return (uint16_t)(u[0] | (u[1] << 8));
    }

    static uint32_t read_u32(const char *p)
    {
        const unsigned char *u = reinterpret_cast<const unsigned char *>(p);
        return (uint32_t)(u[0] | (u[1] << 8) | (u[2] << 16) | (u[3] << 24));
    }

    static const uint32_t CAB_SIGNATURE = 0x4643534D; // "MSCF"
    static const size_t CFHEADER_MIN_SIZE = 36;
    static const size_t CFFOLDER_SIZE = 8;
    static const uint16_t COMPRESS_NONE = 0x0000;
    static const uint16_t COMPRESS_MSZIP = 0x0001;
    static const size_t MSZIP_WINDOW = 32768;

    struct CabFolder
    {
        uint32_t data_offset;
        uint16_t data_count;
        uint16_t compress_type;
    };

    struct CabHeader
    {
        uint32_t cabinet_size;
        uint32_t files_offset;
        uint16_t folder_count;
        uint16_t file_count;
        uint16_t flags;
        uint8_t folder_reserve_size;
        uint8_t data_reserve_size;
    };

    static bool parse_header(const char *data, size_t length, CabHeader &hdr)
    {
        if (length < CFHEADER_MIN_SIZE)
            return false;
        if (read_u32(data) != CAB_SIGNATURE)
            return false;

        hdr.cabinet_size = read_u32(data + 8);
        hdr.files_offset = read_u32(data + 16);
        hdr.folder_count = read_u16(data + 26);
        hdr.file_count = read_u16(data + 28);
        hdr.flags = read_u16(data + 30);
        hdr.folder_reserve_size = 0;
        hdr.data_reserve_size = 0;

        return true;
    }

    static size_t header_end_offset(const char *data, size_t length,
                                    CabHeader &hdr)
    {
        size_t off = CFHEADER_MIN_SIZE;

        if (hdr.flags & 0x0004)
        {
            if (off + 4 > length)
                return 0;
            uint16_t hdr_reserve = read_u16(data + off);
            hdr.folder_reserve_size = (uint8_t)(data[off + 2]);
            hdr.data_reserve_size = (uint8_t)(data[off + 3]);
            off += 4 + hdr_reserve;
        }

        if (hdr.flags & 0x0001)
        {
            while (off < length && data[off] != '\0')
                off++;
            off++;
            while (off < length && data[off] != '\0')
                off++;
            off++;
        }

        if (hdr.flags & 0x0002)
        {
            while (off < length && data[off] != '\0')
                off++;
            off++;
            while (off < length && data[off] != '\0')
                off++;
            off++;
        }

        return off;
    }

    static std::vector<CabFolder> parse_folders(const char *data, size_t length,
                                                size_t offset, uint16_t count,
                                                uint8_t folder_reserve_size)
    {
        std::vector<CabFolder> folders;
        folders.reserve(count);
        size_t stride = CFFOLDER_SIZE + folder_reserve_size;

        for (uint16_t i = 0; i < count; i++)
        {
            if (offset + CFFOLDER_SIZE > length)
                break;
            CabFolder f;
            f.data_offset = read_u32(data + offset);
            f.data_count = read_u16(data + offset + 4);
            f.compress_type = read_u16(data + offset + 6) & 0x000F;
            folders.push_back(f);
            offset += stride;
        }

        return folders;
    }

    static std::vector<CabEntry> parse_files(const char *data, size_t length,
                                             size_t offset, uint16_t count)
    {
        std::vector<CabEntry> entries;
        entries.reserve(count);

        for (uint16_t i = 0; i < count; i++)
        {
            if (offset + 16 > length)
                break;

            CabEntry e;
            e.uncompressed_size = read_u32(data + offset);
            e.offset_in_folder = read_u32(data + offset + 4);
            e.folder_index = read_u16(data + offset + 8);

            size_t name_start = offset + 16;
            size_t name_end = name_start;
            while (name_end < length && data[name_end] != '\0')
                name_end++;

            if (name_end > name_start)
            {
                e.name.assign(data + name_start, name_end - name_start);
                for (auto &ch : e.name)
                {
                    if (ch == '\\')
                        ch = '/';
                }
            }

            entries.push_back(e);
            offset = name_end + 1;
        }

        return entries;
    }

    static std::vector<char> decompress_folder(const char *data, size_t length,
                                               const CabFolder &folder,
                                               uint8_t data_reserve_size)
    {
        std::vector<char> result;
        size_t offset = folder.data_offset;

        unsigned char window[MSZIP_WINDOW];
        size_t window_size = 0;

        for (uint16_t block = 0; block < folder.data_count; block++)
        {
            if (offset + 8 > length)
                break;

            uint16_t comp_size = read_u16(data + offset + 4);
            uint16_t uncomp_size = read_u16(data + offset + 6);
            offset += 8 + data_reserve_size;

            if (offset + comp_size > length)
                break;

            const char *block_data = data + offset;

            if (folder.compress_type == COMPRESS_NONE)
            {
                result.insert(result.end(), block_data, block_data + comp_size);
            }
            else if (folder.compress_type == COMPRESS_MSZIP)
            {
                if (comp_size < 2 || block_data[0] != 'C' || block_data[1] != 'K')
                {
                    offset += comp_size;
                    continue;
                }

                const char *deflate_data = block_data + 2;
                size_t deflate_len = comp_size - 2;
                size_t out_size = uncomp_size > 0 ? uncomp_size : MSZIP_WINDOW;
                std::vector<char> buf(out_size);

                z_stream strm;
                memset(&strm, 0, sizeof(strm));
                if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
                    break;

                if (window_size > 0)
                    inflateSetDictionary(&strm, window, (uInt)window_size);

                strm.next_in = (Bytef *)deflate_data;
                strm.avail_in = (uInt)deflate_len;
                strm.next_out = (Bytef *)buf.data();
                strm.avail_out = (uInt)out_size;

                int ret = inflate(&strm, Z_SYNC_FLUSH);
                inflateEnd(&strm);

                if (ret != Z_STREAM_END && ret != Z_OK && ret != Z_BUF_ERROR)
                {
                    offset += comp_size;
                    continue;
                }

                size_t produced = out_size - strm.avail_out;

                if (produced >= MSZIP_WINDOW)
                {
                    memcpy(window, buf.data() + produced - MSZIP_WINDOW, MSZIP_WINDOW);
                    window_size = MSZIP_WINDOW;
                }
                else
                {
                    size_t keep = (window_size + produced > MSZIP_WINDOW)
                                      ? MSZIP_WINDOW - produced
                                      : window_size;
                    if (keep > 0 && keep < window_size)
                        memmove(window, window + window_size - keep, keep);
                    memcpy(window + keep, buf.data(), produced);
                    window_size = keep + produced;
                }

                result.insert(result.end(), buf.begin(), buf.begin() + produced);
            }
            else
            {
                break;
            }

            offset += comp_size;
        }

        return result;
    }

    std::vector<CabEntry> list_cab(const char *data, size_t length)
    {
        CabHeader hdr;
        if (!parse_header(data, length, hdr))
            return {};

        size_t folder_offset = header_end_offset(data, length, hdr);
        if (folder_offset == 0)
            return {};

        return parse_files(data, length, hdr.files_offset, hdr.file_count);
    }

    std::vector<char> extract_cab_file(const char *data, size_t length,
                                       const std::string &filename)
    {
        CabHeader hdr;
        if (!parse_header(data, length, hdr))
            return {};

        size_t folder_offset = header_end_offset(data, length, hdr);
        if (folder_offset == 0)
            return {};

        auto folders = parse_folders(data, length, folder_offset,
                                     hdr.folder_count, hdr.folder_reserve_size);
        auto files = parse_files(data, length, hdr.files_offset, hdr.file_count);

        const CabEntry *target = nullptr;
        for (const auto &entry : files)
        {
            std::string a = entry.name, b = filename;
            std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            std::transform(b.begin(), b.end(), b.begin(), ::tolower);
            if (a == b)
            {
                target = &entry;
                break;
            }
        }

        if (!target)
            return {};
        if (target->folder_index >= folders.size())
            return {};

        auto folder_data = decompress_folder(data, length,
                                             folders[target->folder_index],
                                             hdr.data_reserve_size);

        uint32_t start = target->offset_in_folder;
        uint32_t end = start + target->uncompressed_size;

        if (end > folder_data.size())
            return {};

        return std::vector<char>(folder_data.begin() + start,
                                 folder_data.begin() + end);
    }

}
