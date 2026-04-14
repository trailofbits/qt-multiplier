// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/SpreadsheetExplorer.h>

#include <QAction>
#include <QCursor>
#include <QDataStream>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoCommand>
#include <QUndoGroup>
#include <QUndoStack>
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
#include <QSortFilterProxyModel>

#include <multiplier/GUI/Widgets/SearchWidget.h>
#include <multiplier/GUI/Widgets/SpreadsheetDelegate.h>
#include <multiplier/GUI/Widgets/SpreadsheetModel.h>
#include <multiplier/GUI/Widgets/SpreadsheetView.h>
#include <multiplier/GUI/Widgets/TabWidget.h>
#include <multiplier/Index.h>

#include <QTabBar>

namespace mx::gui {
namespace {

// Shows a multiline text input dialog. Returns the entered text,
// or std::nullopt if cancelled.
static std::optional<QString> ShowDescriptionDialog(
    const QString &current, QWidget *parent) {
  QDialog dialog(parent);
  dialog.setWindowTitle(QObject::tr("Sheet Description"));
  dialog.resize(400, 200);

  auto *layout = new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel(QObject::tr("Enter a description:"), &dialog));

  auto *edit = new QPlainTextEdit(&dialog);
  edit->setPlainText(current);
  edit->setTabChangesFocus(true);
  layout->addWidget(edit, 1);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  layout->addWidget(buttons);

  QObject::connect(buttons, &QDialogButtonBox::accepted,
                   &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected,
                   &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  return edit->toPlainText();
}

}  // namespace

struct SheetTab {
  SpreadsheetModel *model{nullptr};
  SpreadsheetView *view{nullptr};
  QWidget *container{nullptr};
  QLabel *desc_label{nullptr};
  QString description;
  int sheet_id{-1};
};

struct SpreadsheetExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;

  IWindowWidget *dock{nullptr};
  TabWidget *tab_widget{nullptr};
  QUndoStack *undo_stack{nullptr};
  TriggerHandle open_entity_trigger;
  TriggerHandle refresh_docs_trigger;

  // Document viewer dock — tabbed, one tab per open document.
  IWindowWidget *doc_dock{nullptr};
  TabWidget *doc_tabs{nullptr};

  struct DocTab {
    int doc_id{-1};
    QTextEdit *editor{nullptr};
    QLabel *desc_label{nullptr};
    SpreadsheetModel *cell_model{nullptr};  // If opened from a cell.
    int cell_row{-1};
    int cell_col{-1};
  };
  std::unordered_map<QWidget *, DocTab> doc_tab_map;

  // Track per-tab state.
  std::unordered_map<QWidget *, SheetTab> tabs;

  inline PrivateData(ConfigManager &config_manager_, IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_) {}

  void SaveAllSheets(void) const;
};

// Serialize an open SheetTab into a SheetData for persistence.
static ConfigManager::SheetData SheetTabToData(const SheetTab &tab,
                                                const QString &name) {
  ConfigManager::SheetData data;
  data.sheet_id = tab.sheet_id;
  data.name = name;
  data.description = tab.description;

  auto *model = tab.model;
  int num_cols = model->columnCount();
  int num_rows = model->rowCount();

  for (int c = 0; c < num_cols; ++c) {
    ConfigManager::SheetColumnInfo ci;
    ci.name = model->headerData(c, Qt::Horizontal).toString();
    ci.color = model->ColumnColor(c);
    ci.clickable = tab.view ? tab.view->IsColumnClickable(c) : false;
    data.columns.push_back(ci);
  }

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

  for (int r = 0; r < num_rows; ++r) {
    QColor rc = model->RowColor(r);
    if (rc.isValid()) {
      data.row_colors[r] = rc;
    }
  }

  return data;
}

// Undoable command for closing a sheet tab. On redo (close), the tab is
// removed from the TabWidget but the container widget is kept alive.
// On undo, the tab is re-inserted at the original position.
class CloseSheetCommand : public QUndoCommand {
 public:
  CloseSheetCommand(TabWidget *tab_widget_, IWindowWidget *dock_,
                    ConfigManager &config_manager_,
                    std::unordered_map<QWidget *, SheetTab> &tabs_,
                    QWidget *container_, int tab_index_,
                    const QString &tab_name_, const SheetTab &tab_)
      : QUndoCommand(QObject::tr("Close Sheet \"%1\"").arg(tab_name_)),
        tab_widget(tab_widget_), dock(dock_),
        config_manager(config_manager_), tabs(tabs_),
        container(container_), tab_index(tab_index_),
        tab_name(tab_name_), tab(tab_) {}

  ~CloseSheetCommand(void) override {
    if (closed) {
      container->deleteLater();
    }
  }

  void redo(void) override {
    // Save the sheet with a closed_at timestamp.
    auto data = SheetTabToData(tab, tab_name);
    data.closed_at =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    int new_id = config_manager.SaveSheet(data);
    tab.sheet_id = new_id;

    tab_widget->RemoveTab(tab_widget->indexOf(container));
    tabs.erase(container);
    container->setParent(nullptr);
    closed = true;
    if (!tab_widget->count()) {
      dock->hide();
    }
  }

