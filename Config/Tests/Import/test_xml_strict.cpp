// IMP-M01: the OST XML must be parsed as strictly as the Windows importer does.
//
// Windows: ost_visualizer/infrastructure/mdb/importers/ost_importer.py _parse_ost_xml calls ET.parse(path), i.e. the strict
// expat parser (Python 3.11.4 / expat 2.5.0): a file that is not well-formed XML 1.0 + namespaces raises ET.ParseError and
// nothing is imported; a declared encoding (windows-1252, ISO-8859-1, UTF-16 ...) is honoured. pugixml accepts nearly
// everything and keeps the raw bytes of a windows-1252 file (invalid UTF-8). ost_import::normalize_xml
// (Core/Import/XmlNormalizer.cpp) is the strict front end; parse_ost_xml runs it before pugixml.
//
// Oracle: Fixtures/import_xml_oracle.txt holds byte strings and what xml.etree.ElementTree.parse (the Windows parser) makes
// of each (computed read-only with the Python standard library; see the fixture header for the format).
#include "ImportTestSupport.hpp"

#include "Import/XmlNormalizer.hpp"
#include "Import/pugixml.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <vector>

using ost_import::NormalizedXml;
using ost_import::OstRawData;
using ost_import::parse_ost_xml;

namespace
{
    std::string from_hex(const std::string &hex)
    {
        std::string out;
        for (size_t i = 0; i + 1 < hex.size(); i += 2)
            out += static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16));
        return out;
    }

    // UTF-8 well-formedness (RFC 3629): no overlong forms, no surrogates, nothing above U+10FFFF.
    bool is_valid_utf8(const std::string &s)
    {
        size_t i = 0;
        while (i < s.size())
        {
            const unsigned char c = static_cast<unsigned char>(s[i]);
            size_t need = 0;
            uint32_t cp = 0;
            if (c < 0x80)
            {
                ++i;
                continue;
            }
            if (c >= 0xC2 && c <= 0xDF)
            {
                need = 1;
                cp = c & 0x1F;
            }
            else if (c >= 0xE0 && c <= 0xEF)
            {
                need = 2;
                cp = c & 0x0F;
            }
            else if (c >= 0xF0 && c <= 0xF4)
            {
                need = 3;
                cp = c & 0x07;
            }
            else
                return false;
            if (i + need >= s.size())
                return false;
            for (size_t k = 1; k <= need; ++k)
            {
                const unsigned char cc = static_cast<unsigned char>(s[i + k]);
                if ((cc & 0xC0) != 0x80)
                    return false;
                cp = (cp << 6) | (cc & 0x3F);
            }
            if ((need == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) || (need == 3 && (cp < 0x10000 || cp > 0x10FFFF)))
                return false;
            i += need + 1;
        }
        return true;
    }

    // The oracle's text escaping (xml_oracle_lib.esc): a small printable set verbatim, everything else as backslash u {hex}.
    std::string escape(const std::string &utf8)
    {
        static const std::string safe =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ._:;,=/<>{}()@$%^&*+!?'\"-~[]";
        std::string out;
        size_t i = 0;
        while (i < utf8.size())
        {
            const unsigned char c = static_cast<unsigned char>(utf8[i]);
            size_t len = 1;
            uint32_t cp = c;
            if (c >= 0xF0)
            {
                len = 4;
                cp = c & 0x07;
            }
            else if (c >= 0xE0)
            {
                len = 3;
                cp = c & 0x0F;
            }
            else if (c >= 0xC0)
            {
                len = 2;
                cp = c & 0x1F;
            }
            for (size_t k = 1; k < len && i + k < utf8.size(); ++k)
                cp = (cp << 6) | (static_cast<unsigned char>(utf8[i + k]) & 0x3F);
            if (len == 1 && safe.find(static_cast<char>(c)) != std::string::npos)
            {
                out += static_cast<char>(c);
            }
            else
            {
                char buffer[24];
                std::snprintf(buffer, sizeof(buffer), "\\u{%x}", static_cast<unsigned>(cp));
                out += buffer;
            }
            i += len;
        }
        return out;
    }

    // "_ns:<hex of the uri>:<local>" -> "{uri}local" (the ElementTree spelling).
    std::string et_name(const std::string &name)
    {
        if (name.compare(0, 4, "_ns:") != 0)
            return name;
        const size_t colon = name.find(':', 4);
        if (colon == std::string::npos)
            return name;
        return "{" + from_hex(name.substr(4, colon - 4)) + "}" + name.substr(colon + 1);
    }

    // ElementTree-style event dump of a pugixml element (see the fixture header): start, sorted attributes, text, children, end.
    void dump_element(const pugi::xml_node &element, std::vector<std::string> &out)
    {
        out.push_back("S " + escape(et_name(element.name())));
        std::vector<std::pair<std::string, std::string>> attributes;
        for (const pugi::xml_attribute &attribute : element.attributes())
            attributes.emplace_back(et_name(attribute.name()), attribute.value());
        std::sort(attributes.begin(), attributes.end());
        for (const auto &attribute : attributes)
            out.push_back("A " + escape(attribute.first) + "=" + escape(attribute.second));
        for (const pugi::xml_node &child : element.children())
        {
            if (child.type() == pugi::node_element)
                dump_element(child, out);
            else if (child.type() == pugi::node_pcdata || child.type() == pugi::node_cdata)
                out.push_back(std::string("T ") + escape(child.value()));
        }
        out.push_back("E");
    }

    std::string join(const std::vector<std::string> &parts)
    {
        std::string out;
        for (size_t i = 0; i < parts.size(); ++i)
            out += (i ? "|" : "") + parts[i];
        return out;
    }

    // The tree the iOS front end hands to pugixml, in the oracle's notation. "" when the input is rejected.
    bool tree_of(const std::string &xml, std::string &dump, NormalizedXml *normalized = nullptr)
    {
        NormalizedXml n = ost_import::normalize_xml(xml.data(), xml.size());
        if (normalized)
            *normalized = n;
        if (!n.ok)
            return false;
        pugi::xml_document doc;
        const pugi::xml_parse_result result = doc.load_buffer(n.utf8.data(), n.utf8.size(),
                                                              pugi::parse_default | pugi::parse_ws_pcdata, pugi::encoding_utf8);
        if (!result)
            return false;
        std::vector<std::string> events;
        dump_element(doc.first_child(), events);
        dump = join(events);
        return true;
    }

    struct OracleLine
    {
        std::string input;
        std::string result; // O, E or L
        std::string payload;
        int number;
    };

    std::vector<OracleLine> read_oracle()
    {
        std::vector<OracleLine> lines;
        std::string text;
        if (!ost_test::read_file(ost_test::fixture_path("import_xml_oracle.txt"), text))
            FAIL_TEST("cannot read import_xml_oracle.txt");
        std::istringstream stream(text);
        std::string line;
        int number = 0;
        while (std::getline(stream, line))
        {
            ++number;
            if (line.empty() || line[0] == '#')
                continue;
            const size_t t1 = line.find('\t');
            const size_t t2 = line.find('\t', t1 + 1);
            REQUIRE(t1 != std::string::npos && t2 != std::string::npos);
            lines.push_back({from_hex(line.substr(0, t1)), line.substr(t1 + 1, t2 - t1 - 1), line.substr(t2 + 1), number});
        }
        return lines;
    }

    std::string bid_name(const std::string &xml)
    {
        const OstRawData raw = parse_ost_xml(xml);
        if (raw.empty())
            return "<rejected>";
        auto it = raw.bid_row.find("JobName");
        return it == raw.bid_row.end() ? std::string("<none>") : it->second;
    }

    std::string utf16(const std::string &ascii_xml, bool big_endian, bool bom)
    {
        std::string out;
        if (bom)
            out += big_endian ? "\xFE\xFF" : "\xFF\xFE";
        for (char c : ascii_xml)
        {
            if (big_endian)
            {
                out += '\0';
                out += c;
            }
            else
            {
                out += c;
                out += '\0';
            }
        }
        return out;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The Windows tests and the item's test plan
// ---------------------------------------------------------------------------------------------------------------------

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_import_ost_rejects_unreadable_or_malformed_files_before_the_writer
// (the file cases that reach the parser): empty file, truncated xml, binary garbage and a document without a Bid element
// raise in Windows; iOS reports each as an empty OstRawData and gives the reason.
TEST(ImportXml, ImpM01_EmptyTruncatedGarbageAndBidlessFilesAreRejected)
{
    struct Case
    {
        const char *label;
        std::string xml;
        const char *reason;
    };
    const std::vector<Case> cases = {
        {"empty file", "", "no element found"},
        {"truncated xml", "<XML_ROOT><Bid UID=\"1\">", "no element found"},
        {"not xml", std::string("MDB\0binary garbage", 18), "not well-formed (invalid token)"},
        {"no bid", "<XML_ROOT/>", "No Bid element found in OST file"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.label);
        const OstRawData raw = parse_ost_xml(c.xml);
        CHECK(raw.empty());
        CHECK(raw.error.find(c.reason) == 0u);
    }
}

// IMP-M01 test plan: inputs that expat rejects (verified with ET.parse) and that pugixml used to accept.
TEST(ImportXml, ImpM01_MalformedFilesThatPugixmlAcceptedAreRejected)
{
    struct Case
    {
        const char *label;
        std::string xml;
        const char *reason;
    };
    const std::vector<Case> cases = {
        {"raw 0xE9 without a declaration", "<XML_ROOT><Bid UID=\"1\" JobName=\"Caf\xE9\"/></XML_ROOT>", "not well-formed (invalid token)"},
        {"duplicate attribute", "<XML_ROOT><Bid UID=\"1\" JobName=\"a\" JobName=\"b\"/></XML_ROOT>", "duplicate attribute"},
        {"'<' in an attribute value", "<XML_ROOT><Bid UID=\"1\" JobName=\"a<b\"/></XML_ROOT>", "not well-formed (invalid token)"},
        {"second root element", "<XML_ROOT><Bid UID=\"1\"/></XML_ROOT><XML_ROOT/>", "junk after document element"},
        {"undefined entity", "<XML_ROOT><Bid UID=\"1\" JobName=\"a&foo;b\"/></XML_ROOT>", "undefined entity"},
        {"bare ampersand", "<XML_ROOT><Bid UID=\"1\" JobName=\"a&b\"/></XML_ROOT>", "not well-formed (invalid token)"},
        {"unquoted attribute value", "<XML_ROOT><Bid UID=1/></XML_ROOT>", "not well-formed (invalid token)"},
        {"control character U+0001", "<XML_ROOT><Bid UID=\"1\" JobName=\"a\x01" "b\"/></XML_ROOT>", "not well-formed (invalid token)"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.label);
        const OstRawData raw = parse_ost_xml(c.xml);
        CHECK(raw.empty());
        CHECK(raw.error.find(c.reason) == 0u);
    }
}

// IMP-M01 test plan: windows-1252 and ISO-8859-1 declarations are honoured, the stored value is valid UTF-8.
// ET.parse: windows-1252 'Caf\xE9 \x96' -> 'Café –', ISO-8859-1 'Caf\xE9' -> 'Café'.
TEST(ImportXml, ImpM01_DeclaredWindows1252AndLatin1AreTranscodedToUtf8)
{
    const std::string cp1252 = "<?xml version=\"1.0\" encoding=\"windows-1252\"?><XML_ROOT><Bid UID=\"1\" JobName=\"Caf\xE9 \x96\"/></XML_ROOT>";
    CHECK_STR_EQ(bid_name(cp1252), std::string("Caf\xC3\xA9 \xE2\x80\x93"));
    const std::string latin1 = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><XML_ROOT><Bid UID=\"1\" JobName=\"Caf\xE9\"/></XML_ROOT>";
    CHECK_STR_EQ(bid_name(latin1), std::string("Caf\xC3\xA9"));
    // Python spells the same codecs many ways
    for (const char *label : {"cp1252", "Windows-1252", "windows_1252", "WINDOWS-1252"})
    {
        SCOPED_CONTEXT(std::string() + label);
        const std::string xml = std::string("<?xml version=\"1.0\" encoding=\"") + label + "\"?><XML_ROOT><Bid UID=\"1\" JobName=\"\x80\x96\"/></XML_ROOT>";
        CHECK_STR_EQ(bid_name(xml), std::string("\xE2\x82\xAC\xE2\x80\x93"));
    }
    for (const char *label : {"latin-1", "latin1", "iso8859-1", "ISO_8859-1", "l1", "cp819"})
    {
        SCOPED_CONTEXT(std::string() + label);
        const std::string xml = std::string("<?xml version=\"1.0\" encoding=\"") + label + "\"?><XML_ROOT><Bid UID=\"1\" JobName=\"\x80\xE9\"/></XML_ROOT>";
        CHECK_STR_EQ(bid_name(xml), std::string("\xC2\x80\xC3\xA9"));
    }
    // bytes that windows-1252 does not define are rejected by expat (ISO-8859-1 maps them to C1 controls)
    for (const char *byte : {"\x81", "\x8D", "\x8F", "\x90", "\x9D"})
    {
        SCOPED_CONTEXT(std::to_string(static_cast<unsigned char>(byte[0])));
        CHECK_STR_EQ(bid_name(std::string("<?xml version=\"1.0\" encoding=\"windows-1252\"?><XML_ROOT><Bid UID=\"1\" JobName=\"") + byte + "\"/></XML_ROOT>"),
                     std::string("<rejected>"));
        CHECK(bid_name(std::string("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><XML_ROOT><Bid UID=\"1\" JobName=\"") + byte + "\"/></XML_ROOT>") !=
              std::string("<rejected>"));
    }
}

// IMP-M01 test plan: "Already equal on both sides: UTF-8 BOM and UTF-16LE files are accepted" (and still are).
TEST(ImportXml, ImpM01_Utf8BomAndUtf16FilesStayAccepted)
{
    const std::string xml = "<XML_ROOT><Bid UID=\"1\" JobName=\"Tank\"/></XML_ROOT>";
    CHECK_STR_EQ(bid_name(xml), std::string("Tank"));
    CHECK_STR_EQ(bid_name("\xEF\xBB\xBF" + xml), std::string("Tank"));
    for (bool big_endian : {false, true})
    {
        for (bool bom : {false, true})
        {
            SCOPED_CONTEXT(std::string(big_endian ? "BE" : "LE") + (bom ? " with BOM" : " without BOM"));
            CHECK_STR_EQ(bid_name(utf16(xml, big_endian, bom)), std::string("Tank"));
            CHECK_STR_EQ(bid_name(utf16("<?xml version=\"1.0\" encoding=\"UTF-16\"?>" + xml, big_endian, bom)), std::string("Tank"));
        }
    }
    // a UTF-8 file with non-ASCII text stays as it is
    CHECK_STR_EQ(bid_name("<XML_ROOT><Bid UID=\"1\" JobName=\"Caf\xC3\xA9 \xE2\x80\x93 \xF0\x9F\x98\x80\"/></XML_ROOT>"),
                 std::string("Caf\xC3\xA9 \xE2\x80\x93 \xF0\x9F\x98\x80"));
    // the declaration contradicting the bytes is an error
    CHECK_STR_EQ(bid_name("<?xml version=\"1.0\" encoding=\"UTF-16\"?>" + xml), std::string("<rejected>"));
    CHECK_STR_EQ(bid_name(utf16("<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + xml, false, true)), std::string("<rejected>"));
}

// ElementTree reports a namespaced element as '{uri}Bid': find("Bid") does not see it, and a namespaced attribute is
// a different key than the plain one.
TEST(ImportXml, ImpM01_NamespacedNamesNeverMatchThePlainNames)
{
    CHECK(parse_ost_xml("<XML_ROOT xmlns=\"urn:x\"><Bid UID=\"1\"/></XML_ROOT>").empty());
    CHECK(parse_ost_xml("<XML_ROOT xmlns:p=\"urn:x\"><p:Bid UID=\"1\"/></XML_ROOT>").empty());
    const OstRawData raw = parse_ost_xml("<XML_ROOT><Bid UID=\"1\" xmlns:p=\"urn:x\" p:UID=\"9\"/></XML_ROOT>");
    REQUIRE(!raw.empty());
    CHECK_STR_EQ(raw.bid_row.at("UID"), std::string("1"));
    CHECK_EQ(raw.bid_row.size(), 2u); // UID and the namespaced {urn:x}UID; the xmlns declaration is not an attribute
    CHECK(parse_ost_xml("<XML_ROOT><Bid xmlns=\"urn:y\" UID=\"1\"/></XML_ROOT>").empty()); // the Bid itself is '{urn:y}Bid'
    CHECK(parse_ost_xml("<XML_ROOT><p:Bid UID=\"1\"/></XML_ROOT>").empty()); // unbound prefix
}

// ---------------------------------------------------------------------------------------------------------------------
// The oracle
// ---------------------------------------------------------------------------------------------------------------------

// Every line of the oracle: the Windows parser's accept/reject decision and, when it accepts, the tree it builds.
TEST(ImportXml, ImpM01_TheStrictFrontEndDecidesLikeElementTreeOnEveryOracleCase)
{
    const std::vector<OracleLine> lines = read_oracle();
    REQUIRE(lines.size() > 400u);
    size_t accepted = 0, rejected = 0, limits = 0;
    for (const OracleLine &line : lines)
    {
        SCOPED_CONTEXT("fixture line " + std::to_string(line.number));
        std::string dump;
        const bool ok = tree_of(line.input, dump);
        if (line.result == "O")
        {
            ++accepted;
            REQUIRE(ok);
            CHECK_STR_EQ(dump, line.payload);
        }
        else if (line.result == "E")
        {
            ++rejected;
            CHECK_FALSE(ok);
        }
        else
        {
            ++limits;
            CHECK_FALSE(ok); // documented limit: Windows accepts, iOS rejects on purpose
        }
    }
    CHECK(accepted > 100u);
    CHECK(rejected > 100u);
    CHECK(limits >= 5u);
}

// Whatever the input, an accepted document is valid UTF-8 (never a nil NSString downstream) and never contains a
// character XML forbids.
TEST(ImportXml, ImpM01_AnAcceptedDocumentIsAlwaysValidUtf8)
{
    const std::vector<OracleLine> lines = read_oracle();
    size_t checked = 0;
    for (const OracleLine &line : lines)
    {
        NormalizedXml n = ost_import::normalize_xml(line.input.data(), line.input.size());
        if (!n.ok)
        {
            CHECK(n.utf8.empty());
            CHECK_FALSE(n.error.empty());
            continue;
        }
        SCOPED_CONTEXT("fixture line " + std::to_string(line.number));
        CHECK(is_valid_utf8(n.utf8));
        for (char c : n.utf8)
            CHECK(static_cast<unsigned char>(c) >= 0x20 || c == '\n' || c == '\t');
        ++checked;
    }
    CHECK(checked > 100u);
    // pseudo-random byte soup: never valid UTF-8 trouble, never a crash
    uint32_t x = 12345;
    for (int round = 0; round < 3000; ++round)
    {
        std::string soup = "<XML_ROOT><Bid UID=\"1\" JobName=\"";
        const int length = static_cast<int>((x >> 8) % 12);
        for (int i = 0; i < length; ++i)
        {
            x = x * 1664525u + 1013904223u;
            soup += static_cast<char>(x >> 24);
        }
        soup += "\"/></XML_ROOT>";
        x = x * 1664525u + 1013904223u;
        NormalizedXml n = ost_import::normalize_xml(soup.data(), soup.size());
        if (n.ok)
            CHECK(is_valid_utf8(n.utf8));
        const OstRawData raw = parse_ost_xml(soup);
        for (const auto &field : raw.bid_row)
            CHECK(is_valid_utf8(field.second));
    }
}

// The messages are ElementTree's (ParseError.__str__: "<expat reason>: line L, column C"): the same reason text, and the
// position where the offending token is easy to name. (The oracle holds the full Windows text of every rejected case.)
TEST(ImportXml, ImpM01_RejectionMessagesCarryTheExpatReasonAndPosition)
{
    struct Case
    {
        std::string xml;
        const char *message;
    };
    const std::vector<Case> cases = {
        {"<XML_ROOT></ROOT>", "mismatched tag: line 1, column 12"},
        {"<XML_ROOT a=\"1\" a=\"2\"/>", "duplicate attribute: line 1, column 16"},
        {"<XML_ROOT/><XML_ROOT/>", "junk after document element: line 1, column 11"},
        {"<XML_ROOT a=1/>", "not well-formed (invalid token): line 1, column 12"},
        {"<XML_ROOT a=\"a<b\"/>", "not well-formed (invalid token): line 1, column 14"},
        {"<XML_ROOT><r/>", "no element found: line 1, column 14"},
        {"", "no element found: line 1, column 0"},
        {"<r>\n\n</x>", "mismatched tag: line 3, column 2"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.xml);
        const NormalizedXml n = ost_import::normalize_xml(c.xml.data(), c.xml.size());
        CHECK_FALSE(n.ok);
        CHECK_STR_EQ(n.error, std::string(c.message));
    }
}

// The documented limits (Windows accepts these, the iOS front end rejects them instead of guessing the tree).
TEST(ImportXml, ImpM01_DocumentedLimitsAreRejectedWithTheirOwnReason)
{
    const std::vector<std::pair<std::string, const char *>> cases = {
        {"<!DOCTYPE r [<!ENTITY a \"b\">]><r/>", "unsupported"},
        {"<!DOCTYPE r [<!ELEMENT r ANY>]><r/>", "unsupported"},
        {"<!DOCTYPE r [<!ATTLIST r a CDATA \"d\">]><r/>", "unsupported"},
        {"<!DOCTYPE r [%a;]><r/>", "unsupported"},
        {"<?xml version=\"1.0\" encoding=\"cp1251\"?><r/>", "unsupported encoding"},
    };
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT(c.first);
        const NormalizedXml n = ost_import::normalize_xml(c.first.data(), c.first.size());
        CHECK_FALSE(n.ok);
        CHECK(n.error.find(c.second) != std::string::npos);
    }
    // an empty internal subset, comments and processing instructions in it are fine
    for (const char *xml : {"<!DOCTYPE r []><r/>", "<!DOCTYPE r [ <!-- c --> <?pi x?> ]><r/>", "<!DOCTYPE r SYSTEM \"x.dtd\"><r/>"})
    {
        SCOPED_CONTEXT(std::string() + xml);
        CHECK(ost_import::normalize_xml(xml, std::strlen(xml)).ok);
    }
}
