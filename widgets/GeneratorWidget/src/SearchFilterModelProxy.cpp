/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "SearchFilterModelProxy.h"

#include <algorithm>
#include <vector>

namespace mx::gui {

struct SearchFilterModelProxy::PrivateData final {
  std::vector<bool> column_filter_state_list;
  QMetaObject::Connection data_changed_connection;
};

SearchFilterModelProxy::SearchFilterModelProxy(QObject *parent)
    : QSortFilterProxyModel(parent),
      d(new PrivateData) {}

SearchFilterModelProxy::~SearchFilterModelProxy() {}

void SearchFilterModelProxy::OnColumnFilterStateListChange(
    const std::vector<bool> &column_filter_state_list) {

  // Each parent item can have an arbitrary amount of columns when
  // dealing with a QAbstractItemModel that models a tree, so just
  // take and save whatever we were given
  d->column_filter_state_list = column_filter_state_list;
  invalidateFilter();
}

void SearchFilterModelProxy::setSourceModel(QAbstractItemModel *source_model) {
  if (d->data_changed_connection != nullptr) {
    disconnect(d->data_changed_connection);
  }

  // Initialize the filter list if it was missing.
  auto num_cols = source_model->columnCount({});
  if (d->column_filter_state_list.size() != static_cast<size_t>(num_cols)) {
    d->column_filter_state_list.clear();
  }

  QSortFilterProxyModel::setSourceModel(source_model);

  d->data_changed_connection =
      connect(source_model, &QAbstractItemModel::dataChanged,
              this, &SearchFilterModelProxy::OnDataChange);
}

bool SearchFilterModelProxy::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {

  const auto &filter_expression = filterRegularExpression();
  if (!filter_expression.isValid() || filter_expression.pattern().isEmpty()) {
    return true;
  }

  auto source_model = sourceModel();

  // Initialize the filter list if it was missing.
  if (d->column_filter_state_list.empty()) {
    auto num_cols = source_model->columnCount({});
    d->column_filter_state_list.clear();
    for (auto i = 0; i < num_cols; ++i) {
      d->column_filter_state_list.push_back(true);
    }
  }

  std::size_t enabled_filter_count{0};
  for (const auto &filter : d->column_filter_state_list) {
    if (filter) {
      ++enabled_filter_count;
    }
  }

  // Accept all rows if there are no filters enabled.
  if (!enabled_filter_count) {
    return true;
  }

  auto filter_role = filterRole();

  for (auto col = 0u; col < d->column_filter_state_list.size(); ++col) {
    if (!d->column_filter_state_list[col]) {
      continue;
    }

    auto index =
        source_model->index(source_row, static_cast<int>(col), source_parent);
    auto filter_role_value_var = index.data(filter_role);
    if (!filter_role_value_var.isValid()) {
      continue;
    }

    const auto &filter_role_value = filter_role_value_var.toString();
    if (filter_role_value.contains(filter_expression)) {
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
