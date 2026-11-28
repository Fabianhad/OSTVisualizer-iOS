// Layer: Persistence
// Depends on: Domain (ProjectData, DatabaseHierarchy), OstXmlParser
// Forbidden: UIKit, Foundation, Renderer
// All SQL and DB ↔ domain mapping lives here. Called only by ImportService.
#pragma once
#include "../../Data/ProjectData.hpp"
#include "../../Domain/ConditionChanges.hpp"
#include "../../Domain/DatabaseHierarchy.hpp"
#include "../../Domain/RenderConfig.hpp"
#include "../../Import/OstXmlParser.hpp"
#include <string>
#include <vector>

namespace ost_store
{

    bool create_schema(const std::string &db_path);

    // Outcome details of import_project (optional out parameter).
    struct ImportResult
    {
        // UID the imported bid got in the target database (every imported row is re-numbered, so this is NOT the
        // UID the file carried). Empty when the import failed.
        std::string bid_uid;
        // Why the import failed (the Windows message where it has one, e.g. "Imported master-data identity is
        // ambiguous for CdnTypes.Name: matching UIDs 40, 41."). Empty on success.
        std::string error;
    };

    // Imports one parsed OST file into the database in ONE transaction: master data is reconciled with the existing
    // rows, every bid row gets a new UID above the database's UID floor, the bid gets Settings.NextBidNo as its BidNo
    // and no project folder. Any failure rolls everything back and returns false.
    bool import_project(const std::string &db_path,
                        const ost_import::OstRawData &raw_data,
                        ImportResult *result = nullptr);

    // Reads the project browser hierarchy. A database that cannot be opened, a missing table or column and a read that
    // fails in the middle (busy, I/O, corrupt page) are failures: the result is then an EMPTY hierarchy, never the rows
    // read so far, so a transient error cannot look like an empty database (Windows: read errors propagate, bid_data_reader
    // / hierarchy_reader). Use the overload below to learn why.
    // The lists are in display order: the folders by name (case-insensitive ASCII, then lowercase first, then UID:
    // ost_domain::folder_precedes, BRW-M01), the bids of a folder and the orphans by BidNo ascending (a NULL BidNo first), equal
    // numbers in reader order, i.e. by BidProjectUID, then UID (ost_domain::bid_precedes, BRW-13). A bid whose BidProjectUID is NULL,
    // 0 or no folder is an orphan.
    ost_domain::DatabaseHierarchy load_hierarchy(const std::string &db_path);

    // Same read, reporting the outcome: false when the hierarchy could not be read completely (`hierarchy` is then
    // empty and `error`, when given, names the cause).
    bool load_hierarchy(const std::string &db_path,
                        ost_domain::DatabaseHierarchy &hierarchy,
                        std::string *error = nullptr);

    // Loads one bid. A bid that cannot be read completely (no such bid, a missing required table or column, a read that
    // fails in the middle) is a failure: the result is an EMPTY project (empty() is true), never a project with empty
    // families. Windows get_bid_data/get_condition_family raise instead of publishing an empty family. Use the overload
    // below to learn why.
    ost_data::ProjectData load_project(const std::string &db_path,
                                       const std::string &bid_uid);

    // Same load, reporting the outcome: false when the bid could not be read completely or its data is inconsistent
    // (`project` is then empty and `error`, when given, names the cause; nothing is dropped silently). Windows refuses such a
    // bid the same way (bid_data_reader.get_bid_data, hierarchy_reader), and the messages are the Windows ones:
    //   "<table> contains malformed UID <uid>; authoritative bid-owned UIDs must be positive integers."   (BidLayers, BidPages,
    //   "<table> contains duplicate UID <n>; authoritative bid-owned UIDs must be unique."                 BidConditions, BidTakeoffs,
    //                                                                                                     BidConditionFolders, BidPageFolders)
    //   "BidTakeoffs.UID=<uid> does not satisfy the Takeoff domain contract"   (a non-finite position or rotation, an empty uid)
    //   "BidTakeoffs.UID=<uid> references missing BidConditions.UID=<n> through BidConditionUID."  (also BidPages / BidPageUID and,
    //                                                                          for a hole, BidTakeoffs / ParentUID)
    //   "A takeoff and its parent must belong to the same page."
    //   "BidTakeoffs.UID=<uid> participates in a ParentUID cycle."  (also BidConditionFolders and BidPageFolders)
    // A takeoff with an empty, unparsable or short position is kept (position []). `project.merged_layers` is the layer list
    // (the bid's layers and the locked template layers through ost_domain::merge_layers_for_bid); a list that cannot be read is
    // empty and logged, it does not fail the load.
    bool load_project(const std::string &db_path,
                      const std::string &bid_uid,
                      ost_data::ProjectData &project,
                      std::string *error = nullptr);

