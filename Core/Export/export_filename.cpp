#include "export_filename.hpp"
#include <cstddef>
#include <stdexcept>

namespace ost_geometry
{
    namespace export_text
    {

        namespace
        {
            // Windows _MAX_EXPORT_FILENAME_LENGTH
            constexpr size_t kMaxFilenameLength = 255;

            // The end of the character that starts at byte `i` of a UTF-8 string: the following continuation bytes (10xxxxxx)
            // belong to it. A byte that is not a continuation byte, or a continuation byte with no character before it, starts
            // a character, so every byte sequence has a well-defined character count (a valid string counts code points).
            size_t character_end(const std::string &s, size_t i)
            {
                size_t j = i + 1;
                while (j < s.size() && (static_cast<unsigned char>(s[j]) & 0xC0u) == 0x80u)
                    ++j;
                return j;
            }

            // Python len(str)
            size_t character_count(const std::string &s)
            {
                size_t count = 0;
                for (size_t i = 0; i < s.size(); i = character_end(s, i))
                    ++count;
                return count;
            }

            // Python s[:n]: the first n characters (all of it when it has fewer).
            std::string first_characters(const std::string &s, size_t n)
            {
                size_t i = 0;
                for (size_t k = 0; k < n && i < s.size(); ++k)
                    i = character_end(s, i);
                return s.substr(0, i);
            }
        } // namespace

        std::string sanitize_filename(const std::string &name)
        {
            // _INVALID_FILENAME_CHARS = [\x00-\x1f\\/:"*?<>|]: ASCII only, so UTF-8 sequences are never split.
            std::string out = name;
            for (char &c : out)
            {
                const unsigned char u = static_cast<unsigned char>(c);
                if (u < 0x20 || c == '\\' || c == '/' || c == ':' || c == '"' || c == '*' || c == '?' || c == '<' || c == '>' ||
                    c == '|')
                    c = '_';
            }
            return out;
        }

        std::string export_filename(const std::string &extension,
                                    const std::string &bid_name,
                                    const std::vector<std::string> &page_names)
        {
            if (page_names.empty())
                throw std::invalid_argument("export_filename needs at least one page name");

            const std::string clean_bid_name = sanitize_filename(bid_name);
            std::vector<std::string> clean_names;
            clean_names.reserve(page_names.size());
            for (const std::string &name : page_names)
                clean_names.push_back(sanitize_filename(name));

            const std::string suffix = "." + extension;
            std::string filename;
            if (clean_names.size() == 1)
                filename = clean_bid_name + " - " + clean_names[0] + suffix;
            else if (clean_names.size() == 2)
                filename = clean_bid_name + " - " + clean_names[0] + " + " + clean_names[1] + suffix;
            else
                filename = clean_bid_name + " - " + clean_names[0] + " + " + clean_names[1] + " + " +
                           std::to_string(clean_names.size() - 2) + " more" + suffix;

            const size_t length = character_count(filename);
            if (length <= kMaxFilenameLength)
                return filename;

            // The bid name gives way first, when more than 10 of its characters are left.
            const size_t overflow = length - kMaxFilenameLength;
            const size_t bid_length = character_count(clean_bid_name);
            // max_project_length = len(clean_bid_name) - overflow - 3 as a signed number: only a value above 10 counts
            const long long max_project_length = static_cast<long long>(bid_length) - static_cast<long long>(overflow) - 3;
            if (max_project_length > 10)
            {
                // filename.replace(clean_bid_name, truncated_project, 1): the name starts with the bid name, so the first
                // occurrence is the start.
                return first_characters(clean_bid_name, static_cast<size_t>(max_project_length)) + "..." +
                       filename.substr(clean_bid_name.size());
            }
            if (clean_names.size() == 1)
            {
                const size_t suffix_length = character_count(suffix);
                const size_t max_page_length = kMaxFilenameLength > suffix_length ? kMaxFilenameLength - suffix_length : 0;
                std::string page_name = clean_names[0];
                if (character_count(page_name) > max_page_length)
                    page_name = first_characters(page_name, max_page_length > 3 ? max_page_length - 3 : 0) + "...";
                return page_name + suffix;
            }
            return "Export_" + std::to_string(clean_names.size()) + "_pages" + suffix;
        }

    } // namespace export_text
} // namespace ost_geometry
