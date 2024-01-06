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
class SortFilterProxyModel Q_DECL_FINAL : public QSortFilterProxyModel {
  Q_OBJECT

 public:
  //! Constructor
  SortFilterProxyModel(QObject *parent = nullptr);

  //! Destructor
  virtual ~SortFilterProxyModel(void);

  //! Wraps `setSourceModel` in order to connect the required signals
  virtual void setSourceModel(QAbstractItemModel *source_model) Q_DECL_FINAL;

 protected:
  //! A sorting method that understands RawLocationRole types
  bool lessThan(const QModelIndex &source_left,
                const QModelIndex &source_right) const Q_DECL_FINAL;

 // private slots:
 //  void OnBeginInsertRows(const QModelIndex &, int, int);
 //  void OnDataChanged(const QModelIndex &, const QModelIndex &,
 //                     const QList<int> &);

 // signals:
 //  void beginInsertRows(const QModelIndex &, int, int);
 //  void dataChanged(const QModelIndex &, const QModelIndex &,
 //                   const QList<int> &roles);
};

}  // namespace mx::gui
