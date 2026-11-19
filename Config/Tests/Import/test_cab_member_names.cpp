// IMP-18: CAB member names are decoded like the Windows importer does.
//
// Windows: cpp_extensions/src/cab/cab_compressor.cpp decode_cab_member (lines 83-89) and fdi_notify: a name is decoded as UTF-8
// (MultiByteToWideChar CP_UTF8 with MB_ERR_INVALID_CHARS, strict) when the CFFILE attribute _A_NAME_IS_UTF (0x80) is set,
// otherwise with the system ANSI code page (CP_ACP) and re-encoded as UTF-8; a name that does not decode aborts the listing
// (list_cab returns an empty list) and the extraction. The user's decision (IMP-18): iOS reads names without the UTF flag as
// cp1252, the code page the Windows tests assume (tests/helpers/import_workflow.py _write_legacy_ansi_cab encodes with cp1252).
// The cabinet library (FDI) also refuses a member name of 256 bytes or more.
//
// Oracle: the tables below were produced by a native program that drives the REAL Windows cabinet library (FDICopy) and
// the same MultiByteToWideChar / WideCharToMultiByte calls as cab_compressor.cpp, with the ANSI code page fixed to 1252
// (cabinets with the crafted name bytes 'a' + bytes + 'b', once with attribs 0x80 and once with 0). The iOS API reports
// backslashes as '/', so expected names below have no backslashes unless said otherwise.
#include "TestHarness.hpp"

#include "CabTestSupport.hpp"
#include "Import/CabExtractor.hpp"
#include "Import/CabMemberName.hpp"

#include <string>
#include <vector>

using ost_import::CabEntry;
using ost_import::CabStatus;
using ost_itest::CabBuildOptions;
using ost_itest::CabMember;

