// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IWindowWidget.h>

QT_BEGIN_NAMESPACE
class QTableView;
QT_END_NAMESPACE

namespace mx::gui {

class ConfigManager;
class ExpandedMacrosModel;

class MacroExplorer Q_DECL_FINAL : public IWindowWidget {
  Q_OBJECT

  QTableView *table{nullptr};

 public:
  virtual ~MacroExplorer(void);

  MacroExplorer(const ConfigManager &config_manager,
                ExpandedMacrosModel *model,
                QWidget *parent = nullptr);
};

}  // namespace mx::gui
