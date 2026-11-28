// OBJ / MTL writer (Core/Export/obj_writer.*) against the text of the Windows OBJExporter (EXP-16, EXP-12) and the winding
// of the exported meshes (EXP-13). Oracle: ExportFileOracleData.hpp (the Windows exporter run read-only).
// Since Windows b9902e6 the names are written like this on both sides: every control character (U+0000..U+001F, U+007F) of a
// 'g' / 'usemtl' / 'newmtl' name is one '_' (EXP-12) and every CR and LF of a '# <name>' comment line is one space (EXP-16),
// so no name can add a statement to the OBJ or the MTL (the Exp12_ / Exp16_ tests).
#include "ExportTestSupport.hpp"
#include "ExportFileOracleData.hpp"
#include "ExportTextOracleData.hpp"
#include "Export/obj_writer.hpp"
#include "Geometry/boolean_ops.hpp"
#include "Meshing/MeshFactory.hpp"
#include "Meshing/MeshGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace export_test;
using ost_domain::Condition;
using ost_domain::ConditionType;
using ost_domain::ShapeId;
using ost_domain::Takeoff;

namespace
{
    // The writer of a scenario of ExportFileOracleData.hpp: one group per condition, colours as the app has them.
    void fill_writer(ost_geometry::OBJWriter &w, const export_oracle::ObjScenario &sc)
    {
        w.set_display_mode(sc.display_mode);
        for (const auto &g : sc.groups)
        {
            float r = 0, gg = 0, b = 0;
            colour_floats(g.r, g.g, g.b, r, gg, b);
            w.add_mesh(make_mesh(g.vertices, g.faces), g.raw_name, g.original_name, r, gg, b);
        }
    }

    std::vector<std::string> lines_with_prefix(const std::string &text, const std::string &prefix)
    {
        std::vector<std::string> out;
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos)
                end = text.size();
            if (text.compare(pos, prefix.size(), prefix) == 0)
                out.push_back(text.substr(pos + prefix.size(), end - pos - prefix.size()));
            pos = end + 1;
        }
        return out;
    }

    // The lines of a text that is LF separated (read_text_lf) like Python's str.splitlines for such a text: no empty piece
    // after the last LF. Only LF ends a line here, so a stray CR or NUL of a name stays visible inside its line.
    std::vector<std::string> split_lines(const std::string &text)
    {
        std::vector<std::string> out;
        size_t pos = 0;
        while (pos < text.size())
        {
            size_t end = text.find('\n', pos);
            if (end == std::string::npos)
                end = text.size();
            out.push_back(text.substr(pos, end - pos));
            pos = end + 1;
        }
        return out;
    }

    // Every line is empty or starts with one of the OBJ / MTL statements the writer produces: a name that injected a line
    // would add one that starts with something else ('newmtl evil', 'Kd 1 0 0', 'v 1 2 3' inside a comment is no statement).
    bool every_line_is_a_known_statement(const std::vector<std::string> &lines, const std::vector<std::string> &prefixes)
    {
        for (const auto &line : lines)
        {
            if (line.empty())
                continue;
            bool known = false;
            for (const auto &p : prefixes)
                known = known || line.compare(0, p.size(), p) == 0;
            if (!known)
                return false;
        }
        return true;
    }

    const std::vector<std::string> kObjStatements = {"# ", "mtllib ", "g ", "usemtl ", "v ", "f "};
    const std::vector<std::string> kMtlStatements = {"# ", "newmtl ", "Kd ", "Ka ", "Ks ", "Ns ", "Ni ", "d ", "illum "};

    Mesh triangle()
    {
        return make_mesh({{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}}, {{0, 2, 1}});
    }
} // namespace

// ---------------------------------------------------------------- EXP-16 file text
TEST(ObjWriter, ObjAndMtlTextEqualTheWindowsExporterByteForByte)
{
    // Port of tests/presentation/visualization/exporters/test_obj_exporter.py::
    // OBJExporterTests::test_export_resets_indices_skips_backouts_and_writes_paired_material_file (scenario
    // windows_unit_test: the literal text of that test) plus two more Windows runs: three conditions with irregular
    // doubles, signed zeros, unsafe names and the colour byte (80, 131, 182) whose Kd differs between float and double,
    // and the default display mode "Original". Header '# Created by OST Visualizer', '# Display Mode', '# Materials: n',
    // '# <original name>' before every group and in the MTL, Kd %.6f, Ka 0.2, Ks 0.5, Ns 20, Ni 1, d 1, illum 2.
    for (const auto &sc : export_oracle::kObjScenarios)
    {
        SCOPED_CONTEXT(sc.name);
        ScratchDir dir(std::string("obj_") + sc.name);
        ost_geometry::OBJWriter w;
        fill_writer(w, sc);
        REQUIRE(w.save(dir.file("model.obj")));
        CHECK_STR_EQ(read_text_lf(dir.file("model.obj")), sc.obj);
        CHECK_STR_EQ(read_text_lf(dir.file("model.mtl")), sc.mtl);
    }
}