    // Moves the bid to the system folder "Deleted Bids" (the BidProjects row with UID 1; project_write_handler._delete_bids,
    // move_bids_to_project). The folder the bid is in is stored as Bids.OrigBidProjectUID (the restore target) only when it is a
    // folder that exists; a bid without a folder (NULL / 0 / a UID that is no folder) is trashed with OrigBidProjectUID left as
    // it is. false, nothing written, when the bid does not exist. A bid already in the trash is left alone.
    bool delete_project(const std::string &db_path,
                        const std::string &bid_uid);

    // Moves a bid out of "Deleted Bids" (project_write_handler.restore_bids): back to its OrigBidProjectUID folder when that
    // folder exists, otherwise to no folder. OrigBidProjectUID is not changed. A bid that does not exist is a success that
    // changes nothing (orphan_bids). false when the uid is not a bid uid or a statement failed.
    bool restore_bid(const std::string &db_path,
                     const std::string &bid_uid);

    bool permanently_delete_project(const std::string &db_path,
                                    const std::string &bid_uid);

    // A new folder (project_operations.create_project): the name is stored as given, duplicate names are allowed (also
    // "Deleted Bids"; the system folder is recognised by its UID only). "" when nothing was written.
    std::string create_folder(const std::string &db_path,
                              const std::string &name);

    // Renames a folder. The system folder "Deleted Bids" (UID 1) is never renamed (the Windows tree disables Rename for it; the
    // iOS store enforces it so no screen can rename it): false, nothing written. Any other folder may take any name, also
    // "Deleted Bids". A uid that is no valid project uid is refused (false); a valid uid without a row is a success that
    // changes nothing, as on Windows.
    bool rename_folder(const std::string &db_path,
                       const std::string &folder_uid,
                       const std::string &new_name);

    // Deletes an EMPTY folder (project_write_handler._delete_projects + project_operations.delete_projects). Refused (false,
    // nothing written, `error` set to the Windows text) when the folder still has bids (ost_domain::CANNOT_DELETE_FOLDER_MESSAGE)
    // or is the system folder "Deleted Bids" (UID 1: "Cannot delete the system project 'Deleted Bids' (UID 1)"). The delete
    // clears Bids.BidProjectUID and Bids.OrigBidProjectUID wherever they name the folder (a trashed bid whose original folder is
    // gone has no restore target) and removes the row, in one transaction. A valid uid without a row is a success.
    bool delete_folder(const std::string &db_path,
                       const std::string &folder_uid,
                       std::string *error = nullptr);

    // The same rule without writing, for the screen that has to choose between the warning and the confirmation: true when
    // delete_folder would delete the folder; false with `error` set to the reason otherwise.
    bool can_delete_folder(const std::string &db_path,
                           const std::string &folder_uid,
                           std::string *error = nullptr);

    bool assign_bid_to_folder(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &folder_uid);

    bool remove_bid_from_folder(const std::string &db_path,
                                const std::string &bid_uid);

    // The saved display configuration. A stored mode outside the Windows vocabulary (Solid, Original, Transparent) loads as
    // the default (Original) and that correction is written back once (a failed correction write is ignored); a missing row
    // is the default and writes nothing.
    ost_domain::RenderConfig load_render_config(const std::string &db_path);
    // Saves the configuration (a mode outside the vocabulary is saved as the default). Both preferences are written in one
    // transaction; false, nothing written, when the database cannot be opened or any write fails: the caller must not
    // announce a change that was not stored.
    bool save_render_config(const std::string &db_path,
                            const ost_domain::RenderConfig &config);

