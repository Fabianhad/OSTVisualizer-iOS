// Layer: Import (pure C++)
// See OspPackage.hpp.
#include "OspPackage.hpp"

#include "../Domain/CoreLog.hpp"
#include "CabExtractor.hpp"
#include "CabMemberName.hpp"
#include "Digest.hpp"
#include "ImportDiagnostics.hpp"
#include "TextRules.hpp"
#include "XmlNormalizer.hpp"
#include "pugixml.hpp"

#include <algorithm>
#include <cstring>
#include <set>
#include <unordered_map>

namespace ost_import
{
    namespace
    {
        const char *const IMAGE_PATH_ATTRS[] = {"ImagePath", "OverlayImagePath"};

        std::string join(const std::vector<std::string> &parts, char separator, size_t first = 0)
        {
            std::string out;
            for (size_t i = first; i < parts.size(); ++i)
            {
                if (i > first)
                    out += separator;
                out += parts[i];
            }
            return out;
        }

        std::string fold(const std::string &text)
        {
            return text_rules::py_casefold(text);
        }

        // _cab_member_path(root, member_name): root.joinpath(*parts)
        std::string member_path(const std::string &root, const std::string &member_name)
        {
            const std::string relative = join(windows_path_parts(member_name), '/');
            if (root.empty() || root.back() == '/' || root.back() == '\\')
                return root + relative;
            return root + "/" + relative;
        }

        // The folder part of a path, with its trailing separator, and the file name.
        void split_name(const std::string &path, std::string &folder, std::string &name)
        {
            const size_t cut = path.find_last_of("/\\");
            if (cut == std::string::npos)
            {
                folder.clear();
                name = path;
            }
            else
            {
                folder = path.substr(0, cut + 1);
                name = path.substr(cut + 1);
            }
        }

        bool is_ost_extension(const std::string &member_name)
        {
            return text_rules::py_lower(windows_path_suffix(member_name)) == ".ost";
        }

        // Fills `buffer` up to `capacity` bytes (a short read only at the end of the file, like Python's BufferedReader.read(n)).
        bool read_chunk(OspFileReader &reader, std::vector<char> &buffer, size_t capacity, size_t &got)
        {
            buffer.resize(capacity);
            got = 0;
            while (got < capacity)
            {
                size_t piece = 0;
                if (!reader.read(buffer.data() + got, capacity - got, piece))
                    return false;
                if (piece == 0)
                    break;
                got += piece;
            }
            buffer.resize(got);
            return true;
        }

        const size_t CHUNK = 1024 * 1024;

        // _file_content_digest: sha256 of the file, read in chunks. false when the file cannot be read.
        bool file_sha256(OspFileSystem &fs, const std::string &path, std::string &digest)
        {
            std::unique_ptr<OspFileReader> reader = fs.open_read(path);
            if (!reader)
                return false;
            digest::Sha256 hash;
            std::vector<char> buffer;
            for (;;)
            {
                size_t got = 0;
                if (!read_chunk(*reader, buffer, CHUNK, got))
                    return false;
                if (got == 0)
                    break;
                hash.update(buffer.data(), got);
            }
            digest = hash.hex();
            return true;
        }

        struct PageCollector : pugi::xml_tree_walker
        {
            std::vector<pugi::xml_node> pages;
            bool for_each(pugi::xml_node &node) override
            {
                if (node.type() == pugi::node_element && std::strcmp(node.name(), "BidPage") == 0)
                    pages.push_back(node);
                return true;
            }
        };
    }

    // ---- IMP-14 -----------------------------------------------------------------------------------------------------------

