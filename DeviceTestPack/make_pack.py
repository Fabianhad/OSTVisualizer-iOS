#!/usr/bin/env python3
"""Builds the M1 device test pack: ten small .osp packages the owner loads on an iPhone / iPad.

Python 3 standard library only. Usage:

    python make_pack.py                  write the .osp files and PACK_SHA256.txt next to this script
    python make_pack.py --out DIR        write the .osp files into DIR (no PACK_SHA256.txt)
    python make_pack.py --facts FILE     also write the facts of every bid (what the README promises) as tab separated lines
    python make_pack.py --check          rebuild in memory and compare with the files next to this script and with PACK_SHA256.txt
    python make_pack.py --selftest       checks that need no other tool: a multi-block cabinet round trip, the xref table of every PDF,
                                         the references inside every OST file, byte-identical output of two builds

What it contains
  * a pure-Python Microsoft Cabinet (MSZIP) writer: one folder, CFDATA blocks of at most 32768 bytes, each block "CK" + a raw deflate
    stream (zlib, the previous block's bytes as the dictionary, like makecab.exe), real CFDATA checksums (CSUMCOMPUTE);
  * a minimal PDF writer (xref table, MediaBox 612 x 792, lines and Helvetica text);
  * the OST XML builder: XML_ROOT / OST / Bid / BidPages ... in the element and attribute order of the Windows exporter
    (ost_visualizer/infrastructure/mdb/exporters/ost_exporter.py), CRLF line ends, UTF-8, no XML declaration;
  * the eight bid specifications of the pack (ten files: the Locked and the Limits fixtures have a second file).

Everything is deterministic: no clock, no random numbers (GUIDs come from uuid5 of the bid and row names). The cabinet bytes depend on
the zlib version of the machine that runs the script (deflate output is not specified across zlib versions); the DECOMPRESSED members
never do, and --check compares those when the bytes differ.

These files are generated. They were not exported by the Windows app and the Windows app has not opened them.
"""

import argparse
import hashlib
import os
import re
import struct
import sys
import uuid
import xml.etree.ElementTree as ET
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))

# =====================================================================================================================
# Microsoft Cabinet writer (MSZIP)
# =====================================================================================================================

CAB_BLOCK = 32768
CAB_SET_ID = 0x4D31  # "M1"
CAB_DATE = ((2026 - 1980) << 9) | (11 << 5) | 22  # DOS date 2026-11-22
CAB_TIME = (12 << 11) | (0 << 5) | 0  # DOS time 12:00:00
CAB_ATTR_ARCHIVE = 0x20  # _A_ARCH, what makecab writes


def cab_checksum(data, seed=0):
    """CSUMCOMPUTE of the cabinet specification: little-endian 32-bit words XORed, the 1-3 trailing bytes combined with the first one
    in the highest position."""
    value = seed
    words = len(data) // 4
    if words:
        for word in struct.unpack("<%dI" % words, data[: 4 * words]):
            value ^= word
    tail = data[4 * words:]
    if len(tail) == 3:
        value ^= (tail[0] << 16) | (tail[1] << 8) | tail[2]
    elif len(tail) == 2:
        value ^= (tail[0] << 8) | tail[1]
    elif len(tail) == 1:
        value ^= tail[0]
    return value


def mszip_block(chunk, history):
    """'CK' + a raw deflate stream of `chunk` that may refer to the last 32 KB of `history` (the bytes of the previous blocks)."""
    dictionary = history[-CAB_BLOCK:]
    if dictionary:
        compressor = zlib.compressobj(9, zlib.DEFLATED, -15, 8, zlib.Z_DEFAULT_STRATEGY, zdict=dictionary)
    else:
        compressor = zlib.compressobj(9, zlib.DEFLATED, -15, 8, zlib.Z_DEFAULT_STRATEGY)
    return b"CK" + compressor.compress(chunk) + compressor.flush(zlib.Z_FINISH)


def build_cab(members):
    """members: list of (name, bytes); one cabinet, one MSZIP folder, the members in the order given. Names are stored as given."""
    folder = b"".join(data for _name, data in members)
    file_table = b""
    offset = 0
    for name, data in members:
        name_bytes = name.encode("ascii")  # every member name of the pack is ASCII (no _A_NAME_IS_UTF flag needed)
        file_table += struct.pack("<IIHHHH", len(data), offset, 0, CAB_DATE, CAB_TIME, CAB_ATTR_ARCHIVE) + name_bytes + b"\0"
        offset += len(data)
    blocks = []
    for position in range(0, max(len(folder), 1), CAB_BLOCK):
        chunk = folder[position: position + CAB_BLOCK]
        payload = mszip_block(chunk, folder[:position])
        header = struct.pack("<HH", len(payload), len(chunk))
        checksum = cab_checksum(header, cab_checksum(payload))
        blocks.append(struct.pack("<IHH", checksum, len(payload), len(chunk)) + payload)
    data_section = b"".join(blocks)
    files_offset = 36 + 8
    data_offset = files_offset + len(file_table)
    total = data_offset + len(data_section)
    header = b"MSCF" + struct.pack("<IIIIIBBHHHHH", 0, total, 0, files_offset, 0, 3, 1, 1, len(members), 0, CAB_SET_ID, 0)
    folder_record = struct.pack("<IHH", data_offset, len(blocks), 1)  # typeCompress 1 = MSZIP
    return header + folder_record + file_table + data_section


# =====================================================================================================================
# Minimal PDF writer
# =====================================================================================================================

def pdf_num(value):
    text = ("%.2f" % value).rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


class Sheet:
    """One PDF page. Coordinates are SHEET points with the origin at the TOP LEFT and Y down (the frame the plan view uses for takeoffs);
    the writer flips Y for the PDF."""

    def __init__(self, width=612.0, height=792.0):
        self.width = width
        self.height = height
        self.ops = []

    def _y(self, y):
        return self.height - y

    def stroke_width(self, w):
        self.ops.append("%s w" % pdf_num(w))

    def gray(self, g):
        self.ops.append("%s G" % pdf_num(g))

    def dash(self, on=None, off=None):
        self.ops.append("[%s %s] 0 d" % (pdf_num(on), pdf_num(off)) if on else "[] 0 d")

    def line(self, x1, y1, x2, y2):
        self.ops.append("%s %s m %s %s l S" % (pdf_num(x1), pdf_num(self._y(y1)), pdf_num(x2), pdf_num(self._y(y2))))

    def rect(self, x1, y1, x2, y2):
        self.ops.append("%s %s %s %s re S" % (pdf_num(x1), pdf_num(self._y(y2)), pdf_num(x2 - x1), pdf_num(y2 - y1)))

    def polygon(self, points):
        parts = ["%s %s %s" % (pdf_num(x), pdf_num(self._y(y)), "m" if i == 0 else "l") for i, (x, y) in enumerate(points)]
        self.ops.append(" ".join(parts) + " h S")

    def text(self, x, y, text, size=12, bold=False):
        text.encode("ascii")
        escaped = text.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")
        self.ops.append("BT /%s %s Tf %s %s Td (%s) Tj ET" % ("F2" if bold else "F1", pdf_num(size), pdf_num(x), pdf_num(self._y(y)), escaped))

    def content(self):
        return ("\n".join(self.ops) + "\n").encode("ascii")


def build_pdf(sheet, title):
    """A valid one-page PDF 1.4: catalog, pages, page, content stream, two standard fonts, info, a classic xref table."""
    content = sheet.content()
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        ("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %s %s] /Resources << /Font << /F1 5 0 R /F2 6 0 R >> >> /Contents 4 0 R >>"
         % (pdf_num(sheet.width), pdf_num(sheet.height))).encode("ascii"),
        b"<< /Length %d >>\nstream\n" % len(content) + content + b"endstream",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>",
        ("<< /Title (%s) /Producer (make_pack.py of the M1 device test pack) >>" % title.replace("(", " ").replace(")", " ")).encode("ascii"),
    ]
    out = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = []
    for number, body in enumerate(objects, start=1):
        offsets.append(len(out))
        out += b"%d 0 obj\n" % number + body + b"\nendobj\n"
    xref_at = len(out)
    out += b"xref\n0 %d\n" % (len(objects) + 1)
    out += b"0000000000 65535 f \n"
    for offset in offsets:
        out += b"%010d 00000 n \n" % offset
    file_id = hashlib.md5(bytes(out)).hexdigest().upper().encode("ascii")
    out += b"trailer\n<< /Size %d /Root 1 0 R /Info 7 0 R /ID [<%s> <%s>] >>\n" % (len(objects) + 1, file_id, file_id)
    out += b"startxref\n%d\n%%%%EOF\n" % xref_at
    return bytes(out)


