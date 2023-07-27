/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QList>
#include <QModelIndex>
#include <QObject>
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

  //! Wraps `setSourceModel` in order to connect the required signals
  virtual void setSourceModel(QAbstractItemModel *source_model) override;

 protected:
  //! A sorting method that understands RawLocationRole types
  bool lessThan(const QModelIndex &source_left,
                const QModelIndex &source_right) const override;

 private slots:
  void OnBeginInsertRows(const QModelIndex &, int, int);
  void OnDataChanged(const QModelIndex &, const QModelIndex &,
                     const QList<int> &);

 signals:
  void beginInsertRows(const QModelIndex &, int, int);
  void dataChanged(const QModelIndex &, const QModelIndex &,
                   const QList<int> &roles);
};

}  // namespace mx::gui
