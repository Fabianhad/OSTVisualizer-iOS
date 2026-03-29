// ProjectStore component — condition CRUD and render config.
#include "../ProjectStore_Internal.hpp"

namespace ost_store
{

    bool update_condition(const std::string &db_path,
                          const ost_domain::Condition &cdn)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        ost_db::Statement stmt = db.prepare(
            "UPDATE BidConditions SET "
            "Name = ?, Type = ?, Shape = ?, Pattern = ?, "
            "ColorLine = ?, ColorFill = ?, "
            "Width = ?, Height = ?, Thickness = ?, Depth = ?, "
            "Rise = ?, Run = ?, Spacing = ?, "
            "DisplaySize = ?, DisplayName = ?, DisplayDimension = ?, Backout = ?, "
            "DropRun = ?, DropValue = ?, "
            "UOM1 = ?, UOM2 = ?, UOM3 = ?, "
            "Quantity1 = ?, Quantity2 = ?, Quantity3 = ?, "
            "RoundUp = ?, RoundQuantity = ?, "
            "Connect = ?, ConnectTolerance = ?, Trim = ?, "
            "IsCurvedSegment = ?, "
            "Grid = ?, GridSize1 = ?, GridSize2 = ?, Gap = ?, "
            "DisplayGridWhileDrawing = ?, SnapToLinear = ?, "
            "RefNo = ?, Notes = ?, "
            "BidLayerUID = ?, CdnTypeUID = ?, BidConditionFolderUID = ? "
            "WHERE UID = ?");
        if (!stmt.valid())
            return false;

        int i = 1;
        stmt.bind_text(i++, cdn.name);
        stmt.bind_int(i++, static_cast<int>(cdn.condition_type));
        stmt.bind_int(i++, static_cast<int>(cdn.shape));
        stmt.bind_int(i++, static_cast<int>(cdn.pattern));
        stmt.bind_int(i++, cdn.color_line);
        stmt.bind_int(i++, cdn.color_fill);
        stmt.bind_double(i++, cdn.width);
        stmt.bind_double(i++, cdn.height);
        stmt.bind_double(i++, cdn.thickness);
        stmt.bind_double(i++, cdn.depth);
        stmt.bind_double(i++, cdn.rise);
        stmt.bind_double(i++, cdn.run);
        stmt.bind_double(i++, cdn.spacing);
        stmt.bind_double(i++, cdn.display_size);
        stmt.bind_int(i++, cdn.display_name ? 1 : 0);
        stmt.bind_int(i++, cdn.display_dimension ? 1 : 0);
        stmt.bind_int(i++, cdn.backout ? -1 : 0);
        stmt.bind_int(i++, cdn.drop_run ? 1 : 0);
        stmt.bind_double(i++, cdn.drop_value);
        stmt.bind_int(i++, cdn.uom1);
        stmt.bind_int(i++, cdn.uom2);
        stmt.bind_int(i++, cdn.uom3);
        stmt.bind_int(i++, cdn.calc_type1);
        stmt.bind_int(i++, cdn.calc_type2);
        stmt.bind_int(i++, cdn.calc_type3);
        stmt.bind_double(i++, cdn.round_up);
        stmt.bind_int(i++, cdn.round_quantity ? 1 : 0);
        stmt.bind_int(i++, cdn.connect ? 1 : 0);
        stmt.bind_double(i++, cdn.connect_tolerance);
        stmt.bind_int(i++, cdn.trim ? 1 : 0);
        stmt.bind_int(i++, cdn.is_curved_segment ? 1 : 0);
        stmt.bind_int(i++, cdn.grid ? 1 : 0);
        stmt.bind_double(i++, cdn.grid_size1);
        stmt.bind_double(i++, cdn.grid_size2);
        stmt.bind_double(i++, cdn.gap);
        stmt.bind_int(i++, cdn.display_grid_while_drawing ? 1 : 0);
        stmt.bind_int(i++, cdn.snap_to_linear);
        stmt.bind_int(i++, cdn.ref_no);
        stmt.bind_text(i++, cdn.notes);
        stmt.bind_text(i++, cdn.layer_uid);
        stmt.bind_text(i++, cdn.cdn_type_uid);
        stmt.bind_text(i++, cdn.condition_folder_uid);
        stmt.bind_text(i++, cdn.uid); // WHERE UID = ?

