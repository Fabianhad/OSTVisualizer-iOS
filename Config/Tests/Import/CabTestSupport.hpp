// CabTestSupport.hpp - builds Microsoft Cabinet archives for the CAB reader tests.
//
// Layout (MS-CAB): CFHEADER (36 bytes), one CFFOLDER (8), the CFFILE entries, then the CFDATA blocks of the folder.
// Stored folders (type 0) copy the bytes; MSZIP folders (type 1) compress each block of up to 32768 bytes as
// "CK" + a raw deflate stream that uses the previous block's data as its dictionary. The CFDATA checksum is optional
// (0 = none) and, when requested, follows the CSUMCOMPUTE algorithm of the specification.
//
// The shape of the stored variant is tests/helpers/import_workflow.py::_write_legacy_ansi_cab (checksum 0, one block).
#pragma once

#include <zlib.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ost_itest
{
    struct CabMember
    {
        std::string name;
        std::string content;
    };

    struct CabBuildOptions
    {
        bool mszip = false;
        bool checksums = false;       // write real CFDATA checksums instead of 0
        size_t block_size = 32768;    // uncompressed bytes per CFDATA block
        int corrupt_block = -1;       // >= 0: damage this block after building (see corrupt_mode)
        enum CorruptMode
        {
            None,
            DeflateBytes,   // overwrite the deflate payload with 0xFF (invalid block type)
            FlipPayloadByte // flip one payload byte (a checksum, when present, no longer matches)
        } corrupt_mode = None;
    };

    inline void put16(std::string &out, uint32_t v)
    {
        out += static_cast<char>(v & 0xFF);
        out += static_cast<char>((v >> 8) & 0xFF);
    }

    inline void put32(std::string &out, uint32_t v)
    {
        put16(out, v & 0xFFFF);
        put16(out, (v >> 16) & 0xFFFF);
    }

    // CSUMCOMPUTE of the cabinet specification: the data is XORed as little-endian 32-bit words; the 1-3 trailing bytes
    // are combined with the FIRST trailing byte in the highest position (case 3: byte0 << 16, byte1 << 8, byte2).
    inline uint32_t spec_checksum(const unsigned char *data, size_t bytes, uint32_t seed)
    {
        uint32_t csum = seed;
        size_t words = bytes / 4;
        for (size_t i = 0; i < words; ++i, data += 4)
            csum ^= (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
        uint32_t ul = 0;
        switch (bytes % 4)
        {
        case 3:
            ul |= (uint32_t)(*data++) << 16;
            [[fallthrough]];
        case 2:
            ul |= (uint32_t)(*data++) << 8;
            [[fallthrough]];
        case 1:
            ul |= (uint32_t)(*data++);
            break;
        default:
            break;
        }
        return csum ^ ul;
    }

    struct CabBlock
    {
        std::string payload;      // as stored (with "CK" for MSZIP)
        uint16_t uncompressed = 0;
    };

    inline std::string deflate_block(const std::string &chunk, const std::string &dictionary)
    {
        z_stream strm;
        std::memset(&strm, 0, sizeof(strm));
        deflateInit2(&strm, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        if (!dictionary.empty())
            deflateSetDictionary(&strm, reinterpret_cast<const Bytef *>(dictionary.data()), static_cast<uInt>(dictionary.size()));
        std::string out(deflateBound(&strm, static_cast<uLong>(chunk.size())) + 16, '\0');
        strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(chunk.data()));
        strm.avail_in = static_cast<uInt>(chunk.size());
        strm.next_out = reinterpret_cast<Bytef *>(&out[0]);
        strm.avail_out = static_cast<uInt>(out.size());
        deflate(&strm, Z_FINISH);
        out.resize(out.size() - strm.avail_out);
        deflateEnd(&strm);
        return out;
    }

    inline std::string build_cab(const std::vector<CabMember> &members, const CabBuildOptions &options)
    {
        std::string folder_data;
        std::string file_table;
        for (const CabMember &m : members)
        {
            put32(file_table, static_cast<uint32_t>(m.content.size()));
            put32(file_table, static_cast<uint32_t>(folder_data.size()));
            put16(file_table, 0); // iFolder
            put16(file_table, 0); // date
            put16(file_table, 0); // time
            put16(file_table, 0); // attribs
            file_table += m.name;
            file_table += '\0';
            folder_data += m.content;
        }

        std::vector<CabBlock> blocks;
        for (size_t pos = 0; pos < folder_data.size() || (pos == 0 && blocks.empty()); pos += options.block_size)
        {
            CabBlock block;
            const std::string chunk = folder_data.substr(pos, options.block_size);
            block.uncompressed = static_cast<uint16_t>(chunk.size());
            if (options.mszip)
            {
                const std::string history = folder_data.substr(0, pos);
                const std::string dictionary = history.size() > 32768 ? history.substr(history.size() - 32768) : history;
                block.payload = "CK" + deflate_block(chunk, dictionary);
            }
            else
            {
                block.payload = chunk;
            }
            blocks.push_back(block);
            if (folder_data.empty())
                break;
        }

        std::string data_section;
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            CabBlock &block = blocks[i];
            if (options.corrupt_block == static_cast<int>(i))
            {
                if (options.corrupt_mode == CabBuildOptions::DeflateBytes)
                    std::memset(&block.payload[options.mszip ? 2 : 0], 0xFF, block.payload.size() - (options.mszip ? 2 : 0));
                else if (options.corrupt_mode == CabBuildOptions::FlipPayloadByte)
                    block.payload[block.payload.size() / 2] ^= 0x01;
            }
        }
        // checksums are computed over the ORIGINAL payload (a damaged block keeps the checksum of the good one)
        std::vector<CabBlock> good = blocks;
        for (size_t i = 0; i < good.size(); ++i)
        {
            const std::string chunk = folder_data.substr(i * options.block_size, options.block_size);
            if (options.mszip)
            {
                const std::string history = folder_data.substr(0, i * options.block_size);
                const std::string dictionary = history.size() > 32768 ? history.substr(history.size() - 32768) : history;
                good[i].payload = "CK" + deflate_block(chunk, dictionary);
            }
            else
            {
                good[i].payload = chunk;
            }
        }
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            uint32_t csum = 0;
            if (options.checksums)
            {
                std::string header;
                put16(header, static_cast<uint32_t>(good[i].payload.size()));
                put16(header, blocks[i].uncompressed);
                csum = spec_checksum(reinterpret_cast<const unsigned char *>(header.data()), 4,
                                     spec_checksum(reinterpret_cast<const unsigned char *>(good[i].payload.data()),
                                                   good[i].payload.size(), 0));
            }
            put32(data_section, csum);
            put16(data_section, static_cast<uint32_t>(blocks[i].payload.size()));
            put16(data_section, blocks[i].uncompressed);
            data_section += blocks[i].payload;
        }

        const uint32_t files_offset = 36 + 8;
        const uint32_t data_offset = files_offset + static_cast<uint32_t>(file_table.size());
        const uint32_t total = data_offset + static_cast<uint32_t>(data_section.size());

        std::string out = "MSCF";
        put32(out, 0);
        put32(out, total);
        put32(out, 0);
        put32(out, files_offset);
        put32(out, 0);
        out += static_cast<char>(3); // versionMinor
        out += static_cast<char>(1); // versionMajor
        put16(out, 1);               // cFolders
        put16(out, static_cast<uint32_t>(members.size()));
        put16(out, 0);               // flags
        put16(out, 12345);           // setID
        put16(out, 0);               // iCabinet
        put32(out, data_offset);
        put16(out, static_cast<uint32_t>(blocks.size()));
        put16(out, options.mszip ? 1 : 0);
        out += file_table;
        out += data_section;
        return out;
    }

    // Deterministic text for the makecab fixture: must stay identical to scratchpad make_cab_fixture.py lcg_text().
    inline std::string lcg_text(uint32_t seed, size_t size)
    {
        static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz ";
        std::string out;
        uint64_t x = seed;
        for (size_t i = 0; i < size; ++i)
        {
            x = (x * 1103515245ull + 12345ull) & 0x7FFFFFFFull;
            out += alphabet[(x >> 16) % 27];
        }
        return out;
    }

    // Pseudo-random bytes (incompressible, any value), for stored and MSZIP blocks with binary content.
    inline std::string noise(uint32_t seed, size_t size)
    {
        std::string out;
        uint64_t x = seed;
        for (size_t i = 0; i < size; ++i)
        {
            x = (x * 6364136223846793005ull + 1442695040888963407ull);
            out += static_cast<char>((x >> 33) & 0xFF);
        }
        return out;
    }
}
