// ProjectStore component — load_project (DB → in-memory ProjectData).
#include "../ProjectStore_Internal.hpp"
#include "../../../Domain/CoreLog.hpp"
#include "../../../Domain/UomService.hpp"
#include "../../../Import/TextRules.hpp"

namespace ost_store
{

    namespace
    {
        // Windows `int(Rotation or 0) % 360`: the value is truncated toward zero, then wrapped with Python's floored modulo
        // (-90 -> 270, 450 -> 90, -0.5 -> 0). Values beyond 63 bits are clamped by sqlite before the modulo.
        int wrap_page_rotation(long long rotation)
        {
            long long wrapped = rotation % 360;
            if (wrapped < 0)
                wrapped += 360;
            return static_cast<int>(wrapped);
        }

        // Python's `value in (-1, True)` for a stored numeric flag: only -1 and 1 (also -1.0 / 1.0) are set. Text and NULL never are.
        bool numeric_flag_is_minus_one_or_one(const ost_db::Statement &stmt, int idx)
        {
            switch (stmt.column_type(idx))
            {
            case ost_db::Statement::TypeInteger:
            {
                const long long value = stmt.column_int64(idx);
                return value == -1 || value == 1;
            }
            case ost_db::Statement::TypeFloat:
            {
                const double value = stmt.column_double(idx);
                return value == -1.0 || value == 1.0;
            }
            default:
                return false;
            }
        }

        // Length of the Unicode whitespace (Python str.isspace) at the start of s[from, to), 0 when there is none. UTF-8 input.
        std::size_t whitespace_length_at(const std::string &s, std::size_t from, std::size_t to)
        {
            const unsigned char c = static_cast<unsigned char>(s[from]);
            if ((c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x20))
                return 1;
            auto byte = [&](std::size_t i) -> int { return from + i < to ? static_cast<unsigned char>(s[from + i]) : -1; };
            if (c == 0xC2 && (byte(1) == 0x85 || byte(1) == 0xA0)) // U+0085, U+00A0
                return 2;
            if (c == 0xE1 && byte(1) == 0x9A && byte(2) == 0x80) // U+1680
                return 3;
            if (c == 0xE2 && byte(1) == 0x80 &&
                ((byte(2) >= 0x80 && byte(2) <= 0x8A) || byte(2) == 0xA8 || byte(2) == 0xA9 || byte(2) == 0xAF)) // U+2000-200A, 2028, 2029, 202F
                return 3;
            if (c == 0xE2 && byte(1) == 0x81 && byte(2) == 0x9F) // U+205F
                return 3;
            if (c == 0xE3 && byte(1) == 0x80 && byte(2) == 0x80) // U+3000
                return 3;
            return 0;
        }

        // Python str.strip() of UTF-8 text.
        std::string strip_unicode_whitespace(const std::string &s)
        {
            std::size_t begin = 0;
            while (begin < s.size())
            {
                const std::size_t length = whitespace_length_at(s, begin, s.size());
                if (length == 0)
                    break;
                begin += length;
            }
            std::size_t end = s.size();
            while (end > begin)
            {
                // the whitespace sequences are at most 3 bytes: find one that ends exactly at `end`
                std::size_t length = 0;
                for (std::size_t candidate = 1; candidate <= 3 && candidate <= end - begin; ++candidate)
                {
                    const std::size_t start = end - candidate;
                    if (whitespace_length_at(s, start, end) == candidate)
                    {
                        length = candidate;
                        break;
                    }
                }
                if (length == 0)
                    break;
                end -= length;
            }
            return s.substr(begin, end - begin);
        }

        // Python str.strip() of bytes decoded as latin-1 (decode_value of a BLOB): the latin-1 whitespace is 09-0D, 1C-20, 85 and A0.
        std::string strip_latin1_whitespace(const std::string &s)
        {
            auto is_space = [](unsigned char c) { return (c >= 0x09 && c <= 0x0D) || (c >= 0x1C && c <= 0x20) || c == 0x85 || c == 0xA0; };
            std::size_t begin = 0;
            std::size_t end = s.size();
            while (begin < end && is_space(static_cast<unsigned char>(s[begin])))
                ++begin;
            while (end > begin && is_space(static_cast<unsigned char>(s[end - 1])))
                --end;
            return s.substr(begin, end - begin);
        }

        // Windows _is_stored_flag_set (bid_data_reader.py): a stored text / bytes value is set when it reads "-1", "1" or "true"
        // (stripped, case-insensitive); a number when it is -1 or 1. BidPages.OverlayResized is VARCHAR in Access, so the flag
        // normally arrives as text.
        bool stored_flag_is_set(const ost_db::Statement &stmt, int idx)
        {
            const ost_db::Statement::ColumnType type = stmt.column_type(idx);
            if (type != ost_db::Statement::TypeText && type != ost_db::Statement::TypeBlob)
                return numeric_flag_is_minus_one_or_one(stmt, idx);
            std::string text = type == ost_db::Statement::TypeText ? strip_unicode_whitespace(stmt.column_text(idx))
                                                                    : strip_latin1_whitespace(stmt.column_text(idx));
            for (char &c : text)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            return text == "-1" || text == "1" || text == "true";
        }

