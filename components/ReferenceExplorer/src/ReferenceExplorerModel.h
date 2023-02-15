/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <unordered_set>
#include <unordered_map>
#include <filesystem>

#include <multiplier/ui/IReferenceExplorerModel.h>

namespace mx::gui {

//! Implements the IReferenceExplorerModel interface
class ReferenceExplorerModel final : public IReferenceExplorerModel {
  Q_OBJECT

 public:
  //! \copybrief IReferenceExplorerModel::AppendEntityObject
  virtual bool AppendEntityObject(RawEntityId entity_id, EntityObjectType type,
                                  const QModelIndex &parent,
                                  std::optional<std::size_t> opt_ttl) override;

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

  //! A node tree representing the model data
  struct NodeTree final {
    //! A single node in the model
    struct Node final {
      //! Multiplier-specific identifiers
      struct Identifiers final {
        //! An optional file id
        std::optional<RawEntityId> opt_file_id;

        //! Fragment identifier
        RawEntityId fragment_id{};

        //! Entity identifier
        RawEntityId entity_id{};
      };

      //! The id for this node
      std::uint64_t node_id{};

      //! The parent node id
      std::uint64_t parent_node_id{};

      //! Multiplier-specific identifiers
      Identifiers identifiers;

      //! An optional name for this entity
      std::optional<std::string> opt_name;

      //! Optional file location information (path + line + column)
      std::optional<Location> opt_location;

      //! Child nodes
      std::vector<std::uint64_t> child_node_id_list;
    };

    //! A list of visited decl entity IDs
    std::unordered_set<mx::RawEntityId> visited_entity_id_set;

    //! A map containing all the nodes in the tree
    std::unordered_map<std::uint64_t, Node> node_map;
  };

  //! Contains both the Index and the file location cache
  struct IndexData final {
    //! Multiplier's Index object
    mx::Index index;

    //! Multiplier's file location cache
    mx::FileLocationCache file_location_cache;

    //! The path map from mx::Index, keyed by id
    std::unordered_map<mx::PackedFileId, std::filesystem::path> file_path_map;
  };

  //! Initializes the node tree object
  static void InitializeNodeTree(NodeTree &node_tree);

  //!
  static void ImportEntityById(NodeTree &node_tree, const IndexData &index_data,
                               const std::uint64_t &parent_node_id,
                               const RawEntityId &entity_id,
                               std::optional<std::size_t> opt_ttl);

  //!
  static void ImportDeclEntity(NodeTree &node_tree, const IndexData &index_data,
                               const std::uint64_t &parent_node_id,
                               mx::Decl entity,
                               std::optional<std::size_t> opt_ttl);

  //!
  static void ImportStmtEntity(NodeTree &node_tree, const IndexData &index_data,
                               const std::uint64_t &parent_node_id,
                               mx::Stmt entity,
                               std::optional<std::size_t> opt_ttl);

  //!
  static void ImportAttrEntity(NodeTree &node_tree, const IndexData &index_data,
                               const std::uint64_t &parent_node_id,
                               mx::Attr entity,
                               std::optional<std::size_t> opt_ttl);

  //!
  static void
  ImportMacroEntity(NodeTree &node_tree, const IndexData &index_data,
                    const std::uint64_t &parent_node_id, mx::Macro entity,
                    std::optional<std::size_t> opt_ttl);

  //!
  static void ImportDesignatorEntity(NodeTree &node_tree,
                                     const IndexData &index_data,
                                     const std::uint64_t &parent_node_id,
                                     mx::Designator entity,
                                     std::optional<std::size_t> opt_ttl);

  //!
  static void ImportFileEntity(NodeTree &node_tree, const IndexData &index_data,
                               const std::uint64_t &parent_node_id,
                               mx::File entity,
                               std::optional<std::size_t> opt_ttl);

  //! Serializes the given node
  static void SerializeNode(QDataStream &stream, const NodeTree::Node &node);

  //! Deserializes a node from the given data stream
  static std::optional<NodeTree::Node> DeserializeNode(QDataStream &stream);

  friend class IReferenceExplorerModel;
};

}  // namespace mx::gui
