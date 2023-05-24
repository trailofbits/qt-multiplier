/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetRelatedEntities.h"
#include "Utils.h"

#include <multiplier/Entities/Decl.h>

namespace mx::gui {

void GetRelatedEntities(
    QPromise<IDatabase::RelatedEntitiesResult> &related_entities_promise,
    const Index &index, RawEntityId entity_id) {

  IDatabase::RelatedEntities related_entities;
  auto opt_name = LookupEntityName(index, entity_id);
  if (!opt_name.has_value() || opt_name->isEmpty()) {
    related_entities_promise.addResult(RPCErrorCode::InvalidEntityID);
    return;
  }

  related_entities.name = opt_name.value();

  VariantEntity ent = index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(ent)) {
    related_entities_promise.addResult(RPCErrorCode::InvalidEntityID);
    return;
  }

  if (std::holds_alternative<Decl>(ent)) {
    for (Decl redecl : std::get<Decl>(ent).redeclarations()) {
      if (related_entities_promise.isCanceled()) {
        return;
      }

      related_entities.entity_id_list.insert(redecl.id().Pack());
    }
  } else {
    related_entities.entity_id_list.insert(entity_id);
  }

  related_entities_promise.addResult(std::move(related_entities));
}

}  // namespace mx::gui
