/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "Types.h"

#include <atomic>

namespace mx::gui {
namespace {

static std::atomic<std::uint64_t> gNextNodeId(1u);

}  // namespace

QString Node::kMimeTypeName = "application/mx-reference-explorer-node";

// Initialize this node with a specific parent id node.
void Node::AssignUniqueId(void) {
  node_id = gNextNodeId.fetch_add(1u);
}

// Reset the tree and prevent the practical re-use of node IDs.
NodeTree::NodeTree(void)
    : root_node_id(gNextNodeId.fetch_add(1u)),
      curr_root_node_id(root_node_id) {
  node_map[root_node_id].node_id = root_node_id;
}

Node *NodeTree::CurrentRootNode(void) {
  if (auto it = node_map.find(curr_root_node_id); it != node_map.end()) {
    return &(it->second);
  } else {
    return nullptr;
  }}

const Node *NodeTree::CurrentRootNode(void) const {
  if (auto it = node_map.find(curr_root_node_id); it != node_map.end()) {
    return &(it->second);
  } else {
    return nullptr;
  }
}

QDataStream &operator<<(QDataStream &stream, const Node &node) {
  stream << node.node_id;
  stream << node.parent_node_id;
  stream << node.expansion_mode;
  stream << node.entity_id;
  stream << node.referenced_entity_id;

  if (node.opt_name.has_value()) {
    stream << true;
    stream << node.opt_name.value();
  } else {
    stream << false;
  }

  if (node.opt_location.has_value()) {
    stream << true;
    stream << node.opt_location.value();

  } else {
    stream << false;
  }

  auto child_node_id_list_size =
      static_cast<std::uint64_t>(node.child_node_id_list.size());

  stream << child_node_id_list_size;

  for (const auto &child_node_id : node.child_node_id_list) {
    stream << child_node_id;
  }

  return stream;
}

QDataStream &operator>>(QDataStream &stream, Node &node) {

  stream >> node.node_id;
  stream >> node.parent_node_id;
  stream >> node.expansion_mode;
  stream >> node.entity_id;
  stream >> node.referenced_entity_id;

  bool has_optional_field = false;

  // Read the name.
  stream >> has_optional_field;
  QString string_value;
  if (has_optional_field) {
    stream >> string_value;
    node.opt_name = string_value;
  }

  // Read the location.
  stream >> has_optional_field;
  if (has_optional_field) {
    IReferenceExplorerModel::Location location;
    stream >> location;
    node.opt_location = std::move(location);
  }

  std::uint64_t child_node_id_list_size{};
  stream >> child_node_id_list_size;

  for (std::uint64_t i = 0; i < child_node_id_list_size; ++i) {
    std::uint64_t child_node_id{};
    stream >> child_node_id;

    node.child_node_id_list.push_back(child_node_id);
  }

  return stream;
}

}  // namespace mx::gui
