// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

namespace mx::gui {

class HighlightExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~HighlightExplorer(void);

  HighlightExplorer(ConfigManager &config_manager,
                    IWindowManager *parent = nullptr);

  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index) Q_DECL_FINAL;

 private:
  void CreateDockWidget(void);
  void ColorsUpdated(void);

 private slots:
  void OnIndexChanged(const ConfigManager &config_manager);
  void SetColor(void);
  void RemoveColor(void);
  void ClearAllColors(void);
};

}  // namespace mx::gui
