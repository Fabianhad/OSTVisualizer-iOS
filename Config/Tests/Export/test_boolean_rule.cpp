// The shared boolean pass (Core/Meshing/BooleanPass.*): MSH-16 / EXP-05 (a negative cuts only positives of its own condition
// type), MSH-15 (a positive that is consumed completely is dropped), RB-01 (condition_type is mandatory, checked before any
// boolean operation) and the Windows fallback when the boolean operation fails.
// Windows oracle: presentation/visualization/core/boolean_operations.py apply_boolean_operations and
// cpp_extensions/src/module_geometry.cpp apply_boolean_operations_impl, run read-only (ExportRuleOracleData.hpp unit_cases).
// The rule itself is tested with a stand-in for the boolean difference (the pass takes it as a parameter); the real
// Manifold cases are under '#if OST_HAVE_BOOLEAN' (configure with -DOST_TEST_BOOLEAN=ON).
// Since Windows b9902e6 apply_boolean_operations also returns the ORIGINAL mesh of a positive whose bounding box does not
// strictly overlap the bounding box of any negative of its type (touching counts as apart) and reverses the faces of the
// native result only for the positives that may be cut. iOS has no such bounding-box pre-check and never reverses a face
// (every mesh stays outward by decision, see Winding_ThePassNeverReversesTheFacesOfAPositive): every positive of a type
// that has a negative reaches the difference, so the tests below, whose boxes lie 100 apart, rely on that.
#include "ExportRuleTestSupport.hpp"
#include "ExportRuleOracleData.hpp"

#include "Domain/CoreLog.hpp"
#include "Geometry/boolean_ops.hpp"

#include <cmath>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using namespace rule_test;
using ost_meshing::BooleanDifferenceFn;
using ost_meshing::BooleanPassMesh;
using ost_meshing::BooleanPassResult;

namespace
{
    // A distinguishable mesh: a box whose x origin is `id` (1 apart per id).
    Mesh tagged_box(double id)
    {
        return make_box(id * 100.0, 0.0, 0.0, id * 100.0 + 10.0, 10.0, 10.0);
    }

    double tag_of(const Mesh &m) { return std::round(m.vertices[0][0] / 100.0); }

    BooleanPassMesh entry(double id, bool negative, int type, const std::string &uid)
    {
        BooleanPassMesh e;
        e.mesh = tagged_box(id);
        e.is_negative = negative;
        e.condition_type = type;
        e.takeoff_uid = uid;
        return e;
    }

    // Records every call and lets the test decide the answer.
    struct Recorder
    {
        struct Call
        {
            double positive;
            std::vector<double> negatives;
        };
        std::vector<Call> calls;
        std::function<std::optional<Mesh>(const Mesh &, const std::vector<Mesh> &)> answer;

        BooleanDifferenceFn fn()
        {
            return [this](const Mesh &p, const std::vector<Mesh> &n) -> std::optional<Mesh>
            {
                Call c;
                c.positive = tag_of(p);
                for (const auto &m : n)
                    c.negatives.push_back(tag_of(m));
                calls.push_back(c);
                if (answer)
                    return answer(p, n);
                Mesh cut = p; // "cut": the positive moved up by 1 so the result is recognisable
                for (auto &v : cut.vertices)
                    v[1] += 1.0;
                return cut;
            };
        }
    };

    std::vector<size_t> sources(const std::vector<BooleanPassResult> &r)
    {
        std::vector<size_t> out;
        for (const auto &x : r)
            out.push_back(x.source_index);
        return out;
    }

    // ost_log records of a scope.
    struct LogCapture
    {
        struct Record
        {
            ost_log::Level level;
            std::string message;
        };
        std::vector<Record> records;
        LogCapture()
        {
            ost_log::set_sink([this](ost_log::Level l, const std::string &m, const std::string &)
                              { records.push_back({l, m}); });
        }
        ~LogCapture() { ost_log::set_sink({}); }
    };
} // namespace

