#import "EditConditionHelpers.h"
#import "../EditConditionViewController_Internal.h"
#import <UIKit/UIKit.h>
#include "../../../Core/Domain/ConditionChoices.hpp"
#include "../../../Core/Domain/DimensionFormat.hpp"
#include <algorithm>
#include <cmath>


// --- Label arrays ---

NSArray<NSString *> *patternLabels() {
  return @[
    @"None", @"Solid", @"Horizontal", @"Vertical", @"Backward Diagonal",
    @"Forward Diagonal", @"Crosshatch", @"Diagonal Crosshatch", @"Transparent"
  ];
}

// The shape picker in the Windows order (COND-SHAPE-01): the list lives in Core.
NSArray<NSString *> *shapeLabels() {
  NSMutableArray<NSString *> *labels = [NSMutableArray array];
  for (const auto &choice : ost_domain::condition_shape_choices())
    [labels addObject:[NSString stringWithUTF8String:choice.label]];
  return labels;
}

ost_domain::ShapeId shapeIdAtIndex(NSInteger index) {
  const auto &choices = ost_domain::condition_shape_choices();
  if (index < 0 || (size_t)index >= choices.size())
    return choices.front().shape;
  return choices[(size_t)index].shape;
}

bool isLinePattern(ost_domain::Pattern p) {
  int v = static_cast<int>(p);
  return v >= 2 && v <= 7;
}

// The row of a shape in the picker; a shape the picker does not offer shows the first one (the Windows combo falls back to index 0).
int shapeDisplayIndex(ost_domain::ShapeId s) {
  const auto &choices = ost_domain::condition_shape_choices();
  for (size_t i = 0; i < choices.size(); i++) {
    if (choices[i].shape == s)
      return (int)i;
  }
  return 0;
}

// The elevation of a condition name is read and written by Core (ost_domain::parse_elevation / reassemble_elevation, PositionParser.hpp), the
// same functions the loader uses; the Objective-C copies that lived here (the second one trimmed only U+0020) are gone.

// --- Dimension display ---

NSString *inchesToDisplay(double inches, bool metric) {
  return [NSString
      stringWithUTF8String:ost_domain::inches_to_display(inches, metric).c_str()];
}
