/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetRelatedEntities.h"

#include <multiplier/Entities/Decl.h>

namespace mx::gui {

void GetRelatedEntities(
    QPromise<IDatabase::EntityIDList> &related_entities_promise,
    const Index &index, RawEntityId entity_id) {

  IDatabase::EntityIDList entity_list;

  VariantEntity ent = index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(ent)) {
    related_entities_promise.addResult(std::move(entity_list));
    return;
  }

  if (std::holds_alternative<Decl>(ent)) {
    for (Decl redecl : std::get<Decl>(ent).redeclarations()) {
      entity_list.insert(redecl.id().Pack());
    }
  } else {
    entity_list.insert(entity_id);
  }

  related_entities_promise.addResult(std::move(entity_list));
}

}  // namespace mx::gui