namespace
{
    struct NameCase
    {
        const char *label;
        std::string bytes;      // the stored name: 'a' + bytes + 'b'
        bool utf_ok;            // FDI accepts the name with the UTF-8 flag (attribs 0x80)
        std::string utf_name;   // its name as UTF-8 (when utf_ok)
        std::string ansi_name;  // its name without the flag (cp1252 -> UTF-8)
    };
    const std::vector<NameCase> NAME_CASES = {
        {"ascii", std::string("\x61\x61\x62\x63\x62", 5), true, std::string("\x61\x61\x62\x63\x62", 5), std::string("\x61\x61\x62\x63\x62", 5)},
        {"e acute", std::string("\x61\xc3\xa9\x62", 4), true, std::string("\x61\xc3\xa9\x62", 4), std::string("\x61\xc3\x83\xc2\xa9\x62", 6)},
        {"euro", std::string("\x61\xe2\x82\xac\x62", 5), true, std::string("\x61\xe2\x82\xac\x62", 5), std::string("\x61\xc3\xa2\xe2\x80\x9a\xc2\xac\x62", 9)},
        {"emoji", std::string("\x61\xf0\x9f\x98\x80\x62", 6), true, std::string("\x61\xf0\x9f\x98\x80\x62", 6), std::string("\x61\xc3\xb0\xc5\xb8\xcb\x9c\xe2\x82\xac\x62", 11)},
        {"U+FFFD", std::string("\x61\xef\xbf\xbd\x62", 5), true, std::string("\x61\xef\xbf\xbd\x62", 5), std::string("\x61\xc3\xaf\xc2\xbf\xc2\xbd\x62", 8)},
        {"U+FFFE", std::string("\x61\xef\xbf\xbe\x62", 5), true, std::string("\x61\xef\xbf\xbe\x62", 5), std::string("\x61\xc3\xaf\xc2\xbf\xc2\xbe\x62", 8)},
        {"U+FFFF", std::string("\x61\xef\xbf\xbf\x62", 5), true, std::string("\x61\xef\xbf\xbf\x62", 5), std::string("\x61\xc3\xaf\xc2\xbf\xc2\xbf\x62", 8)},
        {"U+10FFFF", std::string("\x61\xf4\x8f\xbf\xbf\x62", 6), true, std::string("\x61\xf4\x8f\xbf\xbf\x62", 6), std::string("\x61\xc3\xb4\xc2\x8f\xc2\xbf\xc2\xbf\x62", 10)},
        {"U+0080", std::string("\x61\xc2\x80\x62", 4), true, std::string("\x61\xc2\x80\x62", 4), std::string("\x61\xc3\x82\xe2\x82\xac\x62", 7)},
        {"U+07FF", std::string("\x61\xdf\xbf\x62", 4), true, std::string("\x61\xdf\xbf\x62", 4), std::string("\x61\xc3\x9f\xc2\xbf\x62", 6)},
        {"U+0800", std::string("\x61\xe0\xa0\x80\x62", 5), true, std::string("\x61\xe0\xa0\x80\x62", 5), std::string("\x61\xc3\xa0\xc2\xa0\xe2\x82\xac\x62", 9)},
        {"U+D7FF", std::string("\x61\xed\x9f\xbf\x62", 5), true, std::string("\x61\xed\x9f\xbf\x62", 5), std::string("\x61\xc3\xad\xc5\xb8\xc2\xbf\x62", 8)},
        {"U+E000", std::string("\x61\xee\x80\x80\x62", 5), true, std::string("\x61\xee\x80\x80\x62", 5), std::string("\x61\xc3\xae\xe2\x82\xac\xe2\x82\xac\x62", 10)},
        {"U+10000", std::string("\x61\xf0\x90\x80\x80\x62", 6), true, std::string("\x61\xf0\x90\x80\x80\x62", 6), std::string("\x61\xc3\xb0\xc2\x90\xe2\x82\xac\xe2\x82\xac\x62", 12)},
        {"overlong NUL", std::string("\x61\xc0\x80\x62", 4), false, "", std::string("\x61\xc3\x80\xe2\x82\xac\x62", 7)},
        {"overlong slash", std::string("\x61\xc1\xbf\x62", 4), false, "", std::string("\x61\xc3\x81\xc2\xbf\x62", 6)},
        {"overlong 3 byte", std::string("\x61\xe0\x80\x80\x62", 5), false, "", std::string("\x61\xc3\xa0\xe2\x82\xac\xe2\x82\xac\x62", 10)},
        {"overlong 3 byte max", std::string("\x61\xe0\x9f\xbf\x62", 5), false, "", std::string("\x61\xc3\xa0\xc5\xb8\xc2\xbf\x62", 8)},
        {"overlong 4 byte", std::string("\x61\xf0\x80\x80\x80\x62", 6), false, "", std::string("\x61\xc3\xb0\xe2\x82\xac\xe2\x82\xac\xe2\x82\xac\x62", 13)},
        {"overlong 4 byte max", std::string("\x61\xf0\x8f\xbf\xbf\x62", 6), false, "", std::string("\x61\xc3\xb0\xc2\x8f\xc2\xbf\xc2\xbf\x62", 10)},
        {"surrogate D800", std::string("\x61\xed\xa0\x80\x62", 5), false, "", std::string("\x61\xc3\xad\xc2\xa0\xe2\x82\xac\x62", 9)},
        {"surrogate DFFF", std::string("\x61\xed\xbf\xbf\x62", 5), false, "", std::string("\x61\xc3\xad\xc2\xbf\xc2\xbf\x62", 8)},
        {"above 10FFFF", std::string("\x61\xf4\x90\x80\x80\x62", 6), false, "", std::string("\x61\xc3\xb4\xc2\x90\xe2\x82\xac\xe2\x82\xac\x62", 12)},
        {"F5 lead", std::string("\x61\xf5\x80\x80\x80\x62", 6), false, "", std::string("\x61\xc3\xb5\xe2\x82\xac\xe2\x82\xac\xe2\x82\xac\x62", 13)},
        {"5 byte form", std::string("\x61\xf8\x88\x80\x80\x80\x62", 7), false, "", std::string("\x61\xc3\xb8\xcb\x86\xe2\x82\xac\xe2\x82\xac\xe2\x82\xac\x62", 15)},
        {"6 byte form", std::string("\x61\xfc\x84\x80\x80\x80\x80\x62", 8), false, "", std::string("\x61\xc3\xbc\xe2\x80\x9e\xe2\x82\xac\xe2\x82\xac\xe2\x82\xac\xe2\x82\xac\x62", 19)},
        {"lone continuation 80", std::string("\x61\x80\x62", 3), false, "", std::string("\x61\xe2\x82\xac\x62", 5)},
        {"lone continuation BF", std::string("\x61\xbf\x62", 3), false, "", std::string("\x61\xc2\xbf\x62", 4)},
        {"truncated 2", std::string("\x61\xc3\x62", 3), false, "", std::string("\x61\xc3\x83\x62", 4)},
        {"truncated 3", std::string("\x61\xe2\x82\x62", 4), false, "", std::string("\x61\xc3\xa2\xe2\x80\x9a\x62", 7)},
        {"truncated 4", std::string("\x61\xf0\x9f\x98\x62", 5), false, "", std::string("\x61\xc3\xb0\xc5\xb8\xcb\x9c\x62", 8)},
        {"bad continuation 2", std::string("\x61\xc3\x28\x62", 4), false, "", std::string("\x61\xc3\x83\x28\x62", 5)},
        {"bad continuation 3", std::string("\x61\xe2\x28\xa1\x62", 5), false, "", std::string("\x61\xc3\xa2\x28\xc2\xa1\x62", 7)},
        {"bad continuation 4", std::string("\x61\xf0\x28\x8c\xbc\x62", 6), false, "", std::string("\x61\xc3\xb0\x28\xc5\x92\xc2\xbc\x62", 9)},
        {"FE", std::string("\x61\xfe\x62", 3), false, "", std::string("\x61\xc3\xbe\x62", 4)},
        {"FF", std::string("\x61\xff\x62", 3), false, "", std::string("\x61\xc3\xbf\x62", 4)},
        {"cp1252 dash", std::string("\x61\x96\x62", 3), false, "", std::string("\x61\xe2\x80\x93\x62", 5)},
        {"cp1252 e acute", std::string("\x61\xe9\x62", 3), false, "", std::string("\x61\xc3\xa9\x62", 4)},
    };

