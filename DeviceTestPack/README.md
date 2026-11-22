# M1 device test pack

Test data for the M1 device checks M1-01 to M1-20 of `NEW_FEATURES_PLAN.md` section 6.4. Ten small `.osp` packages, one bid each (the importer reads one bid per file), written by `make_pack.py`.

**Load:** AirDrop, or Files > tap a file > Open in OST Visualizer, or the Import button of the project browser (one file at a time). Each import is an independent copy; the bids appear unfiled as `M1-0x ...`. The number in a file name is the fixture number, not a check id. The shared layers (Image, Annotation, Default, hidden Comments) come from the app's database, not from the files. Every page has a one-page PDF with its sheet id in big letters.

| File | Bid | Holds | Serves checks |
| --- | --- | --- | --- |
| `M1-01-folders` | M1-01 Folders | 11 conditions (4 at the root, 7 in folders; next number 12), 3 takeoffs. 14 folders: depth 3 (Commercial > Division 03 Concrete > Footings), a folder holding only a sub-folder with a condition, empty folders, Concrete and concrete, a 50- and a 49-character name. | M1-02 to M1-06 (imperial), M1-08, M1-09, M1-10, M1-19, M1-20 |
| `M1-02-layers` | M1-02 Layers | 5 conditions, 7 takeoffs, six own layers: M1 Used, M1 Unused, M1 Hidden, M1 Locked Copy, M1 Text Only (used only by a text note: delete must be refused), M1 Second. | M1-01 (Layer picker), M1-11, M1-12, M1-13 |
| `M1-03-locked` | M1-03 Locked | Job status "M1 Locked Status" (Locked set). 2 pages, 3 conditions, 1 folder, 2 layers. | M1-07 |
| `M1-03b-unlocked` | M1-03b Unlocked control | Same content, status "M1 Unlocked Status" (Locked cleared): every action must work. | M1-07 (control) |
| `M1-04-pages` | M1-04 Pages | 3 pages. A-101: scale 0.25 : 8, custom; area, linear, count and hole takeoffs (the hole uses its parent's condition). A-102: EMPTY, 1/4" = 1'. A-103: 3/32" = 1', 1 area takeoff and a legend. 4 conditions. | M1-14 to M1-17 |
| `M1-05-one-page` | M1-05 One Page | Exactly one page (1/8" = 1'), 2 conditions, 5 takeoffs. | M1-15 (last page), M1-17 (nothing to confirm), M1-14 |
| `M1-06-metric` | M1-06 Metric | Metric bid, scale 1 : 50, snap 25 mm, conditions in m, m2 and each, 6 takeoffs. | M1-05 (metric), M1-16 |
| `M1-07-limits` | M1-07 Limits 2147483646 | Highest condition number 2147483646: the first New Condition works, the next is refused ("highest condition number"). | extends M1-02, M1-03 |
| `M1-07b-limits-max` | M1-07b Limits 2147483647 | Highest number 2147483647: New Condition is refused at once. | same |
| `M1-08-names` | M1-08 Names | A 255-character condition name. Layers "Comments 2", "Comment", "Commentary", "Annotation " and "Layer With Trailing Space " are listed; "comments ", "COMMENTS" and " Comments" must NOT be (Comments rule). 4 pages ("  Padded Page  ", "Padded Page", "Trailing Space ", accented and Japanese). Padded folder and condition names, two "Duplicate Name" conditions, one name with & < > " '. | M1-06 (256 refused), M1-08, M1-11, M1-14 |

M1-01, M1-13 and M1-19 need an iPad. M1-01 and M1-18 (persistence after reopen and relaunch) work with any file. The Type picker has no entries in any file (no fixture ships condition types), so it shows only "(none)". Every check has a fixture.

**Rebuild:** `python make_pack.py` (Python 3, standard library only) rewrites the `.osp` files and `PACK_SHA256.txt`; `--check` compares, `--selftest` self-checks, `--facts FILE` lists every folder, condition, layer, page and takeoff of every bid. Cabinet bytes depend on the machine's zlib; the members do not. `.gitattributes` keeps the text files LF so the hashes hold after a Windows checkout.

## Honest limits

- Generated from the element layout of the Windows exporter, not exported by the Windows app, and not opened by it (Windows acceptance not run, OI-M1-1).
- Checked on the host only: `expand.exe` unpacks all ten; the Core import pipeline imports all ten with the counts, names, flags, scales, lock status and takeoff positions above asserted; PDFium and MuPDF open every PDF. Nothing was run on a device (PDFKit, UIKit, Objective-C layer).
- Placeholders: takeoff Quantity 0 and Count 1 for counts (the app computes from positions), empty `BidAreaConditions`.
- Side effects: the two JobStatuses rows stay after the bids are deleted; PDFs are copied to `Documents/OST/<file name>/`.
- The scale text drawn in the PDFs does not change with Set Scale.
