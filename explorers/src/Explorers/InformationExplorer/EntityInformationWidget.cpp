/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityInformationWidget.h"

#include <QCheckBox>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QThreadPool>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Widgets/HistoryWidget.h>
#include <multiplier/GUI/Widgets/SearchWidget.h>

#include "EntityInformationModel.h"
#include "EntityInformationRunnable.h"
#include "SortFilterProxyModel.h"

namespace mx::gui {
namespace {

static constexpr unsigned kMaxHistorySize = 32;

bool ShouldAutoExpand(const QModelIndex &index) {
  if (!index.isValid()) {
    return true;
  }

  auto auto_expand_var = index.data(EntityInformationModel::AutoExpandRole);
  if (!auto_expand_var.isValid()) {
    return true;
  }

  return auto_expand_var.toBool();
}

}  // namespace

struct EntityInformationWidget::PrivateData {
  const AtomicU64Ptr version_number;
  QTreeView * const tree;
  QWidget * const status;
  EntityInformationModel * const model;
  SortFilterProxyModel * const sort_model;
  HistoryWidget * const history;
  SearchWidget * const search;
  QThreadPool thread_pool;
  VariantEntity current_entity;
  bool sync{true};

  int num_requests{0};

  QModelIndex selected_index;
  QElapsedTimer selection_timer;

