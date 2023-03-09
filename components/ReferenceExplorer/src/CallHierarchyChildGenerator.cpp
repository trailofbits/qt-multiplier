/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CallHierarchyChildGenerator.h"

#include <multiplier/File.h>
#include <multiplier/Index.h>

#include "Utils.h"

namespace mx::gui {

struct CallHierarchyChildGenerator::PrivateData {
  Index index;
  FileLocationCache file_cache;
  const RawEntityId entity_id;
  const QModelIndex parent;

  inline PrivateData(
      const Index &index_,
      const FileLocationCache &file_cache_,
      RawEntityId entity_id_,
      const QModelIndex &parent_)
      : index(index_),
        file_cache(file_cache_),
        entity_id(entity_id_),
        parent(parent_) {}
};

CallHierarchyChildGenerator::~CallHierarchyChildGenerator(void) {}

CallHierarchyChildGenerator::CallHierarchyChildGenerator(
    const Index &index_, const FileLocationCache &file_cache_,
    RawEntityId entity_id_, const QModelIndex &parent_)
    : d(new PrivateData(index_, file_cache_, entity_id_, parent_)) {}

void CallHierarchyChildGenerator::run() {
  VariantEntity entity = d->index.entity(d->entity_id);
  QVector<IReferenceExplorerModel::Node> nodes;
  int emitted_rows = 0;

  if (!std::holds_alternative<NotAnEntity>(entity)) {
    for (const auto &ref : References(entity)) {
      if (CancelRequested()) {
        break;
      }

      nodes.emplaceBack(IReferenceExplorerModel::Node::Create(
          d->file_cache, ref.first, ref.second,
          IReferenceExplorerModel::CallHierarchyMode));

      if (auto num_nodes = static_cast<int>(nodes.size());
          num_nodes >= 512) {

        emit NodesAvailable(std::move(nodes), emitted_rows, d->parent);
        emitted_rows += num_nodes;
        nodes.clear();
      }

      if (CancelRequested()) {
        break;
      }
    }
  }

  emit Finished(std::move(nodes), emitted_rows, d->parent);
}

}  // namespace mx::gui
