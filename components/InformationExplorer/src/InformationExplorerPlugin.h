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

namespace mx::gui {

class InformationExplorerWidget;

class InformationExplorerPlugin Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  const Context &context;

  QMainWindow * const main_window;

  // The "primary" information explorer. This is the information explorer that
  // is menu-controlled and docked. The contents of the primary info explorer
  // changes as things are clicked on. Secondary, fixed information explorers
  // can be added with `Shift-I`. 
  InformationExplorerWidget *primary_widget{nullptr};

  // Triggered when we want to change the information displayed by the
  // primary widget.
  const TriggerHandle update_primary_trigger;

  // Action for opening an entity when the selection is changed.
  const TriggerHandle open_entity_trigger;

 public:
  virtual ~InformationExplorerPlugin(void);

  InformationExplorerPlugin(const Context &context_, QMainWindow *parent);

  void ActOnSecondaryClick(QMenu *menu, const QModelIndex &index) Q_DECL_FINAL;

  QWidget *CreateDockWidget(QWidget *parent) Q_DECL_FINAL;
};

}  // namespace mx::gui

Q_DECLARE_METATYPE(mx::VariantEntity)
