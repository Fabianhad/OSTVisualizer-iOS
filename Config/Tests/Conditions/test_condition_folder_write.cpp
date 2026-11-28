// CF-4, CF-5, CF-6, CF-7, CF-12 (plan 5.2): the store writers that create and rename a condition folder, and the Move to Folder of one condition,
// run through ost_store::create_condition_folder / rename_condition_folder / move_condition_to_folder on a real SQLite file (a TempDb), with the
// bid lock, the ownership checks, the name rules and the 2147483647 UID limit inside the transaction. The delete is in test_condition_folder_delete.cpp,
// the pure rules in test_condition_folder_rules.cpp, the 8000-deep chain in test_condition_folder_deep.cpp.
//
// Windows sources (HEAD 49a2d59):
//   tests/infrastructure/mdb/components/test_condition_folder_operations.py  (the persistence and contract tests; every ported test cites its line)
//   tests/integration/mdb/test_ownership_and_atomicity.py                    (:115 insert needs a bid, :475 orphan folder)
//   ost_visualizer/infrastructure/mdb/components/condition_folder_operations.py  :11-60 insert, :62-85 rename
//   ost_visualizer/presentation/handlers/condition_action_handler.py        :1577-1617 move a condition to a folder
//   tests/integration/permissions/test_toolbar_projection.py:464-499         (a locked bid blocks every folder write and the move)
//
// ADAPTATION of the ported fixtures: Windows creates `Bids (UID INTEGER)`; the iOS lock check reads Bids.JobStatusUID, so the minimal Bids tables
// of the ports are `Bids (UID INTEGER, JobStatusUID)` (see ConditionFoldersTestSupport.hpp). Every ported test says which Windows test it ports.
#include "ConditionFoldersTestSupport.hpp"
#include "ConditionFolderOracleData.hpp"

#include "Import/TextRules.hpp"
#include "Persistence/OstDatabase.hpp"
#include "Persistence/ProjectStore/ProjectStore_Integrity.hpp"

#include <chrono>
#include <functional>
#include <map>

using ost_ftest::add_condition;
using ost_ftest::add_folder;
using ost_ftest::file_exists;
using ost_ftest::folder_rows;
using ost_ftest::lock_bid;
using ost_ftest::seed_bids;
using ost_ftest::TempDb;
using ost_ftest::windows_folder_db;
using R = ost_store::ConditionFolderRefusal;

namespace
{
    ost_store::ConditionFolderResult create(const TempDb &db, const std::string &name, const std::string &parent = "", const std::string &bid = "1")
    {
        return ost_store::create_condition_folder(db.path(), bid, name, parent);
    }

    ost_store::ConditionFolderResult rename(const TempDb &db, const std::string &folder, const std::string &name, const std::string &bid = "1")
    {
        return ost_store::rename_condition_folder(db.path(), bid, folder, name);
    }

    ost_store::ConditionFolderResult move(const TempDb &db, const std::string &condition, const std::string &folder, const std::string &bid = "1")
    {
        return ost_store::move_condition_to_folder(db.path(), bid, condition, folder);
    }

    std::string repeat(const std::string &unit, std::size_t count)
    {
        std::string out;
        for (std::size_t i = 0; i < count; ++i)
            out += unit;
        return out;
    }

    const char *kEmoji = "\xF0\x9F\x98\x80"; // U+1F600: 2 UTF-16 units

    // The bytes of a text as upper-case hex (what SQLite's hex() gives): an exact, type- and encoding-proof comparison of a stored name.
    std::string hex_of(const std::string &text)
    {
        static const char *digits = "0123456789ABCDEF";
        std::string hex;
        for (const unsigned char c : text)
        {
            hex += digits[c >> 4];
            hex += digits[c & 15];
        }
        return hex;
    }

    // The iOS-schema world of the safety tests: bids 1 and 2; bid 1 has folders 10 'Alpha' (root), 11 'Child' (in 10), 12 'Other' (root) and
    // conditions 30 (no folder) and 31 (in folder 11); bid 2 has folder 20 'Alpha' (root) and condition 40 (in folder 20).
    void seed_world(const TempDb &db)
    {
        seed_bids(db);
        add_folder(db, 10, 1, "Alpha");
        add_folder(db, 11, 1, "Child", "10");
        add_folder(db, 12, 1, "Other");
        add_folder(db, 20, 2, "Alpha");
        add_condition(db, 30, 1);
        add_condition(db, 31, 1, "11");
        add_condition(db, 40, 2, "20");
    }

    // The status of bid 1 for the lock matrix.
    struct LockCase
    {
        const char *label;
        std::function<void(const TempDb &)> setup;
        bool locked;
    };

    const std::vector<LockCase> &lock_cases()
    {
        static const std::vector<LockCase> cases = {
            {"no job status (NULL)", [](const TempDb &) {}, false},
            {"empty job status ''", [](const TempDb &db) { db.must("UPDATE Bids SET JobStatusUID = '' WHERE UID = 1"); }, false},
            {"job status with Locked 0", [](const TempDb &db) { ost_ftest::set_job_status(db, 1, 4, "0"); }, false},
            {"job status with Locked -1", [](const TempDb &db) { lock_bid(db, 1, 5); }, true},
            {"job status that no JobStatuses row names (99)", [](const TempDb &db) { db.must("UPDATE Bids SET JobStatusUID = 99 WHERE UID = 1"); }, true},
            {"job status 0 (resolves to no row)", [](const TempDb &db) { db.must("UPDATE Bids SET JobStatusUID = 0 WHERE UID = 1"); }, true},
            {"JobStatuses table unreadable (dropped)",
             [](const TempDb &db)
             {
                 db.must("UPDATE Bids SET JobStatusUID = 5 WHERE UID = 1");
                 db.must("DROP TABLE JobStatuses");
             },
             true},
            {"an open status while another status is Locked",
             [](const TempDb &db)
             {
                 ost_ftest::set_job_status(db, 1, 5, "-1");
                 ost_ftest::set_job_status(db, 1, 4, "0"); // the bid points at the open one
             },
             false},
        };
        return cases;
    }
}

// =================================================================================================================================
// create: the ported Windows tests
// =================================================================================================================================

// Port of test_condition_folder_operations.py:446 test_a_new_folder_stores_every_column_and_binds_identifiers_as_integers: "Root" -> "1", "Kid"
// under "1" -> "2"; rows [(1,1,NULL,'Root',-1),(2,1,1,'Kid',-1)].
TEST(ConditionFolderCreate, ANewFolderStoresEveryColumn)
{
    TempDb db("fcreate_every_column", false);
    windows_folder_db(db);

    const auto root = create(db, "Root");
    CHECK_FOLDER_OK(root);
    CHECK_STR_EQ(root.uid, "1");
    const auto kid = create(db, "Kid", "1");
    CHECK_FOLDER_OK(kid);
    CHECK_STR_EQ(kid.uid, "2");
    CHECK_EQ(folder_rows(db), "1|1|NULL|'Root'|-1,2|1|1|'Kid'|-1");
}

// The "binds identifiers as integers" half of :446 (Windows asserts type(params[1]) is int): in a table WITHOUT column affinity a text bind would
// stay text, so typeof() shows how BidUID, ParentUID, UID and ExpandState were bound.
TEST(ConditionFolderCreate, BidUidParentUidAndUidAreBoundAsIntegers)
{
    TempDb db("fcreate_bind_types", false);
    windows_folder_db(db, "UID, BidUID, ParentUID, Name, ExpandState");

    CHECK_FOLDER_OK(create(db, "Root"));
    CHECK_FOLDER_OK(create(db, "Kid", "1"));
    CHECK_EQ(db.list("SELECT typeof(UID) || '/' || typeof(BidUID) || '/' || typeof(ParentUID) || '/' || typeof(Name) || '/' || typeof(ExpandState) "
                     "FROM BidConditionFolders ORDER BY UID"),
             "integer/integer/null/text/integer,integer/integer/integer/text/integer");
}

