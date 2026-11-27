// ProjectStore component — takeoff CRUD and layer visibility.
#include "../ProjectStore_Internal.hpp"

namespace ost_store
{

    namespace
    {
        // Windows TakeoffOperationsMixin._TAKEOFF_TYPED_COLUMNS (= bid_data_reader._TAKEOFF_TYPED_COLUMNS): the columns the
        // typed fields of a takeoff write. Every other column of the row is an "extra".
        bool is_typed_takeoff_column(const std::string &column)
        {
            static const std::set<std::string> typed = {"UID",      "BidUID",   "BidConditionUID", "BidPageUID",
                                                        "BidAreaUID", "Position", "Rotation",        "Curve",
                                                        "ParentUID", "IsNegativeQuantity"};
            return typed.count(column) != 0;
        }

        // Windows TakeoffOperationsMixin._TAKEOFF_BID_SCOPED_FKS: the references that only mean something inside the bid they
        // were captured in.
        bool is_bid_scoped_takeoff_reference(const std::string &column)
        {
            static const std::set<std::string> scoped = {"BidZoneUID",        "BidTypAreaUID", "TypGroupTakeoffUID",
                                                         "TypPageTakeoffUID", "TypGroupUID",   "TypGroupMarkerUID"};
            return scoped.count(column) != 0;
        }

        // The columns of BidTakeoffs as the database spells them (declaration order); false when the lookup failed.
        bool takeoff_table_columns(ost_db::OstDatabase &db, std::vector<std::string> &columns)
        {
            ost_db::Statement info = db.prepare("SELECT name FROM pragma_table_info('BidTakeoffs')");
            if (!info.valid())
                return false;
            while (info.step())
                columns.push_back(info.column_text(0));
            return !info.failed();
        }
    }

    bool set_layer_visibility(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &layer_uid,
                              bool visible)
    {
        // Windows update_layer_show: the layer must exist and belong to ONE existing bid (require_single_bid_scope_for_uids): a
        // missing layer and a template layer of no bid are refused and nothing is written. The bid that is open is not compared
        // with the layer's bid (Windows does not either), so bid_uid is not used.
        (void)bid_uid;
        long long layer = 0;
        if (!integrity::parse_uid(layer_uid, layer))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;
        long long owner = 0;
        if (!integrity::owner_bid_of(db, "BidLayers", layer, owner))
        {
            db.rollback();
            return false;
        }

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidLayers SET Show = ? WHERE UID = ?");
        if (!stmt.valid())
        {
            db.rollback();
            return false;
        }

        stmt.bind_int(1, visible ? -1 : 0);
        stmt.bind_int64(2, layer);
        if (!stmt.run())
        {
            db.rollback();
            return false;
        }
        return db.commit();
    }

