/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetEntityName.h"

#include <multiplier/ui/Util.h>

#include <multiplier/Entities/Token.h>
#include <multiplier/Entities/TokenKind.h>

namespace mx::gui {

void GetEntityName(QPromise<OptionalName> &entity_name_promise,
                   const Index &index, const RawEntityId &entity_id) {

  auto variant_entity = index.entity(entity_id);

  auto opt_name = NameOfEntity(variant_entity);
  if (opt_name.has_value() && opt_name->isEmpty()) {
    opt_name = std::nullopt;
  }

  if (!opt_name.has_value()) {
    auto tokens = TokensToString(variant_entity);
    if (!tokens.isEmpty()) {
      opt_name = std::move(tokens);
    }
  }

  entity_name_promise.addResult(opt_name);
}

}  // namespace mx::gui
