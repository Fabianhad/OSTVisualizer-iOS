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

// --- Elevation parsing ---

ElevationParts parseElevation(const std::string &name) {
  ElevationParts parts;
  parts.type = 0;
  auto posT = name.rfind(" @T");
  auto posB = name.rfind(" @B");
  auto pos = std::string::npos;
  int markerLen = 3;
  if (posT != std::string::npos && posB != std::string::npos) {
    pos = std::max(posT, posB);
    parts.type = (pos == posB) ? 1 : 0;
  } else if (posT != std::string::npos) {
    pos = posT;
    parts.type = 0;
  } else if (posB != std::string::npos) {
    pos = posB;
    parts.type = 1;
  }

  if (pos == std::string::npos) {
    auto posAt = name.rfind(" @ ");
    if (posAt == std::string::npos)
      posAt = name.rfind(" @");
    if (posAt != std::string::npos) {
      pos = posAt;
      parts.type = 1;
      markerLen = 2;
    }
  }
  if (pos == std::string::npos) {
    parts.baseName = name;
    parts.value = "";
    return parts;
  }
  parts.baseName = name.substr(0, pos);
  size_t valStart = pos + markerLen;
  if (valStart < name.size() && name[valStart] == ' ')
    valStart++;
  parts.value = (valStart < name.size()) ? name.substr(valStart) : "";
  return parts;
}

std::string reassembleElevation(const std::string &baseName, int type,
                                const std::string &value) {
  std::string trimmedValue = value;
  while (!trimmedValue.empty() && trimmedValue.front() == ' ')
    trimmedValue.erase(trimmedValue.begin());
  while (!trimmedValue.empty() && trimmedValue.back() == ' ')
    trimmedValue.pop_back();
  if (trimmedValue.empty())
    return baseName;
  const char *tag = (type == 0) ? " @T " : " @B ";
  return baseName + tag + trimmedValue;
}

// --- Dimension display ---

NSString *inchesToDisplay(double inches, bool metric) {
  return [NSString
      stringWithUTF8String:ost_domain::inches_to_display(inches, metric).c_str()];
}
