/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TaintedRootGenerator.h"

#include "Utils.h"

namespace mx::gui {

TaintedRootGenerator::~TaintedRootGenerator(void) {}


gap::generator<IReferenceExplorerModel::Node>
TaintedRootGenerator::GenerateNodes(void) {
  VariantEntity entity = Entity();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    co_return;
  }

  auto root_node = IReferenceExplorerModel::Node::Create(
      FileCache(), entity, entity, IReferenceExplorerModel::AlreadyExpanded);
  root_node.opt_name = TokensToString(entity);

  QList<IReferenceExplorerModel::Node> child_nodes;
  for (IReferenceExplorerModel::Node child_node :
           this->TaintedChildGenerator::GenerateNodes()) {
    if (CancelRequested()) {
      break;
    }

    child_node.parent_node_id = root_node.node_id;
    root_node.child_node_id_list.push_back(child_node.node_id);
    child_nodes.emplaceBack(std::move(child_node));
  }

  co_yield root_node;
  for (auto node : child_nodes) {
    co_yield node;
  }
}

}  // namespace mx::gui
