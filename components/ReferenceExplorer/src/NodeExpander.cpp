/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "NodeExpander.h"

#include "NodeImporter.h"
#include "Utils.h"

namespace mx::gui {

void CallHierarchyNodeExpander::run() {
  VariantEntity entity = index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    emit Finished(parent, 0);
    return;
  }

  std::vector<Node> nodes;
  int emitted_rows = 0;

  for (auto &ref : References(entity)) {
    if (CancelRequested()) {
      break;
    }

    nodes.emplace_back(NodeImporter::CreateNode(
        file_cache, std::move(ref.first), std::move(ref.second),
        IReferenceExplorerModel::CallHierarchyMode));

    if (auto num_nodes = static_cast<int>(nodes.size());
        num_nodes >= 512) {

      emit NodesAvailable(std::move(nodes), parent, emitted_rows);
      emitted_rows += num_nodes;
      nodes.clear();
    }

    if (CancelRequested()) {
      break;
    }
  }

  if (!nodes.empty()) {
    emit NodesAvailable(std::move(nodes), parent, emitted_rows);
  }

  emit Finished(parent, emitted_rows);
}

NodeExpander::~NodeExpander(void) {}

void NodeExpander::RequestCancel(void) {
  cancel_requested.testAndSetOrdered(0, 1);
}

bool NodeExpander::CancelRequested(void) {
  return cancel_requested.loadAcquire() == 1;
}

NodeExpander *NodeExpander::CreateNodeExpander(
    const Index &index,
    const FileLocationCache &file_cache_,
    RawEntityId entity_id,
    const QModelIndex &parent,
    IReferenceExplorerModel::ExpansionMode expansion_mode) {
  switch (expansion_mode) {
    case IReferenceExplorerModel::AlreadyExpanded:
      return nullptr;
    case IReferenceExplorerModel::CallHierarchyMode:
      return new CallHierarchyNodeExpander(index, file_cache_, entity_id,
                                           parent);
  }
}

}  // namespace mx::gui
