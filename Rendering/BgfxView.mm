#import "BgfxView.h"

@implementation BgfxView

+ (Class)layerClass {
  return NSClassFromString(@"CAMetalLayer");
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.layer.contentsScale = [UIScreen mainScreen].scale;
    self.layer.opaque = YES;
  }
  return self;
}

@end
