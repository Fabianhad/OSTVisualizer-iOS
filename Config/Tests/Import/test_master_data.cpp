// IMP-10 (CdnTypes / JobStatuses reconciliation) and IMP-11 (Employees, AccessLevels, PayClasses reconciliation).
//
// Windows: ost_visualizer/infrastructure/mdb/components/import_operations.py (_resolve_cdn_types, _resolve_job_statuses,
// _resolve_access_levels, _resolve_pay_classes, _resolve_employees, _employee_identity_key) and
// infrastructure/database/master_data_identity.py. Every test is a port of the Windows test named in its comment; the
// Windows tests run on small hand-written SQLite schemas (tests/helpers/mdb/import_export_support.py), as here.
#include "ImportTestSupport.hpp"

#include "Persistence/ProjectStore/ProjectStore_ImportInternal.hpp"

using ost_import::OstRawData;
using ost_import::UidMap;
using ost_itest::Connection;
using ost_itest::row;
using ost_itest::ScratchDb;

namespace
{
    void create_import_schema(const ScratchDb &db)
    {
        db.must_all({
            "CREATE TABLE Settings (NextBidNo INTEGER)",
            "INSERT INTO Settings (NextBidNo) VALUES (1)",
            "CREATE TABLE Bids (UID INTEGER PRIMARY KEY, BidProjectUID INTEGER, EstimatorUID INTEGER, "
            "JobStatusUID INTEGER, JobName TEXT, BidNo INTEGER)",
            "CREATE TABLE Employees (UID INTEGER PRIMARY KEY, EmployeeNo TEXT, FirstName TEXT, LastName TEXT, "
            "PayClassUID INTEGER)",
            "CREATE TABLE PayClasses (UID INTEGER PRIMARY KEY, Name TEXT)",
            "CREATE TABLE AccessLevels (UID INTEGER PRIMARY KEY, Name TEXT)",
            "CREATE TABLE CdnTypes (UID INTEGER PRIMARY KEY, Name TEXT, ExpandState INTEGER)",
            "CREATE TABLE JobStatuses (UID INTEGER PRIMARY KEY, Name TEXT, Locked INTEGER, Sequence INTEGER)",
        });
    }

    // AccessLevels with the Description column the importer keys on.
    void access_levels_with_description(const ScratchDb &db)
    {
        db.must_all({"DROP TABLE AccessLevels", "CREATE TABLE AccessLevels (UID INTEGER PRIMARY KEY, Description TEXT)"});
    }

    std::string show(const UidMap &map)
    {
        std::vector<std::string> keys;
        for (const auto &entry : map)
            keys.push_back(entry.first);
        std::sort(keys.begin(), keys.end());
        std::string out = "{";
        for (size_t i = 0; i < keys.size(); ++i)
            out += (i ? ", " : "") + keys[i] + ":" + map.at(keys[i]);
        return out + "}";
    }

    std::string ambiguity(const std::string &label, const std::string &uids)
    {
        return "Imported master-data identity is ambiguous for " + label + ": matching UIDs " + uids + ".";
    }

    struct Resolved
    {
        bool ok = false;
        std::string error;
        UidMap map;
        long long max_uid = 0;
    };

    OstRawData raw_with(const std::string &table, ost_import::RowList rows)
    {
        OstRawData raw;
        raw.bid_row = row({{"UID", "1"}});
        raw.global_tables[table] = std::move(rows);
        return raw;
    }

    Resolved resolve(const ScratchDb &db, const std::string &table, const OstRawData &raw, long long max_uid = 100)
    {
        Connection conn(db);
        Resolved r;
        r.max_uid = max_uid;
        if (table == "CdnTypes")
            r.ok = ost_store::import_resolve_cdn_types(conn.get(), raw, r.max_uid, r.map, r.error);
        else if (table == "JobStatuses")
            r.ok = ost_store::import_resolve_job_statuses(conn.get(), raw, r.max_uid, r.map, r.error);
        else if (table == "AccessLevels")
            r.ok = ost_store::import_resolve_access_levels(conn.get(), raw, r.map, r.error);
        else if (table == "PayClasses")
            r.ok = ost_store::import_resolve_pay_classes(conn.get(), raw, r.map, r.error);
        else
            r.ok = ost_store::import_resolve_employees(conn.get(), raw, {}, {}, r.map, r.error);
        return r;
    }