# =====================================================================================================================
# Plan drawings (sheet points, top-left origin)
# =====================================================================================================================

PLAN_A = {
    "outer": (72, 144, 540, 648),
    "vwall": ((306, 144), (306, 648)),
    "hwall": ((306, 396), (540, 396)),
    "living": (72, 144, 306, 648),
    "hole": (120, 300, 200, 380),
    "outlets": [(140, 200), (260, 600), (420, 200)],
}
PLAN_C = {"roof": (108, 180, 504, 612)}


def rect_points(box):
    x1, y1, x2, y2 = box
    return [(x1, y1), (x2, y1), (x2, y2), (x1, y2)]


def frame(sheet, sheet_id, title, scale_text, ratio_text):
    sheet.gray(0.85)
    sheet.stroke_width(0.25)
    for x in range(72, 577, 36):
        sheet.line(x, 36, x, 696)
    for y in range(72, 697, 36):
        sheet.line(36, y, 576, y)
    sheet.gray(0)
    sheet.stroke_width(1.5)
    sheet.rect(36, 36, 576, 756)
    sheet.stroke_width(0.75)
    sheet.line(36, 696, 576, 696)
    sheet.text(48, 82, sheet_id, 40, True)
    sheet.text(48, 718, title, 14, True)
    sheet.text(48, 736, scale_text, 10)
    sheet.text(330, 718, "M1 device test pack (generated)", 9)
    sheet.text(330, 736, ratio_text, 9)
    # a bar that is exactly one inch (72 points) long on the sheet
    sheet.stroke_width(2)
    sheet.line(400, 676, 472, 676)
    sheet.line(400, 670, 400, 682)
    sheet.line(472, 670, 472, 682)
    sheet.text(482, 680, "1 in on the sheet", 9)


def draw_plan_a(sheet_id, title, scale_text, ratio_text):
    s = Sheet()
    frame(s, sheet_id, title, scale_text, ratio_text)
    s.gray(0)
    s.stroke_width(4)
    x1, y1, x2, y2 = PLAN_A["outer"]
    s.rect(x1, y1, x2, y2)
    s.stroke_width(3)
    s.line(*PLAN_A["vwall"][0], *PLAN_A["vwall"][1])
    s.line(*PLAN_A["hwall"][0], *PLAN_A["hwall"][1])
    s.stroke_width(0.75)
    s.rect(*PLAN_A["hole"])
    s.text(130, 420, "LIVING", 16, True)
    s.text(380, 280, "BEDROOM", 16, True)
    s.text(380, 540, "KITCHEN", 16, True)
    s.text(122, 296, "opening", 8)
    return build_pdf(s, "%s %s" % (sheet_id, title))


def draw_plan_c(sheet_id, title, scale_text, ratio_text):
    s = Sheet()
    frame(s, sheet_id, title, scale_text, ratio_text)
    x1, y1, x2, y2 = PLAN_C["roof"]
    s.gray(0)
    s.stroke_width(3)
    s.rect(x1, y1, x2, y2)
    s.stroke_width(2)
    mid = (y1 + y2) / 2.0
    s.line(x1 + 90, mid, x2 - 90, mid)
    s.line(x1, y1, x1 + 90, mid)
    s.line(x2, y1, x2 - 90, mid)
    s.line(x1, y2, x1 + 90, mid)
    s.line(x2, y2, x2 - 90, mid)
    s.text(220, 420, "ROOF", 18, True)
    return build_pdf(s, "%s %s" % (sheet_id, title))


def draw_empty(sheet_id, title, scale_text, ratio_text):
    s = Sheet()
    frame(s, sheet_id, title, scale_text, ratio_text)
    s.gray(0.4)
    s.stroke_width(1)
    s.dash(6, 6)
    s.line(36, 96, 576, 696)
    s.line(576, 96, 36, 696)
    s.dash()
    s.gray(0)
    s.text(150, 400, "EMPTY PAGE", 36, True)
    s.text(150, 424, "no takeoffs are stored on this page", 12)
    return build_pdf(s, "%s %s" % (sheet_id, title))


# =====================================================================================================================
# OST XML (the element and attribute order of the Windows exporter)
# =====================================================================================================================

BID_SECTIONS = ["BidPlanRooms", "BidAreas", "BidTypAreas", "BidConditionFolders", "BidPageFolders", "BidSettings", "BidEmployees",
                "BidLayers", "BidTypAreaCounts", "BidConditions", "UserMasterConditions", "BidConditionUser", "BidZones",
                "BidTypGroupViews", "OCRProps"]
PAGE_SECTIONS = ["BidTakeoffs", "BidHighlights", "BidTexts", "BidDimensions", "BidArrows", "BidALines", "BidCallOuts",
                 "BidAnnotationRects", "BidAnnotationOvals", "BidAnnotationPolygons", "BidAnnotationClouds", "BidAnnoInk",
                 "BidLegends", "BidPageSettings", "BidAreaTranslations", "BidMarkedPages", "BidComments"]
TAIL_SECTIONS = ["BidNamedViews", "BidHotLinks"]
ALWAYS_SELF_CLOSE_CHILD = ("BidAreas", "BidTypAreas", "BidTypAreaCounts")  # the exporter adds an empty child of the same name

PRIORITY_ATTRS = ["UID", "BidUID", "BidPageUID", "BidConditionUID", "BidLayerUID", "BidZoneUID", "BidAreaUID", "BidTypAreaUID",
                  "AreaUID", "TypAreaUID", "ParentUID", "CdnTypeUID"]
