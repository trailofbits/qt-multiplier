/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetRelatedEntities.h"

#include <multiplier/ui/Util.h>

#include <multiplier/AST/Decl.h>

namespace mx::gui {

void GetRelatedEntities(
    QPromise<IDatabase::RelatedEntitiesResult> &related_entities_promise,
    const Index &index, RawEntityId entity_id) {

  IDatabase::RelatedEntities related_entities;
  VariantEntity ent = index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(ent)) {
    related_entities_promise.addResult(RPCErrorCode::InvalidEntityID);
    return;
  }

  // Name.
  related_entities.opt_name_tokens = NameOfEntity(ent);
  if (related_entities.opt_name_tokens) {
    std::string_view name = related_entities.opt_name_tokens.data();
    related_entities.name = QString::fromUtf8(
        name.data(), static_cast<qsizetype>(name.size()));
  }

  std::optional<RawEntityId> opt_primary_entity_id;

  if (std::holds_alternative<Decl>(ent)) {
    for (Decl redecl : std::get<Decl>(ent).redeclarations()) {
      if (related_entities_promise.isCanceled()) {
        return;
      }

      auto current_entity_id = redecl.id().Pack();
      if (!opt_primary_entity_id.has_value()) {
        opt_primary_entity_id = current_entity_id;
      }

      related_entities.entity_id_list.insert(current_entity_id);
    }
  } else {
    opt_primary_entity_id = entity_id;
    related_entities.entity_id_list.insert(entity_id);
  }

  related_entities.primary_entity_id = opt_primary_entity_id.value();

  related_entities_promise.addResult(std::move(related_entities));
}

}  // namespace mx::gui
