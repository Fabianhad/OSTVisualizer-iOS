// Layer: Import (pure C++)
// SHA-1 and SHA-256 (FIPS 180-4), no dependencies. The OSP image staging names a packaged image by the SHA-1 of its member name
// (the directory below Images/) and, when the destination name is taken by different content, by the SHA-256 of the file's
// content (osp_importer._image_destination_relative_path, _non_conflicting_image_destination): the digests are part of the
// resulting file names, so they must be exactly hashlib's.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace ost_import
{
    namespace digest
    {
        class Sha1
        {
        public:
            Sha1();
            void update(const void *data, size_t length);
            // Lower-case hexadecimal digest of everything fed so far (the object stays usable).
            std::string hex() const;

        private:
            void process_block(const unsigned char *block);
            uint32_t state_[5];
            uint64_t total_bytes_ = 0;
            unsigned char buffer_[64];
            size_t buffered_ = 0;
        };

        class Sha256
        {
        public:
            Sha256();
            void update(const void *data, size_t length);
            std::string hex() const;

        private:
            void process_block(const unsigned char *block);
            uint32_t state_[8];
            uint64_t total_bytes_ = 0;
            unsigned char buffer_[64];
            size_t buffered_ = 0;
        };

        std::string sha1_hex(const std::string &data);
        std::string sha256_hex(const std::string &data);
    }
}
