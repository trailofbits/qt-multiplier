/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "Types.h"

#include <multiplier/ui/IReferenceExplorerModel.h>
#include <multiplier/ui/IDatabase.h>

#include <multiplier/Entities/DeclCategory.h>
#include <multiplier/Entities/TokenCategory.h>

#include <unordered_map>

namespace mx::gui {

//! Implements the IReferenceExplorerModel interface
class ReferenceExplorerModel final
    : public IReferenceExplorerModel,
      public IDatabase::QueryEntityReferencesReceiver {
  Q_OBJECT

 public:
  //! Additional internal item data roles for this model
  enum PrivateItemDataRole {
    //! Returns the internal node identifier
    InternalIdentifierRole = Qt::UserRole + 100,

    //! Returns the icon label
    IconLabelRole,
  };

  //! Model data
  struct Context final {
    //! Node ID
    using NodeID = std::uint64_t;

    //! A single tree node
    struct Node final {
      //! The ID for this node
      NodeID node_id;

      //! Parent node ID
      NodeID parent_node_id;

      //! Child nodes
      std::vector<NodeID> child_node_id_list;

      //! List of entity IDs we have within the child nodes
      std::unordered_map<RawEntityId, NodeID> entity_id_node_id_map;

      //! Node data
      IDatabase::QueryEntityReferencesResult::Node data;
    };

    //! Node ID generator
    NodeID node_id_generator{};

    //! The node tree used by the model
    std::unordered_map<NodeID, Node> tree;
  };

  //! Generates a new Node ID
  static Context::NodeID GenerateNodeID(Context &context);

  //! Resets the whole context
  static void ResetContext(Context &context);

  //! \copybrief IReferenceExplorerModel::SetEntity
  virtual void SetEntity(const RawEntityId &entity_id,
                         const ReferenceType &reference_type) override;

  //! Expands the given entity
  virtual void ExpandEntity(const QModelIndex &index, unsigned depth);

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
  //! \todo Fix role=TokenCategoryRole support
  virtual QVariant data(const QModelIndex &index, int role) const override;

  //! Returns the column names for the tree view header
  virtual QVariant headerData(int section, Qt::Orientation orientation,
                              int role) const override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  ReferenceExplorerModel(const Index &index,
                         const FileLocationCache &file_location_cache,
                         QObject *parent);

  //! Returns the category for the given decl
  static TokenCategory GetTokenCategory(const Index &index,
                                        RawEntityId entity_id);

  //! Returns the label for the specified decl category
  static const QString &GetTokenCategoryIconLabel(TokenCategory tok_category);

  //! Returns the decl category name used to build the tooltip
  static const QString &GetTokenCategoryName(TokenCategory tok_category);

  //! Imports the given QueryEntityReferencesResult object into the model
  void ImportReferences(IDatabase::QueryEntityReferencesResult result);

  //! Starts a new data request
  void StartRequest(const QModelIndex &insert_point,
                    const RawEntityId &entity_id,
                    const IDatabase::ReferenceType &reference_type,
                    const bool &include_redeclarations,
                    const bool &emit_root_node, const std::size_t &depth);

  //! A callback used to receive batched data
  virtual void OnDataBatch(DataBatch data_batch) override;

 public slots:
  //! \copybrief IReferenceExplorerModel::CancelRunningRequest
  virtual void CancelRunningRequest() override;

 private slots:
  //! Processes the entire data batch queue
  void ProcessDataBatchQueue();

  //! Called whenever the database request future changes status
  void FutureResultStateChanged();

  friend class IReferenceExplorerModel;
};

}  // namespace mx::gui
