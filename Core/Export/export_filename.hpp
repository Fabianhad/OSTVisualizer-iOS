#pragma once
// The suggested file name of an export (EXP-09). Pure C++, no Foundation.
//
// Port of the Windows infrastructure/utils/filename.py sanitize_filename and infrastructure/visualization_provider.py
// _prepare_export_filename (the name the save dialog proposes): '<bid> - <pages>.<extension>', cut to 255 characters.
#include <string>
#include <vector>

namespace ost_geometry
{
    namespace export_text
    {

        // sanitize_filename: every character of [\x00-\x1f \ / : " * ? < > |] becomes '_' (UTF-8 text; every other byte,
        // non-ASCII included, is kept).
        std::string sanitize_filename(const std::string &name);

        // _prepare_export_filename(extension, bid_name, page_names): both the bid name and the page names are sanitised, then
        //   one page    '<bid> - <page>.<ext>'
        //   two pages   '<bid> - <A> + <B>.<ext>'
        //   more pages  '<bid> - <A> + <B> + <N - 2> more.<ext>'
        // A name longer than 255 characters (code points, like Python's len; not bytes or UTF-16 units) is shortened: the bid
        // name is cut to leave room and ends in '...' when it keeps more than 10 characters; otherwise a single page gives
        // '<page cut to 255 - len(.ext) - 3>....<ext>' and several pages give 'Export_<N>_pages.<ext>'. `extension` has no dot.
        // `page_names` are the names of the pages that have something to export, in the order of the selection; it must not
        // be empty (Windows never asks for a name without a page: its export fails with NO_DATA before): an empty list
        // throws std::invalid_argument.
        std::string export_filename(const std::string &extension,
                                    const std::string &bid_name,
                                    const std::vector<std::string> &page_names);

    } // namespace export_text
} // namespace ost_geometry
