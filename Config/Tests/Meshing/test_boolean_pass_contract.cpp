// Boolean pass input contract (RB-01): a mesh without condition_type is rejected, naming the takeoff, instead of being
// treated as type 0.
// Windows oracle: core/boolean_operations.py _require_condition_types (called first in apply_boolean_operations, before
// the positives-only shortcut and before the native call). The Windows tests patch the native rule and assert it is
// never reached; require_condition_types is the check that precedes any boolean operation, so it never reaches one:
// the host stand-in of the boolean ops (OST_TEST_BOOLEAN=OFF) throws std::runtime_error when called, which these tests
// would report as a wrong exception type.
#include "MeshingTestUtil.hpp"

#include "Meshing/BooleanPass.hpp"

#include <optional>
#include <string>
#include <vector>

using namespace mesh_test;
using ost_meshing::BooleanPassMesh;

namespace
{
    // Windows: MeshData(vertices=[(0.0, 0.0, 0.0)] * 3, faces=[(0, 1, 2)]).
    Mesh dummy_mesh()
    {
        Mesh m;
        m.vertices = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
        m.faces = {{0, 1, 2}};
        return m;
    }

    BooleanPassMesh entry(bool is_negative, std::optional<int> type, const std::string &uid)
    {
        BooleanPassMesh e;
        e.mesh = dummy_mesh();
        e.is_negative = is_negative;
        e.condition_type = type;
        e.takeoff_uid = uid;
        return e;
    }

    // ValueError -> std::invalid_argument; returns false when something else (or nothing) is thrown.
    bool rejected(const std::vector<BooleanPassMesh> &batch, std::string &what)
    {
        return throws_invalid_argument([&]
                                       { ost_meshing::require_condition_types(batch); },
                                       &what);
    }

    bool ends_with(const std::string &s, const std::string &suffix)
    {
        return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }
} // namespace

TEST(BooleanPassContract, MeshWithoutConditionTypeIsRejectedNamingTheTakeoff)
{
    // Port of tests/presentation/visualization/core/test_boolean_operations.py::BooleanRuleContractTests::
    //   test_mesh_without_condition_type_is_rejected_naming_the_takeoff
    // typed = {IsNegativeQuantity False, condition_type 2, takeoff_uid 'a'}; untyped_negative = {True, uid 'b'};
    // untyped_positive = {False, uid 'c'}.
    struct Case
    {
        std::vector<BooleanPassMesh> batch;
        std::string missing;
    };
    const std::vector<Case> cases{
        {{entry(false, 2, "a"), entry(true, std::nullopt, "b")}, "b"},
        {{entry(false, std::nullopt, "c")}, "c"},
    };
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT("missing " + c.missing);
        std::string what;
        CHECK(rejected(c.batch, what));
        CHECK(what.find("condition_type") != std::string::npos);
        CHECK(ends_with(what, "takeoff " + c.missing));
        CHECK_STR_EQ(what, "Boolean operations need condition_type for takeoff " + c.missing);
    }
}

TEST(BooleanPassContract, TheFirstOffendingMeshInListOrderNamesTheTakeoff)
{
    // Windows loops over the pairs in list order and raises on the first one without the key, positive or negative.
    std::string what;
    CHECK(rejected({entry(false, 1, "ok"), entry(true, std::nullopt, "first"), entry(false, std::nullopt, "second")}, what));
    CHECK_STR_EQ(what, "Boolean operations need condition_type for takeoff first");
    CHECK(rejected({entry(false, std::nullopt, "x"), entry(true, std::nullopt, "y")}, what));
    CHECK_STR_EQ(what, "Boolean operations need condition_type for takeoff x");
}

TEST(BooleanPassContract, AMissingTakeoffUidIsAnEmptyUid)
{
    // Windows meta.get('takeoff_uid', ''): the message ends with "takeoff " when the uid is absent.
    std::string what;
    CHECK(rejected({entry(false, std::nullopt, "")}, what));
    CHECK_STR_EQ(what, "Boolean operations need condition_type for takeoff ");
}

TEST(BooleanPassContract, AllNegativesAreCheckedToo)
{
    // A batch of negatives only is rejected like any other (the check precedes every shortcut).
    std::string what;
    CHECK(rejected({entry(true, std::nullopt, "n")}, what));
    CHECK_STR_EQ(what, "Boolean operations need condition_type for takeoff n");
}

TEST(BooleanPassContract, TypedMeshesPassTheCheck)
{
    // Port of ...BooleanRuleContractTests::test_typed_meshes_reach_the_native_rule: positive type 2 'a' + negative type
    // 3 'b' pass the check (the rule that follows is MSH-16). Type 0 is a real type, not a missing one.
    std::string what;
    CHECK_FALSE(rejected({entry(false, 2, "a"), entry(true, 3, "b")}, what));
    CHECK_FALSE(rejected({entry(false, 0, "zero"), entry(true, 0, "zero-neg")}, what));
    CHECK_FALSE(rejected({entry(false, 1, "a")}, what));
    // No exception of any kind (the boolean stand-in throws std::runtime_error when reached).
    bool threw = false;
    try
    {
        ost_meshing::require_condition_types({entry(false, 2, "a"), entry(true, 3, "b")});
    }
    catch (...)
    {
        threw = true;
    }
    CHECK_FALSE(threw);
}

TEST(BooleanPassContract, EmptyBatchIsAccepted)
{
    // Windows: `if not meshes_with_metadata: return []` comes before the check.
    std::string what;
    CHECK_FALSE(rejected({}, what));
}
