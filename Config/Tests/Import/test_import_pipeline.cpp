// End-to-end import (ost_store::import_project): IMP-06 UID remap, IMP-01 bid number, IMP-03 zero UIDs, IMP-04 page-area
// canonicalisation, IMP-05 typed values, IMP-07 source-database references, IMP-08 atomic import + unknown attributes,
// IMP-10 / IMP-11 master data, and the Windows write order.
//
// Windows: ImportOperationsMixin.import_ost_data (import_operations.py) driven by OstImporter (ost_importer.py). The
// Windows tests run the same pipeline on small SQLite schemas (tests/helpers/mdb/import_export_support.py
// _create_import_schema); where a test needs the iOS schema (page selector, hierarchy, loaders) the real create_schema
// is used instead. iOS has no target-project picker, so Bids.BidProjectUID is always NULL (import_operations.py:74-77
// with target_project_uid None).
#include "ImportTestSupport.hpp"

#include "Persistence/OstSchema.hpp"
#include "Persistence/ProjectStore/ProjectStore_ImportInternal.hpp"

using ost_import::OstRawData;
using ost_import::parse_ost_xml;
using ost_itest::row;
using ost_itest::ScratchDb;
using ost_store::ImportResult;

namespace
{
    void create_import_schema(const ScratchDb &db, const std::vector<std::string> &next_bid_numbers = {"1"},
                              bool unique_page_selected = false)
    {
        db.must("CREATE TABLE Settings (NextBidNo INTEGER)");
        for (const std::string &value : next_bid_numbers)
            db.must("INSERT INTO Settings (NextBidNo) VALUES (" + value + ")");
        db.must_all({
            "CREATE TABLE Bids (UID INTEGER PRIMARY KEY, BidProjectUID INTEGER, EstimatorUID INTEGER, JobStatusUID INTEGER, "
            "JobName TEXT, BidNo INTEGER)",
            "CREATE TABLE Employees (UID INTEGER PRIMARY KEY, EmployeeNo TEXT, FirstName TEXT, LastName TEXT, PayClassUID INTEGER)",
            "CREATE TABLE PayClasses (UID INTEGER PRIMARY KEY, Name TEXT)",
            "CREATE TABLE AccessLevels (UID INTEGER PRIMARY KEY, Name TEXT)",
            "CREATE TABLE CdnTypes (UID INTEGER PRIMARY KEY, Name TEXT, ExpandState INTEGER)",
            "CREATE TABLE JobStatuses (UID INTEGER PRIMARY KEY, Name TEXT, Locked INTEGER, Sequence INTEGER)",
            "CREATE TABLE BidAreas (UID INTEGER PRIMARY KEY, BidUID INTEGER, ParentUID INTEGER, Name TEXT, Sequence INTEGER, GUID TEXT)",
            "CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT, Sequence INTEGER)",
            "CREATE TABLE BidSettings (UID INTEGER PRIMARY KEY, BidUID INTEGER, BidPageSelectedUID INTEGER)",
            "CREATE TABLE BidPageSettings (UID INTEGER PRIMARY KEY, BidPageUID INTEGER, BidAreaUID INTEGER, BidTypAreaUID INTEGER, "
            "BidAreaSelected INTEGER)",
        });
        if (unique_page_selected)
            db.must("CREATE UNIQUE INDEX ux_page_selected ON BidPageSettings (BidPageUID, BidAreaSelected)");
    }

    // The Windows test data: one project with its own page/layer/condition UIDs (all small numbers, so two projects collide).
    std::string project_xml(const std::string &bid_uid, const std::string &page_name, const std::string &layer_name,
                            const std::string &job_name)
    {
        return "<XML_ROOT><Bid UID=\"" + bid_uid + "\" JobName=\"" + job_name + "\">"
               "<BidLayers><BidLayer UID=\"5\" BidUID=\"" + bid_uid + "\" Name=\"" + layer_name + "\" Show=\"True\" Sequence=\"1\"/></BidLayers>"
               "<BidConditions><BidCondition UID=\"10\" BidUID=\"" + bid_uid + "\" BidLayerUID=\"5\" Name=\"Cdn " + job_name + "\" RefNo=\"1\"/></BidConditions>"
               "<BidPages><BidPage UID=\"20\" BidUID=\"" + bid_uid + "\" Name=\"" + page_name + "\" Index1=\"1\">"
               "<BidTakeoffs><BidTakeoff UID=\"30\" BidUID=\"" + bid_uid + "\" BidPageUID=\"20\" BidConditionUID=\"10\" Position=\"1;2\"/>"
               "<BidTakeoff UID=\"31\" BidUID=\"" + bid_uid + "\" BidPageUID=\"20\" BidConditionUID=\"10\" ParentUID=\"30\" Position=\"3;4\"/></BidTakeoffs>"
               "<BidDimensions><BidDimension UID=\"40\" BidUID=\"" + bid_uid + "\" BidPageUID=\"20\" BidTakeoffFromUID=\"30\" BidTakeoffToUID=\"31\"/></BidDimensions>"
               "</BidPage></BidPages>"
               "<BidSettings><BidSetting UID=\"50\" BidUID=\"" + bid_uid + "\" BidPageSelectedUID=\"20\"/></BidSettings>"
               "</Bid></XML_ROOT>";
    }

    bool import_xml(const ScratchDb &db, const std::string &xml, ImportResult *result = nullptr)
    {
        const OstRawData raw = parse_ost_xml(xml);
        if (raw.empty())
            FAIL_TEST("fixture XML did not parse");
        return ost_store::import_project(db.path(), raw, result);
    }

}

// ---------------------------------------------------------------------------------------------------------------
// IMP-06 / IMP-01
// ---------------------------------------------------------------------------------------------------------------

// IMP-06 test plan (5) / LP-07 / DBP-09: import bid A (Bid UID 1, page 20 'Sheet A', layer 5 'L-A') and bid B (Bid UID 2,
// page 20 'Sheet B', layer 5 'L-B'). Windows: both bids keep their own pages and layers (every UID is re-numbered). iOS used
// INSERT OR REPLACE with the file's UIDs: bid 1 ended with 0 pages and 0 layers.
TEST(ImportPipeline, Imp06_SecondProjectWithCollidingUidsNeverOverwritesTheFirst)
{
    ScratchDb db("pipe_collision", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    ImportResult a, b;
    REQUIRE(import_xml(db, project_xml("1", "Sheet A", "L-A", "A"), &a));
    REQUIRE(import_xml(db, project_xml("2", "Sheet B", "L-B", "B"), &b));
    CHECK_NE(a.bid_uid, b.bid_uid);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids WHERE UID IN (" + a.bid_uid + ", " + b.bid_uid + ")"), 2);

    // each bid owns exactly its own rows
    CHECK_EQ(db.rows("SELECT Name FROM BidPages WHERE BidUID = " + a.bid_uid, 1), std::string("Sheet A"));
    CHECK_EQ(db.rows("SELECT Name FROM BidPages WHERE BidUID = " + b.bid_uid, 1), std::string("Sheet B"));
    CHECK_EQ(db.rows("SELECT Name FROM BidLayers WHERE BidUID = " + a.bid_uid, 1), std::string("L-A"));
    CHECK_EQ(db.rows("SELECT Name FROM BidLayers WHERE BidUID = " + b.bid_uid, 1), std::string("L-B"));
    CHECK_EQ(db.rows("SELECT Name FROM BidConditions WHERE BidUID = " + a.bid_uid, 1), std::string("Cdn A"));
    CHECK_EQ(db.rows("SELECT Name FROM BidConditions WHERE BidUID = " + b.bid_uid, 1), std::string("Cdn B"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE BidUID = " + a.bid_uid), 2);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE BidUID = " + b.bid_uid), 2);
    // counts add up: nothing was replaced
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 2);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions"), 2);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 4);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidDimensions"), 2);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidLayers WHERE BidUID IS NOT NULL"), 2);
    // the file's UIDs are not kept
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids WHERE UID IN (1, 2)"), 0);

    // the loaders see them (LP-07: load_project('1') had 0 pages and 0 layers)
    const ost_data::ProjectData project_a = ost_store::load_project(db.path(), a.bid_uid);
    REQUIRE(!project_a.empty());
    CHECK_EQ(project_a.pages.size(), 1u);
    CHECK_EQ(project_a.layers.size(), 1u);
    CHECK_EQ(project_a.conditions.size(), 1u);
    CHECK_EQ(project_a.takeoffs.size(), 2u);
    const ost_data::ProjectData project_b = ost_store::load_project(db.path(), b.bid_uid);
    CHECK_STR_EQ(project_b.pages.begin()->second.name, std::string("Sheet B"));
    CHECK_STR_EQ(project_b.layers.begin()->second.name, std::string("L-B"));
}

