// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/InformationExplorer.h>

#include <QThreadPool>

#include "EntityInformationRunnable.h"

namespace mx::gui {

struct InformationExplorer::PrivateData {
  QThreadPool thread_pool;
};

InformationExplorer::~InformationExplorer(void) {}

InformationExplorer::InformationExplorer(ConfigManager &config_manager,
                                         QMainWindow *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData) {}

QWidget *InformationExplorer::CreateDockWidget(QWidget *parent) {
  (void) parent;
  return nullptr;
}

void InformationExplorer::ActOnPrimaryClick(const QModelIndex &index) {
  (void) index;
}

std::optional<NamedAction> InformationExplorer::ActOnSecondaryClick(
    const QModelIndex &index) {
  (void) index;
  return std::nullopt;
}

std::optional<NamedAction> InformationExplorer::ActOnKeyPress(
    const QKeySequence &keys, const QModelIndex &index) {
  (void) keys;
  (void) index;
  return std::nullopt;
}

void InformationExplorer::OnIndexChanged(const ConfigManager &config_manager) {
  (void) config_manager;
}

}  // namespace mx::gui