// ---------------------------------------------------------------- the rule (stand-in difference)
TEST(BooleanRule, ANegativeCutsOnlyThePositivesOfItsOwnConditionType)
{
    // Port of tests/presentation/visualization/exporters/test_obj_exporter.py::OBJExporterBooleanRuleTests::
    //   test_negative_of_another_condition_type_does_not_cut_the_positive and ...::test_negative_cuts_the_positive_of_the_
    //   same_condition_type (and the same pair of the DXF and HTML tests); native negatives_by_type
    //   (module_geometry.cpp:25,76-79).
    // Two types: P1 and P2 are type 2, P3 is type 3; N1 and N3 type 2, N2 type 3 (input order).
    Recorder rec;
    auto results = ost_meshing::apply_boolean_operations(
        {entry(1, false, 2, "p1"), entry(2, false, 3, "p3"), entry(3, true, 2, "n1"), entry(4, true, 3, "n2"),
         entry(5, true, 2, "n3"), entry(6, false, 2, "p2")},
        rec.fn());
    // the positives in input order (negatives are never output)
    REQUIRE_EQ(results.size(), static_cast<size_t>(3));
    CHECK_EQ(sources(results), (std::vector<size_t>{0, 1, 5}));
    REQUIRE_EQ(rec.calls.size(), static_cast<size_t>(3));
    // every positive got exactly the negatives of ITS type, in input order
    CHECK_EQ(rec.calls[0].positive, 1.0);
    CHECK_EQ(rec.calls[0].negatives, (std::vector<double>{3.0, 5.0}));
    CHECK_EQ(rec.calls[1].positive, 2.0);
    CHECK_EQ(rec.calls[1].negatives, (std::vector<double>{4.0}));
    CHECK_EQ(rec.calls[2].positive, 6.0);
    CHECK_EQ(rec.calls[2].negatives, (std::vector<double>{3.0, 5.0}));
}

TEST(BooleanRule, APositiveWithoutANegativeOfItsTypeIsUnchangedAndNeverSentToTheDifference)
{
    // Port of ...::test_negative_of_another_condition_type_does_not_cut_the_positive: positive type 2, negative type 3.
    // module_geometry.cpp:95-103 (untouched positives) and core/test_boolean_operations.py::BooleanRuleContractTests::
    //   test_typed_meshes_reach_the_native_rule (positive condition_type 2, negative condition_type 3).
    Recorder rec;
    auto results = ost_meshing::apply_boolean_operations({entry(1, false, 2, "p"), entry(2, true, 3, "n")}, rec.fn());
    REQUIRE_EQ(results.size(), static_cast<size_t>(1));
    CHECK(rec.calls.empty());
    CHECK_EQ(results[0].source_index, static_cast<size_t>(0));
    const Mesh expected = tagged_box(1);
    CHECK(results[0].mesh.vertices == expected.vertices);
    CHECK(results[0].mesh.faces == expected.faces);
}

TEST(BooleanRule, ACutPositiveIsTheDifferenceResultRepairedOfDegenerateFaces)
{
    // module_geometry.cpp:80-93: the valid difference replaces the positive after remove_degenerate_faces.
    Recorder rec;
    rec.answer = [](const Mesh &p, const std::vector<Mesh> &) -> std::optional<Mesh>
    {
        Mesh cut = p;
        cut.faces.push_back({0, 0, 1}); // a face with a repeated vertex
        return cut;
    };
    auto results = ost_meshing::apply_boolean_operations({entry(1, false, 2, "p"), entry(2, true, 2, "n")}, rec.fn());
    REQUIRE_EQ(results.size(), static_cast<size_t>(1));
    CHECK_EQ(results[0].mesh.faces.size(), static_cast<size_t>(12));
    CHECK(results[0].mesh.vertices == tagged_box(1).vertices);
}

TEST(BooleanRule, Winding_ThePassNeverReversesTheFacesOfAPositive)
{
    // PIN of a deliberate difference. Windows (b9902e6) reverses (f0, f2, f1) the native result of every positive that may be
    // cut (bounding box strictly overlapping a same-type negative) and returns the original mesh of the others; iOS keeps
    // every mesh outward-facing by decision (GEO-15 / EXP-13 / EXP-M02 / EXP-17 / MSH-21), so the faces that leave the pass
    // are exactly the faces that went in (untouched positive) or that the difference returned (cut positive), in the same
    // order and winding, and the volume stays positive. Nothing downstream consumes a flip either.
    const Mesh box = tagged_box(1);
    REQUIRE(export_test::signed_volume(box) > 0.0);
    auto only = [&](std::vector<BooleanPassMesh> input, Recorder &rec)
    {
        auto results = ost_meshing::apply_boolean_operations(std::move(input), rec.fn());
        REQUIRE_EQ(results.size(), static_cast<size_t>(1));
        CHECK(results[0].mesh.faces == box.faces);
        CHECK(export_test::signed_volume(results[0].mesh) > 0.0);
        return results;
    };
    {
        SCOPED_CONTEXT("no negative");
        Recorder rec;
        only({entry(1, false, 2, "p")}, rec);
        CHECK(rec.calls.empty());
    }
    {
        SCOPED_CONTEXT("a negative of another type");
        Recorder rec;
        only({entry(1, false, 2, "p"), entry(2, true, 3, "n")}, rec);
        CHECK(rec.calls.empty());
    }
    {
        SCOPED_CONTEXT("a same-type negative whose box does not touch the positive (Windows returns the original mesh)");
        Recorder rec;
        const auto results = only({entry(1, false, 2, "p"), entry(2, true, 2, "n")}, rec);
        CHECK_EQ(rec.calls.size(), static_cast<size_t>(1)); // iOS has no bounding-box pre-check: the difference ran
        CHECK(results[0].mesh.vertices != box.vertices);    // and its (here: shifted) result replaced the positive
    }
    {
        SCOPED_CONTEXT("a same-type negative that overlaps the positive");
        Recorder rec;
        BooleanPassMesh overlapping = entry(1, true, 2, "n");
        overlapping.mesh = make_box(5.0, 0.0, 0.0, 15.0, 10.0, 10.0);
        only({entry(1, false, 2, "p"), overlapping}, rec);
        CHECK_EQ(rec.calls.size(), static_cast<size_t>(1));
    }
}

