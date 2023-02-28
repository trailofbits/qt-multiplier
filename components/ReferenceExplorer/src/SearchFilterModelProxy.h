/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "FilterSettingsWidget.h"

#include <QSortFilterProxyModel>

namespace mx::gui {
class SearchFilterModelProxy final : public QSortFilterProxyModel {
  Q_OBJECT

 public:
  SearchFilterModelProxy(QObject *parent);
  virtual ~SearchFilterModelProxy() override;

  void SetPathFilterType(
      const FilterSettingsWidget::PathFilterType &path_filter_type);

  void EnableEntityNameFilter(const bool &enable);

  void EnableEntityIDFilter(const bool &enable);

 protected:
  virtual bool
  filterAcceptsRow(int source_row,
                   const QModelIndex &source_parent) const override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;
};
}  // namespace mx::gui
