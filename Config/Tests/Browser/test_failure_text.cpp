// EXP-21 (owner decision 1, 2026-11-27): every alert of an import, a load, an export and a folder delete uses the CURRENT Windows wording
// (Windows HEAD d1c430a) and shows ONE user sentence; the exception, parser, SQL and writer text goes to the log only
// (presentation/utils/messagebox.py:130-170). Core/Domain/FailureText.hpp holds the sentences and the classification, the Objective-C only
// calls them. Every sentence is pinned byte for byte against its Windows source.
// Windows oracle:
//   tests/presentation/handlers/test_import_handler.py (rejected import, worker exception, FAILED_TEXT)
//   tests/application/use_cases/project/test_import_project_files_from_args_use_case.py
//   tests/application/services/test_export_service.py, tests/domain/services/test_file_error_reasons.py
//   tests/infrastructure/persistence/repositories/test_file_project_repository.py (reason texts)
#include "TestHarness.hpp"

#include "Domain/BrowserRules.hpp"
#include "Domain/DatabaseHierarchy.hpp"
#include "Domain/FailureText.hpp"
#include "Import/ImportDiagnostics.hpp"

#include <cerrno>
#include <string>

using ost_domain::ExportFailureKind;
using ost_domain::FileFailureCause;
using ost_domain::ImportContext;
using ost_domain::ImportFailureKind;
using ost_domain::ImportFormat;
using ost_domain::LoadTarget;

