#import <UIKit/UIKit.h>
#import "../Services/ImportService+ConditionFolders.h"
#include "../Core/Data/ProjectData.hpp"

@class ConditionFoldersViewController;

// The two faces of the screen (plan 5.2): the folder manager, and the same list as a picker for "Move to Folder...".
typedef NS_ENUM(NSInteger, ConditionFoldersMode) {
  ConditionFoldersModeManage,
  ConditionFoldersModePickFolder
};

// The screen holds no database and no bid: every write goes through its delegate (ViewController+Conditions.mm), which also owns the bid lock
// guard (plan 4.1: this class is not a ViewController category, so it reaches the one helper -_refuseIfBidLocked through the delegate).
@protocol ConditionFoldersViewControllerDelegate <NSObject>
// Called FIRST by every action that writes (a tap that opens an alert, a swipe action, the delete, the pick). NO = refuse: the lock alert
// has been shown and the action stops.
- (BOOL)conditionFoldersViewControllerMayWrite:(ConditionFoldersViewController *)vc;
// The writes. The delegate writes (ImportService), refreshes the project in memory and the Conditions list, calls
// -loadFoldersFromProject: on `vc` and hands the outcome back; the screen shows a refusal or a failure.
- (OSTConditionFolderOutcome *)conditionFoldersViewController:(ConditionFoldersViewController *)vc
                                            createFolderNamed:(NSString *)name
                                            inParentFolderUID:(NSString *)parentFolderUID;
- (OSTConditionFolderOutcome *)conditionFoldersViewController:(ConditionFoldersViewController *)vc
                                              renameFolderUID:(NSString *)folderUID
                                                       toName:(NSString *)name;
- (OSTConditionFolderOutcome *)conditionFoldersViewController:(ConditionFoldersViewController *)vc
                                              deleteFolderUIDs:(NSArray<NSString *> *)folderUIDs;
- (OSTConditionFolderOutcome *)conditionFoldersViewController:(ConditionFoldersViewController *)vc
                                               moveConditionUID:(NSString *)conditionUID
                                                    toFolderUID:(NSString *)folderUID;
@end

// The folder manager (New Folder, rename, delete, New Folder Inside; swipe, context menu and a multi-select edit mode) and the folder picker of
// "Move to Folder..." (a "(No folder)" row, the current folder checked). Texts, order, rules and failure alerts all come from Core
// (ConditionFolderUi.hpp, ConditionFolderRules.hpp, ProjectStore_ConditionFolderAlerts.hpp); this class shows them.
@interface ConditionFoldersViewController : UIViewController

@property(nonatomic, weak) id<ConditionFoldersViewControllerDelegate> delegate;

// `conditionUID` is the condition to move in the pick mode (nil in the manager).
- (instancetype)initWithProject:(const ost_data::ProjectData &)project
                           mode:(ConditionFoldersMode)mode
               pickConditionUID:(NSString *)conditionUID;

// Reads the folders and conditions of the project again (after a write) and redraws the list.
- (void)loadFoldersFromProject:(const ost_data::ProjectData &)project;

@end
