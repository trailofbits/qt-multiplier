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
  //! Model data
  struct Context final {
    //! A node id, matching the QModelIndex::internalId type
    using NodeID = quintptr;

    //! A list of node IDs
    using NodeIDList = std::vector<NodeID>;

    //! A node representing either a section or a property
    struct Node final {
      //! Section data
      struct SectionData final {
        //! The mandatory section name
        QString name;
      };

      //! Property data
      struct PropertyData final {
        //! The property value
        QString display_role;

        //! Additional data roles
        std::unordered_map<int, QVariant> value_map;
      };

      //! Either a SectionData or a PropertyData object
      using NodeData = std::variant<SectionData, PropertyData>;

      //! Node data
      NodeData data;

      //! Parent node id
      NodeID parent_node_id{};

      //! Child nodes
      NodeIDList child_id_list;
    };

    //! Node id generator, starting from the last top-level node ID
    NodeID node_id_generator{};

    //! The name of the active entity
    QString entity_name;

    //! All the nodes in the model
    std::unordered_map<NodeID, Node> node_map;
  };

  static void
  CreateProperty(Context &context, const QString &path,
                 const std::unordered_map<int, QVariant> &value_map = {});

  //! Generates a unique node ID
  static quintptr GenerateNodeID(Context &context);

  //! Resets the context structure
  static void ResetContext(Context &context);

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
