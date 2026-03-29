#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace ost_import
{

    using Row = std::unordered_map<std::string, std::string>;
    using RowList = std::vector<Row>;
    using TableMap = std::unordered_map<std::string, RowList>;

    struct OstRawData
    {
        Row bid_row;
        TableMap bid_tables;
        TableMap page_tables;
        TableMap global_tables;

        bool empty() const { return bid_row.empty(); }
    };

    OstRawData parse_ost_xml(const char *data, size_t length);
    OstRawData parse_ost_xml(const std::string &xml_content);

}
