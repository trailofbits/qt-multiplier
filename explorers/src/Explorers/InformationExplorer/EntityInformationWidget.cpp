/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityInformationWidget.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QThreadPool>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Widgets/HistoryWidget.h>

#include "EntityInformationModel.h"
#include "EntityInformationRunnable.h"

namespace mx::gui {
namespace {

static constexpr unsigned kMaxHistorySize = 32;

}  // namespace

struct EntityInformationWidget::PrivateData {
  const AtomicU64Ptr version_number;
  QTreeView * const tree;
  QWidget * const status_widget;
  EntityInformationModel * const model;
  HistoryWidget * const history;
  QThreadPool thread_pool;
  VariantEntity current_entity;
  bool sync{true};

  int num_requests{0};

  inline PrivateData(const ConfigManager &config_manager, bool enable_history,
                     QWidget *parent)
      : version_number(std::make_shared<AtomicU64>()),
        tree(new QTreeView(parent)),
        status_widget(new QWidget(parent)),
        model(new EntityInformationModel(
            config_manager.FileLocationCache(), version_number, tree)),
        history(
            enable_history ?
            new HistoryWidget(config_manager, kMaxHistorySize, false, parent) :
            nullptr) {}
};

EntityInformationWidget::~EntityInformationWidget(void) {}

EntityInformationWidget::EntityInformationWidget(
    const ConfigManager &config_manager, bool enable_history,
    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(config_manager, enable_history, this)) {

  setWindowTitle(tr("Information Explorer"));
  d->tree->setModel(d->model);
  d->tree->setHeaderHidden(true);
  d->tree->setAlternatingRowColors(false);
  d->tree->setSelectionBehavior(QAbstractItemView::SelectRows);
  d->tree->setSelectionMode(QAbstractItemView::SingleSelection);
  d->tree->setAllColumnsShowFocus(true);
  d->tree->setTreePosition(0);
  d->tree->setTextElideMode(Qt::TextElideMode::ElideMiddle);
  d->tree->header()->setStretchLastSection(true);

  // Create the status widget
  d->status_widget->setVisible(false);

  auto status_widget_layout = new QHBoxLayout(this);
  status_widget_layout->setContentsMargins(0, 0, 0, 0);

  status_widget_layout->addWidget(new QLabel(tr("Updating..."), this));
  status_widget_layout->addStretch();

  auto cancel_button = new QPushButton(tr("Cancel"), this);
  status_widget_layout->addWidget(cancel_button);

  connect(cancel_button, &QPushButton::pressed,
          this, &EntityInformationWidget::OnCancelRunningRequest);

  d->status_widget->setLayout(status_widget_layout);

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

  layout->addWidget(d->tree, 1);
  layout->addStretch();
  setLayout(layout);

  config_manager.InstallItemDelegate(d->tree);

  // connect(info_explorer, &InformationExplorer::SelectedItemChanged, this,
  //         &EntityInformationWidget::SelectedItemChanged);

  connect(&config_manager, &ConfigManager::IndexChanged,
          d->model, &EntityInformationModel::OnIndexChanged);
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
      if (!d->status_widget->isVisible()) {
        d->status_widget->setVisible(true);
      }

      ++d->num_requests;
      d->thread_pool.start(runnable);
    }
  }
}

void EntityInformationWidget::OnAllDataFound(void) {
  --d->num_requests;

  if (!d->num_requests) {
    d->status_widget->setVisible(false);
  }

  Q_ASSERT(d->num_requests >= 0);
}

void EntityInformationWidget::OnCancelRunningRequest(void) {
  d->status_widget->setVisible(false);
  d->version_number->fetch_add(1u);
}

void EntityInformationWidget::OnChangeSync(int state) {
  d->sync = Qt::Checked == state;
}

}  // namespace mx::gui