namespace
{
    const char *const kOstDamaged = "The OST file is damaged or uses an unsupported format, so it cannot be imported.";
    const char *const kOspDamaged = "The OSP file is damaged or uses an unsupported format, so it cannot be imported.";
    const char *const kOstUnexpected =
        "An unexpected error stopped the import of the OST file. Please check that the file is valid and try again; if the problem "
        "continues, contact support.";
    const char *const kOspUnexpected =
        "An unexpected error stopped the import of the OSP file. Please check that the file is valid and try again; if the problem "
        "continues, contact support.";
    const char *const kArgsDamaged = "The file is damaged or uses an unsupported format, so it cannot be imported.";
    const char *const kArgsUnexpected = "An unexpected error stopped the import. Please try again; if the problem continues, contact support.";
    const char *const kArgsMissing = "File does not exist.";
    const char *const kWriteGeneric =
        "The file could not be written. Please ensure you have write permissions to the destination folder and try again.";
    const char *const kNotFound = "The file was not found. It may have been moved, renamed or deleted.";
    const char *const kDestinationNotFound = "The destination folder was not found. Choose another folder and try again.";
    const char *const kDenied = "Access to the file was denied. Check that you have permission to use the file and its folder.";
    const char *const kDiskFull = "There is not enough space on the disk. Free up space and try again.";
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Import: the raw diagnostics of Core are kept (their producers share the constants), the classifier maps them to a kind
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(Exp21_ImportDiagnostics, TheSharedConstantsKeepTheCoreTexts)
{
    CHECK_STR_EQ(ost_import::kNoBidError, "No Bid element found in OST file");
    CHECK_STR_EQ(ost_import::kTakeoffGraphInvalidPrefix, "The imported takeoff graph is invalid: ");
    CHECK_STR_EQ(ost_import::kInvalidReferencesError, "The imported project contains invalid references.");
    CHECK_STR_EQ(ost_import::kOspTempFolderError, "Cannot create a temporary folder for the OSP package");
}

TEST(Exp21_ImportClassify, TheKnownDiagnosticsHaveTheirOwnKind)
{
    CHECK(ost_domain::classify_import_failure("No Bid element found in OST file") == ImportFailureKind::NoBid);
    CHECK(ost_domain::classify_import_failure("The imported takeoff graph is invalid: BidTakeoffs.UID=40 occurs 2 times") ==
          ImportFailureKind::TakeoffGraphInvalid);
    CHECK(ost_domain::classify_import_failure("The imported project contains invalid references.") == ImportFailureKind::InvalidReferences);
    // a device side failure, not a statement about the file
    CHECK(ost_domain::classify_import_failure("Cannot create a temporary folder for the OSP package") == ImportFailureKind::Unexpected);
}

TEST(Exp21_ImportClassify, EveryOtherDiagnosticIsDamaged)
{
    const char *const raws[] = {
        "mismatched tag: line 3, column 5",
        "unsafe CAB member path: ../x.pdf",
        "The OSP archive could not be extracted",
        "The embedded OST file could not be read",
        "embedded OST data is corrupt: no element found",
        "Invalid OSP file format",
        "Exceeds the limit (4300 digits) for integer string conversion: value has 5000 digits; use sys.set_int_max_str_digits() to increase the limit",
        "",
    };
    for (const char *raw : raws)
    {
        SCOPED_CONTEXT(std::string("raw: ") + raw);
        CHECK(ost_domain::classify_import_failure(raw) == ImportFailureKind::Damaged);
    }
    // the no-bid and the invalid-references texts are compared exactly; the graph text is matched by its prefix
    CHECK(ost_domain::classify_import_failure("No Bid element found in OST file.") == ImportFailureKind::Damaged);
    CHECK(ost_domain::classify_import_failure("The imported takeoff graph is invalid") == ImportFailureKind::Damaged);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Import alert, the picker (the Windows LOCAL import: import_handler.py:218-246)
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(Exp21_ImportText, ThePickerTitleIsImportError)
{
    CHECK_STR_EQ(ost_domain::IMPORT_ERROR_TITLE, "Import Error");
    CHECK_STR_EQ(ost_domain::import_failure_text(ImportFailureKind::Damaged, ImportFormat::Ost, ImportContext::Picker, "").title, "Import Error");
    CHECK_STR_EQ(ost_domain::import_failure_text(ImportFailureKind::Unexpected, ImportFormat::Osp, ImportContext::OpenIn, "a.osp").title,
                 "Import Error");
}

TEST(Exp21_ImportText, ThePickerShowsTheDamagedSentenceForEveryFailureOfTheFile)
{
    // presentation/texts.py:945-948. The local path has no per-case sentence: no bid, invalid takeoffs, invalid references, a missing or
    // unreadable file all give the same one (the per-case sentences reach a dialog only on the SQL import, which iOS does not have).
    const ImportFailureKind kinds[] = {ImportFailureKind::Damaged,          ImportFailureKind::NoBid,
                                       ImportFailureKind::TakeoffGraphInvalid, ImportFailureKind::InvalidReferences,
                                       ImportFailureKind::FileNotFound};
    for (ImportFailureKind kind : kinds)
    {
        SCOPED_CONTEXT(std::to_string(static_cast<int>(kind)));
        CHECK_STR_EQ(ost_domain::import_failure_text(kind, ImportFormat::Ost, ImportContext::Picker, "").message, kOstDamaged);
        CHECK_STR_EQ(ost_domain::import_failure_text(kind, ImportFormat::Osp, ImportContext::Picker, "").message, kOspDamaged);
    }
}

TEST(Exp21_ImportText, ThePickerShowsTheUnexpectedSentenceWithTheFormat)
{
    // presentation/texts.py:949-952
    CHECK_STR_EQ(ost_domain::import_failure_text(ImportFailureKind::Unexpected, ImportFormat::Ost, ImportContext::Picker, "").message,
                 kOstUnexpected);
    CHECK_STR_EQ(ost_domain::import_failure_text(ImportFailureKind::Unexpected, ImportFormat::Osp, ImportContext::Picker, "").message,
                 kOspUnexpected);
}

TEST(Exp21_ImportText, ThePickerIgnoresTheFileName)
{
    CHECK_STR_EQ(ost_domain::import_failure_text(ImportFailureKind::Damaged, ImportFormat::Ost, ImportContext::Picker, "job.ost").message,
                 kOstDamaged);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Import alert, open-in (the Windows "args" use case: the line is "<file>: <sentence>", main_window.py:1198; shared/texts.py:863-885)
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(Exp21_ImportText, OpenInPrefixesTheFileNameAndUsesTheArgsSentences)
{
    CHECK_STR_EQ(ost_domain::import_failure_text(ImportFailureKind::Damaged, ImportFormat::Ost, ImportContext::OpenIn, "job.ost").message,
                 std::string("job.ost: ") + kArgsDamaged);
    CHECK_STR_EQ(ost_domain::import_failure_text(ImportFailureKind::Damaged, ImportFormat::Osp, ImportContext::OpenIn, "job.osp").message,
                 std::string("job.osp: ") + kArgsDamaged);
    CHECK_STR_EQ(ost_domain::import_failure_text(ImportFailureKind::Unexpected, ImportFormat::Ost, ImportContext::OpenIn, "job.ost").message,
                 std::string("job.ost: ") + kArgsUnexpected);
    CHECK_STR_EQ(ost_domain::import_failure_text(ImportFailureKind::FileNotFound, ImportFormat::Ost, ImportContext::OpenIn, "job.ost").message,
                 std::string("job.ost: ") + kArgsMissing);
}

TEST(Exp21_ImportText, OpenInGivesTheDamagedSentenceForTheCasesWithoutAnArgsSentence)
{
    for (ImportFailureKind kind : {ImportFailureKind::NoBid, ImportFailureKind::TakeoffGraphInvalid, ImportFailureKind::InvalidReferences})
    {
        SCOPED_CONTEXT(std::to_string(static_cast<int>(kind)));
        CHECK_STR_EQ(ost_domain::import_failure_text(kind, ImportFormat::Ost, ImportContext::OpenIn, "a b.ost").message,
                     std::string("a b.ost: ") + kArgsDamaged);
    }
}

TEST(Exp21_ImportText, OpenInWithoutAFileNameHasNoDanglingPrefix)
{
    CHECK_STR_EQ(ost_domain::import_failure_text(ImportFailureKind::Damaged, ImportFormat::Ost, ImportContext::OpenIn, "").message, kArgsDamaged);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Load: the frame stays "Error Loading File" / "Failed to load <name>:\n<reason>"; the reason is a user sentence
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(Exp21_LoadReason, TheProjectListSaysTheFileCouldNotBeRead)
{
    // shared/texts.py:1238-1240 (FILE_PROJECT_REPOSITORY_MESSAGE_THE_FILE_COULD_NOT_BE_READ)
    CHECK_STR_EQ(ost_domain::load_failure_reason(LoadTarget::Database), "The file could not be read. It may be damaged or may not be an OST database.");
}

TEST(Exp21_LoadReason, ABidSaysAnUnexpectedErrorStoppedOpeningIt)
{
    // shared/texts.py:316-318 (PROJECT_OPERATIONS_SERVICE_MESSAGE_AN_UNEXPECTED_ERROR_STOPPED_OPENING)
    CHECK_STR_EQ(ost_domain::load_failure_reason(LoadTarget::Bid),
                 "An unexpected error stopped opening the bid. Please try again; if the problem continues, contact support.");
}

TEST(Exp21_LoadReason, TheFrameCarriesTheReasonAndNoDeveloperText)
{
    ost_domain::DialogText text = ost_domain::load_failure_text("Tower A", ost_domain::load_failure_reason(LoadTarget::Bid));
    CHECK_STR_EQ(text.title, "Error Loading File");
    CHECK_STR_EQ(text.message,
                 "Failed to load Tower A:\nAn unexpected error stopped opening the bid. Please try again; if the problem continues, contact support.");
    text = ost_domain::load_failure_text("Projects", ost_domain::load_failure_reason(LoadTarget::Database));
    CHECK_STR_EQ(text.message, "Failed to load Projects:\nThe file could not be read. It may be damaged or may not be an OST database.");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Folder delete: only the designed refusal keeps "Cannot Delete Project"; a store failure is the Windows "Delete Error"
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(Exp21_FolderDelete, AProjectWithBidsKeepsTheCannotDeleteDialog)
{
    // project_write_handler._delete_projects: "Cannot Delete Project" / "Some selected projects still have bids inside.\nOnly empty projects can be deleted."
    const ost_domain::DialogText text = ost_domain::folder_delete_alert(ost_domain::CANNOT_DELETE_FOLDER_MESSAGE);
    CHECK_STR_EQ(text.title, "Cannot Delete Project");
    CHECK_STR_EQ(text.message, "Some selected projects still have bids inside.\nOnly empty projects can be deleted.");
}

TEST(Exp21_FolderDelete, EveryOtherReasonIsTheDeleteErrorAndNeverShowsTheReason)
{
    const char *const reasons[] = {
        "Cannot read Bids: disk I/O error",
        "Cannot open the database: unable to open database file",
        "Invalid BidProjects UID: 'x'",
        "Cannot delete the system project 'Deleted Bids' (UID 1)",
        "",
    };
    for (const char *reason : reasons)
    {
        SCOPED_CONTEXT(std::string("reason: ") + reason);
        const ost_domain::DialogText text = ost_domain::folder_delete_alert(reason);
        const ost_domain::DialogText want = ost_domain::browser_failure_text(ost_domain::BrowserFailure::DeleteFolders);
        CHECK_STR_EQ(text.title, "Delete Error");
        CHECK_STR_EQ(text.message, "Failed to delete projects. The database may be locked by another application.");
        CHECK_STR_EQ(text.title, want.title);
        CHECK_STR_EQ(text.message, want.message);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Known-cause file failures (file_error_reasons.py): the vocabulary for the writers that report an errno (M3); nothing wires it today
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(Exp21_FileFailure, TheReasonSentencesAreTheWindowsOnes)
{
    // shared/texts.py:57-75
    CHECK_STR_EQ(ost_domain::file_failure_reason(FileFailureCause::NotFound), kNotFound);
    CHECK_STR_EQ(ost_domain::file_failure_reason(FileFailureCause::Denied), kDenied);
    CHECK_STR_EQ(ost_domain::file_failure_reason(FileFailureCause::DiskFull), kDiskFull);
    // file_error_reason returns None for an unknown cause
    CHECK_STR_EQ(ost_domain::file_failure_reason(FileFailureCause::Unknown), "");
}

TEST(Exp21_FileFailure, AWriteNamesTheDestinationFolderWhenTheFileWasNotFound)
{
    // file_write_error_reason (file_error_reasons.py:47-51)
    CHECK_STR_EQ(ost_domain::file_write_failure_reason(FileFailureCause::NotFound), kDestinationNotFound);
    CHECK_STR_EQ(ost_domain::file_write_failure_reason(FileFailureCause::Denied), kDenied);
    CHECK_STR_EQ(ost_domain::file_write_failure_reason(FileFailureCause::DiskFull), kDiskFull);
    CHECK_STR_EQ(ost_domain::file_write_failure_reason(FileFailureCause::Unknown), "");
}

TEST(Exp21_FileFailure, TheCauseComesFromErrno)
{
    CHECK(ost_domain::cause_from_errno(ENOENT) == FileFailureCause::NotFound);
    CHECK(ost_domain::cause_from_errno(EACCES) == FileFailureCause::Denied);
    CHECK(ost_domain::cause_from_errno(EPERM) == FileFailureCause::Denied); // Python PermissionError covers both
    CHECK(ost_domain::cause_from_errno(ENOSPC) == FileFailureCause::DiskFull);
    CHECK(ost_domain::cause_from_errno(0) == FileFailureCause::Unknown);
    CHECK(ost_domain::cause_from_errno(EIO) == FileFailureCause::Unknown);
    CHECK(ost_domain::cause_from_errno(-1) == FileFailureCause::Unknown);
}

TEST(Exp21_FileFailure, TheWriteFailureSentenceUsesTheCauseOrTheGenericText)
{
    // shared/texts.py:282-288
    CHECK_STR_EQ(ost_domain::write_failure_text(FileFailureCause::Unknown), kWriteGeneric);
    CHECK_STR_EQ(ost_domain::write_failure_text(FileFailureCause::NotFound), std::string("The file could not be written. ") + kDestinationNotFound);
    CHECK_STR_EQ(ost_domain::write_failure_text(FileFailureCause::Denied), std::string("The file could not be written. ") + kDenied);
    CHECK_STR_EQ(ost_domain::write_failure_text(FileFailureCause::DiskFull), std::string("The file could not be written. ") + kDiskFull);
    CHECK_STR_EQ(ost_domain::write_failure_text(ost_domain::cause_from_errno(ENOSPC)),
                 "The file could not be written. There is not enough space on the disk. Free up space and try again.");
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Export (OBJ, DXF, HTML: the strategies of export_service.py:157-189; the title of every failure is "Export Error")
// ---------------------------------------------------------------------------------------------------------------------------------

TEST(Exp21_ExportText, NothingWasWrittenIsTheWindowsNoFileWasWrittenSentence)
{
    // shared/texts.py:274-277
    const ost_domain::DialogText text = ost_domain::export_failure_text(ExportFailureKind::NoFileWritten);
    CHECK_STR_EQ(text.title, "Export Error");
    CHECK_STR_EQ(text.message,
                 "No file was written. Check that the selected pages have takeoffs this format can export, then try again or choose a different "
                 "destination.");
}

TEST(Exp21_ExportText, AnUnexpectedErrorHasNoFormatName)
{
    // shared/texts.py:278-281 (export_service.py:182-189: the format-less sentence for OBJ, DXF and HTML)
    const ost_domain::DialogText text = ost_domain::export_failure_text(ExportFailureKind::Unexpected);
    CHECK_STR_EQ(text.title, "Export Error");
    CHECK_STR_EQ(text.message,
                 "An unexpected error stopped the export. Please try again or choose a different destination; if the problem continues, contact "
                 "support.");
}

TEST(Exp21_ExportText, AWriteFailureIsTheGenericSentenceUnlessTheCauseIsKnown)
{
    // shared/texts.py:282-285
    ost_domain::DialogText text = ost_domain::export_failure_text(ExportFailureKind::WriteFailed);
    CHECK_STR_EQ(text.title, "Export Error");
    CHECK_STR_EQ(text.message, kWriteGeneric);
    text = ost_domain::export_failure_text(ExportFailureKind::WriteFailed, FileFailureCause::DiskFull);
    CHECK_STR_EQ(text.title, "Export Error");
    CHECK_STR_EQ(text.message, "The file could not be written. There is not enough space on the disk. Free up space and try again.");
}

TEST(Exp21_ExportText, NoFailureTextCarriesAPrefixOrADeveloperPhrase)
{
    const ExportFailureKind kinds[] = {ExportFailureKind::NoFileWritten, ExportFailureKind::Unexpected, ExportFailureKind::WriteFailed};
    for (ExportFailureKind kind : kinds)
    {
        SCOPED_CONTEXT(std::to_string(static_cast<int>(kind)));
        const std::string message = ost_domain::export_failure_text(kind).message;
        CHECK(message.find("Error creating") == std::string::npos);
        CHECK(message.find("Export function returned False") == std::string::npos);
        CHECK(message.find("The export failed") == std::string::npos);
        CHECK(!message.empty() && message.back() == '.');
    }
}
