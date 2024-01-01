/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "SortFilterProxyModel.h"
#include "InformationExplorerModel.h"

#include <multiplier/GUI/Assert.h>

namespace mx::gui {

SortFilterProxyModel::SortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

SortFilterProxyModel::~SortFilterProxyModel() {}

void SortFilterProxyModel::setSourceModel(QAbstractItemModel *source_model) {
  Assert(sourceModel() == nullptr,
         "The source model was already set. Changing it is not supported");

  QSortFilterProxyModel::setSourceModel(source_model);

  connect(source_model, &QAbstractItemModel::rowsAboutToBeInserted,
          this, &SortFilterProxyModel::OnBeginInsertRows);

  connect(source_model, &QAbstractItemModel::dataChanged,
          this, &SortFilterProxyModel::OnDataChanged);
}

bool SortFilterProxyModel::lessThan(const QModelIndex &source_left,
                                    const QModelIndex &source_right) const {
  return source_left.row() < source_right.row();
}

void SortFilterProxyModel::OnBeginInsertRows(
    const QModelIndex &parent, int begin_row, int end_row) {

  emit beginInsertRows(mapFromSource(parent), begin_row, end_row);
}

void SortFilterProxyModel::OnDataChanged(const QModelIndex &top_left,
                                         const QModelIndex &bottom_right,
                                         const QList<int> &roles) {
  emit dataChanged(mapFromSource(top_left), mapFromSource(bottom_right), roles);
}

}  // namespace mx::gui
