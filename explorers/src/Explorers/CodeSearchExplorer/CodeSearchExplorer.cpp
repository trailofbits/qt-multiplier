// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/CodeSearchExplorer.h>

#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QThreadPool>
#include <QVBoxLayout>

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Interfaces/ITheme.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/GUI/Widgets/LineEditWidget.h>
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
// This avoids using InstallItemDelegate which would delete us on theme change.
class ColumnTintDelegate Q_DECL_FINAL : public QAbstractItemDelegate {
  QAbstractItemDelegate *inner{nullptr};
  IThemePtr theme;

  void RebuildInner(IThemePtr new_theme) {
    delete inner;
    theme = std::move(new_theme);
    // Create a new ThemedItemDelegate. We replicate what InstallItemDelegate
    // does, but own the delegate ourselves.
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

    // Draw column tint overlay on odd columns.
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

}  // namespace

struct CodeSearchExplorer::PrivateData {
  Index index;

  const ConfigManager &config_manager;
  IWindowManager * const manager;

  IWindowWidget *dock{nullptr};
  LineEditWidget *search_input{nullptr};
  QLabel *status_label{nullptr};
  QTableView *results_table{nullptr};
  CodeSearchResultsModel *model{nullptr};
  QSortFilterProxyModel *sort_proxy{nullptr};

  AtomicU64Ptr search_version;
  int result_count{0};

  const TriggerHandle open_entity_trigger;
  const TriggerHandle preview_entity_trigger;

  inline PrivateData(ConfigManager &config_manager_, IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_),
        search_version(std::make_shared<AtomicU64>(0u)),
        open_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntity")),
        preview_entity_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenEntityPreview")) {}
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
  CreateDockWidget(parent);
}

void CodeSearchExplorer::CreateDockWidget(IWindowManager *manager) {
  auto &theme_manager = d->config_manager.ThemeManager();

  d->dock = new IWindowWidget;
  d->dock->setWindowTitle(tr("Code Search"));

  auto layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);

  // Search input.
  d->search_input = new LineEditWidget(d->dock);
  d->search_input->setClearButtonEnabled(true);
  d->search_input->setPlaceholderText(tr("Regex pattern (Enter to search)"));

  d->search_input->setFont(theme_manager.Theme()->Font());
  connect(&theme_manager, &ThemeManager::ThemeChanged,
          d->search_input, [this] (const ThemeManager &tm) {
                             d->search_input->setFont(tm.Theme()->Font());
                           });

  connect(d->search_input, &QLineEdit::returnPressed,
          this, &CodeSearchExplorer::OnSearchTriggered);

  layout->addWidget(d->search_input);

  // Status label.
  d->status_label = new QLabel(d->dock);
  d->status_label->setContentsMargins(4, 2, 4, 2);
  layout->addWidget(d->status_label);

  // Results table.
  d->model = new CodeSearchResultsModel(d->dock);
  d->sort_proxy = new QSortFilterProxyModel(d->dock);
  d->sort_proxy->setSourceModel(d->model);

