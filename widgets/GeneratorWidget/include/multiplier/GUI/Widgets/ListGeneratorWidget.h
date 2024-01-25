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

#include <multiplier/GUI/Interfaces/IListGenerator.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>

namespace mx::gui {

class ConfigManager;
class IWindowManager;
class MediaManager;
class ThemeManager;

//! A concrete implementation for the IGeneratorView interface
class ListGeneratorWidget Q_DECL_FINAL : public IWindowWidget {
  Q_OBJECT

  struct PrivateData;
  const std::unique_ptr<PrivateData> d;

  void InstallModel(void);

  void InitializeWidgets(const ConfigManager &config_manager);

 public:

  //! Destructor
  virtual ~ListGeneratorWidget(void);

  //! Constructor
  ListGeneratorWidget(const ConfigManager &config_manager,
                      const QString &model_id, QWidget *parent = nullptr);

  //! Called when we want to act on the context menu.
  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index);
  
  //! Install a new generator.
  void InstallGenerator(IListGeneratorPtr generator);

 private:

  //! Used to update the OSD buttons
  void resizeEvent(QResizeEvent *event) Q_DECL_FINAL;

  //! Used to hide the OSD buttons when focus is lost
  void focusOutEvent(QFocusEvent *event) Q_DECL_FINAL;

  //! Used to handle clicks and hover events
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

  //! Called when the user clicks an item
  void OnItemActivated(const QModelIndex &current_index);

  //! Custom context menu for the tree view items
  void OnOpenItemContextMenu(const QPoint &point);

  //! Called by the ISearchWidget component whenever search options change
  void OnSearchParametersChange(void);

  //! Called when the "expand" item button has been pressed
  void OnGotoOriginalButtonPressed(void);

  //! Called by the media manager
  void OnIconsChanged(const MediaManager &media_manager);

  //! Called by the theme manager
  void OnThemeChanged(const ThemeManager &theme_manager);

  //! Called when a new model request is started
  void OnModelRequestStarted(void);

  //! Called when the model request has finished
  void OnModelRequestFinished(void);
};

}  // namespace mx::gui
