// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IReferenceExplorerPlugin.h>

#include <memory>

namespace mx::gui {

// Implements the call hierarchy plugin, which shows recursive users of entities
// in the reference explorer.
class CallHierarchyPlugin Q_DECL_FINAL : public IReferenceExplorerPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~CallHierarchyPlugin(void);

  CallHierarchyPlugin(ConfigManager &config_manager, QObject *parent = nullptr);

  std::optional<NamedAction> ActOnMainWindowSecondaryClick(
      IWindowManager *manager, const QModelIndex &index) Q_DECL_FINAL;

  // Allow a main window plugin to act on a key sequence.
  std::optional<NamedAction> ActOnMainWindowKeyPress(
      IWindowManager *manager, const QKeySequence &keys,
      const QModelIndex &index) Q_DECL_FINAL;
};

}  // namespace mx::gui
