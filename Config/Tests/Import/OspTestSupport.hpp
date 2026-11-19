// OspTestSupport.hpp - helpers of the OSP package tests (Core/Import/OspPackage.hpp): scratch folders in the system temp dir, a log
// capture, a file system that counts or fails copies (the Windows tests patch shutil.copy2), and builders of small OSP packages.
#pragma once

#include "TestHarness.hpp"

#include "CabTestSupport.hpp"
#include "Domain/CoreLog.hpp"
#include "Import/OspFileSystem.hpp"
#include "Import/OspPackage.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace ost_osptest
{
    inline const std::string &process_token()
    {
        static const std::string token = []
        {
            std::random_device device;
            char buffer[24];
            std::snprintf(buffer, sizeof(buffer), "%08x", static_cast<unsigned>(device()));
            return std::string(buffer);
        }();
        return token;
    }

    // A scratch folder below the system temp dir, removed (with everything in it) at the end of the scope.
    class TempTree
    {
    public:
        explicit TempTree(const std::string &name) : _root(ost_test::temp_path("osp_" + process_token() + "_" + name))
        {
            std::error_code ec;
            std::filesystem::remove_all(std::filesystem::u8path(_root), ec);
            std::filesystem::create_directories(std::filesystem::u8path(_root), ec);
        }
        ~TempTree()
        {
            std::error_code ec;
            std::filesystem::remove_all(std::filesystem::u8path(_root), ec);
        }
        TempTree(const TempTree &) = delete;
        TempTree &operator=(const TempTree &) = delete;

        const std::string &root() const { return _root; }
        std::string path(const std::string &relative) const { return _root + "/" + relative; }

        void write(const std::string &relative, const std::string &content) const
        {
            const std::filesystem::path file = std::filesystem::u8path(path(relative));
            std::error_code ec;
            std::filesystem::create_directories(file.parent_path(), ec);
            std::ofstream out(file, std::ios::binary);
            out.write(content.data(), static_cast<std::streamsize>(content.size()));
        }

        void make_directory(const std::string &relative) const
        {
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::u8path(path(relative)), ec);
        }

        bool exists(const std::string &relative) const
        {
            std::error_code ec;
            return std::filesystem::exists(std::filesystem::u8path(path(relative)), ec);
        }

        bool is_directory(const std::string &relative) const
        {
            std::error_code ec;
            return std::filesystem::is_directory(std::filesystem::u8path(path(relative)), ec);
        }

        std::string read(const std::string &relative) const
        {
            std::string out;
            ost_test::read_file(path(relative), out);
            return out;
        }

        // Every regular file below `relative`, as paths relative to the root ('/'), sorted.
        std::vector<std::string> files_below(const std::string &relative) const
        {
            std::vector<std::string> out;
            std::error_code ec;
            const std::filesystem::path base = std::filesystem::u8path(_root);
            const std::filesystem::path start = std::filesystem::u8path(path(relative));
            if (!std::filesystem::exists(start, ec))
                return out;
            for (std::filesystem::recursive_directory_iterator it(start, ec), end; it != end; it.increment(ec))
                if (it->is_regular_file(ec))
                    out.push_back(std::filesystem::relative(it->path(), base, ec).generic_string());
            std::sort(out.begin(), out.end());
            return out;
        }

        // Every entry (file or folder) directly in `relative`, names only, sorted.
        std::vector<std::string> entries_in(const std::string &relative) const
        {
            std::vector<std::string> out;
            std::error_code ec;
            const std::filesystem::path start = std::filesystem::u8path(path(relative));
            if (!std::filesystem::exists(start, ec))
                return out;
            for (std::filesystem::directory_iterator it(start, ec), end; it != end; it.increment(ec))
                out.push_back(it->path().filename().generic_string());
            std::sort(out.begin(), out.end());
            return out;
        }

    private:
        std::string _root;
    };

    // Collects what Core logs (the Windows logger.warning / logger.error records) while it lives.
    class LogCapture
    {
    public:
        struct Record
        {
            ost_log::Level level;
            std::string message;
        };
        LogCapture()
        {
            ost_log::set_sink([this](ost_log::Level level, const std::string &message, const std::string &)
                              { records.push_back({level, message}); });
        }
        ~LogCapture() { ost_log::set_sink(ost_log::Sink()); }
        LogCapture(const LogCapture &) = delete;
        LogCapture &operator=(const LogCapture &) = delete;

        std::vector<Record> records;

        size_t count(ost_log::Level level) const
        {
            size_t n = 0;
            for (const Record &r : records)
                if (r.level == level)
                    ++n;
            return n;
        }
    };

    // The real file system, counting the copies and failing the n-th one (a full disk).
    class CountingFs : public ost_import::StdOspFileSystem
    {
    public:
        int copies = 0;
        int fail_on_copy = -1; // 1 based; -1: never
        std::vector<std::string> destinations;
        bool copy_file(const std::string &from, const std::string &to) override
        {
            ++copies;
            destinations.push_back(to);
            if (copies == fail_on_copy)
                return false; // "disk full"
            return ost_import::StdOspFileSystem::copy_file(from, to);
        }
    };

    // The image member index of a package that holds Project.ost and the given members (osp_importer tests' _image_members).
    inline ost_import::ImageMemberIndex image_members(const std::vector<std::string> &members)
    {
        std::vector<std::string> names = {"Project.ost"};
        names.insert(names.end(), members.begin(), members.end());
        ost_import::OspPackage package;
        std::string error;
        if (!ost_import::inspect_osp_package(names, package, error))
            FAIL_TEST("fixture package rejected: " + error);
        return package.image_members_by_path;
    }

    // <XML_ROOT><Bid UID="1"><BidPages><BidPage UID="20" ...attributes/></BidPages></Bid></XML_ROOT>
    inline std::string page_xml(const std::vector<std::pair<std::string, std::string>> &attributes)
    {
        std::string xml = "<XML_ROOT><Bid UID=\"1\"><BidPages><BidPage UID=\"20\"";
        for (const auto &attribute : attributes)
            xml += " " + attribute.first + "=\"" + attribute.second + "\"";
        xml += "/></BidPages></Bid></XML_ROOT>";
        return xml;
    }

    // An OSP package (one folder, stored blocks) with the given members; names are stored as given (use '\' like makecab).
    inline std::string make_osp(const std::vector<std::pair<std::string, std::string>> &members)
    {
        std::vector<ost_itest::CabMember> cab_members;
        for (const auto &member : members)
        {
            ost_itest::CabMember m;
            m.name = member.first;
            m.content = member.second;
            cab_members.push_back(m);
        }
        return ost_itest::build_cab(cab_members, ost_itest::CabBuildOptions());
    }

    inline std::string page_value(const ost_import::OstRawData &raw, size_t page_index, const std::string &column)
    {
        const auto pages = raw.bid_tables.find("BidPages");
        if (pages == raw.bid_tables.end() || page_index >= pages->second.size())
            return "<no page>";
        const auto value = pages->second[page_index].find(column);
        return value == pages->second[page_index].end() ? "<absent>" : value->second;
    }
}
