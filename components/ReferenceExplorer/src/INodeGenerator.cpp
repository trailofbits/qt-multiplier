/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "INodeGenerator.h"
#include "CallHierarchyChildGenerator.h"
#include "CallHierarchyRootGenerator.h"
#include "TaintedChildGenerator.h"
#include "TaintedRootGenerator.h"

namespace mx::gui {

INodeGenerator::~INodeGenerator(void) {}

void INodeGenerator::RequestCancel(void) {
  cancel_requested.testAndSetOrdered(0, 1);
}

bool INodeGenerator::CancelRequested(void) {
  return cancel_requested.loadAcquire() == 1;
}

gap::generator<Node> INodeGenerator::GenerateNodes(void) {
  co_return;
}

void INodeGenerator::run(void) {
  QVector<Node> nodes;
  int emitted_rows = 0;

  for (Node node : this->GenerateNodes()) {
    nodes.emplaceBack(std::move(node));

    if (auto num_nodes = static_cast<int>(nodes.size()); num_nodes >= 512) {

      emit NodesAvailable(std::move(nodes), emitted_rows, parent_index);
      emitted_rows += num_nodes;
      nodes.clear();
    }

    if (CancelRequested()) {
      break;
    }
  }

  emit Finished(std::move(nodes), emitted_rows, parent_index);
}

//! Create a node generator for a root node.
INodeGenerator *INodeGenerator::CreateRootGenerator(
    const Index &index, const FileLocationCache &file_cache,
    RawEntityId entity_id, const QModelIndex &parent,
    IReferenceExplorerModel::ExpansionMode expansion_mode) {

  switch (expansion_mode) {
    case IReferenceExplorerModel::AlreadyExpanded: return nullptr;
    case IReferenceExplorerModel::CallHierarchyMode:
      return new CallHierarchyRootGenerator(index, file_cache, entity_id,
                                            parent);
    case IReferenceExplorerModel::TaintMode:
      return new TaintedRootGenerator(index, file_cache, entity_id, parent);
  }
}

//! Create a node generator for a child node.
INodeGenerator *INodeGenerator::CreateChildGenerator(
    const Index &index, const FileLocationCache &file_cache,
    RawEntityId entity_id, const QModelIndex &parent,
    IReferenceExplorerModel::ExpansionMode expansion_mode) {

  switch (expansion_mode) {
    case IReferenceExplorerModel::AlreadyExpanded: return nullptr;
    case IReferenceExplorerModel::CallHierarchyMode:
      return new CallHierarchyChildGenerator(index, file_cache, entity_id,
                                             parent);
    case IReferenceExplorerModel::TaintMode:
      return new TaintedChildGenerator(index, file_cache, entity_id, parent);
  }
}

}  // namespace mx::gui
