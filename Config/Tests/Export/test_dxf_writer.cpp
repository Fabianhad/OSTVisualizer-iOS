// DXF writer (Core/Export/dxf_writer.*): the file text against the Windows native ost_dxf writer, and the frame of the
// mesh lines (EXP-02). Oracle: ExportFileOracleData.hpp (the Windows writer / exporter run read-only).
#include "ExportTestSupport.hpp"
#include "ExportFileOracleData.hpp"
#include "Export/dxf_writer.hpp"
#include "Geometry/feature_edges.hpp"
#include "Meshing/MeshFactory.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace export_test;
using ost_domain::Condition;
using ost_domain::ConditionType;
using ost_domain::ShapeId;
using ost_domain::Takeoff;

namespace
{
    std::string section_between(const std::string &text, const std::string &from, const std::string &to)
    {
        const size_t a = text.find(from);
        if (a == std::string::npos)
            return "<missing " + from + ">";
        const size_t b = to.empty() ? text.size() : text.find(to, a);
        if (b == std::string::npos)
            return "<missing " + to + ">";
        return text.substr(a, b - a);
    }

    // Everything up to the CLASSES section (the HEADER section) and the ENTITIES section of a DXF text.
    std::string header_section(const std::string &text)
    {
        const size_t b = text.find("  0\nSECTION\n  2\nCLASSES");
        return b == std::string::npos ? "<no CLASSES section>" : text.substr(0, b);
    }

    std::string entities_section(const std::string &text)
    {
        return section_between(text, "  0\nSECTION\n  2\nENTITIES\n", "  0\nSECTION\n  2\nOBJECTS");
    }

    struct ParsedLine
    {
        std::string layer;
        std::array<double, 6> p{};
    };

    // The LINE entities of a DXF text: layer (group 8) and the endpoints (groups 10 20 30 / 11 21 31).
    std::vector<ParsedLine> parse_lines(const std::string &entities)
    {
        std::vector<ParsedLine> out;
        std::vector<std::string> rows;
        size_t pos = 0;
        while (pos < entities.size())
        {
            size_t end = entities.find('\n', pos);
            if (end == std::string::npos)
                end = entities.size();
            std::string row = entities.substr(pos, end - pos);
            const size_t s = row.find_first_not_of(' ');
            const size_t e = row.find_last_not_of(" \r");
            rows.push_back(s == std::string::npos ? std::string() : row.substr(s, e - s + 1));
            pos = end + 1;
        }
        bool in_line = false;
        ParsedLine cur;
        for (size_t i = 0; i + 1 < rows.size(); i += 2)
        {
            const std::string &code = rows[i];
            const std::string &value = rows[i + 1];
            if (code == "0")
            {
                if (in_line)
                    out.push_back(cur);
                in_line = value == "LINE";
                cur = ParsedLine();
            }
            else if (in_line && code == "8")
                cur.layer = value;
            else if (in_line && code.size() == 2 && (code[0] == '1' || code[0] == '2' || code[0] == '3') && (code[1] == '0' || code[1] == '1'))
            {
                const int axis = code[0] - '1';
                const int end_index = code[1] == '1' ? 1 : 0;
                // 10 20 30 are the start (axis 0, 1, 2 -> codes 10, 20, 30); 11 21 31 the end
                cur.p[static_cast<size_t>(end_index * 3 + axis)] = std::strtod(value.c_str(), nullptr);
            }
        }
        if (in_line)
            out.push_back(cur);
        return out;
    }

