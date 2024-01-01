// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ProjectExplorer.h"

#include <multiplier/GUI/Managers/ConfigManager.h>

#include "FileTreeModel.h"
#include "FileTreeView.h"

namespace mx::gui {

struct ProjectExplorer::PrivateData {
  const ConfigManager &config_manager;
  FileTreeModel *model{nullptr};
  FileTreeView *view{nullptr};

  inline PrivateData(ConfigManager &config_manager_)
      : config_manager(config_manager_) {}
};

ProjectExplorer::~ProjectExplorer(void) {}

ProjectExplorer::ProjectExplorer(ConfigManager &config_manager,
                                 QMainWindow *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager)) {

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &ProjectExplorer::OnIndexChanged);
}

// Requests a dock wiget from this plugin. Can return `nullptr`.
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
  }
  return d->view;
}

void ProjectExplorer::OnIndexChanged(const ConfigManager &config_manager) {
  if (d->model) {
    d->model->SetIndex(config_manager.Index());
  }
}

}  // namespace mx::gui