  void undo(void) override {
    closed = false;
    int idx = std::min(tab_index, tab_widget->count());
    tab_widget->InsertTab(idx, container);
    tab_widget->setTabText(idx, tab_name);
    tabs.emplace(container, tab);
    dock->show();
    dock->EmitRequestAttention();
  }

 private:
  TabWidget *tab_widget;
  IWindowWidget *dock;
  ConfigManager &config_manager;
  std::unordered_map<QWidget *, SheetTab> &tabs;
  QWidget *container;
  int tab_index;
  QString tab_name;
  SheetTab tab;
  bool closed{false};
};

void SpreadsheetExplorer::PrivateData::SaveAllSheets(void) const {
  if (tab_widget) {
    config_manager.SaveHeaderState(
        QStringLiteral("sheets_active_tab"),
        QByteArray::number(tab_widget->currentIndex()));
  }

  // Save sheets in tab order (left to right).
  for (int i = 0; tab_widget && i < tab_widget->count(); ++i) {
    auto *w = tab_widget->widget(i);
    auto it = tabs.find(w);
    if (it == tabs.end()) continue;
    const auto &tab = it->second;
    auto data = SheetTabToData(tab, tab.container->windowTitle());
    // Open sheets: closed_at stays empty (NULL in DB).
    int new_id = config_manager.SaveSheet(data);
    const_cast<SheetTab &>(tab).sheet_id = new_id;
  }
}

SpreadsheetExplorer::~SpreadsheetExplorer(void) {
  // Save any open document before shutdown.
  SaveCurrentDocument();

  // Clear the undo stack before saving. This deletes any
  // CloseSheetCommands, which will deleteLater their containers.
  d->undo_stack->clear();
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

  action_manager.Register(
      this, "com.trailofbits.action.OpenDocument",
      &SpreadsheetExplorer::OnOpenDocument);

  d->open_entity_trigger = action_manager.Find(
      "com.trailofbits.action.OpenEntity");
  d->refresh_docs_trigger = action_manager.Find(
      "com.trailofbits.action.RefreshDocuments");

  // Toolbar button for creating a new sheet.
  NamedAction new_sheet_named_action;
  new_sheet_named_action.name = tr("New Sheet");
  new_sheet_named_action.action = new_sheet_trigger;
  parent->AddToolBarButton(
      media_manager.Icon("com.trailofbits.icon.NewSheet"),
      new_sheet_named_action);

  // Create docks eagerly (hidden) so restoreState can manage them.
  CreateDockWidget(parent);
  CreateDocumentDock(parent);

  d->undo_stack = new QUndoStack(this);
  config_manager.UndoGroup().addStack(d->undo_stack);

  // Periodic autosave (every 30 seconds).
  auto *autosave_timer = new QTimer(this);
  autosave_timer->setInterval(30000);
  connect(autosave_timer, &QTimer::timeout, this, [this] () {
    d->SaveAllSheets();
    SaveCurrentDocument();
  });
  autosave_timer->start();

  // Add "New Sheet" and "Reopen Closed Sheet" to the File menu.
  auto *file_menu = parent->Menu(tr("File"));
  if (file_menu) {
    auto *new_sheet_action = new QAction(tr("New Sheet"), this);
    new_sheet_action->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(new_sheet_action, &QAction::triggered,
            this, [this] () { OnNewBlankSheet({}); });
    file_menu->addAction(new_sheet_action);

    auto *reopen_action = new QAction(tr("Reopen Closed Sheet..."), this);
    connect(reopen_action, &QAction::triggered,
            this, &SpreadsheetExplorer::ShowReopenClosedSheetsMenu);
    file_menu->addAction(reopen_action);
  }
}

void SpreadsheetExplorer::LoadPersistedSheets(void) {
  OnIndexChanged(d->config_manager);

  // Restore previously open document tabs.
  auto open_doc_ids = d->config_manager.LoadOpenDocumentIds();
  for (int doc_id : open_doc_ids) {
    OpenDocumentViewer(nullptr, -1, -1, doc_id);
  }

  // Restore active document tab.
  if (d->doc_tabs && d->doc_tabs->count() > 0) {
    auto saved = d->config_manager.LoadHeaderState(
        QStringLiteral("docs_active_tab"));
    int idx = saved.isEmpty() ? 0 : saved.toInt();
    if (idx >= 0 && idx < d->doc_tabs->count()) {
      d->doc_tabs->setCurrentIndex(idx);
    }
  }

  // Hide documents dock if no documents are open.
  if (d->doc_tabs && !d->doc_tabs->count()) {
    d->doc_dock->hide();
    // Also hide the QDockWidget wrapper directly so it doesn't
    // reappear when a sibling dock in the same tab group is shown.
    auto *window = d->manager->Window();
    if (auto *dw = window->findChild<QDockWidget *>(
            QStringLiteral("com.trailofbits.dock.Documents"))) {
      dw->hide();
    }
  }
}