    // Windows oracle: the code point MultiByteToWideChar(1252, 0) gives for the byte 0x01..0xFF (index = byte - 1)
    const unsigned CP1252_WINDOWS[255] = {
        0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C,
        0x000D, 0x000E, 0x000F, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018,
        0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024,
        0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F, 0x0030,
        0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C,
        0x003D, 0x003E, 0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048,
        0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054,
        0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F, 0x0060,
        0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C,
        0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078,
        0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F, 0x20AC, 0x0081, 0x201A, 0x0192, 0x201E,
        0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090,
        0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153,
        0x009D, 0x017E, 0x0178, 0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8,
        0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF, 0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4,
        0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF, 0x00C0,
        0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC,
        0x00CD, 0x00CE, 0x00CF, 0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7, 0x00D8,
        0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF, 0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4,
        0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF, 0x00F0,
        0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC,
        0x00FD, 0x00FE, 0x00FF,
    };

    std::vector<CabEntry> list_one(const std::string &name_bytes, uint16_t attribs)
    {
        CabMember member{name_bytes, "x"};
        member.attribs = attribs;
        const std::string cab = ost_itest::build_cab({member}, CabBuildOptions());
        return ost_import::list_cab(cab.data(), cab.size());
    }

    std::string utf8_of(unsigned code_point)
    {
        std::string out;
        if (code_point < 0x80)
        {
            out += static_cast<char>(code_point);
        }
        else if (code_point < 0x800)
        {
            out += static_cast<char>(0xC0 | (code_point >> 6));
            out += static_cast<char>(0x80 | (code_point & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xE0 | (code_point >> 12));
            out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code_point & 0x3F));
        }
        return out;
    }
}

