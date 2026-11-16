// IMP-02 (+XML-02) and the parse side of IMP-11 (+DBP-23, XML-01): OST XML -> OstRawData.
//
// Windows parser: ost_visualizer/infrastructure/mdb/importers/ost_importer.py _parse_ost_xml (plain ElementTree, no
// pre-processing) with schema_contract.py (section lists, singular(), CopyTimestamp -> CopyTimeStamp).
// "No Bid element" and malformed XML raise in Windows; the iOS parser reports both as an EMPTY OstRawData.
#include "ImportTestSupport.hpp"

#include "Persistence/OstSchema.hpp"

using ost_import::OstRawData;
using ost_import::parse_ost_xml;

namespace
{
    std::vector<std::string> table_names(const ost_import::TableMap &tables)
    {
        std::vector<std::string> names;
        for (const auto &table : tables)
            names.push_back(table.first);
        return names;
    }

    std::vector<std::string> uids(const ost_import::TableMap &tables, const std::string &table)
    {
        auto it = tables.find(table);
        if (it == tables.end())
            return {"<no table>"};
        return ost_itest::values_of(it->second, "UID");
    }
}

// IMP-02. ElementTree returns attribute values byte-exact (verified with xml.etree.ElementTree on these very
// strings): a backslash before the closing quote is data, not an escape. iOS used to rewrite it into a forward slash.
TEST(ImportParser, Imp02_BackslashBeforeTheClosingQuoteIsKeptInEveryAttribute)
{
    const OstRawData raw = parse_ost_xml(
        R"xml(<XML_ROOT><Bid UID="1" JobName="Tank\" Notes='C:\dir\'><BidPages><BidPage UID="10" ImagePath="C:\Plans\" OverlayImagePath='D:\x\'/></BidPages></Bid></XML_ROOT>)xml");
    REQUIRE(!raw.empty());
    CHECK_STR_EQ(raw.bid_row.at("JobName"), std::string("Tank\\"));
    CHECK_STR_EQ(raw.bid_row.at("Notes"), std::string("C:\\dir\\"));
    const auto &pages = raw.bid_tables.find("BidPages")->second;
    REQUIRE_EQ(pages.size(), 1u);
    CHECK_STR_EQ(pages[0].at("ImagePath"), std::string("C:\\Plans\\"));
    CHECK_STR_EQ(pages[0].at("OverlayImagePath"), std::string("D:\\x\\"));
}

