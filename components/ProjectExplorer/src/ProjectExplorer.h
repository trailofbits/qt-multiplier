/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IProjectExplorer.h>
#include <multiplier/ui/ISearchWidget.h>

namespace mx::gui {

//! The main class implementing the IProjectExplorer interface
class ProjectExplorer final : public IProjectExplorer {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~ProjectExplorer() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  ProjectExplorer(IFileTreeModel *model, QWidget *parent);

  //! Initializes the widgets
  void InitializeWidgets();

  //! Installs the model, updating the UI state
  void InstallModel(IFileTreeModel *model);

  //! Returns the list of nodes that have been expanded
  std::vector<QModelIndex> SaveExpandedNodeList();

  //! Expands the given set of model nodes
  void ApplyExpandedNodeList(const std::vector<QModelIndex> &index_list);

 private slots:
  //! Try to open the file related to a specific model index.
  void SelectionChanged(const QModelIndex &index, const QModelIndex &previous);

  //! Called when an item has been clicked in the tree view
  void OnFileTreeItemClicked(const QModelIndex &index);

  //! Called by the ISearchWidget component whenever search options change
  void OnSearchParametersChange(
      const ISearchWidget::SearchParameters &search_parameters);

  //! Custom context menu for the tree view items
  void OnOpenItemContextMenu(const QPoint &point);

  //! Called when an action in the context menu is triggered
  void OnContextMenuActionTriggered(QAction *action);

  //! Called at each model reset
  void OnModelReset();

  //! Called when the user disables the custom root item from the warning widget
  void OnDisableCustomRootLinkClicked();

  //! Called right after search is enabled to save the node expansion status
  void OnStartSearching();

  //! Called right after search is disabled to restore the node expansion status
  void OnStopSearching();

  friend class IProjectExplorer;
};

}  // namespace mx::gui