    // Unordered match of two line sets (the LINE order of a DXF is the STL order of the feature-edge set, EXP-24; the
    // endpoint order follows the vertex order, EXP-17): every expected line has one unused line with the same layer whose
    // endpoints agree, in either direction, within `tol`. Signed zeros are not told apart.
    bool lines_match_as_sets(const std::vector<ParsedLine> &got, const std::vector<std::array<double, 6>> &expected,
                             double tol, std::string &why)
    {
        if (got.size() != expected.size())
        {
            why = "line count " + std::to_string(got.size()) + " != " + std::to_string(expected.size());
            return false;
        }
        std::vector<bool> used(got.size(), false);
        for (size_t i = 0; i < expected.size(); ++i)
        {
            bool found = false;
            for (size_t j = 0; j < got.size() && !found; ++j)
            {
                if (used[j])
                    continue;
                for (int flip = 0; flip < 2 && !found; ++flip)
                {
                    bool ok = true;
                    for (int k = 0; k < 3; ++k)
                    {
                        const double a = got[j].p[static_cast<size_t>(flip ? 3 + k : k)];
                        const double b = got[j].p[static_cast<size_t>(flip ? k : 3 + k)];
                        if (std::fabs(a - expected[i][static_cast<size_t>(k)]) > tol || std::fabs(b - expected[i][static_cast<size_t>(3 + k)]) > tol)
                            ok = false;
                    }
                    if (ok)
                    {
                        used[j] = true;
                        found = true;
                    }
                }
            }
            if (!found)
            {
                why = "no line near expected line " + std::to_string(i);
                return false;
            }
        }
        return true;
    }

    // The ".000000" text of a number as the Windows file has it, with the sign of a zero ignored.
    std::string zero_sign_free(std::string text)
    {
        size_t pos = 0;
        while ((pos = text.find("-0.000000\n", pos)) != std::string::npos)
        {
            // only a whole value: preceded by a line start
            if (pos == 0 || text[pos - 1] == '\n')
                text.erase(pos, 1);
            else
                pos += 1;
        }
        return text;
    }
} // namespace

// ---------------------------------------------------------------- the writer itself (unchanged, Windows == iOS)
TEST(DxfWriter, TextEqualsTheWindowsNativeWriterByteForByte)
{
    // Layers (every ACI branch: red, yellow, green, cyan, blue, magenta, white, grey, the 6x6x6 cube, true colour 420),
    // a layer re-added with another colour, the default layer "0", signed zeros, tiny and huge coordinates, HANDSEED and
    // EXTMIN / EXTMAX: the text of the native ost_dxf.DXFWriter of the Windows app for the same layers and lines.
    const auto &sc = export_oracle::kDxfTextScenario;
    ScratchDir dir("dxf_text");
    ost_geometry::DXFWriter w;
    for (const auto &l : sc.layers)
        w.add_layer(l.name, std::string(l.hex));
    for (const auto &l : sc.lines)
        w.add_line(l.a, l.b, l.layer);
    REQUIRE(w.save(dir.file("t.dxf")));
    CHECK_STR_EQ(read_text_lf(dir.file("t.dxf")), sc.expected);
    CHECK_EQ(w.get_line_count(), sc.lines.size());
}

// ---------------------------------------------------------------- EXP-02 frame
TEST(DxfFrame, IosMeshPointsMapToTheZUpDxfFrame)
{
    // iOS (X, Y up, Z) -> DXF (X, -Z, Y): plan in XY, height in Z.
    const auto p = ost_geometry::ios_to_dxf_point({1.5, 96.0, -3.25});
    CHECK_EQ(p[0], 1.5);
    CHECK_EQ(p[1], 3.25);
    CHECK_EQ(p[2], 96.0);
    const auto q = ost_geometry::ios_to_dxf_point({-100.0, 0.0, 3.0});
    CHECK_EQ(q[0], -100.0);
    CHECK_EQ(q[1], -3.0);
    CHECK_EQ(q[2], 0.0);
}

TEST(DxfFrame, MapIsTheInverseOfTheWindowsObjFrameBitForBit)
{
    // The Windows DXF point is the Z-up mesh point (x, y, z); the iOS mesh point is the Windows OBJ / viewer point
    // (x, z, -y). (X, -Z, Y) of the second is the first again, signs of zeros included.
    const std::vector<double> values = {0.0, -0.0, 1.0, -1.0, 3.0, 96.0, -100.0, 1e-9, -1e-9, 0.1};
    for (double x : values)
        for (double y : values)
            for (double z : values)
            {
                const V3 ios{x, z, -y};
                const auto d = ost_geometry::ios_to_dxf_point(ios);
                CHECK(std::memcmp(&d[0], &x, sizeof(double)) == 0);
                CHECK(std::memcmp(&d[1], &y, sizeof(double)) == 0);
                CHECK(std::memcmp(&d[2], &z, sizeof(double)) == 0);
            }
}