void SpreadsheetExplorer::CreateDockWidget(IWindowManager *manager) {
  d->dock = new IWindowWidget;
  d->dock->setWindowTitle(tr("Sheets"));
  d->dock->setContentsMargins(0, 0, 0, 0);
  d->dock->setMinimumHeight(100);

  d->tab_widget = new TabWidget(d->dock);
  d->tab_widget->setDocumentMode(true);
  d->tab_widget->setTabsClosable(true);

  connect(d->tab_widget->tabBar(), &QTabBar::tabCloseRequested,
          this, [this] (int i) {
    auto *widget = d->tab_widget->widget(i);
    auto it = d->tabs.find(widget);
    if (it == d->tabs.end()) return;

    auto tab_name = d->tab_widget->tabText(i);
    d->config_manager.UndoGroup().setActiveStack(d->undo_stack);
    d->undo_stack->push(new CloseSheetCommand(
        d->tab_widget, d->dock, d->config_manager, d->tabs,
        widget, i, tab_name, it->second));
  });

  // Double-click on a tab renames it; on empty area creates a new sheet.
  connect(d->tab_widget->tabBar(), &QTabBar::tabBarDoubleClicked,
          this, [this] (int index) {
    if (index == -1) {
      OnNewBlankSheet({});
    } else {
      auto current_name = d->tab_widget->tabText(index);
      SimpleTextInputDialog dialog(tr("Enter the new tab name"),
                                   current_name, d->tab_widget);
      dialog.setWindowTitle(tr("Rename Sheet"));
      if (dialog.exec() == QDialog::Accepted) {
        auto opt_name = dialog.TextInput();
        if (opt_name.has_value() && !opt_name->isEmpty()) {
          d->tab_widget->setTabText(index, opt_name.value());
          if (auto *w = d->tab_widget->widget(index)) {
            w->setWindowTitle(opt_name.value());
          }
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

    menu.addAction(tr("Reopen Closed Sheet..."), this,
                   &SpreadsheetExplorer::ShowReopenClosedSheetsMenu);

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
            if (auto *w = d->tab_widget->widget(index)) {
              w->setWindowTitle(opt_name.value());
            }
          }
        }
      });

      menu.addAction(tr("Set Description..."), this, [this, index] () {
        auto *widget = d->tab_widget->widget(index);
        auto it = d->tabs.find(widget);
        if (it == d->tabs.end()) return;
        auto &tab = it->second;

        auto result = ShowDescriptionDialog(tab.description, d->tab_widget);
        if (result.has_value()) {
          tab.description = result.value();
          if (tab.desc_label) {
            tab.desc_label->setText(tab.description);
            tab.desc_label->setVisible(!tab.description.isEmpty());
          }
        }
      });

      menu.addSeparator();

      menu.addAction(tr("Close"), this, [this, index] () {
        auto *widget = d->tab_widget->widget(index);
        auto it = d->tabs.find(widget);
        if (it == d->tabs.end()) return;
        auto tab_name = d->tab_widget->tabText(index);
        d->config_manager.UndoGroup().setActiveStack(d->undo_stack);
        d->undo_stack->push(new CloseSheetCommand(
            d->tab_widget, d->dock, d->config_manager, d->tabs,
            widget, index, tab_name, it->second));
      });
    }

    menu.exec(d->tab_widget->tabBar()->mapToGlobal(pos));
  });

  auto layout = new QVBoxLayout(d->dock);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tab_widget, 1);
  d->dock->setLayout(layout);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.Sheets";
  config.location = IWindowManager::DockLocation::Bottom;
  config.tabify = true;
  config.start_hidden = true;
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->dock, config);
}

