/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "SearchFilterModelProxy.h"

#include <multiplier/ui/IReferenceExplorerModel.h>

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
      const auto &location =
          qvariant_cast<IReferenceExplorerModel::Location>(location_var);

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

}  // namespace mx::gui