TEST(DxfFrame, MeshEdgesAreWrittenAsTheWindowsDxfBytes)
{
    // Port of tests/presentation/visualization/exporters/test_dxf_exporter.py::
    // DXFExporterTests::test_writer_is_reset_and_receives_only_positive_feature_edges (the line list goes to the writer
    // as it is) on three real Windows meshes: wall [0,0,100,0] height 96 thickness 6 (vertex set
    // (-0,-3,0),(0,3,0),(-100,3,0),(-100,-3,0) and the same four at z 96), a count box and a slab. The mesh is given in
    // the iOS frame with the Windows feature-edge list; the header and the LINE entities must equal the Windows file.
    // The first scenario is compared as a whole file.
    for (const auto &sc : export_oracle::kDxfMeshScenarios)
    {
        SCOPED_CONTEXT(sc.name);
        ScratchDir dir(std::string("dxf_mesh_") + sc.name);
        ost_geometry::DXFWriter w;
        w.add_layer(sc.layer, std::string(sc.hex));
        Mesh mesh = make_mesh(sc.vertices, sc.faces);
        mesh.edges = sc.edges;
        w.add_mesh_as_lines(mesh, sc.layer);
        CHECK_EQ(w.get_line_count(), sc.edges.size());
        REQUIRE(w.save(dir.file("m.dxf")));
        const std::string got = read_text_lf(dir.file("m.dxf"));
        CHECK_STR_EQ(header_section(got), sc.header);
        CHECK_STR_EQ(entities_section(got), sc.entities);
        if (std::strlen(sc.full) > 0)
            CHECK_STR_EQ(got, sc.full);
    }
}

TEST(DxfFrame, WallLinesHaveThePlanInXyAndTheHeightInZ)
{
    // The headline case of the EXP-02 plan: wall [0,0,100,0] height 96 thickness 6 in the iOS frame (vertices
    // (X, Y, Z) = (-ost_x, height, -ost_y +- 3)); every DXF endpoint has |y| = 3, z in {0, 96} and x in {-100, 0}.
    Condition cond;
    cond.uid = "wall";
    cond.name = "wall";
    cond.condition_type = ConditionType::Linear;
    cond.height = 96.0;
    cond.thickness = 6.0;
    Takeoff t;
    t.uid = "t";
    t.condition_uid = "wall";
    t.page_uid = "p";
    t.position = {0.0, 0.0, 100.0, 0.0};
    t.parent_uid = "0";
    auto r = ost_meshing::create_mesh_for_takeoff(t, cond, 1.0);
    REQUIRE(r.valid);
    Mesh mesh = r.mesh;
    mesh.edges = ost_geometry::extract_feature_edges(mesh, 0.1);
    REQUIRE_EQ(mesh.edges.size(), static_cast<size_t>(12));
    ost_geometry::DXFWriter w;
    w.add_mesh_as_lines(mesh, "0");
    ScratchDir dir("dxf_wall_frame");
    REQUIRE(w.save(dir.file("w.dxf")));
    const auto lines = parse_lines(entities_section(read_text_lf(dir.file("w.dxf"))));
    REQUIRE_EQ(lines.size(), static_cast<size_t>(12));
    for (const auto &l : lines)
        for (int e = 0; e < 2; ++e)
        {
            const double x = l.p[static_cast<size_t>(e * 3)], y = l.p[static_cast<size_t>(e * 3 + 1)], z = l.p[static_cast<size_t>(e * 3 + 2)];
            CHECK(std::fabs(x) < 1e-12 || std::fabs(x + 100.0) < 1e-12);
            CHECK(std::fabs(std::fabs(y) - 3.0) < 1e-12);
            CHECK(std::fabs(z) < 1e-12 || std::fabs(z - 96.0) < 1e-12);
        }
}

