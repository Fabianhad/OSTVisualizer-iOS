#ifndef PDFCANVASVIEW_H
#define PDFCANVASVIEW_H

#import <UIKit/UIKit.h>

// The page background: one page of a PDF (BidPages.ImagePath, page BidPages.Index1).
@interface PDFCanvasView : UIView

/// Opens `path` and picks the PDF page `pageIndex` (0-based, kept inside the document like the Windows page cache does).
- (instancetype)initWithPdfPath:(NSString *)path pageIndex:(int)pageIndex;

/// The size of that page in points as Windows reports it (FPDF_GetPageWidth / Height): the crop box (the media box when there
/// is none) with the page's own /Rotate applied. CGSizeZero when the file or the page cannot be read.
@property(nonatomic, readonly) CGSize pageSize;

/// Where the upright page (pageSize, top-left origin, Y down) goes inside the view: the page transform of the plan page (flips
/// about the page centre, then a counter-clockwise rotation; Core ost_page_view::page_affine). The view's bounds are the
/// transformed page. Identity by default.
@property(nonatomic) CGAffineTransform pageTransform;

@end

#endif
