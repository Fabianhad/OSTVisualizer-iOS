// The Set Scale form (plan 5.4, touch adaptation). Windows source of truth: ost_visualizer/presentation/dialogs/set_scale_dialog.py (styles, the Pre-defined
// and the Custom scale, "Apply to all pages", the texts, the validation). Every rule (what a typed pair means, the alert texts, the dirty state, the confirmation,
// the footer) lives in Core (PageScaleRules / PageScaleApply); this controller only shows the form and converts the types. It writes nothing itself: a Save is handed
// to the delegate, which writes through ImportService+PageScale and updates the project.
#import <UIKit/UIKit.h>
#include "../Core/Domain/PageScaleRules.hpp"

@class PageScaleViewController;

/// What the delegate did with a Save.
typedef NS_ENUM(NSInteger, PageScaleSaveResult) {
  /// Written and the project updated: the form closes.
  PageScaleSaveResultSaved = 0,
  /// Refused or failed: nothing was written; the form stays open and shows the
  /// Windows alert "Save Failed" / "Failed to save page scale.".
  PageScaleSaveResultFailed = 1,
  /// The bid is locked: the delegate has already shown "This bid is locked.";
  /// the form stays open and shows nothing more.
  PageScaleSaveResultLocked = 2,
  /// Refused because the takeoffs and annotations of the page cannot be stored at
  /// that scale: the form stays open and shows "Save Failed" / "The scale was
  /// not changed because the takeoffs and annotations on the page cannot be
  /// stored at that scale." (Windows PageScaleRangeError).
  PageScaleSaveResultUnstorable = 3
};

@protocol PageScaleViewControllerDelegate <NSObject>

/// Writes the pair of `settings` to the page of the form (every page of the bid
/// when settings.apply_to_all_pages). All pages or none.
- (PageScaleSaveResult)pageScaleViewController:(PageScaleViewController *)controller
                                didRequestSave:(const ost_domain::ScaleSettings &)settings;

/// How many pages a Save with "Apply to all pages" writes now: the form asks
/// for one confirmation when it is more than one page (N-M1-21).
- (NSUInteger)pageScaleViewControllerPageCountForApplyToAll:(PageScaleViewController *)controller;

@end

@interface PageScaleViewController : UIViewController

@property(nonatomic, weak) id<PageScaleViewControllerDelegate> delegate;

/// The page the form was opened on, the bid it was opened for, and the pair that
/// page stored at that moment (Windows _page_dialog_context_is_current compares
/// them again when the form saves).
@property(nonatomic, copy, readonly) NSString *pageUID;
@property(nonatomic, copy, readonly) NSString *bidUID;
@property(nonatomic, readonly) double openedScaleFactor1;
@property(nonatomic, readonly) double openedScaleFactor2;

/// `scaleFactor1` / `scaleFactor2` are the pair the page stores (the form reads
/// them like Windows: a 0 reads 1.0). `pageName` is only shown in the footer.
- (instancetype)initWithPageUID:(NSString *)pageUID
                       pageName:(NSString *)pageName
                         bidUID:(NSString *)bidUID
                   scaleFactor1:(double)scaleFactor1
                   scaleFactor2:(double)scaleFactor2;

@end
