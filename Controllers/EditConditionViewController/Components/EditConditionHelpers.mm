#import "EditConditionHelpers.h"
#import "../EditConditionViewController_Internal.h"
#import <UIKit/UIKit.h>
#include <algorithm>
#include <cmath>


// --- Label arrays ---

NSArray<NSString *> *styleLabels() {
  return @[ @"Linear", @"Area", @"Count", @"Attachment" ];
}

NSArray<NSString *> *patternLabels() {
  return @[
    @"None", @"Solid", @"Horizontal", @"Vertical", @"Backward Diagonal",
    @"Forward Diagonal", @"Crosshatch", @"Diagonal Crosshatch", @"Transparent"
  ];
}

NSArray<NSString *> *shapeLabels() {
  return @[
    @"Square", @"Circle", @"Triangle", @"Rectangle", @"Diamond", @"Ellipse",
    @"Hexagon", @"Octagon"
  ];
}

const int shapeValues[] = {0, 1, 2, 3, 12, 13, 14, 15};
const int shapeValueCount = 8;

bool isLinePattern(ost_domain::Pattern p) {
  int v = static_cast<int>(p);
  return v >= 2 && v <= 7;
}

int shapeDisplayIndex(ost_domain::ShapeId s) {
  int v = static_cast<int>(s);
  for (int i = 0; i < shapeValueCount; i++) {
    if (shapeValues[i] == v)
      return i;
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

// --- Dimension display / parsing ---

static const char *kEighthsFractions[] = {"",    "1/8", "1/4", "3/8",
                                          "1/2", "5/8", "3/4", "7/8"};

NSString *inchesToDisplay(double inches) {
  if (inches == 0.0)
    return @"";
  NSString *sign = (inches < 0) ? @"-" : @"";
  double total = fabs(inches);
  int eighths = (int)round(total * 8.0);
  int totalInches = eighths / 8;
  int frac = eighths % 8;
  int feet = totalInches / 12;
  int wholeInches = totalInches % 12;
  const char *fracStr = kEighthsFractions[frac];
  bool hasFrac = (fracStr[0] != '\0');

  if (feet > 0) {
    if (wholeInches > 0 && hasFrac)
      return [NSString
          stringWithFormat:@"%@%d' %d %s\"", sign, feet, wholeInches, fracStr];
    if (wholeInches > 0)
      return [NSString stringWithFormat:@"%@%d' %d\"", sign, feet, wholeInches];
    if (hasFrac)
      return [NSString stringWithFormat:@"%@%d' 0 %s\"", sign, feet, fracStr];
    return [NSString stringWithFormat:@"%@%d' 0\"", sign, feet];
  }
  if (wholeInches > 0 && hasFrac)
    return [NSString stringWithFormat:@"%@%d %s\"", sign, wholeInches, fracStr];
  if (wholeInches > 0)
    return [NSString stringWithFormat:@"%@%d\"", sign, wholeInches];
  if (hasFrac)
    return [NSString stringWithFormat:@"%@%s\"", sign, fracStr];
  return @"";
}

double parseFraction(NSString *frac) {
  if ([frac isEqualToString:@"1/8"])
    return 0.125;
  if ([frac isEqualToString:@"1/4"])
    return 0.25;
  if ([frac isEqualToString:@"3/8"])
    return 0.375;
  if ([frac isEqualToString:@"1/2"])
    return 0.5;
  if ([frac isEqualToString:@"5/8"])
    return 0.625;
  if ([frac isEqualToString:@"3/4"])
    return 0.75;
  if ([frac isEqualToString:@"7/8"])
    return 0.875;
  return -1.0;
}

double displayToInches(NSString *text, bool *valid) {
  *valid = true;
  text = [text
      stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
  if (text.length == 0)
    return 0.0;

  NSRegularExpressionOptions opts = 0;

  NSRegularExpression *reFIF = [NSRegularExpression
      regularExpressionWithPattern:
          @"^\\s*(-?)\\s*(\\d+)\\s*'\\s*-?\\s*(\\d+)\\s+(\\d/\\d)\\s*\"?\\s*$"
                           options:opts
                             error:nil];

  NSRegularExpression *reFI = [NSRegularExpression
      regularExpressionWithPattern:
          @"^\\s*(-?)\\s*(\\d+)\\s*'\\s*-?\\s*(\\d+(?:\\.\\d+)?)\\s*\"?\\s*$"
                           options:opts
                             error:nil];

  NSRegularExpression *reFF = [NSRegularExpression
      regularExpressionWithPattern:
          @"^\\s*(-?)\\s*(\\d+)\\s*'\\s*-?\\s*(\\d/\\d)\\s*\"?\\s*$"
                           options:opts
                             error:nil];

  NSRegularExpression *reFO = [NSRegularExpression
      regularExpressionWithPattern:@"^\\s*(-?)\\s*(\\d+)\\s*'\\s*$"
                           options:opts
                             error:nil];

  NSRegularExpression *reIF =
      [NSRegularExpression regularExpressionWithPattern:
                               @"^\\s*(-?)\\s*(\\d+)\\s+(\\d/\\d)\\s*\"?\\s*$"
                                                options:opts
                                                  error:nil];

  NSRegularExpression *reFrac = [NSRegularExpression
      regularExpressionWithPattern:@"^\\s*(-?)\\s*(\\d/\\d)\\s*\"?\\s*$"
                           options:opts
                             error:nil];

  NSRegularExpression *reIO =
      [NSRegularExpression regularExpressionWithPattern:
                               @"^\\s*(-?)\\s*(\\d+(?:\\.\\d+)?)\\s*\"?\\s*$"
                                                options:opts
                                                  error:nil];

  NSRange range = NSMakeRange(0, text.length);
  NSTextCheckingResult *m;

  m = [reFIF firstMatchInString:text options:0 range:range];
  if (m) {
    double sign =
        ([[text substringWithRange:[m rangeAtIndex:1]] isEqualToString:@"-"])
            ? -1
            : 1;
    int feet = [[text substringWithRange:[m rangeAtIndex:2]] intValue];
    int whole = [[text substringWithRange:[m rangeAtIndex:3]] intValue];
    double frac = parseFraction([text substringWithRange:[m rangeAtIndex:4]]);
    if (frac < 0) {
      *valid = false;
      return 0;
    }
    return sign * (feet * 12.0 + whole + frac);
  }

  m = [reFI firstMatchInString:text options:0 range:range];
  if (m) {
    double sign =
        ([[text substringWithRange:[m rangeAtIndex:1]] isEqualToString:@"-"])
            ? -1
            : 1;
    int feet = [[text substringWithRange:[m rangeAtIndex:2]] intValue];
    double inches = [[text substringWithRange:[m rangeAtIndex:3]] doubleValue];
    return sign * (feet * 12.0 + inches);
  }

  m = [reFF firstMatchInString:text options:0 range:range];
  if (m) {
    double sign =
        ([[text substringWithRange:[m rangeAtIndex:1]] isEqualToString:@"-"])
            ? -1
            : 1;
    int feet = [[text substringWithRange:[m rangeAtIndex:2]] intValue];
    double frac = parseFraction([text substringWithRange:[m rangeAtIndex:3]]);
    if (frac < 0) {
      *valid = false;
      return 0;
    }
    return sign * (feet * 12.0 + frac);
  }

  m = [reFO firstMatchInString:text options:0 range:range];
  if (m) {
    double sign =
        ([[text substringWithRange:[m rangeAtIndex:1]] isEqualToString:@"-"])
            ? -1
            : 1;
    int feet = [[text substringWithRange:[m rangeAtIndex:2]] intValue];
    return sign * feet * 12.0;
  }

  m = [reIF firstMatchInString:text options:0 range:range];
  if (m) {
    double sign =
        ([[text substringWithRange:[m rangeAtIndex:1]] isEqualToString:@"-"])
            ? -1
            : 1;
    int whole = [[text substringWithRange:[m rangeAtIndex:2]] intValue];
    double frac = parseFraction([text substringWithRange:[m rangeAtIndex:3]]);
    if (frac < 0) {
      *valid = false;
      return 0;
    }
    return sign * (whole + frac);
  }

  m = [reFrac firstMatchInString:text options:0 range:range];
  if (m) {
    double sign =
        ([[text substringWithRange:[m rangeAtIndex:1]] isEqualToString:@"-"])
            ? -1
            : 1;
    double frac = parseFraction([text substringWithRange:[m rangeAtIndex:2]]);
    if (frac < 0) {
      *valid = false;
      return 0;
    }
    return sign * frac;
  }

  m = [reIO firstMatchInString:text options:0 range:range];
  if (m) {
    double sign =
        ([[text substringWithRange:[m rangeAtIndex:1]] isEqualToString:@"-"])
            ? -1
            : 1;
    double val = [[text substringWithRange:[m rangeAtIndex:2]] doubleValue];
    return sign * val;
  }

  NSScanner *scanner = [NSScanner scannerWithString:text];
  double val = 0;
  if ([scanner scanDouble:&val] && scanner.isAtEnd)
    return val;

  *valid = false;
  return 0.0;
}

double parseDimField(UITextField *field) {
  NSString *text = field.text;
  if (!text || text.length == 0)
    return 0.0;
  bool valid = true;
  return displayToInches(text, &valid);
}
