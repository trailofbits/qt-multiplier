// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Context.h>
#include <multiplier/GUI/ActionRegistry.h>
#include <multiplier/GUI/IMainWindowPlugin.h>
#include <multiplier/GUI/IReferenceExplorerPlugin.h>

#include <vector>

namespace mx::gui {

class MxTabWidget;

class ReferenceExplorerPlugin Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

 public:
  const Context &context;

  QMainWindow * const main_window;

  // The tabbed reference explorer widget docked inside of the main window.
  MxTabWidget *tab_widget{nullptr};

  // List of plugins.
  std::vector<std::unique_ptr<IReferenceExplorerPlugin>> plugins;

  // Launches a reference explorer given a data generator.
  TriggerHandle popup_reference_explorer_trigger;

  virtual ~ReferenceExplorerPlugin(void);

  inline ReferenceExplorerPlugin(const Context &context_,
                                 QMainWindow *parent)
      : IMainWindowPlugin(context_, parent),
        context(context_),
        main_window(parent),
        popup_reference_explorer_trigger(context.ActionRegistry().Register(
            this,
            "com.trailofbits.action.OpenReferenceExplorer",
            &ReferenceExplorerPlugin::OnPopupReferenceExplorer)) {}

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
  void OnPopupReferenceExplorer(const QVariant &data);

 private slots:
  void OnTabBarClose(int index);
  void OnTabBarDoubleClick(int index);
};

}  // namespace mx::gui
