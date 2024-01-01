// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "EntityExplorer.h"

#include <QAction>
#include <QApplication>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Widgets/ListGeneratorWidget.h>
#include <multiplier/Index.h>

namespace mx::gui {

struct EntityExplorer::PrivateData {
  Index index;

  const ConfigManager &config_manager;
  ListGeneratorWidget *view{nullptr};

  QModelIndex context_index;
  QModelIndex clicked_index;

  // Action for opening an entity when the selection is changed.
  const TriggerHandle open_entity_trigger;

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_),
        open_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntity")) {}
};

EntityExplorer::~EntityExplorer(void) {}

EntityExplorer::EntityExplorer(ConfigManager &config_manager,
                                 QMainWindow *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &EntityExplorer::OnIndexChanged);

  OnIndexChanged(d->config_manager);
}

QWidget *EntityExplorer::CreateDockWidget(QWidget *parent) {
  if (d->view) {
    return d->view;
  }

  d->view = new ListGeneratorWidget(d->config_manager, parent);
  d->view->setWindowTitle(tr("Entity Explorer"));

  connect(d->view, &ListGeneratorWidget::RequestContextMenu,
          [this] (const QModelIndex &index) {
            d->context_index = index;
            emit RequestContextMenu(d->context_index);
          });

  connect(d->view, &ListGeneratorWidget::SelectedItemChanged,
          [this] (const QModelIndex &index) {
            d->clicked_index = index;
            emit RequestPrimaryClick(d->clicked_index);
          });

  return d->view;
}

void EntityExplorer::ActOnPrimaryClick(const QModelIndex &index) {
  if (!d->view || index != d->clicked_index || !index.isValid()) {
    return;
  }

  d->open_entity_trigger.Trigger(index.data(IModel::EntityRole));
}

void EntityExplorer::ActOnContextMenu(QMenu *menu, const QModelIndex &index) {
  if (!d->view || index != d->clicked_index || !index.isValid()) {
    return;
  }

  d->view->ActOnContextMenu(menu, index);
}

void EntityExplorer::OnIndexChanged(const ConfigManager &config_manager) {
  d->index = config_manager.Index();
}

}  // namespace mx::gui
