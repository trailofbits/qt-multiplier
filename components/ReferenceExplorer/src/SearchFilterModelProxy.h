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

//! A custom model proxy used by the ref explorer to sort and filter items
class SearchFilterModelProxy final : public QSortFilterProxyModel {
  Q_OBJECT

 public:
  //! Constructor
  SearchFilterModelProxy(QObject *parent);

  //! Destructor
  virtual ~SearchFilterModelProxy() override;

  //! Sets the path filtering type
  void SetPathFilterType(
      const FilterSettingsWidget::PathFilterType &path_filter_type);

  //! Enables or disables entity name-based filtering
  void EnableEntityNameFilter(const bool &enable);

  //! Enables or disables entity ID-based filtering
  void EnableEntityIDFilter(const bool &enable);

 protected:
  //! Returns true if the specified row should be included in the view
  virtual bool
  filterAcceptsRow(int source_row,
                   const QModelIndex &source_parent) const override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;
};
}  // namespace mx::gui
