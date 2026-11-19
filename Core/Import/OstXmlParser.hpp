#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ost_import
{

    using Row = std::unordered_map<std::string, std::string>;
    using RowList = std::vector<Row>;

    // Table name -> rows, iterated in INSERTION order like the Python dict of the Windows RawBidData. The order is
    // observable: the importer assigns the new UIDs table after table in this order (ost_importer._build_uid_map), and
    // page tables are keyed in the order they are first seen. Small (a few dozen tables), so lookups are linear.
    class TableMap
    {
    public:
        using value_type = std::pair<std::string, RowList>;
        using iterator = std::vector<value_type>::iterator;
        using const_iterator = std::vector<value_type>::const_iterator;

        // Inserts an empty table at the end when the key is new (std::map/dict semantics).
        RowList &operator[](const std::string &key)
        {
            auto it = find(key);
            if (it != _items.end())
                return it->second;
            _items.emplace_back(key, RowList());
            return _items.back().second;
        }

        iterator find(const std::string &key)
        {
            for (auto it = _items.begin(); it != _items.end(); ++it)
                if (it->first == key)
                    return it;
            return _items.end();
        }

        const_iterator find(const std::string &key) const
        {
            for (auto it = _items.begin(); it != _items.end(); ++it)
                if (it->first == key)
                    return it;
            return _items.end();
        }

        size_t count(const std::string &key) const { return find(key) != _items.end() ? 1 : 0; }

        iterator begin() { return _items.begin(); }
        iterator end() { return _items.end(); }
        const_iterator begin() const { return _items.begin(); }
        const_iterator end() const { return _items.end(); }
        size_t size() const { return _items.size(); }
        bool empty() const { return _items.empty(); }

    private:
        std::vector<value_type> _items;
    };

    struct OstRawData
    {
        Row bid_row;
        TableMap bid_tables;
        TableMap page_tables;
        TableMap global_tables;
        // Set by parse_ost_xml when the document has a Bid element (it may have no attributes at all: the Windows
        // importer still imports it, only a MISSING Bid element is an error).
        bool has_bid = false;
        // Why parse_ost_xml returned an empty value: ElementTree's ParseError text for malformed XML ("mismatched tag:
        // line 1, column 12"), or the Windows ValueError text "No Bid element found in OST file". Empty on success.
        std::string error;

        // "No Bid element": the parser found none (or the XML is malformed). A hand-built value counts as having a Bid
        // as soon as it carries a Bid row.
        bool empty() const { return !has_bid && bid_row.empty(); }
    };

    OstRawData parse_ost_xml(const char *data, size_t length);
    OstRawData parse_ost_xml(const std::string &xml_content);

}
