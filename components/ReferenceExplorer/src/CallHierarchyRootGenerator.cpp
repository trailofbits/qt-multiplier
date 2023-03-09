// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CallHierarchyRootGenerator.h"

#include <multiplier/File.h>
#include <multiplier/Index.h>

#include "Utils.h"

namespace mx::gui {

struct CallHierarchyRootGenerator::PrivateData final {
  Index index;
  FileLocationCache file_cache;
  RawEntityId entity_id{};
  QModelIndex loc;

  inline PrivateData(const Index &index_, const FileLocationCache &file_cache_)
      : index(index_),
        file_cache(file_cache_) {}
};

CallHierarchyRootGenerator::CallHierarchyRootGenerator(
    const Index &index, const FileLocationCache &file_cache_,
    RawEntityId entity_id, const QModelIndex &location)
    : d(new PrivateData(index, file_cache_)) {

  d->entity_id = entity_id;
  d->loc = location;
}

CallHierarchyRootGenerator::~CallHierarchyRootGenerator() {}

void CallHierarchyRootGenerator::run() {
  VariantEntity entity = d->index.entity(d->entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  // TODO(pag): Cancellation.

  QVector<IReferenceExplorerModel::Node> nodes;

  // If it's a declaration, then we want to have multiple roots where each
  // root is a redeclaration of the entity. The first redeclaration is the
  // definition if one is available.
  if (std::holds_alternative<Decl>(entity)) {
    for (Decl decl : std::get<Decl>(entity).redeclarations()) {
      VariantEntity decl_entity(std::move(decl));
      nodes.emplaceBack(IReferenceExplorerModel::Node::Create(
          d->file_cache, decl_entity, decl_entity,
          IReferenceExplorerModel::AlreadyExpanded));
    }

  } else {
    nodes.emplaceBack(IReferenceExplorerModel::Node::Create(
        d->file_cache, entity, entity,
        IReferenceExplorerModel::AlreadyExpanded));
  }

  if (nodes.empty()) {
    return;
  }

  // For the call hierarchy, we want to expand the first node up to the first
  // level.
  for (const auto &ref : References(entity)) {
    auto &added_node = nodes.emplaceBack(IReferenceExplorerModel::Node::Create(
        d->file_cache, ref.first, ref.second,
        IReferenceExplorerModel::CallHierarchyMode));

    nodes[0].child_node_id_list.push_back(added_node.node_id);
    added_node.parent_node_id = nodes[0].node_id;
  }

  // Insert these nows before row `0`.
  emit Finished(std::move(nodes), 0, d->loc);
}

}  // namespace mx::gui
