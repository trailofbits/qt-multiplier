// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "HighlightedItemsModel.h"

#include <multiplier/GUI/Util.h>
#include <multiplier/Index.h>

namespace mx::gui {

struct HighlightedItemsModel::PrivateData {
  std::vector<VariantEntity> entities;
};

HighlightedItemsModel::~HighlightedItemsModel(void) {}

HighlightedItemsModel::HighlightedItemsModel(QObject *parent)
    : IModel(parent),
      d(new PrivateData) {}

QModelIndex HighlightedItemsModel::index(int row, int column,
                                         const QModelIndex &parent) const {

  if (!hasIndex(row, column, parent)) {
    return {};
  }

  if (parent.isValid() || column != 0) {
    return {};
  }

  auto row_count = static_cast<int>(d->entities.size());
  if (0 > row || row >= row_count) {
    return {};
  }

  return createIndex(row, 0);
}

QModelIndex HighlightedItemsModel::parent(const QModelIndex &) const {
  return QModelIndex();
}

int HighlightedItemsModel::rowCount(const QModelIndex &parent) const {
  return !parent.isValid() ? static_cast<int>(d->entities.size()) : 0;
}

int HighlightedItemsModel::columnCount(const QModelIndex &parent) const {
  return !parent.isValid() ? 1 : 0;
}

QVariant HighlightedItemsModel::data(const QModelIndex &index, int role) const {
  if (index.column() != 0 || 0 > index.row()) {
    return {};
  }

  auto row = static_cast<unsigned>(index.row());
  if (row >= d->entities.size()) {
    return {};
  } 

  const VariantEntity &entity = d->entities[row];

  if (role == Qt::DisplayRole) {
    if (auto name = NameOfEntityAsString(entity)) {
      return name.value();
    }

  // Tooltip used for hovering. Also, this is used for the copy details.
  } else if (role == Qt::ToolTipRole) {
    QString tooltip = tr("Entity Id: %1").arg(::mx::EntityId(entity).Pack());
    if (auto name = NameOfEntityAsString(entity)) {
      tooltip += tr("\nEntity Name: %1").arg(name.value());
    }
    return tooltip;

  } else if (role == IModel::EntityRole) {
    return QVariant::fromValue(entity);

  } else if (role == IModel::TokenRangeDisplayRole) {
    return QVariant::fromValue(NameOfEntity(entity));

  } else if (role == IModel::ModelIdRole) {
    return "com.trailofbits.explorer.HighlightExplorer.HighlightedItemsModel";
  }

  return {};
}

void HighlightedItemsModel::AddEntity(const VariantEntity &entity) {
  emit beginResetModel();
  d->entities.emplace_back(entity);
  emit endResetModel();
}

void HighlightedItemsModel::RemoveEntity(const std::vector<RawEntityId> &eids) {
  std::vector<VariantEntity> new_entities;
  emit beginResetModel();
  for (auto &entity : d->entities) {
    auto eid = ::mx::EntityId(entity).Pack();
    if (std::find(eids.begin(), eids.end(), eid) == eids.end()) {
      new_entities.emplace_back(std::move(entity));
    }
  }
  d->entities.swap(new_entities);
  emit endResetModel();
}

}  // namespace mx::gui