ATTR_ORDER = {
    "Bid": ["UID", "BidProjectUID", "ParentBidUID", "OrigBidProjectUID", "OrigParentBidUID", "JobStatusUID", "EstimatorUID",
            "PrManagerUID", "JobSiteManagerUID", "SourceBidUID", "ExternalID", "QuickBidDB", "BidNo", "BidType", "JobID", "JobName",
            "ImageFolder", "Notes", "IsAccepted", "RecalcNeeded", "CreateDateTime", "ModDateTime", "PriceUsing",
            "PriceUsingDatabase", "PriceUsingWorksheet", "TakeoffIncrements", "HoursPerDay", "MeasureBase", "WeekStartDay",
            "QuantitiesInLegend", "JobSendRec", "ScaleStyle", "IsCustomScale", "ScaleFactor1", "ScaleFactor2", "PageScale",
            "PageWidth", "PageHeight", "LastReceiveDateTime", "LastSendDateTime", "DeliverEntireBid", "EstimatedDays", "Percent",
            "ProjectOver", "DPCMode", "IsDPCUpdated", "IgnoreBidAreas", "SendImageFiles", "HasSTSClash", "SRPending", "IsUnlocked",
            "FullBidSent", "TypicalType", "GUID", "BidDate", "CopyFromBidNO", "CopyTimestamp", "CoverSheetSelItemType",
            "CoverSheetSelItemUID", "LegendFlags", "IsCalculatedForSlope", "IsCalculatedForLaborCostCodeTotals"],
    "BidCondition": ["UID", "BidUID", "BidConditionFolderUID", "BidLayerUID", "CdnTypeUID", "ExternalID", "GUID", "RefNo", "Name",
                     "Notes", "Type", "Shape", "Pattern", "ColorLine", "ColorFill", "Width", "Height", "Spacing", "Thickness", "Rise",
                     "Run", "Depth", "UOM1", "UOM2", "UOM3", "RoundUp", "Backout", "DropRun", "DropValue", "Grid", "GridSize1",
                     "GridSize2", "Gap", "Connect", "ConnectTolerance", "Trim", "Curve", "SnapToGrid", "SnapToLinear",
                     "SnapToLinearTolerance", "ManualLength", "MatAmount", "LabAmount", "SubAmount", "DirectQuantity1",
                     "DirectQuantity2", "DirectQuantity3", "FontName", "FontSize", "FontBold", "FontItalic", "DisplayDimension",
                     "RoundQuantity", "Quantity1", "Quantity2", "Quantity3", "IsTemplate", "ExcelCell1", "ExcelCell2", "ExcelCell3",
                     "IsCurvedSegment", "OpenExternalID", "DisplayGridWhileDrawing", "TypGroupUID", "ShowTakeoff", "DisplaySize",
                     "DisplayName"],
    "BidPage": ["UID", "BidUID", "BidPageFolderUID", "Name", "ImagePath", "OverlayImagePath", "Show", "RasterDrawMethod", "ScaleStyle",
                "IsCustomScale", "ScaleFactor1", "ScaleFactor2", "Scale", "Width", "Height", "ZoomFac", "CurrentX", "CurrentY",
                "FlipX", "FlipY", "Rotation", "DeskewRotation", "OverlayOffsetX", "OverlayOffsetY", "OverlayRotation",
                "MultiPageCount", "Index1", "Sequence", "Invert", "Bitonal", "DigitizerNX1", "DigitizerNY1", "DigitizerNX2",
                "DigitizerNY2", "DigitizerWidth", "DigitizerHeight", "DigitizerResX", "DigitizerResY", "WasSent", "MasterPageUID",
                "TypicalPageRepeats", "GUID", "OverlayRect", "OverlayResized", "DeskewRotationOverlay", "ZoomFlag", "SheetNo",
                "OCRState", "OCRUID"],
    "BidTakeoff": ["UID", "BidUID", "BidConditionUID", "BidZoneUID", "BidPageUID", "BidAreaUID", "BidTypAreaUID", "ParentUID", "No",
                   "Quantity", "Count", "Rotation", "Position", "GridOffsetX", "GridOffsetY", "GridRotation", "IsNegativeQuantity",
                   "FontName", "FontColor", "FontSize", "FontBold", "FontItalic", "FontUnderline", "TypGroupTakeoffUID",
                   "TypPageTakeoffUID", "TakeoffModified", "TypGroupUID", "TypGroupMarkerUID", "FlipX", "FlipY", "GUID",
                   "NameFontName", "NameFontColor", "NameFontSize", "NameFontBold", "NameFontItalic", "NameFontUnderline", "Curve"],
    "BidLayer": ["UID", "BidUID", "IsTemplate", "Name", "Show", "IsLocked", "Sequence"],
    "BidSetting": ["UID", "BidUID", "BidPageSelectedUID", "STSGUID", "STSServerName", "STSClientName"],
    "BidConditionFolder": ["UID", "BidUID", "ParentUID", "Name", "Description", "ExpandState"],
    "BidLegend": ["UID", "BidUID", "BidPageUID", "Position", "Rotation", "FontName", "FontColor", "FontSize", "FontBold",
                  "FontItalic", "FontUnderline", "IsShowTotals", "MoveToCorner"],
    "BidText": ["UID", "BidUID", "BidPageUID", "BidLayerUID", "Name", "FontName", "FontColor", "FontSize", "FontBold", "FontItalic",
                "FontUnderline", "TextAlign", "Position"],
}


def float_text(value):
    """serialize_value of the exporter for a float: an integer value without a point, otherwise 15 digits with trailing zeros cut."""
    if value == int(value) and abs(value) < 1e15:
        return str(int(value))
    return ("%.15f" % value).rstrip("0").rstrip(".")


def cell(value):
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, float):
        return float_text(value)
    return str(value)


def escape_attr(text):
    """_escape_xml_attr of the exporter."""
    if not text:
        return text
    return (text.replace("&", "&amp;").replace("'", "&apos;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")
            .replace("\r", "&#xd;").replace("\n", "&#xa;"))


def sorted_attrs(attrs, element):
    order = ATTR_ORDER.get(element, PRIORITY_ATTRS)
    ordered = [(key, attrs[key]) for key in order if key in attrs]
    ordered += [(key, attrs[key]) for key in sorted(attrs) if key not in order]
    return ordered


def element(tag, attrs=(), children=None, newline="\r\n"):
    attr_text = "".join(' %s="%s"' % (key, escape_attr(value)) for key, value in attrs)
    if not children:
        return "<%s%s/>%s" % (tag, attr_text, newline)
    return "<%s%s>%s%s</%s>%s" % (tag, attr_text, newline, "".join(children), tag, newline)


def guid(*parts):
    return "{" + str(uuid.uuid5(uuid.NAMESPACE_URL, "m1-device-test-pack/" + "/".join(str(p) for p in parts))).upper() + "}"


def trimmed3(value):
    """The position text of the exporter / writer: three decimals, trailing zeros cut."""
    text = ("%.3f" % value).rstrip("0").rstrip(".")
    return "0" if text in ("", "-0") else text


# =====================================================================================================================
# Bid specification
# =====================================================================================================================

COLORS = [(230, 60, 60), (60, 170, 80), (60, 100, 220), (240, 150, 40), (150, 80, 190), (40, 170, 170), (255, 192, 203)]
CREATED = "2026 11 22 9 0 0"  # the OST datetime form: six integers


def bgr(rgb):
    return rgb[0] | (rgb[1] << 8) | (rgb[2] << 16)


def mm(value):
    return value / 25.4


# per style defaults, in inches, like TYPE_DEFAULTS of the Windows Edit Condition dialog (imperial)
KIND_IMPERIAL = {
    "linear": {"Type": 0, "Shape": -1, "Spacing": 4.0, "Thickness": 4.0, "Height": 96.0, "UOM1": 2, "Quantity1": 1},
    "area": {"Type": 1, "Shape": -1, "Spacing": 6.0, "Thickness": 4.0, "Height": 0.0, "UOM1": 5, "Quantity1": 11,
             "DisplayGridWhileDrawing": 1},
    "count": {"Type": 2, "Shape": 0, "Spacing": 4.0, "Thickness": 4.0, "Height": 0.0, "UOM1": 0, "Quantity1": 23},
}
# metric: the dialog shows millimetres, the database keeps inches (mm / 25.4); first units m, m2
KIND_METRIC = {
    "linear": {"Type": 0, "Shape": -1, "Spacing": mm(100.0), "Thickness": mm(100.0), "Height": mm(2700.0), "Width": mm(300.0),
               "UOM1": 13, "Quantity1": 1},
    "area": {"Type": 1, "Shape": -1, "Spacing": mm(150.0), "Thickness": mm(150.0), "Height": 0.0, "Width": mm(300.0), "UOM1": 14,
             "Quantity1": 11, "DisplayGridWhileDrawing": 1},
    "count": {"Type": 2, "Shape": 0, "Spacing": mm(100.0), "Thickness": mm(100.0), "Height": 0.0, "Width": mm(300.0), "UOM1": 0,
              "Quantity1": 23},
}
STATUS_UID = 9001  # the UID of the one JobStatuses row of a locked / control bid (global master data, remapped on import)
CONDITION_TYPE_NAME = {0: "linear", 1: "area", 2: "count", 3: "attachment"}

SHARED_LAYERS = [("Image", 0), ("Annotation", 1), ("Default", 2), ("Comments", 3)]  # Config seed of the app database (OstSchema DEFAULT_LAYERS)


