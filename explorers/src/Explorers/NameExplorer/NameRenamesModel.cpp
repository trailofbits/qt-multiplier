// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "NameRenamesModel.h"

#include <algorithm>

namespace mx::gui {

NameRenamesModel::~NameRenamesModel(void) {}

NameRenamesModel::NameRenamesModel(QObject *parent)
    : QAbstractTableModel(parent) {}

int NameRenamesModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

int NameRenamesModel::columnCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : 4;
}

QVariant NameRenamesModel::headerData(int section,
                                      Qt::Orientation orientation,
                                      int role) const {
  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
    return {};
  }
  switch (section) {
    case 0: return tr("Original Name");
    case 1: return tr("New Name");
    case 2: return tr("Kind");
    case 3: return tr("Location");
    default: return {};
  }
}

QVariant NameRenamesModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return {};
  }

  auto row = static_cast<unsigned>(index.row());
  if (row >= entries_.size()) {
    return {};
  }

  const auto &entry = entries_[row];

  if (role == Qt::DisplayRole) {
    switch (index.column()) {
      case 0: return entry.original_name;
      case 1: return entry.new_name;
      case 2: return entry.kind;
      case 3: return entry.location;
      default: return {};
    }
  }

  return {};
}

void NameRenamesModel::addRename(RawEntityId canonical_id,
                                 const QString &original_name,
                                 const QString &new_name,
                                 const QString &kind,
                                 const QString &location,
                                 const QVector<RawEntityId> &all_ids) {
  int existing_row = findRow(canonical_id);
  if (existing_row >= 0) {
    entries_[static_cast<unsigned>(existing_row)].new_name = new_name;
    auto top_left = createIndex(existing_row, 1);
    auto bottom_right = createIndex(existing_row, 1);
    emit dataChanged(top_left, bottom_right);
    emitRenamesChanged();
    return;
  }

  auto row = static_cast<int>(entries_.size());
  emit beginInsertRows({}, row, row);
  RenameEntry entry;
  entry.canonical_id = canonical_id;
  entry.original_name = original_name;
  entry.new_name = new_name;
  entry.kind = kind;
  entry.location = location;
  entry.all_ids = all_ids;
  entries_.push_back(std::move(entry));
  emit endInsertRows();
  emitRenamesChanged();
}

void NameRenamesModel::removeRename(RawEntityId canonical_id) {
  int row = findRow(canonical_id);
  if (row < 0) {
    return;
  }

  emit beginRemoveRows({}, row, row);
  entries_.erase(entries_.begin() + row);
  emit endRemoveRows();
  emitRenamesChanged();
}

void NameRenamesModel::clear(void) {
  if (entries_.empty()) {
    return;
  }
  emit beginResetModel();
  entries_.clear();
  emit endResetModel();
  emitRenamesChanged();
}

QMap<RawEntityId, QString> NameRenamesModel::buildRenameMap(void) const {
  QMap<RawEntityId, QString> result;
  for (const auto &entry : entries_) {
    for (auto id : entry.all_ids) {
      result.insert(id, entry.new_name);
    }
  }
  return result;
}

int NameRenamesModel::findRow(RawEntityId canonical_id) const {
  for (size_t i = 0u; i < entries_.size(); ++i) {
    if (entries_[i].canonical_id == canonical_id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void NameRenamesModel::emitRenamesChanged(void) {
  emit renamesChanged(buildRenameMap());
}

}  // namespace mx::gui