// Port of :169 test_insert_condition_folder_rejects_missing_or_cross_bid_parent: no parent row, or a parent (7) that belongs to bid 2: the insert for
// bid 1 under parent "7" is refused and the table is unchanged.
TEST(ConditionFolderCreate, AMissingOrCrossBidParentIsRefused)
{
    struct Variant
    {
        const char *label;
        bool parent_in_bid_two;
        R expected;
    };
    for (const Variant &variant : {Variant{"no parent row", false, R::ParentNotFound}, Variant{"parent 7 of bid 2", true, R::ParentOfAnotherBid}})
    {
        SCOPED_CONTEXT(variant.label);
        TempDb db("fcreate_bad_parent", false);
        windows_folder_db(db, ost_ftest::kWindowsFolderColumns, {2});
        if (variant.parent_in_bid_two)
            db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (7, 2, 'Other')");
        const std::string before = db.snapshot_all();

        CHECK_FOLDER_REFUSED(create(db, "Child", "7"), variant.expected);
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// Port of :200 test_insert_condition_folder_rejects_missing_bid_without_allocating: bid 99 has no Bids row -> refused, the table stays empty.
TEST(ConditionFolderCreate, ABidWithoutARowIsRefusedWithoutAllocating)
{
    TempDb db("fcreate_missing_bid", false);
    windows_folder_db(db);
    const std::string before = db.snapshot_all();

    const auto result = create(db, "Ownerless", "", "99");
    CHECK_FOLDER_REFUSED(result, R::BidNotFound);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 0);
}

// Port of tests/integration/mdb/test_ownership_and_atomicity.py:115 test_root_hierarchy_inserts_require_authoritative_bid_before_allocation (the
// folder half): an insert for bid 99 without a Bids row is refused and the table is empty; after `INSERT INTO Bids VALUES (99)` the same insert works
// and stores (99, "Owned folder").
TEST(ConditionFolderCreate, TheOwningBidMustExistBeforeAnythingIsAllocated)
{
    TempDb db("fcreate_orphan_owner", false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER, BidUID INTEGER, ParentUID INTEGER, Name TEXT)");

    CHECK_FOLDER_REFUSED(create(db, "Ownerless folder", "", "99"), R::BidNotFound);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 0);

    db.must("INSERT INTO Bids (UID) VALUES (99)");
    const auto owned = create(db, "Owned folder", "", "99");
    CHECK_FOLDER_OK(owned);
    CHECK_STR_EQ(owned.uid, "1");
    CHECK_EQ(db.list("SELECT BidUID || '/' || Name FROM BidConditionFolders"), "99/Owned folder");
}

// Port of :220 test_insert_condition_folder_accepts_same_bid_parent: folder 7 'Parent' of bid 1; insert 'Child' under "7" -> "8"; row (8, 1, 7).
TEST(ConditionFolderCreate, ASameBidParentIsAccepted)
{
    TempDb db("fcreate_same_bid_parent", false);
    windows_folder_db(db);
    db.must("INSERT INTO BidConditionFolders (UID, BidUID, Name) VALUES (7, 1, 'Parent')");

    const auto child = create(db, "Child", "7");
    CHECK_FOLDER_OK(child);
    CHECK_STR_EQ(child.uid, "8");
    CHECK_EQ(db.text("SELECT UID || ',' || BidUID || ',' || ParentUID FROM BidConditionFolders WHERE UID = 8"), "8,1,7");
}

// Port of :246 test_insert_condition_folder_rejects_parent_when_schema_is_flat: a table without ParentUID refuses a parent with the Windows text
// ("This OST database does not support Condition folder hierarchy persistence.") and leaves the rows; a root insert still works and gets "8".
TEST(ConditionFolderCreate, AFlatSchemaRefusesAParentButStoresARoot)
{
    TempDb db("fcreate_flat_schema", false);
    windows_folder_db(db, "UID INTEGER, BidUID INTEGER, Name TEXT, ExpandState INTEGER");
    db.must("INSERT INTO BidConditionFolders VALUES (7, 1, 'Parent', -1)");
    const std::string before = db.snapshot_all();

    const auto refused = create(db, "Child", "7");
    CHECK_FOLDER_REFUSED(refused, R::HierarchyNotSupported);
    CHECK_STR_EQ(refused.message, "This OST database does not support Condition folder hierarchy persistence.");
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.list("SELECT UID || '/' || BidUID || '/' || Name FROM BidConditionFolders ORDER BY UID"), "7/1/Parent");

    const auto root = create(db, "Root");
    CHECK_FOLDER_OK(root);
    CHECK_STR_EQ(root.uid, "8");
}

// Port of :280 test_condition_folder_insert_reserves_dangling_parent_uid: folder 7 has ParentUID 8 (no folder 8): the new root folder gets "9", the
// rows are [(7,8),(9,NULL)] and no folder joins a parent that is a row of its own.
TEST(ConditionFolderCreate, ADanglingParentUidIsReservedBeforeTheNewUid)
{
    TempDb db("fcreate_dangling_parent", false);
    windows_folder_db(db);
    db.must("INSERT INTO BidConditionFolders VALUES (7, 1, 8, 'Orphan', -1)");

    const auto created = create(db, "Unrelated");
    CHECK_FOLDER_OK(created);
    CHECK_STR_EQ(created.uid, "9");
    CHECK_EQ(db.list("SELECT UID || ':' || ifnull(ParentUID, 'NULL') FROM BidConditionFolders ORDER BY UID"), "7:8,9:NULL");
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders AS child INNER JOIN BidConditionFolders AS parent ON child.ParentUID = parent.UID"), 0);
}

// The other reference of the allocator (identity_allocation.py:5-47, BID_RELATIONSHIPS): a dangling BidConditions.BidConditionFolderUID = 12 makes the
// next uid 13 (the folder table is empty).
TEST(ConditionFolderCreate, ADanglingConditionFolderReferenceIsReservedToo)
{
    TempDb db("fcreate_dangling_condition_ref");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_condition(db, 30, 1, "12");

    const auto created = create(db, "Walls");
    CHECK_FOLDER_OK(created);
    CHECK_STR_EQ(created.uid, "13");
}

// Port of :469 test_a_flat_folder_table_reports_each_column_it_cannot_store: a table with UID, BidUID and Name only still takes the insert (ParentUID
// and ExpandState are skipped; Windows logs one skip each, the log is not portable, the stored row is asserted instead).
TEST(ConditionFolderCreate, AFlatFolderTableStoresWhatItCan)
{
    TempDb db("fcreate_flat_table", false);
    windows_folder_db(db, "UID INTEGER, BidUID INTEGER, Name TEXT");

    const auto root = create(db, "Root");
    CHECK_FOLDER_OK(root);
    CHECK_STR_EQ(root.uid, "1");
    CHECK_EQ(db.list("SELECT UID || '/' || BidUID || '/' || Name FROM BidConditionFolders"), "1/1/Root");
}

