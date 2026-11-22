// Drop-stub union failure (MSH-19b, RB-04) and the Core log hook it reports through.
// Windows oracle: meshing/mesh_factory.py _union_with_drop_stub (straight call site and curved call site):
//   try: combined = boolean_union(mesh, stub, mesh.metadata)
//   except Exception: logger.warning("Drop stub union raised for takeoff %s; keeping the wall mesh", uid, exc_info=True); return mesh
//   if combined is None or not combined.faces: logger.warning("Drop stub union failed for takeoff %s; keeping the wall mesh", uid); return mesh
//   return combined
// Windows has no real failing union to provoke (the native union only throws or returns None on Manifold errors), so its
// tests patch mesh_factory.boolean_union. The iOS equivalent is the DropStubUnionFn parameter of union_with_drop_stub
// (the function create_mesh_for_takeoff calls at both call sites, whose default is the real union). The warning goes
// through ost_log (CoreLog.hpp); the tests read it through a collecting sink.
#include "MeshingTestUtil.hpp"

#include "Domain/CoreLog.hpp"

#include <string>
#include <vector>

using namespace mesh_test;

namespace
{
    struct LogRecord
    {
        ost_log::Level level;
        std::string message;
        std::string exception_text;
    };

    // Collects every ost_log record while alive; restores the default sink afterwards.
    class LogCapture
    {
    public:
        LogCapture()
        {
            ost_log::set_sink([this](ost_log::Level level, const std::string &message, const std::string &exception_text)
                              { records.push_back({level, message, exception_text}); });
        }
        ~LogCapture() { ost_log::set_sink({}); }
        LogCapture(const LogCapture &) = delete;
        LogCapture &operator=(const LogCapture &) = delete;
        std::vector<LogRecord> records;
    };

    bool contains(const std::string &text, const std::string &part)
    {
        return text.find(part) != std::string::npos;
    }

    // Port of tests/presentation/visualization/meshing/test_mesh_factory.py::MeshFactoryDropStubUnionTests helpers:
    // Condition(uid='wall', Linear, height 10.0, thickness 2.0, drop_run, drop_value 4.0) and the same condition without
    // a drop (the reference); takeoffs wall-straight [0,0,100,0] and wall-curved [0,0,100,0,50,20] curve 0.
    Condition wall_condition(bool with_drop)
    {
        Condition c = make_condition("wall", ConditionType::Linear);
        c.height = 10.0;
        c.thickness = 2.0;
        c.drop_run = with_drop;
        c.drop_value = with_drop ? 4.0 : 0.0;
        return c;
    }

    struct WallCase
    {
        std::string shape;
        Takeoff takeoff;
    };

    std::vector<WallCase> wall_cases()
    {
        Takeoff straight = make_takeoff("wall-straight", "wall", "p", {0, 0, 100, 0});
        Takeoff curved = make_takeoff("wall-curved", "wall", "p", {0, 0, 100, 0, 50, 20});
        curved.curve = 0;
        return {{"straight", straight}, {"curved", curved}};
    }

    // The wall as the factory builds it without a drop (the Windows _plain_wall) and the stub the factory unites it with.
    Mesh plain_wall(const Takeoff &takeoff)
    {
        auto r = ost_meshing::create_mesh_for_takeoff(takeoff, wall_condition(false), 1.0);
        REQUIRE(r.valid);
        return r.mesh;
    }

    Mesh stub_of(const Takeoff &takeoff)
    {
        // The iOS frame of create_linear: (x, z) = (-x_ost, -y_ost); thickness 2, drop 4, y offset 0, height 10.
        const auto &p = takeoff.position;
        if (takeoff.curve >= 0)
            return ost_meshing::generate_curved_drop_stub_mesh(-p[0], -p[1], -p[2], -p[3], -p[4], -p[5], 2.0, 4.0, 0.0,
                                                               10.0);
        return ost_meshing::generate_drop_stub_mesh(-p[0], -p[1], -p[2], -p[3], 2.0, 4.0, 0.0, 10.0);
    }

    Mesh three_vertex_triangle()
    {
        Mesh m;
        m.vertices = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
        m.faces = {{0, 1, 2}};
        return m;
    }
} // namespace

