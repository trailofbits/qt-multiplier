/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QPoint>
#include <QResizeEvent>
#include <QWidget>

#include <multiplier/GUI/Interfaces/ITreeGenerator.h>

namespace mx::gui {

class ConfigManager;
class MediaManager;
class ThemeManager;
class TreeGeneratorModel;

//! A concrete implementation for the IGeneratorView interface
class TreeGeneratorWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;

  //! Private instance data
  const std::unique_ptr<PrivateData> d;

  void InstallModel(TreeGeneratorModel *model);

  void InitializeWidgets(const ConfigManager &config_manager,
                         TreeGeneratorModel *model);

 public:

  //! Constructor
  TreeGeneratorWidget(const ConfigManager &config_manager,
                      ITreeGeneratorPtr generator,
                      QWidget *parent = nullptr);

  //! Destructor
  virtual ~TreeGeneratorWidget(void);

  //! Called when copying the details of a reference explorer item
  void CopyItemDetails(const QModelIndex &index);

  //! Called when attempt to expand a tree item.
  void ExpandItem(const QModelIndex &index, unsigned depth=1u);

  //! Called when attempt to go to the original verison of a duplicate item.
  void GotoOriginalItem(const QModelIndex &index);

 private:

  //! Called when attempt to go to the original verison of a duplicate item.
  void GotoOriginalItem(void);

  //! Used to update the OSD buttons
  void resizeEvent(QResizeEvent *event) Q_DECL_FINAL;

  //! Used to hide the OSD buttons when focus is lost
  void focusOutEvent(QFocusEvent *event) Q_DECL_FINAL;

  //! Used for the tree view hover events
  bool eventFilter(QObject *obj, QEvent *event) Q_DECL_FINAL;

  //! Updates the treeview item hover buttons
  void UpdateItemButtons(void);

 private slots:
  //! Used to expand and resize the items after a model reset
  void OnModelReset(void);

  //! \brief Handles item button invalidation
  //! Currently only used to update the item buttons when the
  //! ExpansionStatusRole changes
  void OnDataChanged(void);

  //! Automatically expands all nodes
  void ExpandAllNodes(void);

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
  void OnSearchParametersChange(void);

  //! Called when the "open" item button has been pressed
  void OnActivateItem(void);

  //! Called when the "expand" item button has been pressed
  void OnExpandItem(void);

  //! Called when the "expand" item button has been pressed
  void OnGotoOriginalItem(void);

  //! Called by the media manager
  void OnIconsChanged(const MediaManager &media_manager);

  //! Called by the theme manager
  void OnThemeChanged(const ThemeManager &theme_manager);

  //! Called when a new model request is started
  void OnModelRequestStarted(void);

  //! Called when the model request has finished
  void OnModelRequestFinished(void);
 
 signals:
  //! This tells us when a specific item in the tree is activated, i.e. when
  //! the "activate" (typically open) button is pressed.
  void ItemActivated(const QModelIndex &index);

  //! This tells us when a specific item in the tree is selected.
  void SelectedItemChanged(const QModelIndex &index);
};

}  // namespace mx::gui
