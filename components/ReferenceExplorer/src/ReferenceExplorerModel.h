/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "Types.h"

#include <gap/core/generator.hpp>
#include <multiplier/ui/IReferenceExplorerModel.h>
#include <multiplier/Reference.h>

namespace mx {
class Decl;
class TokenRange;
class Macro;
}  // namespace mx
namespace mx::gui {

//! Implements the IReferenceExplorerModel interface
class ReferenceExplorerModel final : public IReferenceExplorerModel {
  Q_OBJECT

 public:
  //! \copybrief IReferenceExplorerModel::AppendEntityObject
  virtual bool AppendEntityObject(RawEntityId entity_id,
                                  const QModelIndex &parent) override;

  //! \copybrief IReferenceExplorerModel::ExpandEntity
  virtual void ExpandEntity(const QModelIndex &index) override;

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

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  ReferenceExplorerModel(mx::Index index,
                         mx::FileLocationCache file_location_cache,
                         QObject *parent);

  //! Serializes the given node
  static void SerializeNode(QDataStream &stream, const NodeTree::Node &node);

  //! Deserializes a node from the given data stream
  static std::optional<NodeTree::Node> DeserializeNode(QDataStream &stream);

  friend class IReferenceExplorerModel;
};

}  // namespace mx::gui
