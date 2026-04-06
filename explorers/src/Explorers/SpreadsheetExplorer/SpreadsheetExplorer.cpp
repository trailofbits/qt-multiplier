// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/SpreadsheetExplorer.h>

#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <multiplier/GUI/Widgets/SimpleTextInputDialog.h>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Interfaces/ITheme.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Widgets/SpreadsheetDelegate.h>
#include <multiplier/GUI/Widgets/SpreadsheetModel.h>
#include <multiplier/GUI/Widgets/SpreadsheetView.h>
#include <multiplier/GUI/Widgets/TabWidget.h>
#include <multiplier/Index.h>

#include <QTabBar>

namespace mx::gui {

struct SheetTab {
  SpreadsheetModel *model{nullptr};
  SpreadsheetView *view{nullptr};
  QWidget *container{nullptr};
  int sheet_id{-1};
};

struct SpreadsheetExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;

  IWindowWidget *dock{nullptr};
  TabWidget *tab_widget{nullptr};
  int sheet_counter{0};

  // Track per-tab state.
  std::unordered_map<QWidget *, SheetTab> tabs;

  inline PrivateData(ConfigManager &config_manager_, IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_) {}

  void SaveAllSheets(void) const;
};

void SpreadsheetExplorer::PrivateData::SaveAllSheets(void) const {
  for (const auto &[widget, tab] : tabs) {
    ConfigManager::SheetData data;
    data.sheet_id = tab.sheet_id;
    data.name = tab.container->windowTitle();

    auto *model = tab.model;
    int num_cols = model->columnCount();
    int num_rows = model->rowCount();

    // Columns.
    for (int c = 0; c < num_cols; ++c) {
      QString col_name = model->headerData(c, Qt::Horizontal).toString();
      QColor col_color = model->ColumnColor(c);
      data.columns.push_back({col_name, col_color});
    }

    // Cells.
    data.cells.resize(num_rows);
    for (int r = 0; r < num_rows; ++r) {
      data.cells[r].resize(num_cols);
      for (int c = 0; c < num_cols; ++c) {
        QVariant cell = model->data(model->index(r, c),
                                    SpreadsheetRoles::RawValueRole);
        if (cell.isValid()) {
          data.cells[r][c] = SpreadsheetModel::value_to_json(cell);
        }
      }
    }

    // Row colors.
    for (int r = 0; r < num_rows; ++r) {
      QColor rc = model->RowColor(r);
      if (rc.isValid()) {
        data.row_colors[r] = rc;
      }
    }

    int new_id = config_manager.SaveSheet(data);
    // Update the stored sheet_id for future saves.
    const_cast<SheetTab &>(tab).sheet_id = new_id;
  }
}

SpreadsheetExplorer::~SpreadsheetExplorer(void) {
  d->SaveAllSheets();
}