    bool inspect_osp_package(const std::vector<std::string> &raw_names, OspPackage &package, std::string &error)
    {
        package = OspPackage();
        error.clear();
        std::set<std::string> seen;
        std::vector<std::string> ost_members;
        const std::string root_key = fold(OSP_IMAGE_MEMBER_ROOT);
        for (const std::string &raw_name : raw_names)
        {
            if (!is_safe_cab_member_name(raw_name))
            {
                error = unsafe_member_path_message(raw_name);
                return false;
            }
            const std::vector<std::string> parts = windows_path_parts(raw_name);
            const std::string member_name = join(parts, '\\');
            const std::string member_key = fold(member_name);
            if (!seen.insert(member_key).second)
            {
                error = "duplicate or conflicting CAB member: " + python_repr(member_name);
                return false;
            }
            package.member_names.push_back(member_name);
            if (parts.size() == 1 && is_ost_extension(member_name))
                ost_members.push_back(member_name);
            if (fold(parts[0]) != root_key)
                continue;
            if (parts.size() < 2 || !is_supported_image_path(member_name))
            {
                error = std::string("unsupported member under ") + OSP_IMAGE_MEMBER_ROOT + ": " + python_repr(member_name);
                return false;
            }
            package.image_members_by_path.emplace_back(member_key, member_name);
        }
        if (ost_members.size() != 1)
        {
            error = "archive must contain exactly one top-level .ost file; found " + std::to_string(ost_members.size());
            return false;
        }
        package.ost_member_name = ost_members[0];
        return true;
    }

    // ---- IMP-16 -----------------------------------------------------------------------------------------------------------

    bool is_supported_image_path(const std::string &path)
    {
        const std::string suffix = text_rules::py_lower(windows_path_suffix(path));
        return suffix == ".pdf" || suffix == ".tif" || suffix == ".tiff";
    }

    bool resolve_image_member(const std::string &image_path, const ImageMemberIndex &index, std::string &member_name)
    {
        member_name.clear();
        std::vector<std::string> reference_parts = windows_path_parts(image_path);
        if (reference_parts.empty())
            return false; // Python raises IndexError; every caller passes a path that has an extension (so a component)
        for (std::string &part : reference_parts)
            part = fold(part);
        if (reference_parts[0] == fold(OSP_IMAGE_MEMBER_ROOT))
        {
            const std::string key = join(reference_parts, '\\');
            for (const auto &entry : index)
            {
                if (entry.first == key)
                {
                    member_name = entry.second;
                    return true;
                }
            }
            return false;
        }
        bool found = false;
        size_t best_depth = 0;
        for (const auto &entry : index)
        {
            std::vector<std::string> member_parts = windows_path_parts(entry.second);
            if (!member_parts.empty())
                member_parts.erase(member_parts.begin()); // [1:]: without TempImages!.tmp
            for (std::string &part : member_parts)
                part = fold(part);
            const size_t depth = member_parts.size();
            if (depth > best_depth && depth <= reference_parts.size() &&
                std::equal(member_parts.begin(), member_parts.end(), reference_parts.end() - static_cast<std::ptrdiff_t>(depth)))
            {
                member_name = entry.second;
                found = true;
                best_depth = depth;
            }
        }
        return found;
    }

