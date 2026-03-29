#ifndef EDITCONDITIONHELPERS_H
#define EDITCONDITIONHELPERS_H

#import <UIKit/UIKit.h>
#include "../../../Core/Domain/Types.hpp"
#include <string>

struct ElevationParts;

ElevationParts parseElevation(const std::string &name);
std::string reassembleElevation(const std::string &baseName, int type, const std::string &value);

NSString *inchesToDisplay(double inches);
double displayToInches(NSString *text, bool *valid);
double parseDimField(UITextField *field);
double parseFraction(NSString *frac);

NSArray<NSString *> *styleLabels(void);
NSArray<NSString *> *patternLabels(void);
NSArray<NSString *> *shapeLabels(void);

int shapeDisplayIndex(ost_domain::ShapeId s);
bool isLinePattern(ost_domain::Pattern p);

extern const int shapeValues[];
extern const int shapeValueCount;

#endif