// IMP-06 test plan (DBP-09): foreign keys are remapped consistently.
TEST(ImportPipeline, Imp06_ForeignKeysAreRemappedConsistently)
{
    ScratchDb db("pipe_fk", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    ImportResult first, second;
    REQUIRE(import_xml(db, project_xml("100", "Sheet", "L", "A"), &first));
    REQUIRE(import_xml(db, project_xml("100", "Sheet", "L", "B"), &second)); // same Bid UID from "another source"
    for (const ImportResult *r : {&first, &second})
    {
        SCOPED_CONTEXT("bid " + r->bid_uid);
        const std::string bid = r->bid_uid;
        // page -> bid, condition -> bid/layer, takeoff -> page/condition/parent, dimension -> takeoffs, selected page
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages p JOIN Bids b ON b.UID = p.BidUID WHERE b.UID = " + bid), 1);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions c JOIN BidLayers l ON l.UID = c.BidLayerUID AND l.BidUID = c.BidUID WHERE c.BidUID = " + bid), 1);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs t JOIN BidPages p ON p.UID = t.BidPageUID AND p.BidUID = t.BidUID "
                          "JOIN BidConditions c ON c.UID = t.BidConditionUID AND c.BidUID = t.BidUID WHERE t.BidUID = " + bid),
                 2);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs child JOIN BidTakeoffs parent ON parent.UID = child.ParentUID "
                          "WHERE child.BidUID = " + bid + " AND parent.BidUID = " + bid), 1);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidDimensions d JOIN BidTakeoffs f ON f.UID = d.BidTakeoffFromUID "
                          "JOIN BidTakeoffs t ON t.UID = d.BidTakeoffToUID WHERE d.BidUID = " + bid + " AND f.BidUID = " + bid + " AND t.BidUID = " + bid),
                 1);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidSettings s JOIN BidPages p ON p.UID = s.BidPageSelectedUID WHERE s.BidUID = " + bid +
                          " AND p.BidUID = " + bid),
                 1);
    }
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 4);
    CHECK_EQ(db.count("SELECT COUNT(DISTINCT UID) FROM BidTakeoffs"), 4);
}

// IMP-06: the new UIDs lie above every UID that exists in the database or is referenced by a child column.
TEST(ImportPipeline, Imp06_NewUidsAreAboveTheFloor)
{
    ScratchDb db("pipe_above_floor", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    db.must_all({"INSERT INTO Bids (UID, JobName, BidNo) VALUES (500, 'Existing', 9)",
                 "INSERT INTO BidAreas (UID, BidUID, Name) VALUES (800, 500, 'Area')",
                 "INSERT INTO BidNotes (UID, BidUID) VALUES (3, 900)"}); // dangling reference to Bids 900
    ImportResult r;
    REQUIRE(import_xml(db, project_xml("1", "Sheet", "L", "A"), &r));
    // Numbering order: bid row, then the bid tables in schema_contract order (BidSettings, BidEmployees, BidLayers,
    // BidConditions, ...), then BidPages, then the page tables in first-seen order (BidTakeoffs, BidDimensions).
    CHECK_EQ(r.bid_uid, std::string("901"));
    CHECK_EQ(db.rows("SELECT UID FROM BidSettings WHERE BidUID = 901", 1), std::string("902"));
    CHECK_EQ(db.rows("SELECT UID FROM BidLayers WHERE BidUID = 901", 1), std::string("903"));
    CHECK_EQ(db.rows("SELECT UID FROM BidConditions WHERE BidUID = 901", 1), std::string("904"));
    CHECK_EQ(db.rows("SELECT UID FROM BidPages WHERE BidUID = 901", 1), std::string("905"));
    CHECK_EQ(db.rows("SELECT UID FROM BidTakeoffs WHERE BidUID = 901 ORDER BY UID", 1), std::string("906;907"));
    CHECK_EQ(db.rows("SELECT UID FROM BidDimensions WHERE BidUID = 901", 1), std::string("908"));
    for (const char *table : {"BidPages", "BidConditions", "BidTakeoffs", "BidDimensions", "BidSettings"})
        CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM ") + table + " WHERE UID <= 900"), 0);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_passes_resolved_maps_to_transform_and_assigns_project
// with the real transform: NextBidNo 7, an existing 'Concrete' condition type 40, incoming concrete/Won/Regular. The floor is 40
// (CdnTypes), the new status gets 41, the bid 42, BidNo = 7 and NextBidNo becomes 8; no target project -> BidProjectUID NULL.
TEST(ImportPipeline, Imp01_Imp06_BidGetsTheNextBidNoNewUidAndNoProject)
{
    ScratchDb db("pipe_resolved_maps");
    create_import_schema(db, {"7"});
    db.must("INSERT INTO CdnTypes (UID, Name, ExpandState) VALUES (40, 'Concrete', 0)");
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" JobName=\"Imported\" BidProjectUID=\"99\"/>"
                       "<CdnTypes><CdnType UID=\"9\" Name=\"concrete\" ExpandState=\"0\"/></CdnTypes>"
                       "<JobStatuses><JobStatuse UID=\"3\" Name=\"Won\" Locked=\"0\" Sequence=\"1\"/></JobStatuses>"
                       "<PayClasses><PayClass UID=\"4\" Name=\"Regular\"/></PayClasses></XML_ROOT>",
                       &r));
    CHECK_EQ(r.bid_uid, std::string("42"));
    CHECK_EQ(db.rows("SELECT UID, BidProjectUID, JobName, BidNo FROM Bids", 4), std::string("42|NULL|Imported|7"));
    CHECK_EQ(db.rows("SELECT UID, Name FROM JobStatuses", 2), std::string("41|Won"));
    CHECK_EQ(db.rows("SELECT UID, Name FROM PayClasses", 2), std::string("1|Regular"));
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("8"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM CdnTypes"), 1);
}

// IMP-01 test plan / IMP-19 plan: a fresh iOS database gives the first import BidNo 1 (NextBidNo is seeded with 1), the same
// file imported again is an independent copy with BidNo 2 and NextBidNo ends at 3. The file's own BidNo (99) is discarded.
TEST(ImportPipeline, Imp01_BidNoComesFromSettingsNotFromTheFile)
{
    ScratchDb db("pipe_bidno", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    CHECK_EQ(db.count("SELECT NextBidNo FROM Settings"), 1);
    const std::string xml = "<XML_ROOT><Bid UID=\"1\" JobName=\"Same\" BidNo=\"99\"/></XML_ROOT>";
    ImportResult a, b;
    REQUIRE(import_xml(db, xml, &a));
    REQUIRE(import_xml(db, xml, &b));
    CHECK_NE(a.bid_uid, b.bid_uid);
    CHECK_EQ(db.rows("SELECT BidNo FROM Bids WHERE UID = " + a.bid_uid, 1), std::string("1"));
    CHECK_EQ(db.rows("SELECT BidNo FROM Bids WHERE UID = " + b.bid_uid, 1), std::string("2"));
    CHECK_EQ(db.count("SELECT NextBidNo FROM Settings"), 3);
}

// IMP-19 (decided: the "already imported" guard of ImportService.mm is removed, every import is an independent copy): the SAME
// project file imported twice gives two bids with BidNo 1 and 2, each with a complete copy of its layer, condition, pages,
// takeoffs and dimension under new UIDs; and a bid that already carries the file's Bid UID does not refuse the import (the
// stored UIDs are re-numbered, nothing is overwritten). Windows has no duplicate check (ost_importer.py:84-99,
// import_operations.py:33-82). The Core import never had a guard; ost_store::is_bid_imported only served the Objective-C one.
TEST(ImportPipeline, Imp19_TheSameProjectImportedTwiceIsTwoIndependentCopies)
{
    ScratchDb db("pipe_twice", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    db.must("INSERT INTO Bids (UID, JobName) VALUES (1, 'Existing bid with the file UID')");
    const std::string xml = project_xml("1", "Sheet", "L", "Same");
    ImportResult first, second;
    REQUIRE(import_xml(db, xml, &first));
    REQUIRE(import_xml(db, xml, &second));
    CHECK(!first.bid_uid.empty());
    CHECK(!second.bid_uid.empty());
    CHECK_NE(first.bid_uid, second.bid_uid);
    CHECK_NE(first.bid_uid, std::string("1"));
    CHECK_NE(second.bid_uid, std::string("1"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 3);
    CHECK_EQ(db.rows("SELECT JobName FROM Bids WHERE UID = 1", 1), std::string("Existing bid with the file UID"));
    CHECK_EQ(db.rows("SELECT BidNo FROM Bids WHERE UID = " + first.bid_uid, 1), std::string("1"));
    CHECK_EQ(db.rows("SELECT BidNo FROM Bids WHERE UID = " + second.bid_uid, 1), std::string("2"));
    for (const ImportResult *r : {&first, &second})
    {
        SCOPED_CONTEXT("bid " + r->bid_uid);
        const std::string bid = r->bid_uid;
        CHECK_EQ(db.rows("SELECT Name FROM BidPages WHERE BidUID = " + bid, 1), std::string("Sheet"));
        CHECK_EQ(db.rows("SELECT Name FROM BidLayers WHERE BidUID = " + bid, 1), std::string("L"));
        CHECK_EQ(db.rows("SELECT Name FROM BidConditions WHERE BidUID = " + bid, 1), std::string("Cdn Same"));
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE BidUID = " + bid), 2);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidDimensions WHERE BidUID = " + bid), 1);
    }
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 2);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 4);
    // every row of the first copy has a UID of its own, none shared with the second one
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages a JOIN BidPages b ON a.UID = b.UID AND a.BidUID <> b.BidUID"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs a JOIN BidTakeoffs b ON a.UID = b.UID AND a.BidUID <> b.BidUID"), 0);
    // both copies load
    for (const ImportResult *r : {&first, &second})
    {
        const ost_data::ProjectData project = ost_store::load_project(db.path(), r->bid_uid);
        CHECK(!project.empty());
        CHECK_EQ(project.takeoffs.size(), 2u);
    }
}