TEST(ObjWriter, MtlKdIsTheColourByteOverTwoFiftyFiveToSixDecimals)
{
    // Windows hex_to_rgb(byte) / 255.0 formatted {:.6f} for every byte. Computed from the stored float, the bytes 80, 131
    // and 182 would print 0.313726 / 0.513726 / 0.713726 instead of 0.313725 / 0.513725 / 0.713725.
    ScratchDir dir("obj_kd_bytes");
    ost_geometry::OBJWriter w;
    for (int k = 0; k < 256; ++k)
    {
        float r = 0, g = 0, b = 0;
        colour_floats(k, (k + 85) % 256, (k + 170) % 256, r, g, b);
        w.add_mesh(triangle(), "m" + std::to_string(k), r, g, b);
    }
    REQUIRE(w.save(dir.file("kd.obj")));
    const std::vector<std::string> kd = lines_with_prefix(read_text_lf(dir.file("kd.mtl")), "Kd ");
    REQUIRE_EQ(kd.size(), static_cast<size_t>(256));
    for (int k = 0; k < 256; ++k)
    {
        SCOPED_CONTEXT("byte " + std::to_string(k));
        const std::string expected = std::string(export_oracle::kKdByByte[k]) + " " + export_oracle::kKdByByte[(k + 85) % 256] +
                                     " " + export_oracle::kKdByByte[(k + 170) % 256];
        CHECK_STR_EQ(kd[static_cast<size_t>(k)], expected);
    }
}

TEST(ObjWriter, DefaultDisplayModeIsTheWindowsDefaultOriginal)
{
    // The display mode comes from the config in Windows (Config.DEFAULT_DISPLAY_MODE = "Original"; the iOS RenderConfig
    // default is the same text); an explicit mode is written as given.
    ScratchDir dir("obj_mode");
    ost_geometry::OBJWriter w;
    w.add_mesh(triangle(), "a", 0.5f, 0.5f, 0.5f);
    REQUIRE(w.save(dir.file("m.obj")));
    const std::string mtl = read_text_lf(dir.file("m.mtl"));
    CHECK(mtl.rfind("# Created by OST Visualizer\n# Display Mode: Original\n# Materials: 1\n\nnewmtl a\n# a\n", 0) == 0);
    w.set_display_mode("Transparent");
    REQUIRE(w.save(dir.file("m.obj")));
    CHECK(read_text_lf(dir.file("m.mtl")).rfind("# Created by OST Visualizer\n# Display Mode: Transparent\n", 0) == 0);
}

TEST(ObjWriter, MaterialsCountAndOriginalNameDefault)
{
    // '# Materials: n' counts the materials written; without an original name the comment lines carry the group name.
    ScratchDir dir("obj_count");
    ost_geometry::OBJWriter w;
    w.add_mesh(triangle(), "First one", 1.0f, 0.0f, 0.0f);
    w.add_mesh(triangle(), "Second/one", 0.0f, 1.0f, 0.0f);
    w.add_mesh(Mesh(), "no faces", 0.0f, 0.0f, 1.0f); // an empty mesh is skipped
    REQUIRE(w.save(dir.file("c.obj")));
    const std::string obj = read_text_lf(dir.file("c.obj"));
    const std::string mtl = read_text_lf(dir.file("c.mtl"));
    CHECK(mtl.find("# Materials: 2\n") != std::string::npos);
    CHECK(obj.find("# First one\ng First_one\nusemtl First_one\n") != std::string::npos);
    CHECK(obj.find("# Second/one\ng Second_one\nusemtl Second_one\n") != std::string::npos);
    CHECK(mtl.find("newmtl First_one\n# First one\n") != std::string::npos);
    CHECK_EQ(w.get_total_vertices(), static_cast<size_t>(6));
    CHECK_EQ(w.get_total_faces(), static_cast<size_t>(2));
}

TEST(ObjWriter, NothingIsWrittenWithoutAMesh)
{
    // Windows OBJExporter.export returns False when there is no mesh; the iOS writer saves nothing then.
    ScratchDir dir("obj_empty");
    ost_geometry::OBJWriter w;
    CHECK_FALSE(w.save(dir.file("e.obj")));
    w.add_mesh(Mesh(), "empty", 1.0f, 1.0f, 1.0f);
    CHECK_FALSE(w.save(dir.file("e.obj")));
    std::string text;
    CHECK_FALSE(ost_test::read_file(dir.file("e.obj"), text));
    CHECK_FALSE(ost_test::read_file(dir.file("e.mtl"), text));
}

