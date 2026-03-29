// ProjectStore component — takeoff CRUD and layer visibility.
#include "../ProjectStore_Internal.hpp"

namespace ost_store
{

    bool set_layer_visibility(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &layer_uid,
                              bool visible)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidLayers SET Show = ? WHERE UID = ?");
        if (!stmt.valid())
            return false;

        stmt.bind_int(1, visible ? -1 : 0);
        stmt.bind_text(2, layer_uid);
        stmt.step();
        return true;
    }

    bool save_takeoff_position(const std::string &db_path,
                               const std::string &bid_uid,
                               const std::string &takeoff_uid,
                               const std::vector<double> &position)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidTakeoffs SET Position = ? WHERE UID = ?");
        if (!stmt.valid())
            return false;

        stmt.bind_text(1, serialize_position(position));
        stmt.bind_text(2, takeoff_uid);
        stmt.step();
        return true;
    }

    std::string insert_takeoff(const std::string &db_path,
                               const std::string &bid_uid,
                               const ost_domain::Takeoff &takeoff)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return {};

        std::string uid = takeoff.uid;
        if (uid.empty())
        {
            ost_db::Statement max_stmt = db.prepare(
                "SELECT MAX(UID) FROM BidTakeoffs");
            if (max_stmt.valid() && max_stmt.step())
            {
                int max_uid = max_stmt.column_int(0);
                uid = std::to_string(max_uid + 1);
            }
            else
            {
                uid = "1";
            }
        }

        ost_db::Statement stmt = db.prepare(
            "INSERT INTO BidTakeoffs "
            "(UID, BidUID, BidConditionUID, BidPageUID, BidAreaUID, "
            "Position, Rotation, Curve, ParentUID, IsNegativeQuantity) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        if (!stmt.valid())
            return {};

        std::string area = takeoff.area_uid;
        if (area == "0")
            area.clear();
        std::string parent = takeoff.parent_uid;
        if (parent == "0")
            parent.clear();

        stmt.bind_text(1, uid);
        stmt.bind_text(2, bid_uid);
        stmt.bind_text(3, takeoff.condition_uid);
        stmt.bind_text(4, takeoff.page_uid);
        stmt.bind_text(5, area);
        stmt.bind_text(6, serialize_position(takeoff.position));
        stmt.bind_double(7, takeoff.rotation);
        stmt.bind_int(8, takeoff.curve);
        stmt.bind_text(9, parent);
        stmt.bind_int(10, takeoff.is_negative ? 1 : 0);
        stmt.step();
        return uid;
    }

    bool delete_takeoff(const std::string &db_path,
                        const std::string &takeoff_uid)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;

        std::vector<std::string> takeoff_uids = {takeoff_uid};
        {
            ost_db::Statement parent_stmt = db.prepare(
                "SELECT ParentUID FROM BidTakeoffs WHERE UID = ?");
            if (!parent_stmt.valid())
            {
                db.exec("ROLLBACK");
                return false;
            }
            parent_stmt.bind_text(1, takeoff_uid);
            if (parent_stmt.step())
            {
                std::string parent_uid = parent_stmt.column_text(0);
                if (parent_uid.empty() || parent_uid == "0")
                {
                    ost_db::Statement child_stmt = db.prepare(
                        "SELECT UID FROM BidTakeoffs WHERE ParentUID = ?");
                    if (!child_stmt.valid())
                    {
                        db.exec("ROLLBACK");
                        return false;
                    }
                    child_stmt.bind_text(1, takeoff_uid);
                    while (child_stmt.step())
                        takeoff_uids.push_back(child_stmt.column_text(0));
                }
            }
        }

        std::string placeholders;
        for (size_t i = 0; i < takeoff_uids.size(); ++i)
        {
            if (i > 0)
                placeholders += ", ";
            placeholders += "?";
        }

        const char *child_tables[] = {
            "BidDimensions", "BidALines", "BidArrows"};
        for (const char *table : child_tables)
        {
            std::string sql =
                "DELETE FROM [" + std::string(table) +
                "] WHERE BidTakeoffFromUID IN (" + placeholders + ")";
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid())
            {
                db.exec("ROLLBACK");
                return false;
            }
            for (size_t i = 0; i < takeoff_uids.size(); ++i)
                stmt.bind_text(static_cast<int>(i + 1), takeoff_uids[i]);
            stmt.step();
        }

        {
            ost_db::Statement stmt = db.prepare(
                "DELETE FROM BidPercents WHERE BidTakeoffUID IN (" + placeholders + ")");
            if (!stmt.valid())
            {
                db.exec("ROLLBACK");
                return false;
            }
            for (size_t i = 0; i < takeoff_uids.size(); ++i)
                stmt.bind_text(static_cast<int>(i + 1), takeoff_uids[i]);
            stmt.step();
        }

        {
            ost_db::Statement stmt = db.prepare(
                "DELETE FROM BidTakeoffs WHERE UID IN (" + placeholders + ")");
            if (!stmt.valid())
            {
                db.exec("ROLLBACK");
                return false;
            }
            for (size_t i = 0; i < takeoff_uids.size(); ++i)
                stmt.bind_text(static_cast<int>(i + 1), takeoff_uids[i]);
            stmt.step();
        }

        return db.commit();
    }

    bool set_takeoff_negative(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &takeoff_uid,
                              bool is_negative)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidTakeoffs SET IsNegativeQuantity = ? WHERE UID = ?");
        if (!stmt.valid())
            return false;

        stmt.bind_int(1, is_negative ? 1 : 0);
        stmt.bind_text(2, takeoff_uid);
        stmt.step();
        return true;
    }

}