// IMP-01 test plan: Settings.NextBidNo = 7 -> BidNo 7 / NextBidNo 8; then 8 / 9. Two Settings rows make the import fail with
// nothing written; an empty Settings table gives BidNo 1 and NextBidNo 2.
TEST(ImportPipeline, Imp01_SettingsCardinalityAndEmptySettings)
{
    {
        ScratchDb db("pipe_bidno_two_settings");
        create_import_schema(db, {"7", "12"});
        ImportResult r;
        CHECK_FALSE(import_xml(db, "<XML_ROOT><Bid UID=\"1\" JobName=\"X\"/></XML_ROOT>", &r));
        CHECK(r.error.find("Settings has multiple rows; expected at most one") != std::string::npos);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 0);
        CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings ORDER BY NextBidNo", 1), std::string("7;12"));
        CHECK(r.bid_uid.empty());
    }
    {
        ScratchDb db("pipe_bidno_empty_settings");
        create_import_schema(db, {});
        ImportResult r;
        REQUIRE(import_xml(db, "<XML_ROOT><Bid UID=\"1\" JobName=\"X\"/></XML_ROOT>", &r));
        CHECK_EQ(db.rows("SELECT BidNo FROM Bids", 1), std::string("1"));
        CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("2"));
    }
}

// A bid whose file carries no usable UID is numbered by the database and still reports the UID it got.
TEST(ImportPipeline, Imp06_BidWithoutAUidReportsTheUidTheDatabaseAssigned)
{
    ScratchDb db("pipe_bid_no_uid");
    create_import_schema(db);
    db.must("INSERT INTO Bids (UID, JobName) VALUES (12, 'Existing')");
    ImportResult r;
    REQUIRE(import_xml(db, "<XML_ROOT><Bid JobName=\"No uid\"/></XML_ROOT>", &r));
    CHECK_EQ(r.bid_uid, std::string("13"));
    CHECK_EQ(db.rows("SELECT UID FROM Bids WHERE JobName = 'No uid'", 1), std::string("13"));
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-03 / IMP-05 / IMP-07
// ---------------------------------------------------------------------------------------------------------------

// IMP-03 test plan: after a full import of a page with BidPageFolderUID="0", the column IS NULL and load_project gives an empty
// folder_uid (the page selector hides pages whose folder_uid is the string "0").
TEST(ImportPipeline, Imp03_ZeroUidFieldsAreStoredAsNull)
{
    ScratchDb db("pipe_zero_uids", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" JobName=\"Z\"><BidPages>"
                       "<BidPage UID=\"20\" BidUID=\"1\" Name=\"P\" BidPageFolderUID=\"0\" MasterPageUID=\"0\" Sequence=\"0\" Index1=\"1\"/>"
                       "</BidPages></Bid></XML_ROOT>",
                       &r));
    // a "0" in a field that does not end with UID stays 0 (BidPages.Sequence is a REAL column in the iOS schema)
    CHECK_EQ(db.rows("SELECT BidPageFolderUID IS NULL, MasterPageUID IS NULL, Sequence = 0, typeof(Sequence) FROM BidPages WHERE BidUID = " + r.bid_uid, 4),
             std::string("1|1|1|real"));
    const ost_data::ProjectData project = ost_store::load_project(db.path(), r.bid_uid);
    REQUIRE_EQ(project.pages.size(), 1u);
    CHECK_STR_EQ(project.pages.begin()->second.folder_uid, std::string());
}