// Names with the UTF-8 flag decode as strict UTF-8: overlong forms, surrogates, code points above U+10FFFF, 5/6 byte forms and
// truncated sequences fail (the listing is empty); the noncharacters U+FFFE / U+FFFF and U+10FFFF are accepted.
TEST(ImportCabMemberNames, Imp18_NamesWithTheUtf8FlagAreDecodedAsStrictUtf8)
{
    for (const NameCase &c : NAME_CASES)
    {
        SCOPED_CONTEXT(std::string("case ") + c.label);
        const std::vector<CabEntry> entries = list_one(c.bytes, 0x80);
        if (c.utf_ok)
        {
            REQUIRE_EQ(entries.size(), 1u);
            CHECK_EQ(entries[0].name, c.utf_name);
            CHECK_EQ(entries[0].attribs, uint16_t(0x80));
        }
        else
        {
            CHECK(entries.empty());
        }
        // the direct decoder
        std::string decoded = "untouched";
        CHECK_EQ(ost_import::decode_cab_member_name(c.bytes, 0x80, decoded), c.utf_ok);
        if (c.utf_ok)
            CHECK_EQ(decoded, c.utf_name);
    }
}

// Names without the flag are cp1252, whatever the bytes are (a name that happens to be UTF-8 comes out as mojibake, exactly
// as it does on Windows with a cp1252 system code page).
TEST(ImportCabMemberNames, Imp18_NamesWithoutTheUtf8FlagAreDecodedAsCp1252)
{
    for (const NameCase &c : NAME_CASES)
    {
        SCOPED_CONTEXT(std::string("case ") + c.label);
        const std::vector<CabEntry> entries = list_one(c.bytes, 0);
        REQUIRE_EQ(entries.size(), 1u);
        CHECK_EQ(entries[0].name, c.ansi_name);
        std::string decoded;
        CHECK(ost_import::decode_cab_member_name(c.bytes, 0, decoded));
        CHECK_EQ(decoded, c.ansi_name);
    }
}

// Every single byte 0x01..0xFF through cp1252 (the table is what MultiByteToWideChar(1252) returns on Windows: the five
// undefined bytes 0x81, 0x8D, 0x8F, 0x90, 0x9D map to the C1 controls of the same value).
TEST(ImportCabMemberNames, Imp18_EveryByteMapsLikeWindowsCp1252)
{
    for (unsigned byte = 1; byte <= 255; ++byte)
    {
        SCOPED_CONTEXT("byte " + std::to_string(byte));
        const std::string raw = std::string("A") + static_cast<char>(byte) + "Z";
        const std::vector<CabEntry> entries = list_one(raw, 0);
        REQUIRE_EQ(entries.size(), 1u);
        const std::string mapped = byte == '\\' ? std::string("/") : utf8_of(CP1252_WINDOWS[byte - 1]);
        CHECK_EQ(entries[0].name, "A" + mapped + "Z");
        std::string decoded;
        CHECK(ost_import::decode_cab_member_name(raw, 0, decoded));
        CHECK_EQ(decoded, "A" + utf8_of(CP1252_WINDOWS[byte - 1]) + "Z"); // the decoder itself keeps the backslash
    }
    // ASCII is unchanged
    for (unsigned byte = 0x20; byte < 0x7F; ++byte)
        CHECK_EQ(utf8_of(CP1252_WINDOWS[byte - 1]), std::string(1, static_cast<char>(byte)));
}

