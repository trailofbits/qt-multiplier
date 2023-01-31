/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IIndexView.h>

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

  //! Handler for clicks and double clicks
  void OnFileTreeItemActivated(const QModelIndex &index, bool double_click);

 private slots:
  //! Called when an item has been clicked in the tree view
  void OnFileTreeItemClicked(const QModelIndex &index);

  //! Called when an item has been double clicked in the tree view
  void OnFileTreeItemDoubleClicked(const QModelIndex &index);

  friend class IIndexView;
};

}  // namespace mx::gui
