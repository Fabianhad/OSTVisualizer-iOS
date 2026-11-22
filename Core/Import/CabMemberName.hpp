// Layer: Import (pure C++)
// Member-name rules of the OSP importer (IMP-15): which names inside a cabinet are acceptable, and how a name splits
// into path components. The names come from untrusted archives; the Windows importer rejects every package that holds a
// name that would be unsafe on a Windows file system before anything is extracted, and iOS does the same.
//
// Windows: ost_visualizer/infrastructure/mdb/importers/osp_importer.py _is_windows_device_name,
// _is_safe_cab_member_name and _windows_path_parts (PureWindowsPath semantics, Python 3.11).
// All strings are UTF-8.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ost_import
{
    // Windows cab_compressor.cpp decode_cab_member (+ encode_utf8): the member name as stored in the cabinet (`raw`, the
    // bytes before the NUL) as UTF-8. With the CFFILE attribute _A_NAME_IS_UTF (0x80) set the bytes must be valid UTF-8
    // (strict: no overlong forms, surrogates or code points above U+10FFFF) and are kept as they are; otherwise they are
    // the legacy ANSI code page, read as cp1252 (IMP-18: the Windows code uses the machine's ANSI code page; the tests and
    // the user's decision say cp1252) and converted. false = the name does not decode (the Windows listing/extraction fails).
    // Separators are not touched (the cabinet reader turns '\' into '/').
    bool decode_cab_member_name(const std::string &raw, uint16_t attribs, std::string &utf8);

    // _is_windows_device_name(part): the base name of a path component (everything before the first '.', trailing
    // spaces removed, upper-cased) is a reserved device name: CON, PRN, AUX, NUL, CONIN$, CONOUT$, COM1-9, LPT1-9 (also
    // with the superscript digits U+00B9, U+00B2, U+00B3).
    bool is_windows_device_name(const std::string &part);

    // _windows_path_parts(path): PureWindowsPath(path.replace("/", "\\")).parts. Both separators split, empty and "."
    // components vanish, ".." stays. A drive and/or root ("C:", "C:\\", "\\", "\\\\server\\share\\", "\\\\?\\C:\\")
    // comes first as one anchor component.
    std::vector<std::string> windows_path_parts(const std::string &path);

    // _is_safe_cab_member_name(member_name): not empty, no drive or root (so not absolute, not UNC, no "//?/"
    // prefix) and every component is free of ':', does not end in '.' or ' ' (this also rejects "..") and is not a
    // reserved device name. At least one component is required ("." alone is not a name).
    bool is_safe_cab_member_name(const std::string &member_name);

    // The Windows error text for a name that is not safe: _OspFormatError(f"unsafe CAB member path: {raw_name!r}"), i.e.
    // "unsafe CAB member path: " + the Python repr of the name (python_repr below). The name is the one the cabinet
    // reader reports, with '/' where the cabinet has a backslash (the raw separators are not kept).
    std::string unsafe_member_path_message(const std::string &member_name);

    // Python repr(str) of a text: quotes, backslash and control characters escaped as repr does, and every character that
    // str.isprintable() rejects (Unicode 14 as in CPython 3.11: the categories Cc, Cf, Cs, Co, Cn, Zl, Zp and Zs except the
    // space) written as the escape repr uses; the other characters stay as they are. Bytes that are not well-formed UTF-8
    // stay as they are. The Windows messages quote member names with {name!r}.
    std::string python_repr(const std::string &text);

    // PureWindowsPath(path.replace("/", "\\")).name / .suffix / .stem: the final component after the anchor ("" when the path
    // is only an anchor or has no components), its extension (the text from the last '.', only when that dot is neither the first
    // nor the last character of the name) and the name without the extension.
    std::string windows_path_name(const std::string &path);
    std::string windows_path_suffix(const std::string &path);
    std::string windows_path_stem(const std::string &path);
}