        stmt.step();
        return true;
    }

    bool shift_condition_ref_nos(const std::string &db_path,
                                 const std::string &bid_uid,
                                 int from_ref_no,
                                 const std::string &exclude_uid)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidConditions "
            "SET RefNo = RefNo + 1 "
            "WHERE BidUID = ? AND RefNo >= ? AND UID <> ?");
        if (!stmt.valid())
            return false;

        stmt.bind_text(1, bid_uid);
        stmt.bind_int(2, from_ref_no);
        stmt.bind_text(3, exclude_uid);
        stmt.step();
        return true;
    }

    std::string duplicate_condition(const std::string &db_path,
                                    const std::string &bid_uid,
                                    const std::string &condition_uid)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return "";

        ost_db::Statement uid_stmt = db.prepare(
            "SELECT COALESCE(MAX(UID), 0) + 1 FROM BidConditions");
        int new_uid = 1;
        if (uid_stmt.valid() && uid_stmt.step())
            new_uid = uid_stmt.column_int(0);

        ost_db::Statement ref_stmt = db.prepare(
            "SELECT COALESCE(MAX(RefNo), 0) + 1 FROM BidConditions WHERE BidUID = ?");
        int new_ref = 1;
        if (ref_stmt.valid())
        {
            ref_stmt.bind_text(1, bid_uid);
            if (ref_stmt.step())
                new_ref = ref_stmt.column_int(0);
        }

        if (!db.begin_transaction())
            return "";

        db.exec("DROP TABLE IF EXISTS _dup_cond");
        ost_db::Statement copy = db.prepare(
            "CREATE TEMP TABLE _dup_cond AS "
            "SELECT * FROM BidConditions WHERE UID = ?");
        if (!copy.valid())
        {
            db.exec("ROLLBACK");
            return "";
        }
        copy.bind_text(1, condition_uid);
        copy.step();

        ost_db::Statement chk = db.prepare("SELECT COUNT(*) FROM _dup_cond");
        if (!chk.valid() || !chk.step() || chk.column_int(0) == 0)
        {
            db.exec("DROP TABLE IF EXISTS _dup_cond");
            db.exec("ROLLBACK");
            return "";
        }

        ost_db::Statement upd = db.prepare(
            "UPDATE _dup_cond SET UID = ?, RefNo = ?");
        if (upd.valid())
        {
            upd.bind_int(1, new_uid);
            upd.bind_int(2, new_ref);
            upd.step();
        }

        db.exec("INSERT INTO BidConditions SELECT * FROM _dup_cond");
        db.exec("DROP TABLE IF EXISTS _dup_cond");

        db.commit();

        return std::to_string(new_uid);
    }

    bool delete_condition(const std::string &db_path,
                          const std::string &bid_uid,
                          const std::string &condition_uid)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        db.begin_transaction();

        const char *takeoff_children[] = {
            "BidDimensions", "BidALines", "BidArrows"};
        for (const char *table : takeoff_children)
        {
            std::string sql =
                "DELETE FROM [" + std::string(table) +
                "] WHERE BidTakeoffFromUID IN "
                "(SELECT UID FROM BidTakeoffs "
                "WHERE BidConditionUID = ? AND BidUID = ?)";
            ost_db::Statement stmt = db.prepare(sql);
            if (stmt.valid())
            {
                stmt.bind_text(1, condition_uid);
                stmt.bind_text(2, bid_uid);
                stmt.step();
            }
        }

        {
            ost_db::Statement stmt = db.prepare(
                "DELETE FROM BidPercents WHERE BidTakeoffUID IN "
                "(SELECT UID FROM BidTakeoffs "
                "WHERE BidConditionUID = ? AND BidUID = ?)");
            if (stmt.valid())
            {
                stmt.bind_text(1, condition_uid);
                stmt.bind_text(2, bid_uid);
                stmt.step();
            }
        }

        {
            ost_db::Statement del_tk = db.prepare(
                "DELETE FROM BidTakeoffs "
                "WHERE BidConditionUID = ? AND BidUID = ?");
            if (del_tk.valid())
            {
                del_tk.bind_text(1, condition_uid);
                del_tk.bind_text(2, bid_uid);
                del_tk.step();
            }
        }

        {
            ost_db::Statement del_la = db.prepare(
                "DELETE FROM BidLaborActivity "
                "WHERE BidConditionUID = ? AND BidUID = ?");
            if (del_la.valid())
            {
                del_la.bind_text(1, condition_uid);
                del_la.bind_text(2, bid_uid);
                del_la.step();
            }
        }

        {
            ost_db::Statement del_styles = db.prepare(
                "DELETE FROM ConditionSetStyles "
                "WHERE ConditionStyleUID = ?");
            if (!del_styles.valid())
            {
                db.exec("ROLLBACK");
                return false;
            }
            del_styles.bind_text(1, condition_uid);
            del_styles.step();
        }

        ost_db::Statement del_cdn = db.prepare(
            "DELETE FROM BidConditions "
            "WHERE UID = ? AND BidUID = ?");
        if (!del_cdn.valid())
        {
            db.exec("ROLLBACK");
            return false;
        }
        del_cdn.bind_text(1, condition_uid);
        del_cdn.bind_text(2, bid_uid);
        del_cdn.step();

        return db.commit();
    }

    ost_domain::RenderConfig load_render_config(const std::string &db_path)
    {
        ost_domain::RenderConfig config;
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return config;

        std::string mode = get_pref(db, "color_mode", ost_domain::RenderConfig::MODE_SOLID);
        if (config.is_valid_mode(mode))
            config.color_mode = mode;

        std::string gray = get_pref(db, "grayscale_enabled", "0");
        config.grayscale_enabled = (gray == "1");

        return config;
    }

    bool save_render_config(const std::string &db_path,
                            const ost_domain::RenderConfig &config)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        set_pref(db, "color_mode", config.color_mode);
        set_pref(db, "grayscale_enabled", config.grayscale_enabled ? "1" : "0");
        return true;
    }

}