// IMP-03, the read side (regression guard, not a fix): a database imported before the zero UIDs became NULL still holds the
// integer 0 in BidPages.BidPageFolderUID. The Windows reader (bid_data_reader.py:742, settings_reader.py:268
// `str(row.BidPageFolderUID) if row.BidPageFolderUID else None`) reads 0 and NULL as "no folder". load_project already clears
// the folder of every page whose folder does not exist, so such a page keeps a button in the page selector; a page in a real
// folder keeps its folder.
TEST(ImportPipeline, Imp03_AStoredZeroFolderUidReadsAsNoFolder)
{
    ScratchDb db("pipe_legacy_zero_folder", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    db.must_all({"INSERT INTO Bids (UID, JobName) VALUES (500, 'Legacy')",
                 "INSERT INTO BidPageFolders (UID, BidUID, Name) VALUES (77, 500, 'F')",
                 "INSERT INTO BidPages (UID, BidUID, Name, BidPageFolderUID, Index1) VALUES (601, 500, 'zero', 0, 1)",
                 "INSERT INTO BidPages (UID, BidUID, Name, BidPageFolderUID, Index1) VALUES (602, 500, 'null', NULL, 2)",
                 "INSERT INTO BidPages (UID, BidUID, Name, BidPageFolderUID, Index1) VALUES (603, 500, 'real', 77, 3)"});
    const ost_data::ProjectData project = ost_store::load_project(db.path(), "500");
    REQUIRE_EQ(project.pages.size(), 3u);
    CHECK_STR_EQ(project.pages.at("601").folder_uid, std::string());
    CHECK_STR_EQ(project.pages.at("602").folder_uid, std::string());
    CHECK_STR_EQ(project.pages.at("603").folder_uid, std::string("77"));
}

// IMP-05 test plan: BidLayers Show="True" must load as visible; a condition with Backout="True" loads with backout.
TEST(ImportPipeline, Imp05_YesNoTextValuesLoadAsTrue)
{
    ScratchDb db("pipe_yesno", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" JobName=\"Y\" IsAccepted=\"False\">"
                       "<BidLayers><BidLayer UID=\"5\" BidUID=\"1\" Name=\"On\" Show=\"True\" IsLocked=\"true\" Sequence=\"1\"/>"
                       "<BidLayer UID=\"6\" BidUID=\"1\" Name=\"Off\" Show=\"False\" IsLocked=\"\" Sequence=\"2\"/></BidLayers>"
                       "<BidConditions><BidCondition UID=\"10\" BidUID=\"1\" Name=\"C\" RefNo=\"1\" Backout=\"True\" Grid=\"0\" Trim=\"1\" RoundQuantity=\"\"/></BidConditions>"
                       "</Bid></XML_ROOT>",
                       &r));
    CHECK_EQ(db.rows("SELECT Name, Show, IsLocked FROM BidLayers WHERE BidUID = " + r.bid_uid + " ORDER BY Sequence", 3),
             std::string("On|-1|-1;Off|0|0"));
    CHECK_EQ(db.rows("SELECT Backout, Grid, Trim, RoundQuantity FROM BidConditions WHERE BidUID = " + r.bid_uid, 4),
             std::string("-1|0|-1|0"));
    CHECK_EQ(db.rows("SELECT IsAccepted, typeof(IsAccepted) FROM Bids WHERE UID = " + r.bid_uid, 2), std::string("0|integer"));
    const ost_data::ProjectData project = ost_store::load_project(db.path(), r.bid_uid);
    REQUIRE_EQ(project.layers.size(), 2u);
    bool on_visible = false, off_visible = true;
    for (const auto &layer : project.layers)
    {
        if (layer.second.name == "On")
            on_visible = layer.second.visible;
        if (layer.second.name == "Off")
            off_visible = layer.second.visible;
    }
    CHECK(on_visible);
    CHECK_FALSE(off_visible);
    REQUIRE_EQ(project.conditions.size(), 1u);
    CHECK(project.conditions.begin()->second.backout);
}

// IMP-05: numeric columns are stored as numbers, non-numeric text in a numeric column becomes NULL, text columns keep ''.
TEST(ImportPipeline, Imp05_NumericAndTextValuesAreStoredByColumnType)
{
    ScratchDb db("pipe_typed_values", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" JobName=\"\" PageScale=\"0.125\" HoursPerDay=\"abc\" MeasureBase=\"\" BidNo=\"77\"/></XML_ROOT>", &r));
    CHECK_EQ(db.rows("SELECT typeof(JobName), JobName, typeof(PageScale), PageScale, typeof(HoursPerDay), typeof(MeasureBase) FROM Bids WHERE UID = " + r.bid_uid, 6),
             std::string("text||real|0.125|null|null"));
}

// IMP-07 test plan / BRW-M02: on a database where BidProjects UID 1 is 'Deleted Bids', a Bid with BidProjectUID=1,
// OrigBidProjectUID=1, ParentBidUID=5, SourceBidUID=9, OCRUID=3, CoverSheetSelItemUID=4 gets all of those NULL and is listed
// under orphan_bids, not under Deleted Bids.
TEST(ImportPipeline, Imp07_SourceDatabaseReferencesAreNulledAndTheBidIsAnOrphan)
{
    ScratchDb db("pipe_orphan", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    CHECK_EQ(db.rows("SELECT Name FROM BidProjects WHERE UID = 1", 1), std::string("Deleted Bids"));
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" JobName=\"Orphan\" BidProjectUID=\"1\" OrigBidProjectUID=\"1\" ParentBidUID=\"5\" "
                       "OrigParentBidUID=\"6\" SourceBidUID=\"9\" OCRUID=\"3\" CoverSheetSelItemUID=\"4\" EstimatorUID=\"33\" "
                       "PrManagerUID=\"34\" JobSiteManagerUID=\"35\" JobStatusUID=\"3\"/></XML_ROOT>",
                       &r));
    CHECK_EQ(db.rows("SELECT BidProjectUID, OrigBidProjectUID, ParentBidUID, OrigParentBidUID, SourceBidUID, CoverSheetSelItemUID, "
                     "EstimatorUID, PrManagerUID, JobSiteManagerUID, JobStatusUID FROM Bids WHERE UID = " + r.bid_uid,
                     10),
             std::string("NULL|NULL|NULL|NULL|NULL|NULL|NULL|NULL|NULL|NULL"));
    const ost_domain::DatabaseHierarchy hierarchy = ost_store::load_hierarchy(db.path());
    bool in_orphans = false;
    for (const auto &bid : hierarchy.orphan_bids)
        in_orphans = in_orphans || bid.bid_uid == r.bid_uid;
    CHECK(in_orphans);
    for (const auto &group : hierarchy.folders)
        for (const auto &bid : group.bids)
            CHECK_NE(bid.bid_uid, r.bid_uid);
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-08: atomic import, unknown attributes
// ---------------------------------------------------------------------------------------------------------------

// IMP-08 test plan (a) / DBP-10: <BidCondition UID=10/>, <BidCondition UID=11 NotAColumn="1"/>, <BidCondition UID=12/> store
// all three (the unknown attribute is dropped), a Bid carrying Bogus="x" still stores its Bids row, and an XML BidTakeoff with a
// Name attribute (BidTakeoffs has no Name column) is stored.
TEST(ImportPipeline, Imp08_UnknownAttributesAreDroppedAndTheRestIsStored)
{
    ScratchDb db("pipe_unknown_attrs", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"100\" JobName=\"Bid A\" Bogus=\"x\" BrandNewColumn=\"x\">"
                       "<BidConditions><BidCondition UID=\"10\" BidUID=\"100\" Name=\"One\"/>"
                       "<BidCondition UID=\"11\" BidUID=\"100\" Name=\"Two\" NotAColumn=\"1\"/>"
                       "<BidCondition UID=\"12\" BidUID=\"100\" Name=\"Three\"/></BidConditions>"
                       "<BidPages><BidPage UID=\"20\" BidUID=\"100\" Name=\"P\" Index1=\"1\">"
                       "<BidTakeoffs><BidTakeoff UID=\"30\" BidUID=\"100\" BidPageUID=\"20\" BidConditionUID=\"10\" Position=\"1;1\" Name=\"label\"/>"
                       "<BidTakeoff UID=\"31\" BidUID=\"100\" BidPageUID=\"20\" BidConditionUID=\"11\" Position=\"2;2\" NewTakeoffAttr=\"y\"/></BidTakeoffs>"
                       "</BidPage></BidPages></Bid></XML_ROOT>",
                       &r));
    CHECK_EQ(db.rows("SELECT JobName FROM Bids WHERE UID = " + r.bid_uid, 1), std::string("Bid A"));
    CHECK_EQ(db.rows("SELECT Name FROM BidConditions WHERE BidUID = " + r.bid_uid + " ORDER BY UID", 1), std::string("One;Two;Three"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs WHERE BidUID = " + r.bid_uid), 2);
}

// A table the database does not have is skipped without failing the import (the Windows catalog returns no columns).
TEST(ImportPipeline, Imp08_RowsOfAnUnknownTableAreSkipped)
{
    ScratchDb db("pipe_unknown_table");
    create_import_schema(db);
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" JobName=\"T\"><BidAreas><BidArea UID=\"2\" BidUID=\"1\" Name=\"A\"/></BidAreas>"
                       "<BidZones><BidZone UID=\"3\" BidUID=\"1\" Name=\"Z\"/></BidZones></Bid></XML_ROOT>",
                       &r));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidAreas"), 1);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rolls_back_every_write_when_a_bid_row_fails
