/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "Utils.h"

#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>

namespace mx::gui {

std::optional<NamedEntity> NamedEntityContaining(VariantEntity entity) {
  if (std::holds_alternative<Decl>(entity)) {
    if (auto cd = NamedDeclContaining(std::get<Decl>(entity))) {
      return cd.value();
    }

    if (auto nd = NamedDecl::from(std::get<Decl>(entity))) {
      return nd.value();
    }

    // TODO(pag): Do token-based lookup?

  } else if (std::holds_alternative<Stmt>(entity)) {
    const Stmt &stmt = std::get<Stmt>(entity);
    if (auto nd = NamedDeclContaining(stmt)) {
      return nd.value();
    }

    // TODO(pag): Do token-based lookup?

    if (auto file = File::containing(stmt)) {
      return file.value();
    }

  } else if (std::holds_alternative<Macro>(entity)) {
    Macro macro = std::move(std::get<Macro>(entity));
    if (auto dd = DefineMacroDirective::from(macro)) {
      return dd.value();
    }

    if (std::optional<Macro> parent = macro.parent()) {
      return NamedEntityContaining(std::move(parent.value()));
    }

    if (auto file = File::containing(macro)) {
      return file.value();
    }

  } else if (std::holds_alternative<File>(entity)) {
    return std::get<File>(entity);

  } else if (std::holds_alternative<Fragment>(entity)) {
    if (auto file = File::containing(std::get<Fragment>(entity))) {
      return file.value();
    }

  } else if (std::holds_alternative<Designator>(entity)) {
    const Designator &d = std::get<Designator>(entity);
    if (auto field = d.field()) {
      return field.value();
    }

  } else if (std::holds_alternative<Token>(entity)) {
    const Token &tok = std::get<Token>(entity);
    if (auto nd = NamedDeclContaining(tok)) {
      return nd.value();
    }

    if (std::optional<Fragment> frag = Fragment::containing(tok)) {
      for (NamedDecl nd : NamedDecl::in(*frag)) {
        if (nd.tokens().index_of(tok)) {
          return nd;
        }
      }
    }
  }

  // TODO(pag): CXXBaseSpecifier, CXXTemplateArgument, CXXTemplateParameterList.

  return std::nullopt;
}


//! Generate references to the entity with `entity`. The references
//! pairs of named entities and the referenced entity. Sometimes the
//! referenced entity will match the named entity, other times the named
//! entity will contain the reference (e.g. a function containing a call).
gap::generator<std::pair<std::optional<NamedEntity>, Reference>>
References(VariantEntity entity) {

  if (std::holds_alternative<NotAnEntity>(entity)) {
    co_return;

#define REFERENCES_TO_CATEGORY(type_name, lower_name, enum_name, category) \
  } \
  else if (std::holds_alternative<type_name>(entity)) { \
    for (Reference ref : std::get<type_name>(entity).references()) { \
      std::optional<NamedEntity> ne = NamedEntityContaining(ref.as_variant()); \
      co_yield std::make_pair<std::optional<NamedEntity>, Reference>( \
          std::move(ne), std::move(ref)); \
    }

    MX_FOR_EACH_ENTITY_CATEGORY(REFERENCES_TO_CATEGORY, REFERENCES_TO_CATEGORY,
                                MX_IGNORE_ENTITY_CATEGORY,
                                REFERENCES_TO_CATEGORY, REFERENCES_TO_CATEGORY)
  } else {
    co_return;
  }
}


}  // namespace mx::gui
