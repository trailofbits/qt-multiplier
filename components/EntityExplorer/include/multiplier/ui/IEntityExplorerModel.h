/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>
#include <multiplier/Types.h>

#include <QAbstractItemModel>

namespace mx::gui {

//! A model for the entity explorer widget
class IEntityExplorerModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a Token object
    TokenRole = Qt::UserRole + 1,
  };

  //! Factory method
  static IEntityExplorerModel *
  Create(const Index &index, const FileLocationCache &file_location_cache,
         QObject *parent = nullptr);

  //! Constructor
  IEntityExplorerModel(QObject *parent) : QAbstractItemModel(parent) {}

  //! Destructor
  virtual ~IEntityExplorerModel() override = default;

  //! Disabled copy constructor
  IEntityExplorerModel(const IEntityExplorerModel &) = delete;

  //! Disabled copy assignment operator
  IEntityExplorerModel &operator=(const IEntityExplorerModel &) = delete;

 public slots:
  //! Starts a new search request
  virtual void Search(const QString &name, const bool &exact_name) = 0;

  //! Cancels the active search, if any
  virtual void CancelSearch() = 0;

 signals:
  //! Emitted when a new search request is started
  void SearchStarted();

  //! Emitted when the active search terminates or is cancelled
  void SearchFinished();
};

}  // namespace mx::gui