// Port of :481 test_a_folder_insert_needs_its_identity_and_name_columns: a table without UID, without BidUID or without Name refuses the insert and
// writes nothing (the Windows message `Missing BidConditionFolders.<column>` is a schema-inspector text: the outcome is ported, SchemaUnsupported).
TEST(ConditionFolderCreate, ATableWithoutItsIdentityOrNameColumnRefusesTheInsert)
{
    struct Variant
    {
        const char *columns;
        const char *missing;
    };
    for (const Variant &variant : {Variant{"BidUID INTEGER, Name TEXT", "UID"}, Variant{"UID INTEGER, Name TEXT", "BidUID"},
                                   Variant{"UID INTEGER, BidUID INTEGER", "Name"}})
    {
        SCOPED_CONTEXT(std::string("missing ") + variant.missing);
        TempDb db("fcreate_missing_column", false);
        windows_folder_db(db, variant.columns);
        const std::string before = db.snapshot_all();

        CHECK_FOLDER_REFUSED(create(db, "N"), R::SchemaUnsupported);
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// A file without the table at all (a database that is not an OST file) cannot take a folder: refused, nothing created.
TEST(ConditionFolderCreate, ADatabaseWithoutTheFolderTableRefusesTheInsert)
{
    TempDb db("fcreate_no_table", false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(create(db, "N"), R::SchemaUnsupported);
    CHECK_EQ(db.snapshot_all(), before);
}

// =================================================================================================================================
// create: the data-safety cases of the plan (names, ownership, uids)
// =================================================================================================================================

// A bid uid that Windows would refuse (require_valid_unique_bid_owned_uids: ASCII decimal digits, greater than zero) is InvalidUid; nothing is
// written and no uid is burnt: the next good create gets the uid this one would have had.
TEST(ConditionFolderCreate, ABadBidUidIsRefusedAndBurnsNoUid)
{
    TempDb db("fcreate_bad_bid_uid");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    const std::string before = db.snapshot_all();

    for (const std::string bid : {"", "0", "-1", " 7", "1 ", "abc", "1.0", "+1", "9223372036854775808", "99999999999999999999"})
    {
        SCOPED_CONTEXT("bid uid " + ost_test::detail::quote(bid));
        CHECK_FOLDER_REFUSED(create(db, "Walls", "", bid), R::InvalidUid);
        CHECK_EQ(db.snapshot_all(), before);
    }
    const auto good = create(db, "Walls");
    CHECK_FOLDER_OK(good);
    CHECK_STR_EQ(good.uid, "1");
}

TEST(ConditionFolderCreate, ABadParentUidIsRefused)
{
    TempDb db("fcreate_bad_parent_uid");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    // "0" is refused although the loader reads a ParentUID of 0 as the root (Windows condition_folder_operations.py:25-36, oddity W14): the root is ""
    for (const std::string parent : {"0", "-1", " 10", "10 ", "abc", "1.5", "99999999999999999999"})
    {
        SCOPED_CONTEXT("parent uid " + ost_test::detail::quote(parent));
        CHECK_FOLDER_REFUSED(create(db, "Walls", parent), R::InvalidUid);
        CHECK_EQ(db.snapshot_all(), before);
    }
}

TEST(ConditionFolderCreate, AParentThatIsMissingOrOfAnotherBidIsRefusedOnTheIosSchema)
{
    TempDb db("fcreate_parent_ownership");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(create(db, "Walls", "999"), R::ParentNotFound);
    CHECK_FOLDER_REFUSED(create(db, "Walls", "20"), R::ParentOfAnotherBid); // folder 20 is bid 2's
    CHECK_FOLDER_REFUSED(create(db, "Walls", "10", "2"), R::ParentOfAnotherBid); // the same folder from the other side
    CHECK_FOLDER_REFUSED(create(db, "Walls", "999", "2"), R::ParentNotFound);
    CHECK_EQ(db.snapshot_all(), before);
}

// A new folder is stored stripped, with ExpandState -1, Description NULL, ParentUID NULL for a root (conditions_sidebar.py / condition_folder_operations.py
// :41-54); nothing else of the database changes.
TEST(ConditionFolderCreate, ANewFolderIsStoredStrippedWithExpandStateMinusOne)
{
    TempDb db("fcreate_row");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string conditions_before = db.dump("BidConditions");
    const std::string bids_before = db.dump("Bids");

    const auto created = create(db, "  Walls \t", "10");
    CHECK_FOLDER_OK(created);
    CHECK_STR_EQ(created.uid, "21");
    CHECK_EQ(db.row("BidConditionFolders", "21"), "UID=21 BidUID=1 ParentUID=10 Name='Walls' Description=NULL ExpandState=-1");
    const auto root = create(db, "Root folder");
    CHECK_FOLDER_OK(root);
    CHECK_STR_EQ(root.uid, "22");
    CHECK_EQ(db.row("BidConditionFolders", "22"), "UID=22 BidUID=1 ParentUID=NULL Name='Root folder' Description=NULL ExpandState=-1");
    CHECK_EQ(db.dump("BidConditions"), conditions_before);
    CHECK_EQ(db.dump("Bids"), bids_before);
}

// CF-12: the Windows store accepts any name, blank included; the iOS Core strips and refuses a blank one (every Python str.isspace() character, so
// U+0085 and U+001C..U+001F too), invalid UTF-8 and a NUL: nothing is written.
TEST(ConditionFolderCreate, ABlankOrUnreadableNameIsRefusedBeforeAnyWrite)
{
    TempDb db("fcreate_bad_names");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    for (const std::string name : {"", " ", "   ", "\t\r\n", "\xC2\x85", "\x1c\x1d\x1e\x1f", "\xE3\x80\x80", "\xC2\xA0 \xE2\x80\xA8"})
    {
        SCOPED_CONTEXT("blank name " + ost_test::detail::quote(name));
        CHECK_FOLDER_REFUSED(create(db, name), R::BlankName);
        CHECK_EQ(db.snapshot_all(), before);
    }
    for (const std::string &name : {std::string("\xC0\x80"), std::string("\xED\xA0\x80"), std::string("A\xFF"), std::string("A\0B", 3), std::string("\0", 1)})
    {
        SCOPED_CONTEXT("unreadable name " + ost_test::detail::quote(name));
        CHECK_FOLDER_REFUSED(create(db, name), R::InvalidName);
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// CF-6 / N-M1-13: 50 UTF-16 units are stored, 51 are refused with the Domain text before any write (an emoji counts 2: 25 fit, 26 do not; the
// strip comes first). The text is the exact name stored: the byte-for-byte comparison is hex().
TEST(ConditionFolderCreate, TheNameLimitIsFiftyUtf16Units)
{
    TempDb db("fcreate_name_limit");
    REQUIRE(db.schema_ok());
    seed_bids(db);

    struct Row
    {
        std::string typed;
        bool ok;
    };
    const std::vector<Row> rows = {
        {repeat("A", 51), false},
        {repeat(kEmoji, 26), false},
        {repeat(kEmoji, 24) + "aaa", false},
        {repeat("\xC3\xA9", 51), false},
        {repeat("e\xCC\x81", 26), false},
        {" " + repeat("A", 50) + " ", true},
        {repeat(kEmoji, 25), true},
        {repeat(kEmoji, 24) + "aa", true},
        {repeat("\xC3\xA9", 50), true},
        {repeat("e\xCC\x81", 25), true},
    };
    int index = 0;
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT("row " + std::to_string(index++));
        const std::string before = db.snapshot_all();
        const auto result = create(db, row.typed);
        if (row.ok)
        {
            CHECK_FOLDER_OK(result);
            CHECK_EQ(db.text("SELECT hex(Name) FROM BidConditionFolders WHERE UID = " + result.uid), hex_of(ost_import::text_rules::py_strip(row.typed)));
        }
        else
        {
            CHECK_FOLDER_REFUSED(result, R::NameTooLong);
            CHECK_STR_EQ(result.message, "Folder name must not be longer than 50 characters.");
            CHECK_EQ(db.snapshot_all(), before);
        }
    }
}

// Unicode names round-trip byte for byte (Windows reads folder names decoded from bytes, test_bid_data_reader.py:4045).
TEST(ConditionFolderCreate, UnicodeNamesRoundTripByteForByte)
{
    TempDb db("fcreate_unicode");
    REQUIRE(db.schema_ok());
    seed_bids(db);

    const std::vector<std::pair<std::string, std::string>> names = {
        {"\xC3\x89p\xC3\xA9" "e", "C389" "70" "C3A9" "65"},                 // Épée
        {std::string("A") + kEmoji + "B", "41" "F09F9880" "42"},            // A😀B
        {"\xE4\xB8\xAD\xE6\x96\x87", "E4B8AD" "E69687"},                    // 中文
        {"e\xCC\x81", "65" "CC81"},                                         // e + combining acute (not normalised)
        {"a\tb", "61" "09" "62"},                                           // a tab inside a name is kept
        {"O'Brien", "4F" "27" "4272" "69656E"},                             // an apostrophe is data, not SQL
        {"x'); DROP TABLE Bids; --", "78" "27" "29" "3B" "2044524F5020544142" "4C4520" "42696473" "3B" "202D2D"},
    };
    for (const auto &row : names)
    {
        SCOPED_CONTEXT("name " + row.second);
        const auto result = create(db, row.first);
        CHECK_FOLDER_OK(result);
        CHECK_STR_EQ(db.text("SELECT hex(Name) FROM BidConditionFolders WHERE UID = " + result.uid), row.second);
    }
    CHECK_EQ(db.count("SELECT COUNT(*) FROM Bids"), 2); // the second name was text, not a statement
}

// Windows accepts a bid-owned uid with leading zeros ("007" is 7: require_valid_unique_bid_owned_uids reads ASCII digits, the number is what counts):
// every uid text of every writer may carry them and means the same row.
TEST(ConditionFolderCreate, LeadingZerosInUidsMeanTheSameRows)
{
    TempDb db("fcreate_leading_zeros");
    REQUIRE(db.schema_ok());
    seed_world(db);

    const auto child = create(db, "Walls", "010", "001");
    CHECK_FOLDER_OK(child);
    CHECK_STR_EQ(child.uid, "21");
    CHECK_EQ(db.row("BidConditionFolders", "21"), "UID=21 BidUID=1 ParentUID=10 Name='Walls' Description=NULL ExpandState=-1");
    CHECK_FOLDER_OK(rename(db, "021", "Walls two", "01"));
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 21"), "Walls two");
    CHECK_FOLDER_OK(move(db, "030", "021", "1"));
    CHECK_EQ(db.text("SELECT BidConditionFolderUID FROM BidConditions WHERE UID = 30"), "21");
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "01", {"0021"}), R::InUse);
    CHECK_FOLDER_OK(move(db, "30", "", "0001"));
    CHECK_FOLDER_OK(ost_store::delete_condition_folders(db.path(), "01", {"0021"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID = 21"), 0);
}

// CF-5 (plan N-M1-15): the sibling rule of the rename also holds at create. The same name (strip + Python lower()) among the folders with the same
// effective parent is refused with the Windows text; another parent, another bid, or the parent's own name are fine.
TEST(ConditionFolderCreate, ASiblingWithTheSameNameIsRefusedAtCreate)
{
    TempDb db("fcreate_sibling");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 10, 1, "Walls");                // root
    add_folder(db, 11, 1, "Floors", "10");         // child of Walls
    add_folder(db, 12, 1, "Dangling", "999");      // listed at the root: its parent is no folder
    add_folder(db, 13, 1, "Zero", "'0'");          // ParentUID '0' reads as the root
    add_folder(db, 14, 1, "Padded ");              // a stored name with a trailing space
    add_folder(db, 20, 2, "Elsewhere");            // bid 2

    struct Row
    {
        const char *name;
        const char *parent;
        bool refused;
    };
    const Row rows[] = {
        {"walls", "", true},       {"WALLS ", "", true},     {" Walls", "", true},      {"dangling", "", true},    {"ZERO", "", true},
        {"padded", "", true},      {"floors", "10", true},   {"Floors", "", false},     {"walls", "10", false},    {"Walls", "11", false},
        {"elsewhere", "", false},  {"Walls2", "", false},
    };
    for (const Row &row : rows)
    {
        SCOPED_CONTEXT(std::string("name '") + row.name + "' parent '" + row.parent + "'");
        const std::string before = db.snapshot_all();
        const auto result = create(db, row.name, row.parent);
        if (row.refused)
        {
            CHECK_FOLDER_REFUSED(result, R::DuplicateName);
            std::string stripped = row.name;
            while (!stripped.empty() && stripped.back() == ' ')
                stripped.pop_back();
            while (!stripped.empty() && stripped.front() == ' ')
                stripped.erase(stripped.begin());
            CHECK_STR_EQ(result.message, "Folder " + stripped + " already exists.");
            CHECK_EQ(db.snapshot_all(), before);
        }
        else
        {
            CHECK_FOLDER_OK(result);
        }
    }
}

// CF-5 on the existing duplicates of a Windows file: two root folders that already share a name stay as they are; the sibling rule only looks at the
// new name.
TEST(ConditionFolderCreate, ExistingDuplicatesDoNotBlockOtherNames)
{
    TempDb db("fcreate_existing_dups");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 10, 1, "Same");
    add_folder(db, 11, 1, "Same");

    CHECK_FOLDER_OK(create(db, "Different"));
    CHECK_FOLDER_REFUSED(create(db, "SAME"), R::DuplicateName);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 3);
}

// Order of the checks of plan 4.4 item 2: the uids first, then the ownership (the bid and the parent), then the lock, then the name rules. A
// missing bid is BidNotFound (even if its lock cannot be read); a foreign parent on a locked bid is still ParentOfAnotherBid; a blank name on a
// locked bid is BidLocked (the lock is checked before the name).
TEST(ConditionFolderCreate, TheChecksRunInTheOrderUidOwnershipLockName)
{
    TempDb db("fcreate_order");
    REQUIRE(db.schema_ok());
    seed_world(db);
    lock_bid(db, 1);

    CHECK_FOLDER_REFUSED(create(db, "Walls", "", "99"), R::BidNotFound);
    CHECK_FOLDER_REFUSED(create(db, "Walls", "20"), R::ParentOfAnotherBid);
    CHECK_FOLDER_REFUSED(create(db, "Walls", "999"), R::ParentNotFound);
    CHECK_FOLDER_REFUSED(create(db, "   "), R::BidLocked);
    CHECK_FOLDER_REFUSED(create(db, repeat("A", 60)), R::BidLocked);
    CHECK_FOLDER_REFUSED(create(db, "Alpha"), R::BidLocked); // a duplicate name on a locked bid is BidLocked, not DuplicateName
    CHECK_FOLDER_REFUSED(create(db, "Walls", "abc"), R::InvalidUid);
}

// =================================================================================================================================
// create: the 2147483647 limit (plan 4.2, N-M1-04) and the allocator
// =================================================================================================================================

// A new folder may take the uid 2147483647 and not one more: with the highest folder uid at 2147483646 the create answers "2147483647"; the next one is
// refused with the "no identity" text and writes nothing.
TEST(ConditionFolderCreate, TheLastUidBelowTheAccessLimitIsAllowedAndTheNextRefused)
{
    TempDb db("fcreate_limit_edge");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 2147483646LL, 1, "Near the end");

    const auto last = create(db, "Last");
    CHECK_FOLDER_OK(last);
    CHECK_STR_EQ(last.uid, "2147483647");

    const std::string before = db.snapshot_all();
    const auto past = create(db, "Past the end");
    CHECK_FOLDER_REFUSED(past, R::UidExhausted);
    CHECK_STR_EQ(past.message, "Cannot add a folder: no identity is left below 2147483647.");
    CHECK_EQ(db.snapshot_all(), before);
}

// The reservation of dangling references reaches the limit too: a folder row whose ParentUID is 2147483647, or a condition whose folder uid is
// 2147483647, makes the next uid 2147483648: refused. A highest uid of 2^63 - 1 is the generic failure (the allocator cannot add one), not the text.
TEST(ConditionFolderCreate, DanglingReferencesAndTheLargestUidAreRefusedBeforeAnyWrite)
{
    {
        TempDb db("fcreate_limit_parent_ref");
        REQUIRE(db.schema_ok());
        seed_bids(db);
        add_folder(db, 5, 1, "Orphan", "2147483647");
        const std::string before = db.snapshot_all();
        const auto result = create(db, "Walls");
        CHECK_FOLDER_REFUSED(result, R::UidExhausted);
        CHECK_STR_EQ(result.message, "Cannot add a folder: no identity is left below 2147483647.");
        CHECK_EQ(db.snapshot_all(), before);
    }
    {
        TempDb db("fcreate_limit_condition_ref");
        REQUIRE(db.schema_ok());
        seed_bids(db);
        add_condition(db, 30, 1, "2147483647");
        const std::string before = db.snapshot_all();
        CHECK_FOLDER_REFUSED(create(db, "Walls"), R::UidExhausted);
        CHECK_EQ(db.snapshot_all(), before);
    }
    {
        TempDb db("fcreate_limit_int64_max");
        REQUIRE(db.schema_ok());
        seed_bids(db);
        add_folder(db, 9223372036854775807LL, 1, "Largest");
        const std::string before = db.snapshot_all();
        const auto result = create(db, "Walls");
        CHECK_FOLDER_REFUSED(result, R::WriteFailed);
        CHECK_STR_EQ(result.message, ""); // the generic failure: not the "no identity" text
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// The limit is only about NEW uids: with a folder at 2147483647 rename, move-into and delete still work (they allocate nothing).
TEST(ConditionFolderCreate, TheLimitDoesNotBlockTheWritersThatAllocateNothing)
{
    TempDb db("fcreate_limit_others");
    REQUIRE(db.schema_ok());
    seed_bids(db);
    add_folder(db, 2147483647LL, 1, "Big");
    add_condition(db, 30, 1);

    CHECK_FOLDER_OK(rename(db, "2147483647", "Bigger"));
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 2147483647"), "Bigger");
    CHECK_FOLDER_OK(move(db, "30", "2147483647"));
    CHECK_EQ(db.text("SELECT BidConditionFolderUID FROM BidConditions WHERE UID = 30"), "2147483647");
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "1", {"2147483647"}), R::InUse);
    CHECK_FOLDER_OK(move(db, "30", ""));
    CHECK_FOLDER_OK(ost_store::delete_condition_folders(db.path(), "1", {"2147483647"}));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders"), 0);
}

// =================================================================================================================================
// create: rollback, failed reads, busy database
// =================================================================================================================================

// Failure injection (the device of tests/integration/mdb/test_ownership_and_atomicity.py:154): a BEFORE INSERT trigger that aborts the insert: the
// create fails, the database is byte-identical, the write lock is released (another connection writes at once), no uid is burnt.
TEST(ConditionFolderCreate, AFailedInsertRollsBackAndReleasesTheLock)
{
    TempDb db("fcreate_rollback");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    const std::string trigger = db.block("BidConditionFolders", "INSERT");
    CHECK_FOLDER_REFUSED(create(db, "Walls"), R::WriteFailed);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK(db.exec("UPDATE Bids SET JobName = 'still writable' WHERE UID = 2")); // would wait 5 s and fail if a transaction were left open
    db.must("UPDATE Bids SET JobName = 'Bid two' WHERE UID = 2");

    db.unblock(trigger);
    const auto retry = create(db, "Walls");
    CHECK_FOLDER_OK(retry);
    CHECK_STR_EQ(retry.uid, "21"); // the failed call consumed nothing
}

// A database whose file cannot be opened (the folder of the path does not exist) refuses every writer; nothing is created and nothing throws.
TEST(ConditionFolderCreate, AnUnopenableDatabaseRefusesEveryWriter)
{
    const std::string path = ost_test::temp_path("folders_no_such_dir_" + ost_ctest::run_token() + "/folders.db");
    CHECK_FOLDER_REFUSED(ost_store::create_condition_folder(path, "1", "Walls", ""), R::WriteFailed);
    CHECK_FOLDER_REFUSED(ost_store::rename_condition_folder(path, "1", "10", "Walls"), R::WriteFailed);
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(path, "1", {"10"}), R::WriteFailed);
    CHECK_FOLDER_REFUSED(ost_store::move_condition_to_folder(path, "1", "30", "10"), R::WriteFailed);
    CHECK_FALSE(file_exists(path));
}

