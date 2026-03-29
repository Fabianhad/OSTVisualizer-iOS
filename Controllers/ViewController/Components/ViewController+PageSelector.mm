#include "../../../Core/Domain/Folder.hpp"
#import "../ViewController_Internal.h"

@implementation ViewController (PageSelector)

- (void)_modeLabelTapped:(UIButton *)sender {
  BOOL want2D = (sender.tag == kModeLabelTag2D);
  [self _switchToMode2D:want2D];
}

- (void)_highlightModeLabels {
  for (UIScrollView *bar in @[ self.pageBar3D, self.pageBar2D ]) {
    for (UIView *sub in bar.subviews) {
      if (sub.tag == kModeLabelTag3D) {
        UIButton *btn = (UIButton *)sub;
        BOOL active = !self.is2DMode;
        btn.backgroundColor = active ? [UIColor colorWithRed:0.2
                                                       green:0.5
                                                        blue:0.9
                                                       alpha:0.9]
                                     : [UIColor colorWithWhite:0.3 alpha:0.6];
        [btn setTitleColor:active ? [UIColor whiteColor]
                                  : [UIColor colorWithWhite:1 alpha:0.5]
                  forState:UIControlStateNormal];
      } else if (sub.tag == kModeLabelTag2D) {
        UIButton *btn = (UIButton *)sub;
        BOOL active = self.is2DMode;
        btn.backgroundColor = active ? [UIColor colorWithRed:0.2
                                                       green:0.7
                                                        blue:0.4
                                                       alpha:0.9]
                                     : [UIColor colorWithWhite:0.3 alpha:0.6];
        [btn setTitleColor:active ? [UIColor whiteColor]
                                  : [UIColor colorWithWhite:1 alpha:0.5]
                  forState:UIControlStateNormal];
      }
    }
  }
}

- (void)_populatePageSelector {
  for (UIView *sub in [self.pageBar3D.subviews copy])
    [sub removeFromSuperview];
  for (UIView *sub in [self.pageBar2D.subviews copy])
    [sub removeFromSuperview];

  if (self.projectService.pageList.empty()) {
    self.pageBar3D.hidden = YES;
    self.pageBar2D.hidden = YES;
    return;
  }
  self.pageBar3D.hidden = NO;
  self.pageBar2D.hidden = NO;

  [self _populateBar:self.pageBar3D
               label:@"3D"
         pageTagBase:kPageTagBase3D
       folderTagBase:kFolderTagBase3D
          pageAction:@selector(_3DButtonTapped:)
        folderAction:@selector(_3DButtonTapped:)];

  [self _populateBar:self.pageBar2D
               label:@"2D"
         pageTagBase:kPageTagBase2D
       folderTagBase:0
          pageAction:@selector(_2DButtonTapped:)
        folderAction:nil];

  [self _highlightSelection];
}

