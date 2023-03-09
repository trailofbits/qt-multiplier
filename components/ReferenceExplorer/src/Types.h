/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QDataStream>
#include <QString>

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <optional>

#include <multiplier/Index.h>
#include <multiplier/ui/IReferenceExplorerModel.h>

namespace mx::gui {

//! A single node in the model
struct Node final {
  static QString kMimeTypeName;

  //! How this node was imported
  IReferenceExplorerModel::ExpansionMode expansion_mode{
      IReferenceExplorerModel::CallHierarchyMode};

  // Initialize this node with a specific parent id node.
  void AssignUniqueId(void);

  //! The id for this node
  std::uint64_t node_id{};

  //! The parent node id
  std::uint64_t parent_node_id{0u};

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

  friend QDataStream &operator<<(QDataStream &stream, const Node &self);

  // NOTE(pag): May throw.
  friend QDataStream &operator>>(QDataStream &stream, Node &self);
};

//! A node tree representing the model data
struct NodeTree final {

  //! A map containing all the nodes in the tree, indexed by their unique
  //! node IDs.
  std::unordered_map<std::uint64_t, Node> node_map;

  //! The id of the root node. There are two separate IDs because we allow the
  //! tree to be "re-rooted." `root_node_id` reflects the true root of the tree,
  //! and `curr_root_node_id` reflects the current active / visible root.
  const std::uint64_t root_node_id;
  std::uint64_t curr_root_node_id;

  // Reset the tree and prevent the practical re-use of node IDs.
  NodeTree(void);

  Node *CurrentRootNode(void);
  const Node *CurrentRootNode(void) const;
};

}  // namespace mx::gui
