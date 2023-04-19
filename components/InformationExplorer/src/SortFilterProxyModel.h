/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QSortFilterProxyModel>

namespace mx::gui {

//! A subclass of QSortFilterProxyModel that understands RawLocation objects
class SortFilterProxyModel final : public QSortFilterProxyModel {
  Q_OBJECT

 public:
  //! Constructor
  SortFilterProxyModel(QObject *parent);

  //! Destructor
  virtual ~SortFilterProxyModel() override;

 protected:
  //! A sorting method that understands RawLocationRole types
  bool lessThan(const QModelIndex &source_left,
                const QModelIndex &source_right) const override;
};

}  // namespace mx::gui
