// Layer: Import (pure C++)
// The raw import diagnostics that something else has to RECOGNISE: each is produced in one place of Core and matched by (or tested
// against) ost_domain::classify_import_failure (Domain/FailureText.hpp), so the producer and the classifier share one constant and cannot
// drift apart. The texts are the Windows importer texts (ost_importer.py, osp_importer.py); they go to the log, the alert shows one user
// sentence (EXP-21, messagebox.py:130-170). Every other diagnostic of the import is free text that the classifier treats as "damaged".
#pragma once

namespace ost_import
{
    // ost_importer.py:446-447 ValueError("No Bid element found in OST file"): OstXmlParser.cpp and OspPackage.cpp (embedded OST).
    constexpr const char *kNoBidError = "No Bid element found in OST file";

    // raw_bid_integrity.py: "The imported takeoff graph is invalid: <issues>". The producer appends the issue list to this prefix.
    constexpr const char *kTakeoffGraphInvalidPrefix = "The imported takeoff graph is invalid: ";

    // ost_importer.py:205-210 (RawBidIntegrity.cpp validate_raw_data).
    constexpr const char *kInvalidReferencesError = "The imported project contains invalid references.";

    // OspPackage.cpp stage_osp_package: the temporary folder could not be created. The classifier shows it like every other local failure
    // ("damaged", as Windows swallows it inside import_osp); the constant keeps the producer and its test on one text.
    constexpr const char *kOspTempFolderError = "Cannot create a temporary folder for the OSP package";
}
