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
  virtual ~SearchFilterModelProxy();

  //! Wraps `setSourceModel` in order to connect the required signals
  void setSourceModel(QAbstractItemModel *source_model) Q_DECL_FINAL;

 public slots:

  //! Enables or disables filtering on the columns.
  void OnStateChange(std::vector<bool> new_states);

 protected:
  //! Returns true if the specified row should be included in the view
  bool filterAcceptsRow(int source_row,
                        const QModelIndex &source_parent) const Q_DECL_FINAL;

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
