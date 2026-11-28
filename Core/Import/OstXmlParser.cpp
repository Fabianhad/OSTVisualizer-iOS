#include "OstXmlParser.hpp"
#include "ImportDiagnostics.hpp"
#include "XmlNormalizer.hpp"
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

        // The Windows importer parses the file with ElementTree (expat, strict: ost_importer._parse_ost_xml) and takes
        // the attribute values byte-exact. pugixml accepts nearly everything, so the bytes first go through the strict
        // front end (IMP-M01): it rejects what expat rejects, honours the declared encoding and hands pugixml plain UTF-8.
        NormalizedXml xml = normalize_xml(data, length);
        if (!xml.ok)
        {
            result.error = xml.error;
            return result;
        }

        pugi::xml_document doc;
        pugi::xml_parse_result parse_res = doc.load_buffer_inplace(&xml.utf8[0], xml.utf8.size(), pugi::parse_default, pugi::encoding_utf8);
        if (!parse_res)
        {
            result.error = parse_res.description();
            return result;
        }

        pugi::xml_node root = doc.first_child();
        if (!root)
        {
            result.error = kNoBidError;
            return result;
        }

        pugi::xml_node bid_elem = root.child("Bid");
        if (!bid_elem)
        {
            result.error = kNoBidError;
            return result;
        }

        result.has_bid = true;
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