    Resolved resolve_employees(const ScratchDb &db, const OstRawData &raw, const UidMap &pay, const UidMap &access)
    {
        Connection conn(db);
        Resolved r;
        r.ok = ost_store::import_resolve_employees(conn.get(), raw, pay, access, r.map, r.error);
        return r;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// ambiguity / duplicates (the import is rolled back by the caller)
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rejects_ambiguous_job_status_name
TEST(ImportMasterData, Imp10_AmbiguousExistingJobStatusNameIsRejected)
{
    ScratchDb db("md_js_ambiguous");
    create_import_schema(db);
    db.must_all({"INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (10, 'Open', 0, 1)",
                 "INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (11, 'Open', 1, 2)"});
    const Resolved r = resolve(db, "JobStatuses",
                               raw_with("JobStatuses", {row({{"UID", "90"}, {"Name", "Open"}, {"Locked", "0"}, {"Sequence", "1"}})}));
    CHECK_FALSE(r.ok);
    CHECK_STR_EQ(r.error, ambiguity("JobStatuses.Name", "10, 11"));
    CHECK_EQ(db.rows("SELECT UID FROM JobStatuses ORDER BY UID", 1), std::string("10;11"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rejects_duplicate_target_master_uid
TEST(ImportMasterData, Imp10_DuplicateTargetMasterUidIsRejected)
{
    ScratchDb db("md_dup_target_uid");
    create_import_schema(db);
    db.must_all({"DROP TABLE CdnTypes", "CREATE TABLE CdnTypes (UID INTEGER, Name TEXT, ExpandState INTEGER)",
                 "INSERT INTO CdnTypes (UID, Name) VALUES (7, 'Concrete')",
                 "INSERT INTO CdnTypes (UID, Name) VALUES (7, 'Conflicting')"});
    const Resolved r = resolve(db, "CdnTypes", raw_with("CdnTypes", {row({{"UID", "90"}, {"Name", "Concrete"}})}));
    CHECK_FALSE(r.ok);
    CHECK(r.error.find("CdnTypes contains duplicate UID 7") != std::string::npos);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rejects_ambiguous_pay_class_name
TEST(ImportMasterData, Imp11_AmbiguousExistingPayClassNameIsRejected)
{
    ScratchDb db("md_pc_ambiguous");
    create_import_schema(db);
    db.must_all({"INSERT INTO PayClasses (UID, Name) VALUES (20, 'Regular')", "INSERT INTO PayClasses (UID, Name) VALUES (21, 'Regular')"});
    const Resolved r = resolve(db, "PayClasses", raw_with("PayClasses", {row({{"UID", "91"}, {"Name", "Regular"}})}));
    CHECK_FALSE(r.ok);
    CHECK_STR_EQ(r.error, ambiguity("PayClasses.Name", "20, 21"));
    CHECK_EQ(db.rows("SELECT UID FROM PayClasses ORDER BY UID", 1), std::string("20;21"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rejects_ambiguous_access_level_description
TEST(ImportMasterData, Imp11_AmbiguousExistingAccessLevelDescriptionIsRejected)
{
    ScratchDb db("md_al_ambiguous");
    create_import_schema(db);
    db.must_all({"DROP TABLE AccessLevels", "CREATE TABLE AccessLevels (UID INTEGER PRIMARY KEY, Description TEXT, Privileges INTEGER)",
                 "INSERT INTO AccessLevels (UID, Description) VALUES (15, 'Full')",
                 "INSERT INTO AccessLevels (UID, Description) VALUES (16, 'Full')"});
    const Resolved r = resolve(db, "AccessLevels", raw_with("AccessLevels", {row({{"UID", "96"}, {"Description", "Full"}})}));
    CHECK_FALSE(r.ok);
    CHECK_STR_EQ(r.error, ambiguity("AccessLevels.Description", "15, 16"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rejects_duplicate_incoming_pay_class_name
TEST(ImportMasterData, Imp11_DuplicateIncomingPayClassNameIsRejected)
{
    ScratchDb db("md_pc_dup_incoming");
    create_import_schema(db);
    const Resolved r = resolve(db, "PayClasses",
                               raw_with("PayClasses", {row({{"UID", "91"}, {"Name", "Regular"}}), row({{"UID", "92"}, {"Name", "Regular"}})}));
    CHECK_FALSE(r.ok);
    CHECK_STR_EQ(r.error, ambiguity("PayClasses.Name", "91, 92"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM PayClasses"), 0);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rejects_ambiguous_employee_business_key
TEST(ImportMasterData, Imp11_AmbiguousExistingEmployeeBusinessKeyIsRejected)
{
    ScratchDb db("md_emp_ambiguous");
    create_import_schema(db);
    db.must_all({"INSERT INTO Employees (UID, EmployeeNo, FirstName, LastName) VALUES (30, 'E100', 'Alice', 'One')",
                 "INSERT INTO Employees (UID, EmployeeNo, FirstName, LastName) VALUES (31, 'E100', 'Alex', 'Two')"});
    const Resolved r = resolve_employees(
        db,
        raw_with("Employees", {row({{"UID", "92"}, {"EmployeeNo", "e100"}, {"FirstName", "Imported"}, {"LastName", "Employee"}})}),
        {}, {});
    CHECK_FALSE(r.ok);
    CHECK_STR_EQ(r.error, ambiguity("Employees business key", "30, 31"));
    CHECK_EQ(db.rows("SELECT UID FROM Employees ORDER BY UID", 1), std::string("30;31"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_incoming_duplicates_are_rejected_before_anything_is_inserted
TEST(ImportMasterData, Imp10_Imp11_IncomingDuplicatesAreRejectedBeforeAnythingIsInserted)
{
    struct Case
    {
        const char *table;
        bool access_levels_with_description;
        ost_import::RowList rows;
        const char *label;
        const char *uids;
    };
    const std::vector<Case> cases = {
        {"CdnTypes", false, {row({{"UID", "93"}, {"Name", "Concrete"}}), row({{"UID", "94"}, {"Name", " concrete "}})}, "CdnTypes.Name", "93, 94"},
        {"JobStatuses", false, {row({{"UID", "7"}, {"Name", "Open"}}), row({{"UID", "8"}, {"Name", "Open"}})}, "JobStatuses.Name", "7, 8"},
        {"AccessLevels", true, {row({{"UID", "96"}, {"Description", "Full"}}), row({{"UID", "97"}, {"Description", "Full"}})}, "AccessLevels.Description", "96, 97"},
        {"Employees", false, {row({{"UID", "90"}, {"EmployeeNo", "E1"}}), row({{"UID", "91"}, {"EmployeeNo", " e1 "}})}, "Employees business key", "90, 91"},
        {"Employees", false,
         {row({{"UID", "90"}, {"FirstName", "Ann"}, {"LastName", "Lee"}}), row({{"UID", "91"}, {"FirstName", " ANN"}, {"LastName", "lee "}})},
         "Employees business key", "90, 91"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string(c.table) + " / " + c.label);
        ScratchDb db(std::string("md_incoming_dups_") + c.table);
        create_import_schema(db);
        if (c.access_levels_with_description)
            access_levels_with_description(db);
        const Resolved r = resolve(db, c.table, raw_with(c.table, c.rows));
        CHECK_FALSE(r.ok);
        CHECK_STR_EQ(r.error, ambiguity(c.label, c.uids));
        CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM ") + c.table), 0);
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_nameless_condition_types_and_job_statuses_are_one_identity
TEST(ImportMasterData, Imp10_NamelessCondTypesAndJobStatusesAreOneIdentity)
{
    for (const char *table : {"CdnTypes", "JobStatuses"})
    {
        SCOPED_CONTEXT(std::string("table ") + table);
        ScratchDb db(std::string("md_nameless_") + table);
        create_import_schema(db);
        const Resolved r = resolve(db, table, raw_with(table, {row({{"UID", "1"}}), row({{"UID", "2"}})}));
        CHECK_FALSE(r.ok);
        CHECK_STR_EQ(r.error, ambiguity(std::string(table) + ".Name", "1, 2"));
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rolls_back_ambiguous_master_reconciliation
// (the master data half: the existing duplicate names are detected only when an incoming row looks them up).
TEST(ImportMasterData, Imp10_ExistingDuplicatesOnlyMatterWhenLookedUp)
{
    ScratchDb db("md_existing_dups_unused");
    create_import_schema(db);
    db.must_all({"INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (10, 'Open', 0, 1)",
                 "INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (11, 'Open', 1, 2)"});
    const Resolved r = resolve(db, "JobStatuses", raw_with("JobStatuses", {row({{"UID", "7"}, {"Name", "Won"}})}));
    CHECK(r.ok);
    CHECK_EQ(show(r.map), std::string("{7:101}"));
}

// ---------------------------------------------------------------------------------------------------------------
// reuse / insert above the floor
// ---------------------------------------------------------------------------------------------------------------

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_uses_condition_type_name_contract
TEST(ImportMasterData, Imp10_CondTypeNameMatchesAfterStripAndLower)
{
    ScratchDb db("md_cdn_contract");
    create_import_schema(db);
    db.must("INSERT INTO CdnTypes (UID, Name, ExpandState) VALUES (40, ' Concrete ', 0)");
    const Resolved r = resolve(db, "CdnTypes",
                               raw_with("CdnTypes", {row({{"UID", "93"}, {"Name", "concrete"}, {"ExpandState", "0"}})}));
    CHECK(r.ok);
    CHECK_EQ(show(r.map), std::string("{93:40}"));
    CHECK_EQ(r.max_uid, 100);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM CdnTypes"), 1);
}

// DBP-25 alias test plan: existing CdnTypes (1,'Walls'); an incoming ' walls ' maps to UID 1 without a new row.
TEST(ImportMasterData, Imp10_Dbp25_ExistingWallsIsReusedForAPaddedLowerCaseName)
{
    ScratchDb db("md_cdn_walls");
    create_import_schema(db);
    db.must("INSERT INTO CdnTypes (UID, Name, ExpandState) VALUES (1, 'Walls', 0)");
    const Resolved r = resolve(db, "CdnTypes", raw_with("CdnTypes", {row({{"UID", "9"}, {"Name", " walls "}})}), 0);
    CHECK(r.ok);
    CHECK_EQ(show(r.map), std::string("{9:1}"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM CdnTypes"), 1);
}

// The key is Unicode-aware like Python's str.lower(): accents and case must not make a second condition type.
TEST(ImportMasterData, Imp10_CondTypeKeyFoldsNonAsciiCaseLikePython)
{
    ScratchDb db("md_cdn_unicode");
    create_import_schema(db);
    db.must("INSERT INTO CdnTypes (UID, Name, ExpandState) VALUES (40, 'Funda\xc3\xa7\xc3\xa3o', 0)");
    const Resolved r = resolve(db, "CdnTypes",
                               raw_with("CdnTypes", {row({{"UID", "93"}, {"Name", " FUNDA\xc3\x87\xc3\x83O "}})}));
    CHECK(r.ok);
    CHECK_EQ(show(r.map), std::string("{93:40}"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM CdnTypes"), 1);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_rejects_ambiguous_condition_type_name_contract
TEST(ImportMasterData, Imp10_AmbiguousCondTypeNameContractIsRejected)
{
    ScratchDb db("md_cdn_ambiguous");
    create_import_schema(db);
    db.must_all({"INSERT INTO CdnTypes (UID, Name, ExpandState) VALUES (40, 'Concrete', 0)",
                 "INSERT INTO CdnTypes (UID, Name, ExpandState) VALUES (41, ' concrete ', 0)"});
    const Resolved r = resolve(db, "CdnTypes",
                               raw_with("CdnTypes", {row({{"UID", "93"}, {"Name", "CONCRETE"}, {"ExpandState", "0"}})}));
    CHECK_FALSE(r.ok);
    CHECK_STR_EQ(r.error, "Imported master-data identity is ambiguous for CdnTypes.Name: matching UIDs 40, 41.");
    CHECK_EQ(db.rows("SELECT UID FROM CdnTypes ORDER BY UID", 1), std::string("40;41"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_inserts_new_master_rows_above_the_uid_floor
TEST(ImportMasterData, Imp10_NewMasterRowsAreInsertedAboveTheUidFloor)
{
    ScratchDb db("md_above_floor");
    create_import_schema(db);
    db.must_all({"INSERT INTO CdnTypes (UID, Name, ExpandState) VALUES (40, 'Existing', 0)",
                 "INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (50, 'Open', 0, 1)"});
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.global_tables["CdnTypes"] = {row({{"UID", "93"}, {"Name", "Existing"}, {"ExpandState", "0"}}),
                                     row({{"UID", "94"}, {"Name", "Footings"}, {"ExpandState", "-1"}}),
                                     row({{"UID", "95"}, {"Name", "Slabs"}, {"ExpandState", ""}})};
    raw.global_tables["JobStatuses"] = {row({{"UID", "7"}, {"Name", "Open"}, {"Locked", "0"}, {"Sequence", "1"}}),
                                        row({{"UID", "8"}, {"Name", "Won"}, {"Locked", "1"}, {"Sequence", "2"}})};
    Connection conn(db);
    long long max_uid = 100;
    UidMap cdn_map, status_map;
    std::string error;
    CHECK(ost_store::import_resolve_cdn_types(conn.get(), raw, max_uid, cdn_map, error));
    CHECK_EQ(show(cdn_map), std::string("{93:40, 94:101, 95:102}"));
    CHECK_EQ(max_uid, 102);
    CHECK(ost_store::import_resolve_job_statuses(conn.get(), raw, max_uid, status_map, error));
    CHECK_EQ(show(status_map), std::string("{7:50, 8:103}"));
    CHECK_EQ(max_uid, 103);
    CHECK_EQ(db.rows("SELECT UID, Name, ExpandState FROM CdnTypes ORDER BY UID", 3),
             std::string("40|Existing|0;101|Footings|-1;102|Slabs|NULL"));
    // JobStatuses.Locked is a YESNO column of the Windows schema (the Windows test declares it INTEGER in its SQLite
    // stand-in and so sees 1): '1' is stored as -1 like Access does.
    CHECK_EQ(db.rows("SELECT UID, Name, Locked, Sequence FROM JobStatuses ORDER BY UID", 4),
             std::string("50|Open|0|1;103|Won|-1|2"));
    // Re-resolving the same payload reuses the rows inserted above.
    UidMap again;
    long long again_max = 103;
    CHECK(ost_store::import_resolve_cdn_types(conn.get(), raw, again_max, again, error));
    CHECK_EQ(show(again), std::string("{93:40, 94:101, 95:102}"));
    CHECK_EQ(again_max, 103);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_empty_or_missing_master_tables_return_empty_maps
TEST(ImportMasterData, Imp11_EmptyOrMissingMasterTablesReturnEmptyMaps)
{
    ScratchDb db("md_missing_tables");
    create_import_schema(db);
    db.must_all({"DROP TABLE PayClasses", "DROP TABLE AccessLevels", "DROP TABLE Employees"});
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.global_tables["PayClasses"] = {row({{"UID", "1"}, {"Name", "Regular"}})};
    raw.global_tables["AccessLevels"] = {row({{"UID", "2"}, {"Description", "Full"}})};
    raw.global_tables["Employees"] = {row({{"UID", "3"}, {"EmployeeNo", "E1"}})};
    CHECK(resolve(db, "PayClasses", raw).map.empty());
    CHECK(resolve(db, "PayClasses", raw).ok);
    CHECK(resolve(db, "AccessLevels", raw).map.empty());
    CHECK(resolve(db, "AccessLevels", raw).ok);
    CHECK(resolve_employees(db, raw, {}, {}).map.empty());
    CHECK(resolve_employees(db, raw, {}, {}).ok);
    OstRawData empty;
    empty.bid_row = row({{"UID", "1"}});
    const Resolved cdn = resolve(db, "CdnTypes", empty, 7);
    CHECK(cdn.ok);
    CHECK(cdn.map.empty());
    CHECK_EQ(cdn.max_uid, 7);
    const Resolved js = resolve(db, "JobStatuses", empty, 7);
    CHECK(js.map.empty());
    CHECK_EQ(js.max_uid, 7);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_employees_reuse_business_key_and_remap_references
TEST(ImportMasterData, Imp11_EmployeesReuseTheBusinessKeyAndRemapReferences)
{
    ScratchDb db("md_emp_reuse");
    create_import_schema(db);
    db.must_all({"DROP TABLE Employees",
                 "CREATE TABLE Employees (UID INTEGER, EmployeeNo TEXT, FirstName TEXT, LastName TEXT, EMail TEXT, "
                 "PayClassUID INTEGER, AccessLevelUID INTEGER)",
                 "INSERT INTO Employees VALUES (4, 'E1', 'Ava', 'Lee', NULL, NULL, NULL)",
                 "INSERT INTO Employees VALUES (8, NULL, 'Bo', 'Kay', NULL, NULL, NULL)"});
    OstRawData raw = raw_with(
        "Employees",
        {row({{"UID", "90"}, {"EmployeeNo", " e1 "}, {"FirstName", "Renamed"}}),
         row({{"UID", "91"}, {"EmployeeNo", ""}, {"FirstName", "Mia"}, {"LastName", "Ray"}, {"EMail", "mia@example.test"},
              {"PayClassUID", "70"}, {"AccessLevelUID", "80"}}),
         row({{"UID", "92"}, {"EmployeeNo", "E3"}, {"FirstName", "Zed"}, {"PayClassUID", "71"}, {"AccessLevelUID", "81"}}),
         row({{"UID", "93"}, {"EmployeeNo", ""}, {"FirstName", ""}, {"LastName", ""}}),
         row({{"UID", "94"}, {"FirstName", " BO "}, {"LastName", "kay"}})});
    const Resolved r = resolve_employees(db, raw, {{"70", "12"}}, {{"81", "13"}});
    CHECK(r.ok);
    CHECK_EQ(show(r.map), std::string("{90:4, 91:9, 92:10, 93:11, 94:8}"));
    CHECK_EQ(db.rows("SELECT UID, EmployeeNo, FirstName, LastName, EMail, PayClassUID, AccessLevelUID FROM Employees ORDER BY UID", 7),
             std::string("4|E1|Ava|Lee|NULL|NULL|NULL;8|NULL|Bo|Kay|NULL|NULL|NULL;9||Mia|Ray|mia@example.test|12|NULL;"
                         "10|E3|Zed|NULL|NULL|NULL|13;11||||NULL|NULL|NULL"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_pay_classes_and_access_levels_reuse_names_and_insert_rest
TEST(ImportMasterData, Imp11_PayClassesAndAccessLevelsReuseNamesAndInsertTheRest)
{
    ScratchDb db("md_pc_al_reuse");
    create_import_schema(db);
    db.must_all({"DROP TABLE AccessLevels", "CREATE TABLE AccessLevels (UID INTEGER, Description TEXT, Privileges INTEGER)",
                 "INSERT INTO PayClasses VALUES (20, 'Regular')", "INSERT INTO AccessLevels VALUES (15, 'Full', 7)"});
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.global_tables["PayClasses"] = {row({{"UID", "91"}, {"Name", "Regular"}}), row({{"UID", "92"}, {"Name", "Overtime"}}),
                                       row({{"UID", "93"}, {"Name", ""}}), row({{"UID", "94"}, {"Name", ""}})};
    raw.global_tables["AccessLevels"] = {row({{"UID", "96"}, {"Description", "Full"}, {"Privileges", "1"}}),
                                         row({{"UID", "97"}, {"Description", "Read"}, {"Privileges", "2"}}),
                                         row({{"UID", "98"}, {"Description", ""}, {"Privileges", "3"}})};
    CHECK_EQ(show(resolve(db, "PayClasses", raw).map), std::string("{91:20, 92:21, 93:22, 94:23}"));
    CHECK_EQ(show(resolve(db, "AccessLevels", raw).map), std::string("{96:15, 97:16, 98:17}"));
    CHECK_EQ(db.rows("SELECT UID, Name FROM PayClasses ORDER BY UID", 2), std::string("20|Regular;21|Overtime;22|;23|"));
    CHECK_EQ(db.rows("SELECT UID, Description, Privileges FROM AccessLevels ORDER BY UID", 3),
             std::string("15|Full|7;16|Read|2;17||3"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_access_import_master_inserts_preserve_dangling_reference_uids
TEST(ImportMasterData, Imp11_NewMasterUidsStayAboveDanglingReferences)
{
    struct Case
    {
        const char *table;
        std::vector<std::string> setup;
        OstRawData raw;
    };
    std::vector<Case> cases;
    {
        OstRawData raw = raw_with("AccessLevels", {row({{"UID", "90"}, {"Description", "New"}})});
        cases.push_back({"AccessLevels",
                         {"CREATE TABLE AccessLevels (UID INTEGER, Description TEXT)", "CREATE TABLE Employees (UID INTEGER, AccessLevelUID INTEGER)",
                          "INSERT INTO AccessLevels VALUES (1, 'Existing')", "INSERT INTO Employees VALUES (10, 2)"},
                         raw});
    }
    {
        OstRawData raw = raw_with("PayClasses", {row({{"UID", "90"}, {"Name", "New"}})});
        cases.push_back({"PayClasses",
                         {"CREATE TABLE PayClasses (UID INTEGER, Name TEXT)", "CREATE TABLE Employees (UID INTEGER, PayClassUID INTEGER)",
                          "INSERT INTO PayClasses VALUES (1, 'Existing')", "INSERT INTO Employees VALUES (10, 2)"},
                         raw});
    }
    {
        OstRawData raw = raw_with("Employees", {row({{"UID", "90"}, {"EmployeeNo", "E2"}, {"FirstName", "Mia"}, {"LastName", "Ray"}})});
        cases.push_back({"Employees",
                         {"CREATE TABLE Employees (UID INTEGER, EmployeeNo TEXT, FirstName TEXT, LastName TEXT)",
                          "CREATE TABLE Bids (UID INTEGER, EstimatorUID INTEGER)", "INSERT INTO Employees VALUES (1, 'E1', 'Ava', 'Lee')",
                          "INSERT INTO Bids VALUES (10, 2)"},
                         raw});
    }
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.table);
        ScratchDb db(std::string("md_dangling_") + c.table);
        db.must_all(c.setup);
        const Resolved r = std::string(c.table) == "Employees" ? resolve_employees(db, c.raw, {}, {}) : resolve(db, c.table, c.raw);
        CHECK(r.ok);
        CHECK_EQ(show(r.map), std::string("{90:3}"));
        CHECK_EQ(db.rows(std::string("SELECT [UID] FROM [") + c.table + "] ORDER BY [UID]", 1), std::string("1;3"));
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_table_uid_allocation_closes_its_cursor_and_respects_references
TEST(ImportMasterData, Imp11_NextTableUidRespectsReferencesFromChildTables)
{
    ScratchDb db("md_next_table_uid");
    db.must_all({"CREATE TABLE PayClasses (UID INTEGER, Name TEXT)", "INSERT INTO PayClasses VALUES (3, 'A')",
                 "CREATE TABLE Employees (UID INTEGER, PayClassUID INTEGER)", "INSERT INTO Employees VALUES (1, 9)"});
    Connection conn(db);
    long long next = 0;
    std::string error;
    CHECK(ost_store::import_next_table_uid(conn.get(), "PayClasses", next, error));
    CHECK_EQ(next, 10);
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_empty_identities_never_collide_for_access_levels_and_employees
TEST(ImportMasterData, Imp11_EmptyIdentitiesNeverCollide)
{
    ScratchDb db("md_empty_identities");
    create_import_schema(db);
    access_levels_with_description(db);
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.global_tables["AccessLevels"] = {row({{"UID", "96"}}), row({{"UID", "97"}, {"Description", ""}})};
    raw.global_tables["Employees"] = {row({{"UID", "90"}, {"EmployeeNo", ""}, {"FirstName", " "}}), row({{"UID", "91"}})};
    CHECK_EQ(show(resolve(db, "AccessLevels", raw).map), std::string("{96:1, 97:2}"));
    CHECK_EQ(show(resolve_employees(db, raw, {}, {}).map), std::string("{90:1, 91:2}"));
    CHECK_EQ(db.rows("SELECT UID FROM AccessLevels ORDER BY UID", 1), std::string("1;2"));
    CHECK_EQ(db.rows("SELECT UID FROM Employees ORDER BY UID", 1), std::string("1;2"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_rows_without_uid_or_name_are_not_mistaken_for_a_literal_xx
TEST(ImportMasterData, Imp10_Imp11_RowsWithoutUidOrNameAreNotMistakenForALiteralXx)
{
    struct Case
    {
        const char *table;
        std::vector<std::string> setup;
        ost_import::RowList rows;
        const char *expected;
    };
    const std::vector<Case> cases = {
        {"CdnTypes",
         {"INSERT INTO CdnTypes (UID, Name, ExpandState) VALUES (40, 'XX', 0)"},
         {row({{"ExpandState", "0"}}), row({{"UID", "2"}, {"Name", "XX"}, {"ExpandState", "0"}})},
         "{:101, 2:40}"},
        {"JobStatuses",
         {"INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (50, 'XX', 0, 1)"},
         {row({{"Locked", "0"}, {"Sequence", "1"}}), row({{"UID", "2"}, {"Name", "XX"}, {"Locked", "0"}, {"Sequence", "1"}})},
         "{:101, 2:50}"},
        {"AccessLevels",
         {"DROP TABLE AccessLevels", "CREATE TABLE AccessLevels (UID INTEGER PRIMARY KEY, Description TEXT)",
          "INSERT INTO AccessLevels (UID, Description) VALUES (15, 'XX')"},
         {row({}), row({{"UID", "2"}, {"Description", "XX"}})},
         "{:16, 2:15}"},
        {"PayClasses",
         {"INSERT INTO PayClasses (UID, Name) VALUES (20, 'XX')"},
         {row({}), row({{"UID", "2"}, {"Name", "XX"}})},
         "{:21, 2:20}"},
        {"Employees",
         {"INSERT INTO Employees (UID, EmployeeNo) VALUES (5, 'XX')"},
         {row({{"FirstName", "Ann"}}), row({{"UID", "2"}, {"EmployeeNo", "XX"}})},
         "{:6, 2:5}"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.table);
        ScratchDb db(std::string("md_xx_") + c.table);
        create_import_schema(db);
        db.must_all(c.setup);
        const OstRawData raw = raw_with(c.table, c.rows);
        const Resolved r = std::string(c.table) == "Employees" ? resolve_employees(db, raw, {}, {}) : resolve(db, c.table, raw);
        CHECK(r.ok);
        CHECK_EQ(show(r.map), std::string(c.expected));
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_a_nameless_incoming_condition_type_reuses_a_nameless_existing_one
// and ::test_a_nameless_incoming_job_status_reuses_a_nameless_existing_one
TEST(ImportMasterData, Imp10_NamelessIncomingRowsReuseNamelessExistingOnes)
{
    {
        ScratchDb db("md_nameless_reuse_cdn");
        create_import_schema(db);
        db.must("INSERT INTO CdnTypes (UID, Name, ExpandState) VALUES (40, NULL, 0)");
        const Resolved r = resolve(db, "CdnTypes", raw_with("CdnTypes", {row({{"UID", "9"}})}));
        CHECK(r.ok);
        CHECK_EQ(show(r.map), std::string("{9:40}"));
        CHECK_EQ(r.max_uid, 100);
    }
    {
        ScratchDb db("md_nameless_reuse_js");
        create_import_schema(db);
        db.must("INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (40, NULL, 0, 1)");
        const Resolved r = resolve(db, "JobStatuses", raw_with("JobStatuses", {row({{"UID", "9"}})}));
        CHECK(r.ok);
        CHECK_EQ(show(r.map), std::string("{9:40}"));
        CHECK_EQ(r.max_uid, 100);
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_a_nameless_row_is_never_taken_for_a_row_literally_named_x
TEST(ImportMasterData, Imp10_Imp11_ANamelessRowIsNeverTakenForARowLiterallyNamedX)
{
    struct Case
    {
        const char *table;
        std::vector<std::string> setup;
        std::string existing;
        ost_import::RowList rows;
    };
    const std::vector<Case> cases = {
        {"CdnTypes", {}, "INSERT INTO CdnTypes (UID, Name, ExpandState) VALUES (40, 'x', 0)", {row({{"UID", "1"}}), row({{"UID", "2"}, {"Name", "x"}})}},
        {"JobStatuses", {}, "INSERT INTO JobStatuses (UID, Name, Locked, Sequence) VALUES (40, 'x', 0, 1)", {row({{"UID", "1"}}), row({{"UID", "2"}, {"Name", "x"}})}},
        {"AccessLevels", {"DROP TABLE AccessLevels", "CREATE TABLE AccessLevels (UID INTEGER PRIMARY KEY, Description TEXT)"},
         "INSERT INTO AccessLevels (UID, Description) VALUES (40, 'x')", {row({{"UID", "1"}}), row({{"UID", "2"}, {"Description", "x"}})}},
        {"PayClasses", {}, "INSERT INTO PayClasses (UID, Name) VALUES (40, 'x')", {row({{"UID", "1"}}), row({{"UID", "2"}, {"Name", "x"}})}},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.table);
        ScratchDb db(std::string("md_x_") + c.table);
        create_import_schema(db);
        db.must_all(c.setup);
        db.must(c.existing);
        const Resolved r = resolve(db, c.table, raw_with(c.table, c.rows));
        CHECK(r.ok);
        CHECK_EQ(r.map.size(), 2u);
        CHECK_STR_EQ(r.map.at("2"), std::string("40"));
        CHECK_NE(r.map.at("1"), std::string("40"));
        CHECK_EQ(db.count(std::string("SELECT COUNT(*) FROM ") + c.table), 2);
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_employee_references_to_unmapped_pay_classes_and_levels_become_null
TEST(ImportMasterData, Imp11_EmployeeReferencesToUnmappedPayClassesAndLevelsBecomeNull)
{
    ScratchDb db("md_emp_unmapped_refs");
    create_import_schema(db);
    db.must_all({"DROP TABLE Employees",
                 "CREATE TABLE Employees (UID INTEGER, EmployeeNo TEXT, PayClassUID TEXT DEFAULT 'default-pay', "
                 "AccessLevelUID TEXT DEFAULT 'default-level')"});
    OstRawData raw = raw_with(
        "Employees",
        {row({{"UID", "1"}, {"EmployeeNo", "E1"}, {"PayClassUID", "70"}, {"AccessLevelUID", "80"}}),
         row({{"UID", "2"}, {"EmployeeNo", "E2"}, {"PayClassUID", "71"}, {"AccessLevelUID", "81"}}),
         row({{"UID", "3"}, {"EmployeeNo", "E3"}}),
         row({{"UID", "4"}, {"EmployeeNo", "E4"}, {"PayClassUID", ""}, {"AccessLevelUID", ""}})});
    const Resolved r = resolve_employees(db, raw, {{"70", "12"}}, {{"81", "13"}});
    CHECK(r.ok);
    CHECK_EQ(show(r.map), std::string("{1:1, 2:2, 3:3, 4:4}"));
    CHECK_EQ(db.rows("SELECT UID, PayClassUID, AccessLevelUID FROM Employees ORDER BY UID", 3),
             std::string("1|12|NULL;2|NULL|13;3|default-pay|default-level;4||"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_employee_identity_keys_use_number_first_then_trimmed_lowercase_names
TEST(ImportMasterData, Imp11_EmployeeIdentityKeysUseTheNumberFirstThenTrimmedLowercaseNames)
{
    struct Case
    {
        ost_import::Row r;
        const char *expected;
    };
    const Case cases[] = {
        {row({{"EmployeeNo", " E1 "}}), "no:e1"},
        {row({{"EmployeeNo", "E1"}, {"FirstName", "Ann"}}), "no:e1"},
        {row({{"FirstName", "Ann"}, {"LastName", "Lee"}, {"EMail", "A@X.com"}}), "name:ann|lee|a@x.com"},
        {row({{"FirstName", " Ann "}}), "name:ann||"},
        {row({{"LastName", "LEE"}}), "name:|lee|"},
        {row({{"EMail", " a@x "}}), "name:||a@x"},
        {row({}), ""},
        {row({{"EmployeeNo", " "}, {"FirstName", ""}, {"LastName", "  "}, {"EMail", " "}}), ""},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(ost_itest::describe(c.r));
        CHECK_STR_EQ(ost_store::import_employee_identity_key(c.r), std::string(c.expected));
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_employee_identity_keys_fold_case_with_casefold_not_lower
TEST(ImportMasterData, Imp11_EmployeeIdentityKeysFoldCaseWithCasefoldNotLower)
{
    const std::string sharp = "Stra\xc3\x9f" "e";
    struct Case
    {
        const char *column;
        const char *expected;
    };
    for (const Case &c : {Case{"EmployeeNo", "no:strasse"}, Case{"FirstName", "name:strasse||"}, Case{"LastName", "name:|strasse|"},
                          Case{"EMail", "name:||strasse"}})
    {
        SCOPED_CONTEXT(c.column);
        CHECK_STR_EQ(ost_store::import_employee_identity_key(row({{c.column, "STRASSE"}})), std::string(c.expected));
        CHECK_STR_EQ(ost_store::import_employee_identity_key(row({{c.column, sharp}})), std::string(c.expected));
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_an_existing_employee_is_matched_through_the_folded_number
TEST(ImportMasterData, Imp11_AnExistingEmployeeIsMatchedThroughTheFoldedNumber)
{
    ScratchDb db("md_emp_folded");
    create_import_schema(db);
    db.must("INSERT INTO Employees (UID, EmployeeNo) VALUES (7, 'Stra\xc3\x9f" "e')");
    const Resolved r = resolve_employees(db, raw_with("Employees", {row({{"UID", "90"}, {"EmployeeNo", "STRASSE"}})}), {}, {});
    CHECK(r.ok);
    CHECK_EQ(show(r.map), std::string("{90:7}"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_existing_employees_are_matched_on_email_and_duplicates_are_rejected
TEST(ImportMasterData, Imp11_ExistingEmployeesAreMatchedOnEmailAndDuplicateUidsAreRejected)
{
    ScratchDb db("md_emp_email");
    create_import_schema(db);
    db.must_all({"DROP TABLE Employees", "CREATE TABLE Employees (UID INTEGER, EmployeeNo TEXT, FirstName TEXT, LastName TEXT, EMail TEXT)",
                 "INSERT INTO Employees VALUES (3, NULL, 'Ann', 'Lee', 'ann@x.test')",
                 "INSERT INTO Employees VALUES (4, NULL, 'Ann', 'Lee', 'other@x.test')"});
    OstRawData raw = raw_with("Employees",
                              {row({{"UID", "90"}, {"FirstName", "ann"}, {"LastName", "LEE"}, {"EMail", "ANN@x.test"}}),
                               row({{"UID", "91"}, {"FirstName", "Ann"}, {"LastName", "Lee"}, {"EMail", "new@x.test"}})});
    const Resolved r = resolve_employees(db, raw, {}, {});
    CHECK(r.ok);
    CHECK_EQ(show(r.map), std::string("{90:3, 91:5}"));
    db.must("INSERT INTO Employees VALUES (3, 'E9', 'Dup', 'Row', NULL)");
    const Resolved dup = resolve_employees(db, raw, {}, {});
    CHECK_FALSE(dup.ok);
    CHECK_STR_EQ(dup.error, std::string("Employees contains duplicate UID 3; authoritative master-data UIDs must be unique."));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_an_existing_numeric_zero_employee_number_still_forms_a_key
TEST(ImportMasterData, Imp11_AnExistingNumericZeroEmployeeNumberStillFormsAKey)
{
    ScratchDb db("md_emp_zero_no");
    create_import_schema(db);
    db.must_all({"DROP TABLE Employees", "CREATE TABLE Employees (UID INTEGER, EmployeeNo)",
                 "INSERT INTO Employees (UID, EmployeeNo) VALUES (7, 0)"});
    const Resolved r = resolve_employees(db, raw_with("Employees", {row({{"UID", "90"}, {"EmployeeNo", "0"}})}), {}, {});
    CHECK(r.ok);
    CHECK_EQ(show(r.map), std::string("{90:7}"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_repeated_rows_with_the_same_source_uid_are_stored_once
TEST(ImportMasterData, Imp10_Imp11_RepeatedRowsWithTheSameSourceUidAreStoredOnce)
{
    struct Case
    {
        const char *table;
        bool access_levels_with_description;
        ost_import::Row r;
        const char *expected_map;
        const char *select;
        int columns;
        const char *expected_rows;
    };
    const std::vector<Case> cases = {
        {"CdnTypes", false, row({{"UID", "90"}, {"Name", "X"}, {"ExpandState", "0"}}), "{90:101}", "SELECT UID, Name, ExpandState FROM CdnTypes", 3, "101|X|0"},
        {"JobStatuses", false, row({{"UID", "90"}, {"Name", "X"}, {"Locked", "0"}, {"Sequence", "1"}}), "{90:101}",
         "SELECT UID, Name, Locked, Sequence FROM JobStatuses", 4, "101|X|0|1"},
        {"AccessLevels", true, row({{"UID", "90"}, {"Description", "X"}}), "{90:1}", "SELECT UID, Description FROM AccessLevels", 2, "1|X"},
        {"PayClasses", false, row({{"UID", "90"}, {"Name", "X"}}), "{90:1}", "SELECT UID, Name FROM PayClasses", 2, "1|X"},
        {"Employees", false, row({{"UID", "90"}, {"EmployeeNo", "E1"}, {"FirstName", "a"}}), "{90:1}",
         "SELECT UID, EmployeeNo, FirstName FROM Employees", 3, "1|E1|a"},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.table);
        ScratchDb db(std::string("md_repeat_") + c.table);
        create_import_schema(db);
        if (c.access_levels_with_description)
            access_levels_with_description(db);
        const OstRawData raw = raw_with(c.table, {c.r, c.r});
        const Resolved r = std::string(c.table) == "Employees" ? resolve_employees(db, raw, {}, {}) : resolve(db, c.table, raw);
        CHECK(r.ok);
        CHECK_EQ(show(r.map), std::string(c.expected_map));
        CHECK_EQ(db.rows(c.select, c.columns), std::string(c.expected_rows));
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_names_that_differ_only_in_case_or_spacing_are_distinct_outside_cdn_types
// (and DBP-25: JobStatuses 'open' next to an existing 'Open' is a NEW row).
TEST(ImportMasterData, Imp10_NamesThatDifferOnlyInCaseOrSpacingAreDistinctOutsideCondTypes)
{
    struct Case
    {
        const char *table;
        bool access_levels_with_description;
        const char *column;
        const char *expected_map;
    };
    const Case cases[] = {{"JobStatuses", false, "Name", "{1:101, 2:102, 3:103}"},
                          {"PayClasses", false, "Name", "{1:1, 2:2, 3:3}"},
                          {"AccessLevels", true, "Description", "{1:1, 2:2, 3:3}"}};
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.table);
        ScratchDb db(std::string("md_case_distinct_") + c.table);
        create_import_schema(db);
        if (c.access_levels_with_description)
            access_levels_with_description(db);
        ost_import::RowList rows;
        int uid = 1;
        for (const char *name : {"Open", "open", " Open "})
        {
            ost_import::Row r = row({{"UID", std::to_string(uid++)}, {c.column, name}});
            if (std::string(c.table) == "JobStatuses")
            {
                r["Locked"] = "0";
                r["Sequence"] = "1";
            }
            rows.push_back(r);
        }
        const Resolved r = resolve(db, c.table, raw_with(c.table, rows));
        CHECK(r.ok);
        CHECK_EQ(show(r.map), std::string(c.expected_map));
        CHECK_EQ(db.rows(std::string("SELECT ") + c.column + " FROM " + c.table + " ORDER BY UID", 1), std::string("Open;open; Open "));
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_an_existing_name_is_matched_exactly_outside_cdn_types_characterisation
TEST(ImportMasterData, Imp10_AnExistingNameIsMatchedExactlyOutsideCondTypes)
{
    ScratchDb db("md_exact_match");
    create_import_schema(db);
    db.must("INSERT INTO PayClasses VALUES (20, 'Regular')");
    const Resolved r = resolve(db, "PayClasses",
                               raw_with("PayClasses", {row({{"UID", "1"}, {"Name", "Regular"}}), row({{"UID", "2"}, {"Name", "regular"}})}));
    CHECK(r.ok);
    CHECK_EQ(show(r.map), std::string("{1:20, 2:21}"));
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_an_incoming_empty_identity_never_reuses_an_existing_empty_row
TEST(ImportMasterData, Imp11_AnIncomingEmptyIdentityNeverReusesAnExistingEmptyRow)
{
    for (const char *table : {"PayClasses", "AccessLevels"})
    {
        SCOPED_CONTEXT(std::string("table ") + table);
        const std::string column = std::string(table) == "PayClasses" ? "Name" : "Description";
        ScratchDb db(std::string("md_empty_never_reused_") + table);
        create_import_schema(db);
        if (std::string(table) == "AccessLevels")
            access_levels_with_description(db);
        db.must(std::string("INSERT INTO ") + table + " (UID, " + column + ") VALUES (15, '')");
        const Resolved r = resolve(db, table, raw_with(table, {row({{"UID", "90"}, {column, ""}})}));
        CHECK(r.ok);
        CHECK_EQ(show(r.map), std::string("{90:16}"));
        CHECK_EQ(db.rows(std::string("SELECT UID FROM ") + table + " ORDER BY UID", 1), std::string("15;16"));
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_the_resolvers_leave_the_callers_incoming_rows_untouched
TEST(ImportMasterData, Imp10_Imp11_TheResolversLeaveTheIncomingRowsUntouched)
{
    ScratchDb db("md_rows_untouched");
    create_import_schema(db);
    access_levels_with_description(db);
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.global_tables["AccessLevels"] = {row({{"UID", "90"}, {"Description", "Full"}})};
    raw.global_tables["PayClasses"] = {row({{"UID", "91"}, {"Name", "Reg"}})};
    raw.global_tables["Employees"] = {row({{"UID", "92"}, {"EmployeeNo", "E1"}, {"PayClassUID", "91"}})};
    raw.global_tables["CdnTypes"] = {row({{"UID", "93"}, {"Name", "C"}, {"ExpandState", "0"}})};
    raw.global_tables["JobStatuses"] = {row({{"UID", "94"}, {"Name", "J"}, {"Locked", "0"}, {"Sequence", "1"}})};
    const OstRawData before = raw;
    Connection conn(db);
    long long max_uid = 100;
    UidMap cdn, js, access, pay, employee;
    std::string error;
    CHECK(ost_store::import_resolve_cdn_types(conn.get(), raw, max_uid, cdn, error));
    CHECK(ost_store::import_resolve_job_statuses(conn.get(), raw, max_uid, js, error));
    CHECK(ost_store::import_resolve_access_levels(conn.get(), raw, access, error));
    CHECK(ost_store::import_resolve_pay_classes(conn.get(), raw, pay, error));
    CHECK(ost_store::import_resolve_employees(conn.get(), raw, pay, access, employee, error));
    for (const auto &table : before.global_tables)
    {
        const auto it = raw.global_tables.find(table.first);
        REQUIRE(it != raw.global_tables.end());
        REQUIRE_EQ(it->second.size(), table.second.size());
        for (size_t i = 0; i < table.second.size(); ++i)
            CHECK(it->second[i] == table.second[i]);
    }
}

// Port of tests/infrastructure/mdb/components/test_import_operations.py::test_new_condition_type_and_job_status_uids_are_allocated_one_above_the_other
// and ::test_an_imported_condition_type_alone_raises_the_floor_for_the_transform (floor of the existing bid 100).
TEST(ImportMasterData, Imp10_NewCondTypeAndJobStatusUidsAreAllocatedOneAboveTheOther)
{
    ScratchDb db("md_floor_chain");
    create_import_schema(db);
    db.must("INSERT INTO Bids (UID, JobName) VALUES (100, 'Existing')");
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.global_tables["CdnTypes"] = {row({{"UID", "9"}, {"Name", "Footings"}, {"ExpandState", "0"}})};
    raw.global_tables["JobStatuses"] = {row({{"UID", "3"}, {"Name", "Won"}, {"Locked", "0"}, {"Sequence", "1"}})};
    Connection conn(db);
    long long max_uid = 0;
    std::string error;
    CHECK(ost_store::import_get_max_uid(conn.get(), max_uid, error));
    CHECK_EQ(max_uid, 100);
    UidMap cdn, js;
    CHECK(ost_store::import_resolve_cdn_types(conn.get(), raw, max_uid, cdn, error));
    CHECK_EQ(max_uid, 101);
    CHECK(ost_store::import_resolve_job_statuses(conn.get(), raw, max_uid, js, error));
    CHECK_EQ(max_uid, 102);
    CHECK_EQ(show(cdn), std::string("{9:101}"));
    CHECK_EQ(show(js), std::string("{3:102}"));
    CHECK_EQ(db.rows("SELECT UID, Name FROM CdnTypes", 2), std::string("101|Footings"));
    CHECK_EQ(db.rows("SELECT UID, Name FROM JobStatuses", 2), std::string("102|Won"));
}
