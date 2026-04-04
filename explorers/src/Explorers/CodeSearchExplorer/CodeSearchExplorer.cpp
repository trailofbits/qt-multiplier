// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/CodeSearchExplorer.h>

#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QThreadPool>
#include <QVBoxLayout>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Interfaces/ITheme.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Widgets/LineEditWidget.h>
#include <multiplier/GUI/Widgets/TabWidget.h>
#include <multiplier/Index.h>
#include <multiplier/Re2.h>

#include "CodeSearchResultsModel.h"
#include "CodeSearchRunnable.h"

// Private header for creating ThemedItemDelegate directly.
#include "../../../../managers/ConfigManager/src/ThemedItemDelegate.h"

namespace mx::gui {
namespace {

static const QString kModelId =
    "com.trailofbits.explorer.CodeSearchExplorer";

// Delegate that creates and owns a ThemedItemDelegate internally, and draws
// a subtle column tint over odd columns after the themed delegate has painted.
class ColumnTintDelegate Q_DECL_FINAL : public QAbstractItemDelegate {
  QAbstractItemDelegate *inner{nullptr};
  IThemePtr theme;

  void RebuildInner(IThemePtr new_theme) {
    delete inner;
    theme = std::move(new_theme);
    inner = new ThemedItemDelegate(theme, std::nullopt, 4u, this);
  }

 public:
  explicit ColumnTintDelegate(IThemePtr theme_, QObject *parent = nullptr)
      : QAbstractItemDelegate(parent) {
    RebuildInner(std::move(theme_));
  }

  void OnThemeChanged(const ThemeManager &tm) {
    RebuildInner(tm.Theme());
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const Q_DECL_FINAL {
    if (inner) {
      inner->paint(painter, option, index);
    }

    if (index.column() % 2 == 1) {
      painter->save();
      painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
      QColor bg = theme ? theme->DefaultBackgroundColor() : QColor(0, 0, 0);
      int luma = bg.red() * 299 + bg.green() * 587 + bg.blue() * 114;
      QColor tint = (luma > 128000) ? QColor(0, 0, 0, 18)
                                    : QColor(255, 255, 255, 18);
      painter->fillRect(option.rect, tint);
      painter->restore();
    }
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const Q_DECL_FINAL {
    if (inner) {
      return inner->sizeHint(option, index);
    }
    return QSize();
  }
};

// Per-tab state: model, proxy, table, version, result count.
struct SearchTab {
  CodeSearchResultsModel *model;
  QSortFilterProxyModel *sort_proxy;
  QTableView *table;
  QLabel *status_label;
  QWidget *container;  // The widget added to the TabWidget.
  AtomicU64Ptr search_version;
  int result_count{0};
};

}  // namespace

struct CodeSearchExplorer::PrivateData {
  Index index;

  const ConfigManager &config_manager;
  IWindowManager * const manager;

  // The dock holds a TabWidget with per-search tabs.
  IWindowWidget *dock{nullptr};
  TabWidget *tab_widget{nullptr};

  // The search input lives in the main toolbar.
  LineEditWidget *search_input{nullptr};

  const TriggerHandle open_entity_trigger;
  const TriggerHandle preview_entity_trigger;

  // Track per-tab state so we can route signals correctly.
  // Key: the container widget pointer.
  std::unordered_map<QWidget *, SearchTab> tabs;

  inline PrivateData(ConfigManager &config_manager_, IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_),
        open_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntity")),
        preview_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntityPreview")) {}

  SearchTab *CurrentTab(void) {
    if (!tab_widget) return nullptr;
    auto *w = tab_widget->currentWidget();
    if (!w) return nullptr;
    auto it = tabs.find(w);
    return it != tabs.end() ? &it->second : nullptr;
  }
};

CodeSearchExplorer::~CodeSearchExplorer(void) {}

CodeSearchExplorer::CodeSearchExplorer(ConfigManager &config_manager,
                                       IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  static bool metatypes_registered = false;
  if (!metatypes_registered) {
    qRegisterMetaType<QVector<CodeSearchResultRow>>(
        "QVector<CodeSearchResultRow>");
    metatypes_registered = true;
  }

  auto &action_manager = config_manager.ActionManager();
  action_manager.Register(
      this, "com.trailofbits.action.OpenCodeSearch",
      &CodeSearchExplorer::OnOpenCodeSearch);

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &CodeSearchExplorer::OnIndexChanged);

  OnIndexChanged(d->config_manager);

  // Add search input to the main toolbar.
  auto &theme_manager = config_manager.ThemeManager();
  d->search_input = new LineEditWidget(parent->Window());
  d->search_input->setClearButtonEnabled(true);
  d->search_input->setPlaceholderText(tr("Regex Search (Enter)"));
  d->search_input->setMinimumWidth(300);
  d->search_input->setMaximumWidth(500);