    bool collect_image_references(const char *xml, size_t length, const ImageMemberIndex &index, ImageReferenceScan &scan,
                                  std::string &parse_error)
    {
        scan = ImageReferenceScan();
        parse_error.clear();
        const NormalizedXml normalized = normalize_xml(xml, length);
        if (!normalized.ok)
        {
            parse_error = normalized.error;
            return false;
        }
        pugi::xml_document doc;
        const pugi::xml_parse_result parsed =
            doc.load_buffer(normalized.utf8.data(), normalized.utf8.size(), pugi::parse_default, pugi::encoding_utf8);
        if (!parsed)
        {
            parse_error = parsed.description();
            return false;
        }

        PageCollector collector;
        doc.traverse(collector);

        std::unordered_map<std::string, size_t> position_by_original;
        std::set<std::string> missing;
        for (pugi::xml_node &page : collector.pages)
        {
            for (const char *attribute_name : IMAGE_PATH_ATTRS)
            {
                const pugi::xml_attribute attribute = page.attribute(attribute_name);
                if (!attribute)
                    continue;
                const std::string image_path = attribute.value();
                if (image_path.empty() || !is_supported_image_path(image_path))
                    continue;
                std::string member_name;
                if (!resolve_image_member(image_path, index, member_name))
                {
                    if (missing.insert(image_path).second)
                    {
                        if (scan.unresolved_count == 0)
                            scan.first_unresolved = image_path;
                        ++scan.unresolved_count;
                    }
                    continue;
                }
                const auto known = position_by_original.find(image_path);
                if (known == position_by_original.end())
                {
                    position_by_original[image_path] = scan.references.size();
                    scan.references.push_back({image_path, member_name});
                }
                else
                {
                    scan.references[known->second].member_name = member_name;
                }
            }
        }
        if (scan.unresolved_count > 0)
        {
            ost_log::warning("OSP import could not resolve " + std::to_string(scan.unresolved_count) +
                             " referenced image(s) in the package. Import will continue and preserve those original paths. "
                             "First missing source path: " +
                             scan.first_unresolved);
        }
        return true;
    }

    bool rewrite_image_paths(RowList &page_rows, const std::vector<std::pair<std::string, std::string>> &copied_paths)
    {
        bool modified = false;
        for (Row &row : page_rows)
        {
            for (const char *attribute_name : IMAGE_PATH_ATTRS)
            {
                const auto value = row.find(attribute_name);
                if (value == row.end() || value->second.empty())
                    continue;
                for (const auto &copied : copied_paths)
                {
                    if (copied.first == value->second)
                    {
                        value->second = copied.second;
                        modified = true;
                        break;
                    }
                }
            }
        }
        return modified;
    }

    // ---- IMP-12 / IMP-M02 --------------------------------------------------------------------------------------------------

    bool image_destination_relative_path(const std::string &member_name, std::string &relative_path, std::string &error)
    {
        relative_path.clear();
        error.clear();
        const std::vector<std::string> parts = windows_path_parts(member_name);
        for (const std::string &part : parts)
        {
            if (is_windows_device_name(part))
            {
                error = "image staging name is a reserved device name: " + python_repr(member_name);
                return false;
            }
        }
        if (parts.empty())
        {
            error = "image staging name is empty";
            return false;
        }
        const std::string &file_name = parts.back();
        if (parts.size() == 2)
        {
            relative_path = file_name;
            return true;
        }
        const std::string member_digest = digest::sha1_hex(fold(member_name)).substr(0, 16);
        relative_path = "Images/" + member_digest + "/" + file_name;
        return true;
    }

    bool files_have_same_content(OspFileSystem &fs, const std::string &first, const std::string &second, bool &same)
    {
        same = false;
        // A name taken by a directory (or anything that is not a regular file) is never reused.
        if (!fs.is_file(second))
            return true;
        uint64_t first_size = 0;
        uint64_t second_size = 0;
        if (!fs.file_size(first, first_size) || !fs.file_size(second, second_size))
            return false;
        if (first_size != second_size)
            return true;
        std::unique_ptr<OspFileReader> first_reader = fs.open_read(first);
        std::unique_ptr<OspFileReader> second_reader = fs.open_read(second);
        if (!first_reader || !second_reader)
            return false;
        std::vector<char> first_chunk;
        std::vector<char> second_chunk;
        for (;;)
        {
            size_t first_got = 0;
            size_t second_got = 0;
            if (!read_chunk(*first_reader, first_chunk, CHUNK, first_got) || !read_chunk(*second_reader, second_chunk, CHUNK, second_got))
                return false;
            if (first_got != second_got || (first_got > 0 && std::memcmp(first_chunk.data(), second_chunk.data(), first_got) != 0))
                return true; // different
            if (first_got == 0)
            {
                same = true;
                return true;
            }
        }
    }