// A duplicate source UID inside one table maps to one new UID, so the second INSERT violates the primary key: Bids, BidAreas,
// CdnTypes and Settings.NextBidNo (7) are exactly as before and the import reports failure with the Windows log line.
TEST(ImportPipeline, Imp08_AFailedBidTableRowRollsBackEveryWrite)
{
    ScratchDb db("pipe_rollback_bid_row");
    create_import_schema(db, {"7"});
    const std::string before = db.snapshot({"Bids", "BidAreas", "CdnTypes", "Settings"});
    ImportResult r;
    CHECK_FALSE(import_xml(db,
                           "<XML_ROOT><Bid UID=\"5\" JobName=\"Imported\"><BidAreas>"
                           "<BidArea UID=\"30\" BidUID=\"5\" Name=\"A\"/><BidArea UID=\"30\" BidUID=\"5\" Name=\"Duplicate primary key\"/>"
                           "</BidAreas></Bid><CdnTypes><CdnType UID=\"9\" Name=\"New\" ExpandState=\"0\"/></CdnTypes></XML_ROOT>",
                           &r));
    CHECK(r.error.find("Failed inserting into [BidAreas], row_uid=") != std::string::npos);
    CHECK(r.error.find("columns=['BidUID', 'Name', 'UID'], error=UNIQUE constraint failed: BidAreas.UID") != std::string::npos);
    CHECK(r.bid_uid.empty());
    CHECK_EQ(db.snapshot({"Bids", "BidAreas", "CdnTypes", "Settings"}), before);
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("7"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_a_page_table_failure_rolls_back_every_earlier_table_and_is_logged
TEST(ImportPipeline, Imp08_AFailedPageTableRowRollsBackEveryEarlierTable)
{
    ScratchDb db("pipe_rollback_page_row");
    create_import_schema(db, {"7"});
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER PRIMARY KEY, BidPageUID INTEGER)");
    const std::string before = db.snapshot({"Bids", "BidAreas", "BidPages", "BidTakeoffs", "CdnTypes", "JobStatuses", "Settings"});
    ImportResult r;
    CHECK_FALSE(import_xml(db,
                           "<XML_ROOT><Bid UID=\"5\" JobName=\"Imported\"><BidAreas><BidArea UID=\"30\" BidUID=\"5\" Name=\"Area\"/></BidAreas>"
                           "<BidPages><BidPage UID=\"20\" BidUID=\"5\" Name=\"Sheet\"><BidTakeoffs>"
                           "<BidTakeoff UID=\"40\" BidPageUID=\"20\"/><BidTakeoff UID=\"40\" BidPageUID=\"20\"/></BidTakeoffs></BidPage></BidPages></Bid>"
                           "<CdnTypes><CdnType UID=\"9\" Name=\"New\" ExpandState=\"0\"/></CdnTypes>"
                           "<JobStatuses><JobStatuse UID=\"3\" Name=\"Won\" Locked=\"0\" Sequence=\"1\"/></JobStatuses></XML_ROOT>",
                           &r));
    CHECK(r.error.find("Failed inserting into [BidTakeoffs], row_uid=") != std::string::npos);
    CHECK(r.error.find("error=UNIQUE constraint failed: BidTakeoffs.UID") != std::string::npos);
    CHECK_EQ(db.snapshot({"Bids", "BidAreas", "BidPages", "BidTakeoffs", "CdnTypes", "JobStatuses", "Settings"}), before);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_missing_settings_table_rolls_back_master_data_and_bid_writes
TEST(ImportPipeline, Imp08_MissingSettingsTableRollsBackMasterDataAndBidWrites)
{
    ScratchDb db("pipe_missing_settings");
    create_import_schema(db, {"7"});
    db.must("DROP TABLE Settings");
    ImportResult r;
    CHECK_FALSE(import_xml(db,
                           "<XML_ROOT><Bid UID=\"5\" JobName=\"Imported\"/><CdnTypes><CdnType UID=\"9\" Name=\"New\" ExpandState=\"0\"/></CdnTypes></XML_ROOT>",
                           &r));
    CHECK(r.error.find("Settings table is unavailable") != std::string::npos);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM CdnTypes"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 0);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_a_non_numeric_uid_in_the_target_stops_the_import_before_any_write
TEST(ImportPipeline, Imp08_ANonNumericUidInTheTargetStopsTheImportBeforeAnyWrite)
{
    ScratchDb db("pipe_non_numeric_target");
    create_import_schema(db, {"7"});
    db.must_all({"DROP TABLE BidAreas", "CREATE TABLE BidAreas (UID INTEGER, Name TEXT)",
                 "INSERT INTO BidAreas (UID, Name) VALUES ('abc', 'corrupt')"});
    ImportResult r;
    CHECK_FALSE(import_xml(db,
                           "<XML_ROOT><Bid UID=\"5\" JobName=\"Imported\"/><CdnTypes><CdnType UID=\"9\" Name=\"New\" ExpandState=\"0\"/></CdnTypes></XML_ROOT>",
                           &r));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM CdnTypes"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 0);
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("7"));
}

// Port of tests/integration/import_export/test_master_data_reconciliation.py (both tests): two stored statuses share the imported
// name 'Open' (10 and 11). CdnTypes are reconciled before JobStatuses, so the new condition type is already inserted when the
// ambiguity is found; it is rolled back together with the never written Bid. The control run with exactly one 'Open' commits,
// reuses UID 10 for the Bid and inserts the genuinely new condition type.
TEST(ImportPipeline, Imp10_AmbiguousMasterReconciliationRollsBackAndTheControlCommits)
{
    const std::string xml =
        "<XML_ROOT><Bid UID=\"1\" JobStatusUID=\"90\" JobName=\"Imported\"><BidAreas/><BidPages/></Bid>"
        "<CdnTypes><CdnType UID=\"70\" Name=\"Brand New Type\" ExpandState=\"0\"/></CdnTypes>"
        "<JobStatuses><JobStatuse UID=\"90\" Name=\"Open\" Locked=\"0\" Sequence=\"1\"/></JobStatuses></XML_ROOT>";
    {
        ScratchDb db("pipe_master_ambiguous");
        create_import_schema(db);
        db.must_all({"INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (10, 'Open', 0, 1)",
                     "INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (11, 'Open', 1, 2)"});
        ImportResult r;
        CHECK_FALSE(import_xml(db, xml, &r));
        CHECK(r.error.find("Imported master-data identity is ambiguous") != std::string::npos);
        CHECK(r.error.find("JobStatuses.Name") != std::string::npos);
        CHECK(r.error.find("matching UIDs 10, 11") != std::string::npos);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 0);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM CdnTypes"), 0);
        CHECK_EQ(db.rows("SELECT UID, Locked, Sequence FROM JobStatuses ORDER BY UID", 3), std::string("10|0|1;11|1|2"));
    }
    {
        ScratchDb db("pipe_master_control");
        create_import_schema(db);
        db.must("INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (10, 'Open', 0, 1)");
        ImportResult r;
        REQUIRE(import_xml(db, xml, &r));
        CHECK_EQ(db.rows("SELECT UID, Name FROM JobStatuses", 2), std::string("10|Open"));
        CHECK_EQ(db.rows("SELECT JobStatusUID, JobName FROM Bids", 2), std::string("10|Imported"));
        CHECK_EQ(db.rows("SELECT Name FROM CdnTypes", 1), std::string("Brand New Type"));
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_every_pre_write_stage_failure_rolls_back_the_earlier_stages
// (multiple Settings rows after the condition type and job status were inserted).
TEST(ImportPipeline, Imp08_AFailureAfterMasterDataWasInsertedRollsItBack)
{
    ScratchDb db("pipe_stage_rollback");
    create_import_schema(db, {"7", "12"});
    ImportResult r;
    CHECK_FALSE(import_xml(db,
                           "<XML_ROOT><Bid UID=\"5\"/><CdnTypes><CdnType UID=\"9\" Name=\"New\" ExpandState=\"0\"/></CdnTypes>"
                           "<JobStatuses><JobStatuse UID=\"3\" Name=\"Open\" Locked=\"0\" Sequence=\"1\"/></JobStatuses></XML_ROOT>",
                           &r));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM CdnTypes"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM JobStatuses"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 0);
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings ORDER BY NextBidNo", 1), std::string("7;12"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_a_successful_import_returns_true_exactly_and_logs_nothing
// (+ test_a_successful_import_commits_the_bid_its_children_and_the_bid_number): committed, no transaction left open, the bid
// number consumed.
TEST(ImportPipeline, Imp08_ASuccessfulImportCommitsEverythingAndLeavesNoTransactionOpen)
{
    ScratchDb db("pipe_success");
    create_import_schema(db, {"7"});
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER PRIMARY KEY, BidPageUID INTEGER)");
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"5\" JobName=\"Imported\"><BidAreas><BidArea UID=\"30\" BidUID=\"5\" Name=\"Area\"/></BidAreas>"
                       "<BidPages><BidPage UID=\"20\" BidUID=\"5\" Name=\"Sheet\"><BidTakeoffs><BidTakeoff UID=\"40\" BidPageUID=\"20\"/></BidTakeoffs>"
                       "</BidPage></BidPages></Bid></XML_ROOT>",
                       &r));
    CHECK(r.error.empty());
    // the file has bid 5, area 30, page 20, takeoff 40; the floor of the empty tables is 0 -> 1 (bid), 2 (area), 3 (page), 4 (takeoff)
    CHECK_EQ(r.bid_uid, std::string("1"));
    CHECK_EQ(db.rows("SELECT UID, BidProjectUID, BidNo FROM Bids", 3), std::string("1|NULL|7"));
    CHECK_EQ(db.rows("SELECT UID, BidUID, Name FROM BidAreas", 3), std::string("2|1|Area"));
    CHECK_EQ(db.rows("SELECT UID, BidUID, Name FROM BidPages", 3), std::string("3|1|Sheet"));
    CHECK_EQ(db.rows("SELECT UID, BidPageUID FROM BidTakeoffs", 2), std::string("4|3"));
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("8"));
    CHECK(db.exec("INSERT INTO Bids (UID, JobName) VALUES (99, 'a write after the import works')"));
}

