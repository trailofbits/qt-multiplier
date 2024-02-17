/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "SortFilterProxyModel.h"
#include "EntityInformationModel.h"

#include <multiplier/GUI/Util.h>

namespace mx::gui {

SortFilterProxyModel::SortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

SortFilterProxyModel::~SortFilterProxyModel(void) {}

void SortFilterProxyModel::setSourceModel(QAbstractItemModel *source_model) {
  Q_ASSERT(!sourceModel());

  QSortFilterProxyModel::setSourceModel(source_model);

  // connect(source_model, &QAbstractItemModel::rowsAboutToBeInserted,
  //         this, &SortFilterProxyModel::OnBeginInsertRows);

  // connect(source_model, &QAbstractItemModel::dataChanged,
  //         this, &SortFilterProxyModel::OnDataChanged);
}

bool SortFilterProxyModel::lessThan(const QModelIndex &source_left,
                                    const QModelIndex &source_right) const {
  // The original `struct Node` can now be acquired from the model
  // indexes for additional sorting methods
  auto sort_role = sortRole();
  if (!source_left.parent().isValid()) {
    // Ignore sort ordering
    switch (sortOrder()) {
    case Qt::AscendingOrder:
      return source_left.row() < source_right.row();

    case Qt::DescendingOrder:
      return source_right.row() < source_left.row();
    }
  }

  switch (sort_role) {
  case Qt::DisplayRole:
  case EntityInformationModel::StringLocationRole:
  case EntityInformationModel::StringFileNameLocationRole:
    return source_left.data(sort_role).toString() <
           source_right.data(sort_role).toString();

  case IModel::TokenRangeDisplayRole: {
    auto left_token_range = qvariant_cast<TokenRange>(source_left.data(sort_role));
    auto right_token_range = qvariant_cast<TokenRange>(source_right.data(sort_role));

    return TokensToString(left_token_range) <
           TokensToString(right_token_range);
  }

  default:
    return source_left.row() < source_right.row();
  }
}

// void SortFilterProxyModel::OnBeginInsertRows(
//     const QModelIndex &parent, int begin_row, int end_row) {

//   emit beginInsertRows(mapFromSource(parent), begin_row, end_row);
// }

// void SortFilterProxyModel::OnDataChanged(const QModelIndex &top_left,
//                                          const QModelIndex &bottom_right,
//                                          const QList<int> &roles) {
//   emit dataChanged(mapFromSource(top_left), mapFromSource(bottom_right), roles);
// }

}  // namespace mx::gui
