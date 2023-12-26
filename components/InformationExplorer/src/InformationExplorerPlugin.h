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

class InformationExplorerWidget;

class UpdateInformationExplorerAction Q_DECL_FINAL : public IAction {
  Q_OBJECT

 public:
  using IAction::IAction;

  InformationExplorerWidget *widget{nullptr};

  virtual ~UpdateInformationExplorerAction(void) = default;

  QString Verb(void) const noexcept Q_DECL_FINAL;
  void Run(const QVariant &input) noexcept Q_DECL_FINAL;
};

class InformationExplorerPlugin Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  const Context &context;

  QMainWindow * const main_window;

  // Action for controlling the "primary" information explorer. This is the
  // information explorer that is menu-controlled and docked. The contents of
  // the primary info explorer changes as things are clicked on. Secondary,
  // fixed information explorers can be added with `Shift-I`. 
  UpdateInformationExplorerAction update_primary;
  const TriggerHandle update_primary_trigger;

 public:
  virtual ~InformationExplorerPlugin(void);

  InformationExplorerPlugin(const Context &context_, QMainWindow *parent);

  void ActOnSecondaryClick(QMenu *menu, const QModelIndex &index) Q_DECL_FINAL;

  QWidget *CreateDockWidget(QWidget *parent) Q_DECL_FINAL;
};

}  // namespace mx::gui

Q_DECLARE_METATYPE(mx::VariantEntity)
