// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
#pragma once
#include "DatabaseFolder.hpp"
#include <string>
#include <vector>

namespace ost_domain
{

    // domain/entities/project_constants.py: the system folder "Deleted Bids" is the BidProjects row with UID "1", whatever
    // its name. Everything that has to recognise it (rename / delete / drop target / trash / restore) uses the UID.
    constexpr const char *DELETED_BIDS_FOLDER_UID = "1";
    constexpr const char *DELETED_BIDS_FOLDER_NAME = "Deleted Bids";

    inline bool is_deleted_bids_folder_uid(const std::string &folder_uid)
    {
        return folder_uid == DELETED_BIDS_FOLDER_UID;
    }

    // project_write_handler.py _delete_projects: only empty projects are deleted. The warning shown when the project still has
    // bids, and the confirmation of an empty one (the Windows app words it for "projects"; the folder is its project).
    constexpr const char *CANNOT_DELETE_FOLDER_TITLE = "Cannot Delete Project";
    constexpr const char *CANNOT_DELETE_FOLDER_MESSAGE =
        "Some selected projects still have bids inside.\nOnly empty projects can be deleted.";
    constexpr const char *DELETE_FOLDER_CONFIRM_TITLE = "Delete Projects";
    constexpr const char *DELETE_FOLDER_CONFIRM_MESSAGE = "Permanently delete this empty project?\nThis cannot be undone.";

    struct BidSummary
    {
        std::string bid_uid;
        std::string name;
        std::string folder_uid;
        // Bids.BidNo (hierarchy_reader.py bid_no): the key of the default bid order. has_bid_no is false for a NULL BidNo, which
        // sorts before every number (project_tree_view.py SortableTreeWidgetItem.__lt__).
        bool has_bid_no = false;
        long long bid_no = 0;
    };

    struct FolderGroup
    {
        DatabaseFolder folder;
        std::vector<BidSummary> bids;
    };

    struct DatabaseHierarchy
    {
        std::vector<FolderGroup> folders;
        std::vector<BidSummary> orphan_bids;
    };

    // ---- display order -----------------------------------------------------------------------------------------------------

    // BRW-13: the default order of the bids of a folder and of the orphans. The Windows reader lists the bids by BidProjectUID, BidNo,
    // UID (hierarchy_reader.py) and the tree then sorts column 0 = BidNo ascending with a stable sort (sortByColumn(0, Ascending),
    // SortableTreeWidgetItem.__lt__): a NULL BidNo is less than any number, equal keys keep the reader order. Use with a STABLE sort
    // over the bids in reader order.
    inline bool bid_precedes(const BidSummary &a, const BidSummary &b)
    {
        if (!a.has_bid_no)
            return b.has_bid_no;
        if (!b.has_bid_no)
            return false;
        return a.bid_no < b.bid_no;
    }

    // BRW-M01 (decided by the owner): the display order of the folder names is a case-insensitive ASCII compare (only A-Z fold, every other
    // byte compares as an unsigned byte), then, for names equal up to case, lowercase first at the first byte that differs. Qt's own
    // locale-aware compare (Windows) cannot be reproduced in portable code. <0, 0, >0.
    inline int compare_folder_names(const std::string &a, const std::string &b)
    {
        const size_t common = a.size() < b.size() ? a.size() : b.size();
        for (size_t i = 0; i < common; ++i)
        {
            unsigned char x = static_cast<unsigned char>(a[i]);
            unsigned char y = static_cast<unsigned char>(b[i]);
            if (x >= 'A' && x <= 'Z')
                x = static_cast<unsigned char>(x + ('a' - 'A'));
            if (y >= 'A' && y <= 'Z')
                y = static_cast<unsigned char>(y + ('a' - 'A'));
            if (x != y)
                return x < y ? -1 : 1;
        }
        if (a.size() != b.size())
            return a.size() < b.size() ? -1 : 1;
        for (size_t i = 0; i < common; ++i)
        {
            const unsigned char x = static_cast<unsigned char>(a[i]);
            const unsigned char y = static_cast<unsigned char>(b[i]);
            if (x != y)
                return x > y ? -1 : 1; // the names are equal up to case: the lowercase letter has the larger byte and comes first
        }
        return 0;
    }

    // UIDs of BidProjects are integers: digits compare as numbers (9 before 10, any length), a value that is not all digits comes after the
    // numbers and compares as text. <0, 0, >0.
    inline int compare_folder_uids(const std::string &a, const std::string &b)
    {
        auto all_digits = [](const std::string &s)
        {
            if (s.empty())
                return false;
            for (const char c : s)
                if (c < '0' || c > '9')
                    return false;
            return true;
        };
        const bool a_number = all_digits(a);
        const bool b_number = all_digits(b);
        if (a_number != b_number)
            return a_number ? -1 : 1;
        if (!a_number)
            return a < b ? -1 : (a == b ? 0 : 1);
        size_t a_start = 0;
        size_t b_start = 0;
        while (a_start + 1 < a.size() && a[a_start] == '0')
            ++a_start;
        while (b_start + 1 < b.size() && b[b_start] == '0')
            ++b_start;
        const size_t a_length = a.size() - a_start;
        const size_t b_length = b.size() - b_start;
        if (a_length != b_length)
            return a_length < b_length ? -1 : 1;
        const int digits = a.compare(a_start, a_length, b, b_start, b_length);
        return digits < 0 ? -1 : (digits == 0 ? 0 : 1);
    }

    // The full folder order: name (compare_folder_names), then UID ascending (hierarchy_reader.py ORDER BY [Name], [UID]).
    inline bool folder_precedes(const DatabaseFolder &a, const DatabaseFolder &b)
    {
        const int by_name = compare_folder_names(a.name, b.name);
        if (by_name != 0)
            return by_name < 0;
        return compare_folder_uids(a.uid, b.uid) < 0;
    }

}
