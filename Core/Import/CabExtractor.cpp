// Layer: Import (pure C++)
// Minimal CAB (Microsoft Cabinet) archive reader with MSZIP decompression.
// Reference: Microsoft Cabinet File Format specification.
#include "CabExtractor.hpp"
#include "CabMemberName.hpp"
#include <zlib.h>
#include <atomic>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
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

    static std::atomic<unsigned long long> g_blocks_decoded(0);

    unsigned long long cab_blocks_decoded()
    {
        return g_blocks_decoded.load();
    }

    static const uint32_t CAB_SIGNATURE = 0x4643534D; // "MSCF"
    static const size_t CFHEADER_MIN_SIZE = 36;
    static const size_t CFFOLDER_SIZE = 8;
    static const uint16_t COMPRESS_NONE = 0x0000;
    static const uint16_t COMPRESS_MSZIP = 0x0001;
    static const size_t MSZIP_WINDOW = 32768;
    // The Windows cabinet library reads every member name into a 256 byte buffer: a name of 256 bytes or more (before
    // decoding) fails the cabinet.
    static const size_t CAB_MAX_NAME_BYTES = 255;

    static std::string ascii_lower(const std::string &text)
    {
        std::string out = text;
        for (char &c : out)
        {
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
        }
        return out;
    }

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
        // The Windows cabinet library (FDI) reads only format version 1.3: versionMinor 3 at offset 24, versionMajor 1 at
        // offset 25 (every other version failed in a probe with expand.exe).
        if (static_cast<unsigned char>(data[24]) != 3 || static_cast<unsigned char>(data[25]) != 1)
            return false;

        hdr.cabinet_size = read_u32(data + 8);
        hdr.files_offset = read_u32(data + 16);
        hdr.folder_count = read_u16(data + 26);
        hdr.file_count = read_u16(data + 28);
        hdr.flags = read_u16(data + 30);
        hdr.folder_reserve_size = 0;
        hdr.data_reserve_size = 0;

        // cbCFHeader (the header reserve) is at most 60000 bytes: a larger one is not a cabinet for the Windows library
        // (measured: 60000 extracts, 60001 and 65535 fail with "not a cabinet").
        if ((hdr.flags & 0x0004) && length >= CFHEADER_MIN_SIZE + 2 && read_u16(data + CFHEADER_MIN_SIZE) > 60000)
            return false;

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

        // The previous / next cabinet and disk names: each at most 255 bytes and terminated (the Windows library fails the
        // cabinet at 256 bytes or an unterminated name; measured with prev/next names of 255 and 256 bytes).
        auto skip_name = [&](size_t &position) -> bool
        {
            size_t end = position;
            while (end < length && data[end] != '\0' && end - position < 256)
                end++;
            if (end >= length || data[end] != '\0' || end - position > 255)
                return false;
            position = end + 1;
            return true;
        };

        if (hdr.flags & 0x0001)
        {
            if (!skip_name(off) || !skip_name(off))
                return 0;
        }

        if (hdr.flags & 0x0002)
        {
            if (!skip_name(off) || !skip_name(off))
                return 0;
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

    enum class TableStatus
    {
        Complete,
        Truncated, // an entry is cut short: fewer than `count` entries could be read
        BadName    // a name is 256 bytes or longer, or the name of a listed member does not decode
    };

    // The member table, as the Windows cabinet library reports it: the members of a folder that exists (a member of another
    // folder is neither reported nor decoded, so an undecodable name there is no failure either), with the name decoded to
    // UTF-8 by the rule of cab_compressor.cpp decode_cab_member (IMP-18) and '\' turned into '/'. The library reads every
    // entry's name into a 256 byte buffer, so a name of 256 bytes or more fails the table whether it is reported or not.
    // A name that runs to the end of the data without a NUL is the rest of the data (the library accepts that too).
    static TableStatus parse_files(const char *data, size_t length, size_t offset, uint16_t count, uint16_t folder_count,
                                   std::vector<CabEntry> &entries)
    {
        entries.clear();
        entries.reserve(count);

        for (uint16_t i = 0; i < count; i++)
        {
            // An entry is its 16 byte header and a name of at least one byte (the library reads the name from the data after
            // the header and fails when nothing is left: a table cut right after a header is corrupt).
            if (offset > length || length - offset < 17)
                return TableStatus::Truncated;

            CabEntry e;
            e.uncompressed_size = read_u32(data + offset);
            e.offset_in_folder = read_u32(data + offset + 4);
            e.folder_index = read_u16(data + offset + 8);
            e.attribs = read_u16(data + offset + 14);

            const size_t name_start = offset + 16;
            size_t name_end = name_start;
            while (name_end < length && name_end - name_start <= CAB_MAX_NAME_BYTES && data[name_end] != '\0')
                name_end++;
            if (name_end - name_start > CAB_MAX_NAME_BYTES)
                return TableStatus::BadName;
            offset = name_end + 1;

            if (e.folder_index >= folder_count)
                continue;

            std::string utf8;
            if (!decode_cab_member_name(std::string(data + name_start, name_end - name_start), e.attribs, utf8))
                return TableStatus::BadName;
            e.stored_name = utf8;
            for (auto &ch : utf8)
            {
                if (ch == '\\')
                    ch = '/';
            }
            e.name = std::move(utf8);
            entries.push_back(std::move(e));
        }

        return TableStatus::Complete;
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

    // Decodes the CFDATA blocks of ONE folder one after the other, handing out the bytes of one block at a time (the MSZIP
    // dictionary of a block is the end of the previous one, so a folder can only be read from its start). The Windows importer
    // extracts through the Windows cabinet library (FDICopy), where any corrupt block makes the whole extraction fail, so
    // nothing here is skipped or repaired: a truncated block, a CFDATA checksum that does not match (a checksum of 0 means
    // "none"), an MSZIP block without the "CK" signature or that does not inflate, and a block that does not produce exactly
    // cbUncomp bytes all fail the folder.
    class FolderStream
    {
    public:
        enum class Step
        {
            Block, // `bytes` / `count` are the next block (valid until the next call)
            End,   // no more blocks
            Error  // `detail` says why
        };

        FolderStream(const char *data, size_t length, const CabFolder &folder, uint8_t data_reserve_size)
            : data_(data), length_(length), folder_(folder), data_reserve_size_(data_reserve_size), offset_(folder.data_offset),
              window_(MSZIP_WINDOW)
        {
        }

        Step next(const char *&bytes, size_t &count, std::string &detail)
        {
            if (!type_checked_)
            {
                type_checked_ = true;
                if (folder_.compress_type != COMPRESS_NONE && folder_.compress_type != COMPRESS_MSZIP)
                {
                    detail = "unsupported compression type " + std::to_string(folder_.compress_type);
                    return Step::Error;
                }
            }
            if (block_ >= folder_.data_count)
                return Step::End;
            const uint16_t block = block_++;

            const std::string where = "CFDATA block " + std::to_string(block + 1) + " of " + std::to_string(folder_.data_count);
            if (offset_ > length_ || length_ - offset_ < 8 + (size_t)data_reserve_size_)
            {
                detail = where + " is truncated";
                return Step::Error;
            }

            const uint32_t stored_checksum = read_u32(data_ + offset_);
            const uint16_t comp_size = read_u16(data_ + offset_ + 4);
            const uint16_t uncomp_size = read_u16(data_ + offset_ + 6);
            const size_t header_offset = offset_;
            offset_ += 8 + data_reserve_size_;

            if (length_ - offset_ < comp_size)
            {
                detail = where + " is truncated";
                return Step::Error;
            }

            const char *block_data = data_ + offset_;
            offset_ += comp_size;
            ++g_blocks_decoded;

            if (stored_checksum != 0)
            {
                // The checksum covers the payload, then cbData, cbUncomp and the per-block reserve bytes (abReserve sits
                // between cbUncomp and the payload; the Windows library rejects a checksum that leaves the reserve out).
                const uint32_t partial = cab_checksum(reinterpret_cast<const unsigned char *>(block_data), comp_size, 0);
                const uint32_t computed = cab_checksum(reinterpret_cast<const unsigned char *>(data_ + header_offset + 4),
                                                       4 + (size_t)data_reserve_size_, partial);
                if (computed != stored_checksum)
                {
                    detail = where + " has a wrong checksum";
                    return Step::Error;
                }
            }

            if (folder_.compress_type == COMPRESS_NONE)
            {
                if (comp_size != uncomp_size)
                {
                    detail = where + " stores " + std::to_string(comp_size) + " bytes but declares " +
                             std::to_string(uncomp_size);
                    return Step::Error;
                }
                bytes = block_data;
                count = comp_size;
                return Step::Block;
            }

            if (comp_size < 2 || block_data[0] != 'C' || block_data[1] != 'K')
            {
                detail = where + " lacks the MSZIP signature";
                return Step::Error;
            }

            const char *deflate_data = block_data + 2;
            size_t deflate_len = comp_size - 2;
            size_t out_size = uncomp_size;
            buffer_.assign(out_size > 0 ? out_size : 1, 0);

            z_stream strm;
            memset(&strm, 0, sizeof(strm));
            if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
            {
                detail = "the inflater could not be initialised";
                return Step::Error;
            }

            if (window_size_ > 0)
                inflateSetDictionary(&strm, window_.data(), (uInt)window_size_);

            strm.next_in = (Bytef *)deflate_data;
            strm.avail_in = (uInt)deflate_len;
            strm.next_out = (Bytef *)buffer_.data();
            strm.avail_out = (uInt)out_size;

            int ret = inflate(&strm, Z_SYNC_FLUSH);
            size_t produced = out_size - strm.avail_out;
            // A block that holds MORE data than cbUncomp declares is corrupt: the Windows library fails it with a decompression
            // error (found by a differential fuzz after a flipped cbUncomp). Not for a block declared with the full 32768
            // bytes: its decompressor stops at its 32 KiB buffer and accepts whatever follows (measured), so there the bytes
            // after the declared size are ignored as before. When the buffer is exactly full and the stream has not ended,
            // ask for one more byte.
            bool overlong = false;
            if (out_size < MSZIP_WINDOW && produced == out_size && ret != Z_STREAM_END && (ret == Z_OK || ret == Z_BUF_ERROR))
            {
                unsigned char extra = 0;
                strm.next_out = &extra;
                strm.avail_out = 1;
                inflate(&strm, Z_SYNC_FLUSH);
                overlong = strm.avail_out == 0;
            }
            inflateEnd(&strm);
            if (overlong)
            {
                detail = where + " inflates to more than the " + std::to_string(out_size) + " bytes it declares";
                return Step::Error;
            }

            // Z_OK / Z_BUF_ERROR are fine when the buffer is exactly full (the end-of-block code of the last
            // symbol has not been read yet); anything that is an error, or a block that falls short of or
            // runs past cbUncomp, is corruption.
            const bool stream_ok = (ret == Z_STREAM_END) || ret == Z_OK || ret == Z_BUF_ERROR;
            if (!stream_ok)
            {
                detail = where + " does not inflate (zlib " + std::to_string(ret) + ")";
                return Step::Error;
            }
            if (produced != out_size)
            {
                detail = where + " inflates to " + std::to_string(produced) + " bytes but declares " +
                         std::to_string(out_size);
                return Step::Error;
            }

            if (produced >= MSZIP_WINDOW)
            {
                memcpy(window_.data(), buffer_.data() + produced - MSZIP_WINDOW, MSZIP_WINDOW);
                window_size_ = MSZIP_WINDOW;
            }
            else
            {
                size_t keep = (window_size_ + produced > MSZIP_WINDOW)
                                  ? MSZIP_WINDOW - produced
                                  : window_size_;
                if (keep > 0 && keep < window_size_)
                    memmove(window_.data(), window_.data() + window_size_ - keep, keep);
                memcpy(window_.data() + keep, buffer_.data(), produced);
                window_size_ = keep + produced;
            }

            bytes = buffer_.data();
            count = produced;
            return Step::Block;
        }

    private:
        const char *data_;
        size_t length_;
        CabFolder folder_;
        uint8_t data_reserve_size_;
        size_t offset_; // where the next CFDATA header starts
        uint16_t block_ = 0;
        bool type_checked_ = false;
        std::vector<unsigned char> window_; // the last (up to) 32 KiB of output: the MSZIP dictionary of the next block
        size_t window_size_ = 0;
        std::vector<char> buffer_; // the output of the current MSZIP block
    };

    // Decodes a whole folder into one buffer (what extract_cab_member needs): the memory of the folder, not of a block.
    static bool decompress_folder(const char *data, size_t length,
                                  const CabFolder &folder,
                                  uint8_t data_reserve_size,
                                  std::vector<char> *result, // nullptr: decode and check only
                                  uint64_t &total,           // bytes the folder decoded to
                                  std::string &detail)
    {
        if (result)
            result->clear();
        total = 0;
        FolderStream stream(data, length, folder, data_reserve_size);
        for (;;)
        {
            const char *bytes = nullptr;
            size_t count = 0;
            const FolderStream::Step step = stream.next(bytes, count, detail);
            if (step == FolderStream::Step::Error)
                return false;
            if (step == FolderStream::Step::End)
                return true;
            total += count;
            if (result)
                result->insert(result->end(), bytes, bytes + count);
        }
    }

    // Header, folder table and member table of a cabinet. false: `failure` (status and detail) says why.
    struct CabTable
    {
        CabHeader header;
        std::vector<CabFolder> folders;
        std::vector<CabEntry> files; // the members the Windows library reports: those of a folder that exists
    };

    static bool read_table(const char *data, size_t length, CabTable &table, CabExtractResult &failure)
    {
        if (!parse_header(data, length, table.header))
        {
            failure.status = CabStatus::NotACabinet;
            failure.detail = "not a cabinet file";
            return false;
        }
        const size_t folder_offset = header_end_offset(data, length, table.header);
        if (folder_offset == 0)
        {
            failure.status = CabStatus::Corrupt;
            failure.detail = "the cabinet header is truncated";
            return false;
        }
        table.folders = parse_folders(data, length, folder_offset, table.header.folder_count, table.header.folder_reserve_size);
        const TableStatus status = parse_files(data, length, table.header.files_offset, table.header.file_count,
                                               table.header.folder_count, table.files);
        if (status == TableStatus::BadName)
        {
            failure.status = CabStatus::Corrupt;
            failure.detail = "a member name does not decode or is longer than 255 bytes";
            return false;
        }
        // The library reads a folder record when a member needs it (run_extraction fails then): a cabinet cut inside the
        // folder table still extracts when its members lie in the folders that are there (measured: 0 files cut inside the
        // folder table gives "OK 0").
        if (status == TableStatus::Truncated)
        {
            failure.status = CabStatus::Corrupt;
            failure.detail = "the file table of the cabinet is truncated";
            return false;
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

        // Like the Windows listing: any problem with the table (cut short, a name that does not decode or is too long) and
        // nothing is listed.
        std::vector<CabEntry> entries;
        if (parse_files(data, length, hdr.files_offset, hdr.file_count, hdr.folder_count, entries) != TableStatus::Complete)
            return {};
        return entries;
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
        std::vector<CabEntry> files;
        if (parse_files(data, length, hdr.files_offset, hdr.file_count, hdr.folder_count, files) == TableStatus::BadName)
        {
            out.status = CabStatus::Corrupt;
            out.detail = "a member name does not decode or is longer than 255 bytes";
            return out;
        }

        const CabEntry *target = nullptr;
        for (const auto &entry : files)
        {
            if (ascii_lower(entry.name) == ascii_lower(filename))
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
        uint64_t folder_total = 0;
        if (!decompress_folder(data, length, folders[target->folder_index], hdr.data_reserve_size,
                               &folder_data, folder_total, out.detail))
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

    // The single pass (IMP-M03). The Windows library (FDICopy, measured with a native probe) walks the file table in order;
    // for a member of a folder that exists it
    //   - creates the output (a zero-length member: nothing else happens, no block is read, its offset is never looked at),
    //   - otherwise reads the folder forward to the member's offset and copies its bytes; when the offset lies behind the
    //     position already reached (descending offsets, overlap, another folder in between) the folder is read again from its
    //     first block,
    //   - and never reads a block beyond the one holding the last byte it copied, so damage in later blocks is not noticed.
    // The same here, with one block in memory at a time. A member that runs past the end of its folder fails the extraction
    // after the bytes that exist were delivered.
    static CabExtractResult run_extraction(const char *data, size_t length, const CabTable &table, CabMemberSink &sink)
    {
        CabExtractResult out;
        std::unique_ptr<FolderStream> stream;
        size_t stream_folder = 0;
        const char *block = nullptr;
        size_t block_size = 0, block_pos = 0;
        uint64_t position = 0; // folder offset of the next unread byte of the stream

        auto fail = [&out](const std::string &detail) -> CabExtractResult &
        {
            out.status = CabStatus::Corrupt;
            out.detail = detail;
            return out;
        };
        // The next block of the stream; false = `out` holds the failure.
        auto fetch = [&]() -> bool
        {
            std::string detail;
            switch (stream->next(block, block_size, detail))
            {
            case FolderStream::Step::Block:
                block_pos = 0;
                return true;
            case FolderStream::Step::End:
                fail("a member extends past the end of its folder");
                return false;
            default:
                fail(detail);
                return false;
            }
        };

        for (const CabEntry &entry : table.files)
        {
            if (entry.folder_index >= table.header.folder_count)
                continue; // no such folder: the library skips the member
            if (entry.folder_index >= table.folders.size())
                return fail("the folder table of the cabinet is truncated");
            const CabMemberAction action = sink.begin_member(entry);
            if (action == CabMemberAction::Abort)
                return fail("aborted by the sink");
            const bool deliver = action == CabMemberAction::Extract;

            if (entry.uncompressed_size > 0)
            {
                if (!stream || stream_folder != entry.folder_index || entry.offset_in_folder < position)
                {
                    stream.reset(new FolderStream(data, length, table.folders[entry.folder_index], table.header.data_reserve_size));
                    stream_folder = entry.folder_index;
                    block_size = block_pos = 0;
                    position = 0;
                }
                while (position < entry.offset_in_folder) // read up to the member
                {
                    if (block_pos == block_size && !fetch())
                        return out;
                    const uint64_t skip = std::min<uint64_t>(block_size - block_pos, entry.offset_in_folder - position);
                    block_pos += static_cast<size_t>(skip);
                    position += skip;
                }
                uint64_t remaining = entry.uncompressed_size;
                while (remaining > 0)
                {
                    if (block_pos == block_size && !fetch())
                        return out;
                    const size_t take = static_cast<size_t>(std::min<uint64_t>(block_size - block_pos, remaining));
                    if (deliver && !sink.write(block + block_pos, take))
                        return fail("write failed");
                    block_pos += take;
                    position += take;
                    remaining -= take;
                }
            }
            if (deliver && !sink.end_member())
                return fail("write failed");
        }
        out.status = CabStatus::Ok;
        return out;
    }

    CabExtractResult extract_cab(const char *data, size_t length, CabMemberSink &sink)
    {
        CabTable table;
        CabExtractResult failure;
        if (!read_table(data, length, table, failure))
            return failure;
        return run_extraction(data, length, table, sink);
    }

    namespace
    {
        class DiscardSink : public CabMemberSink
        {
        public:
            CabMemberAction begin_member(const CabEntry &) override { return CabMemberAction::Skip; }
            bool write(const char *, size_t) override { return true; }
            bool end_member() override { return true; }
        };

        // Writes every member below a directory, straight from the decoded blocks.
        class DirectorySink : public CabMemberSink
        {
        public:
            explicit DirectorySink(const std::string &directory) : directory_(std::filesystem::u8path(directory)) {}

            const std::string &error() const { return error_; }

            CabMemberAction begin_member(const CabEntry &entry) override
            {
                if (file_.is_open())
                    file_.close();
                const std::filesystem::path path = directory_ / std::filesystem::u8path(entry.name);
                std::error_code ec;
                std::filesystem::create_directories(path.parent_path(), ec);
                if (!std::filesystem::is_directory(path.parent_path(), ec))
                {
                    error_ = "cannot create the directory of " + entry.name;
                    return CabMemberAction::Abort;
                }
                file_.clear();
                file_.open(path, std::ios::binary | std::ios::trunc);
                if (!file_)
                {
                    error_ = "cannot create " + entry.name;
                    return CabMemberAction::Abort;
                }
                return CabMemberAction::Extract;
            }
            bool write(const char *bytes, size_t length) override
            {
                file_.write(bytes, static_cast<std::streamsize>(length));
                if (!file_)
                    error_ = "cannot write the member";
                return static_cast<bool>(file_);
            }
            bool end_member() override
            {
                file_.close();
                if (file_.fail())
                    error_ = "cannot write the member";
                return !file_.fail();
            }

        private:
            std::filesystem::path directory_;
            std::ofstream file_;
            std::string error_;
        };
    }

    CabExtractResult verify_cab(const char *data, size_t length)
    {
        DiscardSink sink;
        return extract_cab(data, length, sink);
    }

    CabExtractResult extract_cab_to_directory(const char *data, size_t length, const std::string &directory)
    {
        CabTable table;
        CabExtractResult failure;
        if (!read_table(data, length, table, failure))
            return failure;
        // Windows osp_importer._inspect_package: every name is judged before anything is created or extracted.
        for (const CabEntry &entry : table.files)
        {
            if (!is_safe_cab_member_name(entry.name))
            {
                failure.status = CabStatus::UnsafeName;
                failure.detail = unsafe_member_path_message(entry.stored_name);
                return failure;
            }
        }
        DirectorySink sink(directory);
        CabExtractResult result = run_extraction(data, length, table, sink);
        if (result.status != CabStatus::Ok && !sink.error().empty())
            result.detail = sink.error();
        return result;
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
