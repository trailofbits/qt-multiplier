/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetRelatedEntities.h"
#include "Utils.h"

#include <multiplier/ui/Util.h>

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


  //
  // Get the token containing the entity name
  //

  // Warning: unreliable!
  //
  // TODO: There should be an API to acquire the token that is 1:1
  //       with the entity name

  // First attempt, try to acquire the token containing the entity
  // name using the related entity id
  {

    auto token = FirstFileToken(ent);

    auto related_entity_id = token.related_entity_id();
    auto related_entity = index.entity(related_entity_id);

    auto file_token = FirstFileToken(related_entity);

    auto file_token_view = file_token.data();
    auto file_token_size = static_cast<qsizetype>(file_token_view.size());

    auto token_name =
        QString::fromUtf8(file_token_view.data(), file_token_size);

    if (token_name == related_entities.name) {
      related_entities.opt_name_token = token;
    }
  }

  // Second attempt, go through all the file tokens and find
  // the one and only token matching the entity name. Skip it
  // if we find more
  if (!related_entities.opt_name_token.has_value()) {
    Token token_name;
    std::size_t match_count{};

    for (const auto &token : FileTokens(ent)) {
      auto token_view = token.data();
      auto token_size = static_cast<qsizetype>(token_view.size());

      auto token_string = QString::fromUtf8(token_view.data(), token_size);
      if (token_string == related_entities.name) {
        token_name = token;
        ++match_count;
      }
    }

    if (match_count == 1) {
      related_entities.opt_name_token = token_name;
    }
  }


  related_entities_promise.addResult(std::move(related_entities));
}

}  // namespace mx::gui