class Bid:
    """One bid and everything the OST file of it carries. UIDs are allocated 2, 3, 4 ... (the bid is 1), unique over the whole file."""

    def __init__(self, stem, name, purpose, measure_base=0, status=None):
        self.stem = stem
        self.name = name
        self.purpose = purpose
        self.measure_base = measure_base
        self.status = status  # None or (status name, locked flag)
        self.uid = 1
        self._next = 2
        self.folders = []  # dicts: uid, parent, name
        self.layers = []  # dicts: uid, name, show, locked, template, sequence
        self.conditions = []  # dicts: uid, name, kind, ref_no, folder, layer, row overrides
        self.pages = []  # dicts: uid, name, ...
        self.takeoffs = []  # dicts: uid, page, cond, pts, parent
        self.legends = []
        self.texts = []
        self.pdfs = {}  # member name -> bytes, in insertion order
        self.setting_uid = self.alloc()  # the one BidSettings row
        self.selected_page = None
        self._kind_table = KIND_METRIC if measure_base == 1 else KIND_IMPERIAL
        self.bid_scale = (1, 0, 0.125, 12.0)  # ScaleStyle, IsCustomScale, ScaleFactor1, ScaleFactor2 of the bid row

    def alloc(self):
        value = self._next
        self._next += 1
        return value

    # ---- rows ----
    def folder(self, name, parent=None):
        row = {"uid": self.alloc(), "parent": parent, "name": name}
        self.folders.append(row)
        return row["uid"]

    def layer(self, name, show=True, locked=False, template=False, sequence=None):
        if sequence is None:
            sequence = 4 + len(self.layers)  # above the four seeded shared layers (0..3), no ties
        row = {"uid": self.alloc(), "name": name, "show": show, "locked": locked, "template": template, "sequence": sequence}
        self.layers.append(row)
        return row["uid"]

    def condition(self, name, kind, folder=None, layer=None, ref_no=None, backout=False, **over):
        if ref_no is None:
            ref_no = max([c["ref_no"] for c in self.conditions] + [0]) + 1
        color = COLORS[len(self.conditions) % len(COLORS)]
        row = {"uid": self.alloc(), "name": name, "kind": kind, "ref_no": ref_no, "folder": folder, "layer": layer, "color": bgr(color),
               "backout": backout, "over": over}
        self.conditions.append(row)
        return row["uid"]

    def page(self, name, pdf_key, pdf_bytes, scale, custom=False, style=1, member=None):
        """scale = (ScaleFactor1, ScaleFactor2). The PDF member is shared by pages that use the same pdf_key."""
        member = member or "%s-%s.pdf" % (self.stem.lower(), pdf_key)
        self.pdfs.setdefault(member, pdf_bytes)
        row = {"uid": self.alloc(), "name": name, "member": member, "sf1": scale[0], "sf2": scale[1], "custom": custom, "style": style,
               "sequence": len(self.pages) + 1}
        self.pages.append(row)
        if self.selected_page is None:
            self.selected_page = row["uid"]
        return row["uid"]

    def takeoff(self, page_uid, cond_uid, points, parent=None):
        """points: sheet points (top-left origin, Y down) of the takeoff: 1 point (count), 2 points (linear), 3 or more (area)."""
        number = 1 + sum(1 for t in self.takeoffs if t["cond"] == cond_uid)
        row = {"uid": self.alloc(), "page": page_uid, "cond": cond_uid, "pts": list(points), "parent": parent, "no": number}
        self.takeoffs.append(row)
        return row["uid"]

    def legend(self, page_uid):
        self.legends.append({"uid": self.alloc(), "page": page_uid})

    def text_note(self, page_uid, layer_uid, text):
        self.texts.append({"uid": self.alloc(), "page": page_uid, "layer": layer_uid, "text": text})

    # ---- derived ----
    def page_by_uid(self, uid):
        return next(p for p in self.pages if p["uid"] == uid)

    def condition_by_uid(self, uid):
        return next(c for c in self.conditions if c["uid"] == uid)

    def ratio(self, page):
        return page["sf2"] / page["sf1"]

    def position_text(self, takeoff):
        page = self.page_by_uid(takeoff["page"])
        ratio = self.ratio(page)
        values = []
        for x, y in takeoff["pts"]:
            values.append(trimmed3(x / 72.0 * ratio))
            values.append(trimmed3(y / 72.0 * ratio))
        return ";".join(values)

    # ---- XML ----
    def _bid_row(self):
        style, custom, sf1, sf2 = self.bid_scale
        status_uid = STATUS_UID if self.status else 0
        row = {
            "UID": self.uid, "BidProjectUID": 0, "ParentBidUID": 0, "OrigBidProjectUID": 0, "OrigParentBidUID": 0,
            "JobStatusUID": status_uid, "EstimatorUID": 0, "PrManagerUID": 0, "JobSiteManagerUID": 0, "SourceBidUID": "",
            "ExternalID": 0, "QuickBidDB": "", "BidNo": 1, "BidType": 0, "JobID": self.stem.split("-")[0] + "-" + self.stem.split("-")[1],
            "JobName": self.name, "ImageFolder": "", "Notes": self.purpose, "IsAccepted": 0, "RecalcNeeded": 0,
            "CreateDateTime": CREATED, "ModDateTime": CREATED, "PriceUsing": 0, "PriceUsingDatabase": "", "PriceUsingWorksheet": "",
            "TakeoffIncrements": 25 if self.measure_base == 1 else 1, "HoursPerDay": 8, "MeasureBase": self.measure_base, "WeekStartDay": 0, "QuantitiesInLegend": 1,
            "JobSendRec": 0, "ScaleStyle": style, "IsCustomScale": custom, "ScaleFactor1": sf1, "ScaleFactor2": sf2, "PageScale": 1,
            "PageWidth": 42, "PageHeight": 30, "LastReceiveDateTime": "", "LastSendDateTime": "", "DeliverEntireBid": 0,
            "EstimatedDays": 0, "Percent": 0, "ProjectOver": 0, "DPCMode": 0, "IsDPCUpdated": 0, "IgnoreBidAreas": 0,
            "SendImageFiles": 0, "HasSTSClash": 0, "SRPending": 0, "IsUnlocked": 0, "FullBidSent": 0, "TypicalType": 0,
            "GUID": guid(self.stem, "bid"), "BidDate": "", "CopyFromBidNO": 0, "CopyTimestamp": "", "CoverSheetSelItemType": 0,
            "CoverSheetSelItemUID": 0, "LegendFlags": 0, "IsCalculatedForSlope": 0, "IsCalculatedForLaborCostCodeTotals": 0,
        }
        return {key: cell(value) for key, value in row.items()}

    def _folder_rows(self):
        rows = []
        for f in self.folders:
            row = {"UID": f["uid"], "BidUID": self.uid, "ParentUID": f["parent"] or 0, "Name": f["name"], "Description": "",
                   "ExpandState": 1}
            rows.append({key: cell(value) for key, value in row.items()})
        return rows

    def _layer_rows(self):
        rows = [{key: cell(value) for key, value in {"UID": l["uid"], "BidUID": self.uid, "IsTemplate": l["template"], "Name": l["name"],
                                                     "Show": l["show"], "IsLocked": l["locked"], "Sequence": l["sequence"]}.items()}
                for l in self.layers]
        rows.sort(key=lambda r: -int(r["UID"]))  # the exporter lists BidLayers newest first
        return rows

    def _condition_rows(self):
        rows = []
        for c in self.conditions:
            base = dict(self._kind_table[c["kind"]])
            width = base.pop("Width", 12.0)
            row = {
                "UID": c["uid"], "BidUID": self.uid, "BidConditionFolderUID": c["folder"] or 0, "BidLayerUID": c["layer"] or 0,
                "CdnTypeUID": 0, "ExternalID": 0, "GUID": guid(self.stem, "condition", c["uid"]), "RefNo": c["ref_no"],
                "Name": c["name"], "Notes": "", "Type": 0, "Shape": -1, "Pattern": 8, "ColorLine": 0, "ColorFill": c["color"],
                "Width": width, "Height": 0.0, "Spacing": 4.0, "Thickness": 4.0, "Rise": 0.0, "Run": 0.0, "Depth": 0.0, "UOM1": 0,
                "UOM2": -1, "UOM3": -1, "RoundUp": 0.0, "Backout": 1 if c["backout"] else 0, "DropRun": 0, "DropValue": 0.0, "Grid": 0,
                "GridSize1": 0.0, "GridSize2": 0.0, "Gap": 0.0, "Connect": 0, "ConnectTolerance": 6.0, "Trim": 0, "Curve": 0,
                "SnapToGrid": 0, "SnapToLinear": 0, "SnapToLinearTolerance": 0.0, "ManualLength": 0, "MatAmount": 0.0, "LabAmount": 0.0,
                "SubAmount": 0.0, "DirectQuantity1": 0.0, "DirectQuantity2": 0.0, "DirectQuantity3": 0.0, "FontName": "Arial",
                "FontSize": -16, "FontBold": 0, "FontItalic": 0, "DisplayDimension": 0, "RoundQuantity": 0, "Quantity1": 0,
                "Quantity2": 0, "Quantity3": 0, "IsTemplate": 0, "ExcelCell1": "", "ExcelCell2": "", "ExcelCell3": "",
                "IsCurvedSegment": 0, "OpenExternalID": 0, "DisplayGridWhileDrawing": 0, "TypGroupUID": 0, "ShowTakeoff": 1,
                "DisplaySize": 100.0, "DisplayName": 0,
            }
            row.update(base)
            row.update(c["over"])
            rows.append({key: cell(value) for key, value in row.items()})
        rows.sort(key=lambda r: int(r["UID"]))
        return rows

    def _page_rows(self):
        rows = []
        for p in self.pages:
            row = {
                "UID": p["uid"], "BidUID": self.uid, "BidPageFolderUID": 0, "Name": p["name"],
                "ImagePath": "TempImages!.tmp\\" + p["member"], "OverlayImagePath": "", "Show": 0, "RasterDrawMethod": 0,
                "ScaleStyle": p["style"], "IsCustomScale": 1 if p["custom"] else 0, "ScaleFactor1": p["sf1"], "ScaleFactor2": p["sf2"],
                "Scale": 1, "Width": 8.5, "Height": 11.0, "ZoomFac": 1, "CurrentX": 0, "CurrentY": 0, "FlipX": 0, "FlipY": 0,
                "Rotation": 0, "DeskewRotation": 0, "OverlayOffsetX": 0, "OverlayOffsetY": 0, "OverlayRotation": 0, "MultiPageCount": 1,
                "Index1": 1, "Sequence": p["sequence"], "Invert": 0, "Bitonal": 0, "DigitizerNX1": 0, "DigitizerNY1": 0,
                "DigitizerNX2": 0, "DigitizerNY2": 0, "DigitizerWidth": 0, "DigitizerHeight": 0, "DigitizerResX": 0, "DigitizerResY": 0,
                "WasSent": 0, "MasterPageUID": 0, "TypicalPageRepeats": 0, "GUID": guid(self.stem, "page", p["uid"]), "OverlayRect": "",
                "OverlayResized": "", "DeskewRotationOverlay": 0, "ZoomFlag": 0, "SheetNo": "", "OCRState": 0, "OCRUID": 0,
            }
            rows.append({key: cell(value) for key, value in row.items()})
        rows.sort(key=lambda r: int(r["Sequence"]))
        return rows

    def _takeoff_rows(self, page_uid):
        rows = []
        for t in self.takeoffs:
            if t["page"] != page_uid:
                continue
            kind = CONDITION_TYPE_NAME[self._kind_table[self.condition_by_uid(t["cond"])["kind"]]["Type"]]
            row = {
                "UID": t["uid"], "BidUID": self.uid, "BidConditionUID": t["cond"], "BidZoneUID": 0, "BidPageUID": page_uid,
                "BidAreaUID": 0, "BidTypAreaUID": 0, "ParentUID": t["parent"] or 0, "No": t["no"], "Quantity": 0.0,
                "Count": 1.0 if kind == "count" else 0.0, "Rotation": 0.0, "Position": self.position_text(t), "GridOffsetX": 0.0,
                "GridOffsetY": 0.0, "GridRotation": 0.0, "IsNegativeQuantity": 0, "FontName": "Arial", "FontColor": 0, "FontSize": -16,
                "FontBold": 0, "FontItalic": 0, "FontUnderline": 0, "TypGroupTakeoffUID": 0, "TypPageTakeoffUID": 0,
                "TakeoffModified": 0, "TypGroupUID": 0, "TypGroupMarkerUID": 0, "FlipX": 0, "FlipY": 0,
                "GUID": guid(self.stem, "takeoff", t["uid"]), "NameFontName": "Arial", "NameFontColor": 0, "NameFontSize": -16,
                "NameFontBold": 0, "NameFontItalic": 0, "NameFontUnderline": 0, "Curve": -1,
            }
            rows.append({key: cell(value) for key, value in row.items()})
        rows.sort(key=lambda r: -int(r["UID"]))
        return rows

    def _legend_rows(self, page_uid):
        rows = []
        for g in self.legends:
            if g["page"] != page_uid:
                continue
            row = {"UID": g["uid"], "BidUID": self.uid, "BidPageUID": page_uid,
                   "Position": '<Legends dX="72" dY="72"><Legend dX="0" dY="0"/></Legends>', "Rotation": 0.0, "FontName": "Arial",
                   "FontColor": 0, "FontSize": -12, "FontBold": 0, "FontItalic": 0, "FontUnderline": 0, "IsShowTotals": 1,
                   "MoveToCorner": 0}
            rows.append({key: cell(value) for key, value in row.items()})
        return rows

    def _text_rows(self, page_uid):
        rows = []
        for t in self.texts:
            if t["page"] != page_uid:
                continue
            row = {"UID": t["uid"], "BidUID": self.uid, "BidPageUID": page_uid, "BidLayerUID": t["layer"] or 0, "Name": t["text"],
                   "FontName": "Arial", "FontColor": 0, "FontSize": -12, "FontBold": 0, "FontItalic": 0, "FontUnderline": 0,
                   "TextAlign": 0, "Position": "40;40;300;70"}
            rows.append({key: cell(value) for key, value in row.items()})
        return rows

    def to_xml(self):
        bid_children = []
        for section in BID_SECTIONS:
            if section == "BidEmployees":
                continue  # the exporter skips an empty BidEmployees
            if section == "BidConditionFolders":
                rows, item = self._folder_rows(), "BidConditionFolder"
            elif section == "BidLayers":
                rows, item = self._layer_rows(), "BidLayer"
            elif section == "BidSettings":
                rows, item = [], "BidSetting"
                if self.pages:
                    rows = [{"UID": cell(self.setting_uid), "BidUID": cell(self.uid), "BidPageSelectedUID": cell(self.selected_page or 0)}]
            elif section == "BidConditions":
                children = []
                for row in self._condition_rows():
                    children.append(element("BidCondition", sorted_attrs(row, "BidCondition"), [element("BidAreaConditions")]))
                bid_children.append(element("BidConditions", (), children))
                continue
            else:
                rows, item = [], section[:-1]
            children = [element(item, sorted_attrs(row, item)) for row in rows]
            if section in ALWAYS_SELF_CLOSE_CHILD:
                children.append(element(section))
            bid_children.append(element(section, (), children))
        page_children = []
        for page_row in self._page_rows():
            page_uid = int(page_row["UID"])
            sections = []
            for section in PAGE_SECTIONS:
                if section == "BidTakeoffs":
                    rows, item = self._takeoff_rows(page_uid), "BidTakeoff"
                elif section == "BidLegends":
                    rows, item = self._legend_rows(page_uid), "BidLegend"
                elif section == "BidTexts":
                    rows, item = self._text_rows(page_uid), "BidText"
                else:
                    rows, item = [], section[:-1]
                sections.append(element(section, (), [element(item, sorted_attrs(row, item)) for row in rows]))
            page_children.append(element("BidPage", sorted_attrs(page_row, "BidPage"), sections))
        bid_children.append(element("BidPages", (), page_children))
        for section in TAIL_SECTIONS:
            bid_children.append(element(section))
        root_children = [element("OST", [("Version", "3.2.0")]), element("Bid", sorted_attrs(self._bid_row(), "Bid"), bid_children)]
        if self.status:
            status_name, locked = self.status
            row = {"UID": str(STATUS_UID), "Locked": cell(bool(locked)), "Name": status_name, "Sequence": "1"}
            root_children.append(element("JobStatuses", (), [element("JobStatuse", sorted_attrs(row, "JobStatuse"))]))
        return element("XML_ROOT", (), root_children)

    def build_package(self):
        xml = self.to_xml().encode("utf-8")
        members = [(self.stem + ".ost", xml)]
        for member, data in self.pdfs.items():
            members.append(("TempImages!.tmp\\" + member, data))
        return build_cab(members), members

    # ---- facts: what the README promises, derived from the specification ----
    def folder_path(self, uid):
        names = []
        while uid:
            row = next(f for f in self.folders if f["uid"] == uid)
            names.append(row["name"])
            uid = row["parent"]
        return " > ".join(reversed(names))

    def merged_layer_names(self):
        """merge_layers_for_bid + the Comments drop of the Windows rule, over the seeded shared layers and this bid's own layers."""
        rows = [{"name": n, "seq": s, "template": True} for n, s in SHARED_LAYERS]
        rows += [{"name": l["name"], "seq": l["sequence"], "template": l["template"]} for l in self.layers]
        merged = {}
        for row in sorted(rows, key=lambda r: (not r["template"], r["seq"])):
            merged[row["name"].lower()] = row
        ordered = sorted(merged.values(), key=lambda r: r["seq"])
        return [r["name"] for r in ordered if r["name"].strip().lower() != "comments"]

    def layer_uses(self, layer_uid):
        uses = {}
        count = sum(1 for c in self.conditions if c["layer"] == layer_uid)
        if count:
            uses["BidConditions"] = count
        count = sum(1 for t in self.texts if t["layer"] == layer_uid)
        if count:
            uses["BidTexts"] = count
        return uses

    def facts(self):
        out = []
        locked = 1 if (self.status and self.status[1]) else 0
        max_ref = max([c["ref_no"] for c in self.conditions] + [0])
        out.append(["BID", self.stem, self.name, self.measure_base, locked, len(self.pages), len(self.conditions), len(self.folders),
                    len(self.layers), len(self.takeoffs), max_ref, self.status[0] if self.status else ""])
        for p in self.pages:
            counts = {"area": 0, "linear": 0, "count": 0, "hole": 0}
            for t in self.takeoffs:
                if t["page"] != p["uid"]:
                    continue
                kind = CONDITION_TYPE_NAME[self._kind_table[self.condition_by_uid(t["cond"])["kind"]]["Type"]]
                counts["hole" if t["parent"] else kind] += 1
            out.append(["PAGE", self.stem, p["name"], cell(p["sf1"]), cell(p["sf2"]), cell(p["custom"]), counts["area"], counts["linear"],
                        counts["count"], counts["hole"], sum(1 for g in self.legends if g["page"] == p["uid"]),
                        sum(1 for t in self.texts if t["page"] == p["uid"]), p["member"]])
        for f in self.folders:
            direct = sum(1 for c in self.conditions if c["folder"] == f["uid"])
            subs = sum(1 for g in self.folders if g["parent"] == f["uid"])
            out.append(["FOLDER", self.stem, self.folder_path(f["uid"]), direct, subs])
        for c in self.conditions:
            layer = next((l["name"] for l in self.layers if l["uid"] == c["layer"]), "")
            folder = self.folder_path(c["folder"]) if c["folder"] else ""
            kind_type = self._kind_table[c["kind"]]["Type"]
            out.append(["COND", self.stem, c["ref_no"], c["name"], kind_type, folder, layer,
                        sum(1 for t in self.takeoffs if t["cond"] == c["uid"])])
        for l in self.layers:
            uses = self.layer_uses(l["uid"])
            out.append(["LAYER", self.stem, l["name"], cell(l["show"]), cell(l["locked"]), cell(l["template"]),
                        ",".join("%s:%d" % kv for kv in sorted(uses.items()))])
        out.append(["MERGED", self.stem, "|".join(self.merged_layer_names())])
        for t in self.takeoffs:
            page = self.page_by_uid(t["page"])
            cond = self.condition_by_uid(t["cond"])
            out.append(["TOPTS", self.stem, page["name"], cond["name"], "hole" if t["parent"] else "", " ".join(
                "%s,%s" % (trimmed3(x), trimmed3(y)) for x, y in t["pts"])])
        for member, data in self.pdfs.items():
            out.append(["IMAGE", self.stem, member, hashlib.sha256(data).hexdigest(), len(data)])
        return out


