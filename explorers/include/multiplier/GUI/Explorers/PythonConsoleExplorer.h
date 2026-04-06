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

class PythonConsoleExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~PythonConsoleExplorer(void);

  explicit PythonConsoleExplorer(ConfigManager &config_manager,
                                 IWindowManager *parent = nullptr);

  void ActOnPrimaryClick(IWindowManager *manager,
                         const QModelIndex &index) Q_DECL_FINAL;

 private:
  void CreateDockWidget(IWindowManager *manager);

 private slots:
  void OnOpenPythonConsole(const QVariant &data);
  void OnIndexChanged(const ConfigManager &config_manager);
};

}  // namespace mx::gui
