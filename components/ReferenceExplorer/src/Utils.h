/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "Types.h"

#include <multiplier/Entities/FunctionDecl.h>
#include <multiplier/Entities/FieldDecl.h>
#include <multiplier/Entities/VarDecl.h>
#include <multiplier/ui/Util.h>
#include <QString>
#include <unordered_map>

namespace mx::gui {

// Return the ID of the entity with a name that contains `entity`.
RawEntityId NamedEntityContaining(VariantEntity entity);

//! Generate references to the entity with `entity`. The references
//! pairs of named entities and the referenced entity. Sometimes the
//! referenced entity will match the named entity, other times the named
//! entity will contain the reference (e.g. a function containing a call).
gap::generator<std::pair<RawEntityId, Reference>>
References(VariantEntity entity);

//! Return the name of an entity.
std::optional<QString> NameOfEntity(
    const VariantEntity &ent,
    const std::unordered_map<RawEntityId, QString> &file_paths);

template <typename T>
static RawEntityId NamedDeclContaining(const T &thing) {
  for (FunctionDecl func : FunctionDecl::containing(thing)) {
    return func.id().Pack();
  }

  for (FieldDecl field : FieldDecl::containing(thing)) {
    return field.id().Pack();
  }

  for (VarDecl var : VarDecl::containing(thing)) {
    if (var.is_local_variable_declaration_or_parm()) {
      if (RawEntityId eid = NamedDeclContaining<Decl>(var);
          eid != kInvalidEntityId) {
        return eid;
      }

    } else {
      return var.id().Pack();
    }
  }

  for (NamedDecl nd : NamedDecl::containing(thing)) {
    return nd.id().Pack();
  }

  return kInvalidEntityId;
}

template <typename... Ts>
struct Overload final : Ts... {
  using Ts::operator()...;
};

template <typename... Ts>
Overload(Ts...) -> Overload<Ts...>;

}  // namespace mx::gui
