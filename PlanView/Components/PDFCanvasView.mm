#import "PDFCanvasView.h"
#include "../../Core/Geometry/page_view.hpp"

@implementation PDFCanvasView {
  CGPDFDocumentRef _pdfDoc;
  CGPDFPageRef _pdfPage;
  CGSize _pageSize;
}

@synthesize pageSize = _pageSize;
@synthesize pageTransform = _pageTransform;

- (instancetype)initWithPdfPath:(NSString *)path pageIndex:(int)pageIndex {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.backgroundColor = [UIColor whiteColor];
    self.opaque = YES;
    self.userInteractionEnabled = NO;
    _pageTransform = CGAffineTransformIdentity;

    NSURL *fileURL = [NSURL fileURLWithPath:path];
    _pdfDoc = CGPDFDocumentCreateWithURL((__bridge CFURLRef)fileURL);
    size_t pageCount = _pdfDoc ? CGPDFDocumentGetNumberOfPages(_pdfDoc) : 0;
    if (pageCount > 0) {
      // BidPages.Index1 - 1, kept inside the document (CGPDF pages count from 1)
      int index = ost_page_view::normalized_pdf_page_index(pageIndex,
                                                           (int)pageCount);
      _pdfPage = CGPDFDocumentGetPage(_pdfDoc, (size_t)index + 1);
    }
    if (_pdfPage) {
      CGRect box = CGPDFPageGetBoxRect(_pdfPage, kCGPDFCropBox);
      auto size = ost_page_view::effective_pdf_page_size(
          box.size.width, box.size.height,
          CGPDFPageGetRotationAngle(_pdfPage));
      _pageSize = CGSizeMake(size[0], size[1]);
    }
  }
  return self;
}

- (void)dealloc {
  if (_pdfDoc) {
    CGPDFDocumentRelease(_pdfDoc);
    _pdfDoc = NULL;
  }
}

- (void)setPageTransform:(CGAffineTransform)pageTransform {
  _pageTransform = pageTransform;
  [self setNeedsDisplay];
}

- (void)drawRect:(CGRect)rect {
  CGContextRef ctx = UIGraphicsGetCurrentContext();
  if (!ctx || !_pdfPage || _pageSize.width <= 0 || _pageSize.height <= 0)
    return;

  CGContextSaveGState(ctx);

  // the upright page onto the canvas: flipped and turned like the takeoffs
  CGContextConcatCTM(ctx, _pageTransform);

  // the PDF page into the upright page rectangle (PDF space is Y up)
  CGRect pageRect = CGRectMake(0, 0, _pageSize.width, _pageSize.height);
  CGContextTranslateCTM(ctx, 0, pageRect.size.height);
  CGContextScaleCTM(ctx, 1.0, -1.0);
  CGAffineTransform drawXform =
      CGPDFPageGetDrawingTransform(_pdfPage, kCGPDFCropBox, pageRect, 0, true);
  CGContextConcatCTM(ctx, drawXform);

  CGContextDrawPDFPage(ctx, _pdfPage);
  CGContextRestoreGState(ctx);
}

@end
