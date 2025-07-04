// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

namespace mx::gui {

class ProjectExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~ProjectExplorer(void);

  ProjectExplorer(ConfigManager &config_manager,
                  IWindowManager *parent = nullptr);

  void ActOnPrimaryClick(
      IWindowManager *manager, const QModelIndex &index) Q_DECL_FINAL;

  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index) Q_DECL_FINAL;

 private:
  void CreateDockWidget(IWindowManager *manager);

 private slots:
  void OnIndexChanged(const ConfigManager &config_manager);
};

}  // namespace mx::gui
