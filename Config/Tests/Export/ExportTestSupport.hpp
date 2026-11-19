// ExportTestSupport.hpp - shared helpers of the Export area tests (OBJ / MTL, DXF and Three.js HTML writers).
// Not a test_*.cpp file, so CMake does not compile it alone.
#pragma once

#include "TestHarness.hpp"
#include "Domain/Condition.hpp"
#include "Domain/Takeoff.hpp"
#include "Geometry/mesh_data.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace export_test
{
    using Mesh = ost_geometry::CppMeshData;
    using V3 = std::array<double, 3>;

    // A scratch directory below the system temp directory (never inside the repo), removed again on scope exit. The
    // files keep their plain names ("model.obj"), so an OBJ writes "mtllib model.mtl" like the Windows oracle text.
    class ScratchDir
    {
    public:
        explicit ScratchDir(const std::string &name)
        {
            namespace fs = std::filesystem;
            std::error_code ec;
            dir_ = fs::temp_directory_path(ec) / "ost_core_tests" / ("export_" + name);
            fs::remove_all(dir_, ec);
            fs::create_directories(dir_, ec);
        }
        ~ScratchDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(dir_, ec);
        }
        ScratchDir(const ScratchDir &) = delete;
        ScratchDir &operator=(const ScratchDir &) = delete;
        std::string file(const std::string &name) const { return (dir_ / name).generic_string(); }

    private:
        std::filesystem::path dir_;
    };

    // The whole file with CRLF turned into LF: the writers open text streams, which write CRLF on a Windows host and LF
    // on Apple platforms (the Windows exporters do the same on Windows).
    inline std::string read_text_lf(const std::string &path)
    {
        std::string raw;
        if (!ost_test::read_file(path, raw))
            return "<missing file " + path + ">";
        std::string out;
        out.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i)
        {
            if (raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n')
                continue;
            out += raw[i];
        }
        return out;
    }

    inline Mesh make_mesh(const std::vector<V3> &vertices, const std::vector<std::array<uint32_t, 3>> &faces)
    {
        Mesh m;
        m.vertices = vertices;
        m.faces = faces;
        return m;
    }

    inline double signed_volume(const std::vector<V3> &vertices, const std::vector<std::array<uint32_t, 3>> &faces)
    {
        double total = 0.0;
        for (const auto &f : faces)
        {
            const V3 &a = vertices[f[0]];
            const V3 &b = vertices[f[1]];
            const V3 &c = vertices[f[2]];
            total += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
                      a[2] * (b[0] * c[1] - b[1] * c[0])) /
                     6.0;
        }
        return total;
    }

    inline double signed_volume(const Mesh &m) { return signed_volume(m.vertices, m.faces); }

    // A closed shell whose triangles all face the same way: every directed edge (a, b) is used by exactly one face and
    // its reverse (b, a) by exactly one other. A single flipped face (a cap of a cylinder lying in the y = 0 plane adds
    // nothing to the signed volume) breaks this although the volume stays positive.
    inline bool closed_and_consistently_oriented(const std::vector<std::array<uint32_t, 3>> &faces)
    {
        std::map<std::pair<uint32_t, uint32_t>, int> directed;
        for (const auto &f : faces)
            for (int k = 0; k < 3; ++k)
                ++directed[{f[static_cast<size_t>(k)], f[static_cast<size_t>((k + 1) % 3)]}];
        for (const auto &kv : directed)
        {
            if (kv.second != 1)
                return false;
            const auto reverse = directed.find({kv.first.second, kv.first.first});
            if (reverse == directed.end() || reverse->second != 1)
                return false;
        }
        return true;
    }

    // The 'v' and 'f' lines of an OBJ text (1-based face indices as written, turned 0-based).
    struct ObjData
    {
        std::vector<V3> vertices;
        std::vector<std::array<uint32_t, 3>> faces;
    };

    inline ObjData parse_obj(const std::string &text)
    {
        ObjData d;
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos)
                end = text.size();
            const std::string line = text.substr(pos, end - pos);
            pos = end + 1;
            if (line.rfind("v ", 0) == 0)
            {
                V3 v{};
                if (std::sscanf(line.c_str(), "v %lf %lf %lf", &v[0], &v[1], &v[2]) == 3)
                    d.vertices.push_back(v);
            }
            else if (line.rfind("f ", 0) == 0)
            {
                unsigned a = 0, b = 0, c = 0;
                if (std::sscanf(line.c_str(), "f %u %u %u", &a, &b, &c) == 3)
                    d.faces.push_back({a - 1, b - 1, c - 1});
            }
        }
        return d;
    }

    inline double obj_signed_volume(const std::string &text)
    {
        const ObjData d = parse_obj(text);
        return signed_volume(d.vertices, d.faces);
    }

    // The colour fill (BGR int) of the byte colour, so tests build colours exactly like the app: float(byte) / 255.0f.
    inline void colour_floats(int r, int g, int b, float &fr, float &fg, float &fb)
    {
        ost_domain::Condition::rgb_from_color_fill(r | (g << 8) | (b << 16), fr, fg, fb);
    }

    // ---- a strict JSON reader (RFC 8259) for the scene data block of an exported page ----
    struct Json
    {
        enum class Kind
        {
            Null,
            Bool,
            Number,
            String,
            Array,
            Object
        };
        Kind kind = Kind::Null;
        bool boolean = false;
        double number = 0.0;
        std::string text; // UTF-8
        std::vector<Json> items;
        std::vector<std::pair<std::string, Json>> members; // document order

        const Json *get(const std::string &key) const
        {
            for (const auto &m : members)
                if (m.first == key)
                    return &m.second;
            return nullptr;
        }
        std::vector<double> numbers() const
        {
            std::vector<double> out;
            for (const auto &i : items)
                out.push_back(i.number);
            return out;
        }
    };

    class JsonParser
    {
    public:
        explicit JsonParser(const std::string &s) : s_(s) {}

        bool parse(Json &out, std::string &error)
        {
            skip_ws();
            if (!value(out))
            {
                error = error_.empty() ? "invalid JSON" : error_;
                error += " at offset " + std::to_string(pos_);
                return false;
            }
            skip_ws();
            if (pos_ != s_.size())
            {
                error = "trailing characters at offset " + std::to_string(pos_);
                return false;
            }
            return true;
        }

    private:
        const std::string &s_;
        size_t pos_ = 0;
        std::string error_;

        bool fail(const char *why)
        {
            error_ = why;
            return false;
        }
        void skip_ws()
        {
            while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r'))
                ++pos_;
        }
        bool literal(const char *word)
        {
            size_t n = std::strlen(word);
            if (s_.compare(pos_, n, word) != 0)
                return fail("bad literal");
            pos_ += n;
            return true;
        }
        static void append_utf8(std::string &out, unsigned cp)
        {
            if (cp < 0x80)
                out += static_cast<char>(cp);
            else if (cp < 0x800)
            {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else if (cp < 0x10000)
            {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else
            {
                out += static_cast<char>(0xF0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
        bool hex4(unsigned &cp)
        {
            if (pos_ + 4 > s_.size())
                return fail("short unicode escape");
            cp = 0;
            for (int i = 0; i < 4; ++i)
            {
                char c = s_[pos_ + i];
                cp <<= 4;
                if (c >= '0' && c <= '9')
                    cp |= static_cast<unsigned>(c - '0');
                else if (c >= 'a' && c <= 'f')
                    cp |= static_cast<unsigned>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F')
                    cp |= static_cast<unsigned>(c - 'A' + 10);
                else
                    return fail("bad unicode escape");
            }
            pos_ += 4;
            return true;
        }
        bool string_value(std::string &out)
        {
            ++pos_; // opening quote
            while (pos_ < s_.size())
            {
                unsigned char c = static_cast<unsigned char>(s_[pos_]);
                if (c == '"')
                {
                    ++pos_;
                    return true;
                }
                if (c < 0x20)
                    return fail("control character in string");
                if (c != '\\')
                {
                    out += static_cast<char>(c);
                    ++pos_;
                    continue;
                }
                ++pos_;
                if (pos_ >= s_.size())
                    return fail("dangling backslash");
                char e = s_[pos_++];
                switch (e)
                {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u':
                {
                    unsigned cp = 0;
                    if (!hex4(cp))
                        return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF)
                    {
                        if (s_.compare(pos_, 2, "\\u") != 0)
                            return fail("lone high surrogate");
                        pos_ += 2;
                        unsigned lo = 0;
                        if (!hex4(lo))
                            return false;
                        if (lo < 0xDC00 || lo > 0xDFFF)
                            return fail("bad low surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    else if (cp >= 0xDC00 && cp <= 0xDFFF)
                        return fail("lone low surrogate");
                    append_utf8(out, cp);
                    break;
                }
                default:
                    return fail("bad escape");
                }
            }
            return fail("unterminated string");
        }
        bool number_value(double &out)
        {
            size_t start = pos_;
            if (pos_ < s_.size() && s_[pos_] == '-')
                ++pos_;
            if (pos_ >= s_.size())
                return fail("bad number");
            if (s_[pos_] == '0')
                ++pos_;
            else if (s_[pos_] >= '1' && s_[pos_] <= '9')
                while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_])))
                    ++pos_;
            else
                return fail("bad number");
            if (pos_ < s_.size() && s_[pos_] == '.')
            {
                ++pos_;
                size_t d = pos_;
                while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_])))
                    ++pos_;
                if (pos_ == d)
                    return fail("bad fraction");
            }
            if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E'))
            {
                ++pos_;
                if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-'))
                    ++pos_;
                size_t d = pos_;
                while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_])))
                    ++pos_;
                if (pos_ == d)
                    return fail("bad exponent");
            }
            out = std::strtod(s_.substr(start, pos_ - start).c_str(), nullptr);
            return true;
        }
        bool value(Json &out)
        {
            if (pos_ >= s_.size())
                return fail("unexpected end");
            char c = s_[pos_];
            if (c == '{')
            {
                out.kind = Json::Kind::Object;
                ++pos_;
                skip_ws();
                if (pos_ < s_.size() && s_[pos_] == '}')
                {
                    ++pos_;
                    return true;
                }
                while (true)
                {
                    skip_ws();
                    if (pos_ >= s_.size() || s_[pos_] != '"')
                        return fail("object key expected");
                    std::string key;
                    if (!string_value(key))
                        return false;
                    skip_ws();
                    if (pos_ >= s_.size() || s_[pos_] != ':')
                        return fail("':' expected");
                    ++pos_;
                    skip_ws();
                    Json v;
                    if (!value(v))
                        return false;
                    out.members.emplace_back(std::move(key), std::move(v));
                    skip_ws();
                    if (pos_ < s_.size() && s_[pos_] == ',')
                    {
                        ++pos_;
                        continue;
                    }
                    if (pos_ < s_.size() && s_[pos_] == '}')
                    {
                        ++pos_;
                        return true;
                    }
                    return fail("',' or '}' expected");
                }
            }
            if (c == '[')
            {
                out.kind = Json::Kind::Array;
                ++pos_;
                skip_ws();
                if (pos_ < s_.size() && s_[pos_] == ']')
                {
                    ++pos_;
                    return true;
                }
                while (true)
                {
                    skip_ws();
                    Json v;
                    if (!value(v))
                        return false;
                    out.items.push_back(std::move(v));
                    skip_ws();
                    if (pos_ < s_.size() && s_[pos_] == ',')
                    {
                        ++pos_;
                        continue;
                    }
                    if (pos_ < s_.size() && s_[pos_] == ']')
                    {
                        ++pos_;
                        return true;
                    }
                    return fail("',' or ']' expected");
                }
            }
            if (c == '"')
            {
                out.kind = Json::Kind::String;
                return string_value(out.text);
            }
            if (c == 't')
            {
                out.kind = Json::Kind::Bool;
                out.boolean = true;
                return literal("true");
            }
            if (c == 'f')
            {
                out.kind = Json::Kind::Bool;
                out.boolean = false;
                return literal("false");
            }
            if (c == 'n')
            {
                out.kind = Json::Kind::Null;
                return literal("null");
            }
            out.kind = Json::Kind::Number;
            return number_value(out.number);
        }
    };

    inline bool parse_json(const std::string &text, Json &out, std::string &error)
    {
        JsonParser p(text);
        return p.parse(out, error);
    }

    // The text between '<script type="application/json" id="scene-data">' and the next '</script>', trimmed: how the
    // Windows tests cut the scene out of an exported page.
    inline std::string scene_data_block(const std::string &html)
    {
        const std::string open = "<script type=\"application/json\" id=\"scene-data\">";
        size_t a = html.find(open);
        if (a == std::string::npos)
            return "<no scene-data block>";
        a += open.size();
        size_t b = html.find("</script>", a);
        if (b == std::string::npos)
            return "<unterminated scene-data block>";
        std::string block = html.substr(a, b - a);
        size_t s = block.find_first_not_of(" \t\r\n");
        size_t e = block.find_last_not_of(" \t\r\n");
        return s == std::string::npos ? std::string() : block.substr(s, e - s + 1);
    }

    inline std::string title_element(const std::string &html)
    {
        size_t a = html.find("<title>");
        size_t b = html.find("</title>");
        if (a == std::string::npos || b == std::string::npos || b < a)
            return "<no title>";
        return html.substr(a, b + 8 - a);
    }

    inline size_t count_of(const std::string &haystack, const std::string &needle)
    {
        size_t n = 0;
        for (size_t p = haystack.find(needle); p != std::string::npos; p = haystack.find(needle, p + needle.size()))
            ++n;
        return n;
    }

    // Windows _compact_code(): the tests of the viewer template ignore whitespace (the template is prettier-formatted).
    inline std::string compact_code(const std::string &s)
    {
        std::string out;
        for (char c : s)
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
                out += c;
        return out;
    }
} // namespace export_test
