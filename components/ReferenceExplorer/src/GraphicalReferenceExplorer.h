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

//! A treeview-based implementation for the IReferenceExplorer interface
class GraphicalReferenceExplorer final : public IReferenceExplorer {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~GraphicalReferenceExplorer() override;

  //! \copybrief IReferenceExplorer::Model
  virtual IReferenceExplorerModel *Model() override;

 protected:
  //! Used to update the button positions
  virtual void resizeEvent(QResizeEvent *event) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  GraphicalReferenceExplorer(IReferenceExplorerModel *model, QWidget *parent,
                             IGlobalHighlighter *global_highlighter);

  //! Initializes the internalwidgets
  void InitializeWidgets();

  //! Installs the given model
  void InstallModel(IReferenceExplorerModel *model,
                    IGlobalHighlighter *global_highlighter);

  //! Called when copying the details of a reference explorer item
  void CopyRefExplorerItemDetails(const QModelIndex &index);

  //! Called when attempt to delete a reference explorer item
  void RemoveRefExplorerItem(const QModelIndex &index);

  //! Called when attempt to expand a reference explorer item
  void ExpandRefExplorerItem(const QModelIndex &index);

  //! Used for the tree view hover events
  virtual bool eventFilter(QObject *obj, QEvent *event) override;

  //! Updates the treeview item hover buttons
  void UpdateTreeViewItemButtons();

 private slots:
  //! Used to expand and resize the items after a model reset
  void OnModelReset();

  //! \brief Handles item button invalidation
  //! Currently only used to update the item buttons when the
  //! ExpansionStatusRole changes
  void OnDataChanged();

  //! Automatically expands all nodes
  void ExpandAllNodes();

  //! Used to automatically select the first inserted root
  void OnRowsInserted(const QModelIndex &parent, int first, int last);

  //! Called when the user selects an item
  void OnCurrentItemChanged(const QModelIndex &current_index,
                            const QModelIndex &previous_index);

  //! Custom context menu for the tree view items
  void OnOpenItemContextMenu(const QPoint &point);

  //! Called when an action in the context menu is triggered
  void OnContextMenuActionTriggered(QAction *action);

  //! Called by the ISearchWidget component whenever search options change
  void OnSearchParametersChange(
      const ISearchWidget::SearchParameters &search_parameters);

  //! Called when the FilterSettingsWidget options are changed
  void OnFilterParametersChange();

  //! Called when the user disables the custom root item from the warning widget
  void OnDisableCustomRootLinkClicked();

  //! Called when the "open" item button has been pressed
  void OnActivateTreeViewItem();

  //! Called when the "close" item button has been pressed
  void OnCloseTreeViewItem();

  //! Called when the "expand" item button has been pressed
  void OnExpandTreeViewItem();

  friend class ReferenceExplorer;
};

}  // namespace mx::gui
