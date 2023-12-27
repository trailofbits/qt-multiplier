// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Frontend/File.h>
#include <multiplier/Index.h>
#include <multiplier/ui/ActionRegistry.h>
#include <multiplier/ui/IMainWindowPlugin.h>
#include <multiplier/ui/IReferenceExplorerPlugin.h>

#include <vector>

namespace mx::gui {

class MxTabWidget;

class ReferenceExplorerPlugin Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

 public:
  const Context &context;

  QMainWindow * const main_window;

  // List of plugins.
  std::vector<std::unique_ptr<IReferenceExplorerPlugin>> plugins;

  MxTabWidget *tab_widget{nullptr};

  // TODO(pag): Add a new trigger here that takes in a data generator. The
  //            reference explorer plugins will send them here.

  virtual ~ReferenceExplorerPlugin(void);

  inline ReferenceExplorerPlugin(const Context &context_,
                                 QMainWindow *parent)
      : IMainWindowPlugin(context_, parent),
        context(context_),
        main_window(parent) {}

  // Act on a primary click. For example, if browse mode is enabled, then this
  // is a "normal" click, however, if browse mode is off, then this is a meta-
  // click.
  void ActOnPrimaryClick(const QModelIndex &index) Q_DECL_FINAL;

  std::optional<NamedAction> ActOnSecondaryClick(
      const QModelIndex &index) Q_DECL_FINAL;

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

 private slots:
  void OnTabBarClose(int index);
  void OnTabBarDoubleClick(int index);
};

}  // namespace mx::gui
