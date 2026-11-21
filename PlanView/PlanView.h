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
/// Asked at the start of every placement gesture (the tap, the first touch of a drag) for the condition the tool is armed on. NO
/// = the press does nothing at all: no preview, no polygon point, no in-progress state (Windows handle_place_press, a condition
/// that is gone or not visible). Without this method every press is allowed.
- (BOOL)planView:(PlanView *)planView canPlaceWithCondition:(const std::string &)conditionUID;
@end

@interface PlanView : UIScrollView

@property(nonatomic, weak) id<PlanViewEditDelegate> editDelegate;
@property(nonatomic) BOOL editEnabled;
@property(nonatomic) BOOL angleSnappingEnabled;
@property(nonatomic, copy) NSString *selectedConditionUID;

/// Shows a page: the PDF page `page.page_index` of `page.image_path` (its own size, else Width x Height, else 612 x 792 points),
/// turned and mirrored by the page rotation and flips. The canvas the overlays and the placement maps use is that displayed
/// page. A default-constructed Page is a blank 612 x 792 page.
- (void)loadPage:(const ost_domain::Page &)page resetZoom:(BOOL)resetZoom;

- (void)renderOverlaysForPage:(const std::string &)pageUID
                      project:(const ost_data::ProjectData &)project
                 renderConfig:(const ost_domain::RenderConfig &)config
              selectedTakeoff:(const std::string &)selectedUID
            selectedCondition:(const std::string &)selectedCondUID
                 activeHandle:(int)activeHandle;

- (NSString *)takeoffUIDAtPoint:(CGPoint)viewPoint;

// Forgets which takeoff was tapped last (the one a drag would move and the next tap cycles from): the selection is dropped when the page
// changes (RPV-16).
- (void)clearTapSelection;

@end

@interface PlanView (Placement)

- (void)enterPlacementModeWithCondition:(const ost_domain::Condition &)condition
                          snapIncrement:(double)snap;
- (void)exitPlacementMode;
- (void)undoLastAreaPoint;
- (void)cancelAreaPlacement;

@end

#endif
