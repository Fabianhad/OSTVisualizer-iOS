// insert_takeoff against the Windows writer (TakeoffOperationsMixin.insert_takeoffs):
//   DBP-19  the stored row: BidAreaUID NULL when unassigned, ParentUID 0 for roots (integers, no '' strings)
//   DBP-17  (+UND-04) the takeoff must reference a condition, page, area and parent of ITS bid, the parent on the same page;
//           a violation writes nothing and returns "" (Windows: IncoherentBidOwnedScopeError / MissingBidOwnedUidError)
// Windows oracle: takeoff_operations.py:456-533, bid_owned_identity.py::require_existing_bid_scoped_uid_matches /
// require_takeoff_parent_pages, tests/infrastructure/mdb/components/test_takeoff_operations.py
// (test_takeoff_insert_rejects_*, test_takeoff_insert_accepts_same_bid_owners) and test_takeoff_operations.py:1490-1491.
#include "StoreTestSupport.hpp"

#include <string>

using ost_ptest::TempDb;
using ost_ptest::make_takeoff;
using ost_ptest::seed_two_bids;

namespace
{
    ost_domain::Takeoff with_position(ost_domain::Takeoff t, std::vector<double> position)
    {
        t.position = std::move(position);
        return t;
    }

    int takeoff_count(const TempDb &db) { return db.count("SELECT COUNT(*) FROM BidTakeoffs"); }
} // namespace

// ---------------------------------------------------------------------------------------------------------------
// DBP-19
// ---------------------------------------------------------------------------------------------------------------

// Windows probe (win_insert.py) row 1: (UID 1, BidAreaUID NULL, ParentUID 0, IsNegativeQuantity 0, Curve -1, Rotation 0.0);
// the typed columns are integers / reals, not text.
TEST(Dbp19_InsertedTakeoffRow, RootTakeoffHasNullAreaAndZeroParent)
{
    TempDb db("insert_root_row");
    seed_two_bids(db);
    const std::string uid = ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", "0", "0"));
    REQUIRE_EQ(uid, std::string("901")); // MAX(UID) 900 + 1
    const std::string where = " FROM BidTakeoffs WHERE UID = " + uid;
    CHECK_STR_EQ(db.text("SELECT typeof(BidAreaUID)" + where), "null");
    CHECK_STR_EQ(db.text("SELECT typeof(ParentUID)" + where), "integer");
    CHECK_EQ(db.count("SELECT ParentUID" + where), 0);
    CHECK_STR_EQ(db.text("SELECT typeof(IsNegativeQuantity)" + where), "integer");
    CHECK_EQ(db.count("SELECT IsNegativeQuantity" + where), 0);
    CHECK_EQ(db.count("SELECT Curve" + where), -1);
    CHECK_STR_EQ(db.text("SELECT typeof(Curve)" + where), "integer");
    CHECK_STR_EQ(db.text("SELECT typeof(Rotation)" + where), "real");
    CHECK_STR_EQ(db.text("SELECT Rotation" + where), "0.0");
    for (const char *column : {"UID", "BidUID", "BidConditionUID", "BidPageUID"})
        CHECK_STR_EQ(db.text(std::string("SELECT typeof(") + column + ")" + where), "integer");
    CHECK_EQ(db.count("SELECT BidUID" + where), 10);
    CHECK_EQ(db.count("SELECT BidConditionUID" + where), 30);
    CHECK_EQ(db.count("SELECT BidPageUID" + where), 100);
    // SQL a reader of the row relies on: a root is found by ParentUID = 0 / IS NOT NULL
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = " + uid + " AND ParentUID = 0"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE UID = " + uid + " AND BidAreaUID IS NULL"), 1);

    // load_project reads the row back as a root without area
    const ost_data::ProjectData project = ost_store::load_project(db.path(), "10");
    REQUIRE_EQ(project.takeoffs.size(), static_cast<size_t>(1));
    CHECK_STR_EQ(project.takeoffs[0].parent_uid, "0");
    CHECK_STR_EQ(project.takeoffs[0].area_uid, "0");
    CHECK_EQ(project.takeoffs[0].curve, -1);
}