// Another connection holds the write lock for longer than the busy timeout (OstDatabase::kBusyTimeoutMs, 5000): the create is refused (WriteFailed),
// nothing is written; after the holder lets go the same call works.
TEST(ConditionFolderCreate, ABusyDatabaseRefusesTheCreateAndWritesNothing)
{
    TempDb db("fcreate_busy");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    ost_db::OstDatabase holder;
    REQUIRE(holder.open(db.path()));
    REQUIRE(holder.begin_transaction());
    const auto started = std::chrono::steady_clock::now();
    const auto refused = create(db, "Walls");
    const auto waited_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    CHECK_FOLDER_REFUSED(refused, R::WriteFailed);
    CHECK(waited_ms >= 4000); // it waited for the lock (the busy timeout) instead of failing at once
    REQUIRE(holder.rollback());
    holder.close();
    CHECK_EQ(db.snapshot_all(), before);

    CHECK_FOLDER_OK(create(db, "Walls"));
}

// A lock that appears while the screen's alert is open must not let the write through (plan 5.2 / test_conditions_sidebar.py:1220-1230 intent):
// the same create works, the bid is locked, the same create is refused, the lock is lifted, it works again.
TEST(ConditionFolderCreate, ALockThatAppearsBetweenTwoCallsStopsTheSecond)
{
    TempDb db("fcreate_lock_appears");
    REQUIRE(db.schema_ok());
    seed_bids(db);

    CHECK_FOLDER_OK(create(db, "First"));
    lock_bid(db, 1);
    const std::string before = db.snapshot_all();
    CHECK_FOLDER_REFUSED(create(db, "Second"), R::BidLocked);
    CHECK_EQ(db.snapshot_all(), before);
    db.must("UPDATE JobStatuses SET Locked = 0 WHERE UID = 5");
    CHECK_FOLDER_OK(create(db, "Second"));
}

