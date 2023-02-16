/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "Types.h"

namespace mx::gui {

//! A worker that can be used to import model nodes asynchronously
class NodeImporter final {
 public:
  //! Constructor
  NodeImporter(mx::Index index, mx::FileLocationCache file_location_cache,
               NodeTree &node_tree);

  //! Destructor
  ~NodeImporter();

  //! Imports a new node
  bool ImportEntity(RawEntityId entity_id,
                    const NodeTree::Node::ImportMode &import_mode,
                    const std::optional<std::uint64_t> opt_parent_node_id,
                    std::optional<std::size_t> opt_ttl);

  //! Expands the specified node a new node
  bool ExpandEntity(const std::uint64_t &node_id,
                    std::optional<std::size_t> opt_ttl);

  //! Disabled copy constructor
  NodeImporter(const NodeImporter &) = delete;

  //! Disabled copy assignment operator
  NodeImporter &operator=(const NodeImporter &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  //! Contains both the Index and the file location cache
  struct IndexData final {
    //! Multiplier's Index object
    mx::Index index;

    //! Multiplier's file location cache
    mx::FileLocationCache file_location_cache;

    //! The path map from mx::Index, keyed by id
    std::unordered_map<mx::PackedFileId, std::filesystem::path> file_path_map;
  };

  //! Imports an entity by id
  static void ImportEntityById(NodeTree &node_tree, const IndexData &index_data,
                               const std::uint64_t &parent_node_id,
                               const RawEntityId &entity_id,
                               std::optional<std::size_t> opt_ttl);

  //! Imports a decl entity
  static void ImportDeclEntity(NodeTree &node_tree, const IndexData &index_data,
                               const std::uint64_t &parent_node_id,
                               mx::Decl entity,
                               std::optional<std::size_t> opt_ttl);

  //! Imports a stmt entity
  static void ImportStmtEntity(NodeTree &node_tree, const IndexData &index_data,
                               const std::uint64_t &parent_node_id,
                               mx::Stmt entity,
                               std::optional<std::size_t> opt_ttl);

  //! Imports an attr entity
  static void ImportAttrEntity(NodeTree &node_tree, const IndexData &index_data,
                               const std::uint64_t &parent_node_id,
                               mx::Attr entity,
                               std::optional<std::size_t> opt_ttl);

  //! Imports a macro entity
  static void
  ImportMacroEntity(NodeTree &node_tree, const IndexData &index_data,
                    const std::uint64_t &parent_node_id, mx::Macro entity,
                    std::optional<std::size_t> opt_ttl);

  //! Imports a designator entity
  static void ImportDesignatorEntity(NodeTree &node_tree,
                                     const IndexData &index_data,
                                     const std::uint64_t &parent_node_id,
                                     mx::Designator entity,
                                     std::optional<std::size_t> opt_ttl);

  //! Imports a file entity
  static void ImportFileEntity(NodeTree &node_tree, const IndexData &index_data,
                               const std::uint64_t &parent_node_id,
                               mx::File entity,
                               std::optional<std::size_t> opt_ttl);
};

}  // namespace mx::gui