    bool set_layers_visibility(const std::string &db_path,
                               const std::string &bid_uid,
                               const std::vector<std::string> &layer_uids,
                               bool visible)
    {
        // Windows update_all_layers_show(db, bid, show, layer_uids), one connection: the bid must exist
        // (require_existing_unique_bid_owned_uid_matches(Bids)); the uids are made unique in order of appearance and an empty
        // list is a success that writes nothing; EVERY uid must name a layer of the bid or, when the table has both flag
        // columns, a locked template layer (BidUID = ? OR (IsTemplate <> 0 AND IsLocked <> 0)) - otherwise nothing is
        // written; then the uids that are not rows of the bid (BidUID = ?: the shared template rows, a locked template layer of
        // no bid or of another bid) are dropped and a call with none left is a success that writes nothing (LP-24 (a)); the
        // rest is one UPDATE ... WHERE UID = ? AND BidUID = ? per layer, all in the one transaction (any failed statement rolls
        // everything back).
        long long bid = 0;
        if (!integrity::parse_uid(bid_uid, bid))
            return false;
        std::vector<long long> uids;
        std::set<long long> seen;
        for (const std::string &text : layer_uids)
        {
            long long uid = 0;
            if (!integrity::parse_uid(text, uid))
                return false;
            if (seen.insert(uid).second)
                uids.push_back(uid);
        }

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        if (!db.begin_transaction())
            return false;
        auto fail = [&db]() -> bool
        {
            db.rollback();
            return false;
        };
        if (!integrity::bid_exists(db, bid))
            return fail();
        if (uids.empty())
        {
            db.rollback(); // nothing to write
            return true;
        }

        const bool template_filter = db.column_exists("BidLayers", "IsTemplate") && db.column_exists("BidLayers", "IsLocked");
        ost_db::Statement eligible = db.prepare(std::string("SELECT COUNT(*) FROM [BidLayers] WHERE [UID] = ? AND ([BidUID] = ?") +
                                                (template_filter ? " OR ([IsTemplate] <> 0 AND [IsLocked] <> 0)" : "") + ")");
        if (!eligible.valid())
            return fail();
        for (long long uid : uids)
        {
            eligible.reset();
            eligible.bind_int64(1, uid);
            eligible.bind_int64(2, bid);
            if (!eligible.step() || eligible.column_int(0) != 1)
                return fail();
        }

        // own_uids = the uids whose row has BidUID = bid; a shared template row is eligible above but is never written
        ost_db::Statement owned = db.prepare("SELECT COUNT(*) FROM [BidLayers] WHERE [UID] = ? AND [BidUID] = ?");
        if (!owned.valid())
            return fail();
        std::vector<long long> own_uids;
        for (long long uid : uids)
        {
            owned.reset();
            owned.bind_int64(1, uid);
            owned.bind_int64(2, bid);
            if (!owned.step())
                return fail();
            if (owned.column_int(0) == 1)
                own_uids.push_back(uid);
        }
        if (own_uids.empty())
            return db.commit(); // only shared rows: nothing to write

        ost_db::Statement update = db.prepare("UPDATE [BidLayers] SET [Show] = ? WHERE [UID] = ? AND [BidUID] = ?");
        if (!update.valid())
            return fail();
        for (long long uid : own_uids)
        {
            update.reset();
            update.bind_int(1, visible ? -1 : 0);
            update.bind_int64(2, uid);
            update.bind_int64(3, bid);
            if (!update.run())
                return fail();
        }
        return db.commit();
    }

    bool save_takeoff_position(const std::string &db_path,
                               const std::string &bid_uid,
                               const std::string &takeoff_uid,
                               const std::vector<double> &position)
    {
        // Windows save_takeoff_positions: the takeoff must exist and belong to one existing bid (require_single_bid_scope_for_uids:
        // the bid is derived from the takeoff, there is no bid argument); otherwise nothing is written. bid_uid is not compared
        // with it (Windows does not either).
        (void)bid_uid;
        long long takeoff = 0;
        if (!integrity::parse_uid(takeoff_uid, takeoff))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;
        long long owner = 0;
        if (!integrity::owner_bid_of(db, "BidTakeoffs", takeoff, owner))
        {
            db.rollback();
            return false;
        }

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidTakeoffs SET Position = ? WHERE UID = ?");
        if (!stmt.valid())
        {
            db.rollback();
            return false;
        }

        stmt.bind_text(1, serialize_position(position));
        stmt.bind_int64(2, takeoff);
        if (!stmt.run())
        {
            db.rollback();
            return false;
        }
        return db.commit();
    }

    bool save_takeoff_positions(const std::string &db_path,
                                const std::string &bid_uid,
                                const std::vector<std::pair<std::string, std::vector<double>>> &positions)
    {
        // Windows save_takeoff_positions: nothing to save is a success that does not even open the database (the scope check
        // only runs `if positions`); otherwise ONE transaction: require_single_bid_scope_for_uids over every uid (valid uids,
        // all rows exist, all belong to the same existing bid; bid_uid is not compared with it), then one UPDATE per row. Any
        // refusal or failed statement rolls everything back.
        (void)bid_uid;
        if (positions.empty())
            return true;
        std::vector<long long> uids;
        uids.reserve(positions.size());
        for (const auto &entry : positions)
        {
            long long uid = 0;
            if (!integrity::parse_uid(entry.first, uid))
                return false;
            uids.push_back(uid);
        }

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;
        long long scope_bid = 0;
        for (size_t i = 0; i < uids.size(); ++i)
        {
            long long owner = 0;
            if (!integrity::owner_bid_of(db, "BidTakeoffs", uids[i], owner) || (i > 0 && owner != scope_bid))
            {
                db.rollback();
                return false;
            }
            scope_bid = owner;
        }

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidTakeoffs SET Position = ? WHERE UID = ?");
        if (!stmt.valid())
        {
            db.rollback();
            return false;
        }
        for (size_t i = 0; i < positions.size(); ++i)
        {
            stmt.reset();
            stmt.bind_text(1, serialize_position(positions[i].second));
            stmt.bind_int64(2, uids[i]);
            if (!stmt.run())
            {
                db.rollback();
                return false;
            }
        }
        return db.commit();
    }