// Shared helper: creates a full sheet tab (container, view, delegate,
// toolbar, description label) from a model and description string.
void SpreadsheetExplorer::OpenSheetFromData(
    const ConfigManager::SheetData &sheet) {

  auto *model = new SpreadsheetModel;

  // Build columns.
  QVector<ColumnDefinition> cols;
  for (int c = 0; c < sheet.columns.size(); ++c) {
    cols.push_back({sheet.columns[c].name, c});
  }

  // Build rows.
  int num_rows = static_cast<int>(sheet.cells.size());
  int num_cols = static_cast<int>(cols.size());
  QVector<QVector<QVariant>> rows(num_rows);
  for (int r = 0; r < num_rows; ++r) {
    rows[r].resize(num_cols);
    for (int c = 0; c < num_cols && c < sheet.cells[r].size(); ++c) {
      if (!sheet.cells[r][c].isEmpty()) {
        rows[r][c] = SpreadsheetModel::value_from_json(
            sheet.cells[r][c], &d->config_manager.Index());
      }
    }
  }

  model->populate_from_results(cols, rows);

  // Restore column colors.
  for (int c = 0; c < sheet.columns.size(); ++c) {
    if (sheet.columns[c].color.isValid()) {
      model->SetColumnColor(c, sheet.columns[c].color);
    }
  }

  // Restore clickable columns (done after view is created below).

  // Restore row colors.
  for (auto it = sheet.row_colors.constBegin();
       it != sheet.row_colors.constEnd(); ++it) {
    model->SetRowColor(it.key(), it.value());
  }

  // --- Build container widget ---

  auto *container = new QWidget(d->tab_widget);
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Description label — hidden when empty.
  auto *desc_label = new QLabel(container);
  desc_label->setWordWrap(true);
  desc_label->setContentsMargins(6, 4, 6, 4);
  if (!sheet.description.isEmpty()) {
    desc_label->setText(sheet.description);
    desc_label->setVisible(true);
  } else {
    desc_label->setVisible(false);
  }
  desc_label->installEventFilter(this);
  layout->addWidget(desc_label);

  auto *filter_proxy = new QSortFilterProxyModel(container);
  filter_proxy->setSourceModel(model);
  filter_proxy->setFilterKeyColumn(-1);  // Filter across all columns.
  filter_proxy->setDynamicSortFilter(false);

  auto *view = new SpreadsheetView(container);
  view->SetConfigManager(&d->config_manager);
  view->setModel(filter_proxy);

  // Restore clickable columns.
  for (int c = 0; c < sheet.columns.size(); ++c) {
    if (sheet.columns[c].clickable) {
      view->SetColumnClickable(c, true);
    }
  }

  // Add the model's undo stack to the global group so the global
  // undo/redo actions work. Set it active when the view gets focus.
  auto *model_undo = model->undoStack();
  d->config_manager.UndoGroup().addStack(model_undo);

  view->setFocusPolicy(Qt::StrongFocus);
  connect(qApp, &QApplication::focusChanged,
          view, [&undo_group = d->config_manager.UndoGroup(),
                 view, model_undo] (QWidget *, QWidget *now) {
    if (now == view || (now && now->parentWidget() == view)) {
      undo_group.setActiveStack(model_undo);
    }
  });

  // Navigate to entity when clicking in a "clickable tokens" column.
  connect(view, &SpreadsheetView::TokenClicked,
          this, [this] (const QModelIndex &idx) {
    QVariant raw = idx.data(SpreadsheetRoles::RawValueRole);
    // Find the first token with a related entity.
    if (raw.canConvert<TokenRange>()) {
      for (Token tok : raw.value<TokenRange>()) {
        auto eid = tok.related_entity_id();
        if (!!eid) {
          auto entity = d->config_manager.Index().entity(eid);
          if (!std::holds_alternative<NotAnEntity>(entity)) {
            d->open_entity_trigger.Trigger(
                QVariant::fromValue(entity));
            return;
          }
        }
      }
    } else if (raw.canConvert<Token>()) {
      auto tok = raw.value<Token>();
      auto eid = tok.related_entity_id();
      if (!!eid) {
        auto entity = d->config_manager.Index().entity(eid);
        if (!std::holds_alternative<NotAnEntity>(entity)) {
          d->open_entity_trigger.Trigger(
              QVariant::fromValue(entity));
        }
      }
    }
  });

  auto &theme_manager = d->config_manager.ThemeManager();
  auto *delegate = new SpreadsheetDelegate(
      theme_manager.Theme(), d->config_manager.TabWidth(), view);
  delegate->SetConfigManager(&d->config_manager);
  view->setItemDelegate(delegate);

  connect(&d->config_manager, &ConfigManager::TabWidthChanged,
          view, [delegate] (unsigned tw) { delegate->SetTabWidth(tw); });

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

  // Bottom toolbar.
  auto *toolbar = new QToolBar(container);
  toolbar->setIconSize(QSize(16, 16));

  auto *add_row = toolbar->addAction(tr("+ Row"));
  connect(add_row, &QAction::triggered, view, [model, view] () {
    auto sel = view->selectionModel()->currentIndex();
    int row = sel.isValid() ? sel.row() + 1 : model->rowCount();
    model->insertRow(row);
  });

  auto *add_col = toolbar->addAction(tr("+ Col"));
  connect(add_col, &QAction::triggered, view, [model, view] () {
    auto sel = view->selectionModel()->currentIndex();
    int col = sel.isValid() ? sel.column() + 1 : model->columnCount();
    model->insertColumn(col);
  });

  auto *add_check_col = toolbar->addAction(tr("+ Checkbox Col"));
  connect(add_check_col, &QAction::triggered, view, [model] () {
    int col = model->columnCount();
    model->insertColumn(col);
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

  // Filter widget (Ctrl+F to activate, Escape to dismiss).
  auto &media_manager = d->config_manager.MediaManager();
  auto *filter_widget = new SearchWidget(
      media_manager, SearchWidget::Mode::Filter, container);
  layout->addWidget(filter_widget);

  connect(filter_widget, &SearchWidget::SearchParametersChanged,
          container, [filter_proxy, filter_widget] () {
    auto &params = filter_widget->Parameters();
    QRegularExpression::PatternOptions opts{
        QRegularExpression::NoPatternOption};
    if (!params.case_sensitive) {
      opts |= QRegularExpression::CaseInsensitiveOption;
    }
    auto pattern = QString::fromStdString(params.pattern);
    if (params.type == SearchWidget::SearchParameters::Type::Text) {
      pattern = QRegularExpression::escape(pattern);
      if (params.whole_word) {
        pattern = QStringLiteral("\\b") + pattern + QStringLiteral("\\b");
      }
    }
    filter_proxy->setFilterRegularExpression(
        QRegularExpression(pattern, opts));
  });

  container->setLayout(layout);
  container->setWindowTitle(sheet.name);

  // Open document viewer when a document cell is clicked.
  connect(view, &SpreadsheetView::DocumentCellClicked,
          this, [this, model] (const QModelIndex &idx) {
    int row = idx.row();
    int col = idx.column();
    if (auto *proxy = qobject_cast<QSortFilterProxyModel *>(
            const_cast<QAbstractItemModel *>(idx.model()))) {
      auto src = proxy->mapToSource(idx);
      row = src.row();
      col = src.column();
    }
    OpenDocumentViewer(model, row, col);
  });

  // Auto-open viewer when a cell becomes a DocumentCell (e.g. via =doc).
  auto *doc_opening = new bool(false);
  connect(model, &QAbstractItemModel::dataChanged,
          this, [this, model, doc_opening] (const QModelIndex &topLeft,
                               const QModelIndex &bottomRight) {
    if (*doc_opening) return;
    if (topLeft != bottomRight) return;
    QVariant raw = topLeft.data(SpreadsheetRoles::RawValueRole);
    if (raw.canConvert<DocumentCell>()) {
      auto dc = raw.value<DocumentCell>();
      if (dc.doc_id < 0) {
        *doc_opening = true;
        OpenDocumentViewer(model, topLeft.row(), topLeft.column());
        *doc_opening = false;
      }
    }
  });

  SheetTab tab;
  tab.model = model;
  tab.view = view;
  tab.container = container;
  tab.desc_label = desc_label;
  tab.description = sheet.description;
  tab.sheet_id = sheet.sheet_id;
  d->tabs.emplace(container, tab);

  d->tab_widget->InsertTab(0, container);
  d->dock->show();
  d->dock->EmitRequestAttention();
}

void SpreadsheetExplorer::OnOpenDocument(const QVariant &data) {
  int doc_id = data.toInt();
  if (doc_id < 0) return;
  OpenDocumentViewer(nullptr, -1, -1, doc_id);
}

void SpreadsheetExplorer::SaveCurrentDocument(void) {
  // Save all open document tabs in tab order.
  QVector<int> open_ids;
  for (int i = 0; d->doc_tabs && i < d->doc_tabs->count(); ++i) {
    auto *w = d->doc_tabs->widget(i);
    auto it = d->doc_tab_map.find(w);
    if (it == d->doc_tab_map.end()) continue;
    auto &tab = it->second;
    if (tab.doc_id < 0 || !tab.editor) continue;
    d->config_manager.SaveDocumentContent(tab.doc_id,
                                          tab.editor->toHtml());
    QString title = w->windowTitle();
    d->config_manager.SaveDocumentTitle(tab.doc_id, title);
    open_ids.push_back(tab.doc_id);

    // Update the cell's cached title if this doc was opened from one.
    if (tab.cell_model && tab.cell_row >= 0 && tab.cell_col >= 0) {
      DocumentCell updated;
      updated.doc_id = tab.doc_id;
      updated.title = title;
      tab.cell_model->set_cell_value(tab.cell_row, tab.cell_col,
                                     QVariant::fromValue(updated));
    }
  }
  d->config_manager.SaveOpenDocumentIds(open_ids);

  // Save active doc tab index.
  if (d->doc_tabs) {
    d->config_manager.SaveHeaderState(
        QStringLiteral("docs_active_tab"),
        QByteArray::number(d->doc_tabs->currentIndex()));
  }
}

void SpreadsheetExplorer::CreateDocumentDock(IWindowManager *manager) {
  d->doc_dock = new IWindowWidget;
  d->doc_dock->setWindowTitle(tr("Documents"));
  d->doc_dock->setContentsMargins(0, 0, 0, 0);

  d->doc_tabs = new TabWidget(d->doc_dock);
  d->doc_tabs->setDocumentMode(true);
  d->doc_tabs->setTabsClosable(true);

  // Double-click tab to rename.
  connect(d->doc_tabs->tabBar(), &QTabBar::tabBarDoubleClicked,
          this, [this] (int index) {
    if (index < 0) return;
    auto current_name = d->doc_tabs->tabText(index);
    SimpleTextInputDialog dialog(tr("Enter the new title"),
                                 current_name, d->doc_tabs);
    dialog.setWindowTitle(tr("Rename Document"));
    if (dialog.exec() == QDialog::Accepted) {
      auto opt = dialog.TextInput();
      if (opt.has_value() && !opt->isEmpty()) {
        d->doc_tabs->setTabText(index, opt.value());
        auto *w = d->doc_tabs->widget(index);
        w->setWindowTitle(opt.value());
        auto jt = d->doc_tab_map.find(w);
        if (jt != d->doc_tab_map.end() && jt->second.doc_id >= 0) {
          d->config_manager.SaveDocumentTitle(
              jt->second.doc_id, opt.value());
          d->refresh_docs_trigger.Trigger({});
        }
      }
    }
  });

  // Close tab: save and remove.
  connect(d->doc_tabs->tabBar(), &QTabBar::tabCloseRequested,
          this, [this] (int i) {
    auto *widget = d->doc_tabs->widget(i);
    auto it = d->doc_tab_map.find(widget);
    if (it != d->doc_tab_map.end()) {
      auto &tab = it->second;
      if (tab.doc_id >= 0 && tab.editor) {
        d->config_manager.SaveDocumentContent(
            tab.doc_id, tab.editor->toHtml());
      }
      d->doc_tab_map.erase(it);
    }
    d->doc_tabs->RemoveTab(i);
    widget->deleteLater();
    if (!d->doc_tabs->count()) {
      d->doc_dock->hide();
    }
  });

  // Right-click on tab bar.
  d->doc_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(d->doc_tabs->tabBar(), &QWidget::customContextMenuRequested,
          this, [this] (const QPoint &pos) {
    int index = d->doc_tabs->tabBar()->tabAt(pos);
    QMenu menu(d->doc_tabs);

    if (index >= 0) {
      auto *widget = d->doc_tabs->widget(index);
      auto it = d->doc_tab_map.find(widget);

      menu.addAction(tr("Rename..."), this, [this, index] () {
        auto current_name = d->doc_tabs->tabText(index);
        SimpleTextInputDialog dialog(tr("Enter the new title"),
                                     current_name, d->doc_tabs);
        dialog.setWindowTitle(tr("Rename Document"));
        if (dialog.exec() == QDialog::Accepted) {
          auto opt = dialog.TextInput();
          if (opt.has_value() && !opt->isEmpty()) {
            d->doc_tabs->setTabText(index, opt.value());
            auto *w = d->doc_tabs->widget(index);
            w->setWindowTitle(opt.value());
            auto jt = d->doc_tab_map.find(w);
            if (jt != d->doc_tab_map.end() && jt->second.doc_id >= 0) {
              d->config_manager.SaveDocumentTitle(
                  jt->second.doc_id, opt.value());
              d->refresh_docs_trigger.Trigger({});
            }
          }
        }
      });

      if (it != d->doc_tab_map.end()) {
        menu.addAction(tr("Copy"), this,
                       [doc_id = it->second.doc_id] () {
          auto *mime = new QMimeData;
          QByteArray data;
          QDataStream stream(&data, QIODevice::WriteOnly);
          stream << static_cast<qint32>(doc_id);
          mime->setData(QStringLiteral(
              "application/x-qtmultiplier-document"), data);
          mime->setText(QStringLiteral("[doc:%1]").arg(doc_id));
          QApplication::clipboard()->setMimeData(mime);
        });

        menu.addAction(tr("Copy HTML"), this,
                       [editor = it->second.editor] () {
          auto *mime = new QMimeData;
          mime->setHtml(editor->toHtml());
          mime->setText(editor->toHtml());
          QApplication::clipboard()->setMimeData(mime);
        });

        menu.addAction(tr("Copy Markdown"), this,
                       [editor = it->second.editor] () {
          QApplication::clipboard()->setText(
              editor->document()->toMarkdown());
        });
      }

      menu.addSeparator();
      menu.addAction(tr("Close"), this, [this, index] () {
        d->doc_tabs->tabBar()->tabCloseRequested(index);
      });
    }

    menu.exec(d->doc_tabs->tabBar()->mapToGlobal(pos));
  });

  auto *layout = new QVBoxLayout(d->doc_dock);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->doc_tabs, 1);
  d->doc_dock->setLayout(layout);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.Documents";
  config.location = IWindowManager::DockLocation::Bottom;
  config.start_hidden = true;
  config.app_menu_location = {tr("View"), tr("Drawers")};
  manager->AddDockWidget(d->doc_dock, config);

  // Tabify with the Code Preview dock when it exists.
  // The Code Preview dock is created lazily, so we try on every show.
  connect(d->doc_dock, &IWindowWidget::Shown, this, [this] () {
    static bool tabified = false;
    if (tabified) return;
    auto *window = d->manager->Window();
    for (auto *dock : window->findChildren<QDockWidget *>()) {
      if (dock->objectName() ==
          QStringLiteral("com.trailofbits.dock.CodePreview")) {
        auto *doc_dw = window->findChild<QDockWidget *>(
            QStringLiteral("com.trailofbits.dock.Documents"));
        if (doc_dw) {
          window->tabifyDockWidget(dock, doc_dw);
          tabified = true;
        }
        break;
      }
    }
  });
}

void SpreadsheetExplorer::OpenDocumentViewer(
    SpreadsheetModel *model, int row, int col, int explicit_doc_id) {

  // Resolve doc_id.
  int doc_id = explicit_doc_id;
  if (model && row >= 0 && col >= 0) {
    QVariant raw = model->data(model->index(row, col),
                               SpreadsheetRoles::RawValueRole);
    DocumentCell dc;
    if (raw.canConvert<DocumentCell>()) {
      dc = raw.value<DocumentCell>();
    }
    if (dc.doc_id < 0) {
      dc.doc_id = d->config_manager.CreateDocument(
          QStringLiteral(""), tr("Document"));
      if (dc.doc_id < 0) return;
      d->config_manager.SaveDocumentTitle(
          dc.doc_id, tr("Document %1").arg(dc.doc_id));
      dc.title = tr("Document %1").arg(dc.doc_id);
      model->set_cell_value(row, col, QVariant::fromValue(dc));
      d->refresh_docs_trigger.Trigger({});
    }
    doc_id = dc.doc_id;
  }
  if (doc_id < 0) return;

  // Check if already open.
  for (auto &[widget, tab] : d->doc_tab_map) {
    if (tab.doc_id == doc_id) {
      d->doc_tabs->setCurrentWidget(widget);
      d->doc_dock->show();
      d->doc_dock->EmitRequestAttention();
      return;
    }
  }

  // Create a new tab.
  auto *container = new QWidget(d->doc_tabs);
  auto *tab_layout = new QVBoxLayout(container);
  tab_layout->setContentsMargins(0, 0, 0, 0);
  tab_layout->setSpacing(0);

  // Description label — hidden when empty, editable on double-click.
  auto *desc_label = new QLabel(container);
  desc_label->setWordWrap(true);
  desc_label->setContentsMargins(6, 4, 6, 4);
  {
    // Load description from DB.
    auto docs = d->config_manager.LoadAllDocuments();
    QString desc;
    for (const auto &doc : docs) {
      if (doc.doc_id == doc_id) { desc = doc.description; break; }
    }
    if (!desc.isEmpty()) {
      desc_label->setText(desc);
      desc_label->setVisible(true);
    } else {
      desc_label->setVisible(false);
    }
  }
  desc_label->installEventFilter(this);
  tab_layout->addWidget(desc_label);

  auto *editor = new QTextEdit(container);
  editor->setAcceptRichText(true);

  auto *fmt = new QToolBar(container);
  fmt->setIconSize(QSize(16, 16));

  auto *bold_act = fmt->addAction(tr("B"));
  bold_act->setCheckable(true);
  auto bf = bold_act->font(); bf.setBold(true); bold_act->setFont(bf);
  connect(bold_act, &QAction::toggled, editor,
          [editor] (bool on) {
    editor->setFontWeight(on ? QFont::Bold : QFont::Normal);
  });

  auto *italic_act = fmt->addAction(tr("I"));
  italic_act->setCheckable(true);
  auto itf = italic_act->font(); itf.setItalic(true); italic_act->setFont(itf);
  connect(italic_act, &QAction::toggled, editor, &QTextEdit::setFontItalic);

  auto *underline_act = fmt->addAction(tr("U"));
  underline_act->setCheckable(true);
  auto uf = underline_act->font(); uf.setUnderline(true); underline_act->setFont(uf);
  connect(underline_act, &QAction::toggled, editor, &QTextEdit::setFontUnderline);

  fmt->addSeparator();

  for (int lvl : {1, 2, 3}) {
    auto *act = fmt->addAction(tr("H%1").arg(lvl));
    connect(act, &QAction::triggered, editor, [editor, lvl] () {
      auto cursor = editor->textCursor();
      auto bfmt = cursor.blockFormat();
      bfmt.setHeadingLevel(bfmt.headingLevel() == lvl ? 0 : lvl);
      cursor.setBlockFormat(bfmt);
      auto cfmt = cursor.charFormat();
      if (bfmt.headingLevel() > 0) {
        int sizes[] = {0, 24, 20, 16, 14, 13, 12};
        cfmt.setFontPointSize(sizes[lvl]);
        cfmt.setFontWeight(QFont::Bold);
      } else {
        cfmt.setFontPointSize(0);
        cfmt.setFontWeight(QFont::Normal);
      }
      cursor.mergeCharFormat(cfmt);
      editor->setTextCursor(cursor);
    });
  }

  fmt->addSeparator();
  auto *list_act = fmt->addAction(tr("List"));
  connect(list_act, &QAction::triggered, editor, [editor] () {
    auto cursor = editor->textCursor();
    if (cursor.currentList()) {
      auto f = cursor.blockFormat();
      f.setObjectIndex(-1);
      cursor.setBlockFormat(f);
    } else {
      cursor.createList(QTextListFormat::ListDisc);
    }
    editor->setTextCursor(cursor);
  });

  connect(editor, &QTextEdit::currentCharFormatChanged,
          container, [bold_act, italic_act, underline_act]
          (const QTextCharFormat &f) {
    bold_act->setChecked(f.fontWeight() >= QFont::Bold);
    italic_act->setChecked(f.fontItalic());
    underline_act->setChecked(f.fontUnderline());
  });

  tab_layout->addWidget(fmt);
  tab_layout->addWidget(editor, 1);
  container->setLayout(tab_layout);

  QString content = d->config_manager.LoadDocumentContent(doc_id);
  editor->setHtml(content);

  QString title = d->config_manager.LoadDocumentTitle(doc_id);
  if (title.isEmpty()) {
    title = tr("Document %1").arg(doc_id);
  }
  container->setWindowTitle(title);

  PrivateData::DocTab dt;
  dt.doc_id = doc_id;
  dt.editor = editor;
  dt.desc_label = desc_label;
  dt.cell_model = model;
  dt.cell_row = row;
  dt.cell_col = col;
  d->doc_tab_map.emplace(container, dt);

  d->doc_tabs->AddTab(container);
  d->doc_dock->show();
  d->doc_dock->EmitRequestAttention();
}

void SpreadsheetExplorer::OnNewBlankSheet(const QVariant &) {
  ConfigManager::SheetData blank;
  blank.columns = {
    {tr("A"), QColor(), false},
    {tr("B"), QColor(), false},
    {tr("C"), QColor(), false},
  };
  blank.cells.resize(8);
  for (auto &row : blank.cells) {
    row.resize(3);
  }

  // Save immediately to get a DB-assigned ID for the name.
  blank.name = tr("Sheet");  // Temporary.
  int id = d->config_manager.SaveSheet(blank);
  blank.sheet_id = id;
  blank.name = tr("Sheet %1").arg(id);
  // Update the name in the DB.
  d->config_manager.SaveSheet(blank);

  OpenSheetFromData(blank);
}

void SpreadsheetExplorer::OnOpenInSpreadsheet(const QVariant &data) {
  if (data.canConvert<ConfigManager::SheetData>()) {
    auto sheet = data.value<ConfigManager::SheetData>();
    if (sheet.columns.isEmpty()) {
      OnNewBlankSheet({});
      return;
    }
    OpenSheetFromData(sheet);
  } else {
    OnNewBlankSheet({});
  }
}

void SpreadsheetExplorer::ActOnPrimaryClick(
    IWindowManager *, const QModelIndex &) {
}

void SpreadsheetExplorer::ActOnContextMenu(
    IWindowManager *, QMenu *, const QModelIndex &) {
}

bool SpreadsheetExplorer::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseButtonDblClick) {
    auto *label = qobject_cast<QLabel *>(obj);
    if (label) {
      // Find the SheetTab that owns this label.
      for (auto &[widget, tab] : d->tabs) {
        if (tab.desc_label == label) {
          auto result = ShowDescriptionDialog(tab.description,
                                              d->tab_widget);
          if (result.has_value()) {
            tab.description = result.value();
            label->setText(tab.description);
            label->setVisible(!tab.description.isEmpty());
          }
          return true;
        }
      }

      // Find the DocTab that owns this label.
      for (auto &[widget, tab] : d->doc_tab_map) {
        if (tab.desc_label == label) {
          // Load current description from DB.
          auto docs = d->config_manager.LoadAllDocuments();
          QString desc;
          for (const auto &doc : docs) {
            if (doc.doc_id == tab.doc_id) { desc = doc.description; break; }
          }
          auto result = ShowDescriptionDialog(desc, d->doc_tabs);
          if (result.has_value()) {
            d->config_manager.SaveDocumentDescription(
                tab.doc_id, result.value());
            label->setText(result.value());
            label->setVisible(!result->isEmpty());
            d->refresh_docs_trigger.Trigger({});
          }
          return true;
        }
      }
    }
  }
  return IMainWindowPlugin::eventFilter(obj, event);
}

