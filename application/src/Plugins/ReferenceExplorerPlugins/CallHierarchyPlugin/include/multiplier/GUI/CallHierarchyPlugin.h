// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/IReferenceExplorerPlugin.h>

#include <memory>

namespace mx::gui {

// Implements the call hierarchy plugin, which 
class CallHierarchyPlugin Q_DECL_FINAL : public IReferenceExplorerPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~CallHierarchyPlugin(void);

  CallHierarchyPlugin(const Context &context, QObject *parent = nullptr);

  std::optional<NamedAction> ActOnMainWindowSecondaryClick(
      QMainWindow *window, const QModelIndex &index) Q_DECL_FINAL;

  // Allow a main window plugin to act on a key sequence.
  std::optional<NamedAction> ActOnMainWindowKeyPress(
      QMainWindow *window, const QKeySequence &keys,
      const QModelIndex &index) Q_DECL_FINAL;
};

}  // namespace mx::gui
