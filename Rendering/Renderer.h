#ifndef RENDERER_H
#define RENDERER_H

#import <UIKit/UIKit.h>

@interface Renderer : NSObject

@property(nonatomic) float cameraYaw;
@property(nonatomic) float cameraPitch;
@property(nonatomic) float cameraDistance;
@property(nonatomic) float cameraTargetX;
@property(nonatomic) float cameraTargetY;
@property(nonatomic) float cameraTargetZ;
@property(nonatomic, copy) NSString *selectedConditionUID;
@property(nonatomic, copy) NSString *selectedTakeoffUID;

- (instancetype)initWithView:(UIView *)hostView;
- (void)startBgfx;
- (void)resize:(int)width height:(int)height;
- (void)renderFrameWithTime:(float)time;
- (void)addMeshWithVertices:(const double *)vertices
                vertexCount:(int)vertexCount
                      faces:(const uint32_t *)faces
                  faceCount:(int)faceCount
                     colorR:(float)r
                          g:(float)g
                          b:(float)b
                      alpha:(float)a
               conditionUID:(NSString *)conditionUID
                 takeoffUID:(NSString *)takeoffUID;
- (void)clearMeshes;
- (void)fitCameraToMeshes;
- (NSInteger)meshTriangleCount;
- (NSString *)conditionUIDAtScreenX:(CGFloat)x y:(CGFloat)y;
- (NSString *)takeoffUIDAtScreenX:(CGFloat)x y:(CGFloat)y;
- (void)shutdown;

@end

#endif
