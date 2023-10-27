/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ITreeExplorer.h>
#include <multiplier/ui/ISearchWidget.h>
#include <multiplier/ui/IThemeManager.h>

namespace mx::gui {

//! A treeview-based implementation for the ITreeExplorer interface
class TreeExplorer final : public ITreeExplorer {
  Q_OBJECT

 public:
  //! Destructor
  ~TreeExplorer() Q_DECL_FINAL;

  //! \copybrief ITreeExplorer::Model
  ITreeExplorerModel *Model() const Q_DECL_FINAL;

 protected:
  //! Used to update the button positions
  void resizeEvent(QResizeEvent *event) Q_DECL_FINAL;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  TreeExplorer(ITreeExplorerModel *model, QWidget *parent,
               IGlobalHighlighter *global_highlighter);

  //! Initializes the internal widgets
  void InitializeWidgets(ITreeExplorerModel *model);

  //! Installs the given model
  void InstallModel(ITreeExplorerModel *model,
                    IGlobalHighlighter *global_highlighter);

  //! Called when copying the details of a reference explorer item
  void CopyTreeExplorerItemDetails(const QModelIndex &index);

  //! Called when attempt to expand a reference explorer item
  void ExpandTreeExplorerItem(const QModelIndex &index);

  //! Used for the tree view hover events
  bool eventFilter(QObject *obj, QEvent *event) Q_DECL_FINAL;

  //! Updates the treeview item hover buttons
  void UpdateTreeViewItemButtons();

  //! Updates the icons based on the active theme
  void UpdateIcons();

  void InstallItemDelegate(const CodeViewTheme &code_view_theme);

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

  //! Called when the "open" item button has been pressed
  void OnActivateTreeViewItem();

  //! Called when the "expand" item button has been pressed
  void OnExpandTreeViewItem();

  //! Called when the "expand" item button has been pressed
  void OnGotoOriginalTreeViewItem();

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

  //! Called when a new model request is started
  void OnModelRequestStarted();

  //! Called when the model request has finished
  void OnModelRequestFinished();

  friend class ITreeExplorer;
};

}  // namespace mx::gui
