/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QString>

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <optional>

#include <multiplier/Index.h>
#include <multiplier/ui/IReferenceExplorerModel.h>

namespace mx::gui {

//! A node tree representing the model data
struct NodeTree final {
  //! A single node in the model
  struct Node final {
    //! Import modes
    enum class ImportMode { CallHierarchy };

    //! How this node was imported
    ImportMode import_mode;

    //! The id for this node
    std::uint64_t node_id{};

    //! The parent node id
    std::uint64_t parent_node_id{};

    //! Multiplier's entity id
    RawEntityId entity_id{kInvalidEntityId};

    //! Multiplier's referenced entity id
    RawEntityId referenced_entity_id{kInvalidEntityId};

    //! An optional name for this entity
    std::optional<QString> opt_name;

    //! Optional file location information (path + line + column)
    std::optional<IReferenceExplorerModel::Location> opt_location;

    //! Child nodes
    std::vector<std::uint64_t> child_node_id_list;
  };

  //! A list of visited decl entity IDs
  std::unordered_set<mx::RawEntityId> visited_entity_id_set;

  //! A map containing all the nodes in the tree
  std::unordered_map<std::uint64_t, Node> node_map;
};

}  // namespace mx::gui