    // insert_takeoff for ONE takeoff inside the caller's transaction: the new UID, "" when it is refused or a statement fails (the
    // caller rolls the transaction back). A parent may be a takeoff inserted earlier in the same transaction.
    static std::string insert_takeoff_in(ost_db::OstDatabase &db,
                                         const std::string &bid_uid,
                                         const ost_domain::Takeoff &takeoff)
    {
        // Windows insert_takeoffs: the bid must exist, the condition, the page, an assigned area and the parent must be rows of THAT
        // bid, and a parent must lie on the takeoff's page (Windows: require_existing_bid_scoped_uid_matches,
        // require_takeoff_parent_pages). A failed check writes nothing and returns "".
        long long bid = 0;
        long long condition = 0;
        long long page = 0;
        if (!integrity::parse_uid(bid_uid, bid) || !integrity::parse_uid(takeoff.condition_uid, condition) ||
            !integrity::parse_uid(takeoff.page_uid, page))
            return {};

        // The area is unassigned when it is empty or "0" (area.normalize_area_uid: str(area or "").strip() or "0"); the parent is
        // none when it is empty or "0". Anything else must be a positive integer.
        std::string area_text = takeoff.area_uid;
        {
            const std::string blanks = " \t\n\r\f\v";
            const std::size_t first = area_text.find_first_not_of(blanks);
            area_text = first == std::string::npos ? std::string() : area_text.substr(first, area_text.find_last_not_of(blanks) - first + 1);
        }
        long long area = 0;
        const bool has_area = !(area_text.empty() || area_text == "0");
        if (has_area && !integrity::parse_uid(takeoff.area_uid, area)) // the stored text, not the trimmed one: " 60" is malformed
            return {};
        long long parent = 0;
        const bool has_parent = !(takeoff.parent_uid.empty() || takeoff.parent_uid == "0");
        if (has_parent && !integrity::parse_uid(takeoff.parent_uid, parent))
            return {};
        // An explicit UID is the undo path re-inserting a deleted takeoff with its old UID; it must be a positive integer.
        long long explicit_uid = 0;
        if (!takeoff.uid.empty() && !integrity::parse_uid(takeoff.uid, explicit_uid))
            return {};

        auto fail = []() -> std::string { return {}; };

        if (!integrity::bid_exists(db, bid) || !integrity::row_belongs_to_bid(db, "BidConditions", condition, bid) ||
            !integrity::row_belongs_to_bid(db, "BidPages", page, bid))
            return fail();
        if (has_area && !integrity::row_belongs_to_bid(db, "BidAreas", area, bid))
            return fail();
        if (has_parent)
        {
            if (!integrity::row_belongs_to_bid(db, "BidTakeoffs", parent, bid))
                return fail();
            ost_db::Statement parent_page = db.prepare("SELECT [BidPageUID] FROM [BidTakeoffs] WHERE [UID] = ?");
            if (!parent_page.valid())
                return fail();
            parent_page.bind_int64(1, parent);
            if (parent_page.step_result() != ost_db::StepResult::Row ||
                parent_page.column_type(0) == ost_db::Statement::TypeNull || parent_page.column_int64(0) != page)
                return fail(); // "A takeoff and its parent must belong to the same page."
        }

        // The UID: the explicit one, else MAX(UID) + 1 raised above every column that references a takeoff (Windows
        // _next_uid_preserving_references: BidPercents, the annotation endpoints, the self references)
        long long uid = explicit_uid;
        if (uid == 0)
        {
            uid = integrity::next_uid_preserving_references(db, "BidTakeoffs");
            if (uid <= 0)
                return fail();
        }

        // The other columns of a row that is inserted again (Windows insert_takeoffs raw_extras, takeoff_operations.py:534-550):
        // every extra that is not a typed column and that the table declares is written as it was captured, except that the
        // bid-scoped references become NULL when the extras come from ANOTHER bid, and the GUID is regenerated.
        std::vector<std::pair<std::string, ost_domain::TakeoffExtraValue>> extra_values;
        std::vector<std::string> table_columns;
        if (!takeoff_table_columns(db, table_columns))
            return fail();
        const std::set<std::string> declared(table_columns.begin(), table_columns.end());
        if (!takeoff.extras.empty())
        {
            const bool cross_bid = takeoff.extras.source_bid_uid.has_value() && *takeoff.extras.source_bid_uid != bid_uid;
            for (const auto &extra : takeoff.extras.columns)
            {
                const std::string &column = extra.first;
                // (a name that cannot be quoted as [name] cannot be a declared column either)
                if (is_typed_takeoff_column(column) || !declared.count(column) || column.find(']') != std::string::npos)
                    continue;
                if (cross_bid && is_bid_scoped_takeoff_reference(column))
                    extra_values.emplace_back(column, ost_domain::TakeoffExtraValue::make_null());
                else if (column == "GUID")
                    extra_values.emplace_back(column, ost_domain::TakeoffExtraValue::make_text(new_ost_guid()));
                else
                    extra_values.push_back(extra);
            }
        }

        // The Windows field defaults of BidTakeoffs (reference_schema_metadata.FIELD_DEFAULTS: Count, GridOffsetX, GridOffsetY,
        // GridRotation, FontColor and NameFontColor are 0; IsNegativeQuantity, the sixth, is a typed column written below):
        // Access fills them in for a column the INSERT does not name. A database created before its schema carried the DEFAULT
        // clauses (DBP-33) does not, so they are written here, for the columns the table declares and the captured extras do
        // not already carry (an extra wins, even a NULL one: the undo of a delete restores the row as it was).
        struct TakeoffDefault
        {
            const char *column;
            bool real; // the column is REAL (Access Double), else INTEGER
        };
        static const TakeoffDefault defaults[] = {{"Count", true},      {"GridOffsetX", true}, {"GridOffsetY", true},
                                                  {"GridRotation", true}, {"FontColor", false},  {"NameFontColor", false}};
        for (const TakeoffDefault &field : defaults)
        {
            if (!declared.count(field.column))
                continue;
            bool captured = false;
            for (const auto &extra : extra_values)
                captured = captured || extra.first == field.column;
            if (!captured)
                extra_values.emplace_back(field.column, field.real ? ost_domain::TakeoffExtraValue::make_real(0.0)
                                                                   : ost_domain::TakeoffExtraValue::make_integer(0));
        }

        std::string columns_sql = "UID, BidUID, BidConditionUID, BidPageUID, BidAreaUID, "
                                  "Position, Rotation, Curve, ParentUID, IsNegativeQuantity";
        std::string values_sql = "?, ?, ?, ?, ?, ?, ?, ?, ?, ?";
        for (const auto &extra : extra_values)
        {
            columns_sql += ", [" + extra.first + "]";
            values_sql += ", ?";
        }
        ost_db::Statement stmt = db.prepare("INSERT INTO BidTakeoffs (" + columns_sql + ") VALUES (" + values_sql + ")");
        if (!stmt.valid())
            return fail();

        // BidAreaUID is NULL when unassigned and ParentUID is 0 for a root takeoff (takeoff_operations.py:512-533).
        stmt.bind_int64(1, uid);
        stmt.bind_int64(2, bid);
        stmt.bind_int64(3, condition);
        stmt.bind_int64(4, page);
        if (has_area)
            stmt.bind_int64(5, area);
        else
            stmt.bind_null(5);
        stmt.bind_text(6, serialize_position(takeoff.position));
        stmt.bind_double(7, takeoff.rotation);
        stmt.bind_int(8, takeoff.curve);
        stmt.bind_int64(9, has_parent ? parent : 0);
        stmt.bind_int(10, takeoff.is_negative ? 1 : 0);
        int bind_index = 11;
        for (const auto &extra : extra_values)
        {
            const ost_domain::TakeoffExtraValue &value = extra.second;
            switch (value.kind)
            {
            case ost_domain::TakeoffExtraValue::Kind::Null:
                stmt.bind_null(bind_index);
                break;
            case ost_domain::TakeoffExtraValue::Kind::Integer:
                stmt.bind_int64(bind_index, value.integer);
                break;
            case ost_domain::TakeoffExtraValue::Kind::Real:
                stmt.bind_double(bind_index, value.real);
                break;
            case ost_domain::TakeoffExtraValue::Kind::Text:
                stmt.bind_text(bind_index, value.text);
                break;
            }
            ++bind_index;
        }
        if (!stmt.run())
            return fail();
        return std::to_string(uid);
    }