// =================================================================================================================================
// rename: the ported Windows tests
// =================================================================================================================================

// Port of :311 test_rename_condition_folder_updates_only_target_name: folders 1 'Old', 2 'Other'; rename 1 -> 'New': [(1,'New'),(2,'Other')].
TEST(ConditionFolderRename, OnlyTheNameOfTheTargetChanges)
{
    TempDb db("frename_target_only", false);
    windows_folder_db(db, "UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT");
    db.must("INSERT INTO BidConditionFolders VALUES (1, 1, 'Old')");
    db.must("INSERT INTO BidConditionFolders VALUES (2, 1, 'Other')");

    CHECK_FOLDER_OK(rename(db, "1", "New"));
    CHECK_EQ(db.list("SELECT UID || ':' || Name FROM BidConditionFolders ORDER BY UID"), "1:New,2:Other");
}

// Port of :331 test_rename_condition_folder_rejects_missing_folder: uid 9 has no row -> refused, the names are unchanged.
TEST(ConditionFolderRename, AMissingFolderIsRefused)
{
    TempDb db("frename_missing", false);
    windows_folder_db(db, "UID INTEGER PRIMARY KEY, BidUID INTEGER, Name TEXT");
    db.must("INSERT INTO BidConditionFolders VALUES (1, 1, 'Old')");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(rename(db, "9", "New"), R::FolderNotFound);
    CHECK_EQ(db.snapshot_all(), before);
}

// Port of :507 test_a_rename_binds_the_folder_as_an_integer_and_changes_the_name: folder 5 'Old' -> 'New' (Windows asserts the UPDATE binds
// ("New", 5) with an int). The table has no column affinity, so the uid in the WHERE must be bound as an integer to match the stored integer 5.
TEST(ConditionFolderRename, TheFolderUidIsBoundAsAnInteger)
{
    TempDb db("frename_bind_int", false);
    windows_folder_db(db, "UID, BidUID, ParentUID, Name, ExpandState");
    db.must("INSERT INTO BidConditionFolders VALUES (5, 1, NULL, 'Old', -1)");

    const auto result = rename(db, "5", "New");
    CHECK_FOLDER_OK(result);
    CHECK_STR_EQ(result.uid, "5");
    CHECK_EQ(db.list("SELECT Name FROM BidConditionFolders"), "New");
    CHECK_EQ(db.list("SELECT typeof(UID) || '/' || typeof(BidUID) || '/' || quote(ExpandState) FROM BidConditionFolders"), "integer/integer/-1");
}

// Port of :523 test_rename_and_delete_need_the_columns_they_write (the rename half): a table without UID, or without Name, refuses the rename.
TEST(ConditionFolderRename, TheColumnsItWritesAreRequired)
{
    struct Variant
    {
        const char *columns;
        const char *missing;
    };
    for (const Variant &variant : {Variant{"BidUID INTEGER, Name TEXT", "UID"}, Variant{"UID INTEGER, BidUID INTEGER", "Name"}})
    {
        SCOPED_CONTEXT(std::string("missing ") + variant.missing);
        TempDb db("frename_missing_column", false);
        windows_folder_db(db, variant.columns);
        const std::string before = db.snapshot_all();
        CHECK_FOLDER_REFUSED(rename(db, "5", "N"), R::SchemaUnsupported);
        CHECK_EQ(db.snapshot_all(), before);
    }
}

// Port of :549 test_failures_return_false_and_follow_the_error_policy (the rename half): rename of uid 99 -> refused (FolderNotFound).
TEST(ConditionFolderRename, AFailureIsARefusalNotAnException)
{
    TempDb db("frename_failure", false);
    windows_folder_db(db);
    CHECK_FOLDER_REFUSED(rename(db, "99", "N"), R::FolderNotFound);
}

// Port of tests/integration/mdb/test_ownership_and_atomicity.py:475 test_orphan_layer_and_folder_mutations_reject_before_write (the rename half):
// folder 8 belongs to bid 99, which has no Bids row: the rename is refused (Windows "Bids has no row for UID 99") and the name is kept; once the
// owning bid exists the same rename succeeds.
TEST(ConditionFolderRename, AFolderOfABidWithoutARowIsRefusedBeforeTheWrite)
{
    TempDb db("frename_orphan", false);
    db.must("CREATE TABLE Bids (UID INTEGER, JobStatusUID)");
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER, BidUID INTEGER, Name TEXT, ParentUID INTEGER)");
    db.must("INSERT INTO BidConditionFolders VALUES (8, 99, 'Orphan folder', NULL)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(rename(db, "8", "Changed", "99"), R::BidNotFound);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 8"), "Orphan folder");

    db.must("INSERT INTO Bids (UID) VALUES (99)");
    CHECK_FOLDER_OK(rename(db, "8", "Renamed", "99"));
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 8"), "Renamed");
}

// =================================================================================================================================
// rename: the name rule through the store (the same Windows oracle as the pure rule) and the data-safety cases
// =================================================================================================================================