// Only bit 0x80 of the attributes decides (read-only, hidden, system, archive and execute bits are ignored).
TEST(ImportCabMemberNames, Imp18_OnlyTheUtfFlagBitDecidesHowANameIsDecoded)
{
    const std::string raw = "caf\xC3\xA9";
    for (unsigned attribs : {0x00u, 0x01u, 0x02u, 0x04u, 0x20u, 0x40u, 0x7Fu, 0x100u, 0x7F00u, 0x8000u})
    {
        SCOPED_CONTEXT("attribs " + std::to_string(attribs));
        const std::vector<CabEntry> entries = list_one(raw, static_cast<uint16_t>(attribs));
        REQUIRE_EQ(entries.size(), 1u);
        CHECK_EQ(entries[0].name, std::string("caf\xC3\x83\xC2\xA9")); // cp1252: 'Ã' '©'
        CHECK_EQ(entries[0].attribs, static_cast<uint16_t>(attribs));
    }
    for (unsigned attribs : {0x80u, 0x81u, 0xA0u, 0xC0u, 0xFFu, 0x180u, 0x8080u, 0xFF80u})
    {
        SCOPED_CONTEXT("attribs " + std::to_string(attribs));
        const std::vector<CabEntry> entries = list_one(raw, static_cast<uint16_t>(attribs));
        REQUIRE_EQ(entries.size(), 1u);
        CHECK_EQ(entries[0].name, raw);
    }
}

// The cabinet library refuses a name of 256 bytes or more (255 is fine): measured with FDI. With the UTF flag too.
TEST(ImportCabMemberNames, Imp18_ANameOf256BytesOrMoreFailsTheListing)
{
    for (uint16_t attribs : {uint16_t(0), uint16_t(0x80)})
    {
        for (size_t length : {size_t(1), size_t(254), size_t(255)})
        {
            SCOPED_CONTEXT("length " + std::to_string(length));
            const std::vector<CabEntry> entries = list_one(std::string(length, 'n'), attribs);
            REQUIRE_EQ(entries.size(), 1u);
            CHECK_EQ(entries[0].name.size(), length);
        }
        for (size_t length : {size_t(256), size_t(257), size_t(300), size_t(1000)})
        {
            SCOPED_CONTEXT("length " + std::to_string(length));
            CHECK(list_one(std::string(length, 'n'), attribs).empty());
        }
    }
    // the limit counts the stored bytes: 128 cp1252 characters that need two bytes each in UTF-8 are 128 stored bytes
    CHECK_EQ(list_one(std::string(255, '\xE9'), 0).size(), 1u);
    CHECK_EQ(list_one(std::string(255, '\xE9'), 0)[0].name.size(), 510u);
    CHECK(list_one(std::string(256, '\xE9'), 0).empty());
}

// One name that does not decode empties the whole listing (list_cab clears the names when the notification fails) and fails
// every extraction path; nothing is delivered or written.
TEST(ImportCabMemberNames, Imp18_OneUndecodableNameFailsTheListingAndTheExtraction)
{
    CabMember good1{"Project.ost", "<XML_ROOT />"};
    CabMember bad{"bad \xC3\x28 name.pdf", "%PDF"};
    bad.attribs = 0x80;
    CabMember good2{"TempImages!.tmp\\ok.pdf", "%PDF-ok"};
    const std::string cab = ost_itest::build_cab({good1, bad, good2}, CabBuildOptions());
    CHECK(ost_import::list_cab(cab.data(), cab.size()).empty());

    // the same cabinet without the flag on the bad member is fine
    bad.attribs = 0;
    const std::string fine = ost_itest::build_cab({good1, bad, good2}, CabBuildOptions());
    CHECK_EQ(ost_import::list_cab(fine.data(), fine.size()).size(), 3u);

    const ost_import::CabExtractResult member = ost_import::extract_cab_member(cab.data(), cab.size(), "Project.ost");
    CHECK(member.status == CabStatus::Corrupt);
    CHECK(member.data.empty());
    CHECK(ost_import::extract_cab_file(cab.data(), cab.size(), "Project.ost").empty());
    CHECK(ost_import::verify_cab(cab.data(), cab.size()).status == CabStatus::Corrupt);

    struct Counting : public ost_import::CabMemberSink
    {
        size_t members = 0;
        ost_import::CabMemberAction begin_member(const CabEntry &) override
        {
            ++members;
            return ost_import::CabMemberAction::Extract;
        }
        bool write(const char *, size_t) override { return true; }
        bool end_member() override { return true; }
    } sink;
    const ost_import::CabExtractResult extracted = ost_import::extract_cab(cab.data(), cab.size(), sink);
    CHECK(extracted.status == CabStatus::Corrupt);
    CHECK(extracted.detail.find("name") != std::string::npos);
    CHECK_EQ(sink.members, 0u);
}

