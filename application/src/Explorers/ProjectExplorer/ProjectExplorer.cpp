// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ProjectExplorer.h"

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
  if (!d->view) {
    d->model = new FileTreeModel(this);
    d->view = new FileTreeView(
        d->config_manager.ThemeManager(),
        d->config_manager.MediaManager(),
        d->model,
        parent);

    d->view->setWindowTitle(tr("Project Explorer"));
    OnIndexChanged(d->config_manager);

    connect(d->view, &FileTreeView::RequestContextMenu,
            this, &IMainWindowPlugin::RequestContextMenu);

    connect(d->view, &FileTreeView::RequestPrimaryClick,
            this, &IMainWindowPlugin::RequestPrimaryClick);
  }
  return d->view;
}

void ProjectExplorer::ActOnPrimaryClick(const QModelIndex &index) {
  if (!d->view) {
    return;
  }

  auto selected_index = d->view->SelectedIndex();
  if (index != selected_index || !selected_index.isValid()) {
    return;
  }

  d->open_entity_trigger.Trigger(selected_index.data(IModel::EntityRole));
}

void ProjectExplorer::ActOnContextMenu(QMenu *menu, const QModelIndex &index) {
  if (!d->view) {
    return;
  }

  auto selected_index = d->view->SelectedIndex();
  if (index != selected_index || !selected_index.isValid()) {
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
          [this] (void) {
            d->view->SetRoot(d->view->SelectedIndex());
          });
}

void ProjectExplorer::OnIndexChanged(const ConfigManager &config_manager) {
  if (d->model) {
    d->model->SetIndex(config_manager.Index());
  }
}

}  // namespace mx::gui
