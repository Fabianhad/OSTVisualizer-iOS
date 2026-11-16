// Layer: Import (pure C++)
// Minimal CAB (Microsoft Cabinet) archive reader with MSZIP decompression.
// Reference: Microsoft Cabinet File Format specification.
#include "CabExtractor.hpp"
#include <zlib.h>
#include <cstring>
#include <algorithm>
#include <string>

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

    // Cabinet checksum (Microsoft Cabinet File Format, CSUMCOMPUTE; the same as libmspack cabd_checksum): XOR of the
    // little-endian 32-bit words of the data, the 1-3 trailing bytes read as a partial word.
    static uint32_t cab_checksum(const unsigned char *data, size_t bytes, uint32_t checksum)
    {
        for (size_t words = bytes >> 2; words > 0; --words, data += 4)
            checksum ^= (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
        uint32_t tail = 0;
        switch (bytes & 3)
        {
        case 3:
            tail |= (uint32_t)(*data++) << 16;
            // fall through
        case 2:
            tail |= (uint32_t)(*data++) << 8;
            // fall through
        case 1:
            tail |= (uint32_t)(*data);
            break;
        default:
            break;
        }
        return checksum ^ tail;
    }

    // Decodes every CFDATA block of a folder. The Windows importer extracts through the Windows cabinet library
    // (FDICopy), where any corrupt block makes the whole extraction fail, so nothing here is skipped or repaired: a
    // truncated block, a CFDATA checksum that does not match (a checksum of 0 means "none"), an MSZIP block without
    // the "CK" signature or that does not inflate, and a block that does not produce exactly cbUncomp bytes all
    // fail the folder. Returns false (and a description in `detail`) instead of a shifted/partial buffer.
    static bool decompress_folder(const char *data, size_t length,
                                  const CabFolder &folder,
                                  uint8_t data_reserve_size,
                                  std::vector<char> &result,
                                  std::string &detail)
    {
        result.clear();
        size_t offset = folder.data_offset;

        unsigned char window[MSZIP_WINDOW];
        size_t window_size = 0;

        if (folder.compress_type != COMPRESS_NONE && folder.compress_type != COMPRESS_MSZIP)
        {
            detail = "unsupported compression type " + std::to_string(folder.compress_type);
            return false;
        }

        for (uint16_t block = 0; block < folder.data_count; block++)
        {
            const std::string where = "CFDATA block " + std::to_string(block + 1) + " of " + std::to_string(folder.data_count);
            if (offset > length || length - offset < 8 + (size_t)data_reserve_size)
            {
                detail = where + " is truncated";
                return false;
            }

            const uint32_t stored_checksum = read_u32(data + offset);
            const uint16_t comp_size = read_u16(data + offset + 4);
            const uint16_t uncomp_size = read_u16(data + offset + 6);
            const size_t header_offset = offset;
            offset += 8 + data_reserve_size;

            if (length - offset < comp_size)
            {
                detail = where + " is truncated";
                return false;
            }

            const char *block_data = data + offset;

            if (stored_checksum != 0)
            {
                const uint32_t partial = cab_checksum(reinterpret_cast<const unsigned char *>(block_data), comp_size, 0);
                const uint32_t computed =
                    cab_checksum(reinterpret_cast<const unsigned char *>(data + header_offset + 4), 4, partial);
                if (computed != stored_checksum)
                {
                    detail = where + " has a wrong checksum";
                    return false;
                }
            }

            if (folder.compress_type == COMPRESS_NONE)
            {
                if (comp_size != uncomp_size)
                {
                    detail = where + " stores " + std::to_string(comp_size) + " bytes but declares " +
                             std::to_string(uncomp_size);
                    return false;
                }
                result.insert(result.end(), block_data, block_data + comp_size);
            }
            else
            {
                if (comp_size < 2 || block_data[0] != 'C' || block_data[1] != 'K')
                {
                    detail = where + " lacks the MSZIP signature";
                    return false;
                }

                const char *deflate_data = block_data + 2;
                size_t deflate_len = comp_size - 2;
                size_t out_size = uncomp_size;
                std::vector<char> buf(out_size > 0 ? out_size : 1);

                z_stream strm;
                memset(&strm, 0, sizeof(strm));
                if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
                {
                    detail = "the inflater could not be initialised";
                    return false;
                }

                if (window_size > 0)
                    inflateSetDictionary(&strm, window, (uInt)window_size);

                strm.next_in = (Bytef *)deflate_data;
                strm.avail_in = (uInt)deflate_len;
                strm.next_out = (Bytef *)buf.data();
                strm.avail_out = (uInt)out_size;

                int ret = inflate(&strm, Z_SYNC_FLUSH);
                size_t produced = out_size - strm.avail_out;
                inflateEnd(&strm);

                // Z_OK / Z_BUF_ERROR are fine when the buffer is exactly full (the end-of-block code of the last
                // symbol has not been read yet); anything that is an error, or a block that falls short of or
                // runs past cbUncomp, is corruption.
                const bool stream_ok = (ret == Z_STREAM_END) || ret == Z_OK || ret == Z_BUF_ERROR;
                if (!stream_ok)
                {
                    detail = where + " does not inflate (zlib " + std::to_string(ret) + ")";
                    return false;
                }
                if (produced != out_size)
                {
                    detail = where + " inflates to " + std::to_string(produced) + " bytes but declares " +
                             std::to_string(out_size);
                    return false;
                }

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

            offset += comp_size;
        }

        return true;
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

    CabExtractResult extract_cab_member(const char *data, size_t length,
                                        const std::string &filename)
    {
        CabExtractResult out;
        CabHeader hdr;
        if (!parse_header(data, length, hdr))
        {
            out.status = CabStatus::NotACabinet;
            out.detail = "not a cabinet file";
            return out;
        }

        size_t folder_offset = header_end_offset(data, length, hdr);
        if (folder_offset == 0)
        {
            out.status = CabStatus::Corrupt;
            out.detail = "the cabinet header is truncated";
            return out;
        }

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
        {
            out.status = CabStatus::NotFound;
            out.detail = "no member named " + filename;
            return out;
        }
        if (target->folder_index >= folders.size())
        {
            out.status = CabStatus::Corrupt;
            out.detail = "the member points at a folder that does not exist";
            return out;
        }

        std::vector<char> folder_data;
        if (!decompress_folder(data, length, folders[target->folder_index], hdr.data_reserve_size,
                               folder_data, out.detail))
        {
            out.status = CabStatus::Corrupt;
            return out;
        }

        const uint64_t start = target->offset_in_folder;
        const uint64_t end = start + target->uncompressed_size;
        if (end > folder_data.size())
        {
            out.status = CabStatus::Corrupt;
            out.detail = "the member extends past the end of its folder";
            return out;
        }

        out.status = CabStatus::Ok;
        out.data.assign(folder_data.begin() + (size_t)start, folder_data.begin() + (size_t)end);
        return out;
    }

    std::vector<char> extract_cab_file(const char *data, size_t length,
                                       const std::string &filename)
    {
        CabExtractResult result = extract_cab_member(data, length, filename);
        if (result.status != CabStatus::Ok)
            return {};
        return std::move(result.data);
    }

}
