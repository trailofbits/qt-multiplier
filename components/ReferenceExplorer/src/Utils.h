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

namespace mx::gui {

std::optional<NamedEntity> NamedEntityContaining(VariantEntity entity);

//! Generate references to the entity with `entity`. The references
//! pairs of named entities and the referenced entity. Sometimes the
//! referenced entity will match the named entity, other times the named
//! entity will contain the reference (e.g. a function containing a call).
gap::generator<std::pair<std::optional<NamedEntity>, Reference>>
References(VariantEntity entity);

template <typename T>
static std::optional<NamedDecl> NamedDeclContaining(const T &thing) {
  for (FunctionDecl func : FunctionDecl::containing(thing)) {
    return func;
  }

  for (FieldDecl field : FieldDecl::containing(thing)) {
    return field;
  }

  for (VarDecl var : VarDecl::containing(thing)) {
    if (var.is_local_variable_declaration()) {
      return NamedDeclContaining<Decl>(var);

    } else {
      return var;
    }
  }

  for (NamedDecl nd : NamedDecl::containing(thing)) {
    return nd;
  }

  return std::nullopt;
}

template <typename... Ts>
struct Overload final : Ts... {
  using Ts::operator()...;
};

template <typename... Ts>
Overload(Ts...) -> Overload<Ts...>;

}  // namespace mx::gui
