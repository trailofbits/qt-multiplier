// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

namespace mx {
enum class TokenCategory : unsigned char;
}  // namespace mx
namespace mx::gui {

class EntityExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~EntityExplorer(void);

  EntityExplorer(ConfigManager &config_manager,
                 IWindowManager *parent = nullptr);

  void ActOnPrimaryClick(
      IWindowManager *manager, const QModelIndex &index) Q_DECL_FINAL;

  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index) Q_DECL_FINAL;

 private:
  void CreateDockWidget(IWindowManager *manager);

 private slots:
  void OnSearchShortcutTriggered(void);
  void QueryParametersChanged(void);
  void OnIndexChanged(const ConfigManager &config_manager);
  void OnCategoryChanged(std::optional<TokenCategory> token_category);
};

}  // namespace mx::gui
