// Layer: Import (pure C++)
// The OSP package of the Windows importer in platform independent form: which members a package may hold (IMP-14), which
// packaged image a page's ImagePath / OverlayImagePath refers to (IMP-16), where each image is staged without overwriting anything
// (IMP-12), that a failing write aborts the whole import (IMP-M02), and the flow that ties extraction, staging and parsing together.
//
// Windows: ost_visualizer/infrastructure/mdb/importers/osp_importer.py (_inspect_package, _collect_image_references,
// _resolve_image_member, _copy_referenced_images, _image_destination_relative_path, _non_conflicting_image_destination,
// _rewrite_image_paths, OspImporter._extracted_ost / _extract_images). Every text of an error is the Windows text.
// Member names and paths are UTF-8; the staging paths use '/' as separator (the platform adaptation of the Windows paths).
#pragma once
#include "OspFileSystem.hpp"
#include "OstXmlParser.hpp"
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace ost_import
{
    // The folder every packaged image lives in (osp_importer._IMAGE_MEMBER_ROOT).
    constexpr const char *OSP_IMAGE_MEMBER_ROOT = "TempImages!.tmp";

    // casefolded member name -> member name, of every member below TempImages!.tmp, in package order (the Windows dict order,
    // which decides ties of the image lookup).
    using ImageMemberIndex = std::vector<std::pair<std::string, std::string>>;

    struct OspPackage
    {
        // Every member name normalised: its path parts joined with '\' (osp_importer._OspPackage.member_names).
        std::vector<std::string> member_names;
        // The one top-level .ost member (normalised).
        std::string ost_member_name;
        ImageMemberIndex image_members_by_path;
    };

    // ---- IMP-14: package structure ---------------------------------------------------------------------------------------

    // _inspect_package(member_names): every name must be safe (CabMemberName.hpp), no two names may be equal once casefolded, there must
    // be exactly one top-level member whose extension is .ost (any case), and a member below TempImages!.tmp (any case) must be at
    // least one level deeper and a .pdf / .tif / .tiff. false with the Windows text in `error`:
    //   "unsafe CAB member path: '<name>'"                         (the name as the cabinet lists it)
    //   "duplicate or conflicting CAB member: '<normalised name>'"
    //   "unsupported member under TempImages!.tmp: '<normalised name>'"
    //   "archive must contain exactly one top-level .ost file; found <n>"
    // The first problem in member order is reported. Nothing is extracted before this passes.
    bool inspect_osp_package(const std::vector<std::string> &raw_names, OspPackage &package, std::string &error);

    // ---- IMP-16: image references -----------------------------------------------------------------------------------------

    // _is_supported_image_path: the extension of the final path component (either separator) is .pdf, .tif or .tiff, any case
    // (OSP_IMAGE_EXTENSIONS). ".pdf" alone is a name without extension.
    bool is_supported_image_path(const std::string &path);

    // _resolve_image_member: a reference that starts with TempImages!.tmp (any case) is looked up exactly (casefolded); any other
    // reference matches the packaged member with the deepest path-suffix match (a flat member 'a.pdf' matches when the reference ends in
    // it, a nested member needs all its parts as the end of the reference; there is no basename fallback into nested members).
    // false when nothing matches (Python None). `member_name` is the package's spelling of the member.
    bool resolve_image_member(const std::string &image_path, const ImageMemberIndex &index, std::string &member_name);

    struct ImageReference
    {
        std::string original_path; // the page's ImagePath / OverlayImagePath value
        std::string member_name;   // the packaged member it resolved to
    };

    struct ImageReferenceScan
    {
        // One entry per distinct original path, in order of first appearance (a dict keyed by the original path).
        std::vector<ImageReference> references;
        // The distinct paths that no member matched: they stay as they are (warning logged).
        size_t unresolved_count = 0;
        std::string first_unresolved;
    };

    // ET.parse + _collect_image_references: `xml` is the .ost file as it came out of the package. Only elements named BidPage (anywhere
    // in the document, the root included) and only their ImagePath and OverlayImagePath attributes count, each attribute on its own:
    // empty, missing and unsupported-extension values are skipped, the others are resolved against `index`. The warning of the
    // unresolved ones ("OSP import could not resolve N referenced image(s) in the package. Import will continue and preserve those
    // original paths. First missing source path: P") is logged through ost_log::warning.
    // false when the file is not well-formed XML: `parse_error` is ElementTree's ParseError text.
    bool collect_image_references(const char *xml, size_t length, const ImageMemberIndex &index, ImageReferenceScan &scan,
                                  std::string &parse_error);

    // _rewrite_image_paths on the parsed rows of the pages: every ImagePath / OverlayImagePath that is non-empty and a key of
    // `copied_paths` (original -> staged location) is replaced; everything else stays. true when a value changed.
    bool rewrite_image_paths(RowList &page_rows, const std::vector<std::pair<std::string, std::string>> &copied_paths);

    // ---- IMP-12 / IMP-M02: staging --------------------------------------------------------------------------------------

    // _image_destination_relative_path: a member directly below TempImages!.tmp is staged as "<name>"; any other member as
    // "Images/<first 16 hex digits of sha1(casefolded member name)>/<name>". A reserved Windows device name in any part is refused:
    // "image staging name is a reserved device name: '<member>'".
    bool image_destination_relative_path(const std::string &member_name, std::string &relative_path, std::string &error);

    // _files_have_same_content: same size and same bytes (compared in 1 MiB chunks) -> `same`. A second file that is not a regular file (a
    // directory) is never the same content. The return value is false when a file cannot be read (Windows raises, the import fails).
    bool files_have_same_content(OspFileSystem &fs, const std::string &first, const std::string &second, bool &same);

    // _non_conflicting_image_destination: `destination` when nothing is there or the same bytes are; otherwise
    // "<stem>-<sha256 of the source>[:16|:32|all]<suffix>" in the same folder, the first of the three that is free or holds the same
    // bytes. A name taken by different content is never overwritten. When all three are taken by other content:
    // "image staging content-address collision for '<name>'".
    bool non_conflicting_image_destination(OspFileSystem &fs, const std::string &source, const std::string &destination,
                                           std::string &chosen, std::string &error);

    // _copy_referenced_images: every packaged image the references need is first checked to be an extracted FILE below `extract_root`
    // ("image member was not extracted: <member>" otherwise, before anything is copied), then copied once per member (casefold key) to its
    // staging place below `dest_dir`. Two different members that end at the same destination (compared casefolded) abort with
    // "image staging destination collision between '<member>' and '<other member key>'". Any failure of the file system (a folder or a
    // copy that fails, a full disk) aborts with a message and the caller imports nothing. `copied_paths` maps each original path to the
    // staged file.
    bool copy_referenced_images(OspFileSystem &fs, const std::string &extract_root, const std::string &dest_dir,
                                const std::vector<ImageReference> &references,
                                std::vector<std::pair<std::string, std::string>> &copied_paths, std::string &error);

    // ---- the whole package ------------------------------------------------------------------------------------------------

    struct OspStageResult
    {
        // The parsed .ost with the staged images' locations in its pages' ImagePath / OverlayImagePath.
        OstRawData raw;
        // The name of the .ost without extension: the folder below `images_root` that holds the staged images.
        std::string package_name;
        size_t staged_image_count = 0;
        std::string error;
    };

    // osp_importer.OspImporter._extracted_ost + _extract_images + import_ost's parse, up to (not including) the validation and the
    // database write:
    //   1. list the members (an unreadable package: "Invalid OSP file format"), inspect_osp_package (IMP-14, IMP-15);
    //   2. extract EVERY member in one streaming pass into a fresh folder below `temp_parent` (extract_cab_to_directory, IMP-M03; a
    //      damaged package: "The OSP archive could not be extracted"); the folder is removed again on every path;
    //   3. read the .ost ("embedded OST member was not extracted: <name>"), scan its image references (a malformed file:
    //      "embedded OST data is corrupt: <ParseError text>");
    //   4. copy the referenced images to `images_root`/<package name>/ (IMP-12, IMP-M02): nothing is staged before 1-3 succeeded;
    //   5. parse the .ost (ost_import::parse_ost_xml) and point the pages' paths at the staged files.
    // false: `error` holds the text and nothing is imported (files staged before a later failure stay, as on Windows).
    bool stage_osp_package(const char *data, size_t length, const std::string &temp_parent, const std::string &images_root,
                           OspFileSystem &fs, OspStageResult &result);
}