// Every row of the Windows sidebar oracle through rename_condition_folder, with the folder uids made numeric: 'W' -> the stripped name is stored,
// 'S' -> a blank text is refused (BlankName), the stored name again is a success that changes nothing, 'D' -> DuplicateName with the Windows text.
// (A duplicate warning of Windows is shown by the sidebar; the store re-checks the same rule from the database rows of the bid.)
TEST(ConditionFolderRename, TheRenameRuleOfTheSidebarHoldsInTheStoreOnEveryOracleRow)
{
    TempDb db("frename_oracle");
    REQUIRE(db.schema_ok());
    seed_bids(db);

    int index = 0;
    for (const ost_ftest::OracleRenameCase &row : ost_ftest::rename_cases())
    {
        SCOPED_CONTEXT("oracle row " + std::to_string(index++) + " group " + row.group + " typed " + ost_test::detail::quote(row.typed));
        db.must("DELETE FROM BidConditionFolders");
        std::map<std::string, long long> numeric;
        long long next = 100;
        for (const ost_ftest::OracleFolder &folder : row.folders)
            numeric[folder.uid] = next++;
        for (const ost_ftest::OracleFolder &folder : row.folders)
        {
            const std::string parent = folder.parent;
            std::string literal = "NULL";
            if (!parent.empty())
                literal = parent == "0" ? std::string("'0'") : std::to_string(numeric.count(parent) ? numeric[parent] : 9999);
            db.must("INSERT INTO BidConditionFolders (UID, BidUID, ParentUID, Name, ExpandState) VALUES (" + std::to_string(numeric[folder.uid]) +
                    ", 1, " + literal + ", CAST(x'" + hex_of(folder.name) + "' AS TEXT), -1)");
        }
        const std::string uid = std::to_string(numeric[row.folder_uid]);
        const std::string stored_before = db.text("SELECT hex(Name) FROM BidConditionFolders WHERE UID = " + uid);
        const std::string others_before = db.list("SELECT UID || hex(Name) FROM BidConditionFolders WHERE UID <> " + uid + " ORDER BY UID");
        const auto result = rename(db, uid, row.typed);
        if (row.outcome == 'W')
        {
            CHECK_FOLDER_OK(result);
            CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = " + uid), std::string(row.text1));
        }
        else if (row.outcome == 'D')
        {
            CHECK_FOLDER_REFUSED(result, R::DuplicateName);
            CHECK_STR_EQ(result.message, row.text2);
            CHECK_EQ(db.text("SELECT hex(Name) FROM BidConditionFolders WHERE UID = " + uid), stored_before);
        }
        else if (ost_import::text_rules::py_strip(row.typed).empty())
        {
            CHECK_FOLDER_REFUSED(result, R::BlankName);
            CHECK_EQ(db.text("SELECT hex(Name) FROM BidConditionFolders WHERE UID = " + uid), stored_before);
        }
        else
        {
            CHECK_FOLDER_OK(result);
            CHECK_EQ(db.text("SELECT hex(Name) FROM BidConditionFolders WHERE UID = " + uid), stored_before);
        }
        CHECK_EQ(db.list("SELECT UID || hex(Name) FROM BidConditionFolders WHERE UID <> " + uid + " ORDER BY UID"), others_before);
    }
}

// Only the Name of that row changes: every other column of the row, every other row, and every other table stay identical.
TEST(ConditionFolderRename, OnlyTheNameColumnOfTheTargetRowChanges)
{
    TempDb db("frename_only_name");
    REQUIRE(db.schema_ok());
    seed_world(db);
    db.must("UPDATE BidConditionFolders SET ExpandState = 0, Description = x'0102' WHERE UID = 10");
    const std::string others_before = db.list("SELECT UID || '|' || quote(BidUID) || '|' || quote(ParentUID) || '|' || quote(Name) || '|' || quote(ExpandState) "
                                              "FROM BidConditionFolders WHERE UID <> 10 ORDER BY UID");
    const std::string row_before = db.row("BidConditionFolders", "10", {"Name"});
    const std::string conditions_before = db.dump("BidConditions");
    const std::string bids_before = db.dump("Bids");

    const auto result = rename(db, "10", "  Renamed  ");
    CHECK_FOLDER_OK(result);
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 10"), "Renamed");
    CHECK_EQ(db.row("BidConditionFolders", "10", {"Name"}), row_before);
    CHECK_EQ(db.list("SELECT UID || '|' || quote(BidUID) || '|' || quote(ParentUID) || '|' || quote(Name) || '|' || quote(ExpandState) "
                     "FROM BidConditionFolders WHERE UID <> 10 ORDER BY UID"),
             others_before);
    CHECK_EQ(db.dump("BidConditions"), conditions_before);
    CHECK_EQ(db.dump("Bids"), bids_before);
}

// Windows rename has no bid scope (CF-4, register: one existing bid for the batch is enough): iOS takes the bid and refuses a folder of ANOTHER bid,
// a missing folder, a malformed uid (a folder or a bid) and a missing bid. Nothing is written.
TEST(ConditionFolderRename, TheFolderMustBelongToTheOpenBid)
{
    TempDb db("frename_ownership");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(rename(db, "20", "Renamed"), R::FolderOfAnotherBid);       // bid 2's folder through bid 1
    CHECK_FOLDER_REFUSED(rename(db, "10", "Renamed", "2"), R::FolderOfAnotherBid);  // bid 1's folder through bid 2
    CHECK_FOLDER_REFUSED(rename(db, "999", "Renamed"), R::FolderNotFound);
    CHECK_FOLDER_REFUSED(rename(db, "10", "Renamed", "99"), R::BidNotFound);
    for (const std::string folder : {"", "0", "-1", " 10", "10 ", "abc", "1.0", "99999999999999999999"})
    {
        SCOPED_CONTEXT("folder uid " + ost_test::detail::quote(folder));
        CHECK_FOLDER_REFUSED(rename(db, folder, "Renamed"), R::InvalidUid);
    }
    for (const std::string bid : {"", "0", "x"})
    {
        SCOPED_CONTEXT("bid uid " + ost_test::detail::quote(bid));
        CHECK_FOLDER_REFUSED(rename(db, "10", "Renamed", bid), R::InvalidUid);
    }
    CHECK_EQ(db.snapshot_all(), before);
}

// Blank, unreadable, too long and duplicate names are refused with their own reason; the stored name stays.
TEST(ConditionFolderRename, BlankUnreadableTooLongAndDuplicateNamesAreRefused)
{
    TempDb db("frename_refusals");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(rename(db, "10", "   "), R::BlankName);
    CHECK_FOLDER_REFUSED(rename(db, "10", "\xC2\x85\x1c"), R::BlankName);
    CHECK_FOLDER_REFUSED(rename(db, "10", std::string("A\0B", 3)), R::InvalidName);
    CHECK_FOLDER_REFUSED(rename(db, "10", "A\xFF"), R::InvalidName);
    const auto too_long = rename(db, "10", repeat("A", 51));
    CHECK_FOLDER_REFUSED(too_long, R::NameTooLong);
    CHECK_STR_EQ(too_long.message, "Folder name must not be longer than 50 characters.");
    CHECK_FOLDER_REFUSED(rename(db, "10", repeat(kEmoji, 26)), R::NameTooLong);
    const auto duplicate = rename(db, "10", " OTHER ");
    CHECK_FOLDER_REFUSED(duplicate, R::DuplicateName);
    CHECK_STR_EQ(duplicate.message, "Folder OTHER already exists.");
    CHECK_EQ(db.snapshot_all(), before);

    CHECK_FOLDER_OK(rename(db, "10", repeat("A", 50)));
    CHECK_FOLDER_OK(rename(db, "12", repeat(kEmoji, 25)));
}

// A rename to the stored name is a success that writes nothing: a BEFORE UPDATE trigger that aborts every update would fail the call if any UPDATE
// ran. A case-only change IS written. A sibling of ANOTHER bid with the name does not count (bid 2 has 'Alpha' too).
TEST(ConditionFolderRename, TheStoredNameAgainIsASilentSuccessAndACaseChangeIsWritten)
{
    TempDb db("frename_unchanged");
    REQUIRE(db.schema_ok());
    seed_world(db);

    const std::string trigger = db.block("BidConditionFolders", "UPDATE");
    const std::string before = db.snapshot_all();
    const auto same = rename(db, "10", "Alpha");
    CHECK_FOLDER_OK(same);
    CHECK_STR_EQ(same.uid, "10");
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_FOLDER_OK(rename(db, "10", "  Alpha  ")); // the stripped text is the stored name: still no UPDATE
    CHECK_FOLDER_REFUSED(rename(db, "10", "ALPHA"), R::WriteFailed); // a case-only change is a write: the trigger aborts it
    CHECK_EQ(db.snapshot_all(), before);
    db.unblock(trigger);

    CHECK_FOLDER_OK(rename(db, "10", "ALPHA"));
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 10"), "ALPHA");
}