# =====================================================================================================================
# The ten bids
# =====================================================================================================================

def scale_texts(sf1, sf2, custom=False):
    ratio = sf2 / sf1
    if custom:
        label = "SCALE %s : %s (custom)" % (float_text(sf1), float_text(sf2))
    elif sf2 == 12.0:
        fractions = {0.25: "1/4", 0.125: "1/8", 0.09375: "3/32"}
        label = 'SCALE %s" = 1\' 0"' % fractions.get(sf1, float_text(sf1))
    else:
        label = "SCALE %s : %s" % (float_text(sf1), float_text(sf2))
    return label, "ratio %s (1 inch on the sheet = %s real units)" % (float_text(ratio), float_text(ratio))


def pdf_a(sheet_id, title, scale):
    return draw_plan_a(sheet_id, title, *scale_texts(*scale))


def add_plan_a_takeoffs(bid, page, wall, slab, outlets, hole_cond=None):
    """Takeoffs on the three-room plan A: slab = the living room, walls = the two inner walls, outlets = three points."""
    created = {}
    if slab:
        created["slab"] = bid.takeoff(page, slab, rect_points(PLAN_A["living"]))
    if hole_cond and slab:
        created["hole"] = bid.takeoff(page, hole_cond, rect_points(PLAN_A["hole"]), parent=created["slab"])
    if wall:
        bid.takeoff(page, wall, list(PLAN_A["vwall"]))
        bid.takeoff(page, wall, list(PLAN_A["hwall"]))
    if outlets:
        for point in PLAN_A["outlets"]:
            bid.takeoff(page, outlets, [point])
    return created