// Windows imports a <Bid> element that has no attributes: the empty row gets BidNo = Settings.NextBidNo and BidProjectUID =
// NULL, the engine numbers the UID (OstImporter._parse_ost_xml finds the element, import_operations._write_to_db inserts it).
// iOS used to refuse it as "no Bid element" because OstRawData::empty() only looked at bid_row.
TEST(ImportPipeline, Imp08_ABidElementWithoutAttributesIsImportedWithItsBidNo)
{
    ScratchDb db("pipe_attributeless_bid");
    create_import_schema(db, {"7"});
    const OstRawData raw = parse_ost_xml("<XML_ROOT><Bid/></XML_ROOT>");
    ImportResult r;
    CHECK(ost_store::import_project(db.path(), raw, &r));
    CHECK_EQ(db.rows("SELECT UID, BidProjectUID, BidNo FROM Bids", 3), std::string("1|NULL|7"));
    CHECK_EQ(r.bid_uid, std::string("1"));
    CHECK_EQ(db.rows("SELECT NextBidNo FROM Settings", 1), std::string("8"));
}

// IMP-08 (+ dbp11 control): a BEFORE INSERT trigger that fails on a late table leaves the database exactly as it was.
TEST(ImportPipeline, Imp08_ABlockedTableRollsBackTheWholeImport)
{
    ScratchDb db("pipe_blocked_table", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    db.block("BidTakeoffs", "INSERT");
    const std::vector<std::string> tables = {"Bids", "BidPages", "BidConditions", "BidLayers", "BidTakeoffs", "CdnTypes", "Settings"};
    const std::string before = db.snapshot(tables);
    ImportResult r;
    CHECK_FALSE(import_xml(db, project_xml("1", "Sheet", "L", "A"), &r));
    CHECK(r.error.find("blocked") != std::string::npos);
    CHECK_EQ(db.snapshot(tables), before);
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-04 (+ IMP-11 / IMP-10 through the pipeline)
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/integration/import_export/test_page_area_selection.py::test_import_canonicalizes_duplicate_page_area_selection_then_save_succeeds
// (the import half; the unique index (BidPageUID, BidAreaSelected) would reject the duplicate selected rows).
TEST(ImportPipeline, Imp04_ImportCanonicalisesDuplicatePageAreaSelection)
{
    ScratchDb db("pipe_page_area_selection");
    create_import_schema(db, {"1"}, /*unique_page_selected=*/true);
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" JobName=\"Imported\">"
                       "<BidAreas><BidArea UID=\"10\" BidUID=\"1\" Name=\"Area 1\" Sequence=\"1\"/><BidArea UID=\"11\" BidUID=\"1\" Name=\"Area 2\" Sequence=\"2\"/></BidAreas>"
                       "<BidPages><BidPage UID=\"20\" BidUID=\"1\" Name=\"Sheet\" Sequence=\"1\"><BidPageSettings>"
                       "<BidPageSetting UID=\"30\" BidPageUID=\"20\" BidAreaUID=\"10\" BidAreaSelected=\"1\"/>"
                       "<BidPageSetting UID=\"31\" BidPageUID=\"20\" BidAreaUID=\"11\" BidAreaSelected=\"2\"/>"
                       "</BidPageSettings></BidPage></BidPages></Bid></XML_ROOT>",
                       &r));
    const std::string page = db.rows("SELECT UID FROM BidPages WHERE Name = 'Sheet'", 1);
    const std::string area2 = db.rows("SELECT UID FROM BidAreas WHERE Name = 'Area 2'", 1);
    // two selected rows (ranks 1 and 2) collapse to the highest ranked one, remapped to the imported Area 2 identity
    CHECK_EQ(db.rows("SELECT BidAreaUID, BidAreaSelected FROM BidPageSettings WHERE BidPageUID = " + page + " AND BidAreaSelected > 0", 2),
             area2 + "|2");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPageSettings"), 1);
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_import_remaps_bid_settings_selected_page_to_imported_page
// and ::test_ost_import_clears_zero_and_blank_selected_page.
TEST(ImportPipeline, Imp06_SelectedPageIsRemappedToTheImportedPage)
{
    ScratchDb db("pipe_selected_page");
    create_import_schema(db);
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" JobName=\"Imported\"><BidSettings><BidSetting UID=\"30\" BidUID=\"1\" BidPageSelectedUID=\"20\"/></BidSettings>"
                       "<BidPages><BidPage UID=\"20\" BidUID=\"1\" Name=\"Sheet\" Sequence=\"1\"/></BidPages></Bid></XML_ROOT>",
                       &r));
    const std::string page = db.rows("SELECT UID FROM BidPages WHERE Name = 'Sheet'", 1);
    CHECK_EQ(db.rows("SELECT BidPageSelectedUID FROM BidSettings", 1), page);
    for (const char *value : {"0", ""})
    {
        SCOPED_CONTEXT(std::string("BidPageSelectedUID='") + value + "'");
        ScratchDb other(std::string("pipe_selected_page_") + (value[0] ? "zero" : "blank"));
        create_import_schema(other);
        ImportResult r2;
        REQUIRE(import_xml(other,
                           std::string("<XML_ROOT><Bid UID=\"1\" JobName=\"Imported\"><BidSettings><BidSetting UID=\"30\" BidUID=\"1\" BidPageSelectedUID=\"") +
                               value + "\"/></BidSettings><BidPages><BidPage UID=\"20\" BidUID=\"1\" Name=\"Sheet\" Sequence=\"1\"/></BidPages></Bid></XML_ROOT>",
                           &r2));
        CHECK_EQ(other.rows("SELECT BidPageSelectedUID FROM BidSettings", 1), std::string("NULL"));
    }
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_import_remaps_estimator_to_imported_employee
// (no target project on iOS: BidProjectUID NULL). The existing Employee 7 is a different person, so the imported one is new.
TEST(ImportPipeline, Imp11_EstimatorIsRemappedToTheImportedEmployee)
{
    ScratchDb db("pipe_estimator");
    create_import_schema(db);
    db.must("INSERT INTO Employees (UID, EmployeeNo, FirstName, LastName) VALUES (7, 'EXISTING', 'Existing', 'Employee')");
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" EstimatorUID=\"7\" JobName=\"Imported\"><BidAreas/><BidPages/></Bid>"
                       "<PayClasses><PayClass UID=\"3\" Name=\"Regular\"/></PayClasses>"
                       "<Employees><Employee UID=\"7\" EmployeeNo=\"E100\" FirstName=\"Alice\" LastName=\"Estimator\" PayClassUID=\"3\"/></Employees></XML_ROOT>",
                       &r));
    const std::string employee = db.rows("SELECT UID FROM Employees WHERE EmployeeNo = 'E100'", 1);
    const std::string pay_class = db.rows("SELECT UID FROM PayClasses WHERE Name = 'Regular'", 1);
    CHECK_NE(employee, std::string("7"));
    CHECK_NE(employee, std::string("NULL"));
    CHECK_EQ(db.rows("SELECT PayClassUID FROM Employees WHERE EmployeeNo = 'E100'", 1), pay_class);
    CHECK_EQ(db.rows("SELECT BidProjectUID, EstimatorUID FROM Bids WHERE JobName = 'Imported'", 2), "NULL|" + employee);
    CHECK_EQ(db.rows("SELECT FirstName FROM Employees WHERE UID = 7", 1), std::string("Existing"));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_import_preserves_bid_employee_through_authoritative_employee_uid
