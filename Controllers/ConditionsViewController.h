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
// M1 (plan 5.1 and 5.2). Every one of these is an action that writes or opens a writing screen: the delegate calls its bid lock helper first.
// The "+" button and the "New Condition in <folder>" item of the long-press sheet: a New Condition form for the folder `folderUID`
// ("" = the top level). The "+" asks for the folder of the selected condition (Windows _on_new_clicked, conditions_sidebar.py:1392-1395), the
// long-press item for the folder of the row or section header that was pressed.
- (void)conditionsViewController:(ConditionsViewController *)vc
    didRequestNewConditionInFolderUID:(NSString *)folderUID;
// The "Folders" button: the folder manager (opening it writes nothing; the actions inside it are guarded).
- (void)conditionsViewControllerDidRequestFolders:(ConditionsViewController *)vc;
// "Move to Folder..." of the long-press sheet: the folder picker for the condition.
- (void)conditionsViewController:(ConditionsViewController *)vc
      didRequestMoveConditionUID:(NSString *)conditionUID;
@end

@interface ConditionsViewController : UIViewController

@property(nonatomic, weak) id<ConditionsViewControllerDelegate> delegate;

- (void)loadConditionsFromProject:(const ost_data::ProjectData &)project;
- (void)selectConditionWithUID:(NSString *)uid animated:(BOOL)animated;
- (NSString *)selectedConditionUID;
- (void)updateQuantities:(const std::unordered_map<std::string, ost_domain::QuantityResult> &)quantities;

@end
