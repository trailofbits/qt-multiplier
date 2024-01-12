// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/CodeExplorer.h>

#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Widgets/CodeWidget.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/Index.h>
#include <unordered_map>

namespace mx::gui {

struct CodeExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;

  std::unordered_map<RawEntityId, CodeWidget *> opened_windows;

  inline PrivateData(ConfigManager &config_manager_,
                     IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_) {}
};

CodeExplorer::~CodeExplorer(void) {}

CodeExplorer::CodeExplorer(ConfigManager &config_manager,
                           IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  qDebug() << "initializing code explorer";
  config_manager.ActionManager().Register(
      this, "com.trailofbits.action.OpenEntity",
      &CodeExplorer::OnOpenEntity);
}

void CodeExplorer::ActOnPrimaryClick(IWindowManager *manager,
                                     const QModelIndex &index) {
  (void) manager;
  (void) index;
}

void CodeExplorer::ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                                    const QModelIndex &index) {
  (void) manager;
  (void) menu;
  (void) index;
}

void CodeExplorer::OnOpenEntity(const QVariant &data) {
  if (data.isNull() || !data.canConvert<VariantEntity>()) {
    return;
  }

  VariantEntity entity = data.value<VariantEntity>();
  auto file = File::containing(entity);
  auto frag = Fragment::containing(entity);
  if (!file && !frag) {
    return;
  }

  auto [id, tt] = ([&] (void) -> std::pair<RawEntityId, TokenTree> {
    if (file) {
      return {file->id().Pack(), TokenTree::from(file.value())};
    } else {
      return {frag->id().Pack(), TokenTree::from(frag.value())};
    }
  })();

  auto &code_widget = d->opened_windows[id];
  if (!code_widget) {
    code_widget = new CodeWidget(d->config_manager, tt);

    // Figure out the window title.
    if (file) {
      for (auto path : file->paths()) {
        code_widget->setWindowTitle(QString::fromStdString(
            path.filename().generic_string()));
        break;
      }
    } else {
      // TODO(pag): Choose top-level entity?
    }

    connect(code_widget, &IWindowWidget::Closed,
            [=, this] (void) {
              d->opened_windows.erase(id);
            });

    connect(code_widget, &CodeWidget::RequestPrimaryClick,
            d->manager, &IWindowManager::OnPrimaryClick);

    connect(code_widget, &CodeWidget::RequestSecondaryClick,
            d->manager, &IWindowManager::OnSecondaryClick);

    IWindowManager::CentralConfig config;
    d->manager->AddCentralWidget(code_widget, config);
  } else {
    code_widget->show();
  }
}

void CodeExplorer::OnExpandMacro(RawEntityId entity_id) {
  (void) entity_id;
}

void CodeExplorer::OnRenameEntity(QVector<RawEntityId> entity_ids,
                                  QString new_name) {
  (void) entity_ids;
  (void) new_name;
}

}  // namespace mx::gui
