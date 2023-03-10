/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IReferenceExplorerModel.h>

namespace mx::gui {

//! Implements the IReferenceExplorerModel interface
class ReferenceExplorerModel final : public IReferenceExplorerModel {
  Q_OBJECT

 public:

  //! \copybrief IReferenceExplorerModel::ExpandEntity
  virtual void ExpandEntity(const QModelIndex &index) override;

  //! \copybrief IReferenceExplorerModel::RemoveEntity
  virtual void RemoveEntity(const QModelIndex &index) override;

  //! \copybrief IReferenceExplorerModel::HasAlternativeRoot
  virtual bool HasAlternativeRoot() const override;

  //! \copybrief IReferenceExplorerModel::SetRoot
  virtual void SetRoot(const QModelIndex &index) override;

  //! \copybrief IReferenceExplorerModel::SetDefaultRoot
  virtual void SetDefaultRoot() override;

  //! Destructor
  virtual ~ReferenceExplorerModel() override;

  //! Creates a new Qt model index
  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const override;

  //! Returns the parent of the given model index
  virtual QModelIndex parent(const QModelIndex &child) const override;

  //! Returns the amount or rows in the model
  //! Since this is a tree model, rows are intended as child items
  virtual int rowCount(const QModelIndex &parent) const override;

  //! Returns the amount of columns in the model
  virtual int columnCount(const QModelIndex &parent) const override;

  //! Returns the index data for the specified role
  virtual QVariant data(const QModelIndex &index, int role) const override;

  //! Returns the specified model items as a mime data object
  virtual QMimeData *mimeData(const QModelIndexList &indexes) const override;

  //! Returns the specified model items as a mime data object
  virtual bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                            int row, int column,
                            const QModelIndex &parent) override;

  //! Returns the item flags for the specified index
  virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

  //! Defines the mime types supported by this model
  virtual QStringList mimeTypes() const override;

 public slots:
  //! \copybrief IReferenceExplorerModel::AppendEntityById
  virtual void AppendEntityById(
      RawEntityId entity_id, ExpansionMode expansion_mode,
      const QModelIndex &parent) override;

 private slots:
  void InsertNodes(QVector<Node> node, int row, const QModelIndex &parent);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  ReferenceExplorerModel(const Index &index,
                         const FileLocationCache &file_location_cache,
                         QObject *parent);

  friend class IReferenceExplorerModel;
};

}  // namespace mx::gui
