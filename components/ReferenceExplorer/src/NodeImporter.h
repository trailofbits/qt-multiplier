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

  //! Imports a new node. Here, `entity_id` contains a reference, and the
  //! entity ID corresponding to the location of the reference is associated
  //! with `referenced_entity_id`.
  bool ImportEntity(RawEntityId entity_id, RawEntityId referenced_entity_id,
                    NodeTree::Node::ImportMode import_mode,
                    std::optional<std::uint64_t> opt_parent_node_id,
                    std::optional<std::size_t> opt_max_depth);

  //! Expands the specified node a new node
  void ExpandNode(std::uint64_t node_id,
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
    std::unordered_map<PackedFileId, QString> file_path_map;
  };

  //! Imports an entity by id
  static void ImportEntity(NodeTree &node_tree, const IndexData &index_data,
                           std::uint64_t parent_node_id, RawEntityId entity_id,
                           RawEntityId referenced_entity_id,
                           std::optional<std::size_t> opt_max_depth);

  //! Expands the specified node id
  static void ExpandNode(NodeTree &node_tree, const IndexData &index_data,
                         std::uint64_t node_id,
                         std::optional<std::size_t> opt_max_depth);
};

}  // namespace mx::gui
