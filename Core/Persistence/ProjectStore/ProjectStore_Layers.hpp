// Layer: Persistence
// Depends on: Domain (ProjectData, LayerEdit), OstDatabase
// Forbidden: UIKit, Foundation, Renderer
// Public declarations of the store functions of the M1 layers feature (plan 5.3): create, rename, delete, reorder, the usage read that guards
// a delete, and the layers-only reload. Included once at the end of ProjectStore.hpp (plan 4.4 and 6.1); stream S2 owns this header.
//
// The write pattern of plan 4.4 for every writer: ONE transaction (begin_transaction ... commit), in this order inside it: parse the uids
// (integrity::parse_uid: "007" is 7, "0", "-5", " 7", "1.0", "abc", "" are refused), the bid exists, the row belongs to the OPEN bid (register
// LY-9: Windows checks "one bid", not the open one), the bid lock (integrity::bid_is_locked on the same connection: a locked bid, and a
// lock that cannot be read, refuse), the name / value rules (ost_domain::check_layer_name, re-run here on the rows read inside the
// transaction so that no screen can bypass them), the 32-bit limits (LY-7), the writes, commit. ANY failed statement = rollback and a
// negative result: a refusal or a failure leaves the database byte-identical. Nothing throws across the Objective-C boundary.
//
// Windows source of truth (49a2d59): ost_visualizer/infrastructure/mdb/components/layer_operations.py insert_layer :78-114, delete_layer
// :153-217, update_layer_name :385-410, swap_layer_sequence :432-465; identity_allocation.py:5-39; bid_owned_identity.py:105-152, :205-235;
// constants.py:122-134 (the 11 reference tables); settings_reader.py:450-482 (get_layer_uids_in_use). The deviations are the register rows
// LY-1 to LY-14 (plan section 8) and are named where they matter below.
#pragma once
#include "../../Domain/Layer.hpp"
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace ost_db
{
    class OstDatabase;
}

namespace ost_store
{

    // Windows insert_layer(db_path, bid_uid, name, after_sequence): a new row of the bid with Name = the STRIPPED name (case and inner spaces
    // kept), Show -1, Sequence = after_sequence + 1, IsTemplate 0, IsLocked 0; every row of THAT bid with Sequence > after_sequence moves up by
    // one first (shared template rows have no bid and never move, a NULL Sequence never moves, other bids never move; a locked own copy moves);
    // UID = integrity::next_uid_within_access_limit(db, "BidLayers", 1, ...), i.e. MAX(UID) + 1 raised above every dangling BidLayerUID of the
    // 11 reference tables. Returns the new UID as decimal text, "" when nothing was written.
    // Refusals before any write: ost_domain::check_layer_name verdicts other than Accept (blank, duplicate by strip().lower() against the bid's
    // listed rows and own layers, "comments" in any spelling, more than 50 UTF-16 units, control characters / line breaks / invalid UTF-8),
    // after_sequence + 1 outside the Access Long range, a later row whose Sequence + 1 would pass 2147483647 (also one beyond 32 bit), a new UID
    // past 2147483647. A missing BidLayers column UID, BidUID or Name (D13) is a refusal too, and so is a bid that has a row whose Sequence is
    // neither NULL nor an integer (hardening beyond Windows: the shift of such a value would not mean what the limit check assumes).
    // `error`, when given, is cleared first and set only for texts the user must see: Duplicate / Reserved: ost_domain::duplicate_layer_message
    // (stripped name); TooLong / ControlCharacter: the message of ost_domain::check_layer_name; a UID past the limit:
    // integrity::no_identity_left_text("layer"); a sequence past the limit: ost_domain::kLayerSequenceLimitText. Every other refusal (blank
    // name, malformed or missing bid, locked bid, failed statement or read) leaves it empty: the caller shows the generic text.
    std::string insert_layer(const std::string &db_path,
                             const std::string &bid_uid,
                             const std::string &name,
                             long long after_sequence,
                             std::string *error = nullptr);

    // Windows update_layer_name(db_path, layer_uid, name), for a row of the open bid only: true when the layer now has the name (a name equal to
    // the stored one, exact and case sensitive after the strip, is a success that writes nothing). Only Name changes; Sequence, Show and the
    // flags stay. The row must be the bid's own with IsTemplate = 0 AND IsLocked = 0 (a NULL flag does not match: fail closed, LY-10); a
    // shared template row, a locked copy and a layer of another bid are refused BEFORE the name rules (a locked copy is refused even with its
    // own name). The name rules are ost_domain::check_layer_name with the layer itself excluded from the duplicate check (a change of letter
    // case alone is written). `error` as for insert_layer.
    bool rename_layer(const std::string &db_path,
                      const std::string &bid_uid,
                      const std::string &layer_uid,
                      const std::string &name,
                      std::string *error = nullptr);