// ---------------------------------------------------------------- MSH-19b / RB-04
TEST(DropStubUnionFailure, FailedOrEmptyUnionKeepsThePlainWallAndWarnsWithTheUid)
{
    // Port of tests/presentation/visualization/meshing/test_mesh_factory.py::MeshFactoryDropStubUnionTests::
    //   test_failed_or_empty_union_keeps_the_plain_wall_and_warns_with_the_uid
    // (straight / curved x returns None / raises / returns empty), plus the iOS-only failure shapes: a union with
    // vertices but no faces (Windows `not combined.faces`) and a thrown object that is not a std::exception.
    struct Failure
    {
        std::string name;
        ost_meshing::DropStubUnionFn fn;
        std::string exception_text; // expected exc_info text, empty when no exception
    };
    const std::vector<Failure> failures{
        {"returns None", [](const Mesh &, const Mesh &) -> std::optional<Mesh>
         { return std::nullopt; },
         ""},
        {"raises", [](const Mesh &, const Mesh &) -> std::optional<Mesh>
         { throw std::runtime_error("manifold failed"); },
         "manifold failed"},
        {"returns empty", [](const Mesh &, const Mesh &) -> std::optional<Mesh>
         { return Mesh(); },
         ""},
        {"returns vertices without faces", [](const Mesh &, const Mesh &) -> std::optional<Mesh>
         {
             Mesh m;
             m.vertices = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
             return m;
         },
         ""},
    };
    for (const auto &wc : wall_cases())
    {
        const Mesh plain = plain_wall(wc.takeoff);
        REQUIRE(!plain.faces.empty());
        const Mesh stub = stub_of(wc.takeoff);
        REQUIRE(!stub.faces.empty());
        for (const auto &failure : failures)
        {
            SCOPED_CONTEXT(wc.shape + " / " + failure.name);
            LogCapture log;
            int calls = 0;
            auto counting = [&](const Mesh &w, const Mesh &s)
            {
                ++calls;
                return failure.fn(w, s);
            };
            Mesh result = ost_meshing::union_with_drop_stub(wc.takeoff, plain, stub, counting);
            CHECK_EQ(calls, 1);
            CHECK(result.vertices == plain.vertices);
            CHECK(result.faces == plain.faces);
            REQUIRE_EQ(log.records.size(), static_cast<size_t>(1));
            CHECK(log.records[0].level == ost_log::Level::Warning);
            CHECK(contains(log.records[0].message, wc.takeoff.uid));
            CHECK(contains(log.records[0].message, "keeping the wall mesh"));
            CHECK_STR_EQ(log.records[0].exception_text, failure.exception_text);
            // Windows: "raised" (exc_info) only when the union threw, "failed" for None / no faces.
            CHECK_EQ(contains(log.records[0].message, "Drop stub union raised for takeoff"), !failure.exception_text.empty());
            CHECK_EQ(contains(log.records[0].message, "Drop stub union failed for takeoff"), failure.exception_text.empty());
        }
    }
}

TEST(DropStubUnionFailure, ThrownObjectThatIsNotAStdExceptionIsAFailureToo)
{
    // Windows `except Exception` catches every error; a C++ throw of a non-std::exception type must not escape either.
    for (const auto &wc : wall_cases())
    {
        SCOPED_CONTEXT(wc.shape);
        const Mesh plain = plain_wall(wc.takeoff);
        const Mesh stub = stub_of(wc.takeoff);
        LogCapture log;
        Mesh result = ost_meshing::union_with_drop_stub(
            wc.takeoff, plain, stub, [](const Mesh &, const Mesh &) -> std::optional<Mesh>
            { throw 42; });
        CHECK(result.vertices == plain.vertices);
        CHECK(result.faces == plain.faces);
        REQUIRE_EQ(log.records.size(), static_cast<size_t>(1));
        CHECK(log.records[0].level == ost_log::Level::Warning);
        CHECK(contains(log.records[0].message, wc.takeoff.uid));
        CHECK(contains(log.records[0].message, "Drop stub union raised for takeoff"));
    }
}

