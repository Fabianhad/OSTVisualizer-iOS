// The bid lock RULE (plan 4.1, S0.a): ost_domain::resolve_bid_status_lock, the pure function behind every lock read.
// Windows oracle (Windows 49a2d59):
//   ost_visualizer/domain/entities/cover_sheet.py:15-23   resolve_bid_status_lock: no status (None or "") is unlocked WITHOUT looking at
//                                                         the statuses; the FIRST status whose uid TEXT equals the bid's status text
//                                                         gives its own `locked`; no such status is None (unresolved)
//   ost_visualizer/application/services/project_read_service.py:81-94   is_bid_locked: the callers turn None into locked (fail closed)
//   tests/domain/entities/test_cover_sheet.py (ResolveBidStatusLockTests, :14-87): ported below with the same inputs, the Windows line
//   is cited above each test. BidLockOracleData.hpp (table 3) holds the outcome of the REAL Windows function on 31 rows.
// Python None is the empty text here (both mean "no status"); Python ints are their decimal text (the Windows tests compare by text).
#include "TestHarness.hpp"

#include "BidLockOracleData.hpp"

#include "Domain/BidLock.hpp"

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

using ost_domain::BidStatusLock;
using ost_domain::JobStatusLock;

namespace
{
    std::string text_of(BidStatusLock lock)
    {
        switch (lock)
        {
        case BidStatusLock::Unlocked:
            return "unlocked";
        case BidStatusLock::Locked:
            return "locked";
        case BidStatusLock::Unresolved:
            return "unresolved";
        }
        return "?";
    }

    // test_cover_sheet.py:8-11 _statuses(*rows)
    std::vector<JobStatusLock> statuses(std::initializer_list<std::pair<const char *, bool>> rows)
    {
        std::vector<JobStatusLock> out;
        for (const auto &row : rows)
        {
            JobStatusLock status;
            status.uid = row.first;
            status.locked = row.second;
            out.push_back(status);
        }
        return out;
    }

    std::string resolve(const std::vector<JobStatusLock> &list, const std::string &status_uid)
    {
        return text_of(ost_domain::resolve_bid_status_lock(list, status_uid));
    }
} // namespace

// test_cover_sheet.py:19-24 test_a_bid_without_a_status_is_unlocked_whatever_the_statuses_say (None and "" are both the empty text).
TEST(BidLockRule, ABidWithoutAStatusIsUnlockedWhateverTheStatusesSay)
{
    const std::vector<std::vector<JobStatusLock>> lists = {
        statuses({}),
        statuses({{"1", true}}),
        statuses({{"", true}, {"0", true}}),
    };
    int index = 0;
    for (const auto &list : lists)
    {
        SCOPED_CONTEXT("statuses list " + std::to_string(index++));
        CHECK_STR_EQ(resolve(list, ""), "unlocked");
    }
}

// test_cover_sheet.py:26-33 test_an_unknown_status_uid_is_unresolved_not_unlocked ("0" and 0 are the same text).
TEST(BidLockRule, AnUnknownStatusUidIsUnresolvedNotUnlocked)
{
    const auto list = statuses({{"1", false}, {"2", true}});
    for (const char *status_uid : {"3", "0", "01", "-1"})
    {
        SCOPED_CONTEXT(std::string("status uid ") + status_uid);
        CHECK_STR_EQ(resolve(list, status_uid), "unresolved");
    }
    CHECK_STR_EQ(resolve(statuses({}), "1"), "unresolved");
    CHECK_STR_EQ(resolve(statuses({}), "0"), "unresolved");
}

// test_cover_sheet.py:35-39 test_a_matching_uid_reads_that_statuss_own_locked_flag
TEST(BidLockRule, AMatchingUidReadsThatStatusOwnLockedFlag)
{
    const auto list = statuses({{"1", true}, {"2", false}, {"3", true}});
    CHECK_STR_EQ(resolve(list, "1"), "locked");
    CHECK_STR_EQ(resolve(list, "2"), "unlocked");
    CHECK_STR_EQ(resolve(list, "3"), "locked");
}

// test_cover_sheet.py:41-46 test_the_status_uid_zero_is_an_ordinary_uid_when_it_exists (and project_read_service tests
// test_a_resolvable_zero_status_reads_its_own_locked_flag / test_an_integer_zero_status_uid_reads_like_the_text_zero: 0 is an unknown
// uid, not "no status": only the empty text means no status).
TEST(BidLockRule, TheStatusUidZeroIsAnOrdinaryUidWhenItExists)
{
    for (const bool locked : {true, false})
    {
        SCOPED_CONTEXT(locked ? "zero is locked" : "zero is open");
        const auto list = statuses({{"1", !locked}, {"0", locked}});
        CHECK_STR_EQ(resolve(list, "0"), locked ? "locked" : "unlocked");
    }
}

// test_cover_sheet.py:55-64 test_the_first_matching_status_wins
TEST(BidLockRule, TheFirstMatchingStatusWins)
{
    CHECK_STR_EQ(resolve(statuses({{"2", true}, {"2", false}}), "2"), "locked");
    CHECK_STR_EQ(resolve(statuses({{"2", false}, {"2", true}}), "2"), "unlocked");
    // the match is the first row of the list, wherever it sits: the rows before it do not matter
    CHECK_STR_EQ(resolve(statuses({{"1", true}, {"3", true}, {"2", false}, {"2", true}}), "2"), "unlocked");
}