// A hole stores its parent UID as an integer, an assigned area its UID; flags, curve and rotation as given.
TEST(Dbp19_InsertedTakeoffRow, HoleAndAssignedAreaAreStoredAsIntegers)
{
    TempDb db("insert_hole_row");
    seed_two_bids(db);
    const std::string root = ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100"));
    REQUIRE_EQ(root, std::string("901"));

    ost_domain::Takeoff hole = make_takeoff("31", "100", "60", root);
    hole.is_negative = true;
    hole.curve = 3;
    hole.rotation = -90.5;
    const std::string uid = ost_store::insert_takeoff(db.path(), "10", hole);
    REQUIRE_EQ(uid, std::string("902"));
    const std::string where = " FROM BidTakeoffs WHERE UID = " + uid;
    CHECK_STR_EQ(db.text("SELECT typeof(ParentUID)" + where), "integer");
    CHECK_EQ(db.count("SELECT ParentUID" + where), 901);
    CHECK_STR_EQ(db.text("SELECT typeof(BidAreaUID)" + where), "integer");
    CHECK_EQ(db.count("SELECT BidAreaUID" + where), 60);
    CHECK_EQ(db.count("SELECT IsNegativeQuantity" + where), 1); // 1, not Access -1: DBP-34 is decided wont-fix
    CHECK_EQ(db.count("SELECT Curve" + where), 3);
    CHECK_STR_EQ(db.text("SELECT Rotation" + where), "-90.5");

    const ost_data::ProjectData project = ost_store::load_project(db.path(), "10");
    REQUIRE_EQ(project.takeoffs.size(), static_cast<size_t>(2));
    for (const ost_domain::Takeoff &t : project.takeoffs)
    {
        if (t.uid == uid)
        {
            CHECK_STR_EQ(t.parent_uid, root);
            CHECK_STR_EQ(t.area_uid, "60");
            CHECK(t.is_negative);
            CHECK(t.is_hole());
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// DBP-17 - the references of the new takeoff
// ---------------------------------------------------------------------------------------------------------------

// Test plan: insert_takeoff(bid 10, condition 777, page 888) returns "" and inserts nothing.
TEST(Dbp17_InsertTakeoffValidation, MissingConditionOrPageIsRejected)
{
    TempDb db("insert_missing");
    seed_two_bids(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("777", "888")), "");
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("777", "100")), "");
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "888")), "");
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", "99")), "");      // area
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", "0", "12345")), ""); // parent
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

// Port of test_takeoff_insert_rejects_each_cross_bid_owner: each owner (condition 40, page 200, area 80, parent 900) belongs to
// bid 20, the insert is for bid 10.
TEST(Dbp17_InsertTakeoffValidation, EveryCrossBidOwnerIsRejected)
{
    const struct
    {
        const char *what;
        const char *condition;
        const char *page;
        const char *area;
        const char *parent;
    } cases[] = {
        {"condition of another bid", "40", "100", "0", "0"},
        {"page of another bid", "30", "200", "0", "0"},
        {"area of another bid", "30", "100", "80", "0"},
        {"parent of another bid", "30", "100", "0", "900"},
    };
    int index = 0;
    for (const auto &c : cases)
    {
        SCOPED_CONTEXT(c.what);
        TempDb db("insert_cross_" + std::to_string(index++));
        seed_two_bids(db);
        const std::string before = db.snapshot({"BidTakeoffs"});
        CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff(c.condition, c.page, c.area, c.parent)), "");
        CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
    }
}

// Port of test_takeoff_insert_accepts_same_bid_owners (and the hole): condition, page, area of the bid and a parent of the bid
// on the same page are accepted.
TEST(Dbp17_InsertTakeoffValidation, OwnersOfTheSameBidAreAccepted)
{
    TempDb db("insert_same_bid");
    seed_two_bids(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, BidAreaUID, ParentUID, Position) "
            "VALUES (40, 10, 30, 100, 60, 0, '1;2;3;4')");
    const std::string uid = ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", "60", "40"));
    CHECK_STR_EQ(uid, "901");
    CHECK_STR_EQ(ost_ptest::list(db, "SELECT UID || ':' || BidUID || ':' || BidConditionUID || ':' || BidPageUID || ':' || BidAreaUID || ':' || ParentUID "
                                     "FROM BidTakeoffs WHERE UID = 901"),
                 "901:10:30:100:60:40");
}

// Windows require_takeoff_parent_pages: "A takeoff and its parent must belong to the same page." The parent is a takeoff of the
// bid, but on page 101.
TEST(Dbp17_InsertTakeoffValidation, ParentOnAnotherPageIsRejected)
{
    TempDb db("insert_parent_page");
    seed_two_bids(db);
    db.must("INSERT INTO BidTakeoffs (UID, BidUID, BidConditionUID, BidPageUID, Position) VALUES (40, 10, 30, 101, '1;2;3;4')");
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", "0", "40")), "");
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
    // the same parent from the page it lives on works
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "101", "0", "40")), "901");
}

