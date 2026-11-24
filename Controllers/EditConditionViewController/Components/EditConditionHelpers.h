#ifndef EDITCONDITIONHELPERS_H
#define EDITCONDITIONHELPERS_H

#import <UIKit/UIKit.h>
#include "../../../Core/Domain/Types.hpp"
#include <string>

// Core inches_to_display: feet-inch text, or millimetres in a metric bid (FMT-01, COND-METRIC-01). The text parsers are
// ost_domain::display_to_inches and the parse_* functions of Core/Domain/ConditionFormRules.hpp.
NSString *inchesToDisplay(double inches, bool metric);

NSArray<NSString *> *patternLabels(void);
NSArray<NSString *> *shapeLabels(void);

int shapeDisplayIndex(ost_domain::ShapeId s);
ost_domain::ShapeId shapeIdAtIndex(NSInteger index);
bool isLinePattern(ost_domain::Pattern p);

#endif
