// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>

#include <memory>

class QTableView;

namespace mx::gui {

class ConfigManager;
class IWindowManager;

class CodeSearchExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~CodeSearchExplorer(void);

  explicit CodeSearchExplorer(ConfigManager &config_manager,
                              IWindowManager *parent = nullptr);

  void ActOnPrimaryClick(IWindowManager *manager,
                         const QModelIndex &index) Q_DECL_FINAL;

  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index) Q_DECL_FINAL;

 private:
  void CreateDockWidget(IWindowManager *manager);
  QTableView *CreateResultsTable(QWidget *parent);

 private slots:
  void OnSearchTriggered(void);
  void OnOpenCodeSearch(const QVariant &data);
  void OnIndexChanged(const ConfigManager &config_manager);
  void OnCurrentChanged(const QModelIndex &current, QWidget *container);
  void OnTabClose(int index);
  void OnSearchFinished(void);
};

}  // namespace mx::gui
