// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

#include <memory>

namespace mx::gui {

class ConfigManager;
class IWindowManager;

class DocumentExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~DocumentExplorer(void);

  explicit DocumentExplorer(ConfigManager &config_manager,
                            IWindowManager *parent = nullptr);

  void ActOnPrimaryClick(IWindowManager *manager,
                         const QModelIndex &index) Q_DECL_FINAL;

  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index) Q_DECL_FINAL;

  //! Refresh the document list from the DB.
  void Refresh(void);

 private:
  void CreateDockWidget(IWindowManager *manager);

 private slots:
  void OnNewDocument(const QVariant &data);
  void OnIndexChanged(const ConfigManager &config_manager);
};

}  // namespace mx::gui
