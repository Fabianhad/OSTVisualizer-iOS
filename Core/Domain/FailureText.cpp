// Layer: Domain. The failure texts of the EXP-21 job: see FailureText.hpp.
#include "FailureText.hpp"
#include "DatabaseHierarchy.hpp"
#include "../Import/ImportDiagnostics.hpp"
#include <cerrno>
#include <cstring>

namespace ost_domain
{
    namespace
    {
        // presentation/texts.py:945-952 (the format name is "OST" or "OSP").
        const char *format_name(ImportFormat format)
        {
            return format == ImportFormat::Osp ? "OSP" : "OST";
        }

        // shared/texts.py:863-885
        constexpr const char *ARGS_DAMAGED = "The file is damaged or uses an unsupported format, so it cannot be imported.";
        constexpr const char *ARGS_UNEXPECTED = "An unexpected error stopped the import. Please try again; if the problem continues, contact support.";
        constexpr const char *ARGS_FILE_MISSING = "File does not exist.";

        // shared/texts.py:57-75
        constexpr const char *REASON_NOT_FOUND = "The file was not found. It may have been moved, renamed or deleted.";
        constexpr const char *REASON_DESTINATION_NOT_FOUND = "The destination folder was not found. Choose another folder and try again.";
        constexpr const char *REASON_DENIED = "Access to the file was denied. Check that you have permission to use the file and its folder.";
        constexpr const char *REASON_DISK_FULL = "There is not enough space on the disk. Free up space and try again.";

        // shared/texts.py:274-288
        constexpr const char *EXPORT_NO_FILE_WRITTEN =
            "No file was written. Check that the selected pages have takeoffs this format can export, then try again or choose a different "
            "destination.";
        constexpr const char *EXPORT_UNEXPECTED =
            "An unexpected error stopped the export. Please try again or choose a different destination; if the problem continues, contact "
            "support.";
        constexpr const char *EXPORT_WRITE_FAILED =
            "The file could not be written. Please ensure you have write permissions to the destination folder and try again.";
        constexpr const char *EXPORT_WRITE_FAILED_PREFIX = "The file could not be written. ";

        // presentation/texts.py:801
        constexpr const char *EXPORT_ERROR_TITLE = "Export Error";
    }

    ImportFailureKind classify_import_failure(const std::string &raw)
    {
        if (raw == ost_import::kNoBidError)
            return ImportFailureKind::NoBid;
        if (raw == ost_import::kInvalidReferencesError)
            return ImportFailureKind::InvalidReferences;
        if (raw == ost_import::kOspTempFolderError)
            return ImportFailureKind::Unexpected;
        const size_t prefix_length = std::strlen(ost_import::kTakeoffGraphInvalidPrefix);
        if (raw.compare(0, prefix_length, ost_import::kTakeoffGraphInvalidPrefix) == 0)
            return ImportFailureKind::TakeoffGraphInvalid;
        return ImportFailureKind::Damaged;
    }

    DialogText import_failure_text(ImportFailureKind kind, ImportFormat format, ImportContext context, const std::string &file_name)
    {
        if (context == ImportContext::OpenIn)
        {
            std::string message = file_name.empty() ? std::string() : file_name + ": ";
            if (kind == ImportFailureKind::Unexpected)
                message += ARGS_UNEXPECTED;
            else if (kind == ImportFailureKind::FileNotFound)
                message += ARGS_FILE_MISSING;
            else
                message += ARGS_DAMAGED;
            return {IMPORT_ERROR_TITLE, message};
        }
        if (kind == ImportFailureKind::Unexpected)
        {
            return {IMPORT_ERROR_TITLE,
                    std::string("An unexpected error stopped the import of the ") + format_name(format) +
                        " file. Please check that the file is valid and try again; if the problem continues, contact support."};
        }
        return {IMPORT_ERROR_TITLE,
                std::string("The ") + format_name(format) + " file is damaged or uses an unsupported format, so it cannot be imported."};
    }

    std::string load_failure_reason(LoadTarget target)
    {
        if (target == LoadTarget::Bid)
            return "An unexpected error stopped opening the bid. Please try again; if the problem continues, contact support.";
        return "The file could not be read. It may be damaged or may not be an OST database.";
    }

    DialogText folder_delete_alert(const std::string &reason)
    {
        if (reason == CANNOT_DELETE_FOLDER_MESSAGE)
            return {CANNOT_DELETE_FOLDER_TITLE, CANNOT_DELETE_FOLDER_MESSAGE};
        return browser_failure_text(BrowserFailure::DeleteFolders);
    }

    std::string file_failure_reason(FileFailureCause cause)
    {
        switch (cause)
        {
        case FileFailureCause::NotFound:
            return REASON_NOT_FOUND;
        case FileFailureCause::Denied:
            return REASON_DENIED;
        case FileFailureCause::DiskFull:
            return REASON_DISK_FULL;
        case FileFailureCause::Unknown:
            break;
        }
        return std::string();
    }

    std::string file_write_failure_reason(FileFailureCause cause)
    {
        if (cause == FileFailureCause::NotFound)
            return REASON_DESTINATION_NOT_FOUND;
        return file_failure_reason(cause);
    }

    FileFailureCause cause_from_errno(int error_number)
    {
        switch (error_number)
        {
        case ENOENT:
            return FileFailureCause::NotFound;
        case EACCES:
        case EPERM:
            return FileFailureCause::Denied;
        case ENOSPC:
            return FileFailureCause::DiskFull;
        default:
            break;
        }
        return FileFailureCause::Unknown;
    }

    std::string write_failure_text(FileFailureCause cause)
    {
        const std::string reason = file_write_failure_reason(cause);
        return reason.empty() ? std::string(EXPORT_WRITE_FAILED) : std::string(EXPORT_WRITE_FAILED_PREFIX) + reason;
    }

    DialogText export_failure_text(ExportFailureKind kind, FileFailureCause cause)
    {
        switch (kind)
        {
        case ExportFailureKind::NoFileWritten:
            return {EXPORT_ERROR_TITLE, EXPORT_NO_FILE_WRITTEN};
        case ExportFailureKind::WriteFailed:
            return {EXPORT_ERROR_TITLE, write_failure_text(cause)};
        case ExportFailureKind::Unexpected:
            break;
        }
        return {EXPORT_ERROR_TITLE, EXPORT_UNEXPECTED};
    }
}