TEST(ImportPipeline, Imp11_BidEmployeeKeepsTheAuthoritativeEmployeeUid)
{
    ScratchDb db("pipe_bid_employee");
    create_import_schema(db);
    db.must_all({"CREATE TABLE BidEmployees (UID INTEGER PRIMARY KEY, BidUID INTEGER, EmployeeUID INTEGER)",
                 "INSERT INTO Employees (UID, EmployeeNo, FirstName, LastName) VALUES (70, 'E100', 'Existing', 'Employee')"});
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" JobName=\"Imported\"><BidEmployees><BidEmployee UID=\"20\" BidUID=\"1\" EmployeeUID=\"7\"/></BidEmployees>"
                       "<BidPages/></Bid><Employees><Employee UID=\"7\" EmployeeNo=\"E100\" FirstName=\"Alice\" LastName=\"Estimator\"/></Employees></XML_ROOT>",
                       &r));
    CHECK_EQ(db.rows("SELECT EmployeeUID FROM BidEmployees", 1), std::string("70"));
    CHECK_EQ(db.rows("SELECT BidUID FROM BidEmployees", 1), r.bid_uid);
    CHECK_EQ(db.rows("SELECT UID FROM Employees", 1), std::string("70"));
}

// IMP-11 (DBP-23 / DBP-M04) on the iOS schema: BidEmployees and PayClasses are stored; Employee.PayClassUID is remapped; a
// re-import of the same employees maps to the existing rows; unmapped manager/estimator references become NULL; a JobStatusUID
// without a JobStatuses section becomes NULL (Windows), not the source UID.
TEST(ImportPipeline, Imp11_Dbp23_Dbp_M04_EmployeesBidEmployeesAndPayClassesAreStoredAndMapped)
{
    ScratchDb db("pipe_employees_ios", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    db.must_all({"INSERT INTO Employees (UID, FirstName, LastName) VALUES (2, 'Bob', 'Existing')",
                 "INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (3, 'Lost', 0, 1)"});
    const std::string xml =
        "<XML_ROOT><Bid UID=\"1\" JobName=\"E\" EstimatorUID=\"2\" PrManagerUID=\"3\" JobSiteManagerUID=\"99\" JobStatusUID=\"3\">"
        "<BidEmployees><BidEmployee UID=\"1\" BidUID=\"1\" EmployeeUID=\"2\" PayClassUID=\"1\"/><BidEmployee UID=\"2\" BidUID=\"1\" EmployeeUID=\"3\"/></BidEmployees></Bid>"
        "<Employees><Employee UID=\"2\" EmployeeNo=\"E1\" FirstName=\"Eve\" LastName=\"Est\" PayClassUID=\"1\"/>"
        "<Employee UID=\"3\" EmployeeNo=\"E2\" FirstName=\"Pat\" LastName=\"Pm\"/></Employees>"
        "<PayClasses><PayClass UID=\"1\" Name=\"Reg\"/></PayClasses></XML_ROOT>";
    ImportResult first, second;
    REQUIRE(import_xml(db, xml, &first));
    const std::string eve = db.rows("SELECT UID FROM Employees WHERE FirstName = 'Eve'", 1);
    const std::string pat = db.rows("SELECT UID FROM Employees WHERE FirstName = 'Pat'", 1);
    const std::string reg = db.rows("SELECT UID FROM PayClasses WHERE Name = 'Reg'", 1);
    CHECK_NE(eve, std::string("2"));
    CHECK_NE(eve, std::string("NULL"));
    CHECK_NE(pat, std::string("NULL"));
    CHECK_EQ(db.rows("SELECT PayClassUID FROM Employees WHERE FirstName = 'Eve'", 1), reg);
    CHECK_EQ(db.rows("SELECT EstimatorUID, PrManagerUID, JobSiteManagerUID, JobStatusUID FROM Bids WHERE UID = " + first.bid_uid, 4),
             eve + "|" + pat + "|NULL|NULL");
    CHECK_EQ(db.rows("SELECT EmployeeUID, PayClassUID FROM BidEmployees WHERE BidUID = " + first.bid_uid + " ORDER BY UID", 2),
             eve + "|" + reg + ";" + pat + "|NULL");
    // a re-import maps to the existing employees and pay class (business key), nothing new is inserted
    REQUIRE(import_xml(db, xml, &second));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Employees"), 3);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM PayClasses"), 1);
    CHECK_EQ(db.rows("SELECT EstimatorUID, PrManagerUID FROM Bids WHERE UID = " + second.bid_uid, 2), eve + "|" + pat);
    CHECK_EQ(db.rows("SELECT UID FROM Employees WHERE FirstName = 'Bob'", 1), std::string("2"));
}

