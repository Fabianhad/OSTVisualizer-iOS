// Layer: Import (pure C++)
// Strict XML front end of the OST import (IMP-M01).
//
// The Windows importer reads an .ost with xml.etree.ElementTree.parse (ost_importer._parse_ost_xml), i.e. with the strict
// expat parser: a file that is not well-formed XML 1.0 (+ namespaces) is rejected as a whole (ET.ParseError), a declared
// encoding is honoured (windows-1252, ISO-8859-1, ...), and the attribute values come out as proper Unicode text.
// pugixml, which builds the iOS document tree, accepts nearly everything and keeps raw bytes: duplicate attributes
// (last wins), '<' in a value, a second root element, undefined entities, control characters, and the raw bytes of a
// windows-1252 file (invalid UTF-8, which becomes a nil NSString in the Objective-C layer).
//
// normalize_xml sits in front of pugixml: it parses the bytes with expat's rules, rejects what expat rejects (same
// accept/reject decision, same message text where the position is comparable) and returns the document re-written as
// plain UTF-8 that pugixml reads to the same tree ElementTree builds:
//   - transcoded from the detected / declared encoding; the BOM, the XML declaration, the DOCTYPE, comments and
//     processing instructions are gone; CDATA sections are plain text; all entity and character references are
//     resolved; line ends and attribute values are normalized as the XML specification says;
//   - namespaces are processed like expat does (xmlns declarations are consumed, an unbound prefix is an error); a
//     namespaced element or attribute name {uri}local gets the pugixml-safe spelling
//     "_ns:<hex of the UTF-8 bytes of the uri>:<local>" (never equal to a plain name, which cannot contain ':'), so the
//     importer's lookups for plain names behave exactly like ElementTree's find("Bid") on '{uri}Bid'.
//
// Deliberate limits (rejected with an error instead of guessed; Windows accepts them):
//   - a DOCTYPE internal subset that declares anything (<!ENTITY>, <!ATTLIST>, <!ELEMENT>, <!NOTATION>, parameter
//     entity references): its entities and default attribute values would change the tree. An empty subset, comments
//     and processing instructions inside it are fine.
//   - declared encodings other than UTF-8, UTF-16, US-ASCII, ISO-8859-1, windows-1252 and their Python aliases (Python
//     accepts every single-byte codec of its standard library).
#pragma once
#include <cstddef>
#include <string>

namespace ost_import
{
    struct NormalizedXml
    {
        bool ok = false;
        std::string utf8;  // the normalized document (valid UTF-8, no BOM, no declaration); only when ok
        std::string error; // expat's message "<reason>: line L, column C" (ElementTree.ParseError text); only when !ok
    };

    NormalizedXml normalize_xml(const char *data, size_t length);
}