// The siblings of a folder are the other folders of the SAME bid with the same effective parent (the sidebar looks at the tree children of one
// parent of one bid): a root folder collides with a root folder only, a child with the children of the same parent only, and a folder of another
// bid never collides.
TEST(ConditionFolderRename, OnlyFoldersOfTheSameBidAndParentAreSiblings)
{
    TempDb db("frename_siblings");
    REQUIRE(db.schema_ok());
    seed_world(db);

    CHECK_FOLDER_REFUSED(rename(db, "12", "alpha"), R::DuplicateName);  // folder 10 'Alpha' is a root sibling
    CHECK_FOLDER_OK(rename(db, "11", "alpha"));                          // folder 11 is below 10: its siblings are only itself
    CHECK_FOLDER_OK(rename(db, "12", "Child"));                          // 'Child' is below 10, folder 12 is at the root
    add_folder(db, 13, 1, "Beside child", "10");
    CHECK_FOLDER_REFUSED(rename(db, "13", "ALPHA"), R::DuplicateName);   // now 11 is 'alpha' below 10
    CHECK_FOLDER_OK(rename(db, "20", "child", "2"));                     // bid 2's root has only 'Alpha' (20 itself); bid 1's root 'Child' (12) is another bid
    CHECK_EQ(db.text("SELECT Name FROM BidConditionFolders WHERE UID = 20"), "child");
}

// Failure injection: a BEFORE UPDATE trigger that aborts the rename: refused, nothing changes, the lock is released.
TEST(ConditionFolderRename, AFailedUpdateRollsBackAndReleasesTheLock)
{
    TempDb db("frename_rollback");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    const std::string trigger = db.block("BidConditionFolders", "UPDATE");
    CHECK_FOLDER_REFUSED(rename(db, "10", "Renamed"), R::WriteFailed);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK(db.exec("UPDATE Bids SET JobName = 'still writable' WHERE UID = 2"));
    db.must("UPDATE Bids SET JobName = 'Bid two' WHERE UID = 2");
    db.unblock(trigger);
    CHECK_FOLDER_OK(rename(db, "10", "Renamed"));
}

// =================================================================================================================================
// the bid lock in every writer (plan 4.1; Windows active_bid_write_guard.py:7-36, project_write_service.py:786, :825, :871)
// =================================================================================================================================

// For every state of the bid's job status: the four writers either write (not locked) or refuse with BidLocked and leave the whole database
// untouched (snapshot_all). Locked = a job status with Locked -1, a status no JobStatuses row names (also "0"), or an unreadable JobStatuses table
// (fail closed); not locked = no status, an empty status, a status with Locked 0.
TEST(ConditionFolderLock, EveryWriterRefusesALockedBidAndWritesNothing)
{
    for (const LockCase &lock : lock_cases())
    {
        struct Writer
        {
            const char *label;
            std::function<ost_store::ConditionFolderResult(const TempDb &)> call;
        };
        const std::vector<Writer> writers = {
            {"create", [](const TempDb &db) { return create(db, "New folder"); }},
            {"create under a parent", [](const TempDb &db) { return create(db, "New child", "12"); }},
            {"rename", [](const TempDb &db) { return rename(db, "12", "Renamed"); }},
            {"delete", [](const TempDb &db) { return ost_store::delete_condition_folders(db.path(), "1", {"12"}); }},
            {"move into a folder", [](const TempDb &db) { return move(db, "30", "12"); }},
            {"move to the root", [](const TempDb &db) { return move(db, "31", ""); }},
        };
        for (const Writer &writer : writers)
        {
            SCOPED_CONTEXT(std::string(lock.label) + " / " + writer.label);
            TempDb db("flock_matrix");
            REQUIRE(db.schema_ok());
            seed_world(db);
            lock.setup(db);
            const std::string before = db.snapshot_all();

            const auto result = writer.call(db);
            if (lock.locked)
            {
                CHECK_FOLDER_REFUSED(result, R::BidLocked);
                CHECK_EQ(db.snapshot_all(), before);
            }
            else
            {
                CHECK_FOLDER_OK(result);
                CHECK_NE(db.snapshot_all(), before);
            }
        }
    }
}

// The lock is the bid of the call: bid 2 is not locked when bid 1 is (and the other way round), and a bid that does not exist is BidNotFound.
TEST(ConditionFolderLock, TheLockBelongsToTheBidOfTheCall)
{
    TempDb db("flock_per_bid");
    REQUIRE(db.schema_ok());
    seed_world(db);
    lock_bid(db, 1);
    add_folder(db, 21, 2, "Second");

    CHECK_FOLDER_REFUSED(create(db, "Walls", "", "1"), R::BidLocked);
    const auto open_bid = create(db, "Walls", "", "2");
    CHECK_FOLDER_OK(open_bid);
    CHECK_FOLDER_OK(rename(db, "21", "Renamed", "2"));
    CHECK_FOLDER_REFUSED(rename(db, "10", "Renamed", "1"), R::BidLocked);
    CHECK_FOLDER_REFUSED(create(db, "Walls", "", "99"), R::BidNotFound);
}

// A FAILED READ of the lock is a lock (fail closed, plan 4.1; the Windows fixtures' `Bids (UID INTEGER)` are why the minimal tables of the ports
// carry JobStatusUID): a Bids table without that column cannot say whether the bid is open, so every writer refuses with BidLocked and writes nothing.
TEST(ConditionFolderLock, AnUnreadableLockIsALockedBid)
{
    TempDb db("flock_unreadable", false);
    db.must("CREATE TABLE Bids (UID INTEGER)");
    db.must("INSERT INTO Bids (UID) VALUES (1)");
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER, BidUID INTEGER, ParentUID INTEGER, Name TEXT, ExpandState INTEGER)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, BidConditionFolderUID INTEGER)");
    db.must("INSERT INTO BidConditionFolders VALUES (10, 1, NULL, 'Alpha', -1)");
    db.must("INSERT INTO BidConditions VALUES (30, 1, NULL)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(create(db, "Walls"), R::BidLocked);
    CHECK_FOLDER_REFUSED(create(db, "Walls", "10"), R::BidLocked);
    CHECK_FOLDER_REFUSED(rename(db, "10", "Renamed"), R::BidLocked);
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "1", {"10"}), R::BidLocked);
    CHECK_FOLDER_REFUSED(move(db, "30", "10"), R::BidLocked);
    CHECK_EQ(db.snapshot_all(), before);
}

// Another failed read: a file with NO Bids table cannot name an owner at all. Every writer refuses (the exact reason is not pinned: BidNotFound or
// SchemaUnsupported both say "this file cannot take the write"), nothing is written and nothing throws.
TEST(ConditionFolderLock, AFileWithoutTheBidsTableRefusesEveryWriter)
{
    TempDb db("fcreate_no_bids_table", false);
    db.must("CREATE TABLE BidConditionFolders (UID INTEGER, BidUID INTEGER, ParentUID INTEGER, Name TEXT, ExpandState INTEGER)");
    db.must("CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER, BidConditionFolderUID INTEGER)");
    db.must("INSERT INTO BidConditionFolders VALUES (10, 1, NULL, 'Alpha', -1)");
    db.must("INSERT INTO BidConditions VALUES (30, 1, NULL)");
    const std::string before = db.snapshot_all();

    const std::vector<std::pair<std::string, ost_store::ConditionFolderResult>> results = {
        {"create", create(db, "Walls")},
        {"create under a parent", create(db, "Walls", "10")},
        {"rename", rename(db, "10", "Renamed")},
        {"delete", ost_store::delete_condition_folders(db.path(), "1", {"10"})},
        {"move", move(db, "30", "10")},
    };
    for (const auto &entry : results)
    {
        SCOPED_CONTEXT(entry.first);
        CHECK_FALSE(entry.second.ok);
        CHECK(entry.second.refusal != R::None);
    }
    CHECK_EQ(db.snapshot_all(), before);
}

// =================================================================================================================================
// Move to Folder (plan 5.2, N-M1-14): its own store function, never update_condition
// =================================================================================================================================

// Moving a condition into a folder changes exactly BidConditions.BidConditionFolderUID of that row: every other column of the row (RefNo and the
// others), every other row and every other table are identical.
TEST(ConditionFolderMove, ChangesOnlyTheFolderOfTheOneConditionRow)
{
    TempDb db("fmove_only_folder");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string row_before = db.row("BidConditions", "30", {"BidConditionFolderUID"});
    const std::string others_before = db.list("SELECT UID || '|' || quote(BidConditionFolderUID) || '|' || quote(RefNo) FROM BidConditions WHERE UID <> 30 ORDER BY UID");
    const std::string folders_before = db.dump("BidConditionFolders");
    const std::string bids_before = db.dump("Bids");

    const auto result = move(db, "30", "10");
    CHECK_FOLDER_OK(result);
    CHECK_STR_EQ(result.uid, "10");
    CHECK_EQ(db.text("SELECT typeof(BidConditionFolderUID) || ':' || BidConditionFolderUID FROM BidConditions WHERE UID = 30"), "integer:10");
    CHECK_EQ(db.row("BidConditions", "30", {"BidConditionFolderUID"}), row_before);
    CHECK_EQ(db.list("SELECT UID || '|' || quote(BidConditionFolderUID) || '|' || quote(RefNo) FROM BidConditions WHERE UID <> 30 ORDER BY UID"), others_before);
    CHECK_EQ(db.dump("BidConditionFolders"), folders_before);
    CHECK_EQ(db.dump("Bids"), bids_before);
}