    // Windows ConfigAggregate.update_options / ConfigService.update_app_options: the configuration is validated and compared with the
    // stored one (as load_render_config reads it); only when something differs it is written (one transaction, like
    // save_render_config) and *changed is set. Saving the values that are stored already writes nothing (SET-03), so the caller must
    // not announce a change then. Returns false when a needed write failed (nothing stored, *changed false); true otherwise.
    bool update_render_config(const std::string &db_path,
                              const ost_domain::RenderConfig &config,
                              bool *changed = nullptr);

    bool set_layer_visibility(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &layer_uid,
                              bool visible);

    // Windows update_all_layers_show(db_path, bid_uid, show, layer_uids): Show All / Hide All write the visibility of
    // `layer_uids` in ONE transaction, all or nothing. The bid must exist; every uid (duplicates count once) must be a
    // positive integer naming a layer of that bid or a locked template layer (BidUID = bid OR (IsTemplate AND IsLocked)),
    // otherwise nothing is written (one such uid refuses the whole call). Then only the rows with BidUID = bid are written:
    // the shared template rows (a locked template layer of no bid or of another bid) are skipped, never changed, and a call
    // whose uids are all shared rows succeeds without writing (LP-24 (a); the bid's own locked copy is written). A failed
    // statement rolls everything back. An empty list succeeds without writing.
    bool set_layers_visibility(const std::string &db_path,
                               const std::string &bid_uid,
                               const std::vector<std::string> &layer_uids,
                               bool visible);

    bool save_takeoff_position(const std::string &db_path,
                               const std::string &bid_uid,
                               const std::string &takeoff_uid,
                               const std::vector<double> &position);

    // Windows save_takeoff_positions(db_path, [(uid, position), ...]): every position one gesture changed (a multi-selection
    // drag, a parent with its children) is written in ONE transaction, all or nothing. Like the single write, the bid is
    // derived from the takeoffs (require_single_bid_scope_for_uids): every uid must be a positive integer naming an
    // existing takeoff, all of them must belong to ONE existing bid (a takeoff without a bid, or of a bid without a Bids
    // row, is refused; bid_uid is not compared with it, Windows has no such argument); a uid may repeat (the later position
    // wins). Returns false and writes nothing on any refusal or failed statement. An empty list succeeds without touching
    // the database (Windows skips the scope check for it).
    bool save_takeoff_positions(const std::string &db_path,
                                const std::string &bid_uid,
                                const std::vector<std::pair<std::string, std::vector<double>>> &positions);

    // Writes the typed columns of `takeoff` and, when `takeoff.extras` is not empty, its other BidTakeoffs columns
    // (Windows insert_takeoffs raw_extras). Returns the UID of the new row, "" when nothing was written.
    std::string insert_takeoff(const std::string &db_path,
                               const std::string &bid_uid,
                               const ost_domain::Takeoff &takeoff);

    // Windows insert_takeoffs: the takeoffs of one undo / redo step in ONE transaction, all or nothing; each is inserted like
    // insert_takeoff, in list order, so a takeoff may name an EARLIER takeoff of the same list as its parent. `new_uids` gets the
    // UID of each takeoff. false, nothing written and `new_uids` empty, when any takeoff is refused or a statement fails. An empty
    // list succeeds without touching the database.
    bool insert_takeoffs(const std::string &db_path,
                         const std::string &bid_uid,
                         const std::vector<ost_domain::Takeoff> &takeoffs,
                         std::vector<std::string> &new_uids);

    // Windows get_takeoff_extras (bid_data_reader.py _parse_bid_takeoffs_for_bid): the values of every BidTakeoffs column of
    // the takeoff except the ten typed ones (UID, BidUID, BidConditionUID, BidPageUID, BidAreaUID, Position, Rotation, Curve,
    // ParentUID, IsNegativeQuantity), read from the stored row. Take it BEFORE the delete: insert_takeoff writes it back
    // (Takeoff::extras) so an undo restores the whole row. false, `extras` empty, when the takeoff is not a row of the bid
    // or a statement failed.
    bool read_takeoff_extras(const std::string &db_path,
                             const std::string &bid_uid,
                             const std::string &takeoff_uid,
                             ost_domain::TakeoffExtras &extras);

    bool delete_takeoff(const std::string &db_path,
                        const std::string &takeoff_uid);