TEST(BooleanRule, AFullyConsumedPositiveIsDropped)
{
    // Port of tests/presentation/visualization/exporters/test_obj_exporter.py::OBJExporterBooleanRuleTests::
    //   test_fully_consumed_positive_is_dropped (also the DXF and HTML tests); module_geometry.cpp:80-93 has no else.
    // The difference has no value (nothing is left) or a mesh without faces: the positive is dropped, in input order the
    // other positives stay.
    for (int variant = 0; variant < 2; ++variant)
    {
        SCOPED_CONTEXT("variant " + std::to_string(variant));
        Recorder rec;
        rec.answer = [variant](const Mesh &p, const std::vector<Mesh> &) -> std::optional<Mesh>
        {
            if (tag_of(p) != 1.0)
                return p; // the others are cut to themselves
            if (variant == 0)
                return std::nullopt;
            Mesh empty; // vertices without faces: not valid
            empty.vertices = p.vertices;
            return empty;
        };
        auto results = ost_meshing::apply_boolean_operations(
            {entry(1, false, 2, "consumed"), entry(2, false, 2, "survivor"), entry(3, true, 2, "n")}, rec.fn());
        REQUIRE_EQ(results.size(), static_cast<size_t>(1));
        CHECK_EQ(results[0].source_index, static_cast<size_t>(1));
        CHECK_EQ(rec.calls.size(), static_cast<size_t>(2));
    }
}

TEST(BooleanRule, WithoutNegativesThePositivesComeOutUnchangedInOrder)
{
    // core/boolean_operations.py: negative_count == 0 returns the positives before the native call.
    Recorder rec;
    auto results = ost_meshing::apply_boolean_operations(
        {entry(1, false, 2, "a"), entry(2, false, 3, "b"), entry(3, false, 0, "c")}, rec.fn());
    CHECK(rec.calls.empty());
    CHECK_EQ(sources(results), (std::vector<size_t>{0, 1, 2}));
}

TEST(BooleanRule, NegativesAreNeverOutput)
{
    // Port of ...::test_negative_only_export_fails_without_writing (the pass side: nothing comes out) and
    // core/boolean_operations.py: only positives are returned.
    Recorder rec;
    CHECK(ost_meshing::apply_boolean_operations({entry(1, true, 2, "n1"), entry(2, true, 3, "n2")}, rec.fn()).empty());
    CHECK(rec.calls.empty());
    CHECK(ost_meshing::apply_boolean_operations({}, rec.fn()).empty());
}

TEST(BooleanRule, AFailingBooleanOperationReturnsEveryPositiveUnchangedAndLogsAnError)
{
    // core/boolean_operations.py: except Exception: logger.error("Boolean operations failed; returning positive meshes
    // unchanged: %s", e) and the positives are returned as they are (nothing is dropped, nothing is cut).
    LogCapture log;
    auto results = ost_meshing::apply_boolean_operations(
        {entry(1, false, 2, "a"), entry(2, true, 2, "n"), entry(3, false, 3, "b")},
        [](const Mesh &, const std::vector<Mesh> &) -> std::optional<Mesh>
        { throw std::runtime_error("manifold failed"); });
    CHECK_EQ(sources(results), (std::vector<size_t>{0, 2}));
    CHECK(results[0].mesh.vertices == tagged_box(1).vertices);
    // Returned "as they are" includes the winding: the fallback never reverses the faces of a positive (the iOS decision
    // EXP-13 / EXP-M02: every mesh stays outward-facing; mutation check of 2026-11-20 found this path unpinned).
    REQUIRE_EQ(results.size(), static_cast<size_t>(2));
    CHECK(results[0].mesh.faces == tagged_box(1).faces);
    CHECK(results[1].mesh.faces == tagged_box(3).faces);
    CHECK(export_test::signed_volume(results[0].mesh) > 0.0);
    REQUIRE_EQ(log.records.size(), static_cast<size_t>(1));
    CHECK(log.records[0].level == ost_log::Level::Error);
    CHECK_STR_EQ(log.records[0].message, "Boolean operations failed; returning positive meshes unchanged: manifold failed");
}

