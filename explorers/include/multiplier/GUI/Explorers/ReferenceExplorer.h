// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>
#include <multiplier/GUI/Interfaces/IReferenceExplorerPlugin.h>

namespace mx::gui {

class ReferenceExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:

  virtual ~ReferenceExplorer(void);

  ReferenceExplorer(ConfigManager &config_manager,
                    IWindowManager *parent = nullptr);

  void ActOnPrimaryClick(
      IWindowManager *manager, const QModelIndex &index) Q_DECL_FINAL;

  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index) Q_DECL_FINAL;

  void ActOnLongHover(IWindowManager *manager,
                      const QModelIndex &index) Q_DECL_FINAL;

  std::vector<NamedAction> ActOnKeyPressEx(
      IWindowManager *manager, const QKeySequence &keys,
      const QModelIndex &index) Q_DECL_FINAL;

  void AddPlugin(IReferenceExplorerPluginPtr plugin);

  template <typename T, typename... Args>
  inline void EmplacePlugin(Args&&... args) {
    AddPlugin(IReferenceExplorerPluginPtr(new T(std::forward<Args>(args)...)));
  }

 private:
  void CreateDockWidget(IWindowManager *parent);
  friend class IReferenceExplorerPlugin;

  void OnOpenReferenceExplorer(const QVariant &data);

 private slots:
  void OnTabBarClose(int index);
  void OnTabBarDoubleClick(int index);
  void OnSelectionChange(const QModelIndex &index);
};

}  // namespace mx::gui
