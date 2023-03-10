// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "EntityNameResolver.h"

#include <multiplier/Entities/Token.h>
#include <multiplier/Entities/TokenKind.h>
#include <multiplier/ui/Util.h>

#include <QString>
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
  VariantEntity entity = d->index.entity(d->entity_id);
  std::optional<QString> name = NameOfEntity(entity);
  if (name && !name->isEmpty()) {
    emit Finished(name);
    return;
  }

  if (QString tokens = TokensToString(entity); !tokens.isEmpty()) {
    emit Finished(tokens);
    return;
  }

  emit Finished(std::nullopt);
}

}  // namespace mx::gui