    std::string insert_takeoff(const std::string &db_path,
                               const std::string &bid_uid,
                               const ost_domain::Takeoff &takeoff)
    {
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return {};
        if (!db.begin_transaction())
            return {};
        const std::string uid = insert_takeoff_in(db, bid_uid, takeoff);
        if (uid.empty())
        {
            db.rollback();
            return {};
        }
        if (!db.commit())
            return {};
        return uid;
    }

    bool insert_takeoffs(const std::string &db_path,
                         const std::string &bid_uid,
                         const std::vector<ost_domain::Takeoff> &takeoffs,
                         std::vector<std::string> &new_uids)
    {
        // Windows insert_takeoffs: all takeoffs or none, in ONE transaction; nothing to insert is a success that does not open the
        // database. A refused takeoff (unknown condition / page / area / parent, a parent on another page, a duplicate explicit
        // uid) or a failed statement rolls the whole batch back.
        new_uids.clear();
        if (takeoffs.empty())
            return true;
        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        if (!db.begin_transaction())
            return false;
        std::vector<std::string> inserted;
        inserted.reserve(takeoffs.size());
        for (const ost_domain::Takeoff &takeoff : takeoffs)
        {
            const std::string uid = insert_takeoff_in(db, bid_uid, takeoff);
            if (uid.empty())
            {
                db.rollback();
                return false;
            }
            inserted.push_back(uid);
        }
        if (!db.commit())
            return false;
        new_uids = std::move(inserted);
        return true;
    }

