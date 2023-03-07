/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IIndexView.h>
#include <multiplier/ui/ISearchWidget.h>

namespace mx::gui {

//! The main class implementing the IIndexView interface
class IndexView final : public IIndexView {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~IndexView() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  IndexView(IFileTreeModel *model, QWidget *parent);

  //! Initializes the widgets
  void InitializeWidgets();

  //! Installs the model, updating the UI state
  void InstallModel(IFileTreeModel *model);

 private slots:
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

  friend class IIndexView;
};

}  // namespace mx::gui
