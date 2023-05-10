/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "SortFilterProxyModel.h"
#include "InformationExplorerModel.h"

#include <multiplier/ui/Assert.h>

namespace mx::gui {

SortFilterProxyModel::SortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

SortFilterProxyModel::~SortFilterProxyModel() {}

void SortFilterProxyModel::setSourceModel(QAbstractItemModel *source_model) {
  Assert(sourceModel() == nullptr,
         "The source model was already set. Changing it is not supported");

  connect(source_model, &QAbstractItemModel::dataChanged, this,
          &SortFilterProxyModel::OnDataChange);

  QSortFilterProxyModel::setSourceModel(source_model);
}

bool SortFilterProxyModel::lessThan(const QModelIndex &source_left,
                                    const QModelIndex &source_right) const {

  auto lhs_var = source_left.data(sortRole());
  auto rhs_var = source_right.data(sortRole());
  if (!lhs_var.isValid() || !rhs_var.isValid()) {
    return QSortFilterProxyModel::lessThan(source_left, source_right);
  }

  if (!lhs_var.canConvert<InformationExplorerModel::RawLocation>() ||
      !rhs_var.canConvert<InformationExplorerModel::RawLocation>()) {

    return QSortFilterProxyModel::lessThan(source_left, source_right);
  }

  const auto &lhs =
      qvariant_cast<InformationExplorerModel::RawLocation>(lhs_var);

  const auto &rhs =
      qvariant_cast<InformationExplorerModel::RawLocation>(rhs_var);

  if (lhs.path != rhs.path) {
    return lhs.path < rhs.path;
  }

  if (lhs.line_number != rhs.line_number) {
    return lhs.line_number < rhs.line_number;
  }

  return lhs.column_number < rhs.column_number;
}

void SortFilterProxyModel::OnDataChange(const QModelIndex &top_left,
                                        const QModelIndex &bottom_right,
                                        const QList<int> &roles) {

  auto mapped_top_left{mapFromSource(top_left)};
  auto mapped_bottom_right{mapFromSource(bottom_right)};

  emit dataChanged(mapped_top_left, mapped_bottom_right, roles);
}

}  // namespace mx::gui
