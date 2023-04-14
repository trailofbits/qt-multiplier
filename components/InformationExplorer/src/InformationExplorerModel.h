/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IInformationExplorerModel.h>
#include <multiplier/ui/IDatabase.h>

#include <unordered_map>
#include <vector>
#include <unordered_set>

namespace mx::gui {

//! Implements the IInformationExplorerModel interface
class InformationExplorerModel final : public IInformationExplorerModel {
  Q_OBJECT

 public:
  //! \copybrief IInformationExplorerModel::RequestEntityInformation
  virtual void RequestEntityInformation(const RawEntityId &entity_id) override;

  //! Destructor
  virtual ~InformationExplorerModel() override;

  //! Creates a new Qt model index
  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const override;

  //! Returns the parent of the given model index
  virtual QModelIndex parent(const QModelIndex &child) const override;

  //! Returns the amount or rows in the model
  //! Since this is a tree model, rows are intended as child items
  virtual int rowCount(const QModelIndex &parent) const override;

  //! Returns the amount of columns in the model
  //! \return Zero if the parent index is not valid. One otherwise (file name)
  virtual int columnCount(const QModelIndex &parent) const override;

  //! Returns the index data for the specified role
  virtual QVariant data(const QModelIndex &index, int role) const override;

 private slots:
  //! Called whenever the database request future changes status
  void FutureResultStateChanged();

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  InformationExplorerModel(Index index, FileLocationCache file_location_cache,
                           QObject *parent);

  //! Cancels any active request
  void CancelRunningRequest();

 public:
  //! Top level nodes
  enum TopLevelNodeID : quintptr {
    kEntityInformationNodeId,
    kRedeclarationsNodeId,
    kMacrosUsedNodeId,
    kCalleesNodeId,
    kCallersNodeId,
    kAssignedTo,
    kAssignment,
    kIncludesNodeId,
    kIncludeBysNodeId,
    kTopLevelEntitiesNodeId,
    kTopLevelNodeIDMax,
  };

  //! Model data
  struct Context final {
    //! A node id, matching the QModelIndex::internalId type
    using NodeID = quintptr;

    //! Data for a single node
    struct Node final {
      //! Id of the top level node
      NodeID parent_node;

      //! Node name (Qt::DisplayRole)
      QString name;
    };

    //! A list of node IDs
    using NodeIDList = std::vector<NodeID>;

    //! The name of the active entity
    QString entity_name;

    //! Node id generator, starting from the last top-level node ID
    NodeID node_id_generator{kTopLevelNodeIDMax};

    //! All the non-top level nodes in the model
    std::unordered_map<NodeID, Node> node_map;

    //! Maps a top level node `TopLevelNodeID` to its children
    std::unordered_map<NodeID, NodeIDList> root_node_map;
  };

  //! Generates a unique node ID
  static quintptr GenerateNodeID(Context &context);

  //! Imports the given entity information data
  static void
  ImportEntityInformation(Context &context,
                          const EntityInformation &entity_information);

  //! Returns the row count for the given model index
  static int RowCount(const Context &context, const QModelIndex &parent);

  //! Returns the node data for the given item data role
  static QVariant Data(const Context &context, const QModelIndex &index,
                       int role);

  friend class IInformationExplorerModel;
};

}  // namespace mx::gui