    bool read_takeoff_extras(const std::string &db_path,
                             const std::string &bid_uid,
                             const std::string &takeoff_uid,
                             ost_domain::TakeoffExtras &extras)
    {
        // Windows bid_data_reader._parse_bid_takeoffs_for_bid: takeoff_extras[uid] = {column: value for every column of the
        // row that is not one of the typed columns}. The takeoff must be a row of the bid.
        extras = ost_domain::TakeoffExtras();
        long long bid = 0;
        long long takeoff = 0;
        if (!integrity::parse_uid(bid_uid, bid) || !integrity::parse_uid(takeoff_uid, takeoff))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        if (!integrity::row_belongs_to_bid(db, "BidTakeoffs", takeoff, bid))
            return false;

        std::vector<std::string> table_columns;
        if (!takeoff_table_columns(db, table_columns))
            return false;
        std::vector<std::string> columns;
        for (const std::string &column : table_columns)
        {
            // A name that cannot be quoted as [name] is not a column this schema declares; skip it.
            if (!is_typed_takeoff_column(column) && column.find(']') == std::string::npos)
                columns.push_back(column);
        }
        if (columns.empty())
            return true;

        std::string sql = "SELECT ";
        for (std::size_t i = 0; i < columns.size(); ++i)
            sql += (i ? ", [" : "[") + columns[i] + "]";
        sql += " FROM [BidTakeoffs] WHERE [UID] = ?";
        ost_db::Statement stmt = db.prepare(sql);
        if (!stmt.valid())
            return false;
        stmt.bind_int64(1, takeoff);
        if (stmt.step_result() != ost_db::StepResult::Row)
            return false;

        ost_domain::TakeoffExtras snapshot;
        for (std::size_t i = 0; i < columns.size(); ++i)
        {
            const int index = static_cast<int>(i);
            switch (stmt.column_type(index))
            {
            case ost_db::Statement::TypeInteger:
                snapshot.columns[columns[i]] = ost_domain::TakeoffExtraValue::make_integer(stmt.column_int64(index));
                break;
            case ost_db::Statement::TypeFloat:
                snapshot.columns[columns[i]] = ost_domain::TakeoffExtraValue::make_real(stmt.column_double(index));
                break;
            case ost_db::Statement::TypeText:
                snapshot.columns[columns[i]] = ost_domain::TakeoffExtraValue::make_text(stmt.column_text(index));
                break;
            case ost_db::Statement::TypeNull:
                snapshot.columns[columns[i]] = ost_domain::TakeoffExtraValue::make_null();
                break;
            case ost_db::Statement::TypeBlob:
                // No BidTakeoffs column of the iOS schema holds a blob (Position is text) and the statement layer cannot
                // bind one back; such a column is not captured and comes back as NULL.
                break;
            }
        }
        extras = std::move(snapshot);
        return true;
    }

