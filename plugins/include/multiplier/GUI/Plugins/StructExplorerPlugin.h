// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IReferenceExplorerPlugin.h>

#include <memory>

namespace mx::gui {

// Implements the struct explorer, which shows structure offsets
class StructExplorerPlugin Q_DECL_FINAL : public IReferenceExplorerPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~StructExplorerPlugin(void);

  StructExplorerPlugin(ConfigManager &config_manager,
                       QObject *parent = nullptr);

  std::optional<NamedAction>
  ActOnSecondaryClick(IWindowManager *manager,
                      const QModelIndex &index) Q_DECL_FINAL;

  // Allow a main window plugin to act on a key sequence.
  std::optional<NamedAction>
  ActOnKeyPress(IWindowManager *manager, const QKeySequence &keys,
                const QModelIndex &index) Q_DECL_FINAL;
};

}  // namespace mx::gui
