// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "CallHierarchyRootsResolver.h"

#include <multiplier/ui/Util.h>

#include "NodeImporter.h"
#include "Utils.h"

namespace mx::gui {

struct CallHierarchyRootsResolver::PrivateData final {
  Index index;
  FileLocationCache file_cache;
  RawEntityId entity_id{};
  QModelIndex loc;

  inline PrivateData(const Index &index_, const FileLocationCache &file_cache_)
      : index(index_),
        file_cache(file_cache_) {}
};

CallHierarchyRootsResolver::CallHierarchyRootsResolver(
    const Index &index, const FileLocationCache &file_cache_,
    const RawEntityId &entity_id, const QModelIndex &location)
    : d(new PrivateData(index, file_cache_)) {

  d->entity_id = entity_id;
  d->loc = location;
}

CallHierarchyRootsResolver::~CallHierarchyRootsResolver() {}

void CallHierarchyRootsResolver::run() {
  VariantEntity entity = d->index.entity(d->entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  std::vector<Node> nodes;

  // If it's a declaration, then we want to have multiple roots where each
  // root is a redeclaration of the entity. The first redeclaration is the
  // definition if one is available.
  if (std::holds_alternative<Decl>(entity)) {
    for (Decl decl : std::get<Decl>(entity).redeclarations()) {
      nodes.emplace_back(NodeImporter::CreateNode(
          d->file_cache, decl, decl,
          IReferenceExplorerModel::AlreadyExpanded));
    }
  } else {
    nodes.emplace_back(NodeImporter::CreateNode(
        d->file_cache, entity, entity,
        IReferenceExplorerModel::AlreadyExpanded));
  }

  if (nodes.empty()) {
    return;
  }

  // For the call hierarchy, we want to expand the first node up to the first
  // level.
  for (auto &ref : References(entity)) {
    Node &added_node = nodes.emplace_back(NodeImporter::CreateNode(
        d->file_cache, std::move(ref.first), std::move(ref.second),
        IReferenceExplorerModel::CallHierarchyMode));
    nodes[0].child_node_id_list.push_back(added_node.node_id);
    added_node.parent_node_id = nodes[0].node_id;
  }

  emit Finished(std::move(nodes), d->loc, 0);
}

}  // namespace mx::gui
