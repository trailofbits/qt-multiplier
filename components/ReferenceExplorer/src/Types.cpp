/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "Types.h"

#include <QAtomicInt>

#include <filesystem>
#include <multiplier/File.h>
#include <multiplier/Index.h>
#include <multiplier/ui/Assert.h>

#include "Utils.h"

namespace mx::gui {
namespace {

static QAtomicInteger<std::uint64_t> gNextNodeId(1u);

}  // namespace

QString Node::kMimeTypeName = "application/mx-reference-explorer-node-tree";

QDataStream &operator<<(QDataStream &stream, const Location &location) {
  stream << location.path;
  stream << location.file_id;
  stream << location.line;
  stream << location.column;
  return stream;
}

QDataStream &operator>>(QDataStream &stream, Location &location) {

  stream >> location.path;
  stream >> location.file_id;
  stream >> location.line;
  stream >> location.column;
  return stream;
}

// Create and initialize a location.
//
// NOTE(pag): This is a blocking operation.
std::optional<Location> Location::Create(const FileLocationCache &file_cache,
                                         const VariantEntity &entity) {
  Token file_tok = FirstFileToken(entity).file_token();
  if (!file_tok) {
    return std::nullopt;
  }

  std::optional<File> file = File::containing(file_tok);
  if (!file.has_value()) {
    Assert(file.has_value(), "Token::file_token returned non-file token?");
    return std::nullopt;
  }

  Location location;
  location.file_id = file->id().Pack();

  for (std::filesystem::path path : file->paths()) {
    location.path = QString::fromStdString(path.generic_string());
  }

  Assert(!location.path.isEmpty(), "Empty file paths aren't allowed");

  if (auto line_col = file_tok.location(file_cache)) {
    location.line = line_col->first;
    location.column = line_col->second;
  }

  return location;
}

Node Node::Create(const FileLocationCache &file_cache,
                  const VariantEntity &entity,
                  const VariantEntity &referenced_entity,
                  IReferenceExplorerModel::ExpansionMode import_mode,
                  const bool &expanded,
                  const std::optional<QString> opt_breadcrumbs) {

  Node node;
  node.node_id = gNextNodeId.fetchAndAddOrdered(1);
  node.expansion_mode = import_mode;
  node.entity_id = IdOfEntity(entity);
  node.opt_name = NameOfEntity(entity);
  node.expanded = expanded;
  node.opt_breadcrumbs = opt_breadcrumbs;

  node.opt_location = Location::Create(file_cache, referenced_entity);
  if (!node.opt_location.has_value()) {
    node.opt_location = Location::Create(file_cache, entity);
  }

  // Root nodes reference themselves.
  node.referenced_entity_id = IdOfEntity(referenced_entity);

  return node;
}

// Initialize this node with a specific parent id node.
void Node::AssignUniqueId(void) {
  node_id = gNextNodeId.fetchAndAddOrdered(1);
}

// Reset the tree and prevent the practical re-use of node IDs.
NodeTree::NodeTree(void)
    : root_node_id(gNextNodeId.fetchAndAddOrdered(1)),
      curr_root_node_id(root_node_id) {
  node_map[root_node_id].node_id = root_node_id;
}

Node *NodeTree::CurrentRootNode(void) {
  if (auto it = node_map.find(curr_root_node_id); it != node_map.end()) {
    return &(it->second);
  } else {
    return nullptr;
  }
}

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
  stream << node.expanded;

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

  if (node.opt_breadcrumbs.has_value()) {
    stream << true;
    stream << node.opt_breadcrumbs.value();
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
  stream >> node.expanded;

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
    Location location;
    stream >> location;
    node.opt_location = std::move(location);
  }

  // Read the breadcrumbs.
  stream >> has_optional_field;
  if (has_optional_field) {
    stream >> string_value;
    node.opt_breadcrumbs = string_value;
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