TEST(BooleanRule, TheConditionTypeIsCheckedBeforeAnyBooleanOperation)
{
    // Port of core/test_boolean_operations.py::BooleanRuleContractTests::test_mesh_without_condition_type_is_rejected_
    //   naming_the_takeoff (the native rule is never reached): [typed positive 'a', untyped negative 'b'] -> takeoff b.
    Recorder rec;
    BooleanPassMesh untyped = entry(2, true, 0, "b");
    untyped.condition_type.reset();
    std::string what;
    bool threw = false;
    try
    {
        ost_meshing::apply_boolean_operations({entry(1, false, 2, "a"), untyped}, rec.fn());
    }
    catch (const std::invalid_argument &e)
    {
        threw = true;
        what = e.what();
    }
    CHECK(threw);
    CHECK_STR_EQ(what, "Boolean operations need condition_type for takeoff b");
    CHECK(rec.calls.empty());
}

TEST(BooleanRule, ConditionTypeZeroIsARealType)
{
    // module_geometry.cpp reads the type from the metadata; 0 (linear) is a type like any other (no default).
    Recorder rec;
    auto results = ost_meshing::apply_boolean_operations({entry(1, false, 0, "p"), entry(2, true, 0, "n")}, rec.fn());
    REQUIRE_EQ(results.size(), static_cast<size_t>(1));
    REQUIRE_EQ(rec.calls.size(), static_cast<size_t>(1));
    CHECK_EQ(rec.calls[0].negatives, (std::vector<double>{2.0}));
}

#if OST_HAVE_BOOLEAN
// ---------------------------------------------------------------- real Manifold
TEST(BooleanRuleManifold, UnitBoxCasesOfTheWindowsPass)
{
    // MSH-16 test plan: positive box (0,0,0)-(10,10,10) type 1, run on Windows with apply_boolean_operations
    // (ExportRuleOracleData.hpp unit_cases): half cut by a type-0 negative -> unchanged (|vol| 1000); by a type-1 negative
    // -> 8 vertices, 12 faces, |vol| 500; fully consumed (same type) -> nothing; a covering negative of another type ->
    // unchanged; a negative only -> nothing.
    auto positive = []() { return make_box(0, 0, 0, 10, 10, 10); };
    struct Neg
    {
        const char *name;
        Mesh mesh;
        int type;
        bool with_positive;
    };
    const std::vector<Neg> inputs = {
        {"half_other_type", make_box(5, -5, -5, 15, 15, 15), 0, true},
        {"half_same_type", make_box(5, -5, -5, 15, 15, 15), 1, true},
        {"consumed_same_type", make_box(-5, -5, -5, 15, 15, 15), 1, true},
        {"consumed_cover_other_type", make_box(-5, -5, -5, 15, 15, 15), 0, true},
        {"negative_only", make_box(-5, -5, -5, 15, 15, 15), 1, false}};
    for (const auto &c : export_rule_oracle::unit_cases())
    {
        SCOPED_CONTEXT(c.name);
        const Neg *in = nullptr;
        for (const auto &i : inputs)
            if (std::string(i.name) == c.name)
                in = &i;
        REQUIRE(in != nullptr);
        std::vector<BooleanPassMesh> batch;
        if (in->with_positive)
        {
            BooleanPassMesh p;
            p.mesh = positive();
            p.condition_type = 1;
            p.takeoff_uid = "p";
            batch.push_back(std::move(p));
        }
        BooleanPassMesh n;
        n.mesh = in->mesh;
        n.is_negative = true;
        n.condition_type = in->type;
        n.takeoff_uid = "n";
        batch.push_back(std::move(n));
        auto results = ost_meshing::apply_boolean_operations(std::move(batch));
        REQUIRE_EQ(results.size(), c.results.size());
        for (size_t i = 0; i < results.size(); ++i)
        {
            CHECK_EQ(results[i].mesh.vertices.size(), static_cast<size_t>(c.results[i].vertices));
            CHECK_EQ(results[i].mesh.faces.size(), static_cast<size_t>(c.results[i].faces));
            CHECK_NEAR(std::fabs(export_test::signed_volume(results[i].mesh)), c.results[i].volume, 1e-9 * 1000.0);
        }
    }
}
#endif
