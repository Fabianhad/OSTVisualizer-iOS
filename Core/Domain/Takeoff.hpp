#pragma once
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ost_domain
{

    // The value of one BidTakeoffs column outside the ten typed ones, with the SQLite storage class it had.
    struct TakeoffExtraValue
    {
        enum class Kind
        {
            Null,
            Integer,
            Real,
            Text
        };

        Kind kind = Kind::Null;
        long long integer = 0;
        double real = 0.0;
        std::string text;

        static TakeoffExtraValue make_null() { return {}; }
        static TakeoffExtraValue make_integer(long long value)
        {
            TakeoffExtraValue v;
            v.kind = Kind::Integer;
            v.integer = value;
            return v;
        }
        static TakeoffExtraValue make_real(double value)
        {
            TakeoffExtraValue v;
            v.kind = Kind::Real;
            v.real = value;
            return v;
        }
        static TakeoffExtraValue make_text(std::string value)
        {
            TakeoffExtraValue v;
            v.kind = Kind::Text;
            v.text = std::move(value);
            return v;
        }
    };

    // Windows takeoff "extras" (bid_data_reader.py _parse_bid_takeoffs_for_bid: every BidTakeoffs column except UID, BidUID,
    // BidConditionUID, BidPageUID, BidAreaUID, Position, Rotation, Curve, ParentUID, IsNegativeQuantity) plus
    // InsertTakeoffSpec.source_bid_uid. A takeoff that is inserted again (undo of a delete) carries its extras so the row
    // comes back with the same column values. Empty for a takeoff that is new.
    struct TakeoffExtras
    {
        std::map<std::string, TakeoffExtraValue> columns;
        // The bid the extras were taken from; set only when it can differ from the bid inserted into (a copy between bids).
        std::optional<std::string> source_bid_uid;

        bool empty() const { return columns.empty(); }
    };

    struct Takeoff
    {
        std::string uid;
        std::string condition_uid;
        std::string page_uid;
        std::string area_uid = "0";

        std::vector<double> position;

        double rotation = 0.0;
        int curve = -1;

        std::string parent_uid = "0";
        bool is_negative = false;

        // The other BidTakeoffs columns of the stored row (see TakeoffExtras); empty unless it was read from the database
        // to be inserted again (ost_store::read_takeoff_extras).
        TakeoffExtras extras;

        bool is_hole() const
        {
            return !parent_uid.empty() && parent_uid != "0";
        }

        bool has_valid_position() const
        {
            return position.size() >= 4;
        }
    };

}
