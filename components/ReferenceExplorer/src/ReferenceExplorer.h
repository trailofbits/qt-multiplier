/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IReferenceExplorer.h>
#include <multiplier/ui/ISearchWidget.h>

namespace mx::gui {

//! The implementation for the IReferenceExplorer interface
class ReferenceExplorer final : public IReferenceExplorer {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~ReferenceExplorer() override;

  //! \copybrief IReferenceExplorer::Model
  virtual IReferenceExplorerModel *Model() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  ReferenceExplorer(IReferenceExplorerModel *model, QWidget *parent);

  //! Initializes the internalwidgets
  void InitializeWidgets();

  //! Installs the given model
  void InstallModel(IReferenceExplorerModel *model);

  //! Called when copying the details of a reference explorer item
  void CopyRefExplorerItemDetails(const QModelIndex &index);

  //! Called when attempt to expand a reference explorer item
  void ExpandRefExplorerItem(const QModelIndex &index);

 private slots:
  //! Used to expand and resize the items after a model reset
  void OnModelReset();

  //! Like OnModelReset, but for row insertion
  void OnRowsInserted(const QModelIndex &parent, int first, int last);

  //! Called when the user clicks an item
  void OnItemClick(const QModelIndex &index);

  //! Custom context menu for the tree view items
  void OnOpenItemContextMenu(const QPoint &point);

  //! Called when an action in the context menu is triggered
  void OnContextMenuActionTriggered(QAction *action);

  //! Called by the ISearchWidget component whenever search options change
  void OnSearchParametersChange(
      const ISearchWidget::SearchParameters &search_parameters);

  //! Called when the FilterSettingsWidget options are changed
  void OnFilterParametersChange();

  friend class IReferenceExplorer;
};

}  // namespace mx::gui
