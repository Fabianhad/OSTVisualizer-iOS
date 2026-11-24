#import <UIKit/UIKit.h>
#include "../../Core/Domain/Condition.hpp"
#include "../../Core/Domain/ConditionChanges.hpp"
#include "../../Core/Domain/Takeoff.hpp"
#include "../../Core/Domain/Layer.hpp"
#include "../../Core/Domain/CdnType.hpp"
#include "../../Core/Domain/ConditionCreate.hpp"
#include <unordered_map>
#include <vector>
#include <string>

@class EditConditionViewController;

// What a Save of the New Condition form came to (the create mode, plan 5.1).
typedef NS_ENUM(NSInteger, EditConditionCreateOutcome) {
  // The condition was written: the form closes. A non-nil *message is a notice to show once it has closed (the Windows "Refresh Error" text:
  // the condition is in the database but the list could not be read again).
  EditConditionCreateOutcomeCreated,
  // Nothing was written and the delegate has already told the user why (the lock alert): the form stays open.
  EditConditionCreateOutcomeReported,
  // Nothing was written: the form stays open and shows "Save Failed" with *message, or the generic text when it is nil.
  EditConditionCreateOutcomeFailed
};

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
// Create mode only: Save of the New Condition form. `spec` is what Core built from the form (ost_domain::build_create_spec) and checked
// (ost_domain::validate_create_spec); the delegate writes it (ImportService createCondition:inBid:error:) and answers what came of it.
- (EditConditionCreateOutcome)editConditionViewController:(EditConditionViewController *)vc
                                didRequestCreateCondition:(const ost_domain::CreateConditionSpec &)spec
                                                  message:(NSString **)message;
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

// Create mode (New Condition, plan 5.1): the form of the synthetic condition of ost_domain::new_condition_form (Linear, the defaults of the bid's
// measurement system, the bid's own "Default" layer when it has one), with Save only (no Previous / Next), the Cond. No. row read-only with
// `nextNumberText` ("Next number: N"), the Name field first responder, and its own save path: Save asks the delegate to create the condition
// in `folderUID` ("" or nil = the top level). `mergedLayers`, `layers` and `cdnTypes` are the project's.
- (instancetype)initForNewConditionInFolder:(NSString *)folderUID
                               mergedLayers:(const std::vector<ost_domain::BidLayer> &)mergedLayers
                                     layers:(const std::unordered_map<std::string, ost_domain::Layer> &)layers
                                   cdnTypes:(const std::unordered_map<std::string, ost_domain::CdnType> &)cdnTypes
                                     metric:(BOOL)metric
                                 nextNumber:(NSString *)nextNumberText;

@end
