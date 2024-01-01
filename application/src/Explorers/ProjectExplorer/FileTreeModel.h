/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAbstractItemModel>

namespace mx {
class Index;
}  // namespace mx
namespace mx::gui {

//! Implements the IFileTreeModel interface
class FileTreeModel Q_DECL_FINAL : public QAbstractItemModel {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:

  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a `RawEntityId`.
    FileIdRole = Qt::UserRole + 1,

    //! Returns a QString containing the absolute path
    AbsolutePathRole,
  };

  virtual ~FileTreeModel(void);

  //! Constructor
  FileTreeModel(QObject *parent);

  //! \copybrief IFileTreeModel::Update
  void SetIndex(const Index &index);

  //! \copybrief IFileTreeModel::HasAlternativeRoot
  bool HasAlternativeRoot(void) const;

  //! \copybrief IFileTreeModel::SetRoot
  void SetRoot(const QModelIndex &index);

  //! \copybrief ITreeExplorerModel::SetDefaultRoot
  void SetDefaultRoot(void);

  //! Creates a new Qt model index
  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the parent of the given model index
  virtual QModelIndex parent(const QModelIndex &child) const Q_DECL_FINAL;

  //! Returns the amount or rows in the model
  //! Since this is a tree model, rows are intended as child items
  virtual int rowCount(const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the amount of columns in the model
  //! \return Zero if the parent index is not valid. One otherwise (file name)
  virtual int columnCount(const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the index data for the specified role
  virtual QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL;
};

}  // namespace mx::gui
