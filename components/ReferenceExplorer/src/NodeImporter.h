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
                    std::optional<std::size_t> opt_max_depth);

  //! Expands the specified node a new node
  void ExpandNode(const std::uint64_t &node_id,
                  std::optional<std::size_t> opt_max_depth);

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
  static void ImportEntity(NodeTree &node_tree, const IndexData &index_data,
                           const std::uint64_t &parent_node_id,
                           const RawEntityId &entity_id,
                           std::optional<std::size_t> opt_max_depth);

  //! Imports a Decl entity
  static void ImportEntity(NodeTree &node_tree, const IndexData &index_data,
                           const std::uint64_t &parent_node_id, mx::Decl decl,
                           std::optional<std::size_t> opt_max_depth);

  //! Expands the specified node id
  static void ExpandNode(NodeTree &node_tree, const IndexData &index_data,
                         const std::uint64_t &node_id,
                         std::optional<std::size_t> opt_max_depth);
};

}  // namespace mx::gui
