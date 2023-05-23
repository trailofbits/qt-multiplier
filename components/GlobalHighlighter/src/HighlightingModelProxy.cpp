/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "HighlightingModelProxy.h"

#include <multiplier/ui/Util.h>

namespace mx::gui {

struct HighlightingModelProxy::PrivateData final {
  int entity_id_data_role{};
  EntityColorMap entity_color_map;
};

HighlightingModelProxy::HighlightingModelProxy(QAbstractItemModel *source_model,
                                               int entity_id_data_role)
    : QIdentityProxyModel(source_model->parent()),
      d(new PrivateData) {

  setSourceModel(source_model);
  source_model->setParent(this);

  d->entity_id_data_role = entity_id_data_role;

  connect(source_model, &QAbstractItemModel::modelAboutToBeReset, this,
          &HighlightingModelProxy::OnModelAboutToBeReset);
}

HighlightingModelProxy::~HighlightingModelProxy() {}

QVariant HighlightingModelProxy::data(const QModelIndex &index,
                                      int role) const {
  QVariant ret = QIdentityProxyModel::data(index, role);
  if (role != Qt::BackgroundRole && role != Qt::ForegroundRole) {
    return ret;
  }

  auto entity_id_var = sourceModel()->data(index, d->entity_id_data_role);
  if (!entity_id_var.isValid()) {
    return ret;
  }

  auto entity_id = qvariant_cast<RawEntityId>(entity_id_var);
  auto entity_color_map_it = d->entity_color_map.find(entity_id);
  if (entity_color_map_it == d->entity_color_map.end()) {
    return ret;
  }

  const QColor &background_color = entity_color_map_it->second;
  if (role == Qt::BackgroundRole) {
    return background_color;
  }

  return GetBestForegroundColor(background_color);
}

void HighlightingModelProxy::OnEntityColorMapChange(
    const EntityColorMap &entity_color_map) {

  d->entity_color_map = entity_color_map;

  emit dataChanged(QModelIndex(), QModelIndex(), {Qt::BackgroundRole});
}

void HighlightingModelProxy::OnModelAboutToBeReset() {
  emit modelAboutToBeReset();
}

}  // namespace mx::gui
