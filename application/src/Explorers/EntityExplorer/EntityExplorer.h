// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

namespace mx::gui {

class EntityExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~EntityExplorer(void);

  EntityExplorer(ConfigManager &config_manager,
                 QMainWindow *parent = nullptr);

  QWidget *CreateDockWidget(QWidget *parent) Q_DECL_FINAL;
  void ActOnPrimaryClick(const QModelIndex &index) Q_DECL_FINAL;
  void ActOnContextMenu(QMenu *menu, const QModelIndex &index) Q_DECL_FINAL;

 private slots:
  void OnIndexChanged(const ConfigManager &config_manager);
};

}  // namespace mx::gui