    bool non_conflicting_image_destination(OspFileSystem &fs, const std::string &source, const std::string &destination,
                                           std::string &chosen, std::string &error)
    {
        chosen.clear();
        error.clear();
        auto reusable = [&](const std::string &candidate, bool &ok) -> bool
        {
            if (!fs.exists(candidate))
            {
                ok = true;
                return true;
            }
            bool same = false;
            if (!files_have_same_content(fs, source, candidate, same))
            {
                ok = false;
                error = "cannot compare the packaged image with " + candidate;
                return false;
            }
            ok = true;
            return same;
        };

        bool ok = false;
        if (reusable(destination, ok))
        {
            chosen = destination;
            return true;
        }
        if (!ok)
            return false;

        std::string content_digest;
        if (!file_sha256(fs, source, content_digest))
        {
            error = "cannot read the packaged image " + source;
            return false;
        }
        std::string folder;
        std::string name;
        split_name(destination, folder, name);
        const std::string stem = windows_path_stem(name);
        const std::string suffix = windows_path_suffix(name);
        const size_t lengths[] = {16, 32, content_digest.size()};
        for (const size_t digest_length : lengths)
        {
            const std::string candidate = folder + stem + "-" + content_digest.substr(0, digest_length) + suffix;
            if (reusable(candidate, ok))
            {
                chosen = candidate;
                return true;
            }
            if (!ok)
                return false;
        }
        error = "image staging content-address collision for " + python_repr(name);
        return false;
    }

    bool copy_referenced_images(OspFileSystem &fs, const std::string &extract_root, const std::string &dest_dir,
                                const std::vector<ImageReference> &references,
                                std::vector<std::pair<std::string, std::string>> &copied_paths, std::string &error)
    {
        copied_paths.clear();
        error.clear();
        // Everything the references need must be there before the first copy.
        for (const ImageReference &reference : references)
        {
            if (!fs.is_file(member_path(extract_root, reference.member_name)))
            {
                error = "image member was not extracted: " + reference.member_name;
                return false;
            }
        }
        std::unordered_map<std::string, std::string> destinations_by_member;
        std::unordered_map<std::string, std::string> members_by_destination;
        for (const ImageReference &reference : references)
        {
            const std::string source = member_path(extract_root, reference.member_name);
            const std::string member_key = fold(reference.member_name);
            std::string destination;
            const auto known = destinations_by_member.find(member_key);
            if (known != destinations_by_member.end())
            {
                destination = known->second;
            }
            else
            {
                std::string relative;
                if (!image_destination_relative_path(reference.member_name, relative, error))
                    return false;
                std::string dest_path = dest_dir;
                if (!dest_path.empty() && dest_path.back() != '/' && dest_path.back() != '\\')
                    dest_path += '/';
                dest_path += relative;
                std::string chosen;
                if (!non_conflicting_image_destination(fs, source, dest_path, chosen, error))
                    return false;
                const std::string destination_key = fold(chosen);
                const auto conflicting = members_by_destination.find(destination_key);
                if (conflicting != members_by_destination.end() && conflicting->second != member_key)
                {
                    error = "image staging destination collision between " + python_repr(reference.member_name) + " and " +
                            python_repr(conflicting->second);
                    return false;
                }
                std::string folder;
                std::string name;
                split_name(chosen, folder, name);
                if (!folder.empty() && (folder.back() == '/' || folder.back() == '\\'))
                    folder.pop_back(); // the folder without its trailing separator (Path(chosen).parent)
                if (!folder.empty() && !fs.make_directories(folder))
                {
                    error = "cannot create the folder of the staged image " + chosen;
                    return false;
                }
                if (!fs.copy_file(source, chosen))
                {
                    error = "cannot copy the packaged image " + reference.member_name + " to " + chosen;
                    return false;
                }
                destination = chosen;
                destinations_by_member[member_key] = destination;
                members_by_destination[destination_key] = member_key;
            }
            bool replaced = false;
            for (auto &entry : copied_paths)
            {
                if (entry.first == reference.original_path)
                {
                    entry.second = destination;
                    replaced = true;
                    break;
                }
            }
            if (!replaced)
                copied_paths.emplace_back(reference.original_path, destination);
        }
        return true;
    }