TEST(DropStubUnionFailure, SuccessfulUnionIsReturnedUnchangedWithoutAWarning)
{
    // Port of tests/presentation/visualization/meshing/test_mesh_factory.py::MeshFactoryDropStubUnionTests::
    //   test_successful_union_returns_the_combined_mesh_without_warning
    // The union receives the plain wall and a stub with faces, its result is returned as it is (a 3-vertex 1-face mesh
    // with identical vertices: not touched by any repair) and nothing is logged.
    for (const auto &wc : wall_cases())
    {
        SCOPED_CONTEXT(wc.shape);
        const Mesh plain = plain_wall(wc.takeoff);
        const Mesh stub = stub_of(wc.takeoff);
        const Mesh combined = three_vertex_triangle();
        LogCapture log;
        int calls = 0;
        Mesh result = ost_meshing::union_with_drop_stub(
            wc.takeoff, plain, stub,
            [&](const Mesh &w, const Mesh &s) -> std::optional<Mesh>
            {
                ++calls;
                CHECK(w.vertices == plain.vertices);
                CHECK(w.faces == plain.faces);
                CHECK(!s.faces.empty());
                return combined;
            });
        CHECK_EQ(calls, 1);
        CHECK(result.vertices == combined.vertices);
        CHECK(result.faces == combined.faces);
        CHECK(log.records.empty());
    }
}

#if !OST_HAVE_BOOLEAN
TEST(DropStubUnionFailure, FactoryKeepsThePlainWallWhenTheRealUnionThrows)
{
    // Both call sites of create_mesh_for_takeoff (straight and curved) go through union_with_drop_stub. Without Manifold
    // (OST_TEST_BOOLEAN=OFF) the host stand-in of boolean_union throws, which is the Windows "raises" case: the factory
    // returns the same mesh as the wall without a drop, valid, with exactly one warning naming the takeoff.
    for (const auto &wc : wall_cases())
    {
        SCOPED_CONTEXT(wc.shape);
        const Mesh plain = plain_wall(wc.takeoff);
        LogCapture log;
        auto r = ost_meshing::create_mesh_for_takeoff(wc.takeoff, wall_condition(true), 1.0);
        CHECK(r.valid);
        CHECK(r.mesh.vertices == plain.vertices);
        CHECK(r.mesh.faces == plain.faces);
        REQUIRE_EQ(log.records.size(), static_cast<size_t>(1));
        CHECK(log.records[0].level == ost_log::Level::Warning);
        CHECK(contains(log.records[0].message, wc.takeoff.uid));
        CHECK(contains(log.records[0].message, "Drop stub union raised for takeoff"));
        CHECK(!log.records[0].exception_text.empty());
    }
}
#endif

#if OST_HAVE_BOOLEAN
TEST(DropStubUnionFailure, RealUnionOfTheWindowsWallsLogsNothing)
{
    // A union that works is returned without any log (Windows: no warning on success), straight and curved.
    for (const auto &wc : wall_cases())
    {
        SCOPED_CONTEXT(wc.shape);
        const Mesh plain = plain_wall(wc.takeoff);
        LogCapture log;
        auto r = ost_meshing::create_mesh_for_takeoff(wc.takeoff, wall_condition(true), 1.0);
        REQUIRE(r.valid);
        CHECK(r.mesh.is_valid());
        CHECK(r.mesh.faces != plain.faces);
        CHECK(log.records.empty());
    }
}
#endif

// ---------------------------------------------------------------- the Core log hook
TEST(CoreLog, SinkReceivesLevelMessageAndExceptionText)
{
    LogCapture log;
    ost_log::warning("first");
    ost_log::warning("second", "boom");
    ost_log::log(ost_log::Level::Error, "third");
    REQUIRE_EQ(log.records.size(), static_cast<size_t>(3));
    CHECK(log.records[0].level == ost_log::Level::Warning);
    CHECK_STR_EQ(log.records[0].message, "first");
    CHECK_STR_EQ(log.records[0].exception_text, "");
    CHECK_STR_EQ(log.records[1].message, "second");
    CHECK_STR_EQ(log.records[1].exception_text, "boom");
    CHECK(log.records[2].level == ost_log::Level::Error);
}

TEST(CoreLog, EmptySinkRestoresTheDefaultAndStopsTheCollector)
{
    std::vector<std::string> seen;
    ost_log::set_sink([&](ost_log::Level, const std::string &message, const std::string &)
                      { seen.push_back(message); });
    ost_log::warning("collected");
    ost_log::set_sink({});
    ost_log::warning("CoreLog test: this line is written by the default sink to stderr, the collector must not see it");
    CHECK_EQ(seen, (std::vector<std::string>{"collected"}));
    CHECK_STR_EQ(ost_log::level_name(ost_log::Level::Warning), "WARNING");
}
