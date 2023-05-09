/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GlobalHighlighter.h"
#include "HighlightingModelProxy.h"

#include <multiplier/ui/IDatabase.h>

#include <QFutureWatcher>

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
  EntityHighlightList highlights;

  IDatabase::Ptr database;
  QFuture<IDatabase::EntityIDList> related_entities_future;
  QFutureWatcher<IDatabase::EntityIDList> future_watcher;

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

  connect(this, &GlobalHighlighter::EntityHighlightListChanged, model_proxy,
          &HighlightingModelProxy::OnEntityHighlightListChange);

  model_proxy->OnEntityHighlightListChange(d->highlights);

  return model_proxy;
}

void GlobalHighlighter::SetEntityColor(RawEntityId entity_id,
                                       const QColor &color) {
  CancelRequest();

  d->operation.type = Operation::Type::SetEntityColor;
  d->operation.color = color;

  StartRequest(entity_id);
}

void GlobalHighlighter::RemoveEntity(RawEntityId entity_id) {
  CancelRequest();

  d->operation.type = Operation::Type::RemoveEntity;
  d->operation.color = {};

  StartRequest(entity_id);
}

void GlobalHighlighter::Clear() {
  d->highlights.clear();
  emit EntityHighlightListChanged(d->highlights);
}

GlobalHighlighter::GlobalHighlighter(
    const Index &index, const FileLocationCache &file_location_cache,
    QWidget *parent)
    : IGlobalHighlighter(parent),
      d(new PrivateData) {

  d->index = index;
  d->file_location_cache = file_location_cache;

  d->database = IDatabase::Create(d->index, d->file_location_cache);
  connect(&d->future_watcher,
          &QFutureWatcher<QFuture<IDatabase::EntityIDList>>::finished, this,
          &GlobalHighlighter::EntityListFutureStatusChanged);
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

void GlobalHighlighter::EntityListFutureStatusChanged() {
  if (d->related_entities_future.isCanceled()) {
    return;
  }

  auto related_entity_list = d->related_entities_future.takeResult();
  if (related_entity_list.empty()) {
    return;
  }

  if (d->operation.type == Operation::Type::SetEntityColor) {
    for (RawEntityId related_entity_id : related_entity_list) {
      d->highlights[related_entity_id] = d->operation.color;
    }

  } else {

    for (RawEntityId related_entity_id : related_entity_list) {
      d->highlights.erase(related_entity_id);
    }
  }

  d->operation = {};
  emit EntityHighlightListChanged(d->highlights);
}

}  // namespace mx::gui