    // ---- the whole package ------------------------------------------------------------------------------------------------

    namespace
    {
        // Steps 2-5 of stage_osp_package, with the extraction folder `extract_dir` already created.
        bool stage_inside(const char *data, size_t length, const OspPackage &package, const std::string &extract_dir,
                          const std::string &images_root, OspFileSystem &fs, OspStageResult &result)
        {
            auto fail = [&](const std::string &text)
            {
                result.error = text;
                return false;
            };

            const CabExtractResult extracted = extract_cab_to_directory(data, length, extract_dir);
            if (extracted.status == CabStatus::UnsafeName)
                return fail(extracted.detail);
            if (extracted.status != CabStatus::Ok)
            {
                ost_log::log(ost_log::Level::Error, "Cannot extract the OSP package: " + extracted.detail);
                return fail("The OSP archive could not be extracted");
            }

            const std::string ost_path = member_path(extract_dir, package.ost_member_name);
            if (!fs.is_file(ost_path))
                return fail("embedded OST member was not extracted: " + package.ost_member_name);
            std::vector<char> ost_bytes;
            if (!fs.read_file(ost_path, ost_bytes))
                return fail("The embedded OST file could not be read");

            ImageReferenceScan scan;
            std::string parse_error;
            if (!collect_image_references(ost_bytes.data(), ost_bytes.size(), package.image_members_by_path, scan, parse_error))
                return fail("embedded OST data is corrupt: " + parse_error);

            result.package_name = windows_path_stem(package.ost_member_name);
            std::vector<std::pair<std::string, std::string>> copied_paths;
            if (!scan.references.empty())
            {
                std::string dest_dir = images_root;
                if (!dest_dir.empty() && dest_dir.back() != '/' && dest_dir.back() != '\\')
                    dest_dir += '/';
                dest_dir += result.package_name;
                std::string copy_error;
                if (!copy_referenced_images(fs, extract_dir, dest_dir, scan.references, copied_paths, copy_error))
                    return fail(copy_error);
                result.staged_image_count = copied_paths.size();
            }

            OstRawData raw = parse_ost_xml(ost_bytes.data(), ost_bytes.size());
            if (raw.empty())
                return fail(raw.error.empty() ? std::string(kNoBidError) : raw.error);
            if (!copied_paths.empty())
            {
                const auto pages = raw.bid_tables.find("BidPages");
                if (pages != raw.bid_tables.end())
                    rewrite_image_paths(pages->second, copied_paths);
            }
            result.raw = std::move(raw);
            return true;
        }
    }

    bool stage_osp_package(const char *data, size_t length, const std::string &temp_parent, const std::string &images_root,
                           OspFileSystem &fs, OspStageResult &result)
    {
        result = OspStageResult();
        const std::vector<CabEntry> entries = list_cab(data, length);
        if (entries.empty())
        {
            result.error = "Invalid OSP file format";
            return false;
        }
        std::vector<std::string> names;
        names.reserve(entries.size());
        for (const CabEntry &entry : entries)
            names.push_back(entry.stored_name);
        OspPackage package;
        if (!inspect_osp_package(names, package, result.error))
            return false;

        std::string extract_dir;
        if (!fs.make_temp_directory(temp_parent, "ostv_osp_", extract_dir))
        {
            result.error = kOspTempFolderError;
            return false;
        }
        const bool ok = stage_inside(data, length, package, extract_dir, images_root, fs, result);
        if (!fs.remove_all(extract_dir))
            ost_log::warning("Failed to remove temporary OSP extraction directory " + extract_dir);
        return ok;
    }
}