  inline PrivateData(const ConfigManager &config_manager, bool enable_history,
                     QWidget *parent)
      : version_number(std::make_shared<AtomicU64>()),
        tree(new QTreeView(parent)),
        status(new QWidget(parent)),
        model(new EntityInformationModel(
            config_manager.FileLocationCache(), version_number, tree)),
        sort_model(new SortFilterProxyModel(tree)),
        history(
            enable_history ?
            new HistoryWidget(config_manager, kMaxHistorySize, false, parent) :
            nullptr),
        search(new SearchWidget(config_manager.MediaManager(),
                                SearchWidget::Mode::Filter, parent)) {}
};

EntityInformationWidget::~EntityInformationWidget(void) {}

EntityInformationWidget::EntityInformationWidget(
    const ConfigManager &config_manager, bool enable_history,
    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(config_manager, enable_history, this)) {

  d->selection_timer.start();

  d->sort_model->setRecursiveFilteringEnabled(true);
  d->sort_model->setSourceModel(d->model);

  setWindowTitle(tr("Information Explorer"));
  d->tree->setModel(d->sort_model);
  d->tree->setHeaderHidden(true);
  d->tree->setAlternatingRowColors(false);
  d->tree->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->tree->setSelectionMode(QAbstractItemView::SingleSelection);
  d->tree->setTextElideMode(Qt::TextElideMode::ElideRight);

  d->tree->setSortingEnabled(true);
  d->tree->sortByColumn(0, Qt::AscendingOrder);

  // The auto scroll takes care of keeping the active item within the
  // visible viewport region. This is true for mouse clicks but also
  // keyboard navigation (i.e. arrow keys, page up/down, etc).
  d->tree->setAutoScroll(false);

  // Smooth scrolling.
  d->tree->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  d->tree->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  // We'll potentially have a bunch of columns depending on the configuration,
  // so make sure they span to use all available space.
  QHeaderView *header = d->tree->header();
  header->setStretchLastSection(true);

  // Don't let double click expand things in three; we capture double click so
  // that we can make it open up the use in the code.
  d->tree->setExpandsOnDoubleClick(false);

  // Disallow multiple selection. If we have grouping by file enabled, then when
  // a user clicks on a file name, we instead jump down to the first entry
  // grouped under that file. This is to make using the up/down arrows easier.
  d->tree->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->tree->setSelectionMode(QAbstractItemView::SingleSelection);
  d->tree->setAllColumnsShowFocus(true);
  d->tree->setTreePosition(0);

  d->tree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(d->tree, &QTreeView::customContextMenuRequested,
          this, &EntityInformationWidget::OnOpenItemContextMenu);

  connect(d->tree, &QAbstractItemView::clicked,
          [this] (const QModelIndex &index) {
            OnCurrentItemChanged(index, {});
          });

  auto tree_selection_model = d->tree->selectionModel();
  connect(tree_selection_model, &QItemSelectionModel::currentChanged,
          this, &EntityInformationWidget::OnCurrentItemChanged);

  // Create the status widget
  d->status->setVisible(false);

  auto status_layout = new QHBoxLayout(this);
  status_layout->setContentsMargins(0, 0, 0, 0);

  status_layout->addWidget(new QLabel(tr("Updating..."), this));
  status_layout->addStretch();

  auto cancel_button = new QPushButton(tr("Cancel"), this);
  status_layout->addWidget(cancel_button);

  connect(cancel_button, &QPushButton::pressed,
          this, &EntityInformationWidget::OnCancelRunningRequest);

  d->status->setLayout(status_layout);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  if (d->history) {
    auto toolbar = new QToolBar(this);
    toolbar->addWidget(d->history);
    toolbar->setIconSize(QSize(16, 16));

    d->history->SetIconSize(toolbar->iconSize());

    toolbar->addWidget(new QLabel(" "));

    auto sync = new QCheckBox(tr("Sync"), this);
    sync->setTristate(false);
    sync->setCheckState(Qt::Checked);
    toolbar->addWidget(sync);

#ifndef QT_NO_TOOLTIP
    sync->setToolTip(tr("Keep in sync with clicks in other views"));
#endif

    connect(d->history, &HistoryWidget::GoToEntity, this,
            [this] (VariantEntity original_entity, VariantEntity) {
              emit HistoricalEntitySelected(std::move(original_entity));
            });

    connect(sync, &QCheckBox::stateChanged,
            this, &EntityInformationWidget::OnChangeSync);

    layout->addWidget(toolbar);
  }

  connect(d->search, &SearchWidget::SearchParametersChanged,
          this, &EntityInformationWidget::OnSearchParametersChange);

  layout->addWidget(d->tree, 1);
  layout->addStretch();
  layout->addWidget(d->status);
  layout->addWidget(d->search);

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  config_manager.InstallItemDelegate(d->tree);

  connect(&config_manager, &ConfigManager::IndexChanged,
          d->model, &EntityInformationModel::OnIndexChanged);

  connect(d->sort_model, &QAbstractItemModel::rowsInserted,
          this, &EntityInformationWidget::ExpandAllBelow);
}

void EntityInformationWidget::OnSearchParametersChange(void) {
  
  QRegularExpression::PatternOptions options{
      QRegularExpression::NoPatternOption};

  auto &search_parameters = d->search->Parameters();
  if (!search_parameters.case_sensitive) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

  auto pattern = QString::fromStdString(search_parameters.pattern);

  if (search_parameters.type == SearchWidget::SearchParameters::Type::Text) {
    pattern = QRegularExpression::escape(pattern);
    if (search_parameters.whole_word) {
      pattern = "\\b" + pattern + "\\b";
    }
  }

  QRegularExpression regex(pattern, options);

  // The regex is already validated by the search widget
  Q_ASSERT(regex.isValid());

  d->sort_model->setFilterRegularExpression(regex);
  d->tree->expandRecursively(QModelIndex());
  d->tree->resizeColumnToContents(0);
}

void EntityInformationWidget::ExpandAllBelow(const QModelIndex &parent) {
  std::vector<QModelIndex> next_queue;
  next_queue.emplace_back(parent);

  while (!next_queue.empty()) {
    auto queue = std::move(next_queue);
    next_queue.clear();

    for (const auto &index : queue) {
      if (!ShouldAutoExpand(index)) {
        continue;
      }

      d->tree->expand(index);

      auto row_count = d->sort_model->rowCount(index);
      for (int row{}; row < row_count; ++row) {
        auto child_index = d->sort_model->index(row, 0, index);
        next_queue.push_back(child_index);
      }
    }
  }

  d->tree->resizeColumnToContents(0);
}

void EntityInformationWidget::DisplayEntity(
    VariantEntity entity, const FileLocationCache &file_location_cache,
    const std::vector<IInformationExplorerPluginPtr> &plugins,
    bool is_explicit_request, bool add_to_history) {

  // If we don't have this info browser synchronized with implicit events, then
  // ignore this request to display the entity.
  if (!is_explicit_request && !d->sync) {
    return;
  }

  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  // Canonicalize decls so that we can dedup check.
  if (std::holds_alternative<Decl>(entity)) {
    entity = std::get<Decl>(entity).canonical_declaration();
  }

  // Dedup check; don't want to reload the model unnecessarily.
  if (EntityId(d->current_entity) == EntityId(entity)) {
    return;
  }

  // If we're showing the history widget then keep track of the history.
  if (add_to_history && d->history) {
    d->history->SetCurrentLocation(d->current_entity);
    d->history->CommitCurrentLocationToHistory();
  }

  d->current_entity = entity;
  d->model->Clear();

  for (const auto &plugin : plugins) {
    auto category_generators = plugin->CreateInformationCollectors(entity);
    for (auto category_generator : category_generators) {
      if (!category_generator) {
        continue;
      }

      auto runnable = new EntityInformationRunnable(
          std::move(category_generator), file_location_cache,
          d->version_number);

      connect(runnable, &EntityInformationRunnable::NewGeneratedItems,
              d->model, &EntityInformationModel::AddData);

      connect(runnable, &EntityInformationRunnable::Finished,
              this, &EntityInformationWidget::OnAllDataFound);

      // Show the status widget (allowing us to cancel the request) if there
      // are any outstanding background requests.
      if (!d->status->isVisible()) {
        d->status->setVisible(true);
      }

      ++d->num_requests;
      d->thread_pool.start(runnable);
    }
  }
}

void EntityInformationWidget::OnAllDataFound(void) {
  --d->num_requests;

  if (!d->num_requests) {
    d->status->setVisible(false);
  }

  Q_ASSERT(d->num_requests >= 0);
}

void EntityInformationWidget::OnCancelRunningRequest(void) {
  d->status->setVisible(false);
  d->version_number->fetch_add(1u);
}

void EntityInformationWidget::OnChangeSync(int state) {
  d->sync = Qt::Checked == state;
}

void EntityInformationWidget::OnCurrentItemChanged(
    const QModelIndex &current_index, const QModelIndex &) {
  auto new_index = d->sort_model->mapToSource(current_index);
  if (!new_index.isValid()) {
    return;
  }
  
  // Suppress likely duplicate events.
  if (d->selection_timer.restart() < 100 &&
      d->selected_index == new_index) {
    return;
  }

  d->selected_index = new_index;
  emit SelectedItemChanged(d->selected_index);
}

void EntityInformationWidget::OnOpenItemContextMenu(const QPoint &point) {
  auto index = d->tree->indexAt(point);
  d->selected_index = d->sort_model->mapToSource(index);
  if (!d->selected_index.isValid()) {
    return;
  }

  emit RequestContextMenu(d->selected_index);
}

}  // namespace mx::gui