- (void)_populateBar:(UIScrollView *)bar
               label:(NSString *)labelText
         pageTagBase:(int)pageTagBase
       folderTagBase:(int)folderTagBase
          pageAction:(SEL)pageAction
        folderAction:(SEL _Nullable)folderAction {
  UIFont *font = [UIFont systemFontOfSize:12 weight:UIFontWeightMedium];
  UIFont *folderFont = [UIFont systemFontOfSize:12 weight:UIFontWeightBold];
  NSDictionary *attrs = @{NSFontAttributeName : font};
  NSDictionary *folderAttrs = @{NSFontAttributeName : folderFont};
  CGFloat h = 28;
  CGFloat gap = 6;
  CGFloat x = 0;

  int modeTag =
      [labelText isEqualToString:@"3D"] ? kModeLabelTag3D : kModeLabelTag2D;
  UIButton *modeBtn = [UIButton buttonWithType:UIButtonTypeCustom];
  [modeBtn setTitle:labelText forState:UIControlStateNormal];
  modeBtn.titleLabel.font = [UIFont systemFontOfSize:11
                                              weight:UIFontWeightBold];
  modeBtn.layer.cornerRadius = 10;
  modeBtn.clipsToBounds = YES;
  modeBtn.tag = modeTag;
  [modeBtn addTarget:self
                action:@selector(_modeLabelTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  NSDictionary *modeBtnAttrs = @{NSFontAttributeName : modeBtn.titleLabel.font};
  CGFloat modeW = [labelText sizeWithAttributes:modeBtnAttrs].width + 16;
  modeBtn.frame = CGRectMake(x, 2, modeW, h);
  [bar addSubview:modeBtn];
  x += modeW + gap;

  bool hasFolders = !self.projectService.project.folders.empty();

  if (hasFolders) {
    for (size_t fi = 0; fi < self.projectService.folderOrder.size(); fi++) {
      const auto &folder = self.projectService.project.folders.at(
          self.projectService.folderOrder[fi]);
      NSString *folderName =
          [NSString stringWithFormat:@"\u25B8 %s", folder.name.c_str()];

      if (folderAction) {
        UIButton *fBtn = [self _selectorButtonWithTitle:folderName
                                                    tag:folderTagBase + (int)fi
                                                   font:folderFont
                                                 action:folderAction];
        fBtn.layer.cornerRadius = 6;
        CGFloat fW = [folderName sizeWithAttributes:folderAttrs].width + 20;
        fBtn.frame = CGRectMake(x, 2, fW, h);
        [bar addSubview:fBtn];
        x += fW + gap;
      } else {
        UILabel *fLabel = [[UILabel alloc] init];
        fLabel.text = folderName;
        fLabel.font = folderFont;
        fLabel.textColor = [UIColor colorWithWhite:1.0 alpha:0.5];
        CGFloat fW = [folderName sizeWithAttributes:folderAttrs].width + 8;
        fLabel.frame = CGRectMake(x, 4, fW, h - 4);
        [bar addSubview:fLabel];
        x += fW + gap;
      }

      for (size_t pi = 0; pi < self.projectService.pageList.size(); pi++) {
        if (self.projectService.pageList[pi].folder_uid !=
            self.projectService.folderOrder[fi])
          continue;
        NSString *name = [NSString
            stringWithUTF8String:self.projectService.pageList[pi].name.c_str()];
        UIButton *btn = [self _selectorButtonWithTitle:name
                                                   tag:pageTagBase + (int)pi
                                                  font:font
                                                action:pageAction];
        CGFloat w = [name sizeWithAttributes:attrs].width + 20;
        btn.frame = CGRectMake(x, 2, w, h);
        [bar addSubview:btn];
        x += w + gap;
      }

      [self _addSubfolderItems:folder
                       intoBar:bar
                             x:&x
                             h:h
                           gap:gap
                          font:font
                    folderFont:folderFont
                         attrs:attrs
                   folderAttrs:folderAttrs
                   pageTagBase:pageTagBase
                    pageAction:pageAction
                         depth:0];
    }
    for (size_t pi = 0; pi < self.projectService.pageList.size(); pi++) {
      if (!self.projectService.pageList[pi].folder_uid.empty())
        continue;
      NSString *name = [NSString
          stringWithUTF8String:self.projectService.pageList[pi].name.c_str()];
      UIButton *btn = [self _selectorButtonWithTitle:name
                                                 tag:pageTagBase + (int)pi
                                                font:font
                                              action:pageAction];
      CGFloat w = [name sizeWithAttributes:attrs].width + 20;
      btn.frame = CGRectMake(x, 2, w, h);
      [bar addSubview:btn];
      x += w + gap;
    }
  } else {
    for (size_t pi = 0; pi < self.projectService.pageList.size(); pi++) {
      NSString *name = [NSString
          stringWithUTF8String:self.projectService.pageList[pi].name.c_str()];
      UIButton *btn = [self _selectorButtonWithTitle:name
                                                 tag:pageTagBase + (int)pi
                                                font:font
                                              action:pageAction];
      CGFloat w = [name sizeWithAttributes:attrs].width + 20;
      btn.frame = CGRectMake(x, 2, w, h);
      [bar addSubview:btn];
      x += w + gap;
    }
  }
  bar.contentSize = CGSizeMake(x, 32);
}

- (void)_addSubfolderItems:(const ost_domain::Folder &)parentFolder
                   intoBar:(UIScrollView *)bar
                         x:(CGFloat *)x
                         h:(CGFloat)h
                       gap:(CGFloat)gap
                      font:(UIFont *)font
                folderFont:(UIFont *)folderFont
                     attrs:(NSDictionary *)attrs
               folderAttrs:(NSDictionary *)folderAttrs
               pageTagBase:(int)pageTagBase
                pageAction:(SEL)pageAction
                     depth:(int)depth {
  UIFont *subFont = [UIFont systemFontOfSize:11 weight:UIFontWeightMedium];
  NSDictionary *subAttrs = @{NSFontAttributeName : subFont};

  for (const auto &pair : parentFolder.subfolders) {
    const auto &sub = pair.second;
    NSString *indent = @"";
    for (int d = 0; d < depth; d++)
      indent = [indent stringByAppendingString:@"  "];
    NSString *subName =
        [NSString stringWithFormat:@"%@\u25B9 %s", indent, sub.name.c_str()];
    UILabel *subLabel = [[UILabel alloc] init];
    subLabel.text = subName;
    subLabel.font = [UIFont systemFontOfSize:11 weight:UIFontWeightSemibold];
    subLabel.textColor = [UIColor colorWithWhite:1.0 alpha:0.5];
    CGFloat lw = [subName sizeWithAttributes:subAttrs].width + 8;
    subLabel.frame = CGRectMake(*x, 4, lw, h - 4);
    [bar addSubview:subLabel];
    *x += lw + gap;
    for (size_t pi = 0; pi < self.projectService.pageList.size(); pi++) {
      if (self.projectService.pageList[pi].folder_uid != pair.first)
        continue;
      NSString *name = [NSString
          stringWithUTF8String:self.projectService.pageList[pi].name.c_str()];
      UIButton *btn = [self _selectorButtonWithTitle:name
                                                 tag:pageTagBase + (int)pi
                                                font:font
                                              action:pageAction];
      CGFloat w = [name sizeWithAttributes:attrs].width + 20;
      btn.frame = CGRectMake(*x, 2, w, h);
      [bar addSubview:btn];
      *x += w + gap;
    }
    [self _addSubfolderItems:sub
                     intoBar:bar
                           x:x
                           h:h
                         gap:gap
                        font:font
                  folderFont:folderFont
                       attrs:attrs
                 folderAttrs:folderAttrs
                 pageTagBase:pageTagBase
                  pageAction:pageAction
                       depth:depth + 1];
  }
}

- (UIButton *)_selectorButtonWithTitle:(NSString *)title
                                   tag:(int)tag
                                  font:(UIFont *)font
                                action:(SEL)action {
  UIButton *btn = [UIButton buttonWithType:UIButtonTypeCustom];
  [btn setTitle:title forState:UIControlStateNormal];
  btn.titleLabel.font = font;
  [btn setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
  btn.backgroundColor = [UIColor colorWithWhite:0 alpha:0.5];
  btn.layer.cornerRadius = 14;
  btn.clipsToBounds = YES;
  btn.tag = tag;
  [btn addTarget:self
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  return btn;
}

- (void)_3DButtonTapped:(UIButton *)sender {
  int tag = (int)sender.tag;

  BOOL previousHad3D = !self.projectService.selected3DPageUIDs.empty();

  if (tag >= kPageTagBase3D) {
    int idx = tag - kPageTagBase3D;
    if (idx >= 0 && idx < (int)self.projectService.pageList.size()) {
      [self.projectService toggle3DPage:self.projectService.pageList[idx].uid];
    }
  } else if (tag >= kFolderTagBase3D) {
    int fi = tag - kFolderTagBase3D;
    if (fi >= 0 && fi < (int)self.projectService.folderOrder.size()) {
      [self.projectService select3DFolder:self.projectService.folderOrder[fi]];
    }
  }

  [self _highlightSelection];
  [self _generateMeshesForSelectedPages];
  [self _updateConditionQuantities];

  BOOL currentHas3D = !self.projectService.selected3DPageUIDs.empty();
  if (!previousHad3D && currentHas3D) {
    [self.renderer fitCameraToMeshes];
  }
}

- (void)_2DButtonTapped:(UIButton *)sender {
  int tag = (int)sender.tag;
  if (tag < kPageTagBase2D)
    return;

  int idx = tag - kPageTagBase2D;
  if (idx < 0 || idx >= (int)self.projectService.pageList.size())
    return;

  BOOL previousHad2D = !self.projectService.selected2DPageUID.empty();

  [self.projectService select2DPage:self.projectService.pageList[idx].uid];

  [self _highlightSelection];

  BOOL currentHas2D = !self.projectService.selected2DPageUID.empty();
  BOOL resetZoom = !previousHad2D && currentHas2D;
  [self _loadCurrentPageIntoPlanViewResetZoom:resetZoom];
}

- (void)_highlightSelection {
  for (UIView *sub in self.pageBar3D.subviews) {
    if (![sub isKindOfClass:[UIButton class]])
      continue;
    UIButton *btn = (UIButton *)sub;
    int tag = (int)btn.tag;
    BOOL sel = NO;

    if (tag >= kPageTagBase3D) {
      int idx = tag - kPageTagBase3D;
      if (idx >= 0 && idx < (int)self.projectService.pageList.size()) {
        sel = self.projectService.selected3DPageUIDs.count(
                  self.projectService.pageList[idx].uid) > 0;
      }
    } else if (tag >= kFolderTagBase3D) {
      int fi = tag - kFolderTagBase3D;
      if (fi >= 0 && fi < (int)self.projectService.folderOrder.size()) {
        const auto &folder = self.projectService.project.folders.at(
            self.projectService.folderOrder[fi]);
        auto folderPages = folder.all_page_uids();
        sel = !folderPages.empty();
        for (const auto &uid : folderPages) {
          if (!self.projectService.selected3DPageUIDs.count(uid)) {
            sel = NO;
            break;
          }
        }
      }
    }

    if (tag >= kFolderTagBase3D && tag < kPageTagBase3D) {
      btn.backgroundColor = sel ? [UIColor colorWithRed:0.8
                                                  green:0.5
                                                   blue:0.1
                                                  alpha:0.8]
                                : [UIColor colorWithWhite:0 alpha:0.5];
    } else {
      btn.backgroundColor = sel ? [UIColor colorWithRed:0.2
                                                  green:0.5
                                                   blue:0.9
                                                  alpha:0.8]
                                : [UIColor colorWithWhite:0 alpha:0.5];
    }
  }

  for (UIView *sub in self.pageBar2D.subviews) {
    if (![sub isKindOfClass:[UIButton class]])
      continue;
    UIButton *btn = (UIButton *)sub;
    int tag = (int)btn.tag;
    if (tag < kPageTagBase2D)
      continue;

    int idx = tag - kPageTagBase2D;
    BOOL sel = NO;
    if (idx >= 0 && idx < (int)self.projectService.pageList.size()) {
      sel = (self.projectService.selected2DPageUID ==
             self.projectService.pageList[idx].uid);
    }
    btn.backgroundColor = sel ? [UIColor colorWithRed:0.2
                                                green:0.7
                                                 blue:0.4
                                                alpha:0.8]
                              : [UIColor colorWithWhite:0 alpha:0.5];
  }
}

@end
