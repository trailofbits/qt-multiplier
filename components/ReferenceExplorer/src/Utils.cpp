/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "Utils.h"

#include <utility>
#include <iostream>
namespace mx::gui {

VariantEntity NamedEntityContaining(const VariantEntity &entity,
                                    const VariantEntity &containing) {
  if (std::holds_alternative<Decl>(entity)) {

    if (auto contained_decl = std::get_if<Decl>(&containing);
        contained_decl && TypeDecl::from(*contained_decl)) {

      if (auto nd = NamedDecl::from(std::get<Decl>(entity))) {
        return nd->canonical_declaration();
      }
    }

    if (auto cd = NamedDeclContaining(std::get<Decl>(entity));
        !std::holds_alternative<NotAnEntity>(cd)) {
      return cd;
    }

    if (auto nd = NamedDecl::from(std::get<Decl>(entity))) {
      return nd->canonical_declaration();
    }

    // TODO(pag): Do token-based lookup?

  } else if (std::holds_alternative<Stmt>(entity)) {
    const Stmt &stmt = std::get<Stmt>(entity);

    if (auto nd = NamedDeclContaining(stmt);
        !std::holds_alternative<NotAnEntity>(nd)) {
      return nd;
    }

    // TODO(pag): Do token-based lookup?

    if (auto file = File::containing(stmt)) {
      return file.value();
    }

  } else if (std::holds_alternative<Macro>(entity)) {

    // It could be that we are looking at an expansion that isn't actually
    // used per se (e.g. the expansion happens as a result of PASTA eagerly
    // doing argument pre-expansions), but only the macro name gets used, so
    // we can't connect any final parsed tokens to anything, and thus we want
    // to instead go and find the root of the expansion and ask for the named
    // declaration containing that.
    //
    // Another reason to look at the root macro expansion is that we may be
    // asking for a use of a define that is in the same fragment as the
    // expansion, and we don't want the expansion to put us into the body of
    // a define, but to the use of the top-level macro expansion.
    Macro macro = std::move(std::get<Macro>(entity)).root();

    for (Token tok : macro.generate_expansion_tokens()) {
      if (Token pt = tok.parsed_token()) {
//        std::cerr << "PT " << pt.id() << ' ' << pt.data() << '\n';
        if (auto nd = NamedDeclContaining(pt);
            !std::holds_alternative<NotAnEntity>(nd)) {
          return nd;
        }
      } else {
//        std::cerr << "ET " << tok.id() << ' ' << tok.data() << '\n';
      }
    }

    // If the macro wasn't used inside of a decl/statement, then go try to
    // find the macro definition containing this macro.
    if (auto dd = DefineMacroDirective::from(macro)) {
      return dd.value();
    }

  } else if (std::holds_alternative<File>(entity)) {
    return entity;

  } else if (std::holds_alternative<Fragment>(entity)) {
    if (auto file = File::containing(std::get<Fragment>(entity))) {
      return file.value();
    }

  } else if (std::holds_alternative<Designator>(entity)) {
    const Designator &d = std::get<Designator>(entity);
    if (auto fd = d.field()) {
      return fd.value();
    }

  } else if (std::holds_alternative<Token>(entity)) {
    const Token &tok = std::get<Token>(entity);

    if (auto pt = tok.parsed_token()) {
      if (auto nd = NamedDeclContaining(pt);
          !std::holds_alternative<NotAnEntity>(nd)) {
        return nd;
      }
    }

    for (Macro m : Macro::containing(tok)) {
      if (auto ne = NamedEntityContaining(std::move(m), containing);
          !std::holds_alternative<NotAnEntity>(ne)) {
        return ne;
      }
    }

    if (auto dt = tok.derived_token()) {
      if (auto nd = NamedDeclContaining(dt);
          !std::holds_alternative<NotAnEntity>(nd)) {
        return nd;
      }
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

  return NotAnEntity{};
}

//! Generate references to the entity with `entity`. The references
//! pairs of named entities and the referenced entity. Sometimes the
//! referenced entity will match the named entity, other times the named
//! entity will contain the reference (e.g. a function containing a call).
gap::generator<std::pair<VariantEntity, VariantEntity>>
References(const VariantEntity &entity) {

  if (std::holds_alternative<NotAnEntity>(entity)) {
    co_return;

#define REFERENCES_TO_CATEGORY(type_name, lower_name, enum_name, category) \
  } else if (std::holds_alternative<type_name>(entity)) { \
    for (Reference ref : std::get<type_name>(entity).references()) { \
      VariantEntity rd = ref.as_variant(); \
      VariantEntity nd = NamedEntityContaining(rd, entity); \
      if (!std::holds_alternative<NotAnEntity>(nd)) { \
        co_yield std::pair<VariantEntity, VariantEntity>( \
            std::move(nd), std::move(rd)); \
      } \
    }

    MX_FOR_EACH_ENTITY_CATEGORY(REFERENCES_TO_CATEGORY, REFERENCES_TO_CATEGORY,
                                MX_IGNORE_ENTITY_CATEGORY,
                                REFERENCES_TO_CATEGORY, REFERENCES_TO_CATEGORY)
  } else {
    co_return;
  }
}

}  // namespace mx::gui
