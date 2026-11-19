// Layer: Import (pure C++)
// The file operations of the OSP import (image staging, temporary extraction folder). They sit behind an interface so the
// staging rules can be tested with a file system that fails on demand (a full disk), exactly like the Windows tests patch
// shutil.copy2. StdOspFileSystem is the real implementation (std::filesystem, UTF-8 paths).
//
// Every function reports a failure with false / a null pointer: the Windows code raises (OSError) and the whole import fails.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ost_import
{
    class OspFileReader
    {
    public:
        virtual ~OspFileReader() = default;
        // Reads up to `capacity` bytes; `got` is 0 at the end of the file. false: the read failed.
        virtual bool read(char *buffer, size_t capacity, size_t &got) = 0;
    };

    class OspFileSystem
    {
    public:
        virtual ~OspFileSystem() = default;

        // Path.exists(): a file or a directory.
        virtual bool exists(const std::string &path) = 0;
        // Path.is_file(): a regular file (a directory is not).
        virtual bool is_file(const std::string &path) = 0;
        virtual bool file_size(const std::string &path, uint64_t &size) = 0;
        // nullptr when the file cannot be opened.
        virtual std::unique_ptr<OspFileReader> open_read(const std::string &path) = 0;
        virtual bool read_file(const std::string &path, std::vector<char> &bytes) = 0;
        // Path.mkdir(parents=True, exist_ok=True)
        virtual bool make_directories(const std::string &path) = 0;
        // shutil.copy2(source, destination): the destination is overwritten.
        virtual bool copy_file(const std::string &from, const std::string &to) = 0;
        // tempfile.mkdtemp(prefix=prefix, dir=parent): a new, empty, uniquely named directory below `parent` (created when needed).
        virtual bool make_temp_directory(const std::string &parent, const std::string &prefix, std::string &path) = 0;
        // shutil.rmtree; a path that does not exist is a success.
        virtual bool remove_all(const std::string &path) = 0;
    };

    class StdOspFileSystem : public OspFileSystem
    {
    public:
        bool exists(const std::string &path) override;
        bool is_file(const std::string &path) override;
        bool file_size(const std::string &path, uint64_t &size) override;
        std::unique_ptr<OspFileReader> open_read(const std::string &path) override;
        bool read_file(const std::string &path, std::vector<char> &bytes) override;
        bool make_directories(const std::string &path) override;
        bool copy_file(const std::string &from, const std::string &to) override;
        bool make_temp_directory(const std::string &parent, const std::string &prefix, std::string &path) override;
        bool remove_all(const std::string &path) override;
    };
}