    // Windows delete_takeoffs: the whole set (the takeoffs a delete removes together with their descendants) in ONE transaction,
    // all or nothing: every uid must be a positive integer naming a takeoff, all of the SAME existing bid; otherwise nothing is
    // deleted. Each takeoff is deleted like delete_takeoff. An empty list succeeds without touching the database.
    bool delete_takeoffs(const std::string &db_path,
                         const std::vector<std::string> &takeoff_uids);

    bool set_takeoff_negative(const std::string &db_path,
                              const std::string &bid_uid,
                              const std::string &takeoff_uid,
                              bool is_negative);

    // Windows set_takeoffs_negative for one user action: the negative flag of every listed takeoff in ONE transaction, all or
    // nothing. Like save_takeoff_positions every uid must be a positive integer naming a takeoff, all of the SAME existing bid;
    // otherwise nothing is written. A uid may repeat (the later value wins). An empty list succeeds without touching the database.
    bool set_takeoffs_negative(const std::string &db_path,
                               const std::string &bid_uid,
                               const std::vector<std::pair<std::string, bool>> &updates);

    // Windows update_condition(db_path, bid_uid, condition_uid, UpdateConditionDto): writes only the columns of the
    // fields in `changes`, in one transaction together with the reference-number shift a changed RefNo needs.
    // Returns false and writes nothing when the condition is not one of the bid's, when a new layer / folder does not
    // belong to the bid, when a uid is not a number, or when any statement fails. An empty change set is a success
    // without touching the database.
    // A renumber whose shift would carry another condition of the bid past the highest condition number (the numbers
    // from the new one up move up by one, and one of them is already 2147483647) fails before anything is written, the
    // other fields of the same save included (Windows ConditionNumberOverflowError, DLG-FINITE-01): `error`, when given,
    // is then ost_domain::condition_number_overflow_message(new number). Every other failure leaves `error` empty (the
    // caller shows the generic text), and so does a success.
    bool update_condition(const std::string &db_path,
                          const std::string &bid_uid,
                          const std::string &condition_uid,
                          const ost_domain::ConditionChanges &changes,
                          std::string *error = nullptr);

    bool shift_condition_ref_nos(const std::string &db_path,
                                 const std::string &bid_uid,
                                 int from_ref_no,
                                 const std::string &exclude_uid);

    // Windows duplicate_conditions for ONE condition: the copy gets the next free UID, a fresh GUID and RefNo = highest number of the
    // bid + 1 (1 for a bid without numbers), all in one transaction; returns the new uid, "" on any failure.
    // A copy whose number would pass the highest condition number, 2147483647, is refused BEFORE anything is written (no row, no UID;
    // Windows _allocate_condition_identities, ConditionNumberOverflowError, condition_operations.py :56-62 in Windows 49a2d59):
    // `error`, when given, is then ost_domain::refuse_new_condition_numbers(highest, 1), highest being MAX(RefNo) of the
    // bid (NULL numbers ignored, 0 for a bid without numbers, read as 64 bit inside the transaction after the ownership check).
    // A copy whose UID would pass 2147483647, the largest Access Long, is refused the same way (plan 4.2, owner decision N-M1-04:
    // Windows keeps no UID ceiling, identity_allocation.py:5-47; this is a deliberate safe deviation): `error` is then
    // "Cannot add a condition: no identity is left below 2147483647." (integrity::next_uid_within_access_limit). Every other failure
    // leaves `error` empty (the caller shows the generic text), and so does a success. Condition numbers are allocated by
    // insert_condition and duplicate_condition.
    std::string duplicate_condition(const std::string &db_path,
                                    const std::string &bid_uid,
                                    const std::string &condition_uid,
                                    std::string *error = nullptr);

    bool delete_condition(const std::string &db_path,
                          const std::string &bid_uid,
                          const std::string &condition_uid);

}

// One header per M1 feature (plan 4.4 and 6.1), included here once so that parallel streams never edit the same lines. The first is the
// bid lock guard for the UI; the others are empty stubs that the stream named in plan 6.1 fills.
#include "ProjectStore_BidLock.hpp"
#include "ProjectStore_ConditionCreate.hpp"
#include "ProjectStore_ConditionFolders.hpp"
#include "ProjectStore_Layers.hpp"
#include "ProjectStore_PageRename.hpp"
#include "ProjectStore_PageDelete.hpp"
#include "ProjectStore_PageScale.hpp"
