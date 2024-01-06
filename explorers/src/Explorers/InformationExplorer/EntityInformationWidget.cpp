/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityInformationWidget.h"

#include <QHeaderView>
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
  EntityInformationModel * const model;
  HistoryWidget * const history;
  QThreadPool thread_pool;

  inline PrivateData(const ConfigManager &config_manager, bool enable_history,
                     QWidget *parent)
      : version_number(std::make_shared<AtomicU64>()),
        tree(new QTreeView(parent)),
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

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  if (d->history) {
    auto toolbar = new QToolBar;
    toolbar->addWidget(d->history);
    toolbar->setIconSize(QSize(16, 16));

    d->history->SetIconSize(toolbar->iconSize());

    layout->addWidget(toolbar);

    // connect(d->history_widget, &HistoryWidget::GoToEntity, this,
    //         &InformationExplorer::OnHistoryNavigationEntitySelected);
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

void EntityInformationWidget::DisplayEntity(const QVariant &data) {
  if (!data.isValid() || !data.canConvert<VariantEntity>()) {
    return;
  }

  auto entity = data.value<VariantEntity>();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  qDebug() << "Info request for:" << EntityId(entity).Pack();
}

}  // namespace mx::gui
