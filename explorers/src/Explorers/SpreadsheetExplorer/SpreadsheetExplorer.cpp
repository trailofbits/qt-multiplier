// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/SpreadsheetExplorer.h>

#include <QVBoxLayout>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Widgets/SpreadsheetModel.h>
#include <multiplier/GUI/Widgets/SpreadsheetView.h>
#include <multiplier/GUI/Widgets/TabWidget.h>
#include <multiplier/Index.h>

#include <QTabBar>

namespace mx::gui {

struct SpreadsheetExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;

  IWindowWidget *dock{nullptr};
  TabWidget *tab_widget{nullptr};

  inline PrivateData(ConfigManager &config_manager_, IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_) {}
};

SpreadsheetExplorer::~SpreadsheetExplorer(void) {}

SpreadsheetExplorer::SpreadsheetExplorer(ConfigManager &config_manager,
                                         IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  auto &action_manager = config_manager.ActionManager();

  action_manager.Register(
      this, "com.trailofbits.action.NewBlankSheet",
      &SpreadsheetExplorer::OnNewBlankSheet);

  action_manager.Register(
      this, "com.trailofbits.action.OpenInSpreadsheet",
      &SpreadsheetExplorer::OnOpenInSpreadsheet);

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &SpreadsheetExplorer::OnIndexChanged);
}

void SpreadsheetExplorer::CreateDockWidget(IWindowManager *manager) {
  d->dock = new IWindowWidget;
  d->dock->setWindowTitle(tr("Spreadsheets"));
  d->dock->setContentsMargins(0, 0, 0, 0);

  d->tab_widget = new TabWidget(d->dock);
  d->tab_widget->setDocumentMode(true);
  d->tab_widget->setTabsClosable(true);

  connect(d->tab_widget->tabBar(), &QTabBar::tabCloseRequested,
          this, [this] (int i) {
    auto *widget = d->tab_widget->widget(i);
    d->tab_widget->RemoveTab(i);
    widget->deleteLater();
    if (!d->tab_widget->count()) {
      d->dock->hide();
    }
  });

  auto layout = new QVBoxLayout(d->dock);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tab_widget, 1);
  layout->addStretch();
  d->dock->setLayout(layout);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.Spreadsheets";
  config.location = IWindowManager::DockLocation::Bottom;
  config.tabify = true;
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->dock, config);

  d->dock->hide();
}

void SpreadsheetExplorer::OnNewBlankSheet(const QVariant &) {
  if (!d->dock) {
    CreateDockWidget(d->manager);
  }

  auto *model = new SpreadsheetModel;

  // Create a blank sheet with 10 rows and 3 columns.
  QVector<ColumnDefinition> cols;
  cols.push_back({tr("A"), 0});
  cols.push_back({tr("B"), 1});
  cols.push_back({tr("C"), 2});

  QVector<QVector<QVariant>> rows;
  rows.resize(10);
  for (auto &row : rows) {
    row.resize(3);
  }

  model->populate_from_results(cols, rows);

  auto *view = new SpreadsheetView(d->tab_widget);
  view->setModel(model);
  view->setWindowTitle(tr("New Sheet"));

  d->tab_widget->InsertTab(0, view);
  d->dock->show();
  d->dock->EmitRequestAttention();
}

void SpreadsheetExplorer::OnOpenInSpreadsheet(const QVariant &) {
  // TODO: Phase 6 — accept search result data and populate a sheet.
  OnNewBlankSheet({});
}

void SpreadsheetExplorer::ActOnPrimaryClick(
    IWindowManager *, const QModelIndex &) {
  // No primary click handling for spreadsheets.
}

void SpreadsheetExplorer::ActOnContextMenu(
    IWindowManager *, QMenu *, const QModelIndex &) {
  // No global context menu actions.
}

void SpreadsheetExplorer::OnIndexChanged(const ConfigManager &) {
  // TODO: Phase 5 — load persisted sheets from the database.
}

}  // namespace mx::gui