    // The deletion of ONE takeoff (Windows _run_delete_takeoffs) inside the caller's transaction; false when it is refused or a
    // statement fails (the caller rolls the transaction back).
    static bool delete_takeoff_in(ost_db::OstDatabase &db, long long takeoff)
    {
        // The takeoff must exist and belong to one existing bid (the bid is derived from the takeoff, there is no bid argument).
        // The annotations that start OR end at it and its percents are deleted, every surviving takeoff that referenced it
        // (ParentUID and the three typical-group references) gets NULL - holes stay as roots, they are not deleted with their
        // parent (the plan view passes the holes it wants deleted explicitly) - then the takeoff goes. A cleanup step is skipped
        // when the schema lacks its table or column.
        auto fail = []() -> bool { return false; };
        // DELETE / UPDATE with the takeoff uid as the only parameter.
        auto run_with_uid = [&db, takeoff](const std::string &sql) -> bool
        {
            ost_db::Statement stmt = db.prepare(sql);
            if (!stmt.valid())
                return false;
            stmt.bind_int64(1, takeoff);
            return stmt.run();
        };

        long long owner = 0;
        if (!integrity::owner_bid_of(db, "BidTakeoffs", takeoff, owner))
            return fail();

        const char *child_tables[] = {"BidDimensions", "BidALines", "BidArrows"};
        const char *reference_columns[] = {"BidTakeoffFromUID", "BidTakeoffToUID"};
        for (const char *table : child_tables)
        {
            if (!db.table_exists(table))
                continue;
            for (const char *column : reference_columns)
            {
                if (!db.column_exists(table, column))
                    continue;
                if (!run_with_uid("DELETE FROM [" + std::string(table) + "] WHERE [" + column + "] = ?"))
                    return fail();
            }
        }

        if (db.table_exists("BidPercents") && db.column_exists("BidPercents", "BidTakeoffUID"))
        {
            if (!run_with_uid("DELETE FROM [BidPercents] WHERE [BidTakeoffUID] = ?"))
                return fail();
        }

        const char *self_references[] = {"ParentUID", "TypGroupTakeoffUID", "TypPageTakeoffUID", "TypGroupMarkerUID"};
        for (const char *column : self_references)
        {
            if (!db.column_exists("BidTakeoffs", column))
                continue;
            if (!run_with_uid("UPDATE [BidTakeoffs] SET [" + std::string(column) + "] = NULL WHERE [" + column + "] = ?"))
                return fail();
        }

        if (!run_with_uid("DELETE FROM [BidTakeoffs] WHERE [UID] = ?"))
            return fail();

        return true;
    }

    bool delete_takeoff(const std::string &db_path,
                        const std::string &takeoff_uid)
    {
        long long takeoff = 0;
        if (!integrity::parse_uid(takeoff_uid, takeoff))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;
        if (!delete_takeoff_in(db, takeoff))
        {
            db.rollback();
            return false;
        }
        return db.commit();
    }