        // A reference column (BidConditionUID, BidPageUID) as stored. Windows reads str(value): NULL becomes "None" (a non-empty
        // string, so the Takeoff contract holds and the reference check then fails on int("None")), "" stays empty (the contract
        // fails). `is_null` tells the two apart; `text` is the stored text.
        struct StoredReference
        {
            bool is_null = false;
            std::string text;
        };

        StoredReference read_reference(const ost_db::Statement &stmt, int idx)
        {
            StoredReference reference;
            reference.is_null = stmt.column_type(idx) == ost_db::Statement::TypeNull;
            if (!reference.is_null)
                reference.text = stmt.column_text(idx);
            return reference;
        }

        // Windows require_existing_bid_owned_references for one reference: "" when it names a row of `parents`, else the message
        //   "<child_table>.UID=<uid> references missing <parent_table>.UID=<referenced> through <child_column>."
        // The reference is read like Python int(str): a value that is no integer (a NULL, which Windows reads as int("None"),
        // or text such as "abc") cannot name a row; Windows raises a bare ValueError there, the message here keeps the
        // "references missing" form and shows the stored value (<missing> for NULL).
        std::string missing_reference_problem(const char *child_table, long long child_uid, const StoredReference &reference,
                                              const std::set<long long> &parents, const char *child_column,
                                              const char *parent_table)
        {
            long long referenced = 0;
            const bool is_integer =
                !reference.is_null &&
                ost_import::text_rules::py_parse_int(reference.text, referenced) == ost_import::text_rules::NumberParse::Ok;
            if (is_integer && parents.count(referenced))
                return std::string();
            const std::string shown = is_integer ? std::to_string(referenced)
                                                 : (reference.is_null ? std::string("<missing>") : reference.text);
            return std::string(child_table) + ".UID=" + std::to_string(child_uid) + " references missing " + parent_table +
                   ".UID=" + shown + " through " + child_column + ".";
        }

        // The numbers of the (valid) UIDs of a loaded family.
        template <typename Map>
        std::set<long long> uid_numbers(const Map &family)
        {
            std::set<long long> numbers;
            for (const auto &pair : family)
            {
                long long value = 0;
                if (integrity::parse_uid(pair.first, value))
                    numbers.insert(value);
            }
            return numbers;
        }

        // One BidTakeoffs row as the bid load judges it (bid_data_reader.py _parse_bid_takeoffs_for_bid).
        struct TakeoffRow
        {
            std::string uid_text; // the stored UID, "" for NULL
            StoredReference condition;
            StoredReference page;
            bool rotation_unreadable = false; // Python float(Rotation) raises
            std::string rotation_text;
            ost_domain::Takeoff takeoff;
        };

        // Python `value not in (0, None)` of a stored flag (IsTemplate, IsLocked): NULL and a number 0 are not set, any other number
        // is, and so is any text (a str never equals 0).
        bool flag_is_set(const ost_db::Statement &stmt, int idx)
        {
            switch (stmt.column_type(idx))
            {
            case ost_db::Statement::TypeNull:
                return false;
            case ost_db::Statement::TypeInteger:
                return stmt.column_int64(idx) != 0;
            case ost_db::Statement::TypeFloat:
                return stmt.column_double(idx) != 0.0;
            default:
                return true;
            }
        }

        // The layer list of the layer sheet as load_project uses it: layer_reads::read_merged_layers (below, exported for the layer
        // writers and reload_layers), and a list that cannot be read logs "Failed to load bid layers" and is empty, it never fails the
        // bid (project_read_service.get_merged_bid_layers).
        std::vector<ost_domain::BidLayer> read_layer_list(ost_db::OstDatabase &db, const std::string &bid_uid)
        {
            std::vector<ost_domain::BidLayer> merged;
            std::string why;
            if (!layer_reads::read_merged_layers(db, bid_uid, merged, why))
            {
                ost_log::warning("Failed to load bid layers", why);
                return std::vector<ost_domain::BidLayer>();
            }
            return merged;
        }

        // One bid-owned row of BidPageFolders.
        struct PageFolderRow
        {
            std::string uid, name, description, parent_uid;
        };

