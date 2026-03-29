#ifndef FLOATINGMODEBUTTON_H
#define FLOATINGMODEBUTTON_H

#import <UIKit/UIKit.h>
#include "../../../Core/Domain/EditMode.hpp"

@class FloatingModeButton;

@protocol FloatingModeButtonDelegate <NSObject>
- (void)floatingModeButton:(FloatingModeButton *)fab didSelectMode:(ost_domain::EditMode)mode;
@end

@interface FloatingModeButton : UIView

@property(nonatomic, weak) id<FloatingModeButtonDelegate> delegate;
@property(nonatomic, readonly) ost_domain::EditMode currentMode;
@property(nonatomic) BOOL placeModeAvailable;
@property(nonatomic) BOOL cameraMode;

- (void)setCurrentMode:(ost_domain::EditMode)mode animated:(BOOL)animated;
- (void)installOverlayInView:(UIView *)parent;

@end

#endif