// Port of tests/integration/native/test_osp_cab.py::test_osp_imports_legacy_ansi_members_from_unicode_archive_path (the part about
// the member names): the cabinet shape of tests/helpers/import_workflow.py::_write_legacy_ansi_cab with cp1252 names (en dash =
// 0x96, no UTF flag): list_cab gives the UTF-8 names; the Windows test compares with the Python strings, where the PDF member
// has backslashes (iOS reports '/'), and the member can be extracted under its UTF-8 name.
TEST(ImportCabMemberNames, Imp18_TheLegacyAnsiFixtureOfTheWindowsTestsListsItsUtf8Names)
{
    const std::string project_cp1252 = "26-061 412 \x96 Corporate Lot K, KS";
    const std::string project = "26-061 412 \xE2\x80\x93 Corporate Lot K, KS";
    const std::string ost_member = project + ".ost";
    const std::string image_member = "TempImages!.tmp/1 Estimates through 1-12-2015/2026/" + project + "/01. Drawings/A0-0.0.pdf";
    const std::string image_member_cp1252 = "TempImages!.tmp\\1 Estimates through 1-12-2015\\2026\\" + project_cp1252 + "\\01. Drawings\\A0-0.0.pdf";
    const std::string cab = ost_itest::build_cab({{project_cp1252 + ".ost", "<XML_ROOT />"}, {image_member_cp1252, "%PDF-1.4 fixture"}}, CabBuildOptions());
    const std::vector<CabEntry> entries = ost_import::list_cab(cab.data(), cab.size());
    REQUIRE_EQ(entries.size(), 2u);
    CHECK_EQ(entries[0].name, ost_member);
    CHECK_EQ(entries[1].name, image_member);
    CHECK_EQ(entries[0].attribs, uint16_t(0));

    const ost_import::CabExtractResult ost = ost_import::extract_cab_member(cab.data(), cab.size(), ost_member);
    CHECK(ost.status == CabStatus::Ok);
    CHECK_STR_EQ(std::string(ost.data.begin(), ost.data.end()), std::string("<XML_ROOT />"));
    const ost_import::CabExtractResult pdf = ost_import::extract_cab_member(cab.data(), cab.size(), image_member);
    CHECK(pdf.status == CabStatus::Ok);
    CHECK_STR_EQ(std::string(pdf.data.begin(), pdf.data.end()), std::string("%PDF-1.4 fixture"));
    // the raw cp1252 spelling is not a member name any more
    CHECK(ost_import::extract_cab_member(cab.data(), cab.size(), project_cp1252 + ".ost").status == CabStatus::NotFound);
    // and the same cabinet, flagged as UTF-8 with UTF-8 names, lists the same names
    const std::string utf_cab = [&]
    {
        CabMember a{ost_member, "<XML_ROOT />"};
        CabMember b{"TempImages!.tmp\\1 Estimates through 1-12-2015\\2026\\" + project + "\\01. Drawings\\A0-0.0.pdf", "%PDF-1.4 fixture"};
        a.attribs = b.attribs = 0x80;
        return ost_itest::build_cab({a, b}, CabBuildOptions());
    }();
    const std::vector<CabEntry> utf_entries = ost_import::list_cab(utf_cab.data(), utf_cab.size());
    REQUIRE_EQ(utf_entries.size(), 2u);
    CHECK_EQ(utf_entries[0].name, ost_member);
    CHECK_EQ(utf_entries[1].name, image_member);
}

