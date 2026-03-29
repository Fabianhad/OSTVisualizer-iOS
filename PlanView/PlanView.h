#ifndef PLANVIEW_H
#define PLANVIEW_H

#import <UIKit/UIKit.h>
#include "../Core/Data/ProjectData.hpp"
#include "../Core/Domain/RenderConfig.hpp"

@class PlanView;

@protocol PlanViewEditDelegate <NSObject>
- (void)planView:(PlanView *)planView didTapTakeoff:(const std::string &)takeoffUID;
- (void)planView:(PlanView *)planView didTapHandle:(int)handleIndex
       ofTakeoff:(const std::string &)takeoffUID;
- (void)planView:(PlanView *)planView didTapEmptySpaceAtCanvasPoint:(CGPoint)point;
- (void)planView:(PlanView *)planView didDragTakeoff:(const std::string &)takeoffUID
       handleIndex:(int)handleIndex
       canvasDelta:(CGPoint)delta;
- (void)planView:(PlanView *)planView didEndDragTakeoff:(const std::string &)takeoffUID;
- (void)planView:(PlanView *)planView didPlaceTakeoffWithCondition:(const std::string &)conditionUID
                        position:(const std::vector<double> &)position;
@optional
- (void)planView:(PlanView *)planView didLongPressTakeoff:(const std::string &)takeoffUID
                atPoint:(CGPoint)screenPoint;
- (void)planView:(PlanView *)planView didLongPressEmptySpaceAtPoint:(CGPoint)screenPoint;
- (void)planView:(PlanView *)planView areaPlacementInProgress:(BOOL)inProgress;
@end

@interface PlanView : UIScrollView

@property(nonatomic, weak) id<PlanViewEditDelegate> editDelegate;
@property(nonatomic) BOOL editEnabled;
@property(nonatomic) BOOL angleSnappingEnabled;
@property(nonatomic, copy) NSString *selectedConditionUID;

- (void)loadPageWithWidth:(double)width
                   height:(double)height
                 rotation:(int)rotation
             scaleFactor1:(double)sf1
             scaleFactor2:(double)sf2
                imagePath:(const std::string &)imagePath
                resetZoom:(BOOL)resetZoom;

- (void)renderOverlaysForPage:(const std::string &)pageUID
                      project:(const ost_data::ProjectData &)project
                 renderConfig:(const ost_domain::RenderConfig &)config
              selectedTakeoff:(const std::string &)selectedUID
            selectedCondition:(const std::string &)selectedCondUID
                 activeHandle:(int)activeHandle;

- (NSString *)takeoffUIDAtPoint:(CGPoint)viewPoint;

@end

@interface PlanView (Placement)

- (void)enterPlacementModeWithCondition:(const ost_domain::Condition &)condition
                          snapIncrement:(double)snap;
- (void)exitPlacementMode;
- (void)undoLastAreaPoint;
- (void)cancelAreaPlacement;

@end

#endif
