// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "EntityNameResolver.h"

#include <multiplier/ui/Util.h>

#include <QThread>

namespace mx::gui {

struct EntityNameResolver::PrivateData final {
  Index index;
  RawEntityId entity_id{};
};

EntityNameResolver::EntityNameResolver(Index index,
                                       const RawEntityId &entity_id)
    : d(new PrivateData) {
  d->index = std::move(index);
  d->entity_id = entity_id;
}

EntityNameResolver::~EntityNameResolver() {}

void EntityNameResolver::run() {
  emit Finished(NameOfEntity(d->index.entity(d->entity_id)));
}

}  // namespace mx::gui
