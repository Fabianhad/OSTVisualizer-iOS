// Layer: Import (pure C++)
// See OspFileSystem.hpp.
#include "OspFileSystem.hpp"

#include <filesystem>
#include <fstream>
#include <random>

namespace ost_import
{
    namespace
    {
        namespace fs = std::filesystem;

        fs::path to_path(const std::string &utf8)
        {
            return fs::u8path(utf8);
        }

        std::string to_utf8(const fs::path &path)
        {
            return path.generic_u8string(); // C++17: std::string
        }

        class StreamReader : public OspFileReader
        {
        public:
            explicit StreamReader(const fs::path &path) : stream_(path, std::ios::binary) {}
            bool is_open() const { return stream_.is_open(); }
            bool read(char *buffer, size_t capacity, size_t &got) override
            {
                got = 0;
                stream_.read(buffer, static_cast<std::streamsize>(capacity));
                got = static_cast<size_t>(stream_.gcount());
                // reaching the end is not an error (a short read sets eofbit and failbit); any other failure is
                if (stream_.bad() || (stream_.fail() && !stream_.eof()))
                    return false;
                return true;
            }

        private:
            std::ifstream stream_;
        };
    }

    bool StdOspFileSystem::exists(const std::string &path)
    {
        std::error_code ec;
        return fs::exists(to_path(path), ec) && !ec;
    }

    bool StdOspFileSystem::is_file(const std::string &path)
    {
        std::error_code ec;
        return fs::is_regular_file(to_path(path), ec) && !ec;
    }

    bool StdOspFileSystem::file_size(const std::string &path, uint64_t &size)
    {
        std::error_code ec;
        const std::uintmax_t value = fs::file_size(to_path(path), ec);
        if (ec)
            return false;
        size = static_cast<uint64_t>(value);
        return true;
    }

    std::unique_ptr<OspFileReader> StdOspFileSystem::open_read(const std::string &path)
    {
        std::unique_ptr<StreamReader> reader(new StreamReader(to_path(path)));
        if (!reader->is_open())
            return nullptr;
        return std::unique_ptr<OspFileReader>(reader.release());
    }

    bool StdOspFileSystem::read_file(const std::string &path, std::vector<char> &bytes)
    {
        bytes.clear();
        std::error_code ec;
        const std::uintmax_t size = fs::file_size(to_path(path), ec);
        if (ec)
            return false;
        std::ifstream stream(to_path(path), std::ios::binary);
        if (!stream.is_open())
            return false;
        bytes.resize(static_cast<size_t>(size));
        if (size > 0)
        {
            stream.read(bytes.data(), static_cast<std::streamsize>(size));
            if (static_cast<std::uintmax_t>(stream.gcount()) != size)
            {
                bytes.clear();
                return false;
            }
        }
        return true;
    }

    bool StdOspFileSystem::make_directories(const std::string &path)
    {
        std::error_code ec;
        const fs::path dir = to_path(path);
        fs::create_directories(dir, ec);
        if (ec)
            return false;
        return fs::is_directory(dir, ec) && !ec;
    }

    bool StdOspFileSystem::copy_file(const std::string &from, const std::string &to)
    {
        std::error_code ec;
        fs::copy_file(to_path(from), to_path(to), fs::copy_options::overwrite_existing, ec);
        return !ec;
    }

    bool StdOspFileSystem::make_temp_directory(const std::string &parent, const std::string &prefix, std::string &path)
    {
        std::error_code ec;
        const fs::path base = to_path(parent);
        fs::create_directories(base, ec);
        if (ec)
            return false;
        std::random_device device;
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            char token[24];
            const unsigned long long value = (static_cast<unsigned long long>(device()) << 32) | device();
            static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789_";
            std::string name = prefix;
            unsigned long long rest = value;
            for (int i = 0; i < 8; ++i)
            {
                token[i] = alphabet[rest % (sizeof(alphabet) - 1)];
                rest /= (sizeof(alphabet) - 1);
            }
            token[8] = '\0';
            name += token;
            const fs::path candidate = base / to_path(name);
            if (fs::create_directory(candidate, ec) && !ec)
            {
                path = to_utf8(candidate);
                return true;
            }
            if (ec)
                return false;
        }
        return false;
    }

    bool StdOspFileSystem::remove_all(const std::string &path)
    {
        std::error_code ec;
        fs::remove_all(to_path(path), ec);
        return !ec;
    }
}
