// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/InformationExplorer.h>

#include <QThreadPool>

#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>

#include "EntityInformationRunnable.h"
#include "EntityInformationWidget.h"

namespace mx::gui {

struct InformationExplorer::PrivateData {
  ConfigManager &config_manager;

  EntityInformationModel *model{nullptr};
  EntityInformationWidget *view{nullptr};

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_) {}
};

InformationExplorer::~InformationExplorer(void) {}

InformationExplorer::InformationExplorer(ConfigManager &config_manager,
                                         QMainWindow *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {}

QWidget *InformationExplorer::CreateDockWidget(QWidget *parent) {
  if (d->view) {
    return d->view;
  }

  d->view = new EntityInformationWidget(d->config_manager, true, parent);
  d->config_manager.ActionManager().Register(
      d->view, "com.trailofbits.action.OpenEntity",
      &EntityInformationWidget::DisplayEntity);

  return d->view;
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

}  // namespace mx::gui