TEST(DxfFrame, EndToEndMeshesGiveTheLinesOfTheWindowsDxfExport)
{
    // Whole pipeline per scenario: iOS mesh of the same takeoff (create_mesh_for_takeoff) -> extract_feature_edges(mesh,
    // 0.1) -> DXFWriter::add_mesh_as_lines -> file; against the file of the real Windows DXFExporter.export (OSTCoordinate
    // System, MeshFactory, native feature edges). The LINE order and the endpoint order are not compared (EXP-24 / EXP-17:
    // MSVC-STL set order, vertex order), only the set of lines per layer; the boxes (wall, count box, slab) are exact to
    // the 6 printed decimals, circle and curved wall (trigonometry) within 1e-5. The header's EXTMIN / EXTMAX are the min /
    // max of the lines (signs of zero aside) and HANDSEED depends on the line count.
    struct Case
    {
        const char *name;
        Condition condition;
        Takeoff takeoff;
    };
    std::vector<Case> cases;
    auto cond = [](const std::string &uid, ConditionType type)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.condition_type = type;
        return c;
    };
    auto take = [](const std::string &cuid, std::vector<double> pos, int curve = -1)
    {
        Takeoff t;
        t.uid = "t";
        t.condition_uid = cuid;
        t.page_uid = "p";
        t.position = std::move(pos);
        t.parent_uid = "0";
        t.curve = curve;
        return t;
    };
    Condition wall = cond("wall", ConditionType::Linear);
    wall.height = 96.0;
    wall.thickness = 6.0;
    cases.push_back({"wall", wall, take("wall", {0.0, 0.0, 100.0, 0.0})});
    Condition circle = cond("circle", ConditionType::Count);
    circle.shape = ShapeId::Circle;
    circle.width = circle.depth = 24.0;
    circle.height = 48.0;
    circle.display_size = 100.0;
    cases.push_back({"circle", circle, take("circle", {0.0, 0.0})});
    Condition curved = cond("curved", ConditionType::Linear);
    curved.height = 96.0;
    curved.thickness = 6.0;
    cases.push_back({"curved_wall", curved, take("curved", {0.0, 200.0, 100.0, 200.0, 50.0, 230.0}, 0)});
    Condition slab = cond("slab", ConditionType::Area);
    slab.height = slab.thickness = 6.0;
    cases.push_back({"slab", slab, take("slab", {0.0, 0.0, 100.0, 0.0, 100.0, 100.0, 0.0, 100.0})});
    Condition box = cond("box", ConditionType::Count);
    box.shape = ShapeId::Square;
    box.width = box.depth = 20.0;
    box.height = 10.0;
    box.display_size = 100.0;
    cases.push_back({"box", box, take("box", {0.0, 0.0})});

    size_t checked = 0;
    for (const auto &c : cases)
    {
        const export_oracle::PipelineScenario *oracle = nullptr;
        for (const auto &p : export_oracle::kPipelineScenarios)
            if (std::string(p.name) == c.name)
                oracle = &p;
        REQUIRE(oracle != nullptr);
        SCOPED_CONTEXT(c.name);
        auto r = ost_meshing::create_mesh_for_takeoff(c.takeoff, c.condition, 1.0);
        REQUIRE(r.valid);
        Mesh clean = r.mesh;
        clean.edges = ost_geometry::extract_feature_edges(clean, 0.1);
        ost_geometry::DXFWriter w;
        w.add_layer("Layer", ost_geometry::RGBColor(255, 128, 0));
        w.add_mesh_as_lines(clean, "Layer");
        ScratchDir dir(std::string("dxf_e2e_") + c.name);
        REQUIRE(w.save(dir.file("e.dxf")));
        const std::string got = read_text_lf(dir.file("e.dxf"));
        const auto lines = parse_lines(entities_section(got));
        std::string why;
        const double tol = oracle->exact ? 0.5e-6 : 1e-5;
        if (!lines_match_as_sets(lines, oracle->lines, tol, why))
            ost_test::report_failure(__FILE__, __LINE__, "LINE sets differ: " + why);
        // the header reads the same extents (and the same HANDSEED, which counts the lines)
        if (oracle->exact)
            CHECK_STR_EQ(zero_sign_free(header_section(got)), zero_sign_free(oracle->dxf_header));
        ++checked;
    }
    CHECK_EQ(checked, static_cast<size_t>(5));
}