TEST(ImportParser, Imp02_BackslashesInsideValuesAreUntouchedToo)
{
    const OstRawData raw = parse_ost_xml(
        R"xml(<XML_ROOT><Bid UID="1" JobName="A\B\C" Notes="end\\"/></XML_ROOT>)xml");
    REQUIRE(!raw.empty());
    CHECK_STR_EQ(raw.bid_row.at("JobName"), std::string("A\\B\\C"));
    CHECK_STR_EQ(raw.bid_row.at("Notes"), std::string("end\\\\"));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_import_restores_database_column_name_for_copy_timestamp
TEST(ImportParser, Imp11_CopyTimestampIsStoredUnderTheDatabaseColumnName)
{
    const OstRawData raw = parse_ost_xml(R"xml(<XML_ROOT><Bid UID="1" CopyTimestamp="2026 7 19 0 39 2"/></XML_ROOT>)xml");
    REQUIRE(!raw.empty());
    CHECK_STR_EQ(raw.bid_row.at("CopyTimeStamp"), std::string("2026 7 19 0 39 2"));
    CHECK_EQ(raw.bid_row.count("CopyTimestamp"), 0u);
    CHECK_STR_EQ(raw.bid_row.at("UID"), std::string("1"));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_import_without_bid_element_fails_before_writer
// (the parse half: Windows raises ValueError('No Bid element found in OST file'); iOS returns an empty result).
TEST(ImportParser, DocumentWithoutABidElementIsEmpty)
{
    CHECK(parse_ost_xml("<XML_ROOT/>").empty());
    CHECK(parse_ost_xml("<XML_ROOT><Employees/></XML_ROOT>").empty());
    CHECK(parse_ost_xml("").empty());
    CHECK(parse_ost_xml("not xml at all").empty());
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_section_children_match_the_singular_or_plural_minus_s_tag
TEST(ImportParser, SingularOverridesMatchTheWindowsSchemaContract)
{
    CHECK_STR_EQ(ost_schema::singular("PayClasses"), std::string("PayClass"));
    CHECK_STR_EQ(ost_schema::singular("JobStatuses"), std::string("JobStatuse"));
    CHECK_STR_EQ(ost_schema::singular("BidAreas"), std::string("BidArea"));
    CHECK_STR_EQ(ost_schema::singular("BidConditionUser"), std::string("BidConditionUser"));
    CHECK_STR_EQ(ost_schema::singular("OCRProps"), std::string("OCRProp"));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_section_children_match_the_singular_or_plural_minus_s_tag
// through the parser: (section, tag) pairs of the Windows test, expected True -> the row is read.
TEST(ImportParser, SectionChildrenMatchTheSingularOrThePluralMinusSTag)
{
    struct Case
    {
        const char *container;
        const char *tag;
        bool expected;
        bool in_bid; // container below <Bid> (true) or below the root (false)
    };
    const Case cases[] = {
        {"BidAreas", "BidArea", true, true},
        {"BidAreas", "BidAreas", false, true},
        {"BidAreas", "BidLayer", false, true},
        {"JobStatuses", "JobStatuse", true, false},
        {"JobStatuses", "JobStatus", false, false},
        {"JobStatuses", "JobStatuses", false, false},
        {"PayClasses", "PayClass", true, false},
        {"PayClasses", "PayClasse", true, false},
        {"BidConditionUser", "BidConditionUser", true, true},
        {"BidConditionUser", "BidConditionUse", false, true},
        {"BidConditionUser", "BidConditionUsers", false, true},
        {"OCRProps", "OCRProp", true, true},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string(c.container) + "/" + c.tag);
        const std::string child = std::string("<") + c.tag + " UID=\"7\"/>";
        const std::string container = std::string("<") + c.container + ">" + child + "</" + c.container + ">";
        const std::string xml = c.in_bid ? "<XML_ROOT><Bid UID=\"1\">" + container + "</Bid></XML_ROOT>"
                                         : "<XML_ROOT><Bid UID=\"1\"/>" + container + "</XML_ROOT>";
        const OstRawData raw = parse_ost_xml(xml);
        REQUIRE(!raw.empty());
        const ost_import::TableMap &tables = c.in_bid ? raw.bid_tables : raw.global_tables;
        CHECK_EQ(uids(tables, c.container), c.expected ? std::vector<std::string>{"7"} : std::vector<std::string>{});
    }
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_parse_reads_each_section_family_from_its_own_container_only
TEST(ImportParser, Imp11_EachSectionFamilyIsReadFromItsOwnContainerOnly)
{
    const OstRawData raw = parse_ost_xml(R"xml(
        <XML_ROOT>
          <Bid UID="1" CopyTimestamp="2026 1 2 3 4 5">
            <BidAreas>
              <BidArea UID="10" Name="Area A"/>
              <BidLayer UID="11" Name="Wrong tag"/>
              <BidArea UID="12" Name="Area B"/>
            </BidAreas>
            <BidConditionUser><BidConditionUser UID="13"/><Other UID="99"/></BidConditionUser>
            <BidTakeoffs><BidTakeoff UID="500"/></BidTakeoffs>
            <BidPages>
              <Stray UID="900"/>
              <BidPage UID="20" Name="P1" CopyTimestamp="2026 1 2 3 4 5">
                <BidTakeoffs>
                  <BidTakeoff UID="30"/>
                  <BidArea UID="98"/>
                </BidTakeoffs>
                <BidComments><BidComment UID="40"/></BidComments>
                <BidAreas><BidArea UID="97"/></BidAreas>
              </BidPage>
              <Stray UID="901"/>
              <BidPage UID="21" Name="P2">
                <BidTakeoffs><BidTakeoff UID="31"/></BidTakeoffs>
              </BidPage>
            </BidPages>
            <BidNamedViews><BidNamedView UID="50"/></BidNamedViews>
            <BidHotLinks><BidHotLink UID="60"/><BidHotLinks UID="61"/></BidHotLinks>
            <Employees><Employee UID="999"/></Employees>
          </Bid>
          <Employees><Employee UID="7"/></Employees>
          <PayClasses>
            <PayClass UID="3"/>
            <PayClasse UID="4"/>
            <PayClasses UID="5"/>
          </PayClasses>
          <JobStatuses>
            <JobStatuse UID="8"/>
            <JobStatus UID="9"/>
            <Other UID="10"/>
          </JobStatuses>
          <CdnTypes><CdnType UID="11" Name="C"/></CdnTypes>
        </XML_ROOT>)xml");
    REQUIRE(!raw.empty());
    CHECK_EQ(ost_itest::describe(raw.bid_row), std::string("CopyTimeStamp=2026 1 2 3 4 5,UID=1"));
    CHECK_EQ(uids(raw.bid_tables, "BidAreas"), (std::vector<std::string>{"10", "12"}));
    CHECK_EQ(uids(raw.bid_tables, "BidConditionUser"), (std::vector<std::string>{"13"}));
    const auto &pages = raw.bid_tables.find("BidPages")->second;
    REQUIRE_EQ(pages.size(), 2u);
    CHECK_EQ(ost_itest::describe(pages[0]), std::string("CopyTimeStamp=2026 1 2 3 4 5,Name=P1,UID=20"));
    CHECK_EQ(ost_itest::describe(pages[1]), std::string("Name=P2,UID=21"));
    CHECK_EQ(uids(raw.bid_tables, "BidNamedViews"), (std::vector<std::string>{"50"}));
    CHECK_EQ(uids(raw.bid_tables, "BidHotLinks"), (std::vector<std::string>{"60"}));
    CHECK_EQ(raw.bid_tables.count("BidTakeoffs"), 0u);
    CHECK_EQ(table_names(raw.page_tables), (std::vector<std::string>{"BidTakeoffs", "BidComments"}));
    CHECK_EQ(uids(raw.page_tables, "BidTakeoffs"), (std::vector<std::string>{"30", "31"}));
    CHECK_EQ(uids(raw.page_tables, "BidComments"), (std::vector<std::string>{"40"}));
    CHECK_EQ(table_names(raw.global_tables),
             (std::vector<std::string>{"Employees", "PayClasses", "AccessLevels", "CdnTypes", "JobStatuses"}));
    CHECK_EQ(uids(raw.global_tables, "Employees"), (std::vector<std::string>{"7"}));
    CHECK_EQ(uids(raw.global_tables, "PayClasses"), (std::vector<std::string>{"3", "4"}));
    CHECK_EQ(uids(raw.global_tables, "AccessLevels"), (std::vector<std::string>{}));
    CHECK_EQ(uids(raw.global_tables, "CdnTypes"), (std::vector<std::string>{"11"}));
    CHECK_EQ(uids(raw.global_tables, "JobStatuses"), (std::vector<std::string>{"8"}));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_parse_always_provides_every_bid_and_tail_section_key
// (and the section ORDER of schema_contract.BID_SECTIONS: BidEmployees sits between BidSettings and BidLayers).
TEST(ImportParser, Imp11_EveryBidAndTailSectionKeyIsAlwaysPresentInWindowsOrder)
{
    const OstRawData raw = parse_ost_xml(R"xml(<XML_ROOT><Bid UID="1"/></XML_ROOT>)xml");
    REQUIRE(!raw.empty());
    CHECK_EQ(table_names(raw.bid_tables),
             (std::vector<std::string>{"BidPlanRooms", "BidAreas", "BidTypAreas", "BidConditionFolders",
                                       "BidPageFolders", "BidSettings", "BidEmployees", "BidLayers",
                                       "BidTypAreaCounts", "BidConditions", "UserMasterConditions",
                                       "BidConditionUser", "BidZones", "BidTypGroupViews", "OCRProps", "BidPages",
                                       "BidNamedViews", "BidHotLinks"}));
    for (const auto &table : raw.bid_tables)
        CHECK(table.second.empty());
    CHECK(raw.page_tables.empty());
    CHECK_EQ(table_names(raw.global_tables),
             (std::vector<std::string>{"Employees", "PayClasses", "AccessLevels", "CdnTypes", "JobStatuses"}));
}

// DBP-23 alias: BidEmployees (bid section) and PayClasses (global section) are parsed.
TEST(ImportParser, Imp11_BidEmployeesAndPayClassesAreParsed)
{
    const OstRawData raw = parse_ost_xml(R"xml(
        <XML_ROOT>
          <Bid UID="1">
            <BidEmployees><BidEmployee UID="1" BidUID="5" EmployeeUID="2"/></BidEmployees>
          </Bid>
          <Employees><Employee UID="2" PayClassUID="1"/></Employees>
          <PayClasses><PayClass UID="1" Name="Reg"/></PayClasses>
        </XML_ROOT>)xml");
    REQUIRE(!raw.empty());
    CHECK_EQ(raw.bid_tables.find("BidEmployees")->second.size(), 1u);
    CHECK_EQ(raw.global_tables.find("PayClasses")->second.size(), 1u);
    CHECK_STR_EQ(raw.global_tables.find("PayClasses")->second[0].at("Name"), std::string("Reg"));
    CHECK_STR_EQ(raw.bid_tables.find("BidEmployees")->second[0].at("EmployeeUID"), std::string("2"));
}

// Python-dict semantics of RawBidData: bid tables are keyed in the order the importer reads them, page tables in the
// order they are FIRST SEEN (page 1 sections in PAGE_SECTIONS order, then sections only later pages carry). The order is
// observable: _build_uid_map numbers the rows table after table.
TEST(ImportParser, PageTablesAreKeyedInFirstSeenOrder)
{
    const OstRawData raw = parse_ost_xml(R"xml(
        <XML_ROOT><Bid UID="1"><BidPages>
          <BidPage UID="20">
            <BidTexts><BidText UID="31"/></BidTexts>
            <BidTakeoffs><BidTakeoff UID="30"/></BidTakeoffs>
          </BidPage>
          <BidPage UID="21">
            <BidComments><BidComment UID="41"/></BidComments>
            <BidHighlights><BidHighlight UID="32"/></BidHighlights>
            <BidTakeoffs><BidTakeoff UID="33"/></BidTakeoffs>
          </BidPage>
        </BidPages></Bid></XML_ROOT>)xml");
    REQUIRE(!raw.empty());
    CHECK_EQ(table_names(raw.page_tables),
             (std::vector<std::string>{"BidTakeoffs", "BidTexts", "BidHighlights", "BidComments"}));
    CHECK_EQ(uids(raw.page_tables, "BidTakeoffs"), (std::vector<std::string>{"30", "33"}));
}

// XML-01 edge: Windows reads root.find("Bid"), a DIRECT child of the root element whatever the root is called; a Bid nested
// deeper is "No Bid element found in OST file".
TEST(ImportParser, BidIsLookedUpAsADirectChildOfTheRootOnly)
{
    CHECK(parse_ost_xml(R"xml(<XML_ROOT><Wrapper><Bid UID="1"/></Wrapper></XML_ROOT>)xml").empty());
    const OstRawData other_root = parse_ost_xml(R"xml(<Other><Bid UID="1" JobName="J"/></Other>)xml");
    REQUIRE(!other_root.empty());
    CHECK_STR_EQ(other_root.bid_row.at("JobName"), std::string("J"));
}