        // Windows _get_bid_folder_page_structure + remove_empty_folders: `rows` come in (Name, UID) order, `pages_of[i]` are the pages
        // of rows[i] in page order. A folder whose parent is missing (or not one of the bid's folders) is a root; a folder that is
        // empty (no pages and no non-empty subfolder) is dropped at every level. Subfolders keep the row order. Every folder is moved
        // exactly once and only after all its subfolders were attached, so the result does not depend on the order of the rows. A
        // folder in a ParentUID cycle would be unreachable from any root: load_project refuses such rows before it gets here.
        void build_page_folder_tree(const std::vector<PageFolderRow> &rows,
                                    const std::vector<std::vector<std::string>> &pages_of,
                                    ost_domain::OrderedUidMap<ost_domain::Folder> &tree)
        {
            const std::size_t count = rows.size();
            std::unordered_map<std::string, std::size_t> index_by_uid;
            for (std::size_t i = 0; i < count; ++i)
                index_by_uid[rows[i].uid] = i;

            std::vector<std::vector<std::size_t>> children(count);
            std::vector<std::size_t> roots;
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::string &parent = rows[i].parent_uid;
                auto found = parent.empty() || parent == "0" ? index_by_uid.end() : index_by_uid.find(parent);
                if (found == index_by_uid.end())
                    roots.push_back(i);
                else
                    children[found->second].push_back(i);
            }

            std::vector<ost_domain::Folder> folders(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                folders[i].uid = rows[i].uid;
                folders[i].name = rows[i].name;
                folders[i].description = rows[i].description;
                folders[i].page_uids = pages_of[i];
            }