    bool delete_takeoffs(const std::string &db_path,
                         const std::vector<std::string> &takeoff_uids)
    {
        // Windows delete_takeoffs: every uid must be a positive integer (else nothing happens and the result is False); nothing
        // to delete is a success that does not open the database; otherwise ONE transaction: require_single_bid_scope_for_uids
        // (every takeoff exists and they all belong to the SAME existing bid), then the cleanup and the delete of the whole set.
        // Any refusal or failed statement rolls everything back. A uid listed twice is deleted once.
        std::vector<long long> uids;
        std::set<long long> seen;
        for (const std::string &text : takeoff_uids)
        {
            long long uid = 0;
            if (!integrity::parse_uid(text, uid))
                return false;
            if (seen.insert(uid).second)
                uids.push_back(uid);
        }
        if (uids.empty())
            return true;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        if (!db.begin_transaction())
            return false;
        long long scope_bid = 0;
        for (size_t i = 0; i < uids.size(); ++i)
        {
            long long owner = 0;
            if (!integrity::owner_bid_of(db, "BidTakeoffs", uids[i], owner) || (i > 0 && owner != scope_bid))
            {
                db.rollback();
                return false;
            }
            scope_bid = owner;
        }
        for (long long uid : uids)
        {
            if (!delete_takeoff_in(db, uid))
            {
                db.rollback();
                return false;
            }
        }
        return db.commit();
    }

    bool set_takeoff_negative(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &takeoff_uid,
                              bool is_negative)
    {
        // Windows set_takeoffs_negative: the takeoff must exist and belong to one existing bid (require_single_bid_scope_for_uids:
        // the bid is derived from the takeoff, there is no bid argument); otherwise nothing is written. bid_uid is not compared
        // with it (Windows does not either).
        (void)bid_uid;
        long long takeoff = 0;
        if (!integrity::parse_uid(takeoff_uid, takeoff))
            return false;

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;

        if (!db.begin_transaction())
            return false;
        long long owner = 0;
        if (!integrity::owner_bid_of(db, "BidTakeoffs", takeoff, owner))
        {
            db.rollback();
            return false;
        }

        ost_db::Statement stmt = db.prepare(
            "UPDATE BidTakeoffs SET IsNegativeQuantity = ? WHERE UID = ?");
        if (!stmt.valid())
        {
            db.rollback();
            return false;
        }

        stmt.bind_int(1, is_negative ? 1 : 0);
        stmt.bind_int64(2, takeoff);
        if (!stmt.run())
        {
            db.rollback();
            return false;
        }
        return db.commit();
    }

    bool set_takeoffs_negative(const std::string &db_path,
                               const std::string &bid_uid,
                               const std::vector<std::pair<std::string, bool>> &updates)
    {
        // Windows set_takeoffs_negative / _run_selected_takeoffs_value_update: nothing to change is a success that does not open the
        // database; otherwise ONE transaction: every uid a positive integer, require_single_bid_scope_for_uids (all exist, one
        // bid; bid_uid is not compared with it, Windows has no such argument), then one UPDATE per takeoff. Any refusal or failed
        // statement rolls everything back.
        (void)bid_uid;
        if (updates.empty())
            return true;
        std::vector<long long> uids;
        uids.reserve(updates.size());
        for (const auto &update : updates)
        {
            long long uid = 0;
            if (!integrity::parse_uid(update.first, uid))
                return false;
            uids.push_back(uid);
        }

        ost_db::OstDatabase db;
        if (!db.open(db_path))
            return false;
        if (!db.begin_transaction())
            return false;
        long long scope_bid = 0;
        for (size_t i = 0; i < uids.size(); ++i)
        {
            long long owner = 0;
            if (!integrity::owner_bid_of(db, "BidTakeoffs", uids[i], owner) || (i > 0 && owner != scope_bid))
            {
                db.rollback();
                return false;
            }
            scope_bid = owner;
        }

        ost_db::Statement stmt = db.prepare("UPDATE BidTakeoffs SET IsNegativeQuantity = ? WHERE UID = ?");
        if (!stmt.valid())
        {
            db.rollback();
            return false;
        }
        for (size_t i = 0; i < updates.size(); ++i)
        {
            stmt.reset();
            stmt.bind_int(1, updates[i].second ? 1 : 0);
            stmt.bind_int64(2, uids[i]);
            if (!stmt.run())
            {
                db.rollback();
                return false;
            }
        }
        return db.commit();
    }

}
