// Layer: Persistence
// Depends on: Domain (ConditionCreate), OstDatabase
// Forbidden: UIKit, Foundation, Renderer
// Public declarations of the store functions of one M1 feature: New Condition (insert_condition) and the identity allocation of several conditions (allocate_condition_identities).
// Created as an empty stub by S0 (plan 4.4 and 6.1; included once at the end of ProjectStore.hpp), filled by the stream S1 (signature style of
// duplicate_condition: db_path, bid_uid, ..., std::string *error = nullptr). Implemented in Components/ProjectStore+ConditionCreate.cpp.
#pragma once
#include "../../Domain/ConditionCreate.hpp"
#include "../OstDatabase.hpp"
#include <string>
#include <vector>

namespace ost_store
{

    // Windows insert_condition(db_path, bid_uid, CreateConditionSpec) (condition_operations.py:254-350): ONE transaction that inserts one condition
    // into the bid and returns its new UID as text; "" on ANY failure, with nothing written (a failed statement is rolled back).
    // Inside the transaction, in this order (plan 4.4 item 2): the bid uid is parsed like Python int() (surrounding blanks and a sign are accepted,
    // "0", a negative number and a non-number are not) and the bid must exist; a layer / folder of the spec (a uid of ASCII digits only, greater than
    // zero) must belong to THAT bid (row_belongs_to_bid: another bid's row, a shared template layer with no bid and a template layer of another bid
    // are refused; the bid's own locked copy is accepted); a CdnTypeUID must be a CdnTypes row; the bid lock (4.1, integrity::bid_is_locked: a locked bid
    // or an unreadable status is refused); ost_domain::validate_create_spec; the UID (integrity::next_uid_within_access_limit on BidConditions:
    // MAX(UID) + 1 over ALL bids raised above every referencing column, refused when it would pass 2147483647) and the number (RefNo = MAX(RefNo) of the
    // bid + 1, NULL ignored, 1 for a bid without numbers or without the column, refused BEFORE any write by ost_domain::refuse_new_condition_numbers);
    // a v4 GUID "{8-4-4-4-12}"; the Windows column set in the Windows order (UID, BidUID, GUID, RefNo, IsTemplate 0, Curve 0, SnapToGrid 0,
    // ManualLength 0, MatAmount 0, LabAmount 0, SubAmount 0, FontName "Arial", FontSize -16, FontBold 0, FontItalic 0, ShowTakeoff -1, Backout -1 / 0,
    // then the spec fields; flags 1 / 0; Notes as TEXT, NULL when empty; a uid field "" = NULL; Connect / ConnectTolerance / SnapToLinear only when the
    // spec sets them); an optional column the table lacks is skipped and logged (ost_log::warning, label "insert_condition"), a required one
    // (UID, BidUID, Name, Type) makes the insert fail. The name is stored as given (the form strips it).
    // `error`, when given, is cleared first and set only for a text the user must see: ost_domain::refuse_new_condition_numbers(highest, 1) for the
    // number limit, integrity::no_identity_left_text("condition") for the UID limit, ost_domain::kConditionNameTooLongMessage for a name over 255
    // UTF-16 units. Every other refusal, a failed read and a failed statement leave it empty (the caller shows condition_create_failed_issue()).
    std::string insert_condition(const std::string &db_path,
                                 const std::string &bid_uid,
                                 const ost_domain::CreateConditionSpec &spec,
                                 std::string *error = nullptr);

    // One allocated condition: its UID, its GUID and its number (Windows _allocate_condition_identities returns these triples).
    struct ConditionIdentity
    {
        long long uid = 0;
        std::string guid;
        long long ref_no = 0;
    };

    // Windows _allocate_condition_identities(cursor, bid_uid, count) (condition_operations.py:42-66), the allocator that M2's cross-bid paste
    // reuses: `count` (>= 1) consecutive UIDs from integrity::next_uid_within_access_limit(db, "BidConditions", count), `count` fresh GUIDs and `count`
    // consecutive numbers from MAX(RefNo of the bid) + 1 (NULL ignored, 1 for a bid without numbers or without the column), all refused as a
    // whole when the last number would pass 2147483647 (the text of ost_domain::refuse_new_condition_numbers(highest, count): "Cannot add 3 conditions:
    // the new condition numbers would reach 2147483648, past 2147483647, the highest condition number.") or the last UID would pass 2147483647 (the
    // no_identity_left_text("condition") text). It only READS, on the caller's connection and inside the caller's transaction (it sees the caller's
    // uncommitted rows), and it never allocates `count` entries before it has checked both limits (a huge `count` is refused, not tried).
    // true: `out` holds `count` identities in order and `error` (when given) is empty. false: `out` is empty and `error` (when given) holds the
    // text above for the two limit refusals and is empty for every other failure (`count` < 1, an unreadable table, a missing UID / BidUID column).
    bool allocate_condition_identities(ost_db::OstDatabase &db,
                                       long long bid_uid,
                                       long long count,
                                       std::vector<ConditionIdentity> &out,
                                       std::string *error = nullptr);

}