def fixture_folders():
    b = Bid("M1-01-folders", "M1-01 Folders",
            "M1 device test pack fixture: nested condition folders, empty folders, names at the 50-character limit, case siblings.")
    b.bid_scale = (1, 0, 0.25, 12.0)
    page = b.page("Folders Plan", "a101", pdf_a("A-101", "FOLDERS PLAN", (0.25, 12.0)), (0.25, 12.0))
    # root conditions
    root_wall = b.condition("Exterior Wall (root)", "linear")
    b.condition("Interior Partition (root)", "linear")
    root_slab = b.condition("Floor Area (root)", "area")
    root_count = b.condition("Outlet (root)", "count")
    # depth 3: Commercial > Division 03 Concrete > Footings
    f1 = b.folder("Commercial")
    f2 = b.folder("Division 03 Concrete", f1)
    f3 = b.folder("Footings", f2)
    b.condition("Strip Footing", "linear", folder=f3)
    b.condition("Pad Footing", "count", folder=f3)
    # one folder with a condition
    doors = b.folder("Doors")
    b.condition("Door 3-0", "count", folder=doors)
    b.condition("Door 6-0 Pair", "count", folder=doors)
    # a folder that has only a sub-folder holding a condition
    only_sub = b.folder("Only Sub-folder")
    holds = b.folder("Holds the Condition", only_sub)
    b.condition("Deep Condition", "count", folder=holds)
    # an empty folder, and an empty parent with two empty sub-folders
    b.folder("Empty Folder")
    parent = b.folder("Empty Parent")
    b.folder("Empty Child One", parent)
    b.folder("Empty Child Two", parent)
    # two siblings that differ only in case
    upper = b.folder("Concrete")
    b.folder("concrete")
    b.condition("Slab on Grade", "area", folder=upper)
    # names around the 50-character limit
    name50 = ("F50 fifty characters 1234567890123456789012345678901234567890")[:50]
    name49 = ("F49 forty-nine chars 1234567890123456789012345678901234567890")[:49]
    assert len(name50) == 50 and len(name49) == 49, (len(name50), len(name49))
    long_folder = b.folder(name50)
    b.condition("Condition in the 50 character folder", "linear", folder=long_folder)
    b.folder(name49)
    # takeoffs: a root linear and an area inside a folder, so a move can be watched on the plan
    b.takeoff(page, root_wall, list(PLAN_A["vwall"]))
    b.takeoff(page, root_count, [PLAN_A["outlets"][0]])
    b.takeoff(page, root_slab, rect_points(PLAN_A["living"]))
    return b


def fixture_layers():
    b = Bid("M1-02-layers", "M1-02 Layers",
            "M1 device test pack fixture: bid layers used and unused, hidden, locked copy, used only by a text; the shared layers come from the app.")
    b.bid_scale = (1, 0, 0.25, 12.0)
    page = b.page("Layers Plan", "a101", pdf_a("A-101", "LAYERS PLAN", (0.25, 12.0)), (0.25, 12.0))
    used = b.layer("M1 Used")
    b.layer("M1 Unused")
    hidden = b.layer("M1 Hidden", show=False)
    b.layer("M1 Locked Copy", locked=True)
    text_only = b.layer("M1 Text Only")
    second = b.layer("M1 Second")
    wall = b.condition("Wall on M1 Used", "linear", layer=used)
    slab = b.condition("Slab on M1 Used", "area", layer=used)
    hidden_wall = b.condition("Wall on M1 Hidden", "linear", layer=hidden)
    count = b.condition("Count on M1 Second", "count", layer=second)
    b.condition("Wall without a layer", "linear")
    add_plan_a_takeoffs(b, page, wall, slab, count)
    b.takeoff(page, hidden_wall, [(72, 400), (306, 400)])
    b.text_note(page, text_only, "Only this text uses layer M1 Text Only")
    return b


