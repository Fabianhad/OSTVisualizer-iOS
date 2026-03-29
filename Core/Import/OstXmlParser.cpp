#include "OstXmlParser.hpp"
#include "pugixml.hpp"
#include "../Persistence/OstSchema.hpp"

namespace ost_import
{

    static Row attrs_to_row(const pugi::xml_node &node)
    {
        Row row;
        for (auto &attr : node.attributes())
            row[attr.name()] = attr.value();
        return row;
    }

    static void collect_section(const pugi::xml_node &parent,
                                const char *section_name,
                                TableMap &target)
    {
        std::string singular = ost_schema::singular(section_name);
        pugi::xml_node container = parent.child(section_name);
        RowList rows;
        if (container)
        {
            for (auto &child : container.children())
            {
                if (child.name() == singular)
                    rows.push_back(attrs_to_row(child));
            }
        }
        target[section_name] = std::move(rows);
    }

    static std::string sanitize_ost_xml(const char *data, size_t length)
    {
        std::string xml(data, length);
        bool in_attr = false;
        char attr_quote = 0;

        for (size_t i = 0; i < xml.size(); i++)
        {
            char c = xml[i];
            if (!in_attr)
            {
                if (c == '=' && i + 1 < xml.size() && (xml[i + 1] == '"' || xml[i + 1] == '\''))
                {
                    in_attr = true;
                    attr_quote = xml[i + 1];
                    i++;
                }
            }
            else
            {
                if (c == '\\' && i + 1 < xml.size() && xml[i + 1] == attr_quote)
                {
                    xml[i] = '/';
                }
                else if (c == attr_quote)
                {
                    in_attr = false;
                }
            }
        }
        return xml;
    }

    OstRawData parse_ost_xml(const char *data, size_t length)
    {
        OstRawData result;

        std::string sanitized = sanitize_ost_xml(data, length);

        pugi::xml_document doc;
        pugi::xml_parse_result parse_res = doc.load_buffer(
            sanitized.c_str(), sanitized.size());
        if (!parse_res)
            return result;

        pugi::xml_node root = doc.first_child();
        if (!root)
            return result;

        pugi::xml_node bid_elem = root.child("Bid");
        if (!bid_elem)
            return result;

        result.bid_row = attrs_to_row(bid_elem);

        for (const char *section : ost_schema::BID_SECTIONS)
            collect_section(bid_elem, section, result.bid_tables);

        pugi::xml_node pages_container = bid_elem.child("BidPages");
        RowList bid_pages;
        if (pages_container)
        {
            for (auto &page_elem : pages_container.children())
            {
                if (std::string(page_elem.name()) != "BidPage")
                    continue;

                bid_pages.push_back(attrs_to_row(page_elem));

                for (const char *section : ost_schema::PAGE_SECTIONS)
                {
                    std::string singular = ost_schema::singular(section);
                    pugi::xml_node container = page_elem.child(section);
                    if (container)
                    {
                        for (auto &child : container.children())
                        {
                            if (child.name() == singular)
                                result.page_tables[section].push_back(attrs_to_row(child));
                        }
                    }
                }
            }
        }
        result.bid_tables["BidPages"] = std::move(bid_pages);

        for (const char *section : ost_schema::BID_TAIL_SECTIONS)
            collect_section(bid_elem, section, result.bid_tables);

        for (const char *section : ost_schema::GLOBAL_SECTIONS)
            collect_section(root, section, result.global_tables);

        return result;
    }

    OstRawData parse_ost_xml(const std::string &xml_content)
    {
        return parse_ost_xml(xml_content.c_str(), xml_content.size());
    }

}
