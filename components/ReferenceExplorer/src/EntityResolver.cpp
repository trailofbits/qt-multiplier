// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "EntityResolver.h"

#include <multiplier/ui/Util.h>

#include <QThread>

namespace mx::gui {

struct EntityResolver::PrivateData final {
  Index index;
  RawEntityId entity_id{};
  IReferenceExplorerModel::ExpansionMode mode;
  QModelIndex loc;
};

EntityResolver::EntityResolver(const Index &index,
                               const RawEntityId &entity_id,
                               IReferenceExplorerModel::ExpansionMode mode,
                               const QModelIndex &location)
    : d(new PrivateData) {

  d->index = index;
  d->entity_id = entity_id;
  d->loc = location;
  d->mode = mode;
}

EntityResolver::~EntityResolver() {}

void EntityResolver::run() {
  VariantEntity entity = d->index.entity(d->entity_id);
  if (std::holds_alternative<Decl>(entity)) {
    entity = std::get<Decl>(entity).canonical_declaration();
  }
  emit Finished(std::move(entity), d->mode, d->loc);
}

}  // namespace mx::gui
