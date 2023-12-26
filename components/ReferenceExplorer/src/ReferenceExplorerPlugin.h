// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Frontend/File.h>
#include <multiplier/Index.h>
#include <multiplier/ui/ActionRegistry.h>
#include <multiplier/ui/IAction.h>
#include <multiplier/ui/IMainWindowPlugin.h>

namespace mx::gui {

class MxTabWidget;

class ReferenceExplorerPlugin Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  const Context &context;

  // ActionHandle call_hierarchy_action;

  MxTabWidget *tab_widget{nullptr};

 public:
  virtual ~ReferenceExplorerPlugin(void);

  inline ReferenceExplorerPlugin(const Context &context_,
                                 QMainWindow *parent)
      : IMainWindowPlugin(context_, parent),
        context(context_) {}
        // call_hierarchy_action(
        //     context.Action()) {}

  void ActOnSecondaryClick(QMenu *menu, const QModelIndex &index) Q_DECL_FINAL;

  QWidget *CreateDockWidget(QWidget *parent) Q_DECL_FINAL;

 private slots:
  void OnTabBarClose(int index);
  void OnTabBarDoubleClick(int index);
};

}  // namespace mx::gui
