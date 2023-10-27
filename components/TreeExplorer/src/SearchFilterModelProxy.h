/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "FilterSettingsWidget.h"

#include <vector>

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

  //! Wraps `setSourceModel` in order to connect the required signals
  virtual void setSourceModel(QAbstractItemModel *source_model) override;

 public slots:

  //! Enables or disables filtering on the columns.
  void OnStateChange(std::vector<bool> new_states);

 protected:
  //! Returns true if the specified row should be included in the view
  virtual bool
  filterAcceptsRow(int source_row,
                   const QModelIndex &source_parent) const override;

  //! Used to sort the items based on the value of a single column
  virtual bool lessThan(const QModelIndex &left,
                        const QModelIndex &right) const override;

  //! Disabled copy constructor
  SearchFilterModelProxy(const SearchFilterModelProxy &) = delete;

  //! Disabled copy assignment operator
  SearchFilterModelProxy &operator=(const SearchFilterModelProxy &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 private slots:
  //! Forwards the dataChanged signal
  void OnDataChange(const QModelIndex &top_left,
                    const QModelIndex &bottom_right, const QList<int> &roles);

  void OnModelReset(void);
};
}  // namespace mx::gui