// ---------------------------------------------------------------- EXP-12 names in the file
TEST(ObjWriter, GroupAndMaterialNamesAreSanitisedLikeWindows)
{
    // Windows (_sanitize_material_name) vs the 'g' / 'usemtl' / 'newmtl' lines of the iOS writer for every oracle name.
    ScratchDir dir("obj_names");
    ost_geometry::OBJWriter w;
    std::vector<std::string> expected;
    for (const auto &c : export_oracle::kSanitizeCases)
    {
        w.add_mesh(triangle(), c.input, 0.5f, 0.5f, 0.5f);
        expected.push_back(c.expected);
    }
    REQUIRE(w.save(dir.file("n.obj")));
    const std::string obj = read_text_lf(dir.file("n.obj"));
    const std::string mtl = read_text_lf(dir.file("n.mtl"));
    const auto g = lines_with_prefix(obj, "g ");
    const auto use = lines_with_prefix(obj, "usemtl ");
    const auto mat = lines_with_prefix(mtl, "newmtl ");
    REQUIRE_EQ(g.size(), expected.size());
    REQUIRE_EQ(use.size(), expected.size());
    REQUIRE_EQ(mat.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        SCOPED_CONTEXT("case " + std::to_string(i) + " " + expected[i]);
        CHECK_STR_EQ(g[i], expected[i]);
        CHECK_STR_EQ(use[i], expected[i]);
        CHECK_STR_EQ(mat[i], expected[i]);
    }
}

TEST(ObjWriter, NameExamplesOfThePlan)
{
    ScratchDir dir("obj_plan_names");
    ost_geometry::OBJWriter w;
    for (const char *n : {"Concrete_Wall 1/2_12345678", "5\" Slab, (Top).v2_abcdefgh", "", "{x}[y]", "1st Floor"})
        w.add_mesh(triangle(), n, 0.5f, 0.5f, 0.5f);
    REQUIRE(w.save(dir.file("p.obj")));
    const auto g = lines_with_prefix(read_text_lf(dir.file("p.obj")), "g ");
    REQUIRE_EQ(g.size(), static_cast<size_t>(5));
    CHECK_STR_EQ(g[0], "Concrete_Wall_1_2_12345678");
    CHECK_STR_EQ(g[1], "Mat_5_Slab__Top__v2_abcdefgh");
    CHECK_STR_EQ(g[2], "Unknown");
    CHECK_STR_EQ(g[3], "_x__y_");
    CHECK_STR_EQ(g[4], "Mat_1st_Floor");
}

// ---------------------------------------------------------------- EXP-12 control characters in names (Windows b9902e6)
namespace
{
    // The name of the Windows test test_injected_lines_cannot_survive_in_a_material_name, closing NUL included (an
    // explicit-length std::string: a const char * would end at the NUL).
    std::string injected_name()
    {
        std::string name = "Slab\r\nnewmtl evil\rKd 1 0 0\nv 1 2 3";
        name.push_back('\0');
        return name;
    }

    // Both Windows files for one group: the 'g', 'usemtl' and 'newmtl' lines of the name, the 'Kd' lines of the material.
    struct NameLines
    {
        std::string obj, mtl;
        std::vector<std::string> g, usemtl, newmtl, kd, obj_lines, mtl_lines;
    };

    NameLines write_one_group(const std::string &scratch, bool as_group, const std::string &name, const std::string &original)
    {
        ScratchDir dir(scratch);
        ost_geometry::OBJWriter w;
        w.set_display_mode("solid");
        float r = 0, g = 0, b = 0;
        colour_floats(255, 0, 0, r, g, b);
        const Mesh tri = triangle();
        if (as_group)
            w.add_group(name, original, r, g, b, {&tri}); // the path of Services/ExportService.mm (one group per condition)
        else
            w.add_mesh(tri, name, original, r, g, b);     // the path of add_layered_meshes
        NameLines out;
        if (!w.save(dir.file("model.obj")))
            return out;
        out.obj = read_text_lf(dir.file("model.obj"));
        out.mtl = read_text_lf(dir.file("model.mtl"));
        out.g = lines_with_prefix(out.obj, "g ");
        out.usemtl = lines_with_prefix(out.obj, "usemtl ");
        out.newmtl = lines_with_prefix(out.mtl, "newmtl ");
        out.kd = lines_with_prefix(out.mtl, "Kd ");
        out.obj_lines = split_lines(out.obj);
        out.mtl_lines = split_lines(out.mtl);
        return out;
    }
} // namespace

