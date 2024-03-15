// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Managers/Registry.h>
#include <multiplier/GUI/Managers/ConfigManager.h>

#include <memory>

#include <QWidget>
#include <QLayout>

namespace mx::gui {

class ConfigEditor Q_DECL_FINAL : public QWidget {
  Q_OBJECT

 public:
  static ConfigEditor *Create(const ConfigManager &config_manager,
                              Registry &registry, QWidget *parent);
  virtual ~ConfigEditor(void) override;

  ConfigEditor(ConfigEditor &) = delete;
  ConfigEditor &operator=(const ConfigEditor &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  ConfigEditor(const ConfigManager &config_manager, Registry &registry,
               QWidget *parent);

  void InitializeWidgets(const ConfigManager &config_manager,
                         Registry &registry);

 private slots:
  void OnModelReset(void);
  void OnSearchParametersChange(void);
};

}  // namespace mx::gui
