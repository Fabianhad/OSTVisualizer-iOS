// ProjectStore component — load_project (DB → in-memory ProjectData).
#include "../ProjectStore_Internal.hpp"

namespace ost_store
{

    ost_data::ProjectData load_project(const std::string &db_path,
                                       const std::string &bid_uid)
    {
        ost_data::ProjectData project;
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return project;

        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, JobName, BidNo, BidDate, Notes, JobID, "
                "JobStatusUID, EstimatorUID, MeasureBase, TakeoffIncrements, "
                "OrigBidProjectUID, CopyFromBidNO, CopyTimeStamp "
                "FROM Bids WHERE UID = ?");
            if (stmt.valid())
            {
                stmt.bind_text(1, bid_uid);
                if (stmt.step())
                {
                    project.bid.uid = stmt.column_text(0);
                    project.bid.name = stmt.column_text(1);
                    project.bid.bid_no = stmt.column_int(2);
                    project.bid.bid_date = stmt.column_text(3);
                    project.bid.notes = stmt.column_text(4);
                    project.bid.job_id = stmt.column_text(5);
                    project.bid.status_uid = stmt.column_text(6);
                    project.bid.estimator_uid = stmt.column_text(7);
                    project.bid.measure_base = stmt.column_int(8);
                    project.bid.takeoff_increments = stmt.column_double(9);
                    project.bid.orig_bid_project_uid = stmt.column_text(10);
                    project.bid.copy_from_bid_no = stmt.column_int(11);
                    project.bid.copy_timestamp = stmt.column_text(12);

                    project.bid_name = project.bid.name;
                }
            }
        }

        {
            ost_db::Statement stmt = db.prepare(
                "SELECT p.Name FROM BidProjects p "
                "INNER JOIN Bids b ON b.BidProjectUID = p.UID "
                "WHERE b.UID = ?");
            if (stmt.valid())
            {
                stmt.bind_text(1, bid_uid);
                if (stmt.step())
                    project.project_name = stmt.column_text(0);
            }
            if (project.project_name.empty())
                project.project_name = project.bid_name;
        }

        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name, Width, Height, ScaleFactor1, ScaleFactor2, "
                "Rotation, Index1, BidPageFolderUID, ImagePath, OverlayImagePath, "
                "FlipX, FlipY, OverlayOffsetX, OverlayOffsetY, OverlayRotation, "
                "DeskewRotationOverlay, OverlayResized, OverlayRect, RasterDrawMethod "
                "FROM BidPages WHERE BidUID = ?");
            if (stmt.valid())
            {
                stmt.bind_text(1, bid_uid);
                while (stmt.step())
                {
                    ost_domain::Page page;
                    page.uid = stmt.column_text(0);
                    page.name = stmt.column_text(1);
                    page.width = stmt.column_double(2);
                    page.height = stmt.column_double(3);
                    page.scale_factor1 = stmt.column_double(4);
                    page.scale_factor2 = stmt.column_double(5);
                    if (page.scale_factor1 == 0.0)
                        page.scale_factor1 = 1.0;
                    if (page.scale_factor2 == 0.0)
                        page.scale_factor2 = 1.0;
                    page.rotation = stmt.column_int(6);
                    int idx1 = stmt.column_int(7);
                    page.page_index = (idx1 > 0) ? idx1 - 1 : 0;
                    page.folder_uid = stmt.column_text(8);
                    page.image_path = stmt.column_text(9);
                    page.overlay_image_path = stmt.column_text(10);
                    page.flip_x = stmt.column_int(11) != 0;
                    page.flip_y = stmt.column_int(12) != 0;
                    page.overlay_offset_x = stmt.column_double(13);
                    page.overlay_offset_y = stmt.column_double(14);
                    page.overlay_rotation = stmt.column_double(15);
                    page.deskew_rotation_overlay = stmt.column_double(16);
                    page.overlay_resized = stmt.column_int(17) != 0;
                    page.overlay_rect = stmt.column_text(18);
                    page.image_show_mode = stmt.column_int(19);
                    project.pages[page.uid] = std::move(page);
                }
            }
        }

        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name, Show, Sequence "
                "FROM BidLayers WHERE BidUID = ?");
            if (stmt.valid())
            {
                stmt.bind_text(1, bid_uid);
                while (stmt.step())
                {
                    ost_domain::Layer layer;
                    layer.uid = stmt.column_text(0);
                    layer.name = stmt.column_text(1);
                    int show_val = stmt.column_int(2);
                    layer.visible = (show_val == 1 || show_val == -1);
                    layer.sequence = stmt.column_int(3);
                    project.layers[layer.uid] = std::move(layer);
                }
            }
        }

        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name FROM CdnTypes");
            if (stmt.valid())
            {
                while (stmt.step())
                {
                    ost_domain::CdnType ct;
                    ct.uid = stmt.column_text(0);
                    ct.name = stmt.column_text(1);
                    project.cdn_types[ct.uid] = std::move(ct);
                }
            }
        }

        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name FROM BidConditionFolders WHERE BidUID = ?");
            if (stmt.valid())
            {
                stmt.bind_text(1, bid_uid);
                while (stmt.step())
                {
                    ost_domain::ConditionFolder cf;
                    cf.uid = stmt.column_text(0);
                    cf.name = stmt.column_text(1);
                    project.condition_folders[cf.uid] = std::move(cf);
                }
            }
        }

        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name, Type, Thickness, Height, Width, Depth, "
                "Rise, Run, Shape, ColorFill, Pattern, "
                "DisplaySize, Backout, DropRun, DropValue, BidLayerUID, CdnTypeUID, "
                "BidConditionFolderUID, RefNo, "
                "Notes, ColorLine, Spacing, "
                "UOM1, UOM2, UOM3, "
                "Quantity1, Quantity2, Quantity3, "
                "RoundUp, RoundQuantity, "
                "Connect, ConnectTolerance, Trim, "
                "Grid, GridSize1, GridSize2, Gap, "
                "DisplayDimension, DisplayName, DisplayGridWhileDrawing, "
                "SnapToLinear, IsCurvedSegment "
                "FROM BidConditions WHERE BidUID = ?");
            if (stmt.valid())
            {
                stmt.bind_text(1, bid_uid);
                while (stmt.step())
                {
                    ost_domain::Condition cdn;
                    cdn.uid = stmt.column_text(0);
                    cdn.name = stmt.column_text(1);
                    cdn.condition_type = static_cast<ost_domain::ConditionType>(stmt.column_int(2));
                    cdn.thickness = stmt.column_double(3);
                    cdn.height = stmt.column_double(4);
                    cdn.width = stmt.column_double(5);
                    cdn.depth = stmt.column_double(6);
                    cdn.rise = stmt.column_double(7);
                    cdn.run = stmt.column_double(8);
                    cdn.shape = static_cast<ost_domain::ShapeId>(stmt.column_int(9));
                    cdn.color_fill = stmt.column_int(10);
                    cdn.pattern = static_cast<ost_domain::Pattern>(stmt.column_int(11));
                    cdn.display_size = stmt.column_double(12);
                    if (cdn.display_size == 0.0)
                        cdn.display_size = 100.0;
                    cdn.backout = (stmt.column_int(13) != 0);
                    cdn.drop_run = (stmt.column_int(14) != 0);
                    cdn.drop_value = stmt.column_double(15);
                    cdn.layer_uid = stmt.column_text(16);
                    cdn.cdn_type_uid = stmt.column_text(17);
                    cdn.condition_folder_uid = stmt.column_text(18);
                    cdn.ref_no = stmt.column_int(19);

                    cdn.notes = stmt.column_text(20);
                    cdn.color_line = stmt.column_int(21);
                    cdn.spacing = stmt.column_double(22);
                    cdn.uom1 = stmt.column_int(23);
                    cdn.uom2 = stmt.column_int(24);
                    cdn.uom3 = stmt.column_int(25);
                    cdn.calc_type1 = stmt.column_int(26);
                    cdn.calc_type2 = stmt.column_int(27);
                    cdn.calc_type3 = stmt.column_int(28);
                    cdn.round_up = stmt.column_double(29);
                    cdn.round_quantity = (stmt.column_int(30) != 0);
                    cdn.connect = (stmt.column_int(31) != 0);
                    cdn.connect_tolerance = stmt.column_double(32);
                    cdn.trim = (stmt.column_int(33) != 0);
                    cdn.grid = (stmt.column_int(34) != 0);
                    cdn.grid_size1 = stmt.column_double(35);
                    cdn.grid_size2 = stmt.column_double(36);
                    cdn.gap = stmt.column_double(37);
                    cdn.display_dimension = (stmt.column_int(38) != 0);
                    cdn.display_name = (stmt.column_int(39) != 0);
                    cdn.display_grid_while_drawing = (stmt.column_int(40) != 0);
                    cdn.snap_to_linear = stmt.column_int(41);
                    cdn.is_curved_segment = (stmt.column_int(42) != 0);

                    auto zv = extract_z_value(cdn.name);
                    cdn.z_value = zv.first;
                    cdn.is_top = zv.second;

                    if (!cdn.cdn_type_uid.empty())
                    {
                        auto it = project.cdn_types.find(cdn.cdn_type_uid);
                        if (it != project.cdn_types.end())
                            cdn.cdn_type_name = it->second.name;
                    }

                    project.conditions[cdn.uid] = std::move(cdn);
                }
            }
        }

        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, BidConditionUID, BidPageUID, BidAreaUID, "
                "Rotation, Curve, ParentUID, IsNegativeQuantity, Position "
                "FROM BidTakeoffs WHERE BidUID = ?");
            if (stmt.valid())
            {
                stmt.bind_text(1, bid_uid);
                while (stmt.step())
                {
                    ost_domain::Takeoff to;
                    to.uid = stmt.column_text(0);
                    to.condition_uid = stmt.column_text(1);
                    to.page_uid = stmt.column_text(2);
                    to.area_uid = stmt.column_text(3);
                    if (to.area_uid.empty())
                        to.area_uid = "0";
                    to.rotation = stmt.column_double(4);
                    to.curve = stmt.column_int(5);
                    std::string parent = stmt.column_text(6);
                    to.parent_uid = parent.empty() ? "0" : parent;
                    to.is_negative = (stmt.column_int(7) != 0);

                    std::string pos_str = stmt.column_text(8);
                    to.position = parse_position(pos_str);

                    if (project.conditions.find(to.condition_uid) != project.conditions.end() && to.position.size() >= 2)
                    {
                        project.takeoffs.push_back(std::move(to));
                    }
                }
            }
        }

        {
            struct FolderRow
            {
                std::string uid, name, description, parent_uid;
            };
            std::vector<FolderRow> folder_rows;

            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name, Description, ParentUID "
                "FROM BidPageFolders WHERE BidUID = ?");
            if (stmt.valid())
            {
                stmt.bind_text(1, bid_uid);
                while (stmt.step())
                {
                    FolderRow fr;
                    fr.uid = stmt.column_text(0);
                    fr.name = stmt.column_text(1);
                    fr.description = stmt.column_text(2);
                    fr.parent_uid = stmt.column_text(3);
                    folder_rows.push_back(std::move(fr));
                }
            }

            std::unordered_map<std::string, ost_domain::Folder> all_folders;
            std::unordered_map<std::string, std::string> parent_map;
            for (const auto &fr : folder_rows)
            {
                ost_domain::Folder f;
                f.uid = fr.uid;
                f.name = fr.name;
                f.description = fr.description;
                all_folders[fr.uid] = std::move(f);
                parent_map[fr.uid] = fr.parent_uid;
            }

            std::set<std::string> assigned_pages;
            for (auto &pp : project.pages)
            {
                const std::string &folder_uid = pp.second.folder_uid;
                if (!folder_uid.empty() && all_folders.find(folder_uid) != all_folders.end())
                {
                    all_folders[folder_uid].page_uids.push_back(pp.first);
                    assigned_pages.insert(pp.first);
                }
            }

            for (auto &pp : project.pages)
            {
                if (assigned_pages.find(pp.first) == assigned_pages.end())
                {
                    pp.second.folder_uid = "";
                }
            }

            for (auto &pair : all_folders)
            {
                const std::string &puid = parent_map[pair.first];
                if (!puid.empty() && all_folders.find(puid) != all_folders.end())
                {
                    all_folders[puid].subfolders[pair.first] = std::move(pair.second);
                }
            }

            for (auto &pair : all_folders)
            {
                const std::string &puid = parent_map[pair.first];
                if (puid.empty() || all_folders.find(puid) == all_folders.end())
                {
                    if (!pair.second.empty())
                        project.folders[pair.first] = std::move(pair.second);
                }
            }
        }

        return project;
    }

}