void SpreadsheetExplorer::ShowReopenClosedSheetsMenu(void) {
  auto closed = d->config_manager.LoadClosedSheets();
  if (closed.isEmpty()) {
    return;
  }

  QMenu menu(d->tab_widget);
  for (const auto &info : closed) {
    // Skip sheets that are currently open (by sheet_id).
    bool already_open = false;
    for (const auto &[w, t] : d->tabs) {
      if (t.sheet_id == info.sheet_id) {
        already_open = true;
        break;
      }
    }
    if (already_open) continue;

    QString label = info.name;
    if (!info.description.isEmpty()) {
      label += QStringLiteral(" \u2014 ") + info.description;
    }

    // Format the close time for display.
    auto dt = QDateTime::fromString(info.closed_at, Qt::ISODate);
    if (dt.isValid()) {
      label += QStringLiteral("  (closed ") +
               dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm")) +
               QStringLiteral(")");
    }

    auto *action = menu.addAction(label);
    connect(action, &QAction::triggered, this,
            [this, id = info.sheet_id] () {
      auto sheet = d->config_manager.LoadSheetById(id);
      if (sheet.sheet_id >= 0) {
        OpenSheetFromData(sheet);
      }
    });
  }

  if (menu.isEmpty()) {
    menu.addAction(tr("(no closed sheets)"))->setEnabled(false);
  }

  menu.exec(QCursor::pos());
}

