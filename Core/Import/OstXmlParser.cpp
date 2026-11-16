#include "OstXmlParser.hpp"
#include "pugixml.hpp"
#include "../Persistence/OstSchema.hpp"

namespace ost_import
{

    // Windows schema_contract.OST_XML_TO_DATABASE_COLUMN: the XML spells the column CopyTimestamp, the database
    // CopyTimeStamp (ost_importer._database_row). Applied to every row, including the Bid and page rows.
    static Row attrs_to_row(const pugi::xml_node &node)
    {
        Row row;
        for (auto &attr : node.attributes())
        {
            const std::string name = attr.name();
            row[name == "CopyTimestamp" ? std::string("CopyTimeStamp") : name] = attr.value();
        }
        return row;
    }

    // Windows ost_importer._matches_section_child: the singular tag of the section, or the section name minus its
    // trailing "s" (so both PayClass and PayClasse are PayClasses rows; the container's own name is not).
    static bool matches_section_child(const std::string &section, const std::string &tag)
    {
        if (tag == ost_schema::singular(section))
            return true;
        return !section.empty() && section.back() == 's' && tag == section.substr(0, section.size() - 1);
    }

    static void collect_section(const pugi::xml_node &parent,
                                const char *section_name,
                                TableMap &target)
    {
        pugi::xml_node container = parent.child(section_name);
        RowList rows;
        if (container)
        {
            for (auto &child : container.children())
            {
                if (matches_section_child(section_name, child.name()))
                    rows.push_back(attrs_to_row(child));
            }
        }
        target[section_name] = std::move(rows);
    }

    OstRawData parse_ost_xml(const char *data, size_t length)
    {
        OstRawData result;

        // Attribute values are taken byte-exact as the XML parser returns them, like ElementTree in the Windows
        // importer (ost_importer._parse_ost_xml parses the file as is; there is no pre-processing pass).
        pugi::xml_document doc;
        pugi::xml_parse_result parse_res = doc.load_buffer(data, length);
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
                    pugi::xml_node container = page_elem.child(section);
                    if (container)
                    {
                        for (auto &child : container.children())
                        {
                            if (matches_section_child(section, child.name()))
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