// The folder "" is the root: the column becomes NULL; moving to the folder the condition already is in is a success that changes nothing.
TEST(ConditionFolderMove, TheEmptyFolderIsTheRootAndTheSameFolderIsASuccess)
{
    TempDb db("fmove_root");
    REQUIRE(db.schema_ok());
    seed_world(db);

    CHECK_FOLDER_OK(move(db, "31", ""));
    CHECK_EQ(db.text("SELECT typeof(BidConditionFolderUID) FROM BidConditions WHERE UID = 31"), "null");

    const std::string before = db.snapshot_all();
    CHECK_FOLDER_OK(move(db, "31", "")); // already at the root
    CHECK_FOLDER_OK(move(db, "30", "")); // never had a folder
    CHECK_EQ(db.snapshot_all(), before);
    CHECK_FOLDER_OK(move(db, "30", "11"));
    const std::string moved = db.snapshot_all();
    CHECK_FOLDER_OK(move(db, "30", "11")); // already in 11
    CHECK_EQ(db.snapshot_all(), moved);
}

// Plan 5.2 (data safety): a LOCKED bid refuses the move (test_toolbar_projection.py:464-499 pins the move as a lock-blocked folder write) and the
// whole database is unchanged.
TEST(ConditionFolderMove, ALockedBidRefusesTheMove)
{
    TempDb db("fmove_locked");
    REQUIRE(db.schema_ok());
    seed_world(db);
    lock_bid(db, 1);
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(move(db, "30", "10"), R::BidLocked);
    CHECK_FOLDER_REFUSED(move(db, "31", ""), R::BidLocked);
    CHECK_EQ(db.snapshot_all(), before);
}

// Plan 5.2 (data safety): a folder of another bid is refused (update_condition checks the same, ProjectStore+Conditions.cpp:338-341); a folder that
// does not exist is FolderNotFound; a malformed folder uid is InvalidUid ("0" too: the root is "").
TEST(ConditionFolderMove, AFolderOfAnotherBidOrNoFolderIsRefused)
{
    TempDb db("fmove_folder_ownership");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(move(db, "30", "20"), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(move(db, "30", "999"), R::FolderNotFound);
    for (const std::string folder : {"0", "-1", " 10", "abc", "1.5", "99999999999999999999"})
    {
        SCOPED_CONTEXT("folder uid " + ost_test::detail::quote(folder));
        CHECK_FOLDER_REFUSED(move(db, "30", folder), R::InvalidUid);
    }
    CHECK_EQ(db.snapshot_all(), before);
}

// The condition must belong to the bid, exist and have a valid uid; the bid must exist.
TEST(ConditionFolderMove, TheConditionMustBelongToTheOpenBid)
{
    TempDb db("fmove_condition_ownership");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(move(db, "40", "10"), R::ConditionOfAnotherBid); // bid 2's condition through bid 1
    CHECK_FOLDER_REFUSED(move(db, "40", "20"), R::ConditionOfAnotherBid); // even into bid 2's folder
    CHECK_FOLDER_REFUSED(move(db, "30", "20", "2"), R::ConditionOfAnotherBid);
    CHECK_FOLDER_REFUSED(move(db, "999", "10"), R::ConditionNotFound);
    CHECK_FOLDER_REFUSED(move(db, "30", "10", "99"), R::BidNotFound);
    for (const std::string condition : {"", "0", "-30", " 30", "30 ", "abc", "99999999999999999999"})
    {
        SCOPED_CONTEXT("condition uid " + ost_test::detail::quote(condition));
        CHECK_FOLDER_REFUSED(move(db, condition, "10"), R::InvalidUid);
    }
    for (const std::string bid : {"", "0", "x"})
    {
        SCOPED_CONTEXT("bid uid " + ost_test::detail::quote(bid));
        CHECK_FOLDER_REFUSED(move(db, "30", "10", bid), R::InvalidUid);
    }
    CHECK_EQ(db.snapshot_all(), before);
}

// The same order of checks as the other writers: ownership before the lock.
TEST(ConditionFolderMove, TheChecksRunInTheOrderUidOwnershipLock)
{
    TempDb db("fmove_order");
    REQUIRE(db.schema_ok());
    seed_world(db);
    lock_bid(db, 1);

    CHECK_FOLDER_REFUSED(move(db, "30", "20"), R::FolderOfAnotherBid);
    CHECK_FOLDER_REFUSED(move(db, "40", "10"), R::ConditionOfAnotherBid);
    CHECK_FOLDER_REFUSED(move(db, "999", "10"), R::ConditionNotFound);
    CHECK_FOLDER_REFUSED(move(db, "30", "abc"), R::InvalidUid);
    CHECK_FOLDER_REFUSED(move(db, "30", "10"), R::BidLocked);
}

// Failure injection: a BEFORE UPDATE trigger on BidConditions aborts the move: refused, nothing changes, the lock is released.
TEST(ConditionFolderMove, AFailedUpdateRollsBackAndReleasesTheLock)
{
    TempDb db("fmove_rollback");
    REQUIRE(db.schema_ok());
    seed_world(db);
    const std::string before = db.snapshot_all();

    const std::string trigger = db.block("BidConditions", "UPDATE");
    CHECK_FOLDER_REFUSED(move(db, "30", "10"), R::WriteFailed);
    CHECK_EQ(db.snapshot_all(), before);
    CHECK(db.exec("UPDATE Bids SET JobName = 'still writable' WHERE UID = 2"));
    db.must("UPDATE Bids SET JobName = 'Bid two' WHERE UID = 2");
    db.unblock(trigger);
    CHECK_FOLDER_OK(move(db, "30", "10"));
}

// A file without BidConditions.BidConditionFolderUID cannot store the move: SchemaUnsupported, nothing written.
TEST(ConditionFolderMove, AFileWithoutTheFolderColumnRefusesTheMove)
{
    TempDb db("fmove_no_column", false);
    windows_folder_db(db, ost_ftest::kWindowsFolderColumns, {}, "CREATE TABLE BidConditions (UID INTEGER, BidUID INTEGER)");
    db.must("INSERT INTO BidConditionFolders VALUES (10, 1, NULL, 'F', -1)");
    db.must("INSERT INTO BidConditions VALUES (30, 1)");
    const std::string before = db.snapshot_all();

    CHECK_FOLDER_REFUSED(move(db, "30", "10"), R::SchemaUnsupported);
    CHECK_EQ(db.snapshot_all(), before);
}

// Moving the LAST condition out of a folder makes the folder deletable (it was InUse before); deleting the last condition (delete_condition) leaves
// the folder in place (research section 4, "move a condition"; delete_condition is existing code: this half passes without the new writers).
TEST(ConditionFolderMove, MovingTheLastConditionOutMakesTheFolderDeletable)
{
    TempDb db("fmove_then_delete");
    REQUIRE(db.schema_ok());
    seed_world(db);

    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "1", {"11"}), R::InUse); // condition 31 is in folder 11
    CHECK_FOLDER_OK(move(db, "31", ""));
    const auto deleted = ost_store::delete_condition_folders(db.path(), "1", {"11"});
    CHECK_FOLDER_OK(deleted);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID = 11"), 0);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 31"), 1);

    // moving into a folder makes it used again
    CHECK_FOLDER_OK(move(db, "31", "12"));
    CHECK_FOLDER_REFUSED(ost_store::delete_condition_folders(db.path(), "1", {"12"}), R::InUse);
}

TEST(ConditionFolderMove, DeletingTheLastConditionLeavesItsFolder)
{
    TempDb db("fmove_delete_condition");
    REQUIRE(db.schema_ok());
    seed_world(db);

    CHECK(ost_store::delete_condition(db.path(), "1", "31"));
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditionFolders WHERE UID = 11"), 1);
    CHECK_EQ(db.count("SELECT COUNT(*) FROM BidConditions WHERE UID = 31"), 0);
    CHECK_FOLDER_OK(ost_store::delete_condition_folders(db.path(), "1", {"11"})); // nothing holds a condition any more
}