void SpreadsheetExplorer::OnIndexChanged(const ConfigManager &cm) {
  auto sheets = cm.LoadOpenSheets();
  if (sheets.isEmpty()) {
    // No open sheets — hide the dock in case restoreState showed it.
    if (d->dock) {
      d->dock->hide();
    }
    return;
  }

  if (!d->dock) {
    CreateDockWidget(d->manager);
  }

  for (const auto &sheet : sheets) {
    OpenSheetFromData(sheet);
  }

  // Restore the active tab index.
  if (d->tab_widget->count() > 0) {
    auto saved_index = d->config_manager.LoadHeaderState(
        QStringLiteral("sheets_active_tab"));
    int idx = saved_index.isEmpty() ? 0 : saved_index.toInt();
    if (idx < 0 || idx >= d->tab_widget->count()) {
      idx = 0;
    }

    d->tab_widget->setCurrentIndex(idx);
    if (auto *page = d->tab_widget->currentWidget()) {
      page->show();
      page->raise();
    }

    // restoreState may have saved a tiny splitter allocation from when
    // the dock was empty. Temporarily bump the minimum height.
    d->dock->setMinimumHeight(200);
    QTimer::singleShot(0, d->dock, [dock = d->dock] () {
      dock->setMinimumHeight(100);
    });
  }
}

}  // namespace mx::gui
