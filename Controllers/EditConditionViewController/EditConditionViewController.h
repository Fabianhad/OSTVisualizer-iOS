#import <UIKit/UIKit.h>
#include "../../Core/Domain/Condition.hpp"
#include "../../Core/Domain/ConditionChanges.hpp"
#include "../../Core/Domain/Takeoff.hpp"
#include "../../Core/Domain/Layer.hpp"
#include "../../Core/Domain/CdnType.hpp"
#include <unordered_map>
#include <vector>
#include <string>

@class EditConditionViewController;

@protocol EditConditionViewControllerDelegate <NSObject>
// Writes the edit: `condition` is the condition to keep, `changes` the fields that differ from the stored one (COND-UPD-02).
// NO = the save failed (COND-UPD-03): nothing was changed, the dialog stays open and says so. *failureMessage (may be NULL) is then the
// reason to show under "Save Failed" when the store gave one (a renumber that would push another condition past the highest
// condition number, DLG-FINITE-01), else nil and the dialog shows the generic text.
- (BOOL)editConditionViewController:(EditConditionViewController *)vc
                   didSaveCondition:(const ost_domain::Condition &)condition
                            changes:(const ost_domain::ConditionChanges &)changes
                     failureMessage:(NSString **)failureMessage;
@optional
- (void)editConditionViewController:(EditConditionViewController *)vc
             didNavigateToCondition:(NSString *)conditionUID;
@end

@interface EditConditionViewController : UIViewController

@property(nonatomic, weak) id<EditConditionViewControllerDelegate> delegate;

- (instancetype)initWithCondition:(const ost_domain::Condition &)condition
                    conditionUIDs:(const std::vector<std::string> &)orderedUIDs
                    conditionsMap:(const std::unordered_map<std::string, ost_domain::Condition> &)conditions
                         takeoffs:(const std::vector<ost_domain::Takeoff> &)takeoffs
                           layers:(const std::unordered_map<std::string, ost_domain::Layer> &)layers
                     mergedLayers:(const std::vector<ost_domain::BidLayer> &)mergedLayers
                         cdnTypes:(const std::unordered_map<std::string, ost_domain::CdnType> &)cdnTypes
                           metric:(BOOL)metric;

@end