// Port of test_takeoff_insert_rejects_orphan_bid_before_identity_allocation: the bid itself must exist.
TEST(Dbp17_InsertTakeoffValidation, UnknownBidIsRejected)
{
    TempDb db("insert_unknown_bid");
    seed_two_bids(db);
    db.must("INSERT INTO BidConditions (UID, BidUID, RefNo, Name, Type) VALUES (50, 99, 1, 'C', 0)");
    db.must("INSERT INTO BidPages (UID, BidUID, Name) VALUES (51, 99, 'P')");
    const std::string before = db.snapshot({"BidTakeoffs"});
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "99", make_takeoff("50", "51")), ""); // bid 99 has no Bids row
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "abc", make_takeoff("30", "100")), "");
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "", make_takeoff("30", "100")), "");
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
}

// Windows int(...) of every uid: a non-numeric or non-positive reference is malformed ("authoritative bid-owned UIDs must be
// positive integers").
TEST(Dbp17_InsertTakeoffValidation, MalformedReferencesAreRejected)
{
    TempDb db("insert_malformed");
    seed_two_bids(db);
    const std::string before = db.snapshot({"BidTakeoffs"});
    for (const char *bad : {"abc", "-30", "30.0", "0x1E", "30abc", " 30", "30 ", " ", ""})
    {
        SCOPED_CONTEXT(std::string("uid '") + bad + "'");
        CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff(bad, "100")), "");
        CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", bad)), "");
        CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", "0", bad[0] == '\0' ? "x" : bad)), "");
    }
    // " 60" / "60 " pass Windows' normalize_area_uid (strip) as an ASSIGNED area, but require_valid_unique_bid_owned_uids sees the
    // text as stored and refuses it (not decimal); "\xC2\xA0" 60 is Unicode-padded and refused the same way (Python-run values).
    for (const char *bad : {"abc", "-60", "60.0", "0x3C", "60abc", "00", " 60", "60 ", "\t60", "\xC2\xA0" "60"})
    {
        SCOPED_CONTEXT(std::string("area uid '") + bad + "'");
        CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", bad)), "");
    }
    CHECK_EQ(db.snapshot({"BidTakeoffs"}), before);
    // Windows: an area that is empty or blank is "unassigned" (normalize_area_uid), so the insert goes through
    const std::string uid = ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", " "));
    CHECK_STR_EQ(uid, "901");
    CHECK_STR_EQ(db.text("SELECT typeof(BidAreaUID) FROM BidTakeoffs WHERE UID = 901"), "null");
    // " 0 " strips to "0": unassigned as well (is_unassigned_area_uid(" 0 ") is True)
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", " 0 ")), "902");
    CHECK_STR_EQ(db.text("SELECT typeof(BidAreaUID) FROM BidTakeoffs WHERE UID = 902"), "null");
}

// UND-04: inserting twice with the same explicit UID fails the second time (and keeps the first row); a valid explicit UID is
// returned as given (the undo path re-inserts a deleted takeoff with its old UID); an explicit UID that is not a positive integer
// is rejected.
TEST(Dbp17_InsertTakeoffValidation, ExplicitUidsAreChecked)
{
    TempDb db("insert_explicit_uid");
    seed_two_bids(db);
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", "0", "0", "55")), "55");
    const int count_after_first = takeoff_count(db);
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", with_position(make_takeoff("31", "100", "0", "0", "55"), {9, 9, 9, 9})), "");
    CHECK_EQ(takeoff_count(db), count_after_first);
    CHECK(db.text("SELECT Position FROM BidTakeoffs WHERE UID = 55").find('9') == std::string::npos); // the first row is untouched
    CHECK_EQ(db.count("SELECT BidConditionUID FROM BidTakeoffs WHERE UID = 55"), 30);
    for (const char *bad : {"0", "-5", "abc", "5.5"})
    {
        SCOPED_CONTEXT(std::string("explicit uid ") + bad);
        CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100", "0", "0", bad)), "");
    }
    CHECK_EQ(takeoff_count(db), count_after_first);
}

// A rejected or failed insert allocates nothing: the next valid insert still gets MAX + 1.
TEST(Dbp17_InsertTakeoffValidation, ARejectedInsertLeavesNoTraceInTheNextUid)
{
    TempDb db("insert_no_trace");
    seed_two_bids(db);
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("777", "100")), "");
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100")), "901");
    CHECK_STR_EQ(ost_store::insert_takeoff(db.path(), "10", make_takeoff("30", "100")), "902");
}
