/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "SearchFilterModelProxy.h"

#include <multiplier/ui/IReferenceExplorerModel.h>

#include "Types.h"

namespace mx::gui {

struct SearchFilterModelProxy::PrivateData final {
  FilterSettingsWidget::PathFilterType path_filter_type{
      FilterSettingsWidget::PathFilterType::None};

  bool enable_entity_name_filter{false};
  bool enable_entity_id_filter{false};
};

SearchFilterModelProxy::SearchFilterModelProxy(QObject *parent)
    : QSortFilterProxyModel(parent),
      d(new PrivateData) {}

SearchFilterModelProxy::~SearchFilterModelProxy() {}

void SearchFilterModelProxy::SetPathFilterType(
    const FilterSettingsWidget::PathFilterType &path_filter_type) {
  d->path_filter_type = path_filter_type;
  invalidateFilter();
}

void SearchFilterModelProxy::EnableEntityNameFilter(const bool &enable) {
  d->enable_entity_name_filter = enable;
  invalidateFilter();
}

void SearchFilterModelProxy::EnableEntityIDFilter(const bool &enable) {
  d->enable_entity_id_filter = enable;
  invalidateFilter();
}

bool SearchFilterModelProxy::filterAcceptsRow(
    int source_row, const QModelIndex &source_parent) const {

  bool accept_row{false};

  auto index = sourceModel()->index(source_row, 0, source_parent);
  if (d->enable_entity_name_filter) {
    auto name_var = index.data(Qt::DisplayRole);
    if (name_var.isValid()) {
      const auto &name = name_var.toString();

      if (name.contains(filterRegularExpression())) {
        accept_row = true;
      }
    }
  }

  if (d->enable_entity_id_filter) {
    auto entity_id_var = index.data(IReferenceExplorerModel::EntityIdRole);
    if (entity_id_var.isValid()) {
      auto entity_id = entity_id_var.toULongLong();

      auto string_entity_id = QString::number(entity_id);
      if (string_entity_id.contains(filterRegularExpression())) {
        accept_row = true;
      }
    }
  }

  if (d->path_filter_type != FilterSettingsWidget::PathFilterType::None) {
    auto location_var = index.data(IReferenceExplorerModel::LocationRole);
    if (location_var.isValid()) {
      const Location &location = qvariant_cast<Location>(location_var);

      QString path;
      if (d->path_filter_type ==
          FilterSettingsWidget::PathFilterType::FileName) {

        std::filesystem::path full_path(location.path.toStdString());
        path = QString::fromStdString(full_path.filename());

      } else {
        path = location.path;
      }

      if (path.contains(filterRegularExpression())) {
        accept_row = true;
      }
    }
  }

  return accept_row;
}

bool SearchFilterModelProxy::lessThan(const QModelIndex &left,
                                      const QModelIndex &right) const {
  auto left_location_role_var =
      sourceModel()->data(left, IReferenceExplorerModel::LocationRole);

  auto right_location_role_var =
      sourceModel()->data(right, IReferenceExplorerModel::LocationRole);

  std::optional<bool> opt_line_cmp_result;
  std::optional<bool> opt_column_cmp_result;

  if (left_location_role_var.isValid() && right_location_role_var.isValid()) {
    auto left_location_role = qvariant_cast<Location>(left_location_role_var);
    auto right_location_role = qvariant_cast<Location>(right_location_role_var);

    if (left_location_role.path != right_location_role.path) {
      return left_location_role.path < right_location_role.path;
    }

    if (left_location_role.line != right_location_role.line) {
      opt_line_cmp_result = left_location_role.line < right_location_role.line;
    }

    if (left_location_role.column != right_location_role.column) {
      opt_column_cmp_result =
          left_location_role.column < right_location_role.column;
    }
  }

  auto left_display_role_var = sourceModel()->data(left);
  auto right_display_role_var = sourceModel()->data(right);

  if (left_display_role_var.isValid() && right_display_role_var.isValid()) {
    auto left_display_role = left_display_role_var.toString();
    auto right_display_role = right_display_role_var.toString();

    if (left_display_role != right_display_role) {
      return left_display_role < right_display_role;
    }
  }

  if (opt_line_cmp_result.has_value()) {
    return opt_line_cmp_result.value();
  }

  if (opt_column_cmp_result.has_value()) {
    return opt_column_cmp_result.value();
  }

  auto left_entity_id_role_var =
      sourceModel()->data(left, IReferenceExplorerModel::EntityIdRole);

  auto right_entity_id_role_var =
      sourceModel()->data(right, IReferenceExplorerModel::EntityIdRole);

  auto left_entity_id_role = left_entity_id_role_var.toULongLong();
  auto right_entity_id_role = right_entity_id_role_var.toULongLong();

  return left_entity_id_role < right_entity_id_role;
}

}  // namespace mx::gui