// Port of ::test_native_cab_round_trip_preserves_utf8_paths_and_member_names: FCI writes a name with non-ASCII characters
// as UTF-8 with the flag; it must list as written.
TEST(ImportCabMemberNames, Imp18_AUtf8FlaggedUnicodeNameIsListedAsWritten)
{
    CabMember member{"TempImages!.tmp\\source \xE2\x80\x93 drawing.pdf", "%PDF-1.4 unicode"};
    member.attribs = 0x80;
    const std::string cab = ost_itest::build_cab({member}, CabBuildOptions());
    const std::vector<CabEntry> entries = ost_import::list_cab(cab.data(), cab.size());
    REQUIRE_EQ(entries.size(), 1u);
    CHECK_EQ(entries[0].name, std::string("TempImages!.tmp/source \xE2\x80\x93 drawing.pdf"));
    const ost_import::CabExtractResult result = ost_import::extract_cab_member(cab.data(), cab.size(), entries[0].name);
    CHECK(result.status == CabStatus::Ok);
    CHECK_STR_EQ(std::string(result.data.begin(), result.data.end()), std::string("%PDF-1.4 unicode"));
}

// The name of a file table entry needs at least one byte after its 16 byte header: the NUL, or the bytes of a name that runs to
// the end of the data. Measured with FDI on cabinets whose file table is the last thing in the file: a table cut right after
// an entry header fails the cabinet (corrupt), 'a' without a terminator is the member "a", 'a' + NUL too.
TEST(ImportCabMemberNames, Imp18_AFileTableCutRightAfterAnEntryHeaderIsCorrupt)
{
    auto cabinet = [](const std::string &tail)
    {
        std::string out = "MSCF";
        ost_itest::put32(out, 0);
        ost_itest::put32(out, 0);
        ost_itest::put32(out, 0);
        ost_itest::put32(out, 56); // coffFiles: the table follows the data block (36 + 8 + 12)
        ost_itest::put32(out, 0);
        out += static_cast<char>(3);
        out += static_cast<char>(1);
        ost_itest::put16(out, 1); // cFolders
        ost_itest::put16(out, 1); // cFiles
        ost_itest::put16(out, 0);
        ost_itest::put16(out, 1);
        ost_itest::put16(out, 0);
        ost_itest::put32(out, 44); // the folder record: data at 44, one stored block
        ost_itest::put16(out, 1);
        ost_itest::put16(out, 0);
        ost_itest::put32(out, 0); // the CFDATA block: no checksum, 4 bytes
        ost_itest::put16(out, 4);
        ost_itest::put16(out, 4);
        out += "abcd";
        ost_itest::put32(out, 0); // the entry header of a zero length member in folder 0
        ost_itest::put32(out, 0);
        ost_itest::put16(out, 0);
        ost_itest::put16(out, 0);
        ost_itest::put16(out, 0);
        ost_itest::put16(out, 0);
        return out + tail;
    };
    const std::string header_only = cabinet("");
    CHECK(ost_import::list_cab(header_only.data(), header_only.size()).empty());
    CHECK(ost_import::verify_cab(header_only.data(), header_only.size()).status == CabStatus::Corrupt);
    for (const std::string &tail : {std::string("a"), std::string("a\0", 2)})
    {
        const std::string text = cabinet(tail);
        const std::vector<CabEntry> entries = ost_import::list_cab(text.data(), text.size());
        REQUIRE_EQ(entries.size(), 1u);
        CHECK_EQ(entries[0].name, std::string("a"));
        CHECK(ost_import::verify_cab(text.data(), text.size()).status == CabStatus::Ok);
    }
}

// The decoded name is valid UTF-8 for every input (never a name that becomes a nil NSString in the Objective-C layer): all
// 255 single bytes, without the flag.
TEST(ImportCabMemberNames, Imp18_ADecodedNameIsAlwaysValidUtf8)
{
    for (unsigned byte = 1; byte <= 255; ++byte)
    {
        SCOPED_CONTEXT("byte " + std::to_string(byte));
        std::string decoded;
        REQUIRE(ost_import::decode_cab_member_name(std::string(1, static_cast<char>(byte)), 0, decoded));
        // re-validate with the flagged decoder: valid UTF-8 round-trips
        std::string again;
        CHECK(ost_import::decode_cab_member_name(decoded, 0x80, again));
        CHECK_EQ(again, decoded);
    }
}