    // Windows delete_layer for each uid, ALL IN ONE transaction and all or nothing (LY-4: Windows commits the layers deleted before a refused
    // one). Every uid (a repeated one counts once, order kept) must be a layer of the open bid with IsTemplate = 0 AND IsLocked = 0, else
    // the whole call is refused and nothing is deleted. An empty list is a success that writes nothing (the bid must still exist and be open
    // for writes: a missing or locked bid refuses it too). For each uid in order: set
    // BidLayerUID to NULL in the 11 reference tables (BidConditions, BidZones, BidComments, BidAnnotationClouds, BidAnnotationOvals,
    // BidAnnotationPolygons, BidAnnotationRects, BidTexts, BidHighlights, BidHotLinks, BidCallOuts; a table or column the database does not
    // have is skipped; NOT scoped to the bid, like Windows), delete the row, then renumber the later rows of the same bid
    // (Sequence = Sequence - 1 WHERE BidUID = ? AND Sequence > the sequence the deleted row had at that moment; no renumber when it had none).
    // The store does NOT refuse an in-use layer (Windows contract): the refusal is the usage read plus the delete plan
    // (read_layer_uids_in_use, ost_domain::plan_layer_delete), which a screen runs first. A failed statement in any step (a clear in any of
    // the 11 tables included: the lenient branch of Windows is dead code, LY-5) rolls everything back.
    bool delete_layers(const std::string &db_path,
                       const std::string &bid_uid,
                       const std::vector<std::string> &layer_uids);

    // Windows swap_layer_sequence(db_path, a, b), for rows of the open bid: the two Sequence values are exchanged. Both rows must belong to the
    // open bid (a shared template row, which has no bid, and a layer of another bid are refused: LY-6, LY-9); the bid's own locked copies are
    // allowed on either side (N-M1-19: moving past them rewrites their Sequence); a == b is refused (also "007" against "7": the uids are
    // canonicalised, LY-13); a NULL Sequence on either row is refused (LY-12); equal Sequences are a success that changes nothing visible.
    // Not filtered by the lock / template flags of the two rows (Windows has no such filter; the screens never offer to move a locked row).
    bool swap_layer_sequence(const std::string &db_path,
                             const std::string &bid_uid,
                             const std::string &uid_a,
                             const std::string &uid_b);

    // Windows get_layer_uids_in_use(file, bid) (settings_reader.py:450-482): the UIDs (decimal text) that any of the 11 reference tables names
    // in BidLayerUID for the bid (the tables have a BidUID column: only that bid's rows count; a table WITHOUT a BidUID column counts its rows
    // for every bid; NULL ignored; a table or column the database lacks is skipped). A condition without takeoffs, a zone, a text, a comment, a
    // hotlink, a highlight, a call-out and an annotation-only layer are all "in use" (the strict rule everywhere, LY-3). Reads only (no write,
    // no lock held after the call). On success `used` is replaced by the set and true is returned. Returns false when it could not be read (a
    // database that cannot be opened or read, a malformed bid uid, a failed statement): the caller MUST refuse the delete (LY-2: Windows returns
    // an empty set there and the delete then clears the references). `used` is not meaningful after a false.
    bool read_layer_uids_in_use(const std::string &db_path,
                                const std::string &bid_uid,
                                std::set<std::string> &used);

    // Re-reads the layer lists of the bid without loading the rest of it: `own_layers` = the bid's own layers by uid with Name, visibility
    // (Show -1 / 1 = visible) and Sequence, exactly what load_project puts in ProjectData::layers, and `merged_layers` = the layer sheet's list
    // (ost_domain::merge_layers_for_bid without the Comments layer), exactly what load_project puts in ProjectData::merged_layers. Both are
    // replaced together and only on success; on false (a database that cannot be opened or read, a malformed bid uid, a failed statement, a
    // list that cannot be read) BOTH are left exactly as they were and `error`, when given, says why. A caller keeps its old state on false.
    bool reload_layers(const std::string &db_path,
                       const std::string &bid_uid,
                       std::unordered_map<std::string, ost_domain::Layer> &own_layers,
                       std::vector<ost_domain::BidLayer> &merged_layers,
                       std::string *error = nullptr);

    // INTERNAL (not for the UI): the two layer reads that load_project (ProjectStore+Load.cpp) and the layer writers / reload_layers
    // (ProjectStore+Layers.cpp) share, so that a writer sees exactly the lists a loaded bid shows. Both work on the caller's connection (a
    // writer calls them inside its transaction) and return false, with a text in `why`, when the rows cannot be read; load_project keeps its
    // own behaviour on top of them (an unreadable merged list is logged and empty, an unreadable own-layer table fails the load).
    namespace layer_reads
    {
        // The bid's own layers by uid (`SELECT UID, Name, Show, Sequence FROM BidLayers WHERE BidUID = ?`: Show -1 / 1 = visible, Sequence
        // as column_int), exactly what load_project puts in ProjectData::layers; `uid_texts` = the stored UIDs in row order (for
        // integrity::uid_list_problem). `out` and `uid_texts` are cleared first. why = "Cannot read BidLayers: <sqlite text>".
        bool read_own_layers(ost_db::OstDatabase &db,
                             const std::string &bid_uid,
                             std::unordered_map<std::string, ost_domain::Layer> &out,
                             std::vector<std::string> &uid_texts,
                             std::string &why);

        // The layer list of the layer sheet (Windows get_merged_bid_layers: the bid's rows plus the locked template rows, merged, without the
        // Comments layer), exactly what load_project puts in ProjectData::merged_layers. false when the query fails or the rows of the query
        // break the Windows UID contract (integrity::uid_list_problem, the text is `why`); `out` is cleared first.
        bool read_merged_layers(ost_db::OstDatabase &db,
                                const std::string &bid_uid,
                                std::vector<ost_domain::BidLayer> &out,
                                std::string &why);
    }

}
