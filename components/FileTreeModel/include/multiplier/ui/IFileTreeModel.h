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

class IFileTreeModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  static IFileTreeModel *Create(mx::Index index, QObject *parent = nullptr);
  virtual void Update() = 0;

  // TODO(alessandro): This should be available through
  // QAbstractItemModel::data(index, IFileTreeModel::FileIdentifierRole)
  virtual std::optional<PackedFileId>
  GetFileIdentifier(const QModelIndex &index) const = 0;

  IFileTreeModel(QObject *parent) : QAbstractItemModel(parent) {}
  virtual ~IFileTreeModel() override = default;

  IFileTreeModel(const IFileTreeModel &) = delete;
  IFileTreeModel &operator=(const IFileTreeModel &) = delete;
};

}  // namespace mx::gui
