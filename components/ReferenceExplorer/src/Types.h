/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QDataStream>
#include <QMetaType>
#include <QString>

#include <filesystem>
#include <multiplier/Entity.h>
#include <multiplier/ui/IReferenceExplorerModel.h>
#include <unordered_map>

namespace mx::gui {

//! Location information, containing path + line and column
struct Location final {
  RawEntityId file_id{kInvalidEntityId};

  //! File path
  QString path;

  //! An optional line number
  unsigned line{0};

  //! An optional column number
  unsigned column{0};

  // Create and initialize a location.
  //
  // NOTE(pag): This is a blocking operation.
  static std::optional<Location> Create(const FileLocationCache &file_cache,
                                        const VariantEntity &entity);
};

QDataStream &operator<<(QDataStream &stream, const Location &self);

// NOTE(pag): May throw.
QDataStream &operator>>(QDataStream &stream, Location &self);

//! A single node in the model
struct Node final {
  static QString kMimeTypeName;

  //! How this node was imported
  IReferenceExplorerModel::ExpansionMode expansion_mode{};

  // Create and initialize a node.
  //
  // NOTE(pag): This is a blocking operation.
  static Node Create(const FileLocationCache &file_cache,
                     const VariantEntity &entity,
                     const VariantEntity &referenced_entity,
                     IReferenceExplorerModel::ExpansionMode import_mode);

  // Initialize this node with a specific parent id node.
  void AssignUniqueId(void);

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
  std::optional<Location> opt_location;

  //! Child nodes
  std::vector<std::uint64_t> child_node_id_list;
};

QDataStream &operator<<(QDataStream &stream, const Node &self);

// NOTE(pag): May throw.
QDataStream &operator>>(QDataStream &stream, Node &self);

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

//! Allows mx::gui::Location and mx::gui::Node values to fit inside
//! QVariant objects.
Q_DECLARE_METATYPE(mx::gui::Location);
Q_DECLARE_METATYPE(mx::gui::Node);
