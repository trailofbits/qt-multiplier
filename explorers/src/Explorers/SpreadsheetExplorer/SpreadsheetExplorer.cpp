// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/SpreadsheetExplorer.h>

#include <QAction>
#include <QMenu>
#include <QVBoxLayout>

#include <multiplier/GUI/Widgets/SimpleTextInputDialog.h>

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
  int sheet_counter{0};

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

  // Create the dock eagerly (hidden) so it appears in View > Explorers.
  CreateDockWidget(parent);

  // Add "New Spreadsheet" to the File menu.
  auto *new_sheet_action = new QAction(tr("New Sheet"), this);
  new_sheet_action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
  connect(new_sheet_action, &QAction::triggered,
          this, [this] () { OnNewBlankSheet({}); });

  auto *file_menu = parent->Menu(tr("File"));
  if (file_menu) {
    file_menu->addAction(new_sheet_action);
  }
}

void SpreadsheetExplorer::CreateDockWidget(IWindowManager *manager) {
  d->dock = new IWindowWidget;
  d->dock->setWindowTitle(tr("Sheets"));
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

  // Double-click on a tab renames it; on empty area creates a new sheet.
  connect(d->tab_widget->tabBar(), &QTabBar::tabBarDoubleClicked,
          this, [this] (int index) {
    if (index == -1) {
      OnNewBlankSheet({});
    } else {
      // Rename tab.
      auto current_name = d->tab_widget->tabText(index);
      SimpleTextInputDialog dialog(tr("Enter the new tab name"),
                                   current_name, d->tab_widget);
      dialog.setWindowTitle(tr("Rename Sheet"));
      if (dialog.exec() == QDialog::Accepted) {
        auto opt_name = dialog.TextInput();
        if (opt_name.has_value() && !opt_name->isEmpty()) {
          d->tab_widget->setTabText(index, opt_name.value());
        }
      }
    }
  });

  // Right-click on tab bar shows context menu.
  d->tab_widget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(d->tab_widget->tabBar(), &QWidget::customContextMenuRequested,
          this, [this] (const QPoint &pos) {
    int index = d->tab_widget->tabBar()->tabAt(pos);
    QMenu menu(d->tab_widget);

    menu.addAction(tr("New Sheet"), this, [this] () {
      OnNewBlankSheet({});
    });

    if (index >= 0) {
      menu.addSeparator();
      menu.addAction(tr("Rename..."), this, [this, index] () {
        auto current_name = d->tab_widget->tabText(index);
        SimpleTextInputDialog dialog(tr("Enter the new tab name"),
                                     current_name, d->tab_widget);
        dialog.setWindowTitle(tr("Rename Sheet"));
        if (dialog.exec() == QDialog::Accepted) {
          auto opt_name = dialog.TextInput();
          if (opt_name.has_value() && !opt_name->isEmpty()) {
            d->tab_widget->setTabText(index, opt_name.value());
          }
        }
      });
      menu.addAction(tr("Close"), this, [this, index] () {
        auto *widget = d->tab_widget->widget(index);
        d->tab_widget->RemoveTab(index);
        widget->deleteLater();
        if (!d->tab_widget->count()) {
          d->dock->hide();
        }
      });
    }

    menu.exec(d->tab_widget->tabBar()->mapToGlobal(pos));
  });

  auto layout = new QVBoxLayout(d->dock);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tab_widget, 1);
  layout->addStretch();
  d->dock->setLayout(layout);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.Sheets";
  config.location = IWindowManager::DockLocation::Bottom;
  config.tabify = true;
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->dock, config);

  d->dock->hide();
}

void SpreadsheetExplorer::OnNewBlankSheet(const QVariant &) {

  auto *model = new SpreadsheetModel;

  // Create a blank sheet with some columns and empty rows.
  QVector<ColumnDefinition> cols;
  cols.push_back({tr("Name"), 0});
  cols.push_back({tr("Value"), 1});
  cols.push_back({tr("Active"), 2});

  QVector<QVector<QVariant>> rows;
  rows.resize(8);
  for (auto &row : rows) {
    row.resize(3);
  }

  // Populate a few rows with test data.
  rows[0][0] = QStringLiteral("Hello");
  rows[0][1] = QStringLiteral("World");
  rows[0][2] = true;
  rows[1][0] = QStringLiteral("Foo");
  rows[1][1] = QStringLiteral("Bar");
  rows[1][2] = false;
  rows[2][0] = QStringLiteral("Editable cell");
  rows[2][2] = true;

  model->populate_from_results(cols, rows);

  auto *view = new SpreadsheetView(d->tab_widget);
  view->setModel(model);
  ++(d->sheet_counter);
  view->setWindowTitle(tr("Sheet %1").arg(d->sheet_counter));

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
