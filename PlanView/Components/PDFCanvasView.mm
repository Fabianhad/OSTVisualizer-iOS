#import "PDFCanvasView.h"

@implementation PDFCanvasView {
  CGPDFDocumentRef _pdfDoc;
  CGPDFPageRef _pdfPage;
}

- (instancetype)initWithFrame:(CGRect)frame pdfPath:(NSString *)path {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor whiteColor];
    self.opaque = YES;
    self.userInteractionEnabled = NO;

    NSURL *fileURL = [NSURL fileURLWithPath:path];
    _pdfDoc = CGPDFDocumentCreateWithURL((__bridge CFURLRef)fileURL);
    if (_pdfDoc && CGPDFDocumentGetNumberOfPages(_pdfDoc) > 0) {
      _pdfPage = CGPDFDocumentGetPage(_pdfDoc, 1);
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

- (void)drawRect:(CGRect)rect {
  CGContextRef ctx = UIGraphicsGetCurrentContext();
  if (!ctx || !_pdfPage)
    return;

  CGRect bounds = self.bounds;

  CGContextSaveGState(ctx);

  CGContextTranslateCTM(ctx, 0, bounds.size.height);
  CGContextScaleCTM(ctx, 1.0, -1.0);
  CGAffineTransform drawXform =
      CGPDFPageGetDrawingTransform(_pdfPage, kCGPDFMediaBox, bounds, 0, true);
  CGContextConcatCTM(ctx, drawXform);

  CGContextDrawPDFPage(ctx, _pdfPage);
  CGContextRestoreGState(ctx);
}

@end
