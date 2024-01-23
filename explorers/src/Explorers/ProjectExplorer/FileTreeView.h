/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAction>
#include <QPoint>
#include <QString>
#include <QWidget>

#include <multiplier/Types.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <vector>

namespace mx::gui {

class ConfigManager;
class FileTreeModel;
class MediaManager;
class ThemeManager;

class FileTreeView Q_DECL_FINAL : public IWindowWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  //! Destructor
  virtual ~FileTreeView(void);

  //! Constructor
  FileTreeView(const ConfigManager &config_manager,
               FileTreeModel *model,
               QWidget *parent = nullptr);

  void SortAscending(void);
  void SortDescending(void);

  //! Sets the root index.
  void SetRoot(const QModelIndex &);

 private:
  //! Used to implement click support without using the selection model
  virtual bool eventFilter(QObject *object, QEvent *event);

  //! Initializes the widgets
  void InitializeWidgets(const ConfigManager &config_manager);

  //! Installs the model, updating the UI state
  void InstallModel(FileTreeModel *model);

  //! Returns the list of nodes that have been expanded
  std::vector<QModelIndex> SaveExpandedNodeList(void);

  //! Expands the given set of model nodes
  void ApplyExpandedNodeList(std::vector<QModelIndex> index_list);

 private slots:
  //! Called when an item has been clicked in the tree view
  void OnFileTreeItemClicked(const QModelIndex &index);

  //! Called by the SearchWidget component whenever search options change
  void OnSearchParametersChange(void);

  //! Custom context menu for the tree view items
  void OnOpenItemContextMenu(const QPoint &point);

  //! Called at each model reset
  void OnModelReset(void);

  //! Called when the user disables the custom root item from the warning widget
  void OnDisableCustomRootLinkClicked(void);

  //! Called right after search is enabled to save the node expansion status
  void OnStartSearching(void);

  //! Called right after search is disabled to restore the node expansion status
  void OnStopSearching(void);

  //! Called by the theme manager
  void OnThemeChanged(const ThemeManager &theme_manager);
};

}  // namespace mx::gui
