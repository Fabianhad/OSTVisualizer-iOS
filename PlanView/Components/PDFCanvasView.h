#ifndef PDFCANVASVIEW_H
#define PDFCANVASVIEW_H

#import <UIKit/UIKit.h>

@interface PDFCanvasView : UIView

- (instancetype)initWithFrame:(CGRect)frame pdfPath:(NSString *)path;

@end

#endif
