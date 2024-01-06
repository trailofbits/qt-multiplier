// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

namespace mx::gui {

class InformationExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~InformationExplorer(void);

  InformationExplorer(ConfigManager &config_manager,
                      QMainWindow *parent = nullptr);

  QWidget *CreateDockWidget(QWidget *parent) Q_DECL_FINAL;

  void ActOnPrimaryClick(const QModelIndex &index) Q_DECL_FINAL;

  std::optional<NamedAction> ActOnSecondaryClick(
      const QModelIndex &index) Q_DECL_FINAL;

  std::optional<NamedAction> ActOnKeyPress(
      const QKeySequence &keys, const QModelIndex &index) Q_DECL_FINAL;
};

}  // namespace mx::gui
