// Layer: Domain. See PositionParser.hpp for the Windows sources this file ports.
#include "PositionParser.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <string_view>

namespace ost_domain
{
    namespace
    {
        using u32 = std::u32string;
        constexpr size_t npos = static_cast<size_t>(-1);

        // -----------------------------------------------------------------------------------------------------
        // UTF-8
        // -----------------------------------------------------------------------------------------------------

        // Invalid sequences decode to U+FFFD (one per offending byte); that character is neither whitespace, a
        // digit nor a word character, so it can never be part of a valid elevation or number.
        u32 decode_utf8(std::string_view s)
        {
            u32 out;
            out.reserve(s.size());
            const size_t n = s.size();
            size_t i = 0;
            while (i < n)
            {
                const unsigned char c = static_cast<unsigned char>(s[i]);
                if (c < 0x80)
                {
                    out.push_back(c);
                    ++i;
                    continue;
                }
                size_t len = 0;
                char32_t cp = 0, min_cp = 0;
                if ((c & 0xE0) == 0xC0)
                {
                    len = 2;
                    cp = c & 0x1F;
                    min_cp = 0x80;
                }
                else if ((c & 0xF0) == 0xE0)
                {
                    len = 3;
                    cp = c & 0x0F;
                    min_cp = 0x800;
                }
                else if ((c & 0xF8) == 0xF0)
                {
                    len = 4;
                    cp = c & 0x07;
                    min_cp = 0x10000;
                }
                bool ok = len != 0 && i + len <= n;
                for (size_t k = 1; ok && k < len; ++k)
                {
                    const unsigned char cc = static_cast<unsigned char>(s[i + k]);
                    if ((cc & 0xC0) != 0x80)
                        ok = false;
                    else
                        cp = (cp << 6) | (cc & 0x3F);
                }
                if (!ok || cp < min_cp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
                {
                    out.push_back(0xFFFD);
                    ++i;
                    continue;
                }
                out.push_back(cp);
                i += len;
            }
            return out;
        }

        void append_utf8(std::string &out, char32_t cp)
        {
            if (cp < 0x80)
            {
                out.push_back(static_cast<char>(cp));
            }
            else if (cp < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else if (cp < 0x10000)
            {
                out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            }
        }

        std::string encode_utf8(const u32 &s)
        {
            std::string out;
            out.reserve(s.size());
            for (char32_t c : s)
                append_utf8(out, c);
            return out;
        }

        // -----------------------------------------------------------------------------------------------------
        // Python Unicode character classes
        // -----------------------------------------------------------------------------------------------------

        // str.isspace() == the regex \s for Unicode str patterns (identical sets, verified on Python 3.11.4).
        bool is_space(char32_t c)
        {
            switch (c)
            {
            case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D:
            case 0x1C: case 0x1D: case 0x1E: case 0x1F: case 0x20:
            case 0x85: case 0xA0: case 0x1680:
            case 0x2028: case 0x2029: case 0x202F: case 0x205F: case 0x3000:
                return true;
            default:
                return c >= 0x2000 && c <= 0x200A;
            }
        }

        // Python 3.11 str.isalnum() ranges (Unicode 14.0.0), without '_'.
        static const char32_t kWordRanges[][2] = {
            {0x30, 0x39}, {0x41, 0x5A}, {0x61, 0x7A}, {0xAA, 0xAA},
            {0xB2, 0xB3}, {0xB5, 0xB5}, {0xB9, 0xBA}, {0xBC, 0xBE},
            {0xC0, 0xD6}, {0xD8, 0xF6}, {0xF8, 0x2C1}, {0x2C6, 0x2D1},
            {0x2E0, 0x2E4}, {0x2EC, 0x2EC}, {0x2EE, 0x2EE}, {0x370, 0x374},
            {0x376, 0x377}, {0x37A, 0x37D}, {0x37F, 0x37F}, {0x386, 0x386},
            {0x388, 0x38A}, {0x38C, 0x38C}, {0x38E, 0x3A1}, {0x3A3, 0x3F5},
            {0x3F7, 0x481}, {0x48A, 0x52F}, {0x531, 0x556}, {0x559, 0x559},
            {0x560, 0x588}, {0x5D0, 0x5EA}, {0x5EF, 0x5F2}, {0x620, 0x64A},
            {0x660, 0x669}, {0x66E, 0x66F}, {0x671, 0x6D3}, {0x6D5, 0x6D5},
            {0x6E5, 0x6E6}, {0x6EE, 0x6FC}, {0x6FF, 0x6FF}, {0x710, 0x710},
            {0x712, 0x72F}, {0x74D, 0x7A5}, {0x7B1, 0x7B1}, {0x7C0, 0x7EA},
            {0x7F4, 0x7F5}, {0x7FA, 0x7FA}, {0x800, 0x815}, {0x81A, 0x81A},
            {0x824, 0x824}, {0x828, 0x828}, {0x840, 0x858}, {0x860, 0x86A},
            {0x870, 0x887}, {0x889, 0x88E}, {0x8A0, 0x8C9}, {0x904, 0x939},
            {0x93D, 0x93D}, {0x950, 0x950}, {0x958, 0x961}, {0x966, 0x96F},
            {0x971, 0x980}, {0x985, 0x98C}, {0x98F, 0x990}, {0x993, 0x9A8},
            {0x9AA, 0x9B0}, {0x9B2, 0x9B2}, {0x9B6, 0x9B9}, {0x9BD, 0x9BD},
            {0x9CE, 0x9CE}, {0x9DC, 0x9DD}, {0x9DF, 0x9E1}, {0x9E6, 0x9F1},
            {0x9F4, 0x9F9}, {0x9FC, 0x9FC}, {0xA05, 0xA0A}, {0xA0F, 0xA10},
            {0xA13, 0xA28}, {0xA2A, 0xA30}, {0xA32, 0xA33}, {0xA35, 0xA36},
            {0xA38, 0xA39}, {0xA59, 0xA5C}, {0xA5E, 0xA5E}, {0xA66, 0xA6F},
            {0xA72, 0xA74}, {0xA85, 0xA8D}, {0xA8F, 0xA91}, {0xA93, 0xAA8},
            {0xAAA, 0xAB0}, {0xAB2, 0xAB3}, {0xAB5, 0xAB9}, {0xABD, 0xABD},
            {0xAD0, 0xAD0}, {0xAE0, 0xAE1}, {0xAE6, 0xAEF}, {0xAF9, 0xAF9},
            {0xB05, 0xB0C}, {0xB0F, 0xB10}, {0xB13, 0xB28}, {0xB2A, 0xB30},
            {0xB32, 0xB33}, {0xB35, 0xB39}, {0xB3D, 0xB3D}, {0xB5C, 0xB5D},
            {0xB5F, 0xB61}, {0xB66, 0xB6F}, {0xB71, 0xB77}, {0xB83, 0xB83},
            {0xB85, 0xB8A}, {0xB8E, 0xB90}, {0xB92, 0xB95}, {0xB99, 0xB9A},
            {0xB9C, 0xB9C}, {0xB9E, 0xB9F}, {0xBA3, 0xBA4}, {0xBA8, 0xBAA},
            {0xBAE, 0xBB9}, {0xBD0, 0xBD0}, {0xBE6, 0xBF2}, {0xC05, 0xC0C},
            {0xC0E, 0xC10}, {0xC12, 0xC28}, {0xC2A, 0xC39}, {0xC3D, 0xC3D},
            {0xC58, 0xC5A}, {0xC5D, 0xC5D}, {0xC60, 0xC61}, {0xC66, 0xC6F},
            {0xC78, 0xC7E}, {0xC80, 0xC80}, {0xC85, 0xC8C}, {0xC8E, 0xC90},
            {0xC92, 0xCA8}, {0xCAA, 0xCB3}, {0xCB5, 0xCB9}, {0xCBD, 0xCBD},
            {0xCDD, 0xCDE}, {0xCE0, 0xCE1}, {0xCE6, 0xCEF}, {0xCF1, 0xCF2},
            {0xD04, 0xD0C}, {0xD0E, 0xD10}, {0xD12, 0xD3A}, {0xD3D, 0xD3D},
            {0xD4E, 0xD4E}, {0xD54, 0xD56}, {0xD58, 0xD61}, {0xD66, 0xD78},
            {0xD7A, 0xD7F}, {0xD85, 0xD96}, {0xD9A, 0xDB1}, {0xDB3, 0xDBB},
            {0xDBD, 0xDBD}, {0xDC0, 0xDC6}, {0xDE6, 0xDEF}, {0xE01, 0xE30},
            {0xE32, 0xE33}, {0xE40, 0xE46}, {0xE50, 0xE59}, {0xE81, 0xE82},
            {0xE84, 0xE84}, {0xE86, 0xE8A}, {0xE8C, 0xEA3}, {0xEA5, 0xEA5},
            {0xEA7, 0xEB0}, {0xEB2, 0xEB3}, {0xEBD, 0xEBD}, {0xEC0, 0xEC4},
            {0xEC6, 0xEC6}, {0xED0, 0xED9}, {0xEDC, 0xEDF}, {0xF00, 0xF00},
            {0xF20, 0xF33}, {0xF40, 0xF47}, {0xF49, 0xF6C}, {0xF88, 0xF8C},
            {0x1000, 0x102A}, {0x103F, 0x1049}, {0x1050, 0x1055}, {0x105A, 0x105D},
            {0x1061, 0x1061}, {0x1065, 0x1066}, {0x106E, 0x1070}, {0x1075, 0x1081},
            {0x108E, 0x108E}, {0x1090, 0x1099}, {0x10A0, 0x10C5}, {0x10C7, 0x10C7},
            {0x10CD, 0x10CD}, {0x10D0, 0x10FA}, {0x10FC, 0x1248}, {0x124A, 0x124D},
            {0x1250, 0x1256}, {0x1258, 0x1258}, {0x125A, 0x125D}, {0x1260, 0x1288},
            {0x128A, 0x128D}, {0x1290, 0x12B0}, {0x12B2, 0x12B5}, {0x12B8, 0x12BE},
            {0x12C0, 0x12C0}, {0x12C2, 0x12C5}, {0x12C8, 0x12D6}, {0x12D8, 0x1310},
            {0x1312, 0x1315}, {0x1318, 0x135A}, {0x1369, 0x137C}, {0x1380, 0x138F},
            {0x13A0, 0x13F5}, {0x13F8, 0x13FD}, {0x1401, 0x166C}, {0x166F, 0x167F},
            {0x1681, 0x169A}, {0x16A0, 0x16EA}, {0x16EE, 0x16F8}, {0x1700, 0x1711},
            {0x171F, 0x1731}, {0x1740, 0x1751}, {0x1760, 0x176C}, {0x176E, 0x1770},
            {0x1780, 0x17B3}, {0x17D7, 0x17D7}, {0x17DC, 0x17DC}, {0x17E0, 0x17E9},
            {0x17F0, 0x17F9}, {0x1810, 0x1819}, {0x1820, 0x1878}, {0x1880, 0x1884},
            {0x1887, 0x18A8}, {0x18AA, 0x18AA}, {0x18B0, 0x18F5}, {0x1900, 0x191E},
            {0x1946, 0x196D}, {0x1970, 0x1974}, {0x1980, 0x19AB}, {0x19B0, 0x19C9},
            {0x19D0, 0x19DA}, {0x1A00, 0x1A16}, {0x1A20, 0x1A54}, {0x1A80, 0x1A89},
            {0x1A90, 0x1A99}, {0x1AA7, 0x1AA7}, {0x1B05, 0x1B33}, {0x1B45, 0x1B4C},
            {0x1B50, 0x1B59}, {0x1B83, 0x1BA0}, {0x1BAE, 0x1BE5}, {0x1C00, 0x1C23},
            {0x1C40, 0x1C49}, {0x1C4D, 0x1C7D}, {0x1C80, 0x1C88}, {0x1C90, 0x1CBA},
            {0x1CBD, 0x1CBF}, {0x1CE9, 0x1CEC}, {0x1CEE, 0x1CF3}, {0x1CF5, 0x1CF6},
            {0x1CFA, 0x1CFA}, {0x1D00, 0x1DBF}, {0x1E00, 0x1F15}, {0x1F18, 0x1F1D},
            {0x1F20, 0x1F45}, {0x1F48, 0x1F4D}, {0x1F50, 0x1F57}, {0x1F59, 0x1F59},
            {0x1F5B, 0x1F5B}, {0x1F5D, 0x1F5D}, {0x1F5F, 0x1F7D}, {0x1F80, 0x1FB4},
            {0x1FB6, 0x1FBC}, {0x1FBE, 0x1FBE}, {0x1FC2, 0x1FC4}, {0x1FC6, 0x1FCC},
            {0x1FD0, 0x1FD3}, {0x1FD6, 0x1FDB}, {0x1FE0, 0x1FEC}, {0x1FF2, 0x1FF4},
            {0x1FF6, 0x1FFC}, {0x2070, 0x2071}, {0x2074, 0x2079}, {0x207F, 0x2089},
            {0x2090, 0x209C}, {0x2102, 0x2102}, {0x2107, 0x2107}, {0x210A, 0x2113},
            {0x2115, 0x2115}, {0x2119, 0x211D}, {0x2124, 0x2124}, {0x2126, 0x2126},
            {0x2128, 0x2128}, {0x212A, 0x212D}, {0x212F, 0x2139}, {0x213C, 0x213F},
            {0x2145, 0x2149}, {0x214E, 0x214E}, {0x2150, 0x2189}, {0x2460, 0x249B},
            {0x24EA, 0x24FF}, {0x2776, 0x2793}, {0x2C00, 0x2CE4}, {0x2CEB, 0x2CEE},
            {0x2CF2, 0x2CF3}, {0x2CFD, 0x2CFD}, {0x2D00, 0x2D25}, {0x2D27, 0x2D27},
            {0x2D2D, 0x2D2D}, {0x2D30, 0x2D67}, {0x2D6F, 0x2D6F}, {0x2D80, 0x2D96},
            {0x2DA0, 0x2DA6}, {0x2DA8, 0x2DAE}, {0x2DB0, 0x2DB6}, {0x2DB8, 0x2DBE},
            {0x2DC0, 0x2DC6}, {0x2DC8, 0x2DCE}, {0x2DD0, 0x2DD6}, {0x2DD8, 0x2DDE},
            {0x2E2F, 0x2E2F}, {0x3005, 0x3007}, {0x3021, 0x3029}, {0x3031, 0x3035},
            {0x3038, 0x303C}, {0x3041, 0x3096}, {0x309D, 0x309F}, {0x30A1, 0x30FA},
            {0x30FC, 0x30FF}, {0x3105, 0x312F}, {0x3131, 0x318E}, {0x3192, 0x3195},
            {0x31A0, 0x31BF}, {0x31F0, 0x31FF}, {0x3220, 0x3229}, {0x3248, 0x324F},
            {0x3251, 0x325F}, {0x3280, 0x3289}, {0x32B1, 0x32BF}, {0x3400, 0x4DBF},
            {0x4E00, 0xA48C}, {0xA4D0, 0xA4FD}, {0xA500, 0xA60C}, {0xA610, 0xA62B},
            {0xA640, 0xA66E}, {0xA67F, 0xA69D}, {0xA6A0, 0xA6EF}, {0xA717, 0xA71F},
            {0xA722, 0xA788}, {0xA78B, 0xA7CA}, {0xA7D0, 0xA7D1}, {0xA7D3, 0xA7D3},
            {0xA7D5, 0xA7D9}, {0xA7F2, 0xA801}, {0xA803, 0xA805}, {0xA807, 0xA80A},
            {0xA80C, 0xA822}, {0xA830, 0xA835}, {0xA840, 0xA873}, {0xA882, 0xA8B3},
            {0xA8D0, 0xA8D9}, {0xA8F2, 0xA8F7}, {0xA8FB, 0xA8FB}, {0xA8FD, 0xA8FE},
            {0xA900, 0xA925}, {0xA930, 0xA946}, {0xA960, 0xA97C}, {0xA984, 0xA9B2},
            {0xA9CF, 0xA9D9}, {0xA9E0, 0xA9E4}, {0xA9E6, 0xA9FE}, {0xAA00, 0xAA28},
            {0xAA40, 0xAA42}, {0xAA44, 0xAA4B}, {0xAA50, 0xAA59}, {0xAA60, 0xAA76},
            {0xAA7A, 0xAA7A}, {0xAA7E, 0xAAAF}, {0xAAB1, 0xAAB1}, {0xAAB5, 0xAAB6},
            {0xAAB9, 0xAABD}, {0xAAC0, 0xAAC0}, {0xAAC2, 0xAAC2}, {0xAADB, 0xAADD},
            {0xAAE0, 0xAAEA}, {0xAAF2, 0xAAF4}, {0xAB01, 0xAB06}, {0xAB09, 0xAB0E},
            {0xAB11, 0xAB16}, {0xAB20, 0xAB26}, {0xAB28, 0xAB2E}, {0xAB30, 0xAB5A},
            {0xAB5C, 0xAB69}, {0xAB70, 0xABE2}, {0xABF0, 0xABF9}, {0xAC00, 0xD7A3},
            {0xD7B0, 0xD7C6}, {0xD7CB, 0xD7FB}, {0xF900, 0xFA6D}, {0xFA70, 0xFAD9},
            {0xFB00, 0xFB06}, {0xFB13, 0xFB17}, {0xFB1D, 0xFB1D}, {0xFB1F, 0xFB28},
            {0xFB2A, 0xFB36}, {0xFB38, 0xFB3C}, {0xFB3E, 0xFB3E}, {0xFB40, 0xFB41},
            {0xFB43, 0xFB44}, {0xFB46, 0xFBB1}, {0xFBD3, 0xFD3D}, {0xFD50, 0xFD8F},
            {0xFD92, 0xFDC7}, {0xFDF0, 0xFDFB}, {0xFE70, 0xFE74}, {0xFE76, 0xFEFC},
            {0xFF10, 0xFF19}, {0xFF21, 0xFF3A}, {0xFF41, 0xFF5A}, {0xFF66, 0xFFBE},
            {0xFFC2, 0xFFC7}, {0xFFCA, 0xFFCF}, {0xFFD2, 0xFFD7}, {0xFFDA, 0xFFDC},
            {0x10000, 0x1000B}, {0x1000D, 0x10026}, {0x10028, 0x1003A}, {0x1003C, 0x1003D},
            {0x1003F, 0x1004D}, {0x10050, 0x1005D}, {0x10080, 0x100FA}, {0x10107, 0x10133},
            {0x10140, 0x10178}, {0x1018A, 0x1018B}, {0x10280, 0x1029C}, {0x102A0, 0x102D0},
            {0x102E1, 0x102FB}, {0x10300, 0x10323}, {0x1032D, 0x1034A}, {0x10350, 0x10375},
            {0x10380, 0x1039D}, {0x103A0, 0x103C3}, {0x103C8, 0x103CF}, {0x103D1, 0x103D5},
            {0x10400, 0x1049D}, {0x104A0, 0x104A9}, {0x104B0, 0x104D3}, {0x104D8, 0x104FB},
            {0x10500, 0x10527}, {0x10530, 0x10563}, {0x10570, 0x1057A}, {0x1057C, 0x1058A},
            {0x1058C, 0x10592}, {0x10594, 0x10595}, {0x10597, 0x105A1}, {0x105A3, 0x105B1},
            {0x105B3, 0x105B9}, {0x105BB, 0x105BC}, {0x10600, 0x10736}, {0x10740, 0x10755},
            {0x10760, 0x10767}, {0x10780, 0x10785}, {0x10787, 0x107B0}, {0x107B2, 0x107BA},
            {0x10800, 0x10805}, {0x10808, 0x10808}, {0x1080A, 0x10835}, {0x10837, 0x10838},
            {0x1083C, 0x1083C}, {0x1083F, 0x10855}, {0x10858, 0x10876}, {0x10879, 0x1089E},
            {0x108A7, 0x108AF}, {0x108E0, 0x108F2}, {0x108F4, 0x108F5}, {0x108FB, 0x1091B},
            {0x10920, 0x10939}, {0x10980, 0x109B7}, {0x109BC, 0x109CF}, {0x109D2, 0x10A00},
            {0x10A10, 0x10A13}, {0x10A15, 0x10A17}, {0x10A19, 0x10A35}, {0x10A40, 0x10A48},
            {0x10A60, 0x10A7E}, {0x10A80, 0x10A9F}, {0x10AC0, 0x10AC7}, {0x10AC9, 0x10AE4},
            {0x10AEB, 0x10AEF}, {0x10B00, 0x10B35}, {0x10B40, 0x10B55}, {0x10B58, 0x10B72},
            {0x10B78, 0x10B91}, {0x10BA9, 0x10BAF}, {0x10C00, 0x10C48}, {0x10C80, 0x10CB2},
            {0x10CC0, 0x10CF2}, {0x10CFA, 0x10D23}, {0x10D30, 0x10D39}, {0x10E60, 0x10E7E},
            {0x10E80, 0x10EA9}, {0x10EB0, 0x10EB1}, {0x10F00, 0x10F27}, {0x10F30, 0x10F45},
            {0x10F51, 0x10F54}, {0x10F70, 0x10F81}, {0x10FB0, 0x10FCB}, {0x10FE0, 0x10FF6},
            {0x11003, 0x11037}, {0x11052, 0x1106F}, {0x11071, 0x11072}, {0x11075, 0x11075},
            {0x11083, 0x110AF}, {0x110D0, 0x110E8}, {0x110F0, 0x110F9}, {0x11103, 0x11126},
            {0x11136, 0x1113F}, {0x11144, 0x11144}, {0x11147, 0x11147}, {0x11150, 0x11172},
            {0x11176, 0x11176}, {0x11183, 0x111B2}, {0x111C1, 0x111C4}, {0x111D0, 0x111DA},
            {0x111DC, 0x111DC}, {0x111E1, 0x111F4}, {0x11200, 0x11211}, {0x11213, 0x1122B},
            {0x11280, 0x11286}, {0x11288, 0x11288}, {0x1128A, 0x1128D}, {0x1128F, 0x1129D},
            {0x1129F, 0x112A8}, {0x112B0, 0x112DE}, {0x112F0, 0x112F9}, {0x11305, 0x1130C},
            {0x1130F, 0x11310}, {0x11313, 0x11328}, {0x1132A, 0x11330}, {0x11332, 0x11333},
            {0x11335, 0x11339}, {0x1133D, 0x1133D}, {0x11350, 0x11350}, {0x1135D, 0x11361},
            {0x11400, 0x11434}, {0x11447, 0x1144A}, {0x11450, 0x11459}, {0x1145F, 0x11461},
            {0x11480, 0x114AF}, {0x114C4, 0x114C5}, {0x114C7, 0x114C7}, {0x114D0, 0x114D9},
            {0x11580, 0x115AE}, {0x115D8, 0x115DB}, {0x11600, 0x1162F}, {0x11644, 0x11644},
            {0x11650, 0x11659}, {0x11680, 0x116AA}, {0x116B8, 0x116B8}, {0x116C0, 0x116C9},
            {0x11700, 0x1171A}, {0x11730, 0x1173B}, {0x11740, 0x11746}, {0x11800, 0x1182B},
            {0x118A0, 0x118F2}, {0x118FF, 0x11906}, {0x11909, 0x11909}, {0x1190C, 0x11913},
            {0x11915, 0x11916}, {0x11918, 0x1192F}, {0x1193F, 0x1193F}, {0x11941, 0x11941},
            {0x11950, 0x11959}, {0x119A0, 0x119A7}, {0x119AA, 0x119D0}, {0x119E1, 0x119E1},
            {0x119E3, 0x119E3}, {0x11A00, 0x11A00}, {0x11A0B, 0x11A32}, {0x11A3A, 0x11A3A},
            {0x11A50, 0x11A50}, {0x11A5C, 0x11A89}, {0x11A9D, 0x11A9D}, {0x11AB0, 0x11AF8},
            {0x11C00, 0x11C08}, {0x11C0A, 0x11C2E}, {0x11C40, 0x11C40}, {0x11C50, 0x11C6C},
            {0x11C72, 0x11C8F}, {0x11D00, 0x11D06}, {0x11D08, 0x11D09}, {0x11D0B, 0x11D30},
            {0x11D46, 0x11D46}, {0x11D50, 0x11D59}, {0x11D60, 0x11D65}, {0x11D67, 0x11D68},
            {0x11D6A, 0x11D89}, {0x11D98, 0x11D98}, {0x11DA0, 0x11DA9}, {0x11EE0, 0x11EF2},
            {0x11FB0, 0x11FB0}, {0x11FC0, 0x11FD4}, {0x12000, 0x12399}, {0x12400, 0x1246E},
            {0x12480, 0x12543}, {0x12F90, 0x12FF0}, {0x13000, 0x1342E}, {0x14400, 0x14646},
            {0x16800, 0x16A38}, {0x16A40, 0x16A5E}, {0x16A60, 0x16A69}, {0x16A70, 0x16ABE},
            {0x16AC0, 0x16AC9}, {0x16AD0, 0x16AED}, {0x16B00, 0x16B2F}, {0x16B40, 0x16B43},
            {0x16B50, 0x16B59}, {0x16B5B, 0x16B61}, {0x16B63, 0x16B77}, {0x16B7D, 0x16B8F},
            {0x16E40, 0x16E96}, {0x16F00, 0x16F4A}, {0x16F50, 0x16F50}, {0x16F93, 0x16F9F},
            {0x16FE0, 0x16FE1}, {0x16FE3, 0x16FE3}, {0x17000, 0x187F7}, {0x18800, 0x18CD5},
            {0x18D00, 0x18D08}, {0x1AFF0, 0x1AFF3}, {0x1AFF5, 0x1AFFB}, {0x1AFFD, 0x1AFFE},
            {0x1B000, 0x1B122}, {0x1B150, 0x1B152}, {0x1B164, 0x1B167}, {0x1B170, 0x1B2FB},
            {0x1BC00, 0x1BC6A}, {0x1BC70, 0x1BC7C}, {0x1BC80, 0x1BC88}, {0x1BC90, 0x1BC99},
            {0x1D2E0, 0x1D2F3}, {0x1D360, 0x1D378}, {0x1D400, 0x1D454}, {0x1D456, 0x1D49C},
            {0x1D49E, 0x1D49F}, {0x1D4A2, 0x1D4A2}, {0x1D4A5, 0x1D4A6}, {0x1D4A9, 0x1D4AC},
            {0x1D4AE, 0x1D4B9}, {0x1D4BB, 0x1D4BB}, {0x1D4BD, 0x1D4C3}, {0x1D4C5, 0x1D505},
            {0x1D507, 0x1D50A}, {0x1D50D, 0x1D514}, {0x1D516, 0x1D51C}, {0x1D51E, 0x1D539},
            {0x1D53B, 0x1D53E}, {0x1D540, 0x1D544}, {0x1D546, 0x1D546}, {0x1D54A, 0x1D550},
            {0x1D552, 0x1D6A5}, {0x1D6A8, 0x1D6C0}, {0x1D6C2, 0x1D6DA}, {0x1D6DC, 0x1D6FA},
            {0x1D6FC, 0x1D714}, {0x1D716, 0x1D734}, {0x1D736, 0x1D74E}, {0x1D750, 0x1D76E},
            {0x1D770, 0x1D788}, {0x1D78A, 0x1D7A8}, {0x1D7AA, 0x1D7C2}, {0x1D7C4, 0x1D7CB},
            {0x1D7CE, 0x1D7FF}, {0x1DF00, 0x1DF1E}, {0x1E100, 0x1E12C}, {0x1E137, 0x1E13D},
            {0x1E140, 0x1E149}, {0x1E14E, 0x1E14E}, {0x1E290, 0x1E2AD}, {0x1E2C0, 0x1E2EB},
            {0x1E2F0, 0x1E2F9}, {0x1E7E0, 0x1E7E6}, {0x1E7E8, 0x1E7EB}, {0x1E7ED, 0x1E7EE},
            {0x1E7F0, 0x1E7FE}, {0x1E800, 0x1E8C4}, {0x1E8C7, 0x1E8CF}, {0x1E900, 0x1E943},
            {0x1E94B, 0x1E94B}, {0x1E950, 0x1E959}, {0x1EC71, 0x1ECAB}, {0x1ECAD, 0x1ECAF},
            {0x1ECB1, 0x1ECB4}, {0x1ED01, 0x1ED2D}, {0x1ED2F, 0x1ED3D}, {0x1EE00, 0x1EE03},
            {0x1EE05, 0x1EE1F}, {0x1EE21, 0x1EE22}, {0x1EE24, 0x1EE24}, {0x1EE27, 0x1EE27},
            {0x1EE29, 0x1EE32}, {0x1EE34, 0x1EE37}, {0x1EE39, 0x1EE39}, {0x1EE3B, 0x1EE3B},
            {0x1EE42, 0x1EE42}, {0x1EE47, 0x1EE47}, {0x1EE49, 0x1EE49}, {0x1EE4B, 0x1EE4B},
            {0x1EE4D, 0x1EE4F}, {0x1EE51, 0x1EE52}, {0x1EE54, 0x1EE54}, {0x1EE57, 0x1EE57},
            {0x1EE59, 0x1EE59}, {0x1EE5B, 0x1EE5B}, {0x1EE5D, 0x1EE5D}, {0x1EE5F, 0x1EE5F},
            {0x1EE61, 0x1EE62}, {0x1EE64, 0x1EE64}, {0x1EE67, 0x1EE6A}, {0x1EE6C, 0x1EE72},
            {0x1EE74, 0x1EE77}, {0x1EE79, 0x1EE7C}, {0x1EE7E, 0x1EE7E}, {0x1EE80, 0x1EE89},
            {0x1EE8B, 0x1EE9B}, {0x1EEA1, 0x1EEA3}, {0x1EEA5, 0x1EEA9}, {0x1EEAB, 0x1EEBB},
            {0x1F100, 0x1F10C}, {0x1FBF0, 0x1FBF9}, {0x20000, 0x2A6DF}, {0x2A700, 0x2B738},
            {0x2B740, 0x2B81D}, {0x2B820, 0x2CEA1}, {0x2CEB0, 0x2EBE0}, {0x2F800, 0x2FA1D},
            {0x30000, 0x3134A},
        };

        // First code point (digit value 0) of every run of ten Unicode decimal digits (Nd).
        static const char32_t kDecimalZeros[] = {
            0x30, 0x660, 0x6F0, 0x7C0, 0x966, 0x9E6, 0xA66, 0xAE6,
            0xB66, 0xBE6, 0xC66, 0xCE6, 0xD66, 0xDE6, 0xE50, 0xED0,
            0xF20, 0x1040, 0x1090, 0x17E0, 0x1810, 0x1946, 0x19D0, 0x1A80,
            0x1A90, 0x1B50, 0x1BB0, 0x1C40, 0x1C50, 0xA620, 0xA8D0, 0xA900,
            0xA9D0, 0xA9F0, 0xAA50, 0xABF0, 0xFF10, 0x104A0, 0x10D30, 0x11066,
            0x110F0, 0x11136, 0x111D0, 0x112F0, 0x11450, 0x114D0, 0x11650, 0x116C0,
            0x11730, 0x118E0, 0x11950, 0x11C50, 0x11D50, 0x11DA0, 0x16A60, 0x16AC0,
            0x16B50, 0x1D7CE, 0x1D7D8, 0x1D7E2, 0x1D7EC, 0x1D7F6, 0x1E140, 0x1E2F0,
            0x1E950, 0x1FBF0,
        };

        // The regex \d (== str.isdecimal()): value 0..9, or -1.
        int decimal_value(char32_t c)
        {
            if (c < 0x80)
                return (c >= U'0' && c <= U'9') ? static_cast<int>(c - U'0') : -1;
            const size_t count = sizeof(kDecimalZeros) / sizeof(kDecimalZeros[0]);
            const char32_t *end = kDecimalZeros + count;
            const char32_t *it = std::upper_bound(kDecimalZeros, end, c);
            if (it == kDecimalZeros)
                return -1;
            const char32_t zero = *(it - 1);
            return (c - zero <= 9) ? static_cast<int>(c - zero) : -1;
        }

        // The regex \w (== str.isalnum() or '_').
        bool is_word(char32_t c)
        {
            if (c < 0x80)
                return (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || c == U'_';
            const size_t count = sizeof(kWordRanges) / sizeof(kWordRanges[0]);
            size_t lo = 0, hi = count;
            while (lo < hi)
            {
                const size_t mid = (lo + hi) / 2;
                if (c < kWordRanges[mid][0])
                    hi = mid;
                else if (c > kWordRanges[mid][1])
                    lo = mid + 1;
                else
                    return true;
            }
            return false;
        }

        // str.strip()
        u32 strip(const u32 &s)
        {
            size_t b = 0, e = s.size();
            while (b < e && is_space(s[b]))
                ++b;
            while (e > b && is_space(s[e - 1]))
                --e;
            return s.substr(b, e - b);
        }

        // "".join(text.split())
        u32 without_whitespace(const u32 &s)
        {
            u32 out;
            for (char32_t c : s)
                if (!is_space(c))
                    out.push_back(c);
            return out;
        }

        // " ".join(text.split())
        u32 collapse_whitespace(const u32 &s)
        {
            u32 out;
            bool pending = false;
            for (char32_t c : s)
            {
                if (is_space(c))
                {
                    pending = !out.empty();
                    continue;
                }
                if (pending)
                    out.push_back(U' ');
                pending = false;
                out.push_back(c);
            }
            return out;
        }

        // str.lower() as far as it is observable by the elevation patterns: ASCII letters, U+0130 (which
        // lower-cases to "i" + U+0307) and U+212A KELVIN SIGN (-> "k"). Every other cased character keeps
        // its \w / \s / \d class under lower(), and the patterns only name ASCII letters.
        u32 py_lower(const u32 &s)
        {
            u32 out;
            out.reserve(s.size());
            for (char32_t c : s)
            {
                if (c >= U'A' && c <= U'Z')
                    out.push_back(c + 32);
                else if (c == 0x130)
                {
                    out.push_back(U'i');
                    out.push_back(0x307);
                }
                else if (c == 0x212A)
                    out.push_back(U'k');
                else
                    out.push_back(c);
            }
            return out;
        }

        bool contains(const u32 &s, char32_t c) { return s.find(c) != u32::npos; }

        // -----------------------------------------------------------------------------------------------------
        // A small backtracking regex engine with the semantics of Python's re for the elevation patterns:
        // leftmost match, greedy quantifiers, ordered alternation, Unicode \s \d \b, optional IGNORECASE.
        // Supported syntax: literals, \s \d \b ^, [..] sets (no ranges), (..) (?:..) (?P<name>..), |, ? * +,
        // and \<punctuation> escapes. The pattern strings are copied verbatim from Windows (UTF-8 source; the
        // prime U+2032 appears as the literal character, exactly as in elevation_text.py).
        // -----------------------------------------------------------------------------------------------------

        struct Span
        {
            size_t begin = npos;
            size_t end = npos;
        };

        struct Node
        {
            enum Kind { Lit, Set, Space, Digit, WordBoundary, StartOfText, Seq, Alt, Repeat, Group };
            Kind kind = Seq;
            char32_t ch = 0;
            std::vector<char32_t> chars;
            std::vector<Node> kids;
            int min = 0;
            int max = -1; // -1 = unbounded
            int group = -1;
        };

        // re.IGNORECASE folding for the letters the patterns contain (a c d e i m n r s t; checked against Python
        // 3.11.4 for every code point): the ASCII capitals, U+017F LONG S (matches 's'), U+0130 and U+0131 (dotted
        // capital I / dotless small i, both match 'i') and U+212A KELVIN SIGN (matches 'k').
        char32_t fold_case(char32_t c)
        {
            if (c >= U'A' && c <= U'Z')
                return c + 32;
            switch (c)
            {
            case 0x17F:
                return U's';
            case 0x130:
            case 0x131:
                return U'i';
            case 0x212A:
                return U'k';
            default:
                return c;
            }
        }

        class Regex
        {
        public:
            // pattern: UTF-8 (the Windows patterns contain the literal prime character U+2032)
            Regex(const char *pattern, bool ignore_case) : _ignore_case(ignore_case)
            {
                const u32 decoded = decode_utf8(pattern);
                const char32_t *p = decoded.c_str();
                _root = parse_alt(p);
                _groups = _group_count + 1;
            }

            // re.match: anchored at position 0 (the patterns also start with ^).
            bool match(const u32 &text, std::vector<Span> &caps) const { return run(text, 0, caps); }

            // re.search: first start position (left to right) at which the pattern matches.
            bool search(const u32 &text, std::vector<Span> &caps) const
            {
                for (size_t start = 0; start <= text.size(); ++start)
                    if (run(text, start, caps))
                        return true;
                return false;
            }

        private:
            Node _root;
            bool _ignore_case;
            int _group_count = 0;
            int _groups = 1;

            // ---- pattern parser ----
            Node parse_alt(const char32_t *&p)
            {
                std::vector<Node> alternatives;
                alternatives.push_back(parse_seq(p));
                while (*p == '|')
                {
                    ++p;
                    alternatives.push_back(parse_seq(p));
                }
                if (alternatives.size() == 1)
                    return std::move(alternatives[0]);
                Node n;
                n.kind = Node::Alt;
                n.kids = std::move(alternatives);
                return n;
            }

            Node parse_seq(const char32_t *&p)
            {
                Node seq;
                seq.kind = Node::Seq;
                while (*p && *p != '|' && *p != ')')
                {
                    Node atom = parse_atom(p);
                    atom = parse_quantifier(p, std::move(atom));
                    seq.kids.push_back(std::move(atom));
                }
                return seq;
            }

            Node parse_quantifier(const char32_t *&p, Node atom)
            {
                int min = 1, max = 1;
                if (*p == '?')
                {
                    min = 0;
                    max = 1;
                }
                else if (*p == '*')
                {
                    min = 0;
                    max = -1;
                }
                else if (*p == '+')
                {
                    min = 1;
                    max = -1;
                }
                else
                {
                    return atom;
                }
                ++p;
                Node rep;
                rep.kind = Node::Repeat;
                rep.min = min;
                rep.max = max;
                rep.kids.push_back(std::move(atom));
                return rep;
            }

            // *p is the character after the backslash: an escaped literal such as \' \" \. \+
            static char32_t parse_escape_char(const char32_t *&p) { return *p++; }

            Node parse_atom(const char32_t *&p)
            {
                Node n;
                if (*p == '(')
                {
                    ++p;
                    int group = -1;
                    if (*p == '?')
                    {
                        ++p;
                        if (*p == ':')
                        {
                            ++p;
                        }
                        else if (*p == 'P')
                        {
                            while (*p && *p != '>')
                                ++p;
                            ++p;
                            group = ++_group_count;
                        }
                    }
                    else
                    {
                        group = ++_group_count;
                    }
                    Node inner = parse_alt(p);
                    ++p; // ')'
                    if (group < 0)
                        return inner;
                    n.kind = Node::Group;
                    n.group = group;
                    n.kids.push_back(std::move(inner));
                    return n;
                }
                if (*p == '[')
                {
                    ++p;
                    n.kind = Node::Set;
                    while (*p && *p != ']')
                    {
                        if (*p == '\\')
                        {
                            ++p;
                            n.chars.push_back(parse_escape_char(p));
                        }
                        else
                        {
                            n.chars.push_back(*p++);
                        }
                    }
                    ++p; // ']'
                    return n;
                }
                if (*p == '\\')
                {
                    ++p;
                    if (*p == 's')
                    {
                        ++p;
                        n.kind = Node::Space;
                    }
                    else if (*p == 'd')
                    {
                        ++p;
                        n.kind = Node::Digit;
                    }
                    else if (*p == 'b')
                    {
                        ++p;
                        n.kind = Node::WordBoundary;
                    }
                    else
                    {
                        n.kind = Node::Lit;
                        n.ch = parse_escape_char(p);
                    }
                    return n;
                }
                if (*p == '^')
                {
                    ++p;
                    n.kind = Node::StartOfText;
                    return n;
                }
                n.kind = Node::Lit;
                n.ch = *p++;
                return n;
            }

            // ---- matcher ----
            using Cont = std::function<bool(size_t)>;

            struct State
            {
                const u32 &text;
                std::vector<Span> caps;
            };

            bool run(const u32 &text, size_t start, std::vector<Span> &caps) const
            {
                State st{text, std::vector<Span>(static_cast<size_t>(_groups))};
                size_t end = 0;
                const bool ok = match_node(st, _root, start, [&end](size_t p) {
                    end = p;
                    return true;
                });
                if (ok)
                {
                    st.caps[0] = Span{start, end};
                    caps = std::move(st.caps);
                }
                return ok;
            }

            static bool is_single(const Node &n)
            {
                return n.kind == Node::Lit || n.kind == Node::Set || n.kind == Node::Space || n.kind == Node::Digit;
            }

            bool single_matches(const Node &n, char32_t c) const
            {
                switch (n.kind)
                {
                case Node::Lit:
                    return _ignore_case ? fold_case(c) == fold_case(n.ch) : c == n.ch;
                case Node::Set:
                    for (char32_t m : n.chars)
                        if (_ignore_case ? fold_case(c) == fold_case(m) : c == m)
                            return true;
                    return false;
                case Node::Space:
                    return is_space(c);
                case Node::Digit:
                    return decimal_value(c) >= 0;
                default:
                    return false;
                }
            }

            bool match_node(State &st, const Node &n, size_t pos, const Cont &k) const
            {
                const u32 &text = st.text;
                switch (n.kind)
                {
                case Node::Lit:
                case Node::Set:
                case Node::Space:
                case Node::Digit:
                    return pos < text.size() && single_matches(n, text[pos]) && k(pos + 1);
                case Node::WordBoundary:
                {
                    const bool before = pos > 0 && is_word(text[pos - 1]);
                    const bool after = pos < text.size() && is_word(text[pos]);
                    return before != after && k(pos);
                }
                case Node::StartOfText:
                    return pos == 0 && k(pos);
                case Node::Seq:
                    return match_seq(st, n, 0, pos, k);
                case Node::Alt:
                    for (const Node &alternative : n.kids)
                        if (match_node(st, alternative, pos, k))
                            return true;
                    return false;
                case Node::Group:
                {
                    const Span saved = st.caps[static_cast<size_t>(n.group)];
                    const bool ok = match_node(st, n.kids[0], pos, [&st, &n, &k, pos](size_t p) {
                        const Span previous = st.caps[static_cast<size_t>(n.group)];
                        st.caps[static_cast<size_t>(n.group)] = Span{pos, p};
                        if (k(p))
                            return true;
                        st.caps[static_cast<size_t>(n.group)] = previous;
                        return false;
                    });
                    if (!ok)
                        st.caps[static_cast<size_t>(n.group)] = saved;
                    return ok;
                }
                case Node::Repeat:
                    return match_repeat(st, n, pos, k);
                }
                return false;
            }

            bool match_seq(State &st, const Node &n, size_t index, size_t pos, const Cont &k) const
            {
                if (index == n.kids.size())
                    return k(pos);
                return match_node(st, n.kids[index], pos, [this, &st, &n, index, &k](size_t p) {
                    return match_seq(st, n, index + 1, p, k);
                });
            }

            bool match_repeat(State &st, const Node &n, size_t pos, const Cont &k) const
            {
                const Node &child = n.kids[0];
                if (is_single(child))
                {
                    // greedy run of a one-character item: take the longest run, then give characters back
                    size_t count = 0;
                    while ((n.max < 0 || count < static_cast<size_t>(n.max)) && pos + count < st.text.size() &&
                           single_matches(child, st.text[pos + count]))
                        ++count;
                    const size_t min = static_cast<size_t>(n.min);
                    if (count < min)
                        return false;
                    for (;;)
                    {
                        if (k(pos + count))
                            return true;
                        if (count == min)
                            return false;
                        --count;
                    }
                }
                return match_repeat_general(st, n, 0, pos, k);
            }

            // Only used for (?:...)? groups here: an iteration that consumes nothing is never repeated.
            bool match_repeat_general(State &st, const Node &n, int count, size_t pos, const Cont &k) const
            {
                if (n.max < 0 || count < n.max)
                {
                    const bool took = match_node(st, n.kids[0], pos, [this, &st, &n, count, pos, &k](size_t p) {
                        if (p == pos)
                            return false;
                        return match_repeat_general(st, n, count + 1, p, k);
                    });
                    if (took)
                        return true;
                }
                return count >= n.min && k(pos);
            }
        };

        // -----------------------------------------------------------------------------------------------------
        // The Windows patterns (elevation_text.py; position_parser.py only imports them since b9902e6), copied verbatim
        // -----------------------------------------------------------------------------------------------------

        // elevation_text.py (re.IGNORECASE, used with .match: anchored prefix)
        const Regex &gate_feet_inches()
        {
            static const Regex re(R"re(^-?\s*\d+\s*[\'′]\s*(?:-?\s*)?\s*\d+(?:\s+\d+/\d+)?\s*\")re", true);
            return re;
        }
        const Regex &gate_feet_only()
        {
            static const Regex re(R"re(^-?\s*\d+\s*[\'′]\b)re", true);
            return re;
        }
        const Regex &gate_inches_only()
        {
            static const Regex re(R"re(^-?\s*(?:\d+(?:\s+\d+/\d+)?|\d+/\d+)\s*\")re", true);
            return re;
        }
        const Regex &gate_m_plus_cm()
        {
            static const Regex re(R"re(^-?\s*\d+(?:[\.,]\d+)?\s*m\s*(?:and|\+)?\s*-?\s*\d+(?:[\.,]\d+)?\s*cm)re", true);
            return re;
        }
        const Regex &gate_meters_only()
        {
            static const Regex re(R"re(^-?\s*\d+(?:[\.,]\d+)?\s*(?:m|meters?)\b)re", true);
            return re;
        }
        const Regex &gate_cm_only()
        {
            static const Regex re(R"re(^-?\s*\d+(?:[\.,]\d+)?\s*(?:cm|centimeters?)\b)re", true);
            return re;
        }

        // elevation_text.py::_parse_imperial_elevation (:83-106; re.search, case-sensitive). Groups: 1 feet, 2 inches.
        const Regex &imperial_feet_inches()
        {
            static const Regex re(
                R"re((?P<feet>-?\s*\d+)\s*[\'′]\s*(?:-?\s*)?(?P<inches>\d+(?:\s+\d+/\d+)?|\d+/\d+)\s*\")re", false);
            return re;
        }
        const Regex &imperial_feet_only()
        {
            static const Regex re(R"re((?P<feet>-?\s*\d+)\s*[\'′]\b)re", false);
            return re;
        }
        const Regex &imperial_inches_only()
        {
            static const Regex re(R"re((?P<inches>-?\s*(?:\d+(?:\s+\d+/\d+)?|\d+/\d+))\s*\")re", false);
            return re;
        }

        // elevation_text.py::_parse_metric_elevation (:109-128; re.search on the lower-cased text, case-sensitive)
        const Regex &metric_m_plus_cm()
        {
            static const Regex re(R"re((-?\s*\d+(?:\.\d+)?)\s*m\s*(?:and|\+)?\s*(-?\s*\d+(?:\.\d+)?)\s*cm)re", false);
            return re;
        }
        const Regex &metric_meters()
        {
            static const Regex re(R"re((-?\s*\d+(?:\.\d+)?)\s*(?:m|meters?)\b)re", false);
            return re;
        }
        const Regex &metric_cm()
        {
            static const Regex re(R"re((-?\s*\d+(?:\.\d+)?)\s*(?:cm|centimeters?)\b)re", false);
            return re;
        }

        u32 group_text(const u32 &text, const Span &s) { return text.substr(s.begin, s.end - s.begin); }

        // -----------------------------------------------------------------------------------------------------
        // Python float()
        // -----------------------------------------------------------------------------------------------------

        bool is_ascii_digit(char c) { return c >= '0' && c <= '9'; }
        bool is_ascii_space(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

        // float(text) for already decoded text (CPython PyFloat_FromString for str):
        // 1. _PyUnicode_TransformDecimalAndSpaceToASCII: code points below 127 are kept, other whitespace
        //    becomes ' ', Unicode decimal digits become ASCII digits, anything else becomes '?'
        // 2. underscores are only valid between two ASCII digits and are then removed
        // 3. ASCII whitespace is stripped, then "inf"/"infinity"/"nan" or the decimal grammar must consume
        //    the whole text
        bool py_float(const u32 &text, double &out)
        {
            std::string s;
            s.reserve(text.size());
            for (char32_t c : text)
            {
                if (c < 127)
                    s.push_back(static_cast<char>(c));
                else if (is_space(c))
                    s.push_back(' ');
                else
                {
                    const int d = decimal_value(c);
                    s.push_back(d < 0 ? '?' : static_cast<char>('0' + d));
                }
            }
            if (s.find('_') != std::string::npos)
            {
                std::string cleaned;
                cleaned.reserve(s.size());
                char prev = '\0';
                for (char ch : s)
                {
                    if (ch == '_')
                    {
                        if (!is_ascii_digit(prev))
                            return false;
                    }
                    else
                    {
                        cleaned.push_back(ch);
                        if (prev == '_' && !is_ascii_digit(ch))
                            return false;
                    }
                    prev = ch;
                }
                if (prev == '_')
                    return false;
                s = cleaned;
            }
            size_t b = 0, e = s.size();
            while (b < e && is_ascii_space(s[b]))
                ++b;
            while (e > b && is_ascii_space(s[e - 1]))
                --e;
            s = s.substr(b, e - b);
            if (s.empty())
                return false;

            size_t i = 0;
            bool negative = false;
            if (s[i] == '+' || s[i] == '-')
            {
                negative = s[i] == '-';
                ++i;
            }
            std::string word = s.substr(i);
            for (char &ch : word)
                if (ch >= 'A' && ch <= 'Z')
                    ch = static_cast<char>(ch + 32);
            if (word == "inf" || word == "infinity")
            {
                out = negative ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
                return true;
            }
            if (word == "nan")
            {
                out = std::numeric_limits<double>::quiet_NaN();
                return true;
            }

            size_t j = i;
            size_t int_digits = 0, frac_digits = 0;
            while (j < s.size() && is_ascii_digit(s[j]))
            {
                ++j;
                ++int_digits;
            }
            if (j < s.size() && s[j] == '.')
            {
                ++j;
                while (j < s.size() && is_ascii_digit(s[j]))
                {
                    ++j;
                    ++frac_digits;
                }
            }
            if (int_digits + frac_digits == 0)
                return false;
            if (j < s.size() && (s[j] == 'e' || s[j] == 'E'))
            {
                ++j;
                if (j < s.size() && (s[j] == '+' || s[j] == '-'))
                    ++j;
                size_t exp_digits = 0;
                while (j < s.size() && is_ascii_digit(s[j]))
                {
                    ++j;
                    ++exp_digits;
                }
                if (exp_digits == 0)
                    return false;
            }
            if (j != s.size())
                return false;
            // s is now [+-]digits[.digits][e[+-]digits]: strtod reads exactly this (correctly rounded; overflow
            // gives +-HUGE_VAL = inf and underflow 0.0 like Python). Assumes the C numeric locale.
            out = std::strtod(s.c_str(), nullptr);
            return true;
        }

        void set_error(std::string *error, const std::string &message)
        {
            if (error)
                *error = message;
        }

        // Python's float() ValueError text
        void set_float_error(std::string *error, const u32 &text)
        {
            set_error(error, "could not convert string to float: '" + encode_utf8(text) + "'");
        }

        // elevation_text.py::_fraction_value (:44-51)
        bool fraction_value(const u32 &numerator_text, const u32 &denominator_text, double &out, std::string *error)
        {
            double numerator = 0.0, denominator = 0.0;
            if (!py_float(numerator_text, numerator))
            {
                set_float_error(error, numerator_text);
                return false;
            }
            if (!py_float(denominator_text, denominator))
            {
                set_float_error(error, denominator_text);
                return false;
            }
            if (denominator == 0.0)
            {
                set_error(error, "zero denominator in fraction " + encode_utf8(numerator_text) + "/" +
                                     encode_utf8(denominator_text));
                return false;
            }
            out = numerator / denominator;
            return true;
        }

        bool parse_fractional_inches_u32(const u32 &raw_text, double &out, std::string *error)
        {
            u32 inch_text = collapse_whitespace(raw_text);
            const bool negative = !inch_text.empty() && inch_text[0] == U'-';
            if (negative)
                inch_text = strip(inch_text.substr(1));
            double value = 0.0;
            const size_t space = inch_text.find(U' ');
            if (space != u32::npos)
            {
                const u32 whole_part = inch_text.substr(0, space);
                const u32 frac_part = inch_text.substr(space + 1);
                double whole = 0.0;
                if (!py_float(whole_part, whole))
                    whole = 0.0; // an unreadable whole part counts as zero
                double frac = 0.0;
                const size_t slash = frac_part.find(U'/');
                if (slash != u32::npos)
                {
                    if (!fraction_value(frac_part.substr(0, slash), frac_part.substr(slash + 1), frac, error))
                        return false;
                }
                else if (!py_float(frac_part, frac))
                {
                    set_float_error(error, frac_part);
                    return false;
                }
                value = whole + frac;
            }
            else
            {
                const size_t slash = inch_text.find(U'/');
                if (slash != u32::npos)
                {
                    if (!fraction_value(inch_text.substr(0, slash), inch_text.substr(slash + 1), value, error))
                        return false;
                }
                else if (!py_float(inch_text, value))
                {
                    set_float_error(error, inch_text);
                    return false;
                }
            }
            out = negative ? -value : value;
            return true;
        }

        // ASCII digits of a run of Unicode decimal digits (the captured feet), without sign
        std::string ascii_digits(const u32 &s)
        {
            std::string out;
            for (char32_t c : s)
            {
                const int d = decimal_value(c);
                if (d >= 0)
                    out.push_back(static_cast<char>('0' + d));
            }
            return out;
        }

        // float(int(digits) * 12): exact decimal multiplication, then one correctly rounded conversion (Python
        // converts the exact int). False where Python raises OverflowError (value beyond the float range).
        bool feet_digits_to_inches(const std::string &digits, double &out)
        {
            // Python 3.11 int(str) refuses more than 4300 decimal digits (sys.int_info.default_max_str_digits, leading
            // zeros included) with a ValueError, which the Windows MDB reader catches (-> (0.0, False)).
            if (digits.size() > 4300)
                return false;
            std::string product;
            int carry = 0;
            for (size_t i = digits.size(); i-- > 0;)
            {
                const int v = (digits[i] - '0') * 12 + carry;
                product.push_back(static_cast<char>('0' + v % 10));
                carry = v / 10;
            }
            while (carry > 0)
            {
                product.push_back(static_cast<char>('0' + carry % 10));
                carry /= 10;
            }
            std::reverse(product.begin(), product.end());
            if (product.empty())
                product = "0";
            const double v = std::strtod(product.c_str(), nullptr);
            if (std::isinf(v))
                return false;
            out = v;
            return true;
        }

        bool digits_are_zero(const std::string &digits)
        {
            return digits.find_first_not_of('0') == std::string::npos;
        }

        bool parse_imperial_u32(const u32 &raw_text, double &out, std::string *error)
        {
            const u32 text = strip(raw_text);
            std::vector<Span> caps;
            if (imperial_feet_inches().search(text, caps))
            {
                const u32 feet_str = without_whitespace(group_text(text, caps[1]));
                const u32 inches_str = strip(group_text(text, caps[2]));
                const bool negative = !feet_str.empty() && feet_str[0] == U'-';
                double inches_val = 0.0;
                if (!parse_fractional_inches_u32(inches_str, inches_val, error))
                    return false;
                double feet_inches = 0.0;
                if (!feet_digits_to_inches(ascii_digits(feet_str), feet_inches))
                {
                    set_error(error, "int too large to convert to float");
                    return false;
                }
                const double total = feet_inches + inches_val;
                out = negative ? -total : total;
                return true;
            }
            if (imperial_feet_only().search(text, caps))
            {
                const u32 feet_str = without_whitespace(group_text(text, caps[1]));
                const bool negative = !feet_str.empty() && feet_str[0] == U'-';
                const std::string digits = ascii_digits(feet_str);
                double feet_inches = 0.0;
                if (!feet_digits_to_inches(digits, feet_inches))
                {
                    set_error(error, "int too large to convert to float");
                    return false;
                }
                // float(feet_val * 12) of the signed int: -0 is the int 0, so the result is +0.0
                out = (negative && !digits_are_zero(digits)) ? -feet_inches : feet_inches;
                return true;
            }
            if (imperial_inches_only().search(text, caps))
            {
                const u32 inches_str = strip(group_text(text, caps[1]));
                return parse_fractional_inches_u32(inches_str, out, error);
            }
            out = 0.0;
            return true;
        }

        double parse_metric_u32(const u32 &raw_text)
        {
            u32 text = py_lower(strip(raw_text));
            std::replace(text.begin(), text.end(), U',', U'.');
            std::vector<Span> caps;
            if (metric_m_plus_cm().search(text, caps))
            {
                const u32 meter_text = without_whitespace(group_text(text, caps[1]));
                double meter_part = 0.0, cm_part = 0.0;
                py_float(meter_text, meter_part);
                py_float(without_whitespace(group_text(text, caps[2])), cm_part);
                if (!meter_text.empty() && meter_text[0] == U'-')
                {
                    // two statements, so a compiler that contracts a*b+c into an FMA cannot change the rounding
                    const double meters_in_cm = std::fabs(meter_part) * 100.0;
                    return -(meters_in_cm + std::fabs(cm_part));
                }
                const double meters_in_cm = meter_part * 100.0;
                return meters_in_cm + cm_part;
            }
            if (metric_meters().search(text, caps))
            {
                double meter_part = 0.0;
                py_float(without_whitespace(group_text(text, caps[1])), meter_part);
                return meter_part * 100.0;
            }
            if (metric_cm().search(text, caps))
            {
                double cm = 0.0;
                py_float(without_whitespace(group_text(text, caps[1])), cm);
                return cm;
            }
            return 0.0;
        }

        bool is_elevation_u32(const u32 &text)
        {
            std::vector<Span> caps;
            return gate_feet_inches().match(text, caps) || gate_feet_only().match(text, caps) ||
                   gate_inches_only().match(text, caps) || gate_m_plus_cm().match(text, caps) ||
                   gate_meters_only().match(text, caps) || gate_cm_only().match(text, caps);
        }

        // -----------------------------------------------------------------------------------------------------
        // repr(float)
        // -----------------------------------------------------------------------------------------------------

        // "%.{p-1}e" output -> p digits and the decimal exponent of the first digit
        void split_scientific(const char *buf, std::string &digits, int &exp10)
        {
            digits.clear();
            const char *q = buf;
            while (*q && *q != 'e')
            {
                if (*q != '.')
                    digits.push_back(*q);
                ++q;
            }
            exp10 = (*q == 'e') ? std::atoi(q + 1) : 0;
        }

        bool round_trips(const std::string &digits, int exp10, double value)
        {
            std::string text = digits.substr(0, 1);
            if (digits.size() > 1)
            {
                text += '.';
                text += digits.substr(1);
            }
            text += 'e';
            text += std::to_string(exp10);
            return std::strtod(text.c_str(), nullptr) == value;
        }

        // Shortest decimal digits that read back as `value` (> 0, finite): the fewest digits for which the
        // correctly rounded decimal, or its neighbour in the last digit (an asymmetric rounding interval at a
        // power of two), reads back exactly; this is the digit string of Python's repr (David Gay's mode 0).
        void shortest_digits(double value, std::string &digits, int &exp10)
        {
            for (int precision = 1; precision <= 17; ++precision)
            {
                char buf[64];
                std::snprintf(buf, sizeof buf, "%.*e", precision - 1, value);
                std::string base;
                int base_exp = 0;
                split_scientific(buf, base, base_exp);
                std::string chosen;
                int chosen_exp = base_exp;
                if (round_trips(base, base_exp, value))
                {
                    chosen = base;
                }
                else
                {
                    std::string up = base;
                    int up_exp = base_exp;
                    size_t i = up.size();
                    while (i > 0 && up[i - 1] == '9')
                        up[--i] = '0';
                    if (i == 0)
                    {
                        up.insert(up.begin(), '1');
                        up.pop_back();
                        ++up_exp;
                    }
                    else
                    {
                        ++up[i - 1];
                    }
                    if (round_trips(up, up_exp, value))
                    {
                        chosen = up;
                        chosen_exp = up_exp;
                    }
                    else
                    {
                        std::string down = base;
                        size_t k = down.size();
                        while (k > 0 && down[k - 1] == '0')
                            down[--k] = '9';
                        if (k > 0)
                        {
                            --down[k - 1];
                            if (down[0] != '0' && round_trips(down, base_exp, value))
                                chosen = down;
                        }
                    }
                }
                if (!chosen.empty())
                {
                    while (chosen.size() > 1 && chosen.back() == '0')
                        chosen.pop_back();
                    digits = chosen;
                    exp10 = chosen_exp;
                    return;
                }
            }
            // unreachable: 17 significant digits always round-trip a double
            char buf[64];
            std::snprintf(buf, sizeof buf, "%.16e", value);
            split_scientific(buf, digits, exp10);
        }

        // domain/utils/text_cleanup.py::strip_xml_newline_entities: re.sub(r"&#xa;|&#x0A;|&#10;|&#xA;", "",
        // text, re.IGNORECASE)
        char ascii_lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

        size_t newline_entity_length(const std::string &text, size_t pos)
        {
            static const char *const kEntities[] = {"&#xa;", "&#x0a;", "&#10;"};
            for (const char *entity : kEntities)
            {
                size_t len = 0;
                while (entity[len])
                    ++len;
                if (pos + len > text.size())
                    continue;
                bool match = true;
                for (size_t i = 0; i < len && match; ++i)
                    match = ascii_lower(text[pos + i]) == entity[i];
                if (match)
                    return len;
            }
            return 0;
        }
    } // namespace

    // -------------------------------------------------------------------------------------------------------------
    // Position text
    // -------------------------------------------------------------------------------------------------------------

    std::vector<double> parse_position(const std::string &position_str)
    {
        if (position_str.empty())
            return {};

        std::string cleaned;
        cleaned.reserve(position_str.size());
        for (size_t i = 0; i < position_str.size();)
        {
            const size_t entity_len = newline_entity_length(position_str, i);
            if (entity_len > 0)
            {
                i += entity_len;
                continue;
            }
            cleaned.push_back(position_str[i]);
            ++i;
        }

        const u32 text = strip(decode_utf8(cleaned));
        if (text.empty())
            return {};

        std::vector<double> position;
        size_t start = 0;
        while (start <= text.size())
        {
            size_t semicolon = text.find(U';', start);
            if (semicolon == u32::npos)
                semicolon = text.size();
            const u32 token = strip(text.substr(start, semicolon - start));
            if (!token.empty())
            {
                double value = 0.0;
                if (!py_float(token, value))
                    return {};
                position.push_back(value);
            }
            start = semicolon + 1;
        }
        return position;
    }

    std::string python_float_repr(double value)
    {
        if (std::isnan(value))
            return "nan";
        if (std::isinf(value))
            return value < 0 ? "-inf" : "inf";
        if (value == 0.0)
            return std::signbit(value) ? "-0.0" : "0.0";

        std::string digits;
        int exp10 = 0;
        shortest_digits(std::fabs(value), digits, exp10);

        // format_float_short(..., 'r'): decpt is the position of the decimal point relative to the digits
        const int decpt = exp10 + 1;
        const int n = static_cast<int>(digits.size());
        std::string out = value < 0 ? "-" : "";
        if (decpt <= -4 || decpt > 16)
        {
            out += digits[0];
            if (n > 1)
            {
                out += '.';
                out += digits.substr(1);
            }
            char exp_buf[16];
            std::snprintf(exp_buf, sizeof exp_buf, "e%c%02d", (decpt - 1) < 0 ? '-' : '+', std::abs(decpt - 1));
            out += exp_buf;
        }
        else if (decpt <= 0)
        {
            out += "0.";
            out.append(static_cast<size_t>(-decpt), '0');
            out += digits;
        }
        else if (decpt >= n)
        {
            out += digits;
            out.append(static_cast<size_t>(decpt - n), '0');
            out += ".0";
        }
        else
        {
            out += digits.substr(0, static_cast<size_t>(decpt));
            out += '.';
            out += digits.substr(static_cast<size_t>(decpt));
        }
        return out;
    }

    std::string encode_position(const std::vector<double> &position)
    {
        std::string out;
        for (size_t i = 0; i < position.size(); ++i)
        {
            if (i > 0)
                out += ';';
            out += python_float_repr(position[i]);
        }
        out += '\n';
        return out;
    }

    bool parse_python_float(const std::string &text, double &out)
    {
        return py_float(decode_utf8(text), out);
    }

    std::u32string py_decode_utf8(const std::string &text)
    {
        return decode_utf8(text);
    }

    bool py_is_space(char32_t c)
    {
        return is_space(c);
    }

    int py_decimal_value(char32_t c)
    {
        return decimal_value(c);
    }

    // -------------------------------------------------------------------------------------------------------------
    // Elevations
    // -------------------------------------------------------------------------------------------------------------

    ElevationParts parse_elevation(const std::string &name)
    {
        ElevationParts parts;
        if (name.empty())
            return parts;
        const size_t pos_t = name.rfind(" @T");
        const size_t pos_b = name.rfind(" @B");
        size_t pos = std::string::npos;
        size_t marker_len = 3;
        if (pos_t != std::string::npos && pos_b != std::string::npos)
        {
            pos = std::max(pos_t, pos_b);
            parts.type = (pos == pos_b) ? 1 : 0;
        }
        else if (pos_t != std::string::npos)
        {
            pos = pos_t;
            parts.type = 0;
        }
        else if (pos_b != std::string::npos)
        {
            pos = pos_b;
            parts.type = 1;
        }
        if (pos == std::string::npos)
        {
            size_t pos_at = name.rfind(" @ ");
            if (pos_at == std::string::npos)
                pos_at = name.rfind(" @");
            if (pos_at != std::string::npos)
            {
                pos = pos_at;
                parts.type = 1;
                marker_len = 2;
            }
        }
        if (pos == std::string::npos)
        {
            parts.base_name = name;
            parts.value.clear();
            return parts;
        }
        parts.base_name = name.substr(0, pos);
        size_t val_start = pos + marker_len;
        if (val_start < name.size() && name[val_start] == ' ')
            ++val_start;
        parts.value = (val_start < name.size()) ? name.substr(val_start) : std::string();
        return parts;
    }

    bool is_elevation_text(const std::string &text)
    {
        return is_elevation_u32(decode_utf8(text));
    }

    bool parse_fractional_inches_text(const std::string &text, double &out, std::string *error)
    {
        return parse_fractional_inches_u32(decode_utf8(text), out, error);
    }

    bool parse_imperial_elevation(const std::string &text, double &out, std::string *error)
    {
        return parse_imperial_u32(decode_utf8(text), out, error);
    }

    double parse_metric_elevation(const std::string &text)
    {
        return parse_metric_u32(decode_utf8(text));
    }

    double cm_to_inches(double cm)
    {
        return cm / 2.54;
    }

    ElevationTextStatus elevation_text_inches(const std::string &text, double &inches, std::string *error)
    {
        // elevation_text.py:131-144
        const u32 elevation_text = strip(decode_utf8(text));
        if (!is_elevation_u32(elevation_text))
            return ElevationTextStatus::NotAnElevation;
        const bool has_imperial = contains(elevation_text, U'\'') || contains(elevation_text, 0x2032) ||
                                  contains(elevation_text, U'"');
        // "m" in lowered or "cm" in lowered (the second test is implied by the first)
        const bool has_metric = contains(py_lower(elevation_text), U'm');
        if (has_imperial && !has_metric)
        {
            double value = 0.0;
            if (!parse_imperial_u32(elevation_text, value, error))
                return ElevationTextStatus::Raises;
            inches = value;
            return ElevationTextStatus::Value;
        }
        if (has_metric && !has_imperial)
        {
            inches = cm_to_inches(parse_metric_u32(elevation_text));
            return ElevationTextStatus::Value;
        }
        return ElevationTextStatus::NotAnElevation; // units of both systems in one text
    }

    bool extract_z_value_from_name(const std::string &name, double &z_inches, bool &is_top, std::string *error)
    {
        // position_parser.py:23-33: parse_elevation, then elevation_text_inches of the value; None is (0.0, False), an exception
        // propagates (here: false)
        z_inches = 0.0;
        is_top = false;
        if (name.empty() || name.find('@') == std::string::npos)
            return true;
        const ElevationParts parts = parse_elevation(name);
        if (parts.value.empty())
            return true;
        double inches = 0.0;
        switch (elevation_text_inches(parts.value, inches, error))
        {
        case ElevationTextStatus::Value:
            z_inches = inches;
            is_top = (parts.type == 0);
            return true;
        case ElevationTextStatus::Raises:
            return false;
        case ElevationTextStatus::NotAnElevation:
            break;
        }
        return true;
    }

    void derive_elevation_from_name(const std::string &name, double &z_inches, bool &is_top)
    {
        // bid_data_reader.py:896-906: `except ValueError: z_value, is_top = 0.0, False`
        if (!extract_z_value_from_name(name, z_inches, is_top))
        {
            z_inches = 0.0;
            is_top = false;
        }
    }
}
