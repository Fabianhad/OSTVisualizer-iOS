// Set Scale, the storage classes the store reads (plan 4.5, 5.4; added with the implementation). iOS writes TEXT, but a database that came from Windows (or from an older build) can
// hold a BLOB where the writer expects text, and a loosely typed scale column can hold text: the store must read them like the Windows reader does (page_operations.py:154-217
// parse_position_storage decodes bytes, :67-82 float()), write TEXT back, and never turn a readable page into a refusal for its storage class alone.
#include "PageScaleTestSupport.hpp"

#include <string>

using ost_ptest::TempDb;
using ost_store::PageScaleOutcome;
using ost_store::PageScaleRefusal;
using std::string;

namespace
{
    // Bids (1) with one page (20, bid 1) at (0.125, 12) and the three tables this file needs, with the Position cells given as SQL literals.
    void make_db(const TempDb &db, const string &scale_columns_sql = "ScaleFactor1 REAL, ScaleFactor2 REAL")
    {
        db.must("CREATE TABLE Bids (UID INTEGER PRIMARY KEY, JobStatusUID)");
        db.must("INSERT INTO Bids (UID) VALUES (1)");
        db.must("CREATE TABLE BidPages (UID INTEGER PRIMARY KEY, BidUID INTEGER, " + scale_columns_sql + ")");
        db.must("CREATE TABLE BidTakeoffs (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, Position BLOB)");
        db.must("CREATE TABLE BidTexts (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, Position BLOB)");
        db.must("CREATE TABLE BidLegends (UID INTEGER, BidUID INTEGER, BidPageUID INTEGER, Position BLOB)");
    }

    // The upper-case hex of the bytes of `bytes` (what SQLite's hex() prints).
    string hex(const string &bytes)
    {
        static const char digits[] = "0123456789ABCDEF";
        string out;
        for (const unsigned char c : bytes)
        {
            out += digits[c >> 4];
            out += digits[c & 15];
        }
        return out;
    }
}

// A Position stored as a BLOB (the bytes of the text, what the Windows MDB holds for BidTakeoffs / BidTexts) is read as its text, rescaled and written back as TEXT (plan 4.5): the
// takeoff in the three-decimal form, the text annotation in the exact form with the rotation slot kept.
TEST(SetScale_StoreBytes, ABlobPositionIsRescaledAndWrittenAsText)
{
    TempDb db("scale_bytes_blob_position", false);
    make_db(db);
    db.must("INSERT INTO BidPages VALUES (20, 1, 0.125, 12.0)");
    db.must("INSERT INTO BidTakeoffs VALUES (1, 1, 20, X'31303B32303B33303B34300A')");     // "10;20;30;40\n"
    db.must("INSERT INTO BidTexts VALUES (1, 1, 20, X'31303B32303B33303B34303B302E350A')"); // "10;20;30;40;0.5\n"
    CHECK_EQ(ost_ptest::type_of(db, "BidTakeoffs", "Position", 1), "blob");
    PageScaleOutcome outcome;
    REQUIRE(ost_store::save_page_scale(db.path(), "1", "20", 0.25, 12.0, &outcome));
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "5;10;15;20\n");
    CHECK_EQ(ost_ptest::position_of(db, "BidTexts", 1), "5.0;10.0;15.0;20.0;0.5\n");
    CHECK_EQ(ost_ptest::type_of(db, "BidTakeoffs", "Position", 1), "text");
    CHECK_EQ(ost_ptest::type_of(db, "BidTexts", "Position", 1), "text");
}

// A legend that is a BLOB of LATIN-1 bytes (the Windows writer encodes the XML as latin-1: a name with a lone 0xE9 is not valid UTF-8) is rescaled in place: only the digits of dX / dY
// change, the 0xE9 byte is copied, and the cell is TEXT afterwards. (Rejecting it would make Set Scale impossible on such a page.)
TEST(SetScale_StoreBytes, ALatin1LegendIsRescaledAndTheByteIsKept)
{
    TempDb db("scale_bytes_latin1_legend", false);
    make_db(db);
    db.must("INSERT INTO BidPages VALUES (20, 1, 0.125, 12.0)");
    // a LONE 0xE9 (latin-1 "e acute"), which is not valid UTF-8: the review replaced the UTF-8 spelling (C3 A9) that this test used before and that the comment above contradicted
    const string stored = string("<Legends dX=\"10\" Name=\"caf\xE9\"><Legend dX=\"20\"/></Legends>");
    const string expected = string("<Legends dX=\"5\" Name=\"caf\xE9\"><Legend dX=\"10\"/></Legends>");
    db.must("INSERT INTO BidLegends VALUES (1, 1, 20, X'" + hex(stored) + "')");
    CHECK_EQ(ost_ptest::type_of(db, "BidLegends", "Position", 1), "blob");
    PageScaleOutcome outcome;
    REQUIRE(ost_store::save_page_scale(db.path(), "1", "20", 0.25, 12.0, &outcome));
    CHECK_EQ(db.text("SELECT hex(Position) FROM BidLegends WHERE UID = 1"), hex(expected));
    CHECK_EQ(ost_ptest::type_of(db, "BidLegends", "Position", 1), "text");
}

// A scale column that holds TEXT (a loosely typed table) is read like Python float(): "0.125" and " 12 " are numbers, "abc" is not (BadStoredScale). The new pair is written as REAL.
TEST(SetScale_StoreBytes, AStoredScaleThatIsTextIsReadLikeFloat)
{
    TempDb db("scale_bytes_text_scale", false);
    make_db(db, "ScaleFactor1, ScaleFactor2"); // no declared type: the cells keep what is inserted
    db.must("INSERT INTO BidPages VALUES (20, 1, '0.125', ' 12 ')");
    db.must("INSERT INTO BidTakeoffs VALUES (1, 1, 20, '10;20;30;40')");
    PageScaleOutcome outcome;
    REQUIRE(ost_store::save_page_scale(db.path(), "1", "20", 0.25, 12.0, &outcome));
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "5;10;15;20\n");
    CHECK_EQ(ost_ptest::type_of(db, "BidPages", "ScaleFactor1", 20), "real");
    CHECK_EQ(ost_ptest::scale_values(db, 20), (std::pair<double, double>(0.25, 12.0)));
    db.must("UPDATE BidPages SET ScaleFactor2 = 'abc' WHERE UID = 20");
    const string before = db.snapshot_all();
    PageScaleOutcome bad;
    CHECK_FALSE(ost_store::save_page_scale(db.path(), "1", "20", 0.5, 12.0, &bad));
    CHECK(bad.refusal == PageScaleRefusal::BadStoredScale);
    CHECK_EQ(db.snapshot_all(), before);
}

// A Position cell of a row on the page that is an INTEGER or REAL number (a damaged cell, not text) is read as the text of the number; "5" is the position [5] and is rescaled.
TEST(SetScale_StoreBytes, ANumericPositionCellIsReadAsItsText)
{
    TempDb db("scale_bytes_numeric_cell", false);
    make_db(db);
    db.must("INSERT INTO BidPages VALUES (20, 1, 0.125, 12.0)");
    db.must("INSERT INTO BidTakeoffs VALUES (1, 1, 20, 5)");
    db.must("INSERT INTO BidTakeoffs VALUES (2, 1, 20, 2.5)");
    PageScaleOutcome outcome;
    REQUIRE(ost_store::save_page_scale(db.path(), "1", "20", 0.25, 12.0, &outcome));
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 1), "2.5\n");
    CHECK_EQ(ost_ptest::position_of(db, "BidTakeoffs", 2), "1.25\n");
}