  d->search_input->setFont(theme_manager.Theme()->Font());
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          d->search_input, [this] (const ThemeManager &tm) {
                             d->search_input->setFont(tm.Theme()->Font());
                           });

  connect(d->search_input, &QLineEdit::returnPressed,
          this, &CodeSearchExplorer::OnSearchTriggered);

  parent->AddToolBarWidget(d->search_input);

  // Dock is created lazily on first search.
}

void CodeSearchExplorer::CreateDockWidget(IWindowManager *manager) {
  d->dock = new IWindowWidget;
  d->dock->setWindowTitle(tr("Code Search"));
  d->dock->setContentsMargins(0, 0, 0, 0);

  d->tab_widget = new TabWidget(d->dock);
  d->tab_widget->setDocumentMode(true);
  d->tab_widget->setTabsClosable(true);

  connect(d->tab_widget->tabBar(), &QTabBar::tabCloseRequested,
          this, &CodeSearchExplorer::OnTabClose);

  auto layout = new QVBoxLayout(d->dock);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tab_widget, 1);
  layout->addStretch();
  d->dock->setLayout(layout);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.CodeSearchExplorer";
  config.location = IWindowManager::DockLocation::Bottom;
  config.tabify = true;
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->dock, config);
}

QTableView *CodeSearchExplorer::CreateResultsTable(QWidget *parent) {
  auto &theme_manager = d->config_manager.ThemeManager();

  auto *table = new QTableView(parent);
  table->setSortingEnabled(true);
  table->setSelectionBehavior(
      QAbstractItemView::SelectionBehavior::SelectRows);
  table->setSelectionMode(
      QAbstractItemView::SelectionMode::SingleSelection);
  table->setEditTriggers(
      QAbstractItemView::EditTrigger::NoEditTriggers);
  table->setWordWrap(false);
  table->setTextElideMode(Qt::ElideRight);
  table->verticalHeader()->hide();
  table->horizontalHeader()->setDefaultAlignment(
      Qt::AlignLeft | Qt::AlignVCenter);
  table->horizontalHeader()->setSectionsMovable(true);
  table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);
  table->horizontalHeader()->setStretchLastSection(true);
  table->setVerticalScrollMode(
      QAbstractItemView::ScrollMode::ScrollPerPixel);
  table->setHorizontalScrollMode(
      QAbstractItemView::ScrollMode::ScrollPerPixel);

  // Right-click on header: toggle "Show Full File Paths".
  table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(table->horizontalHeader(),
          &QHeaderView::customContextMenuRequested,
          this, [this, table] (const QPoint &pos) {
    // Find the tab for this table.
    for (auto &[w, tab] : d->tabs) {
      if (tab.table == table) {
        QMenu menu;
        auto *action = menu.addAction(tr("Show Full File Paths"));
        action->setCheckable(true);
        action->setChecked(tab.model->GetShowFullPaths());
        connect(action, &QAction::toggled, this,
                [model = tab.model] (bool checked) {
          model->SetShowFullPaths(checked);
        });
        menu.exec(table->horizontalHeader()->mapToGlobal(pos));
        break;
      }
    }
  });

  // Themed delegate with column tinting.
  auto *tint = new ColumnTintDelegate(theme_manager.Theme(), table);
  table->setItemDelegate(tint);
  table->setFont(theme_manager.Theme()->Font());

  connect(&theme_manager, &ThemeManager::ThemeChanged,
          table, [tint, table] (const ThemeManager &tm) {
            tint->OnThemeChanged(tm);
            table->setFont(tm.Theme()->Font());
            table->viewport()->update();
          });

  return table;
}

