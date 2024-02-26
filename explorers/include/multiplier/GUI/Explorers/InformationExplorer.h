// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>
#include <multiplier/GUI/Interfaces/IInformationExplorerPlugin.h>

namespace mx::gui {

class InformationExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~InformationExplorer(void);

  InformationExplorer(ConfigManager &config_manager,
                      IWindowManager *parent = nullptr);

  void ActOnPrimaryClick(
      IWindowManager *manager, const QModelIndex &index) Q_DECL_FINAL;

  std::optional<NamedAction> ActOnSecondaryClick(
      IWindowManager *manager, const QModelIndex &index) Q_DECL_FINAL;

  std::optional<NamedAction> ActOnKeyPress(
      IWindowManager *manager, const QKeySequence &keys,
      const QModelIndex &index) Q_DECL_FINAL;

  void AddPlugin(IInformationExplorerPluginPtr plugin);

  template <typename T, typename... Args>
  inline void EmplacePlugin(Args&&... args) {
    AddPlugin(IInformationExplorerPluginPtr(new T(std::forward<Args>(args)...)));
  }

 private:
  //! Called when we want to act on the context menu.
  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index);

  void CreateDockWidget(IWindowManager *manager);
  void OpenInfo(const QVariant &data, bool is_explicit);
  void OpenInfoImplicit(const QVariant &data);
  void OpenInfoExplicit(const QVariant &data);
  void OpenPinnedInfo(const QVariant &data);

 private slots:
  void OnHistoricalEntitySelected(const QVariant &data);
  void OnSelectedItemChanged(const QModelIndex &index);
};

}  // namespace mx::gui
