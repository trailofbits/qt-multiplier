/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CallHierarchyChildGenerator.h"
#include "Utils.h"

#include <multiplier/File.h>
#include <multiplier/Index.h>

namespace mx::gui {

struct CallHierarchyChildGenerator::PrivateData {
  Index index;
  FileLocationCache file_cache;
  const RawEntityId entity_id;

  inline PrivateData(const Index &index_, const FileLocationCache &file_cache_,
                     RawEntityId entity_id_)
      : index(index_),
        file_cache(file_cache_),
        entity_id(entity_id_) {}
};

CallHierarchyChildGenerator::~CallHierarchyChildGenerator(void) {}

CallHierarchyChildGenerator::CallHierarchyChildGenerator(
    const Index &index_, const FileLocationCache &file_cache_,
    RawEntityId entity_id_, const QModelIndex &parent_)
    : INodeGenerator(parent_),
      d(new PrivateData(index_, file_cache_, entity_id_)) {}

gap::generator<Node> CallHierarchyChildGenerator::GenerateNodes(void) {
  VariantEntity entity = d->index.entity(d->entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    co_return;
  }

  static const auto kNotYetExpanded{false};

  for (const std::pair<VariantEntity, VariantEntity> &ref :
       References(entity)) {
    co_yield Node::Create(d->file_cache, ref.first, ref.second,
                          IReferenceExplorerModel::CallHierarchyMode,
                          kNotYetExpanded);
  }
}

}  // namespace mx::gui
