// Layer: Services (Objective-C++)
// Shared by ImportService.mm and the feature categories Services/ImportService+<Feature>.mm: the two helpers a category needs and
// cannot reach otherwise (the database path and the UTF-8 -> NSString conversion). Not a public header: only Services/*.mm import it.
#ifndef IMPORTSERVICE_INTERNAL_H
#define IMPORTSERVICE_INTERNAL_H

#import "ImportService.h"
#include <string>

// An NSString for UTF-8 text from Core. Everything the import accepts is valid UTF-8 (IMP-M01), but a database filled by an older build may
// hold raw windows-1252 bytes: those are read as windows-1252 (or Latin-1) instead of becoming nil, which would raise in an NSDictionary
// literal and make the whole project list unreadable.
static inline NSString *OSTString(const std::string &text) {
  NSString *string = [NSString stringWithUTF8String:text.c_str()];
  if (!string) {
    string = [[NSString alloc] initWithBytes:text.data()
                                      length:text.size()
                                    encoding:NSWindowsCP1252StringEncoding];
  }
  if (!string) {
    string = [[NSString alloc] initWithBytes:text.data()
                                      length:text.size()
                                    encoding:NSISOLatin1StringEncoding];
  }
  return string ?: @"";
}

// Declaration only: the method is implemented in ImportService.mm. The path of the database file (ost_projects.sqlite in Documents),
// the db_path every ost_store function takes. A category in another file cannot call an undeclared class method (a compile error under ARC).
@interface ImportService (OSTInternal)
+ (std::string)_dbPath;
@end

#endif
