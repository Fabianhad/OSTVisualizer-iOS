#import <UIKit/UIKit.h>
#include "../Core/Data/ProjectData.hpp"
#include "../Core/Domain/UomService.hpp"

@class ConditionsViewController;

@protocol ConditionsViewControllerDelegate <NSObject>
- (void)conditionsViewController:(ConditionsViewController *)vc
           didSelectConditionUID:(NSString *)conditionUID;
@optional
- (void)conditionsViewController:(ConditionsViewController *)vc
      didRequestEditConditionUID:(NSString *)conditionUID;
- (void)conditionsViewController:(ConditionsViewController *)vc
    didRequestDuplicateConditionUID:(NSString *)conditionUID;
- (void)conditionsViewController:(ConditionsViewController *)vc
    didRequestDeleteConditionUID:(NSString *)conditionUID;
@end

@interface ConditionsViewController : UIViewController

@property(nonatomic, weak) id<ConditionsViewControllerDelegate> delegate;

- (void)loadConditionsFromProject:(const ost_data::ProjectData &)project;
- (void)selectConditionWithUID:(NSString *)uid animated:(BOOL)animated;
- (NSString *)selectedConditionUID;
- (void)updateQuantities:(const std::unordered_map<std::string, ost_domain::QuantityResult> &)quantities;

@end
