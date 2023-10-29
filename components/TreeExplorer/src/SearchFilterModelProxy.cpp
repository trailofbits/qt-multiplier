/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "SearchFilterModelProxy.h"

#include <algorithm>
#include <multiplier/ui/ITreeExplorerModel.h>
#include <vector>

namespace mx::gui {

struct SearchFilterModelProxy::PrivateData final {
  std::vector<bool> filter;
  QMetaObject::Connection model_reset_connection;
  QMetaObject::Connection data_changed_connection;
  int num_columns{0};
};

SearchFilterModelProxy::SearchFilterModelProxy(QObject *parent)
    : QSortFilterProxyModel(parent),
      d(new PrivateData) {}

SearchFilterModelProxy::~SearchFilterModelProxy() {}

//! Enables or disables filtering on the columns.
void SearchFilterModelProxy::OnStateChange(std::vector<bool> new_states) {
  if (d->filter == new_states) {
    return;
  }

  d->filter = std::move(new_states);
  d->filter.resize(static_cast<unsigned>(d->num_columns));
  invalidateFilter();
}

void SearchFilterModelProxy::OnModelReset(void) {
  QModelIndex root_index;
  d->num_columns = std::max(0, sourceModel()->columnCount(root_index));
  d->filter.clear();

  bool accept = true;
  d->filter.insert(d->filter.end(), static_cast<unsigned>(d->num_columns),
                   accept);
}

void SearchFilterModelProxy::setSourceModel(QAbstractItemModel *source_model) {
  if (d->model_reset_connection) {
    disconnect(d->model_reset_connection);
  }

  if (d->data_changed_connection) {
    disconnect(d->data_changed_connection);
  }

  QSortFilterProxyModel::setSourceModel(source_model);

  d->model_reset_connection = connect(
      source_model, &QAbstractItemModel::modelReset,
      this, &SearchFilterModelProxy::OnModelReset);

  d->data_changed_connection = connect(
      source_model, &QAbstractItemModel::dataChanged,
      this, &SearchFilterModelProxy::OnDataChange);

  OnModelReset();
}

bool SearchFilterModelProxy::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {

  auto source_model = sourceModel();
  for (auto col = 0; col < d->num_columns; ++col) {
    if (!d->filter[static_cast<unsigned>(col)]) {
      continue;
    }

    auto index = source_model->index(source_row, col, source_parent);
    auto var = index.data(Qt::DisplayRole);
    if (var.isValid() && var.toString().contains(filterRegularExpression())) {
      return true;
    }
  }

  return false;
}

void SearchFilterModelProxy::OnDataChange(const QModelIndex &top_left,
                                          const QModelIndex &bottom_right,
                                          const QList<int> &roles) {

  auto mapped_top_left{mapFromSource(top_left)};
  auto mapped_bottom_right{mapFromSource(bottom_right)};
  emit dataChanged(mapped_top_left, mapped_bottom_right, roles);
}

}  // namespace mx::gui
