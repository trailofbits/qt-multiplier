// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

namespace mx::gui {

class IReferenceExplorerPlugin;

class ReferenceExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:

  virtual ~ReferenceExplorer(void);

  ReferenceExplorer(ConfigManager &config_manager, QMainWindow *parent);

  // Act on a primary click. For example, if browse mode is enabled, then this
  // is a "normal" click, however, if browse mode is off, then this is a meta-
  // click.
  void ActOnPrimaryClick(const QModelIndex &index) Q_DECL_FINAL;

  // Allow a main window plugin to act on, e.g. modify, a context menu.
  void ActOnContextMenu(QMenu *menu, const QModelIndex &index) Q_DECL_FINAL;

  // Allow a main window plugin to act on a long hover over something.
  void ActOnLongHover(const QModelIndex &index) Q_DECL_FINAL;

  // Allow a main window plugin to provide one of several actions to be
  // performed on a key press.
  std::vector<NamedAction> ActOnKeyPressEx(
      const QKeySequence &keys, const QModelIndex &index) Q_DECL_FINAL;

  // TODO(pag): Consider removing?
  QWidget *CreateDockWidget(QWidget *parent) Q_DECL_FINAL;

 private:
  friend class IReferenceExplorerPlugin;

  void AddPlugin(std::unique_ptr<IReferenceExplorerPlugin> plugin);

  void OnOpenReferenceExplorer(const QVariant &data);

 private slots:
  void OnTabBarClose(int index);
  void OnTabBarDoubleClick(int index);
};

}  // namespace mx::gui
