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

void TaintedRootGenerator::run(void) {
  VariantEntity entity = Entity();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  static const auto kAlreadyExpanded{true};

  QList<Node> nodes;
  nodes.emplaceBack(Node::Create(FileCache(), entity, entity,
                                 IReferenceExplorerModel::TaintMode,
                                 kAlreadyExpanded));
  nodes.front().opt_name = TokensToString(entity);

  for (Node child_node : this->TaintedChildGenerator::GenerateNodes()) {
    if (CancelRequested()) {
      break;
    }

    child_node.parent_node_id = nodes.front().node_id;
    nodes.front().child_node_id_list.push_back(child_node.node_id);
    nodes.emplaceBack(std::move(child_node));
  }

  emit Finished(std::move(nodes), 0, ModelIndex());
}

}  // namespace mx::gui
