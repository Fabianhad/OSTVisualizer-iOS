// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// EXP-21 (owner decision 1, 2026-11-27): every alert of an import, a load, an export and a folder delete uses the CURRENT Windows wording
// and shows ONE user sentence. The exception, parser, SQL and writer text is logged (NSLog) and never shown, as in the Windows
// presentation/utils/messagebox.py:130-170 (show_error / show_warning log the shown text plus the exception, the dialog shows the message
// only). The screens and services (Objective-C++) only call these functions and log the raw diagnostic.
//
// Windows sources (HEAD d1c430a):
//   ost_visualizer/presentation/texts.py                   IMPORT_HANDLER_* (the local import), EXPORT_HANDLER_TITLE_EXPORT_ERROR
//   ost_visualizer/presentation/handlers/import_handler.py:218-246   the local import: EVERY failure of an OST / OSP import is the one
//                                                          "damaged" sentence, an exception is the "unexpected" sentence
//   ost_visualizer/shared/texts.py                         IMPORT_PROJECT_FILES_FROM_ARGS_USE_CASE_* (open-in), EXPORT_SERVICE_*,
//                                                          FILE_ERROR_REASONS_*, FILE_PROJECT_REPOSITORY_*, PROJECT_OPERATIONS_SERVICE_*
//   ost_visualizer/application/services/export_service.py:157-189   the three strategies OBJ, DXF and HTML
//   ost_visualizer/domain/services/file_error_reasons.py   known-cause sentences
#pragma once
#include "BrowserRules.hpp"
#include <string>

namespace ost_domain
{
    // ---- Import -------------------------------------------------------------------------------------------------------------

    // presentation/texts.py:922 IMPORT_HANDLER_TITLE_IMPORT_ERROR (also main_window MAIN_WINDOW_TITLE_IMPORT_ERROR, the open-in alert).
    constexpr const char *IMPORT_ERROR_TITLE = "Import Error";

    enum class ImportFormat
    {
        Ost,
        Osp
    };

    // Where the alert is shown. Picker: the Windows local import (import_handler.py). OpenIn: the file was handed to the app, the Windows
    // "args" use case: one line "<file name>: <sentence>" (main_window.py:1198) with the format-less sentences of shared/texts.py:863-885.
    enum class ImportContext
    {
        Picker,
        OpenIn
    };

    // The value of each kind is stable: ImportService uses it as the code of its NSError.
    enum class ImportFailureKind
    {
        Damaged = 1,         // the file cannot be parsed or staged (everything the Windows importer swallows and reports as False)
        NoBid = 2,           // "No Bid element found in OST file" (Windows: a sentence only the SQL import shows; for the log here)
        TakeoffGraphInvalid = 3, // duplicate takeoff UID, ParentUID cycle, parent on another page (the same)
        InvalidReferences = 4,   // "The imported project contains invalid references." (the same)
        FileNotFound = 5,    // open-in only: the file is not there (use case: "File does not exist.")
        Unexpected = 6       // a genuinely unexpected condition (a C++ exception, no error at all); never returned by classify_import_failure
    };

    // Maps the raw diagnostic of Core (OstXmlParser, RawBidIntegrity, OspPackage: the constants of Import/ImportDiagnostics.hpp) to a kind.
    // Anything it does not know is Damaged, as the Windows local import swallows EVERY exception inside import_ost / import_osp
    // (ost_importer.py:94-107) and shows the "damaged" sentence: that includes a failed store, schema or temporary folder. Unexpected is
    // never returned here. The raw text is for the log.
    ImportFailureKind classify_import_failure(const std::string &raw);