// IMP-10 test plan (d)/(e) through the pipeline: Bid JobStatusUID with no JobStatuses section becomes NULL, a mapped one is
// remapped, an unmapped CdnTypeUID stays as it is.
TEST(ImportPipeline, Imp10_MasterReferencesFollowTheWindowsMapRules)
{
    ScratchDb db("pipe_master_refs", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    ImportResult r;
    REQUIRE(import_xml(db,
                       "<XML_ROOT><Bid UID=\"1\" JobName=\"M\" JobStatusUID=\"7\"><BidConditions>"
                       "<BidCondition UID=\"10\" BidUID=\"1\" Name=\"mapped\" CdnTypeUID=\"4\"/>"
                       "<BidCondition UID=\"11\" BidUID=\"1\" Name=\"unmapped\" CdnTypeUID=\"13\"/></BidConditions></Bid>"
                       "<CdnTypes><CdnType UID=\"4\" Name=\"Walls\"/></CdnTypes></XML_ROOT>",
                       &r));
    CHECK_EQ(db.rows("SELECT JobStatusUID FROM Bids WHERE UID = " + r.bid_uid, 1), std::string("NULL"));
    const std::string walls = db.rows("SELECT UID FROM CdnTypes WHERE Name = 'Walls'", 1);
    CHECK_EQ(db.rows("SELECT CdnTypeUID FROM BidConditions WHERE BidUID = " + r.bid_uid + " ORDER BY Name", 1), walls + ";13");
}

// ---------------------------------------------------------------------------------------------------------------
// the writer (_write_to_db)
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    // Creates [table] (UID INTEGER) with an AFTER INSERT trigger that appends the table name to _order.
    void ordered_table(const ScratchDb &db, const std::string &table)
    {
        db.must("CREATE TABLE [" + table + "] (UID INTEGER)");
        db.must("CREATE TRIGGER [log_" + table + "] AFTER INSERT ON [" + table + "] BEGIN INSERT INTO _order (t) VALUES ('" + table + "'); END");
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_write_order_is_the_documented_dependency_order_for_every_table
// (the tables are supplied in reverse, so only the production order can produce the expected sequence; the first page section
// is empty on purpose). Includes BidEmployees between BidAreas.. and BidLayers like schema_contract.BID_SECTIONS.
TEST(ImportWriter, Imp08_WriteOrderIsTheDocumentedDependencyOrderForEveryTable)
{
    const std::vector<std::string> bid_order = {"BidPlanRooms", "BidAreas", "BidTypAreas", "BidConditionFolders", "BidPageFolders",
                                                "BidEmployees", "BidLayers", "BidTypAreaCounts", "BidConditions",
                                                "UserMasterConditions", "BidConditionUser", "BidZones", "OCRProps", "BidPages",
                                                "BidSettings", "BidTypGroupViews", "BidNamedViews", "BidHotLinks"};
    const std::vector<std::string> page_order = {"BidTakeoffs", "BidHighlights", "BidTexts", "BidDimensions", "BidArrows",
                                                 "BidALines", "BidCallOuts", "BidAnnotationRects", "BidAnnotationOvals",
                                                 "BidAnnotationPolygons", "BidAnnotationClouds", "BidAnnoInk", "BidLegends",
                                                 "BidPageSettings", "BidAreaTranslations", "BidMarkedPages", "BidComments"};
    ScratchDb db("writer_order");
    db.must("CREATE TABLE _order (seq INTEGER PRIMARY KEY AUTOINCREMENT, t TEXT)");
    ordered_table(db, "Bids");
    for (const std::string &t : bid_order)
        ordered_table(db, t);
    for (const std::string &t : page_order)
        ordered_table(db, t);

    OstRawData remapped;
    remapped.bid_row = row({{"UID", "1"}});
    for (auto it = bid_order.rbegin(); it != bid_order.rend(); ++it)
        remapped.bid_tables[*it] = {row({{"UID", "1"}})};
    for (auto it = page_order.rbegin(); it != page_order.rend(); ++it)
        remapped.page_tables[*it] = {row({{"UID", "1"}})};
    remapped.page_tables["BidTakeoffs"] = {};

    ost_itest::Connection conn(db);
    std::string error;
    REQUIRE(ost_store::import_write_to_db(conn.get(), remapped, error));

    std::vector<std::string> expected = {"Bids"};
    expected.insert(expected.end(), bid_order.begin(), bid_order.end());
    expected.insert(expected.end(), page_order.begin() + 1, page_order.end());
    std::string expected_text, actual = db.rows("SELECT t FROM _order ORDER BY seq", 1);
    for (const std::string &t : expected)
        expected_text += (expected_text.empty() ? "" : ";") + t;
    CHECK_EQ(actual, expected_text);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_rows_for_tables_outside_the_write_order_are_not_written
TEST(ImportWriter, Imp08_RowsForTablesOutsideTheWriteOrderAreNotWritten)
{
    ScratchDb db("writer_outside_order");
    db.must_all({"CREATE TABLE Bids (UID INTEGER)", "CREATE TABLE Unlisted (UID INTEGER)"});
    OstRawData remapped;
    remapped.bid_row = row({{"UID", "1"}});
    remapped.bid_tables["Unlisted"] = {row({{"UID", "2"}})};
    remapped.page_tables["Unlisted"] = {row({{"UID", "3"}})};
    ost_itest::Connection conn(db);
    std::string error;
    CHECK(ost_store::import_write_to_db(conn.get(), remapped, error));
    CHECK_EQ(db.rows("SELECT UID FROM Bids", 1), std::string("1"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Unlisted"), 0);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_every_table_row_is_stored_with_that_tables_own_columns_and_types
TEST(ImportWriter, Imp05_Imp08_EveryTableRowIsStoredWithThatTablesOwnColumnsAndTypes)
{
    ScratchDb db("writer_own_columns");
    create_import_schema(db);
    db.must("CREATE TABLE BidTakeoffs (UID INTEGER PRIMARY KEY, BidPageUID INTEGER, Quantity REAL)");
    OstRawData remapped;
    remapped.bid_row = row({{"UID", "5"}, {"JobName", "Imported"}, {"Bogus", "dropped"}});
    remapped.bid_tables["BidAreas"] = {row({{"UID", "30"}, {"BidUID", "5"}, {"ParentUID", "NULL"}, {"Name", "Zone"}, {"Sequence", "2"},
                                            {"GUID", "g-1"}, {"Bogus", "dropped"}})};
    remapped.bid_tables["BidPages"] = {row({{"UID", "20"}, {"BidUID", "5"}, {"Name", "Sheet"}, {"Sequence", "1"},
                                            {"ParentUID", "dropped: not a BidPages column"}})};
    remapped.page_tables["BidTakeoffs"] = {row({{"UID", "40"}, {"BidPageUID", "20"}, {"Quantity", "2.5"}, {"Name", "x"}})};
    remapped.page_tables["BidPageSettings"] = {row({{"UID", "50"}, {"BidPageUID", "20"}, {"BidAreaUID", "30"}, {"BidTypAreaUID", ""},
                                                    {"BidAreaSelected", "-1"}})};
    ost_itest::Connection conn(db);
    std::string error;
    REQUIRE(ost_store::import_write_to_db(conn.get(), remapped, error));
    CHECK_EQ(db.rows("SELECT UID, BidProjectUID, EstimatorUID, JobStatusUID, JobName, BidNo FROM Bids", 6), std::string("5|NULL|NULL|NULL|Imported|NULL"));
    CHECK_EQ(db.rows("SELECT UID, BidUID, ParentUID, Name, Sequence, GUID FROM BidAreas", 6), std::string("30|5|NULL|Zone|2|g-1"));
    CHECK_EQ(db.rows("SELECT UID, BidUID, Name, Sequence FROM BidPages", 4), std::string("20|5|Sheet|1"));
    CHECK_EQ(db.rows("SELECT UID, BidPageUID, Quantity, typeof(Quantity) FROM BidTakeoffs", 4), std::string("40|20|2.5|real"));
    CHECK_EQ(db.rows("SELECT UID, BidPageUID, BidAreaUID, BidTypAreaUID, BidAreaSelected FROM BidPageSettings", 5), std::string("50|20|30|NULL|-1"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_bid_table_rows_are_inserted_parent_before_child
// and ::test_page_table_rows_are_inserted_parent_before_child (foreign keys are enforced on iOS connections).
TEST(ImportWriter, Imp08_RowsOfOneTableAreWrittenInTheGivenOrderParentsFirst)
{
    ScratchDb db("writer_parent_first");
    db.must_all({"CREATE TABLE Bids (UID INTEGER)",
                 "CREATE TABLE BidAreas (UID INTEGER PRIMARY KEY, BidUID INTEGER, ParentUID INTEGER REFERENCES BidAreas (UID))",
                 "CREATE TABLE BidTakeoffs (UID INTEGER PRIMARY KEY, ParentUID INTEGER REFERENCES BidTakeoffs (UID))"});
    OstRawData remapped;
    remapped.bid_row = row({{"UID", "5"}});
    remapped.bid_tables["BidAreas"] = {row({{"UID", "30"}, {"BidUID", "5"}, {"ParentUID", "NULL"}}),
                                       row({{"UID", "31"}, {"BidUID", "5"}, {"ParentUID", "30"}})};
    remapped.page_tables["BidTakeoffs"] = {row({{"UID", "40"}, {"ParentUID", "NULL"}}), row({{"UID", "41"}, {"ParentUID", "40"}})};
    ost_itest::Connection conn(db);
    std::string error;
    CHECK(ost_store::import_write_to_db(conn.get(), remapped, error));
    CHECK_EQ(db.rows("SELECT UID, ParentUID FROM BidAreas ORDER BY rowid", 2), std::string("30|NULL;31|30"));
    CHECK_EQ(db.rows("SELECT UID, ParentUID FROM BidTakeoffs ORDER BY rowid", 2), std::string("40|NULL;41|40"));
}
