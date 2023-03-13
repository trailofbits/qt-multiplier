// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CallHierarchyRootGenerator.h"
#include "Utils.h"

#include <multiplier/File.h>
#include <multiplier/Index.h>

namespace mx::gui {

namespace {

// If it's a declaration, then we want to have multiple roots where each
// root is a redeclaration of the entity. The first redeclaration is the
// definition if one is available.
static gap::generator<VariantEntity> TopLevelEntities(VariantEntity entity) {
  if (std::holds_alternative<Decl>(entity)) {
    for (Decl decl : std::get<Decl>(entity).redeclarations()) {
      co_yield VariantEntity(std::move(decl));
    }

  } else {
    co_yield entity;
  }
}

}  // namespace

struct CallHierarchyRootGenerator::PrivateData final {
  Index index;
  FileLocationCache file_cache;
  RawEntityId entity_id{};

  inline PrivateData(const Index &index_, const FileLocationCache &file_cache_)
      : index(index_),
        file_cache(file_cache_) {}
};

CallHierarchyRootGenerator::CallHierarchyRootGenerator(
    const Index &index, const FileLocationCache &file_cache_,
    RawEntityId entity_id, const QModelIndex &location)
    : INodeGenerator(location),
      d(new PrivateData(index, file_cache_)) {

  d->entity_id = entity_id;
}

CallHierarchyRootGenerator::~CallHierarchyRootGenerator() {}

gap::generator<Node> CallHierarchyRootGenerator::GenerateNodes(void) {
  VariantEntity entity = d->index.entity(d->entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    co_return;
  }

  auto is_first = true;
  for (VariantEntity root_entity : TopLevelEntities(std::move(entity))) {
    Node first_node = Node::Create(
        d->file_cache, root_entity, root_entity,
        IReferenceExplorerModel::AlreadyExpanded);

    if (CancelRequested()) {
      break;
    }

    if (is_first) {
      QList<Node> child_nodes;
      for (const auto &ref : References(root_entity)) {
        if (CancelRequested()) {
          break;
        }

        auto child_node = Node::Create(
            d->file_cache, ref.first, ref.second,
            IReferenceExplorerModel::CallHierarchyMode);

        first_node.child_node_id_list.push_back(child_node.node_id);
        child_node.parent_node_id = first_node.node_id;
        child_nodes.emplaceBack(std::move(child_node));
      }

      co_yield first_node;
      for (auto child_node : child_nodes) {
        co_yield child_node;
      }

      is_first = false;
    } else {
      co_yield first_node;
    }
  }
}

}  // namespace mx::gui