void CodeSearchExplorer::OnSearchTriggered(void) {
  QString pattern = d->search_input->text().trimmed();
  if (pattern.isEmpty()) {
    return;
  }

  RegexQuery query(pattern.toStdString());
  if (!query.is_valid()) {
    // TODO(pag): Show a proper error dialog or status bar message.
    return;
  }

  if (!d->dock) {
    CreateDockWidget(d->manager);
  }

  // Create a new tab for this search.
  auto *container = new QWidget(d->tab_widget);
  auto *layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);

  auto *status_label = new QLabel(container);
  status_label->setContentsMargins(4, 2, 4, 2);
  status_label->setText(tr("Searching..."));
  layout->addWidget(status_label);

  auto *model = new CodeSearchResultsModel(container);
  auto *sort_proxy = new QSortFilterProxyModel(container);
  sort_proxy->setSourceModel(model);

  auto *table = CreateResultsTable(container);
  table->setModel(sort_proxy);
  layout->addWidget(table, 1);

  container->setLayout(layout);
  container->setWindowTitle(pattern);

  SearchTab tab;
  tab.model = model;
  tab.sort_proxy = sort_proxy;
  tab.table = table;
  tab.status_label = status_label;
  tab.container = container;
  tab.search_version = std::make_shared<AtomicU64>(0u);

  d->tabs.emplace(container, tab);

  // Connect selection changes.
  connect(table->selectionModel(),
          &QItemSelectionModel::currentChanged,
          this, [this, container] (const QModelIndex &current, const QModelIndex &) {
            OnCurrentChanged(current, container);
          });

  // Insert tab at front and show.
  d->tab_widget->InsertTab(0, container);
  d->dock->show();
  d->dock->EmitRequestAttention();

  // Start the search.
  auto *runnable = new CodeSearchRunnable(
      std::move(query), d->index,
      d->config_manager.FileLocationCache(),
      tab.search_version);

  connect(runnable, &CodeSearchRunnable::NewResults,
          this, [this, container, version = tab.search_version]
                (uint64_t v, QVector<CodeSearchResultRow> rows) {
            auto it = d->tabs.find(container);
            if (it == d->tabs.end()) return;
            auto &t = it->second;
            if (v != t.search_version->load()) return;
            t.result_count = t.model->AppendRows(std::move(rows));
            t.status_label->setText(tr("%1 results").arg(t.result_count));
          });

  connect(runnable, &CodeSearchRunnable::Finished,
          this, [this, container] () {
            auto it = d->tabs.find(container);
            if (it == d->tabs.end()) return;
            auto &t = it->second;
            if (t.result_count == 0 &&
                t.status_label->text() == tr("Searching...")) {
              t.status_label->setText(tr("No results found"));
            }
          });

  QThreadPool::globalInstance()->start(runnable);
}

void CodeSearchExplorer::OnSearchFinished(void) {
  // Handled inline in the lambda above.
}

void CodeSearchExplorer::OnCurrentChanged(const QModelIndex &current,
                                          QWidget *container) {
  if (!current.isValid()) return;

  auto it = d->tabs.find(container);
  if (it == d->tabs.end()) return;
  auto &tab = it->second;

  QModelIndex source_index = tab.sort_proxy->mapToSource(current);
  const auto *row = tab.model->Row(source_index.row());
  if (!row) return;

  int col = source_index.column();
  int loc_col = tab.model->LocationColumn();

  // Determine which token to navigate to based on the clicked column.
  std::optional<Token> nav_token;
  int capture_index = col - CodeSearchResultsModel::FirstCaptureColumn;
  if (capture_index >= 0 && col != loc_col &&
      capture_index < row->capture_tokens.size() &&
      row->capture_tokens[capture_index]) {
    nav_token = row->capture_tokens[capture_index];
  } else if (row->match_token) {
    nav_token = row->match_token;
  }

  VariantEntity entity;
  if (nav_token) {
    entity = nav_token.value();
  } else if (row->fragment) {
    entity = row->fragment.value();
  } else if (row->file) {
    entity = row->file.value();
  } else {
    return;
  }

  // Clicking the File column opens in the main code explorer;
  // other columns show in the preview pane.
  if (col == loc_col) {
    d->open_entity_trigger.Trigger(
        QVariant::fromValue<VariantEntity>(std::move(entity)));
  } else {
    d->preview_entity_trigger.Trigger(
        QVariant::fromValue<VariantEntity>(std::move(entity)));
  }
}

void CodeSearchExplorer::OnTabClose(int index) {
  auto *widget = d->tab_widget->widget(index);
  d->tab_widget->RemoveTab(index);
  d->tabs.erase(widget);
  widget->close();
  widget->deleteLater();

  if (!d->tab_widget->count()) {
    d->dock->hide();
  }
}

void CodeSearchExplorer::ActOnPrimaryClick(
    IWindowManager *, const QModelIndex &index) {
  if (!d->dock || !d->dock->isVisible() || !index.isValid()) {
    return;
  }

  if (IModel::ModelId(index) == kModelId) {
    d->open_entity_trigger.Trigger(index.data(IModel::EntityRole));
  }
}

void CodeSearchExplorer::ActOnContextMenu(
    IWindowManager *, QMenu *, const QModelIndex &) {
  // No context menu actions for now.
}

void CodeSearchExplorer::OnOpenCodeSearch(const QVariant &) {
  if (d->search_input) {
    d->search_input->setFocus();
  }
}

void CodeSearchExplorer::OnIndexChanged(
    const ConfigManager &config_manager) {
  d->index = config_manager.Index();
}

}  // namespace mx::gui