TEST(ObjWriter, Exp12_ControlCharactersInANameBecomeUnderscoresInGUsemtlAndNewmtl)
{
    // Windows OBJExporter writes the sanitised material name in 'newmtl', 'g' and 'usemtl'; since b9902e6 the sanitiser
    // turns every U+0000..U+001F and U+007F into '_' (it used to keep them, iOS used to drop them). The injection name
    // (CR LF, CR, LF and a NUL) therefore gives ONE of each line and nothing else: no 'newmtl evil', no 'Kd 1 0 0', no 'v 1 2 3'.
    const std::string expected = "Slab__newmtl_evil_Kd_1_0_0_v_1_2_3_";
    for (bool as_group : {true, false})
    {
        SCOPED_CONTEXT(as_group ? "add_group" : "add_mesh");
        // the comment name is plain here, so that no raw control character of the name can reach the files at all
        const NameLines f = write_one_group(as_group ? "obj_exp12_group" : "obj_exp12_mesh", as_group, injected_name(), "plain");
        REQUIRE_EQ(f.g.size(), static_cast<size_t>(1));
        REQUIRE_EQ(f.usemtl.size(), static_cast<size_t>(1));
        REQUIRE_EQ(f.newmtl.size(), static_cast<size_t>(1));
        CHECK_STR_EQ(f.g[0], expected);
        CHECK_STR_EQ(f.usemtl[0], expected);
        CHECK_STR_EQ(f.newmtl[0], expected);
        CHECK_EQ(f.kd, (std::vector<std::string>{"1.000000 0.000000 0.000000"}));
        CHECK(every_line_is_a_known_statement(f.obj_lines, kObjStatements));
        CHECK(every_line_is_a_known_statement(f.mtl_lines, kMtlStatements));
        // the OBJ holds the 3 vertices and the face of the triangle, the MTL no vertex
        CHECK_EQ(lines_with_prefix(f.obj, "v ").size(), static_cast<size_t>(3));
        CHECK_EQ(lines_with_prefix(f.obj, "f ").size(), static_cast<size_t>(1));
        CHECK_EQ(lines_with_prefix(f.mtl, "v ").size(), static_cast<size_t>(0));
        // 'newmtl' is not an OBJ statement the writer produces, and no line of either file carries a raw CR or NUL
        for (const auto &line : f.obj_lines)
            CHECK(line.find_first_of(std::string("\r\0", 2)) == std::string::npos);
        for (const auto &line : f.mtl_lines)
            CHECK(line.find_first_of(std::string("\r\0", 2)) == std::string::npos);
    }
}

TEST(ObjWriter, Exp12_EveryControlCharacterOfANameIsOneUnderscoreInTheFile)
{
    // A name 'A' + U+0000..U+001F + U+007F + 'B' (Windows test_control_characters_become_underscores) written through the
    // OBJ: 33 underscores between A and B in the 'g', 'usemtl' and 'newmtl' lines.
    std::string name = "A";
    for (int code = 0; code < 0x20; ++code)
        name += static_cast<char>(code);
    name += static_cast<char>(0x7F);
    name += "B";
    const NameLines f = write_one_group("obj_exp12_all_controls", true, name, "plain");
    const std::string expected = "A" + std::string(33, '_') + "B";
    REQUIRE_EQ(f.g.size(), static_cast<size_t>(1));
    REQUIRE_EQ(f.usemtl.size(), static_cast<size_t>(1));
    REQUIRE_EQ(f.newmtl.size(), static_cast<size_t>(1));
    CHECK_STR_EQ(f.g[0], expected);
    CHECK_STR_EQ(f.usemtl[0], expected);
    CHECK_STR_EQ(f.newmtl[0], expected);
    CHECK(every_line_is_a_known_statement(f.obj_lines, kObjStatements));
    CHECK(every_line_is_a_known_statement(f.mtl_lines, kMtlStatements));
}