            // parents before children, so walking the order backwards completes every folder before it is attached to its parent
            std::vector<std::size_t> order(roots);
            for (std::size_t k = 0; k < order.size(); ++k)
                for (const std::size_t child : children[order[k]])
                    order.push_back(child);
            for (std::size_t k = order.size(); k-- > 0;)
            {
                const std::size_t i = order[k];
                for (const std::size_t child : children[i])
                    if (!folders[child].empty())
                        folders[i].subfolders.emplace(rows[child].uid, std::move(folders[child]));
            }
            for (const std::size_t root : roots)
                if (!folders[root].empty())
                    tree.emplace(rows[root].uid, std::move(folders[root]));
        }
    } // namespace

    namespace layer_reads
    {

        // The bid's own layers: the `SELECT UID, Name, Show, Sequence FROM BidLayers WHERE BidUID = ?` of load_project, exported (plan 5.3) so
        // that reload_layers fills ProjectData::layers exactly like a load does.
        bool read_own_layers(ost_db::OstDatabase &db,
                             const std::string &bid_uid,
                             std::unordered_map<std::string, ost_domain::Layer> &out,
                             std::vector<std::string> &uid_texts,
                             std::string &why)
        {
            out.clear();
            uid_texts.clear();
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name, Show, Sequence "
                "FROM BidLayers WHERE BidUID = ?");
            if (!stmt.valid())
            {
                why = "Cannot read BidLayers: " + stmt.error_message();
                return false;
            }
            stmt.bind_text(1, bid_uid);
            while (stmt.step())
            {
                ost_domain::Layer layer;
                layer.uid = stmt.column_text(0);
                layer.name = stmt.column_text(1);
                int show_val = stmt.column_int(2);
                layer.visible = (show_val == 1 || show_val == -1);
                layer.sequence = stmt.column_int(3);
                uid_texts.push_back(layer.uid);
                out[layer.uid] = std::move(layer);
            }
            if (stmt.failed())
            {
                why = "Cannot read BidLayers: " + stmt.error_message();
                return false;
            }
            return true;
        }

        // The layer list of the layer sheet: Windows get_bid_layers_for_sidebar (the bid's layers plus the locked template layers,
        // WHERE BidUID = ? OR (IsTemplate <> 0 AND IsLocked <> 0), the optional columns IsTemplate / Show / IsLocked / Sequence
        // default to 0 / -1 / 0 / 0, the filter needs both flag columns) through merge_layers_for_bid without the Comments
        // layer, as project_read_service.get_merged_bid_layers. Windows has no ORDER BY, the order of rows that tie is "database
        // order"; the rows are read in UID order so it is the same on every run.
        bool read_merged_layers(ost_db::OstDatabase &db,
                                const std::string &bid_uid,
                                std::vector<ost_domain::BidLayer> &out,
                                std::string &why)
        {
            out.clear();
            const bool has_template = db.column_exists("BidLayers", "IsTemplate");
            const bool has_locked = db.column_exists("BidLayers", "IsLocked");
            const std::string sql = std::string("SELECT [UID], [BidUID], ") + (has_template ? "[IsTemplate]" : "0") + ", [Name], " +
                                    (db.column_exists("BidLayers", "Show") ? "[Show]" : "-1") + ", " +
                                    (has_locked ? "[IsLocked]" : "0") + ", " +
                                    (db.column_exists("BidLayers", "Sequence") ? "[Sequence]" : "0") +
                                    " FROM [BidLayers] WHERE [BidUID] = ?" +
                                    (has_template && has_locked ? " OR ([IsTemplate] <> 0 AND [IsLocked] <> 0)" : "") +
                                    " ORDER BY [UID]";
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid())
            {
                why = stmt.error_message();
                return false;
            }
            stmt.bind_text(1, bid_uid);
            std::vector<std::string> uid_texts;
            std::vector<ost_domain::BidLayer> rows;
            while (stmt.step())
            {
                ost_domain::BidLayer layer;
                layer.uid = stmt.column_text(0);
                // str(BidUID) if BidUID else "": NULL and 0 belong to no bid
                layer.bid_uid = stmt.column_text(1);
                if (layer.bid_uid == "0")
                    layer.bid_uid.clear();
                layer.is_template = flag_is_set(stmt, 2);
                layer.name = stmt.column_text(3);
                layer.show = numeric_flag_is_minus_one_or_one(stmt, 4);
                layer.is_locked = flag_is_set(stmt, 5);
                layer.sequence = stmt.column_int(6);
                uid_texts.push_back(layer.uid);
                rows.push_back(std::move(layer));
            }
            if (stmt.failed())
            {
                why = stmt.error_message();
                return false;
            }
            const std::string problem = integrity::uid_list_problem(uid_texts, "BidLayers");
            if (!problem.empty())
            {
                why = problem;
                return false;
            }
            // get_merged_bid_layers: the Comments layer is dropped after the merge, it is neither listed nor touched by Show All /
            // Hide All (LP-18)
            std::vector<ost_domain::BidLayer> merged = ost_domain::merge_layers_for_bid(rows);
            merged.erase(std::remove_if(merged.begin(), merged.end(),
                                        [](const ost_domain::BidLayer &layer)
                                        { return ost_domain::is_comments_layer_name(layer.name); }),
                         merged.end());
            out = std::move(merged);
            return true;
        }

    } // namespace layer_reads

    bool load_project(const std::string &db_path,
                      const std::string &bid_uid,
                      ost_data::ProjectData &out,
                      std::string *error)
    {
        out = ost_data::ProjectData();
        if (error)
            error->clear();
        // A bid that cannot be read completely is a failure, never a project with empty families (Windows: a failing read or a
        // missing required column raises). Nothing partial is handed out.
        auto fail = [&out, error](const std::string &message) -> bool
        {
            out = ost_data::ProjectData();
            if (error)
                *error = message;
            return false;
        };
        auto cannot_read = [&fail](const char *what, const ost_db::Statement &stmt) -> bool
        { return fail(std::string("Cannot read ") + what + ": " + stmt.error_message()); };

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return fail("Cannot open the database: " + db.last_error_message());

        ost_data::ProjectData project;

        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, JobName, BidNo, BidDate, Notes, JobID, "
                "JobStatusUID, EstimatorUID, MeasureBase, TakeoffIncrements, "
                "OrigBidProjectUID, CopyFromBidNO, CopyTimeStamp "
                "FROM Bids WHERE UID = ?");
            if (!stmt.valid())
                return cannot_read("Bids", stmt);
            stmt.bind_text(1, bid_uid);
            const ost_db::StepResult found = stmt.step_result();
            if (found == ost_db::StepResult::Error)
                return cannot_read("Bids", stmt);
            if (found == ost_db::StepResult::Done)
                return fail("Bids has no row for UID " + bid_uid + "; the requested bid does not exist.");

            project.bid.uid = stmt.column_text(0);
            project.bid.name = stmt.column_text(1);
            project.bid.bid_no = stmt.column_int(2);
            project.bid.bid_date = stmt.column_text(3);
            project.bid.notes = stmt.column_text(4);
            project.bid.job_id = stmt.column_text(5);
            project.bid.status_uid = stmt.column_text(6);
            project.bid.estimator_uid = stmt.column_text(7);
            project.bid.measure_base = stmt.column_int(8);
            // Only a NULL TakeoffIncrements is 1 inch: hierarchy_reader.py:126-130 loads
            // `1.0 if bid_row.TakeoffIncrements is None else float(bid_row.TakeoffIncrements)` and settings_reader.py:151
            // `parse_float(TakeoffIncrements, 1.0)` agrees. Any stored number is kept, a stored 0 included (test_hierarchy_reader.py:
            // 831-836, DBP-05), and a value <= 0 means "snapping off" (Bid::snap_increments_ost, plan_view/view.py
            // set_snap_settings).
            project.bid.takeoff_increments = stmt.column_double(9);
            if (stmt.column_type(9) == ost_db::Statement::TypeNull)
                project.bid.takeoff_increments = 1.0;
            project.bid.orig_bid_project_uid = stmt.column_text(10);
            project.bid.copy_from_bid_no = stmt.column_int(11);
            project.bid.copy_timestamp = stmt.column_text(12);

            project.bid_name = project.bid.name;
        }

        // iOS only: the name of the project folder. A database without the table, or a bid without a project, shows the bid name.
        if (db.table_exists("BidProjects"))
        {
            ost_db::Statement stmt = db.prepare(
                "SELECT p.Name FROM BidProjects p "
                "INNER JOIN Bids b ON b.BidProjectUID = p.UID "
                "WHERE b.UID = ?");
            if (!stmt.valid())
                return cannot_read("BidProjects", stmt);
            stmt.bind_text(1, bid_uid);
            const ost_db::StepResult found = stmt.step_result();
            if (found == ost_db::StepResult::Error)
                return cannot_read("BidProjects", stmt);
            if (found == ost_db::StepResult::Row)
                project.project_name = stmt.column_text(0);
        }
        if (project.project_name.empty())
            project.project_name = project.bid_name;

        // The pages in Windows order (Sequence, Name, UID); `page_order` keeps that order for the folder tree below.
        std::vector<std::string> page_order;
        std::vector<std::string> page_uid_texts; // the stored UIDs in row order, judged by uid_list_problem below
        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name, Width, Height, ScaleFactor1, ScaleFactor2, "
                "Rotation, Index1, BidPageFolderUID, ImagePath, OverlayImagePath, "
                "FlipX, FlipY, OverlayOffsetX, OverlayOffsetY, OverlayRotation, "
                "DeskewRotationOverlay, OverlayResized, OverlayRect, Show, Sequence, SheetNo "
                "FROM BidPages WHERE BidUID = ? ORDER BY Sequence, Name, UID");
            if (!stmt.valid())
                return cannot_read("BidPages", stmt);
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
                page.rotation = wrap_page_rotation(stmt.column_int64(6));
                int idx1 = stmt.column_int(7);
                page.page_index = (idx1 > 0) ? idx1 - 1 : 0;
                page.folder_uid = stmt.column_text(8);
                page.image_path = stmt.column_text(9);
                page.overlay_image_path = stmt.column_text(10);
                page.flip_x = numeric_flag_is_minus_one_or_one(stmt, 11);
                page.flip_y = numeric_flag_is_minus_one_or_one(stmt, 12);
                page.overlay_offset_x = stmt.column_double(13);
                page.overlay_offset_y = stmt.column_double(14);
                page.overlay_rotation = stmt.column_double(15);
                page.deskew_rotation_overlay = stmt.column_double(16);
                page.overlay_resized = stored_flag_is_set(stmt, 17);
                page.overlay_rect = stmt.column_text(18);
                page.image_show_mode = stmt.column_int(19);
                page.sequence = stmt.column_int(20);
                page.sheet_no = stmt.column_text(21);
                page_order.push_back(page.uid);
                page_uid_texts.push_back(page.uid);
                project.pages[page.uid] = std::move(page);
            }
            if (stmt.failed())
                return cannot_read("BidPages", stmt);
        }
        // LP-08: the page order of the page selector is this SQL order, not Index1.
        project.page_order = page_order;

        // LP-12, Windows bid_data_reader._parse_bid_selected_page: BidSettings is an optional table and BidPageSelectedUID an
        // optional column (a database without either has no saved page). A bid has at most one settings row: a second one
        // fails the load (fetch_optional_bid_settings_row raises BidSettingsCardinalityError, which is not a database error and
        // so is not swallowed). NULL, an empty text and 0 mean no saved page; whether the page still exists is decided where it
        // is used (ost_data::initial_page_uid).
        if (db.table_exists("BidSettings") && db.column_exists("BidSettings", "BidPageSelectedUID"))
        {
            ost_db::Statement stmt = db.prepare("SELECT BidPageSelectedUID FROM BidSettings WHERE BidUID = ?");
            if (!stmt.valid())
                return cannot_read("BidSettings", stmt);
            stmt.bind_text(1, bid_uid);
            const ost_db::StepResult first = stmt.step_result();
            if (first == ost_db::StepResult::Error)
                return cannot_read("BidSettings", stmt);
            if (first == ost_db::StepResult::Row)
            {
                const std::string saved = stmt.column_text(0);
                const ost_db::StepResult second = stmt.step_result();
                if (second == ost_db::StepResult::Error)
                    return cannot_read("BidSettings", stmt);
                if (second == ost_db::StepResult::Row)
                    return fail("BidSettings has multiple rows for Bids.UID=" + bid_uid + "; expected at most one.");
                if (saved != "0")
                    project.selected_page_uid = saved;
            }
        }

        // Windows: BidLayers, CdnTypes, BidConditionFolders and BidPageFolders are optional tables (a database without one has an
        // empty family); a table that exists must be readable.
        std::vector<std::string> layer_uid_texts;
        if (db.table_exists("BidLayers"))
        {
            // layer_reads::read_own_layers (exported for reload_layers, plan 5.3): "Cannot read BidLayers: <sqlite text>" on a failure
            std::string why;
            if (!layer_reads::read_own_layers(db, bid_uid, project.layers, layer_uid_texts, why))
                return fail(why);
        }

        std::vector<std::string> cdn_type_uid_texts; // judged by master_data_uid_problem below
        if (db.table_exists("CdnTypes"))
        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, Name FROM CdnTypes");
            if (!stmt.valid())
                return cannot_read("CdnTypes", stmt);
            while (stmt.step())
            {
                ost_domain::CdnType ct;
                ct.uid = stmt.column_text(0);
                ct.name = stmt.column_text(1);
                cdn_type_uid_texts.push_back(ct.uid);
                project.cdn_types[ct.uid] = std::move(ct);
            }
            if (stmt.failed())
                return cannot_read("CdnTypes", stmt);
        }

        std::vector<std::string> condition_folder_uid_texts;
        if (db.table_exists("BidConditionFolders"))
        {
            // ParentUID is optional like in Windows (schema.optional_column "NULL"): a database without the column has flat
            // folders. NULL, "", 0 and "0" all mean "no parent" and read as "".
            const bool has_parent_column = db.column_exists("BidConditionFolders", "ParentUID");
            ost_db::Statement stmt = db.prepare(
                has_parent_column ? "SELECT UID, Name, ParentUID FROM BidConditionFolders WHERE BidUID = ?"
                                  : "SELECT UID, Name, NULL FROM BidConditionFolders WHERE BidUID = ?");
            if (!stmt.valid())
                return cannot_read("BidConditionFolders", stmt);
            stmt.bind_text(1, bid_uid);
            while (stmt.step())
            {
                ost_domain::ConditionFolder cf;
                cf.uid = stmt.column_text(0);
                cf.name = stmt.column_text(1);
                cf.parent_uid = stmt.column_text(2);
                if (cf.parent_uid == "0")
                    cf.parent_uid.clear();
                condition_folder_uid_texts.push_back(cf.uid);
                project.condition_folders[cf.uid] = std::move(cf);
            }
            if (stmt.failed())
                return cannot_read("BidConditionFolders", stmt);
        }

        std::vector<std::string> condition_uid_texts;
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
            if (!stmt.valid())
                return cannot_read("BidConditions", stmt);
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
                // bid_data_reader._normalize_display_size: NULL and every value <= 0 read as 100 (percent)
                cdn.display_size = stmt.column_double(12);
                if (cdn.display_size <= 0.0)
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

                // Windows (bid_data_reader) normalizes every loaded condition: units of the other measurement
                // system are converted (bid.measure_base == 1 is metric) and a unit that does not fit the
                // slot's calculation becomes the first valid unit of that calculation.
                ost_domain::normalize_condition_uoms_for_system(cdn, project.bid.measure_base == 1);

                condition_uid_texts.push_back(cdn.uid);
                project.conditions[cdn.uid] = std::move(cdn);
            }
            if (stmt.failed())
                return cannot_read("BidConditions", stmt);
        }

        // The takeoffs of the bid. Every row is kept, whatever its position (QTY-19: Windows keeps a takeoff whose Position is
        // empty or unparsable as position [] - a Count still counts 1, a Linear / Area measures 0); a row the Windows reader
        // refuses fails the load below (DBP-31, POS-M01): nothing is dropped silently.
        std::vector<TakeoffRow> takeoff_rows;
        {
            ost_db::Statement stmt = db.prepare(
                "SELECT UID, BidConditionUID, BidPageUID, BidAreaUID, "
                "Rotation, Curve, ParentUID, IsNegativeQuantity, Position "
                "FROM BidTakeoffs WHERE BidUID = ?");
            if (!stmt.valid())
                return cannot_read("BidTakeoffs", stmt);
            stmt.bind_text(1, bid_uid);
            while (stmt.step())
            {
                TakeoffRow row;
                ost_domain::Takeoff &to = row.takeoff;
                row.uid_text = stmt.column_text(0);
                to.uid = row.uid_text;
                row.condition = read_reference(stmt, 1);
                row.page = read_reference(stmt, 2);
                to.condition_uid = row.condition.text;
                to.page_uid = row.page.text;
                to.area_uid = stmt.column_text(3);
                if (to.area_uid.empty())
                    to.area_uid = "0";
                // float(Rotation) if the column is not None else 0.0: a number as it is, a text read like Python float() (so "nan"
                // and "inf" arrive as such and fail the Takeoff contract below); a text that is no float raises in Windows
                switch (stmt.column_type(4))
                {
                case ost_db::Statement::TypeNull:
                    to.rotation = 0.0;
                    break;
                case ost_db::Statement::TypeInteger:
                case ost_db::Statement::TypeFloat:
                    to.rotation = stmt.column_double(4);
                    break;
                default:
                {
                    row.rotation_text = stmt.column_text(4);
                    if (!ost_domain::parse_python_float(row.rotation_text, to.rotation))
                        row.rotation_unreadable = true;
                    break;
                }
                }
                // bid_data_reader.py: curve = int(Curve) if the column is not None else -1 (CURVE_DISABLED): a NULL Curve is
                // a straight takeoff, not curve 0
                to.curve = stmt.column_type(5) == ost_db::Statement::TypeNull ? -1 : stmt.column_int(5);
                std::string parent = stmt.column_text(6);
                to.parent_uid = parent.empty() ? "0" : parent;
                to.is_negative = (stmt.column_int(7) != 0);

                // parse_position_storage: an empty / NULL Position and any unparsable token give []
                std::string pos_str = stmt.column_text(8);
                to.position = parse_position(pos_str);

                takeoff_rows.push_back(std::move(row));
            }
            if (stmt.failed())
                return cannot_read("BidTakeoffs", stmt);
        }

        // ---- The integrity checks of the Windows bid load (bid_data_reader.get_bid_data, bid_owned_identity.py), in the order
        // Windows runs them: CdnTypes (master data: a UID may not repeat), layers, pages, conditions, takeoffs (UIDs, then the row
        // contract, then the references, then the pages of the parents, then the parent cycles), condition folders. The first
        // problem fails the open with its Windows message; the load hands out nothing.
        {
            std::string problem = integrity::master_data_uid_problem(cdn_type_uid_texts, "CdnTypes");
            if (problem.empty())
                problem = integrity::uid_list_problem(layer_uid_texts, "BidLayers");
            if (problem.empty())
                problem = integrity::uid_list_problem(page_uid_texts, "BidPages");
            if (problem.empty())
                problem = integrity::uid_list_problem(condition_uid_texts, "BidConditions");
            if (!problem.empty())
                return fail(problem);
        }
        {
            std::vector<std::string> takeoff_uid_texts;
            takeoff_uid_texts.reserve(takeoff_rows.size());
            for (const TakeoffRow &row : takeoff_rows)
                takeoff_uid_texts.push_back(row.uid_text);
            const std::string uid_problem = integrity::uid_list_problem(takeoff_uid_texts, "BidTakeoffs");
            if (!uid_problem.empty())
                return fail(uid_problem);
        }
        for (const TakeoffRow &row : takeoff_rows)
        {
            // Python float(Rotation) raised while the row was read
            if (row.rotation_unreadable)
                return fail("BidTakeoffs.UID=" + row.uid_text + " has an unreadable Rotation '" + row.rotation_text + "'.");
            // Takeoff.has_valid_contract: a non-empty condition and page uid (NULL reads as "None", only "" is empty), every
            // position value and the rotation finite (POS-M01)
            bool valid = !(!row.condition.is_null && row.condition.text.empty()) && !(!row.page.is_null && row.page.text.empty()) &&
                         std::isfinite(row.takeoff.rotation);
            for (const double value : row.takeoff.position)
                valid = valid && std::isfinite(value);
            if (!valid)
                return fail("BidTakeoffs.UID=" + row.uid_text + " does not satisfy the Takeoff domain contract");
        }
        {
            const std::set<long long> condition_numbers = uid_numbers(project.conditions);
            const std::set<long long> page_numbers = uid_numbers(project.pages);
            std::set<long long> takeoff_numbers;
            for (const TakeoffRow &row : takeoff_rows)
            {
                long long value = 0;
                integrity::parse_uid(row.uid_text, value);
                takeoff_numbers.insert(value);
            }
            auto child_of = [](const TakeoffRow &row)
            {
                long long value = 0;
                integrity::parse_uid(row.uid_text, value);
                return value;
            };
            for (const TakeoffRow &row : takeoff_rows)
            {
                const std::string problem = missing_reference_problem("BidTakeoffs", child_of(row), row.condition, condition_numbers,
                                                                      "BidConditionUID", "BidConditions");
                if (!problem.empty())
                    return fail(problem);
            }
            for (const TakeoffRow &row : takeoff_rows)
            {
                const std::string problem =
                    missing_reference_problem("BidTakeoffs", child_of(row), row.page, page_numbers, "BidPageUID", "BidPages");
                if (!problem.empty())
                    return fail(problem);
            }
            for (const TakeoffRow &row : takeoff_rows)
            {
                if (!row.takeoff.is_hole())
                    continue;
                StoredReference parent;
                parent.text = row.takeoff.parent_uid;
                const std::string problem =
                    missing_reference_problem("BidTakeoffs", child_of(row), parent, takeoff_numbers, "ParentUID", "BidTakeoffs");
                if (!problem.empty())
                    return fail(problem);
            }

            // find_takeoff_parent_page_mismatches: a hole lies on the page of its parent
            std::unordered_map<std::string, std::string> page_by_uid;
            for (const TakeoffRow &row : takeoff_rows)
                page_by_uid[row.takeoff.uid] = row.takeoff.page_uid;
            for (const TakeoffRow &row : takeoff_rows)
            {
                if (!row.takeoff.is_hole())
                    continue;
                auto parent = page_by_uid.find(row.takeoff.parent_uid);
                if (parent != page_by_uid.end() && page_by_uid[row.takeoff.uid] != parent->second)
                    return fail("A takeoff and its parent must belong to the same page.");
            }

            // find_takeoff_parent_cycle_uids over the holes: the smallest uid of the cycles is named
            std::vector<std::pair<std::string, std::string>> parent_of;
            for (const TakeoffRow &row : takeoff_rows)
                if (row.takeoff.is_hole())
                    parent_of.emplace_back(row.takeoff.uid, row.takeoff.parent_uid);
            const std::string cycle_uid = integrity::find_parent_cycle_uid(parent_of);
            if (!cycle_uid.empty())
                return fail("BidTakeoffs.UID=" + cycle_uid + " participates in a ParentUID cycle.");
        }
        {
            // bid_data_reader._parse_bid_condition_folders_for_bid: unique positive UIDs, no ParentUID cycle
            const std::string problem = integrity::uid_list_problem(condition_folder_uid_texts, "BidConditionFolders");
            if (!problem.empty())
                return fail(problem);
            const std::string cycle_uid = ost_domain::find_condition_folder_cycle(project.condition_folders);
            if (!cycle_uid.empty())
                return fail("BidConditionFolders.UID=" + cycle_uid + " participates in a ParentUID cycle.");
        }
        project.takeoffs.reserve(takeoff_rows.size());
        for (TakeoffRow &row : takeoff_rows)
            project.takeoffs.push_back(std::move(row.takeoff));

        // LP-24: the layer list (the bid's own layers merged with the locked template layers), read once the bid has passed its checks
        if (db.table_exists("BidLayers"))
            project.merged_layers = read_layer_list(db, bid_uid);

        {
            // BidPageFolders is an optional table with optional Description and ParentUID columns (hierarchy_reader.py): rows come
            // ORDER BY [Name], [UID], so roots and subfolders keep that order.
            std::vector<PageFolderRow> folder_rows;
            if (db.table_exists("BidPageFolders"))
            {
                const bool has_description = db.column_exists("BidPageFolders", "Description");
                const bool has_parent = db.column_exists("BidPageFolders", "ParentUID");
                ost_db::Statement stmt = db.prepare(
                    std::string("SELECT UID, Name, ") + (has_description ? "Description" : "NULL") + ", " +
                    (has_parent ? "ParentUID" : "NULL") + " FROM BidPageFolders WHERE BidUID = ? ORDER BY Name, UID");
                if (!stmt.valid())
                    return cannot_read("BidPageFolders", stmt);
                stmt.bind_text(1, bid_uid);
                while (stmt.step())
                {
                    PageFolderRow row;
                    row.uid = stmt.column_text(0);
                    row.name = stmt.column_text(1);
                    row.description = stmt.column_text(2);
                    row.parent_uid = stmt.column_text(3);
                    folder_rows.push_back(std::move(row));
                }
                if (stmt.failed())
                    return cannot_read("BidPageFolders", stmt);
            }

            // hierarchy_reader._get_bid_folder_page_structure: unique positive folder UIDs, and a ParentUID cycle refuses the load
            // (require_acyclic_bid_owned_parent_graph): the folders are not silently left out and their pages not hidden (BRW-16).
            {
                std::vector<std::string> folder_uid_texts;
                std::vector<std::pair<std::string, std::string>> parent_of;
                for (const PageFolderRow &row : folder_rows)
                {
                    folder_uid_texts.push_back(row.uid);
                    parent_of.emplace_back(row.uid, row.parent_uid);
                }
                std::string problem = integrity::uid_list_problem(folder_uid_texts, "BidPageFolders");
                if (!problem.empty())
                    return fail(problem);
                const std::string cycle_uid = integrity::find_parent_cycle_uid(parent_of);
                if (!cycle_uid.empty())
                    return fail("BidPageFolders.UID=" + cycle_uid + " participates in a ParentUID cycle.");
            }

            std::unordered_map<std::string, std::size_t> folder_index;
            for (std::size_t i = 0; i < folder_rows.size(); ++i)
                folder_index[folder_rows[i].uid] = i;

            // The pages of each folder in page order; a page whose folder is not one of the bid's folders is loose.
            std::vector<std::vector<std::string>> pages_of(folder_rows.size());
            for (const std::string &page_uid : page_order)
            {
                ost_domain::Page &page = project.pages[page_uid];
                auto found = page.folder_uid.empty() ? folder_index.end() : folder_index.find(page.folder_uid);
                if (found != folder_index.end())
                    pages_of[found->second].push_back(page_uid);
                else
                    page.folder_uid = "";
            }

            build_page_folder_tree(folder_rows, pages_of, project.folders);
        }

        out = std::move(project);
        return true;
    }

    ost_data::ProjectData load_project(const std::string &db_path,
                                       const std::string &bid_uid)
    {
        ost_data::ProjectData project;
        if (!load_project(db_path, bid_uid, project, nullptr))
            return ost_data::ProjectData();
        return project;
    }

}
