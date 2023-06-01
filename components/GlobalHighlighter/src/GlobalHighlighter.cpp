/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GlobalHighlighter.h"
#include "HighlightingModelProxy.h"
#include "GlobalHighlighterItem.h"

#include <multiplier/ui/IDatabase.h>

#include <QFutureWatcher>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>

namespace mx::gui {

struct Operation final {
  enum class Type {
    SetEntityColor,
    RemoveEntity,
  };

  Type type{Type::SetEntityColor};
  QColor color;
};

struct GlobalHighlighter::PrivateData final {
  Index index;
  FileLocationCache file_location_cache;

  QScrollArea *scroll_area{nullptr};

  EntityHighlightList entity_highlight_list;
  EntityColorMap entity_color_map;

  IDatabase::Ptr database;
  QFuture<IDatabase::RelatedEntitiesResult> related_entities_future;
  QFutureWatcher<IDatabase::RelatedEntitiesResult> future_watcher;
  Operation operation;
};

GlobalHighlighter::~GlobalHighlighter() {
  CancelRequest();
}

QAbstractItemModel *
GlobalHighlighter::CreateModelProxy(QAbstractItemModel *source_model,
                                    int entity_id_data_role) {

  auto model_proxy =
      new HighlightingModelProxy(source_model, entity_id_data_role);

  connect(this, &GlobalHighlighter::EntityColorMapChanged, model_proxy,
          &HighlightingModelProxy::OnEntityColorMapChange);

  model_proxy->OnEntityColorMapChange(d->entity_color_map);

  return model_proxy;
}

void GlobalHighlighter::SetEntityColor(const RawEntityId &entity_id,
                                       const QColor &color) {
  CancelRequest();

  d->operation.type = Operation::Type::SetEntityColor;
  d->operation.color = color;

  StartRequest(entity_id);
}

void GlobalHighlighter::RemoveEntity(const RawEntityId &entity_id) {
  CancelRequest();

  d->operation.type = Operation::Type::RemoveEntity;
  d->operation.color = {};

  StartRequest(entity_id);
}

void GlobalHighlighter::Clear() {
  d->entity_color_map.clear();
  d->entity_highlight_list.clear();

  UpdateItemList();

  emit EntityColorMapChanged(d->entity_color_map);
}

GlobalHighlighter::GlobalHighlighter(
    const Index &index, const FileLocationCache &file_location_cache,
    QWidget *parent)
    : IGlobalHighlighter(parent),
      d(new PrivateData) {

  d->index = index;
  d->file_location_cache = file_location_cache;

  setWindowTitle(tr("Highlight Explorer"));

  d->database = IDatabase::Create(d->index, d->file_location_cache);
  connect(&d->future_watcher,
          &QFutureWatcher<QFuture<IDatabase::RelatedEntitiesResult>>::finished,
          this, &GlobalHighlighter::EntityListFutureStatusChanged);

  d->scroll_area = new QScrollArea(this);
  d->scroll_area->setContentsMargins(0, 0, 0, 0);
  d->scroll_area->setWidgetResizable(true);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  layout->addWidget(d->scroll_area);
  setLayout(layout);

  setEnabled(false);
}

void GlobalHighlighter::StartRequest(const RawEntityId &entity_id) {
  CancelRequest();

  d->related_entities_future = d->database->GetRelatedEntities(entity_id);
  d->future_watcher.setFuture(d->related_entities_future);
}

void GlobalHighlighter::CancelRequest() {
  if (!d->related_entities_future.isRunning()) {
    return;
  }

  d->related_entities_future.cancel();
  d->related_entities_future.waitForFinished();

  d->related_entities_future = {};
}

void GlobalHighlighter::UpdateItemList() {
  // Create the new UI items
  std::sort(d->entity_highlight_list.begin(), d->entity_highlight_list.end(),

            [](const EntityHighlight &lhs, const EntityHighlight &rhs) -> bool {
              return lhs.name < rhs.name;
            });

  if (auto widget = d->scroll_area->widget(); widget != nullptr) {
    qDeleteAll(
        widget->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));

    widget->close();
    widget->deleteLater();
  }

  d->entity_color_map.clear();
  auto layout = new QVBoxLayout();

  for (const auto &entity_highlight : d->entity_highlight_list) {

    const auto &primary_entity_id = *entity_highlight.entity_id_list.begin();

    for (const auto &raw_entity_id : entity_highlight.entity_id_list) {
      d->entity_color_map.insert({raw_entity_id, entity_highlight.color});
    }

    auto item = new GlobalHighlighterItem(
        primary_entity_id, entity_highlight.name, entity_highlight.color, this);

    connect(item, &GlobalHighlighterItem::EntityClicked, this,
            &GlobalHighlighter::EntityClicked);

    layout->addWidget(item);
  }

  layout->addStretch();

  auto widget = new QWidget(this);
  widget->setLayout(layout);

  d->scroll_area->setWidget(widget);
}

void GlobalHighlighter::EntityListFutureStatusChanged() {
  if (d->related_entities_future.isCanceled()) {
    return;
  }

  auto request_result = d->related_entities_future.takeResult();
  if (!request_result.Succeeded()) {
    return;
  }

  // Always delete the current entry if available
  EntityHighlight inc_entity_highlight;

  {
    const auto &request_data = request_result.TakeValue();

    inc_entity_highlight.name = request_data.name;
    inc_entity_highlight.entity_id_list = request_data.entity_id_list;
    inc_entity_highlight.color = d->operation.color;
  }

  auto entity_highlight_list_it = std::find_if(
      d->entity_highlight_list.begin(), d->entity_highlight_list.end(),

      [&inc_entity_highlight](const EntityHighlight &item) -> bool {
        return item.entity_id_list == inc_entity_highlight.entity_id_list;
      });

  if (entity_highlight_list_it != d->entity_highlight_list.end()) {
    d->entity_highlight_list.erase(entity_highlight_list_it);
  }

  // If we are setting or resetting a color, we can just save the new
  // structure now
  if (d->operation.type == Operation::Type::SetEntityColor) {
    d->entity_highlight_list.push_back(std::move(inc_entity_highlight));
  }

  d->operation = {};

  UpdateItemList();

  setEnabled(!d->entity_color_map.empty());
  emit EntityColorMapChanged(d->entity_color_map);
}

}  // namespace mx::gui