// ---------------------------------------------------------------- EXP-16 line breaks in the comment lines (Windows b9902e6)
TEST(ObjWriter, Exp16_LineBreaksInAConditionNameAreWrittenAsSpacesLikeWindows)
{
    // PIN: iOS already writes CR and LF of a '# <name>' comment as one space each (OBJWriter::single_line), which is what
    // Windows does since b9902e6 (obj_exporter.py _comment_text: replace('\r', ' ') then replace('\n', ' ')). Port of
    // tests/presentation/visualization/exporters/test_obj_exporter.py OBJExporterCommentTests::
    // test_line_breaks_in_a_condition_name_are_written_as_spaces: condition name 'Slab\r\nv 9 9 9\rf 1 2 3\nend', material
    // 'red', colour #ff0000, display mode 'solid', one triangle. The Windows writer swaps the vertices to (x, z, -y) and the
    // faces to (f0, f2, f1); iOS receives the mesh already swapped (triangle()), so the lines are the same.
    ScratchDir dir("obj_exp16_windows");
    ost_geometry::OBJWriter w;
    w.set_display_mode("solid");
    float r = 0, g = 0, b = 0;
    colour_floats(255, 0, 0, r, g, b);
    const Mesh tri = triangle();
    w.add_group("red", "Slab\r\nv 9 9 9\rf 1 2 3\nend", r, g, b, {&tri});
    REQUIRE(w.save(dir.file("model.obj")));
    const std::string obj = read_text_lf(dir.file("model.obj"));
    const std::string mtl = read_text_lf(dir.file("model.mtl"));
    CHECK(obj.find('\r') == std::string::npos); // the comment holds no raw CR (Windows splitlines would break the line there)
    CHECK(mtl.find('\r') == std::string::npos);
    const auto obj_lines = split_lines(obj);
    const auto mtl_lines = split_lines(mtl);
    REQUIRE(obj_lines.size() > 5);
    REQUIRE(mtl_lines.size() > 5);
    const std::string comment = "# Slab  v 9 9 9 f 1 2 3 end";
    CHECK_STR_EQ(obj_lines[3], comment);
    CHECK_STR_EQ(mtl_lines[5], comment);
    CHECK_STR_EQ(obj_lines[4], "g red");
    CHECK_STR_EQ(obj_lines[5], "usemtl red");
    CHECK_STR_EQ(mtl_lines[4], "newmtl red");
    CHECK_EQ(lines_with_prefix(obj, "v "), (std::vector<std::string>{"0.000000 0.000000 0.000000", "1.000000 0.000000 0.000000",
                                                                      "0.000000 0.000000 -1.000000"}));
    CHECK_EQ(lines_with_prefix(obj, "f "), (std::vector<std::string>{"1 3 2"}));
    CHECK_EQ(lines_with_prefix(mtl, "Kd "), (std::vector<std::string>{"1.000000 0.000000 0.000000"}));
    CHECK(every_line_is_a_known_statement(obj_lines, kObjStatements));
    CHECK(every_line_is_a_known_statement(mtl_lines, kMtlStatements));
}

TEST(ObjWriter, Exp16_EachCrAndLfOfACommentIsOneSpaceAndNothingElseChanges)
{
    // PIN, derived from _comment_text (not a Windows test): 'A\r\nB' gives 'A  B' (one space per CR and per LF, no
    // collapsing), a lone CR and a lone LF one space each; every other character of the name stays raw in the comment, TAB,
    // 0x01, 0x0B, 0x0C, 0x7F, U+0085 and U+2028 too (Windows replaces only CR and LF); and no injected statement appears.
    struct Case
    {
        std::string name, comment;
    };
    const std::vector<Case> cases = {
        {"A\r\nB", "A  B"},
        {"A\rB", "A B"},
        {"A\nB", "A B"},
        {"A\n\r\n\rB", "A    B"},
        {"\nA", " A"},
        {"A\r", "A "},
        {"A\tB", "A\tB"},
        {"A\x01" "B", "A\x01" "B"},
        {"A\x0B" "B\x0C" "C", "A\x0B" "B\x0C" "C"},
        {"A\x7F" "B", "A\x7F" "B"},
        {"A\xC2\x85" "B\xE2\x80\xA8" "C", "A\xC2\x85" "B\xE2\x80\xA8" "C"},
        {"Slab\r\nnewmtl evil\rKd 1 0 0\nv 1 2 3", "Slab  newmtl evil Kd 1 0 0 v 1 2 3"},
    };
    ScratchDir dir("obj_exp16_cases");
    ost_geometry::OBJWriter w;
    const Mesh tri = triangle();
    for (size_t k = 0; k < cases.size(); ++k)
        w.add_group("c" + std::to_string(k), cases[k].name, 0.5f, 0.5f, 0.5f, {&tri});
    REQUIRE(w.save(dir.file("model.obj")));
    const std::string obj = read_text_lf(dir.file("model.obj"));
    const std::string mtl = read_text_lf(dir.file("model.mtl"));
    const auto obj_lines = split_lines(obj);
    const auto mtl_lines = split_lines(mtl);
    // one statement block per group and nothing else: no line was injected by a name
    CHECK_EQ(lines_with_prefix(obj, "g ").size(), cases.size());
    CHECK_EQ(lines_with_prefix(obj, "usemtl ").size(), cases.size());
    CHECK_EQ(lines_with_prefix(mtl, "newmtl ").size(), cases.size());
    CHECK_EQ(lines_with_prefix(mtl, "Kd ").size(), cases.size());
    CHECK_EQ(lines_with_prefix(obj, "v ").size(), 3 * cases.size());
    CHECK_EQ(lines_with_prefix(obj, "f ").size(), cases.size());
    CHECK(every_line_is_a_known_statement(obj_lines, kObjStatements));
    CHECK(every_line_is_a_known_statement(mtl_lines, kMtlStatements));
    CHECK(obj.find('\r') == std::string::npos);
    CHECK(mtl.find('\r') == std::string::npos);
    for (size_t k = 0; k < cases.size(); ++k)
    {
        SCOPED_CONTEXT("case " + std::to_string(k) + " " + ost_test::detail::quote(cases[k].name));
        const std::string expected = "# " + cases[k].comment;
        const std::string group = "c" + std::to_string(k);
        // OBJ: '# <comment>' directly before 'g <group>'
        const auto g = std::find(obj_lines.begin(), obj_lines.end(), "g " + group);
        REQUIRE(g != obj_lines.end() && g != obj_lines.begin());
        CHECK_STR_EQ(*(g - 1), expected);
        // MTL: '# <comment>' directly after 'newmtl <group>'
        const auto m = std::find(mtl_lines.begin(), mtl_lines.end(), "newmtl " + group);
        REQUIRE(m != mtl_lines.end() && m + 1 != mtl_lines.end());
        CHECK_STR_EQ(*(m + 1), expected);
    }
}

