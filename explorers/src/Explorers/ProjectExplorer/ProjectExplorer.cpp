// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/ProjectExplorer.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>

#include "FileTreeModel.h"
#include "FileTreeView.h"

namespace mx::gui {

struct ProjectExplorer::PrivateData {
  const ConfigManager &config_manager;
  FileTreeModel *model{nullptr};
  FileTreeView *view{nullptr};

  QModelIndex context_index;
  QModelIndex clicked_index;

  // Action for opening an entity when the selection is changed.
  const TriggerHandle open_entity_trigger;

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_),
        open_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntity")) {}
};

ProjectExplorer::~ProjectExplorer(void) {}

ProjectExplorer::ProjectExplorer(ConfigManager &config_manager,
                                 QMainWindow *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &ProjectExplorer::OnIndexChanged);
}

QWidget *ProjectExplorer::CreateDockWidget(QWidget *parent) {
  if (d->view) {
    return d->view;
  }

  d->model = new FileTreeModel(this);
  d->view = new FileTreeView(d->config_manager, d->model, parent);

  d->view->setWindowTitle(tr("Project Explorer"));
  OnIndexChanged(d->config_manager);

  connect(d->view, &FileTreeView::RequestContextMenu,
          [this] (const QModelIndex &index) {
            d->context_index = index;
            emit RequestContextMenu(d->context_index);
          });

  connect(d->view, &FileTreeView::RequestPrimaryClick,
          [this] (const QModelIndex &index) {
            d->clicked_index = index;
            emit RequestPrimaryClick(d->clicked_index);
          });

  return d->view;
}

void ProjectExplorer::ActOnPrimaryClick(const QModelIndex &index) {
  auto clicked_index = std::move(d->clicked_index);
  if (!d->view || clicked_index != index || !index.isValid()) {
    return;
  }

  d->open_entity_trigger.Trigger(index.data(IModel::EntityRole));
}

void ProjectExplorer::ActOnContextMenu(QMenu *menu, const QModelIndex &index) {
  auto context_index = std::move(d->context_index);
  if (!d->view || context_index != index || !index.isValid() ||
      !d->view->isVisible()) {
    return;
  }

  auto copy_full_path = new QAction(tr("Copy Path"), menu);
  menu->addAction(copy_full_path);
  auto full_path = index.data(FileTreeModel::AbsolutePathRole).toString();

  auto set_root_action = new QAction(tr("Set As Root"), menu);
  menu->addAction(set_root_action);

  auto sort_menu = new QMenu(tr("Sort..."), menu);
  auto sort_ascending_order = new QAction(tr("Ascending Order"), sort_menu);
  sort_menu->addAction(sort_ascending_order);

  auto sort_descending_order = new QAction(tr("Descending Order"), sort_menu);
  sort_menu->addAction(sort_descending_order);

  menu->addMenu(sort_menu);

  connect(sort_ascending_order, &QAction::triggered,
          [this] (void) {
            d->view->SortAscending();
          });

  connect(sort_descending_order, &QAction::triggered,
          [this] (void) {
            d->view->SortDescending();
          });

  connect(copy_full_path, &QAction::triggered,
          [=] (void) {
            qApp->clipboard()->setText(full_path);
          });

  connect(set_root_action, &QAction::triggered,
          [=, this] (void) {
            d->view->SetRoot(index);
          });
}

void ProjectExplorer::OnIndexChanged(const ConfigManager &config_manager) {
  if (d->model) {
    d->model->SetIndex(config_manager.Index());
  }
}

}  // namespace mx::gui