SpreadsheetExplorer::SpreadsheetExplorer(ConfigManager &config_manager,
                                         IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  auto &action_manager = config_manager.ActionManager();
  auto &media_manager = config_manager.MediaManager();

  auto new_sheet_trigger = action_manager.Register(
      this, "com.trailofbits.action.NewBlankSheet",
      &SpreadsheetExplorer::OnNewBlankSheet);

  action_manager.Register(
      this, "com.trailofbits.action.OpenInSpreadsheet",
      &SpreadsheetExplorer::OnOpenInSpreadsheet);

  // Toolbar button for creating a new sheet.
  NamedAction new_sheet_named_action;
  new_sheet_named_action.name = tr("New Sheet");
  new_sheet_named_action.action = new_sheet_trigger;
  parent->AddToolBarButton(
      media_manager.Icon("com.trailofbits.icon.NewSheet"),
      new_sheet_named_action);

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

  // Load persisted sheets from the project database.
  // The DB is already open because InitializePlugins runs after SetIndex.
  OnIndexChanged(config_manager);
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
    // Delete from DB if it was saved.
    auto it = d->tabs.find(widget);
    if (it != d->tabs.end()) {
      if (it->second.sheet_id >= 0) {
        d->config_manager.DeleteSheet(it->second.sheet_id);
      }
      d->tabs.erase(it);
    }
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
  cols.push_back({tr("A"), 0});
  cols.push_back({tr("B"), 1});
  cols.push_back({tr("C"), 2});

  QVector<QVector<QVariant>> rows;
  rows.resize(8);
  for (auto &row : rows) {
    row.resize(3);
  }

  model->populate_from_results(cols, rows);

  // Container widget: toolbar at bottom + view.
  auto *container = new QWidget(d->tab_widget);
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  auto *view = new SpreadsheetView(container);
  view->setModel(model);

  // Install themed delegate for syntax-highlighted token rendering.
  auto &theme_manager = d->config_manager.ThemeManager();
  auto *delegate = new SpreadsheetDelegate(
      theme_manager.Theme(), d->config_manager.TabWidth(), view);
  view->setItemDelegate(delegate);

  // Update tab width when it changes.
  connect(&d->config_manager, &ConfigManager::TabWidthChanged,
          view, [delegate] (unsigned tw) { delegate->SetTabWidth(tw); });

  // Apply theme colors to headers, grid, delegate, and font.
  auto apply_theme = [view, delegate] (const ThemeManager &tm) {
    auto theme = tm.Theme();
    view->ApplyThemeColors(
        theme->GutterBackgroundColor(),
        theme->GutterForegroundColor(),
        theme->GutterBackgroundColor().darker(120));
    view->setFont(theme->Font());
    delegate->SetTheme(theme);
  };
  apply_theme(theme_manager);
  connect(&theme_manager, &ThemeManager::ThemeChanged, view, apply_theme);

  layout->addWidget(view, 1);

  // Bottom toolbar for row/column operations.
  auto *toolbar = new QToolBar(container);
  toolbar->setIconSize(QSize(16, 16));

  auto *add_row = toolbar->addAction(tr("+ Row"));
  connect(add_row, &QAction::triggered, view, [model, view] () {
    auto sel = view->selectionModel()->currentIndex();
    int row = sel.isValid() ? sel.row() + 1
                            : model->rowCount();
    model->insertRow(row);
  });

  auto *add_col = toolbar->addAction(tr("+ Col"));
  connect(add_col, &QAction::triggered, view, [model, view] () {
    auto sel = view->selectionModel()->currentIndex();
    int col = sel.isValid() ? sel.column() + 1
                            : model->columnCount();
    model->insertColumn(col);
  });

  auto *add_check_col = toolbar->addAction(tr("+ Checkbox Col"));
  connect(add_check_col, &QAction::triggered, view, [model] () {
    int col = model->columnCount();
    model->insertColumn(col);
    // Fill the new column with false (checkbox) values.
    for (int r = 0; r < model->rowCount(); ++r) {
      model->set_cell_value(r, col, QVariant(false));
    }
  });

  toolbar->addSeparator();

  auto *del_row = toolbar->addAction(tr("Del Row"));
  connect(del_row, &QAction::triggered, view, [model, view] () {
    auto sel = view->selectionModel()->currentIndex();
    if (sel.isValid()) {
      model->removeRow(sel.row());
    }
  });

  auto *del_col = toolbar->addAction(tr("Del Col"));
  connect(del_col, &QAction::triggered, view, [model, view] () {
    auto sel = view->selectionModel()->currentIndex();
    if (sel.isValid()) {
      model->removeColumn(sel.column());
    }
  });

  toolbar->addSeparator();

  auto *move_row_up = toolbar->addAction(QStringLiteral("\u2191 Row"));
  connect(move_row_up, &QAction::triggered, view, [model, view] () {
    auto sel = view->selectionModel()->currentIndex();
    if (sel.isValid() && sel.row() > 0) {
      model->move_row(sel.row(), sel.row() - 1);
      view->selectRow(sel.row() - 1);
    }
  });

  auto *move_row_down = toolbar->addAction(QStringLiteral("\u2193 Row"));
  connect(move_row_down, &QAction::triggered, view, [model, view] () {
    auto sel = view->selectionModel()->currentIndex();
    if (sel.isValid() && sel.row() < model->rowCount() - 1) {
      model->move_row(sel.row(), sel.row() + 1);
      view->selectRow(sel.row() + 1);
    }
  });

  auto *move_col_left = toolbar->addAction(QStringLiteral("\u2190 Col"));
  connect(move_col_left, &QAction::triggered, view, [model, view] () {
    auto sel = view->selectionModel()->currentIndex();
    if (sel.isValid() && sel.column() > 0) {
      model->move_column(sel.column(), sel.column() - 1);
      view->selectColumn(sel.column() - 1);
    }
  });

  auto *move_col_right = toolbar->addAction(QStringLiteral("\u2192 Col"));
  connect(move_col_right, &QAction::triggered, view, [model, view] () {
    auto sel = view->selectionModel()->currentIndex();
    if (sel.isValid() && sel.column() < model->columnCount() - 1) {
      model->move_column(sel.column(), sel.column() + 1);
      view->selectColumn(sel.column() + 1);
    }
  });

  layout->addWidget(toolbar);
  container->setLayout(layout);

  ++(d->sheet_counter);
  container->setWindowTitle(tr("Sheet %1").arg(d->sheet_counter));

  // Track the tab.
  SheetTab tab;
  tab.model = model;
  tab.view = view;
  tab.container = container;
  tab.sheet_id = -1;
  d->tabs.emplace(container, tab);

  d->tab_widget->InsertTab(0, container);
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

void SpreadsheetExplorer::OnIndexChanged(const ConfigManager &cm) {
  auto sheets = cm.LoadAllSheets();
  if (sheets.isEmpty()) {
    return;
  }

  if (!d->dock) {
    CreateDockWidget(d->manager);
  }

  for (const auto &sheet : sheets) {
    auto *model = new SpreadsheetModel;

    // Build columns.
    QVector<ColumnDefinition> cols;
    for (int c = 0; c < sheet.columns.size(); ++c) {
      cols.push_back({sheet.columns[c].first, c});
    }

    // Build rows from JSON cells.
    int num_rows = static_cast<int>(sheet.cells.size());
    int num_cols = static_cast<int>(cols.size());
    QVector<QVector<QVariant>> rows(num_rows);
    for (int r = 0; r < num_rows; ++r) {
      rows[r].resize(num_cols);
      for (int c = 0; c < num_cols && c < sheet.cells[r].size(); ++c) {
        if (!sheet.cells[r][c].isEmpty()) {
          rows[r][c] = SpreadsheetModel::value_from_json(sheet.cells[r][c]);
        }
      }
    }

    model->populate_from_results(cols, rows);

    // Restore column colors.
    for (int c = 0; c < sheet.columns.size(); ++c) {
      if (sheet.columns[c].second.isValid()) {
        model->SetColumnColor(c, sheet.columns[c].second);
      }
    }

    // Restore row colors.
    for (auto it = sheet.row_colors.constBegin();
         it != sheet.row_colors.constEnd(); ++it) {
      model->SetRowColor(it.key(), it.value());
    }

    // Create the view container (same as OnNewBlankSheet).
    auto *container = new QWidget(d->tab_widget);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *view = new SpreadsheetView(container);
    view->setModel(model);

    auto &theme_manager = d->config_manager.ThemeManager();
    auto *delegate = new SpreadsheetDelegate(
        theme_manager.Theme(), d->config_manager.TabWidth(), view);
    view->setItemDelegate(delegate);

    auto apply_theme = [view, delegate] (const ThemeManager &tm) {
      auto theme = tm.Theme();
      view->ApplyThemeColors(
          theme->GutterBackgroundColor(),
          theme->GutterForegroundColor(),
          theme->GutterBackgroundColor().darker(120));
      view->setFont(theme->Font());
      delegate->SetTheme(theme);
    };
    apply_theme(theme_manager);
    connect(&theme_manager, &ThemeManager::ThemeChanged, view, apply_theme);

    connect(&d->config_manager, &ConfigManager::TabWidthChanged,
            view, [delegate] (unsigned tw) { delegate->SetTabWidth(tw); });

    layout->addWidget(view, 1);

    // Bottom toolbar (same as OnNewBlankSheet — TODO: extract to helper).
    auto *toolbar = new QToolBar(container);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->addAction(tr("+ Row"), view, [model, view] () {
      auto sel = view->selectionModel()->currentIndex();
      model->insertRow(sel.isValid() ? sel.row() + 1 : model->rowCount());
    });
    toolbar->addAction(tr("+ Col"), view, [model, view] () {
      auto sel = view->selectionModel()->currentIndex();
      model->insertColumn(sel.isValid() ? sel.column() + 1 : model->columnCount());
    });
    layout->addWidget(toolbar);

    container->setLayout(layout);
    container->setWindowTitle(sheet.name);

    SheetTab tab;
    tab.model = model;
    tab.view = view;
    tab.container = container;
    tab.sheet_id = sheet.sheet_id;
    d->tabs.emplace(container, tab);

    d->tab_widget->AddTab(container);
  }

  // Don't force show — the dock visibility is managed by
  // QMainWindow::restoreState from the saved window layout.
}

}  // namespace mx::gui