// ---------------------------------------------------------------- EXP-13 winding of the exported meshes
namespace
{
    // The Windows scenario of the pipeline oracle as an iOS condition / takeoff (OST coordinates, as the app stores them).
    struct ExportCase
    {
        const char *name;
        Condition condition;
        Takeoff takeoff;
    };

    Condition make_condition(const std::string &uid, ConditionType type)
    {
        Condition c;
        c.uid = uid;
        c.name = uid;
        c.condition_type = type;
        return c;
    }

    Takeoff make_takeoff(const std::string &condition_uid, std::vector<double> position, int curve = -1)
    {
        Takeoff t;
        t.uid = "t";
        t.condition_uid = condition_uid;
        t.page_uid = "p";
        t.position = std::move(position);
        t.parent_uid = "0";
        t.curve = curve;
        return t;
    }

    std::vector<ExportCase> export_cases()
    {
        std::vector<ExportCase> v;
        Condition wall = make_condition("wall", ConditionType::Linear);
        wall.height = 96.0;
        wall.thickness = 6.0;
        v.push_back({"wall", wall, make_takeoff("wall", {0.0, 0.0, 100.0, 0.0})});

        Condition circle = make_condition("circle", ConditionType::Count);
        circle.shape = ShapeId::Circle;
        circle.width = 24.0;
        circle.depth = 24.0;
        circle.height = 48.0;
        circle.display_size = 100.0;
        v.push_back({"circle", circle, make_takeoff("circle", {0.0, 0.0})});

        Condition curved = make_condition("curved", ConditionType::Linear);
        curved.height = 96.0;
        curved.thickness = 6.0;
        v.push_back({"curved_wall", curved, make_takeoff("curved", {0.0, 200.0, 100.0, 200.0, 50.0, 230.0}, 0)});

        Condition slab = make_condition("slab", ConditionType::Area);
        slab.height = 6.0;
        slab.thickness = 6.0;
        v.push_back({"slab", slab, make_takeoff("slab", {0.0, 0.0, 100.0, 0.0, 100.0, 100.0, 0.0, 100.0})});

        Condition box = make_condition("box", ConditionType::Count);
        box.shape = ShapeId::Square;
        box.width = 20.0;
        box.depth = 20.0;
        box.height = 10.0;
        box.display_size = 100.0;
        v.push_back({"box", box, make_takeoff("box", {0.0, 0.0})});
        return v;
    }
} // namespace

