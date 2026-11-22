// IMP-09 end to end (alias DBP-M03): the flow of ImportService.mm for an OST file, parse -> validate_raw_data -> import_project, against a
// real database with the iOS schema. Windows: ost_importer.import_ost (_validated_raw_data before the writer) and the DBP-M03 test plan:
// a takeoff with BidPageUID 9 -> the import fails and nothing is written; BidConditionUID 9 -> fails; a duplicate takeoff UID -> fails;
// a takeoff whose parent is missing -> the import succeeds without that takeoff and its dependants; BidSettings.BidPageSelectedUID that
// points at a missing page -> stored as NULL.
#include "ImportTestSupport.hpp"
#include "Import/RawBidIntegrity.hpp"

#include <string>

using ost_itest::ScratchDb;
using ost_store::ImportResult;

namespace
{
    const char *CONDITIONS = "<BidConditions><BidCondition UID=\"10\" BidUID=\"1\" Name=\"Walls\"/></BidConditions>";

    // <Bid UID="1"> with the given settings, conditions and pages (the pages carry their takeoffs).
    std::string project(const std::string &settings, const std::string &pages, const std::string &conditions = CONDITIONS)
    {
        return "<XML_ROOT><Bid UID=\"1\" JobName=\"Imported\">" + settings + conditions + "<BidPages>" + pages + "</BidPages></Bid></XML_ROOT>";
    }

    std::string takeoff(const char *uid, const char *page, const char *condition, const char *parent = nullptr)
    {
        std::string xml = std::string("<BidTakeoff UID=\"") + uid + "\" BidUID=\"1\" BidPageUID=\"" + page + "\" BidConditionUID=\"" +
                          condition + "\" Position=\"p" + uid + "\"";
        if (parent)
            xml += std::string(" ParentUID=\"") + parent + "\"";
        return xml + "/>";
    }

    std::string page(const std::string &takeoffs)
    {
        return "<BidPage UID=\"20\" BidUID=\"1\" Name=\"Sheet\"><BidTakeoffs>" + takeoffs + "</BidTakeoffs></BidPage>";
    }

    // What ImportService does: parse, validate, store. `error` is the validation's ValueError text when it refuses.
    bool import_validated(const ScratchDb &db, const std::string &xml, std::string &error)
    {
        ost_import::OstRawData raw = ost_import::parse_ost_xml(xml);
        REQUIRE(!raw.empty());
        if (!ost_import::validate_raw_data(raw, error))
            return false;
        ImportResult result;
        return ost_store::import_project(db.path(), raw, &result);
    }
}

TEST(ImportValidationFlow, Imp09_AnInvalidProjectWritesNothing)
{
    struct Case
    {
        const char *name;
        std::string xml;
        std::string error;
    };
    const Case cases[] = {
        {"takeoff of a missing page", project("", page(takeoff("40", "9", "10"))), "The imported project contains invalid references."},
        {"takeoff of a missing condition", project("", page(takeoff("40", "20", "9"))), "The imported project contains invalid references."},
        {"duplicate takeoff uid", project("", page(takeoff("40", "20", "10") + takeoff("40", "20", "10"))),
         "The imported takeoff graph is invalid: BidTakeoffs.UID=40 occurs 2 times"},
        {"takeoff parent cycle", project("", page(takeoff("40", "20", "10", "41") + takeoff("41", "20", "10", "40"))),
         "The imported takeoff graph is invalid: BidTakeoffs.UID=40 participates in a ParentUID cycle; BidTakeoffs.UID=41 participates in "
         "a ParentUID cycle"},
        {"two settings rows",
         project("<BidSettings><BidSetting UID=\"30\" BidUID=\"1\"/><BidSetting UID=\"31\" BidUID=\"1\"/></BidSettings>", page("")),
         "The imported project contains invalid references."},
    };
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(c.name);
        ScratchDb db(std::string("valflow_reject_") + std::to_string(&c - cases), /*with_ios_schema=*/true);
        REQUIRE(db.schema_ok());
        std::string error;
        CHECK_FALSE(import_validated(db, c.xml, error));
        CHECK_STR_EQ(error, c.error);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 0);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidPages"), 0);
        CHECK_EQ(db.count("SELECT COUNT(*) FROM BidTakeoffs"), 0);
    }
}

// A takeoff whose parent is missing is not stored, nor are its dependants (Child(31, parent 30), Parent(30), Orphan(32, parent 999),
// Descendant(33, parent 32), Early(34), Late(35, parent 34) -> Child, Parent, Early, Late in file order).
TEST(ImportValidationFlow, Imp09_OrphanedTakeoffsAreSkippedNotStored)
{
    ScratchDb db("valflow_orphans", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    const std::string xml = project("", page(takeoff("31", "20", "10", "30") + takeoff("30", "20", "10") + takeoff("32", "20", "10", "999") +
                                                takeoff("33", "20", "10", "32") + takeoff("34", "20", "10") + takeoff("35", "20", "10", "34")));
    std::string error;
    REQUIRE(import_validated(db, xml, error));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 1);
    CHECK_EQ(db.rows("SELECT Position FROM BidTakeoffs ORDER BY rowid", 1), std::string("p31;p30;p34;p35"));
}

// BidSettings.BidPageSelectedUID that names a missing page is cleared: stored as NULL.
TEST(ImportValidationFlow, Imp09_AMissingSelectedPageIsStoredAsNull)
{
    ScratchDb db("valflow_selected", /*with_ios_schema=*/true);
    REQUIRE(db.schema_ok());
    const std::string xml = project("<BidSettings><BidSetting UID=\"30\" BidUID=\"1\" BidPageSelectedUID=\"999\"/></BidSettings>", page(""));
    std::string error;
    REQUIRE(import_validated(db, xml, error));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidSettings"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidSettings WHERE BidPageSelectedUID IS NULL"), 1);
}
