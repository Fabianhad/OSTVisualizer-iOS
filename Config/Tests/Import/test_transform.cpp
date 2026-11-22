// IMP-06 / IMP-03 / IMP-07 / IMP-04 (pure part): the UID remap of an OST file.
//
// Windows: ost_visualizer/infrastructure/mdb/importers/ost_importer.py (_transform, _build_uid_map,
// _build_page_uid_map, _remap_uid_value, _remap_row, _remap_data) and infrastructure/database/page_area_selection.py.
// Every case below is a port of a Windows test (named in the comment); a "NULL" value is the Windows placeholder for SQL
// NULL.
#include "ImportTestSupport.hpp"

#include "Import/OstImportTransform.hpp"

using ost_import::OstRawData;
using ost_import::RemapMaps;
using ost_import::Row;
using ost_import::RowList;
using ost_import::UidMap;
using ost_itest::row;
using ost_itest::values_of;

namespace
{
    std::string remap(const std::string &field, const std::string &value, const RemapMaps &maps)
    {
        return ost_import::remap_uid_value(field, value, maps);
    }

    std::vector<std::string> uid_list(const RowList &rows)
    {
        return values_of(rows, "UID");
    }
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_transform_remaps_internal_global_and_null_references
TEST(ImportTransform, Imp06_TransformRemapsInternalGlobalAndNullReferences)
{
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}, {"BidProjectUID", "3"}, {"EstimatorUID", "7"}, {"PrManagerUID", "8"},
                       {"JobStatusUID", "5"}, {"JobName", "0"}});
    raw.bid_tables["BidPages"] = {row({{"UID", "20"}, {"BidUID", "1"}, {"MasterPageUID", "0"}}),
                                  row({{"UID", "21"}, {"BidUID", "1"}, {"MasterPageUID", "20"}})};
    raw.bid_tables["BidSettings"] = {row({{"UID", "30"}, {"BidUID", "1"}, {"BidPageSelectedUID", "21"}}),
                                     row({{"UID", "31"}, {"BidUID", "1"}, {"BidPageSelectedUID", "0"}}),
                                     row({{"UID", "32"}, {"BidUID", "1"}, {"BidPageSelectedUID", "999"}})};
    raw.bid_tables["BidConditions"] = {row({{"UID", "40"}, {"BidUID", "1"}, {"CdnTypeUID", "4"}}),
                                       row({{"UID", "41"}, {"BidUID", "1"}, {"CdnTypeUID", "77"}})};
    raw.bid_tables["BidEmployees"] = {row({{"UID", "50"}, {"BidUID", "1"}, {"EmployeeUID", "7"}}),
                                      row({{"UID", "51"}, {"BidUID", "1"}, {"EmployeeUID", "8"}})};
    raw.page_tables["BidTakeoffs"] = {
        row({{"UID", "60"}, {"BidUID", "1"}, {"BidPageUID", "20"}, {"BidConditionUID", "40"}, {"ParentUID", "0"}}),
        row({{"UID", "61"}, {"BidUID", "1"}, {"BidPageUID", "20"}, {"BidConditionUID", "40"}, {"ParentUID", "60"}})};

    const OstRawData remapped = ost_import::transform(raw, 100, {{"4", "400"}}, {{"5", "500"}}, {{"7", "700"}}, {});

    CHECK_EQ(ost_itest::describe(remapped.bid_row),
             std::string("BidProjectUID=NULL,EstimatorUID=700,JobName=0,JobStatusUID=500,PrManagerUID=NULL,UID=101"));
    const RowList &pages = remapped.bid_tables.find("BidPages")->second;
    CHECK_EQ(values_of(pages, "UID"), (std::vector<std::string>{"102", "103"}));
    CHECK_EQ(values_of(pages, "BidUID"), (std::vector<std::string>{"101", "101"}));
    CHECK_EQ(values_of(pages, "MasterPageUID"), (std::vector<std::string>{"NULL", "102"}));
    CHECK_EQ(values_of(remapped.bid_tables.find("BidSettings")->second, "BidPageSelectedUID"),
             (std::vector<std::string>{"103", "NULL", "NULL"}));
    const RowList &conditions = remapped.bid_tables.find("BidConditions")->second;
    CHECK_EQ(values_of(conditions, "CdnTypeUID"), (std::vector<std::string>{"400", "77"}));
    CHECK_EQ(values_of(remapped.bid_tables.find("BidEmployees")->second, "EmployeeUID"),
             (std::vector<std::string>{"700", "NULL"}));
    const RowList &takeoffs = remapped.page_tables.find("BidTakeoffs")->second;
    CHECK_EQ(values_of(takeoffs, "UID"), (std::vector<std::string>{"111", "112"}));
    CHECK_EQ(values_of(takeoffs, "BidPageUID"), (std::vector<std::string>{"102", "102"}));
    CHECK_EQ(values_of(takeoffs, "ParentUID"), (std::vector<std::string>{"NULL", "111"}));
    const std::string condition_uid = conditions[0].at("UID");
    CHECK_EQ(values_of(takeoffs, "BidConditionUID"), (std::vector<std::string>{condition_uid, condition_uid}));
    // the original is never mutated
    CHECK_STR_EQ(raw.bid_row.at("UID"), std::string("1"));
    CHECK_STR_EQ(raw.page_tables.find("BidTakeoffs")->second[1].at("ParentUID"), std::string("60"));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_remap_row_maps_or_nulls_master_data_references_per_field