TEST(ObjWinding, ExportedMeshesHaveThePositiveSignedVolumeOfTheWindowsObj)
{
    // Port of exporters/adapters/obj_writer.py::test_coordinate_winding_and_multi_mesh_indices_without_mutating_input
    // (faces written as stored: every Windows mesh is outward since d1c430a; before, written as (f0, f2, f1) of the inward-stored meshes) with the signed volumes of the real Windows OBJ
    // exports of the EXP-13 plan, no negative takeoff in the project: circle width 24 height 48 +21467.487, curved wall
    // [0,200,100,200,50,230] curve 0 height 96 thickness 6 +70478.256, wall [0,0,100,0] +57600, slab +60000. The iOS
    // writer writes the faces as the mesh stores them, so the meshes themselves must be outward-facing.
    size_t checked = 0;
    for (const auto &c : export_cases())
    {
        const export_oracle::PipelineScenario *oracle = nullptr;
        for (const auto &p : export_oracle::kPipelineScenarios)
            if (std::string(p.name) == c.name)
                oracle = &p;
        REQUIRE(oracle != nullptr);
        SCOPED_CONTEXT(c.name);
        auto r = ost_meshing::create_mesh_for_takeoff(c.takeoff, c.condition, 1.0);
        REQUIRE(r.valid);
        ScratchDir dir(std::string("winding_") + c.name);
        ost_geometry::OBJWriter w;
        w.add_mesh(r.mesh, c.name, r.color_r, r.color_g, r.color_b);
        REQUIRE(w.save(dir.file("w.obj")));
        const std::string obj = read_text_lf(dir.file("w.obj"));
        const ObjData d = parse_obj(obj);
        CHECK_EQ(static_cast<int>(d.vertices.size()), oracle->obj_vertices);
        CHECK_EQ(static_cast<int>(d.faces.size()), oracle->obj_faces);
        const double volume = signed_volume(d.vertices, d.faces);
        CHECK(volume > 0.0); // outward-facing: the Windows OBJ has a positive volume for every kind of mesh
        CHECK(closed_and_consistently_oriented(d.faces)); // every face, not only the sum (a flat cap adds no volume)
        CHECK_NEAR(volume, oracle->obj_volume, 1e-9 * std::max(1.0, oracle->obj_volume));
        ++checked;
    }
    CHECK_EQ(checked, static_cast<size_t>(5));
}

TEST(ObjWinding, GeneratorsOfTheCylinderAndTheCurvedWallAreOutwardFacing)
{
    // MSH-M02 alias plan: generate_curved_linear_mesh(0,0,100,0,50,30 in iOS axes, height 3, thickness 2) must be
    // +734.148 like the Windows OBJ (iOS had -734.148); generate_cylinder_mesh(width 4, height 2) +24.847 (was -24.847).
    // (The Windows numbers are the volumes of the OBJ text, whose vertices carry 6 decimals, so the iOS volume is taken
    // from the OBJ the writer produces as well.)
    const double kCurved = export_oracle::kAliasVolumes[0].obj_volume;
    const double kCylinder = export_oracle::kAliasVolumes[1].obj_volume;
    ScratchDir dir("winding_alias");
    auto obj_volume_of = [&](const Mesh &mesh)
    {
        ost_geometry::OBJWriter w;
        w.add_mesh(mesh, "m", 0.5f, 0.5f, 0.5f);
        REQUIRE(w.save(dir.file("a.obj")));
        return obj_signed_volume(read_text_lf(dir.file("a.obj")));
    };
    Mesh curved = ost_meshing::generate_curved_linear_mesh(0.0, 0.0, 100.0, 0.0, 50.0, 30.0, 3.0, 2.0, 0.0);
    CHECK_EQ(static_cast<int>(curved.vertices.size()), export_oracle::kAliasVolumes[0].obj_vertices);
    CHECK_EQ(static_cast<int>(curved.faces.size()), export_oracle::kAliasVolumes[0].obj_faces);
    CHECK(signed_volume(curved) > 0.0);
    CHECK(closed_and_consistently_oriented(curved.faces));
    CHECK_NEAR(obj_volume_of(curved), kCurved, 1e-9 * kCurved);
    Mesh cylinder = ost_meshing::generate_cylinder_mesh(0.0, 0.0, 4.0, 2.0, 0.0);
    CHECK_EQ(static_cast<int>(cylinder.vertices.size()), export_oracle::kAliasVolumes[1].obj_vertices);
    CHECK_EQ(static_cast<int>(cylinder.faces.size()), export_oracle::kAliasVolumes[1].obj_faces);
    CHECK(signed_volume(cylinder) > 0.0);
    CHECK(closed_and_consistently_oriented(cylinder.faces)); // the bottom cap lies in y = 0 and adds no volume
    CHECK_NEAR(obj_volume_of(cylinder), kCylinder, 1e-9 * kCylinder);
}

TEST(ObjWinding, EveryCurvedWallAndCylinderVariantIsOutwardFacing)
{
    // Both arc senses, a sloped wall with a z offset, a rotated cylinder and a drop stub: the sign never depends on the
    // direction the arc bends or on the slope.
    for (double cz : {30.0, -30.0, 7.5})
        for (double rise : {0.0, 1.0, -1.0})
        {
            SCOPED_CONTEXT("control z " + std::to_string(cz) + " rise " + std::to_string(rise));
            Mesh wall = ost_meshing::generate_curved_linear_mesh(0.0, 0.0, 100.0, 0.0, 50.0, cz, 96.0, 6.0, 2.0, rise,
                                                                 rise == 0.0 ? 0.0 : 12.0);
            REQUIRE(wall.is_valid());
            CHECK(signed_volume(wall) > 0.0);
            CHECK(closed_and_consistently_oriented(wall.faces));
        }
    for (double rotation : {0.0, 0.3, -1.1})
        for (int segments : {3, 12, 24, 37})
        {
            SCOPED_CONTEXT("rotation " + std::to_string(rotation) + " segments " + std::to_string(segments));
            Mesh cylinder = ost_meshing::generate_cylinder_mesh(5.0, -3.0, 10.0, 7.0, 1.5, rotation, segments);
            REQUIRE(cylinder.is_valid());
            CHECK(signed_volume(cylinder) > 0.0);
            CHECK(closed_and_consistently_oriented(cylinder.faces));
        }
    Mesh stub = ost_meshing::generate_curved_drop_stub_mesh(0.0, 0.0, 100.0, 0.0, 50.0, 30.0, 6.0, 12.0, 0.0, 96.0);
    REQUIRE(stub.is_valid());
    CHECK(signed_volume(stub) > 0.0);
    CHECK(closed_and_consistently_oriented(stub.faces));
}