// test_cover_sheet.py:66-78 test_uids_are_compared_verbatim_without_trimming_or_case_folding
TEST(BidLockRule, UidsAreComparedVerbatimWithoutTrimmingOrCaseFolding)
{
    struct Case
    {
        const char *stored;
        const char *requested;
    };
    const Case cases[] = {{"2", " 2"}, {"2", "2 "}, {" 2", "2"}, {"2 ", "2"}, {"a", "A"}, {"A", "a"}};
    for (const Case &c : cases)
    {
        SCOPED_CONTEXT(std::string("stored '") + c.stored + "' requested '" + c.requested + "'");
        CHECK_STR_EQ(resolve(statuses({{c.stored, true}}), c.requested), "unresolved");
    }
    CHECK_STR_EQ(resolve(statuses({{" 2", true}}), " 2"), "locked");
    // no number parsing either: 1.0, +1, 0x1 and 001 are other uids than 1
    for (const char *requested : {"1.0", "+1", "0x1", "001", "1e0"})
    {
        SCOPED_CONTEXT(std::string("requested ") + requested);
        CHECK_STR_EQ(resolve(statuses({{"1", false}}), requested), "unresolved");
    }
}

// test_cover_sheet.py:80-83 test_a_none_status_uid_is_not_matched_against_the_statuses: a status called "None" is not what a missing
// status matches; the lower-case text does not match it either (only the exact text "None" does: Python str(uid) == str("None")).
TEST(BidLockRule, AMissingStatusIsNotMatchedAgainstAStatusCalledNone)
{
    const auto list = statuses({{"None", true}});
    CHECK_STR_EQ(resolve(list, ""), "unlocked");
    CHECK_STR_EQ(resolve(list, "none"), "unresolved");
    CHECK_STR_EQ(resolve(list, "None"), "locked"); // oracle row "extra: the text None matches a status called None"
}

// A status row with an empty uid never matches a real status uid (Windows compares "" with "1"), and the empty request is answered
// before the list is read (cover_sheet.py:17-18).
TEST(BidLockRule, ARowWithAnEmptyUidNeverMatchesARealUid)
{
    CHECK_STR_EQ(resolve(statuses({{"", true}}), "1"), "unresolved");
    CHECK_STR_EQ(resolve(statuses({{"", true}}), ""), "unlocked");
}

// project_read_service.py:81-94 is_bid_locked: an unresolved status is locked (fail closed), a resolved one is its own flag.
TEST(BidLockRule, AnUnresolvedStatusCountsAsLocked)
{
    CHECK_FALSE(ost_domain::bid_status_is_locked(BidStatusLock::Unlocked));
    CHECK(ost_domain::bid_status_is_locked(BidStatusLock::Locked));
    CHECK(ost_domain::bid_status_is_locked(BidStatusLock::Unresolved));
    // end to end on the rule: unknown status -> locked, no status -> unlocked
    CHECK(ost_domain::bid_status_is_locked(ost_domain::resolve_bid_status_lock(statuses({{"1", false}}), "9")));
    CHECK_FALSE(ost_domain::bid_status_is_locked(ost_domain::resolve_bid_status_lock(statuses({{"1", true}}), "")));
}

// The statuses of a real database: many rows, the match is found wherever it is, an absent uid stays unresolved.
TEST(BidLockRule, TheMatchIsFoundInALongList)
{
    std::vector<JobStatusLock> list;
    for (int i = 1; i <= 500; ++i)
    {
        JobStatusLock status;
        status.uid = std::to_string(i);
        status.locked = (i % 7 == 0);
        list.push_back(status);
    }
    CHECK_STR_EQ(resolve(list, "1"), "unlocked");
    CHECK_STR_EQ(resolve(list, "7"), "locked");
    CHECK_STR_EQ(resolve(list, "500"), "unlocked");
    CHECK_STR_EQ(resolve(list, "497"), "locked");
    CHECK_STR_EQ(resolve(list, "501"), "unresolved");
}

// BidLockOracleData.hpp table 3: the real Windows resolve_bid_status_lock on the cases above and a few more (31 rows).
TEST(BidLockRule, MatchesTheWindowsFunctionOnTheOracleRows)
{
    const auto &rows = ost_ptest::bidlock::pure_rows();
    REQUIRE(rows.size() == 31);
    for (const auto &row : rows)
    {
        SCOPED_CONTEXT(row.name);
        std::vector<JobStatusLock> list;
        for (const auto &status : row.statuses)
        {
            JobStatusLock entry;
            entry.uid = status.uid;
            entry.locked = status.locked;
            list.push_back(entry);
        }
        const std::string expected = row.result == 'T' ? "locked" : (row.result == 'F' ? "unlocked" : "unresolved");
        CHECK_STR_EQ(resolve(list, row.has_status ? row.status_uid : ""), expected);
    }
}
