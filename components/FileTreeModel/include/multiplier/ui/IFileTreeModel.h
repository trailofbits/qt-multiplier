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
  //! Factory method
  static IFileTreeModel *Create(mx::Index index, QObject *parent = nullptr);

  //! Resets the model by querying the stored mx::Index from scratch
  virtual void Update() = 0;

  //! Returns the PackedFileId for the given model index
  //! \todo This should be converted to use ::data() with a custom data role
  virtual std::optional<PackedFileId>
  GetFileIdentifier(const QModelIndex &index) const = 0;

  //! Constructor
  IFileTreeModel(QObject *parent) : QAbstractItemModel(parent) {}

  //! Destructor
  virtual ~IFileTreeModel() override = default;

  IFileTreeModel(const IFileTreeModel &) = delete;
  IFileTreeModel &operator=(const IFileTreeModel &) = delete;
};

}  // namespace mx::gui
