#import "../ViewController_Internal.h"

@interface ViewController (Layers) <UITableViewDataSource, UITableViewDelegate>
@end

@implementation ViewController (Layers)

- (void)_handleLayerToggle {
  if (self.projectService.project.empty() ||
      self.projectService.project.layers.empty())
    return;
  self.projectService.sortedLayerUIDs.clear();
  for (const auto &pair : self.projectService.project.layers) {
    self.projectService.sortedLayerUIDs.push_back(pair.first);
  }
  const auto &layers = self.projectService.project.layers;
  std::sort(self.projectService.sortedLayerUIDs.begin(),
            self.projectService.sortedLayerUIDs.end(),
            [&layers](const std::string &a, const std::string &b) {
              int sa = layers.at(a).sequence, sb = layers.at(b).sequence;
              if (sa != sb)
                return sa < sb;
              return layers.at(a).name < layers.at(b).name;
            });
  UITableView *table =
      [[UITableView alloc] initWithFrame:CGRectZero
                                   style:UITableViewStyleInsetGrouped];
  table.dataSource = self;
  table.delegate = self;
  table.translatesAutoresizingMaskIntoConstraints = NO;
  [table registerClass:[UITableViewCell class]
      forCellReuseIdentifier:@"LayerCell"];

  UIViewController *sheetVC = [[UIViewController alloc] init];
  sheetVC.title = @"Layers";
  sheetVC.view.backgroundColor = [UIColor systemGroupedBackgroundColor];
  [sheetVC.view addSubview:table];
  [NSLayoutConstraint activateConstraints:@[
    [table.topAnchor constraintEqualToAnchor:sheetVC.view.topAnchor],
    [table.bottomAnchor constraintEqualToAnchor:sheetVC.view.bottomAnchor],
    [table.leadingAnchor constraintEqualToAnchor:sheetVC.view.leadingAnchor],
    [table.trailingAnchor constraintEqualToAnchor:sheetVC.view.trailingAnchor],
  ]];
  UIBarButtonItem *showAll =
      [[UIBarButtonItem alloc] initWithTitle:@"Show All"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(_layersShowAll:)];
  UIBarButtonItem *hideAll =
      [[UIBarButtonItem alloc] initWithTitle:@"Hide All"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(_layersHideAll:)];
  sheetVC.navigationItem.leftBarButtonItems = @[ showAll, hideAll ];

  UIBarButtonItem *done = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(_layersDone:)];
  sheetVC.navigationItem.rightBarButtonItem = done;

  UINavigationController *nav =
      [[UINavigationController alloc] initWithRootViewController:sheetVC];
  nav.modalPresentationStyle = UIModalPresentationPageSheet;
  if (@available(iOS 15.0, *)) {
    UISheetPresentationControllerDetent *medium =
        [UISheetPresentationControllerDetent mediumDetent];
    UISheetPresentationControllerDetent *large =
        [UISheetPresentationControllerDetent largeDetent];
    nav.sheetPresentationController.detents = @[ medium, large ];
    nav.sheetPresentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
    nav.sheetPresentationController.prefersGrabberVisible = YES;
    nav.sheetPresentationController.largestUndimmedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierMedium;
  }
  objc_setAssociatedObject(self, "layerTable", table,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);

  [self presentViewController:nav animated:YES completion:nil];
}

- (void)_layersShowAll:(id)sender {
  for (auto &pair : self.projectService.mutableProject.layers) {
    pair.second.visible = true;
  }
  [self _generateMeshesForSelectedPages];
  [self _refreshOverlays];
  UITableView *table = objc_getAssociatedObject(self, "layerTable");
  [table reloadData];

  if (self.bidUID.length > 0) {
    for (const auto &pair : self.projectService.project.layers) {
      [ImportService
          setLayerVisibility:YES
                    forLayer:[NSString stringWithUTF8String:pair.first.c_str()]
                       inBid:self.bidUID];
    }
  }
}

- (void)_layersHideAll:(id)sender {
  for (auto &pair : self.projectService.mutableProject.layers) {
    pair.second.visible = false;
  }
  [self _generateMeshesForSelectedPages];
  [self _refreshOverlays];
  UITableView *table = objc_getAssociatedObject(self, "layerTable");
  [table reloadData];

  if (self.bidUID.length > 0) {
    for (const auto &pair : self.projectService.project.layers) {
      [ImportService
          setLayerVisibility:NO
                    forLayer:[NSString stringWithUTF8String:pair.first.c_str()]
                       inBid:self.bidUID];
    }
  }
}

- (void)_layersDone:(id)sender {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - UITableView DataSource/Delegate (Layers)

- (NSInteger)tableView:(UITableView *)tableView
    numberOfRowsInSection:(NSInteger)section {
  return (NSInteger)self.projectService.sortedLayerUIDs.size();
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell =
      [tableView dequeueReusableCellWithIdentifier:@"LayerCell"
                                      forIndexPath:indexPath];
  if ((size_t)indexPath.row < self.projectService.sortedLayerUIDs.size()) {
    const auto &layer = self.projectService.project.layers.at(
        self.projectService.sortedLayerUIDs[indexPath.row]);
    cell.textLabel.text = [NSString stringWithUTF8String:layer.name.c_str()];
    cell.accessoryType = layer.visible ? UITableViewCellAccessoryCheckmark
                                       : UITableViewCellAccessoryNone;
    cell.textLabel.textColor =
        layer.visible ? [UIColor labelColor] : [UIColor tertiaryLabelColor];
  }
  return cell;
}

- (void)tableView:(UITableView *)tableView
    didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  if ((size_t)indexPath.row < self.projectService.sortedLayerUIDs.size()) {
    const std::string &layerUID =
        self.projectService.sortedLayerUIDs[indexPath.row];
    auto &layer = self.projectService.mutableProject.layers[layerUID];
    layer.visible = !layer.visible;
    [tableView reloadRowsAtIndexPaths:@[ indexPath ]
                     withRowAnimation:UITableViewRowAnimationNone];
    [self _generateMeshesForSelectedPages];
    [self _refreshOverlays];

    if (self.bidUID.length > 0) {
      [ImportService
          setLayerVisibility:layer.visible
                    forLayer:[NSString stringWithUTF8String:layerUID.c_str()]
                       inBid:self.bidUID];
    }
  }
}

@end
