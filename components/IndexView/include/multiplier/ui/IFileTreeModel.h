/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>

#include <QAbstractItemModel>

namespace mx::gui {

//! A file tree model based on mx::Index that collapses empty folders
class IFileTreeModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a `RawEntityId`.
    FileIdRole = Qt::UserRole + 1,

    //! Returns a QString containing the absolute path
    AbsolutePathRole,

    //! Returns the internal node identifier
    InternalIdentifierRole,
  };

  //! Factory method
  static IFileTreeModel *Create(mx::Index index, QObject *parent = nullptr);

  //! Resets the model by querying the stored mx::Index from scratch
  virtual void Update() = 0;

  //! Returns true if an alternative root is being used
  virtual bool HasAlternativeRoot() const = 0;

  //! Sets the given item as the new root
  virtual void SetRoot(const QModelIndex &index) = 0;

  //! Restores the default root item
  virtual void SetDefaultRoot() = 0;

  //! Constructor
  IFileTreeModel(QObject *parent) : QAbstractItemModel(parent) {}

  //! Destructor
  virtual ~IFileTreeModel() override = default;

  //! Disabled copy constructor
  IFileTreeModel(const IFileTreeModel &) = delete;

  //! Disabled copy assignment operator
  IFileTreeModel &operator=(const IFileTreeModel &) = delete;
};

}  // namespace mx::gui