TEST(ImportTransform, Imp03_Imp07_RemapRowMapsOrNullsMasterDataReferencesPerField)
{
    RemapMaps maps;
    maps.uid = {{"5", "105"}};
    maps.cdn_type = {{"11", "111"}};
    maps.job_status = {{"6", "206"}};
    maps.pay_class = {{"9", "309"}};
    const Row remapped = ost_import::remap_row(
        row({{"UID", "5"}, {"JobStatusUID", "6"}, {"PayClassUID", "9"}, {"EmployeeUID", "8"}, {"CdnTypeUID", "11"},
             {"OCRUID", "12"}, {"Note", "0"}}),
        maps);
    CHECK_EQ(ost_itest::describe(remapped),
             std::string("CdnTypeUID=111,EmployeeUID=NULL,JobStatusUID=206,Note=0,OCRUID=NULL,PayClassUID=309,UID=105"));

    RemapMaps unmapped_maps;
    unmapped_maps.job_status = {{"6", "206"}};
    unmapped_maps.pay_class = {{"9", "309"}};
    const Row unmapped = ost_import::remap_row(row({{"JobStatusUID", "7"}, {"PayClassUID", "10"}, {"CdnTypeUID", "13"}}),
                                               unmapped_maps);
    CHECK_EQ(ost_itest::describe(unmapped), std::string("CdnTypeUID=13,JobStatusUID=NULL,PayClassUID=NULL"));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_remap_prefixes_name_every_reference_field_family_per_field
TEST(ImportTransform, Imp03_Imp07_EveryReferenceFieldFamilyRemapsPerField)
{
    const char *const zero_fields[] = {"BidProjectUID", "ParentBidUID", "OrigBidProjectUID", "OrigParentBidUID",
                                       "SourceBidUID", "OCRUID", "CoverSheetSelItemUID"};
    const char *const employee_fields[] = {"EstimatorUID", "PrManagerUID", "JobSiteManagerUID", "EmployeeUID"};
    const char *const internal_fields[] = {
        "UID", "BidUID", "BidPageUID", "BidConditionUID", "BidLayerUID", "BidZoneUID", "BidAreaUID",
        "BidTypAreaUID", "ParentUID", "ParentCommentUID", "MasterPageUID", "MasterAreaUID", "TranslateAreaUID",
        "BidConditionFolderUID", "BidPageFolderUID", "BidTakeoffFromUID", "BidTakeoffToUID", "BidPageViewUID",
        "TypGroupTakeoffUID", "TypPageTakeoffUID", "TypGroupMarkerUID", "TypGroupUID"};

    RemapMaps maps;
    maps.uid = {{"5", "105"}};
    auto with_employees = [&](const UidMap &employees)
    {
        RemapMaps m = maps;
        m.employee = employees;
        return m;
    };

    for (const char *field : zero_fields)
    {
        SCOPED_CONTEXT(std::string("source-database family: ") + field);
        CHECK_STR_EQ(remap(field, "5", maps), std::string("NULL"));
        CHECK_STR_EQ(remap(field, "77", maps), std::string("NULL"));
    }
    for (const char *field : employee_fields)
    {
        SCOPED_CONTEXT(std::string("employee family: ") + field);
        CHECK_STR_EQ(remap(field, "5", with_employees({{"5", "605"}})), std::string("605"));
        CHECK_STR_EQ(remap(field, "5", with_employees({{"6", "606"}})), std::string("NULL"));
    }
    for (const char *field : internal_fields)
    {
        SCOPED_CONTEXT(std::string("bid-internal family: ") + field);
        CHECK_STR_EQ(remap(field, "5", maps), std::string("105"));
        CHECK_STR_EQ(remap(field, "77", maps), std::string("77"));
    }
    // IMP-03: a "0" in ANY field that ends with UID is NULL (the Windows exporter writes NULL UIDs as 0)
    for (const char *field : zero_fields)
        CHECK_STR_EQ(remap(field, "0", maps), std::string("NULL"));
    for (const char *field : employee_fields)
        CHECK_STR_EQ(remap(field, "0", maps), std::string("NULL"));
    for (const char *field : internal_fields)
        CHECK_STR_EQ(remap(field, "0", maps), std::string("NULL"));
    for (const char *field : {"Name", "Sequence", "SomethingElseUID"})
    {
        SCOPED_CONTEXT(std::string("untouched: ") + field);
        CHECK_STR_EQ(remap(field, "5", maps), std::string("5"));
    }
    CHECK_STR_EQ(remap("Sequence", "0", maps), std::string("0"));
    CHECK_STR_EQ(remap("SomethingElseUID", "0", maps), std::string("NULL"));
    // IMP-03 test plan: the page folder/master/parent/layer/condition-folder fields
    for (const char *field : {"BidPageFolderUID", "MasterPageUID", "ParentUID", "BidLayerUID", "BidConditionFolderUID"})
        CHECK_STR_EQ(remap(field, "0", maps), std::string("NULL"));
    CHECK_STR_EQ(remap("JobName", "0", maps), std::string("0"));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_blank_references_stay_blank_for_every_remapped_field_family
TEST(ImportTransform, BlankReferencesStayBlankForEveryRemappedFieldFamily)
{
    RemapMaps maps;
    maps.uid = {{"", "1"}};
    maps.page = {{"", "2"}};
    maps.cdn_type = {{"", "3"}};
    maps.job_status = {{"", "4"}};
    maps.employee = {{"", "5"}};
    maps.pay_class = {{"", "6"}};
    for (const char *field : {"UID", "BidUID", "ParentUID", "CdnTypeUID", "JobStatusUID", "PayClassUID", "EstimatorUID",
                              "PrManagerUID", "JobSiteManagerUID", "EmployeeUID", "BidProjectUID", "OCRUID",
                              "SomethingElseUID"})
    {
        SCOPED_CONTEXT(std::string("field ") + field);
        CHECK_STR_EQ(remap(field, "", maps), std::string());
    }
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_page_selection_references_map_only_known_non_zero_pages
// (a Python None is the same as "" here: Row values are always text).
TEST(ImportTransform, PageSelectionReferencesMapOnlyKnownNonZeroPages)
{
    RemapMaps maps;
    maps.page = {{"", "5"}, {"0", "6"}, {"7", "17"}};
    CHECK_STR_EQ(remap("BidPageSelectedUID", "7", maps), std::string("17"));
    CHECK_STR_EQ(remap("BidPageSelectedUID", " 7 ", maps), std::string("17"));
    CHECK_STR_EQ(remap("BidPageSelectedUID", "0", maps), std::string("NULL"));
    CHECK_STR_EQ(remap("BidPageSelectedUID", "", maps), std::string("NULL"));
    CHECK_STR_EQ(remap("BidPageSelectedUID", "8", maps), std::string("NULL"));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_uid_assignment_skips_rows_without_a_usable_uid_without_gaps
TEST(ImportTransform, Imp06_UidAssignmentSkipsRowsWithoutAUsableUidWithoutGaps)
{
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.bid_tables["BidAreas"] = {row({{"Name", "no uid"}}), row({{"UID", ""}, {"Name", "blank"}}),
                                  row({{"UID", "0"}, {"Name", "zero"}}), row({{"UID", "10"}}),
                                  row({{"UID", "10"}, {"Name", "repeat"}})};
    raw.bid_tables["BidPages"] = {row({{"UID", "20"}}), row({{"BidUID", "1"}})};
    raw.page_tables["BidTakeoffs"] = {row({{"Name", "no uid"}}), row({{"UID", "30"}})};
    CHECK(ost_import::build_uid_map(raw, 100) ==
          (UidMap{{"1", "101"}, {"10", "102"}, {"20", "103"}, {"30", "104"}}));

    OstRawData no_bid_uid;
    no_bid_uid.bid_row = row({{"Name", "no uid"}});
    CHECK(ost_import::build_uid_map(no_bid_uid, 5).empty());

    no_bid_uid.bid_tables["BidAreas"] = {row({{"UID", "7"}})};
    CHECK(ost_import::build_uid_map(no_bid_uid, 5) == (UidMap{{"7", "6"}}));
}

// IMP-06 test plan (2): the numbering order is the bid row, every bid table in insertion order, then every page table
// in first-seen order (here BidTexts is keyed before BidTakeoffs although both belong to the same page).
TEST(ImportTransform, Imp06_UidNumberingFollowsTableInsertionOrder)
{
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.bid_tables["BidPages"] = {row({{"UID", "20"}})};
    raw.bid_tables["BidAreas"] = {row({{"UID", "10"}})};
    raw.page_tables["BidTexts"] = {row({{"UID", "31"}})};
    raw.page_tables["BidTakeoffs"] = {row({{"UID", "30"}})};
    const UidMap map = ost_import::build_uid_map(raw, 100);
    CHECK(map == (UidMap{{"1", "101"}, {"20", "102"}, {"10", "103"}, {"31", "104"}, {"30", "105"}}));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_page_uid_map_ignores_pages_without_a_mapped_uid
TEST(ImportTransform, PageUidMapIgnoresPagesWithoutAMappedUid)
{
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.bid_tables["BidPages"] = {row({{"Name", "no uid"}}), row({{"UID", " 7 "}}), row({{"UID", "8"}}),
                                  row({{"UID", "9"}})};
    CHECK(ost_import::build_page_uid_map(raw, {{"7", "17"}, {"9", "19"}}) == (UidMap{{"7", "17"}, {"9", "19"}}));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_page_uid_map_never_maps_a_page_without_a_uid_even_if_the_map_has_a_blank_key
TEST(ImportTransform, PageUidMapNeverMapsAPageWithoutAUidEvenIfTheMapHasABlankKey)
{
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.bid_tables["BidPages"] = {row({{"Name", "no uid"}}), row({{"UID", ""}}), row({{"UID", "  "}})};
    CHECK(ost_import::build_page_uid_map(raw, {{"", "5"}, {"  ", "6"}, {" ", "7"}, {"0", "8"}}).empty());
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_remap_data_applies_every_map_to_the_bid_row_and_both_table_families
TEST(ImportTransform, RemapDataAppliesEveryMapToTheBidRowAndBothTableFamilies)
{
    auto mapped_fields = [](const std::string &uid)
    {
        return row({{"UID", uid}, {"BidUID", "1"}, {"CdnTypeUID", "4"}, {"JobStatusUID", "5"}, {"EstimatorUID", "7"},
                    {"PayClassUID", "9"}, {"BidPageSelectedUID", "20"}});
    };
    OstRawData raw;
    raw.bid_row = mapped_fields("1");
    raw.bid_tables["BidPages"] = {row({{"UID", "20"}, {"BidUID", "1"}})};
    raw.bid_tables["BidSettings"] = {mapped_fields("30")};
    raw.page_tables["BidTakeoffs"] = {mapped_fields("60")};

    const OstRawData remapped =
        ost_import::transform(raw, 100, {{"4", "400"}}, {{"5", "500"}}, {{"7", "700"}}, {{"9", "900"}});

    auto expected = [](const std::string &uid)
    {
        return row({{"UID", uid}, {"BidUID", "101"}, {"CdnTypeUID", "400"}, {"JobStatusUID", "500"},
                    {"EstimatorUID", "700"}, {"PayClassUID", "900"}, {"BidPageSelectedUID", "102"}});
    };
    CHECK(remapped.bid_row == expected("101"));
    CHECK(remapped.bid_tables.find("BidSettings")->second[0] == expected("103"));
    CHECK(remapped.page_tables.find("BidTakeoffs")->second[0] == expected("104"));
}

// Windows parity (tests/integration/import_export/test_backend_graph_parity.py::test_access_and_sql_writers_receive_same_pruned_takeoff_graph):
// with max UID 100 the Access path numbers the bid 101, the condition 102, the page 103 and then the surviving takeoffs in
// FILE order (the child that precedes its parent in the file gets the lower UID but points forward at it). The pruning of
// the orphan branch (IMP-09) happens before the remap; the input here is the already pruned graph.
TEST(ImportTransform, Imp06_ParityGraphNumbersTakeoffsInFileOrder)
{
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}, {"JobName", "Imported"}});
    raw.bid_tables["BidConditions"] = {row({{"UID", "10"}, {"BidUID", "1"}})};
    raw.bid_tables["BidPages"] = {row({{"UID", "20"}, {"BidUID", "1"}, {"Name", "Sheet"}})};
    auto takeoff = [](const std::string &uid, const std::string &parent, const std::string &name)
    {
        Row r = row({{"UID", uid}, {"BidUID", "1"}, {"BidPageUID", "20"}, {"BidConditionUID", "10"}, {"Name", name}});
        if (!parent.empty())
            r["ParentUID"] = parent;
        return r;
    };
    raw.page_tables["BidTakeoffs"] = {takeoff("31", "30", "Child"), takeoff("30", "", "Parent"),
                                      takeoff("34", "", "Early"), takeoff("35", "34", "Late")};
    const OstRawData remapped = ost_import::transform(raw, 100, {}, {}, {}, {});
    CHECK_STR_EQ(remapped.bid_row.at("UID"), std::string("101"));
    CHECK_STR_EQ(remapped.bid_tables.find("BidConditions")->second[0].at("UID"), std::string("102"));
    CHECK_STR_EQ(remapped.bid_tables.find("BidPages")->second[0].at("UID"), std::string("103"));
    const RowList &rows = remapped.page_tables.find("BidTakeoffs")->second;
    REQUIRE_EQ(rows.size(), 4u);
    CHECK_EQ(values_of(rows, "UID"), (std::vector<std::string>{"104", "105", "106", "107"}));
    CHECK_EQ(values_of(rows, "Name"), (std::vector<std::string>{"Child", "Parent", "Early", "Late"}));
    CHECK_STR_EQ(rows[0].at("ParentUID"), std::string("105"));
    CHECK_EQ(rows[1].count("ParentUID"), 0u);
    CHECK_STR_EQ(rows[3].at("ParentUID"), std::string("106"));
}

// ---------------------------------------------------------------------------------------------------------------
// IMP-04: BidPageSettings canonicalisation (page_area_selection.py canonicalize_page_area_settings)
// ---------------------------------------------------------------------------------------------------------------

namespace
{
    Row setting(const std::string &uid, const std::string &page, const std::string &area, const std::string &selected)
    {
        Row r = row({{"UID", uid}, {"BidAreaUID", area}, {"BidAreaSelected", selected}});
        if (page != "<absent>")
            r["BidPageUID"] = page;
        return r;
    }
}

// IMP-04 test plan: rows (UID 50, page 20, selected 1), (51, 20, 1), (52, 20, 0) give [52, 51]: the inactive row first,
// then the later of two equally ranked selected rows (the plan notes it was run on the Windows importer).
TEST(ImportTransform, Imp04_InactiveRowsComeFirstAndTheLaterEqualRankedSelectionWins)
{
    const RowList result = ost_import::canonicalize_page_area_settings(
        {setting("50", "20", "1", "1"), setting("51", "20", "2", "1"), setting("52", "20", "3", "0")});
    CHECK_EQ(uid_list(result), (std::vector<std::string>{"52", "51"}));
}

// Port of tests/integration/import_export/test_page_area_selection.py::test_import_canonicalizes_duplicate_page_area_selection_then_save_succeeds
// (the canonicalisation half: ranks 1 and 2 on one page collapse to the higher ranked row).
TEST(ImportTransform, Imp04_DuplicatePageAreaSelectionCollapsesToTheHighestRank)
{
    const RowList result = ost_import::canonicalize_page_area_settings(
        {setting("30", "20", "10", "1"), setting("31", "20", "11", "2")});
    REQUIRE_EQ(result.size(), 1u);
    CHECK_STR_EQ(result[0].at("UID"), std::string("31"));
    CHECK_STR_EQ(result[0].at("BidAreaUID"), std::string("11"));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_page_uid_zero_is_never_a_page_and_collapses_as_no_page_selection
TEST(ImportTransform, Imp04_PageUidZeroIsNeverAPageAndCollapsesAsNoPageSelection)
{
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.bid_tables["BidPages"] = {row({{"UID", "0"}}), row({{"UID", "7"}})};
    const UidMap uid_map = ost_import::build_uid_map(raw, 100);
    CHECK(uid_map == (UidMap{{"1", "101"}, {"7", "102"}}));
    CHECK(ost_import::build_page_uid_map(raw, uid_map) == (UidMap{{"7", "102"}}));
    // Even a uid map that did contain 0 must not make it a page.
    UidMap with_zero = uid_map;
    with_zero["0"] = "999";
    CHECK(ost_import::build_page_uid_map(raw, with_zero) == (UidMap{{"7", "102"}}));

    RemapMaps maps;
    maps.uid = uid_map;
    maps.page = {{"7", "102"}};
    RowList remapped;
    for (const Row &r : {setting("30", "0", "4", "1"), setting("31", "0", "5", "1"), setting("32", "7", "4", "1")})
        remapped.push_back(ost_import::remap_row(r, maps));
    CHECK_EQ(values_of(remapped, "BidPageUID"), (std::vector<std::string>{"NULL", "NULL", "102"}));
    const RowList canonical = ost_import::canonicalize_page_area_settings(remapped);
    CHECK_EQ(values_of(canonical, "UID"), (std::vector<std::string>{"31", "32"}));
    CHECK_EQ(values_of(canonical, "BidPageUID"), (std::vector<std::string>{"NULL", "102"}));
}

// Port of tests/infrastructure/mdb/importers/test_ost_importer.py::test_ost_blank_and_zero_page_selections_leave_one_no_page_selection
TEST(ImportTransform, Imp04_BlankAndZeroPageSelectionsLeaveOneNoPageSelection)
{
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    raw.bid_tables["BidPages"] = {row({{"UID", "7"}}), row({{"UID", "8"}})};
    raw.page_tables["BidPageSettings"] = {setting("30", "", "4", "1"),         setting("31", "0", "5", "2"),
                                          setting("32", "<absent>", "6", "1"), setting("33", "7", "4", "1"),
                                          setting("34", "8", "4", "1"),        setting("35", "0", "7", "0")};
    UidMap uid_map = ost_import::build_uid_map(raw, 100);
    UidMap page_map = ost_import::build_page_uid_map(raw, uid_map);
    RemapMaps maps;
    maps.uid = uid_map;
    maps.page = page_map;
    OstRawData remapped = ost_import::remap_data(raw, maps);
    const RowList &settings = remapped.page_tables.find("BidPageSettings")->second;
    // The "0" row carries the highest selection rank of the no-page group.
    CHECK_EQ(values_of(settings, "UID"),
             (std::vector<std::string>{uid_map.at("35"), uid_map.at("31"), uid_map.at("33"), uid_map.at("34")}));
    CHECK_EQ(values_of(settings, "BidPageUID"),
             (std::vector<std::string>{"NULL", "NULL", page_map.at("7"), page_map.at("8")}));

    // Equal rank: the highest remapped UID of the group wins, whichever spelling it uses (here the blank one is last).
    raw.page_tables["BidPageSettings"][1]["BidAreaSelected"] = "1";
    raw.page_tables["BidPageSettings"][0]["UID"] = "29";
    uid_map = ost_import::build_uid_map(raw, 100);
    maps.uid = uid_map;
    maps.page = ost_import::build_page_uid_map(raw, uid_map);
    remapped = ost_import::remap_data(raw, maps);
    std::vector<std::string> no_page;
    for (const Row &r : remapped.page_tables.find("BidPageSettings")->second)
    {
        auto selected = r.find("BidAreaSelected");
        auto page = r.find("BidPageUID");
        const bool no_page_row = page == r.end() || page->second.empty() || page->second == "NULL";
        if (selected != r.end() && selected->second != "0" && no_page_row)
            no_page.push_back(r.at("UID"));
    }
    CHECK_EQ(no_page, (std::vector<std::string>{uid_map.at("32")}));
}

// remap_data always provides the BidPageSettings key, even when the file has none (Windows: the canonicalised list is
// assigned unconditionally).
TEST(ImportTransform, Imp04_RemapDataAlwaysSetsTheBidPageSettingsKey)
{
    OstRawData raw;
    raw.bid_row = row({{"UID", "1"}});
    const OstRawData remapped = ost_import::transform(raw, 100, {}, {}, {}, {});
    auto it = remapped.page_tables.find("BidPageSettings");
    REQUIRE(it != remapped.page_tables.end());
    CHECK(it->second.empty());
}

// Non-numeric BidAreaSelected counts as 0 (inactive); a numeric string with blanks is read like int().
TEST(ImportTransform, Imp04_NonNumericSelectionCountsAsInactive)
{
    const RowList result = ost_import::canonicalize_page_area_settings(
        {setting("1", "20", "1", "abc"), setting("2", "20", "1", " 3 "), setting("3", "20", "1", "NULL"),
         setting("4", "20", "1", "2.5")});
    CHECK_EQ(uid_list(result), (std::vector<std::string>{"1", "3", "4", "2"}));
}

// "Python str(value or '')": a page UID with other text is its own group; only "", "0", "NULL" and absent are no-page.
TEST(ImportTransform, Imp04_PageGroupKeyIsTheTextExceptForTheNoPageSpellings)
{
    const RowList result = ost_import::canonicalize_page_area_settings(
        {setting("1", "NULL", "1", "1"), setting("2", "", "2", "1"), setting("3", "5", "1", "1"),
         setting("4", "5 ", "1", "1"), setting("5", "5", "2", "1")});
    // groups: "" (rows 1,2 -> 2 wins, later), "5" (rows 3,5 -> 5 wins), "5 " (row 4)
    CHECK_EQ(uid_list(result), (std::vector<std::string>{"2", "5", "4"}));
}