#if OST_HAVE_BOOLEAN
// ---------------------------------------------------------------- EXP-13: the boolean difference keeps every result outward-facing
TEST(ObjWinding, BooleanDifferenceResultsAreOutwardFacing)
{
    // Scope: this test runs ost_geometry::boolean_difference_batch (the Manifold difference that the boolean pass calls for
    // a positive with a negative of its type) on generator meshes, so it pins that the difference itself never returns an
    // inward-facing mesh. It runs neither the pass nor an export. The pass is pinned by
    // BooleanRule.Winding_ThePassNeverReversesTheFacesOfAPositive and the meshes that leave the 3D view, the HTML export and
    // the OBJ / DXF groups by ExportRule.Winding_EveryMeshThatLeavesTheViewAndBothExportsIsOutwardFacing (with the stand-in
    // difference) and ExportRuleManifold.Winding_EveryMeshThatLeavesTheViewAndBothExportsIsOutwardFacingWithManifold (with
    // Manifold).
    //
    // Windows (d1c430a, outward_faces in domain/mesh.py:62) made every shape outward: the generators, the boolean pass and the
    // OBJ writer no longer flip anything. Before d1c430a (since b9902e6) apply_boolean_operations flipped (f0, f2, f1) the native
    // result of a positive that MAY be cut (its bounding box strictly overlaps the bounding box of a negative of the same
    // condition type; empty positives count as may-be-cut), and the Windows OBJ writer swapped every face again, so one Windows
    // OBJ could mix both windings (the untouched wall of the EXP-13 plan came out with the volume -57600 instead of +57600, and
    // -60000 for the slab). iOS already stored every exported mesh outward-facing, cut or untouched, and so does the volume:
    // winding parity, nothing was ported (EXP-13 / EXP-M02 / GEO-15 / MSH-21). EXP-17 (the order of the triangles after the
    // Windows bounding-box shortcut) stays a difference. Nothing in iOS depends on a flip (no renderer culls faces, the shaders
    // light both sides, and the writers write the faces as stored).
    Mesh cylinder = ost_meshing::generate_cylinder_mesh(0.0, 0.0, 24.0, 48.0, 0.0);
    const double whole = signed_volume(cylinder);
    REQUIRE(whole > 0.0);
    // a negative far away: the positive is untouched
    Mesh far_away = ost_meshing::generate_count_mesh(500.0, 500.0, 40.0, 40.0, 10.0, 0.0);
    auto untouched = ost_geometry::boolean_difference_batch(cylinder, {far_away});
    REQUIRE(untouched.has_value());
    CHECK(signed_volume(*untouched) > 0.0);
    CHECK_NEAR(signed_volume(*untouched), whole, 1e-5 * whole);
    // a negative block covering the bottom 10 of the 48: a cut piece
    Mesh block = ost_meshing::generate_count_mesh(0.0, 0.0, 40.0, 40.0, 10.0, 0.0);
    auto cut = ost_geometry::boolean_difference_batch(cylinder, {block});
    REQUIRE(cut.has_value());
    CHECK(signed_volume(*cut) > 0.0);
    CHECK_NEAR(signed_volume(*cut), whole * 38.0 / 48.0, 1e-5 * whole);
    // the curved wall (it was inside-out before EXP-13) behaves the same way
    Mesh curved = ost_meshing::generate_curved_linear_mesh(0.0, 0.0, 100.0, 0.0, 50.0, 30.0, 96.0, 6.0, 0.0);
    const double curved_whole = signed_volume(curved);
    REQUIRE(curved_whole > 0.0);
    auto curved_untouched = ost_geometry::boolean_difference_batch(curved, {far_away});
    REQUIRE(curved_untouched.has_value());
    CHECK(signed_volume(*curved_untouched) > 0.0);
    CHECK_NEAR(signed_volume(*curved_untouched), curved_whole, 1e-5 * curved_whole);
}
#endif