def lock_fixture(stem, name, locked):
    status = ("M1 Locked Status", True) if locked else ("M1 Unlocked Status", False)
    purpose = ("M1 device test pack fixture: the job status is Locked (every new write must say 'This bid is locked.')." if locked else
               "M1 device test pack control: the same bid with a job status whose Locked flag is cleared (every write must work).")
    b = Bid(stem, name, purpose, status=status)
    b.bid_scale = (1, 0, 0.25, 12.0)
    p1 = b.page("Plan 1", "a101", pdf_a("A-101", "PLAN 1", (0.25, 12.0)), (0.25, 12.0))
    b.page("Plan 2", "a102", draw_plan_c("A-102", "PLAN 2", *scale_texts(0.25, 12.0)), (0.25, 12.0))
    layer_a = b.layer("Layer A")
    b.layer("Layer B")
    wall = b.condition("Wall", "linear", layer=layer_a)
    slab = b.condition("Slab", "area")
    folder = b.folder("Folder")
    count = b.condition("Count in Folder", "count", folder=folder)
    add_plan_a_takeoffs(b, p1, wall, slab, count)
    return b


def fixture_pages():
    b = Bid("M1-04-pages", "M1-04 Pages",
            "M1 device test pack fixture: three pages, takeoffs of every type, an empty page, a legend, three different scales.")
    b.bid_scale = (1, 1, 0.25, 8.0)
    scale1, scale2, scale3 = (0.25, 8.0), (0.25, 12.0), (0.09375, 12.0)
    p1 = b.page("Page 1 A-101 First Floor", "a101", draw_plan_a("A-101", "FIRST FLOOR PLAN", *scale_texts(*scale1, custom=True)), scale1,
                custom=True)
    b.page("Page 2 A-102 Empty", "a102", draw_empty("A-102", "EMPTY SHEET", *scale_texts(*scale2)), scale2)
    p3 = b.page("Page 3 A-103 Roof", "a103", draw_plan_c("A-103", "ROOF PLAN", *scale_texts(*scale3)), scale3)
    wall = b.condition("Wall 2x4", "linear")
    slab = b.condition("Floor Slab", "area")
    outlets = b.condition("Outlets", "count")
    roof = b.condition("Roof Area", "area")
    # the hole is inserted with the condition of its parent (Backout mode of the Windows app places with the parent's condition)
    add_plan_a_takeoffs(b, p1, wall, slab, outlets, hole_cond=slab)
    b.takeoff(p3, roof, rect_points(PLAN_C["roof"]))
    b.legend(p3)
    return b


def fixture_one_page():
    b = Bid("M1-05-one-page", "M1-05 One Page", "M1 device test pack fixture: a bid with exactly one page (Delete Page must be refused).")
    b.bid_scale = (1, 0, 0.125, 12.0)
    page = b.page("Only Page", "a101", pdf_a("A-101", "THE ONLY PAGE", (0.125, 12.0)), (0.125, 12.0))
    wall = b.condition("Wall", "linear")
    count = b.condition("Outlets", "count")
    add_plan_a_takeoffs(b, page, wall, None, count)
    return b


def fixture_metric():
    b = Bid("M1-06-metric", "M1-06 Metric",
            "M1 device test pack fixture: a metric bid (MeasureBase 1, scale 1 : 50, conditions in metres).", measure_base=1)
    b.bid_scale = (3, 0, 1.0, 50.0)
    page = b.page("Metric Plan", "a101", draw_plan_a("A-101", "METRIC PLAN", *scale_texts(1.0, 50.0)), (1.0, 50.0), style=3)
    wall = b.condition("Wall 100 mm", "linear")
    slab = b.condition("Slab 150 mm", "area")
    sockets = b.condition("Socket 300 mm", "count")
    add_plan_a_takeoffs(b, page, wall, slab, sockets)
    return b


def limits_fixture(stem, name, top_ref):
    b = Bid(stem, name, "M1 device test pack fixture: the highest condition number of the bid is %d." % top_ref)
    b.bid_scale = (1, 0, 0.125, 12.0)
    page = b.page("Limits Plan", "a101", pdf_a("A-101", "LIMITS PLAN", (0.125, 12.0)), (0.125, 12.0))
    first = b.condition("Normal condition", "linear", ref_no=1)
    folder = b.folder("Limit Folder")
    b.condition("Condition in a folder", "count", folder=folder, ref_no=2)
    b.condition("Highest number condition", "area", ref_no=top_ref)
    b.takeoff(page, first, list(PLAN_A["vwall"]))
    return b


def fixture_names():
    b = Bid("M1-08-names", "M1-08 Names",
            "M1 device test pack fixture: a 255-character condition name, Comments look-alike layers, padded page and folder names.")
    b.bid_scale = (1, 0, 0.125, 12.0)
    pdf = draw_plan_a("A-101", "NAMES PLAN", *scale_texts(0.125, 12.0))
    first = None
    for key, name in (("p1", "  Padded Page  "), ("p2", "Padded Page"), ("p3", "Trailing Space "), ("p4", "Ünïcode Pagé 日本")):
        uid = b.page(name, "names", pdf, (0.125, 12.0), member="m1-08-names-plan.pdf")
        first = first or uid
    # the layers: visible look-alikes and the ones the strip + lower rule hides (it is the rule of the Comments layer)
    for name in ("Comments 2", "Comment", "Commentary", "Annotation ", "Layer With Trailing Space ", "comments ", "COMMENTS", " Comments"):
        b.layer(name)
    prefix = "N255-"
    body = "".join("%d" % (i % 10) for i in range(255 - len(prefix) - 4))
    name255 = prefix + body + "-END"
    assert len(name255) == 255, len(name255)
    long_cond = b.condition(name255, "linear")
    b.condition("  Spaced Condition  ", "count", folder=b.folder(" Padded Folder "))
    b.condition("Duplicate Name", "area")
    b.condition("Duplicate Name", "area")
    b.condition("A & B <C> \"D\" 'E' ßé 日本", "count")
    b.takeoff(first, long_cond, list(PLAN_A["vwall"]))
    return b


def all_fixtures():
    return [
        fixture_folders(),
        fixture_layers(),
        lock_fixture("M1-03-locked", "M1-03 Locked", True),
        lock_fixture("M1-03b-unlocked", "M1-03b Unlocked control", False),
        fixture_pages(),
        fixture_one_page(),
        fixture_metric(),
        limits_fixture("M1-07-limits", "M1-07 Limits 2147483646", 2147483646),
        limits_fixture("M1-07b-limits-max", "M1-07b Limits 2147483647", 2147483647),
        fixture_names(),
    ]


# =====================================================================================================================
# Output
# =====================================================================================================================

def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        digest.update(handle.read())
    return digest.hexdigest()


def tracked_files(directory):
    names = ["README.md", "make_pack.py"]
    names += sorted(f for f in os.listdir(directory) if f.endswith(".osp"))
    return names


def write_pack(directory, write_hashes):
    os.makedirs(directory, exist_ok=True)
    for bid in all_fixtures():
        package, _members = bid.build_package()
        with open(os.path.join(directory, bid.stem + ".osp"), "wb") as handle:
            handle.write(package)
    if write_hashes:
        lines = ["%s  %s\n" % (sha256_file(os.path.join(directory, name)), name) for name in tracked_files(directory)
                 if os.path.exists(os.path.join(directory, name))]
        with open(os.path.join(directory, "PACK_SHA256.txt"), "w", newline="\n") as handle:
            handle.write("".join(lines))


def cab_members_of(data):
    """Reads back the (name, bytes) members of a cabinet this script wrote (one MSZIP folder), to compare contents."""
    (signature, _r1, _total, _r2, files_offset, _r3, _minor, _major, _folders, files, _flags, _set_id,
     _cabinet) = struct.unpack_from("<4sIIIIIBBHHHHH", data, 0)
    assert signature == b"MSCF"
    data_offset, blocks, _type = struct.unpack_from("<IHH", data, 36)
    folder = bytearray()
    position = data_offset
    for _ in range(blocks):
        _csum, size, unc = struct.unpack_from("<IHH", data, position)
        payload = data[position + 8: position + 8 + size]
        assert payload[:2] == b"CK"
        inflater = zlib.decompressobj(-15, zdict=bytes(folder[-CAB_BLOCK:])) if folder else zlib.decompressobj(-15)
        chunk = inflater.decompress(payload[2:])
        assert len(chunk) == unc
        folder += chunk
        position += 8 + size
    members = []
    position = files_offset
    for _ in range(files):
        size, offset = struct.unpack_from("<II", data, position)
        end = data.index(b"\0", position + 16)
        members.append((data[position + 16: end].decode("ascii"), bytes(folder[offset: offset + size])))
        position = end + 1
    return members