    // The Import Error alert. Picker: every kind but Unexpected is "The <OST|OSP> file is damaged or uses an unsupported format, so it
    // cannot be imported."; Unexpected is "An unexpected error stopped the import of the <OST|OSP> file. Please check that the file is
    // valid and try again; if the problem continues, contact support." OpenIn: "<file_name>: " followed by "The file is damaged or uses an
    // unsupported format, so it cannot be imported." / "An unexpected error stopped the import. Please try again; if the problem
    // continues, contact support." / "File does not exist." (FileNotFound). `file_name` is the NAME, not the sandbox path; with an empty
    // name there is no prefix. The per-case SQL-only sentences (no bid, invalid takeoffs, invalid references) are not used.
    DialogText import_failure_text(ImportFailureKind kind, ImportFormat format, ImportContext context, const std::string &file_name);

    // ---- Load ---------------------------------------------------------------------------------------------------------------

    enum class LoadTarget
    {
        Database, // the project list (the Windows database file)
        Bid       // one bid
    };

    // The reason of the "Failed to load <name>:\n<reason>" frame (load_failure_text keeps the frame). Database: "The file could not be read. It
    // may be damaged or may not be an OST database." (shared/texts.py:1238-1240). Bid: "An unexpected error stopped opening the bid. Please try
    // again; if the problem continues, contact support." (shared/texts.py:316-318). iOS has no errno at the load layer, so no known cause
    // can be chosen; the store text is logged.
    std::string load_failure_reason(LoadTarget target);

    // ---- Folder delete ------------------------------------------------------------------------------------------------------

    // The alert of a folder delete that did not happen. `reason` is the text the store reports. Exactly CANNOT_DELETE_FOLDER_MESSAGE (the
    // folder still has bids) is CANNOT_DELETE_FOLDER_TITLE with that message (project_write_handler: "Cannot Delete Project"). Any other
    // reason (a failed read, a failed statement, the system folder, none) is the Windows "Delete Error" / "Failed to delete projects.
    // <db locked hint>" (browser_failure_text(DeleteFolders)); the reason is for the log.
    DialogText folder_delete_alert(const std::string &reason);

    // ---- Known causes of a file failure (file_error_reasons.py) -------------------------------------------------------------

    // The causes iOS can name: it has no sharing locks and no network paths in its sandbox, so Windows "open in another program" and
    // "network location is not available" have no case here. Nothing wires this vocabulary today: the OBJ / DXF / HTML writers return a
    // bool and the store functions return text only. The M3 writers record errno right after the failed open or write.
    enum class FileFailureCause
    {
        Unknown,
        NotFound,
        Denied,
        DiskFull
    };

    // file_error_reason: the sentence of a cause, "" for Unknown (Windows returns None).
    std::string file_failure_reason(FileFailureCause cause);

    // file_write_error_reason: as file_failure_reason, but a file that was not found is the destination folder.
    std::string file_write_failure_reason(FileFailureCause cause);

    // errno -> cause (file_error_reasons.py:34-39): ENOENT is NotFound, EACCES and EPERM are Denied (Python PermissionError covers both),
    // ENOSPC is DiskFull, anything else Unknown.
    FileFailureCause cause_from_errno(int error_number);

    // describe_write_failure: "The file could not be written. <reason>" with a known cause, else the generic "The file could not be
    // written. Please ensure you have write permissions to the destination folder and try again." (shared/texts.py:282-288).
    std::string write_failure_text(FileFailureCause cause);

    // ---- Export (OBJ, DXF, HTML) --------------------------------------------------------------------------------------------

    enum class ExportFailureKind
    {
        NoFileWritten, // the strategy wrote nothing (every mesh consumed by negatives, nothing the format can write)
        Unexpected,    // an exception, or no more precise cause
        WriteFailed    // the file could not be written
    };

    // The alert of a failed export. The title is always "Export Error" (presentation/texts.py:801); there is no "Nothing to Export" title and
    // no "Error creating <format> export:" prefix. The sentences (shared/texts.py:274-285) name no format: export_service.py:182-189 gives
    // the format-less one for OBJ, DXF and HTML. `cause` is used only by WriteFailed (Unknown gives the generic sentence).
    DialogText export_failure_text(ExportFailureKind kind, FileFailureCause cause = FileFailureCause::Unknown);
}