  d->results_table = new QTableView(d->dock);
  d->results_table->setModel(d->sort_proxy);
  d->results_table->setSortingEnabled(true);
  d->results_table->setSelectionBehavior(
      QAbstractItemView::SelectionBehavior::SelectRows);
  d->results_table->setSelectionMode(
      QAbstractItemView::SelectionMode::SingleSelection);
  d->results_table->setEditTriggers(
      QAbstractItemView::EditTrigger::NoEditTriggers);
  d->results_table->setWordWrap(false);
  d->results_table->setTextElideMode(Qt::ElideRight);
  d->results_table->verticalHeader()->hide();
  d->results_table->horizontalHeader()->setDefaultAlignment(
      Qt::AlignLeft | Qt::AlignVCenter);
  d->results_table->horizontalHeader()->setSectionsMovable(true);
  d->results_table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);
  d->results_table->horizontalHeader()->setStretchLastSection(true);
  d->results_table->setVerticalScrollMode(
      QAbstractItemView::ScrollMode::ScrollPerPixel);
  d->results_table->setHorizontalScrollMode(
      QAbstractItemView::ScrollMode::ScrollPerPixel);

  // Right-click on header: toggle "Show Full File Paths".
  d->results_table->horizontalHeader()->setContextMenuPolicy(
      Qt::CustomContextMenu);
  connect(d->results_table->horizontalHeader(),
          &QHeaderView::customContextMenuRequested,
          this, [this] (const QPoint &pos) {
    QMenu menu;
    auto *action = menu.addAction(tr("Show Full File Paths"));
    action->setCheckable(true);
    action->setChecked(d->model->GetShowFullPaths());
    connect(action, &QAction::toggled, this, [this] (bool checked) {
      d->model->SetShowFullPaths(checked);
    });
    menu.exec(d->results_table->horizontalHeader()->mapToGlobal(pos));
  });

  // Create our own themed delegate with column tinting, instead of using
  // InstallItemDelegate (which would fight us on theme changes).
  auto *tint = new ColumnTintDelegate(theme_manager.Theme(), d->results_table);
  d->results_table->setItemDelegate(tint);
  d->results_table->setFont(theme_manager.Theme()->Font());

  connect(&theme_manager, &ThemeManager::ThemeChanged,
          d->results_table, [tint, table = d->results_table] (const ThemeManager &tm) {
            tint->OnThemeChanged(tm);
            table->setFont(tm.Theme()->Font());
            table->viewport()->update();
          });

  layout->addWidget(d->results_table, 1);

  d->dock->setContentsMargins(0, 0, 0, 0);
  d->dock->setLayout(layout);

  // Connect table selection changes to trigger the global code preview.
  connect(d->results_table->selectionModel(),
          &QItemSelectionModel::currentChanged,
          this, &CodeSearchExplorer::OnCurrentChanged);

  // Register dock at the bottom, tabified with other bottom docks.
  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.CodeSearchExplorer";
  config.location = IWindowManager::DockLocation::Bottom;
  config.tabify = true;
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->dock, config);
}

void CodeSearchExplorer::OnSearchTriggered(void) {
  if (!d->model) {
    return;
  }

  // Cancel any in-progress search.
  d->search_version->fetch_add(1u);
  d->model->Clear();
  d->result_count = 0;

  QString pattern = d->search_input->text().trimmed();
  if (pattern.isEmpty()) {
    d->status_label->clear();
    return;
  }

  RegexQuery query(pattern.toStdString());
  if (!query.is_valid()) {
    d->status_label->setText(
        tr("Invalid regex: %1").arg(pattern));
    return;
  }

  d->status_label->setText(tr("Searching..."));

  auto *runnable = new CodeSearchRunnable(
      std::move(query), d->index,
      d->config_manager.FileLocationCache(),
      d->search_version);

  connect(runnable, &CodeSearchRunnable::NewResults,
          this, [this] (uint64_t version,
                        QVector<CodeSearchResultRow> rows) {
            if (version != d->search_version->load()) {
              return;
            }
            d->result_count = d->model->AppendRows(std::move(rows));
            d->status_label->setText(tr("%1 results").arg(d->result_count));
          });

  connect(runnable, &CodeSearchRunnable::Finished,
          this, &CodeSearchExplorer::OnSearchFinished);

  QThreadPool::globalInstance()->start(runnable);
}

void CodeSearchExplorer::OnSearchFinished(void) {
  if (d->result_count == 0 &&
      d->status_label->text() == tr("Searching...")) {
    d->status_label->setText(tr("No results found"));
  }
}

void CodeSearchExplorer::OnCurrentChanged(const QModelIndex &current,
                                          const QModelIndex &) {
  if (!current.isValid()) {
    return;
  }

  // Map through sort proxy to get the source index.
  QModelIndex source_index = d->sort_proxy->mapToSource(current);
  const auto *row = d->model->Row(source_index.row());
  if (!row) {
    return;
  }

  // Determine which token to navigate to based on the clicked column.
  // If a capture group column is clicked, navigate to that capture's token.
  std::optional<Token> nav_token;
  int col = source_index.column();
  int capture_index = col - CodeSearchResultsModel::FirstCaptureColumn;
  if (capture_index >= 0 &&
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

  // Trigger the global code preview (same one used by reference explorer).
  d->preview_entity_trigger.Trigger(
      QVariant::fromValue<VariantEntity>(std::move(entity)));
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
  if (d->dock) {
    d->dock->EmitRequestAttention();
    d->search_input->setFocus();
  }
}

void CodeSearchExplorer::OnIndexChanged(
    const ConfigManager &config_manager) {
  d->index = config_manager.Index();
  if (d->model) {
    d->model->Clear();
    d->result_count = 0;
  }
  if (d->status_label) {
    d->status_label->clear();
  }
}

}  // namespace mx::gui