def check_pdf(data):
    """Strict structure check of one PDF written by build_pdf: header, every xref entry points at its object, trailer, stream length."""
    assert data.startswith(b"%PDF-1.4\n%") and data.endswith(b"%%EOF\n")
    xref_at = int(re.search(rb"startxref\n(\d+)\n%%EOF\n$", data).group(1))
    head = re.match(rb"xref\n0 (\d+)\n", data[xref_at:])
    count = int(head.group(1))
    table = data[xref_at + head.end(): xref_at + head.end() + 20 * count]
    assert len(table) == 20 * count and table[:20] == b"0000000000 65535 f \n"
    for number in range(1, count):
        entry = table[20 * number: 20 * number + 20]
        assert re.fullmatch(rb"\d{10} 00000 n \n", entry), entry
        assert data[int(entry[:10]):].startswith(b"%d 0 obj\n" % number), number
    assert re.search(rb"trailer\n<< /Size %d /Root 1 0 R /Info 7 0 R /ID \[<[0-9A-F]{32}> <[0-9A-F]{32}>\] >>\n" % count, data)
    stream = re.search(rb"<< /Length (\d+) >>\nstream\n", data)
    assert data[stream.end() + int(stream.group(1)):].startswith(b"endstream\nendobj\n")
    assert re.search(rb"/MediaBox \[0 0 612 792\]", data)


def check_xml_references(xml_bytes):
    """The references of one OST file resolve inside the file (the Windows raw integrity rule, restated with ElementTree)."""
    root = ET.fromstring(xml_bytes)
    assert root.tag == "XML_ROOT" and [c.tag for c in root][:2] == ["OST", "Bid"]
    bid = root.find("Bid")
    tables = {"folder": list(bid.iterfind("BidConditionFolders/BidConditionFolder")),
              "layer": list(bid.iterfind("BidLayers/BidLayer")),
              "condition": list(bid.iterfind("BidConditions/BidCondition")),
              "page": list(bid.iterfind("BidPages/BidPage")),
              "takeoff": list(bid.iterfind("BidPages/BidPage/BidTakeoffs/BidTakeoff")),
              "legend": list(bid.iterfind("BidPages/BidPage/BidLegends/BidLegend")),
              "text": list(bid.iterfind("BidPages/BidPage/BidTexts/BidText")),
              "setting": list(bid.iterfind("BidSettings/BidSetting"))}
    uids = {}
    seen = set()
    for kind, found in tables.items():
        uids[kind] = {row.get("UID") for row in found}
        assert len(uids[kind]) == len(found), kind
        for uid in uids[kind]:
            assert uid.isdigit() and int(uid) > 0 and uid not in seen, (kind, uid)  # positive, unique over the whole file
            seen.add(uid)
    assert bid.get("UID") not in seen

    def ref(row, column, kind):
        value = row.get(column)
        assert value == "0" or value in uids[kind], (row.tag, column, value)

    for row in tables["condition"]:
        ref(row, "BidLayerUID", "layer")
        ref(row, "BidConditionFolderUID", "folder")
    for row in tables["folder"]:
        ref(row, "ParentUID", "folder")
    for row in tables["takeoff"]:
        ref(row, "BidConditionUID", "condition")
        ref(row, "BidPageUID", "page")
        ref(row, "ParentUID", "takeoff")
    for row in tables["legend"] + tables["text"]:
        ref(row, "BidPageUID", "page")
    for row in tables["text"]:
        ref(row, "BidLayerUID", "layer")
    for row in tables["setting"]:
        ref(row, "BidPageSelectedUID", "page")
    statuses = {row.get("UID") for row in root.iterfind("JobStatuses/JobStatuse")}
    job_status = bid.get("JobStatusUID")
    assert job_status == "0" or job_status in statuses
    assert len(root.findall("Bid")) == 1 and len(tables["setting"]) <= 1
    return {kind: len(found) for kind, found in tables.items()}


def selftest():
    # 1. a cabinet of many blocks (repeats beyond the 32 KB window, incompressible bytes, an empty member, exact block sizes)
    pseudo = bytearray()
    state = 7
    for _ in range(40000):
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        pseudo.append(b"abcdefghijklmnopqrstuvwxyz "[(state >> 16) % 27])
    noise = bytearray()
    state = 3
    for _ in range(70000):
        state = (state * 6364136223846793005 + 1442695040888963407) & (2 ** 64 - 1)
        noise.append((state >> 33) & 0xFF)
    members = [("big.txt", bytes(pseudo) * 5), ("random.bin", bytes(noise)), ("empty.txt", b""),
               ("sub\\exact.dat", bytes(pseudo[:32768])), ("sub\\over.dat", bytes(pseudo[:32769])),
               ("TempImages!.tmp\\x.pdf", b"%PDF-1.4\n")]
    cab = build_cab(members)
    assert cab_members_of(cab) == members
    blocks = struct.unpack_from("<H", cab, 40)[0]
    assert blocks == (sum(len(d) for _n, d in members) + CAB_BLOCK - 1) // CAB_BLOCK and blocks > 5, blocks
    position = struct.unpack_from("<I", cab, 36)[0]
    for _ in range(blocks):  # every CFDATA checksum is the CSUMCOMPUTE of its payload and header
        stored, size, unc = struct.unpack_from("<IHH", cab, position)
        payload = cab[position + 8: position + 8 + size]
        assert stored == cab_checksum(struct.pack("<HH", size, unc), cab_checksum(payload))
        position += 8 + size
    assert position == len(cab) == struct.unpack_from("<I", cab, 8)[0]
    print("selftest: cabinet of %d blocks round-trips, checksums and sizes agree" % blocks)
    # 2. the ten packages
    first = [bid.build_package() for bid in all_fixtures()]
    second = [bid.build_package() for bid in all_fixtures()]
    assert [c for c, _m in first] == [c for c, _m in second], "two builds differ"
    for bid, (cabinet, built) in zip(all_fixtures(), first):
        assert cab_members_of(cabinet) == built
        names = [name for name, _data in built]
        assert names[0] == bid.stem + ".ost" and all(n.startswith("TempImages!.tmp\\") and n.endswith(".pdf") for n in names[1:])
        counts = check_xml_references(built[0][1])
        for _name, data in built[1:]:
            check_pdf(data)
        print("selftest: %-20s %5d bytes, %d pdf, rows %s" % (bid.stem, len(cabinet), len(built) - 1,
                                                            " ".join("%s=%d" % kv for kv in counts.items() if kv[1])))
    print("selftest: ok")
    return 0


def check(directory):
    problems = []
    for bid in all_fixtures():
        path = os.path.join(directory, bid.stem + ".osp")
        if not os.path.exists(path):
            problems.append("%s is missing" % path)
            continue
        rebuilt, members = bid.build_package()
        with open(path, "rb") as handle:
            on_disk = handle.read()
        if on_disk == rebuilt:
            continue
        if cab_members_of(on_disk) == members:
            print("%s: bytes differ (another zlib?), members are identical" % bid.stem)
        else:
            problems.append("%s: members differ from the specification" % bid.stem)
    hashes_path = os.path.join(directory, "PACK_SHA256.txt")
    if os.path.exists(hashes_path):
        with open(hashes_path, "r") as handle:
            for line in handle.read().splitlines():
                digest, name = line.split("  ", 1)
                path = os.path.join(directory, name)
                if not os.path.exists(path):
                    problems.append("%s of PACK_SHA256.txt is missing" % name)
                elif sha256_file(path) != digest:
                    problems.append("%s does not match PACK_SHA256.txt" % name)
    else:
        problems.append("PACK_SHA256.txt is missing")
    for line in problems:
        print("PROBLEM: " + line)
    print("check: %s" % ("FAILED" if problems else "ok"))
    return 1 if problems else 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", help="directory for the .osp files (default: next to this script, with PACK_SHA256.txt)")
    parser.add_argument("--facts", help="write the facts of every bid to this file")
    parser.add_argument("--check", action="store_true", help="verify the files next to this script")
    parser.add_argument("--selftest", action="store_true", help="checks that need no other tool")
    args = parser.parse_args(argv)
    if args.selftest:
        return selftest()
    if args.check:
        return check(HERE)
    if args.facts:
        with open(args.facts, "w", encoding="utf-8", newline="\n") as handle:
            for bid in all_fixtures():
                for fact in bid.facts():
                    handle.write("\t".join(str(v) for v in fact) + "\n")
    write_pack(args.out or HERE, write_hashes=args.out is None)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
