/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "Utils.h"

#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>
#include <utility>

namespace mx::gui {

RawEntityId NamedEntityContaining(VariantEntity entity) {
  if (std::holds_alternative<Decl>(entity)) {
    if (auto cd = NamedDeclContaining(std::get<Decl>(entity));
        cd != kInvalidEntityId) {
      return cd;
    }

    if (auto nd = NamedDecl::from(std::get<Decl>(entity))) {
      return nd->canonical_declaration().id().Pack();
    }

    // TODO(pag): Do token-based lookup?

  } else if (std::holds_alternative<Stmt>(entity)) {
    const Stmt &stmt = std::get<Stmt>(entity);

    if (auto nd = NamedDeclContaining(stmt); nd != kInvalidEntityId) {
      return nd;
    }

    // TODO(pag): Do token-based lookup?

    if (auto file = File::containing(stmt)) {
      return file->id().Pack();
    }

  } else if (std::holds_alternative<Macro>(entity)) {

    Macro macro = std::move(std::get<Macro>(entity));

    for (Token tok : macro.expansion_tokens()) {
      if (auto nd = NamedDeclContaining(tok); nd != kInvalidEntityId) {
        return nd;
      }
    }

    // If the macro wasn't used inside of a decl/statement, then go try to
    // find the macro definition containing this macro.
    for (auto parent = macro.parent(); parent; parent = macro.parent()) {
      macro = std::move(parent.value());
    }

    if (auto dd = DefineMacroDirective::from(macro)) {
      return dd->id().Pack();
    }

  } else if (std::holds_alternative<File>(entity)) {
    return std::get<File>(entity).id().Pack();

  } else if (std::holds_alternative<Fragment>(entity)) {
    if (auto file = File::containing(std::get<Fragment>(entity))) {
      return file->id().Pack();
    }

  } else if (std::holds_alternative<Designator>(entity)) {
    const Designator &d = std::get<Designator>(entity);
    if (auto field = d.field()) {
      return field->id().Pack();
    }

  } else if (std::holds_alternative<Token>(entity)) {
    const Token &tok = std::get<Token>(entity);

    if (auto pt = tok.parsed_token()) {
      if (auto nd = NamedDeclContaining(pt); nd != kInvalidEntityId) {
        return nd;
      }
    }

    for (Macro m : Macro::containing(tok)) {
      if (auto ne = NamedEntityContaining(std::move(m));
          ne != kInvalidEntityId) {
        return ne;
      }
    }

    if (auto dt = tok.derived_token()) {
      if (auto nd = NamedDeclContaining(dt); nd != kInvalidEntityId) {
        return nd;
      }
    }

    if (std::optional<Fragment> frag = Fragment::containing(tok)) {
      for (NamedDecl nd : NamedDecl::in(*frag)) {
        if (nd.tokens().index_of(tok)) {
          return nd.id().Pack();
        }
      }
    }
  }

  // TODO(pag): CXXBaseSpecifier, CXXTemplateArgument, CXXTemplateParameterList.

  return kInvalidEntityId;
}

//! Generate references to the entity with `entity`. The references
//! pairs of named entities and the referenced entity. Sometimes the
//! referenced entity will match the named entity, other times the named
//! entity will contain the reference (e.g. a function containing a call).
gap::generator<std::pair<RawEntityId, Reference>>
References(const VariantEntity &entity) {

  if (std::holds_alternative<NotAnEntity>(entity)) {
    co_return;

#define REFERENCES_TO_CATEGORY(type_name, lower_name, enum_name, category) \
  } else if (std::holds_alternative<type_name>(entity)) { \
    for (Reference ref : std::get<type_name>(entity).references()) { \
      RawEntityId eid = NamedEntityContaining(ref.as_variant()); \
      if (eid != kInvalidEntityId) { \
        co_yield std::pair<RawEntityId, Reference>(eid, std::move(ref)); \
      } \
    }

    MX_FOR_EACH_ENTITY_CATEGORY(REFERENCES_TO_CATEGORY, REFERENCES_TO_CATEGORY,
                                MX_IGNORE_ENTITY_CATEGORY,
                                REFERENCES_TO_CATEGORY, REFERENCES_TO_CATEGORY)
  } else {
    co_return;
  }
}

//! Return the file containing an entity.
std::optional<File> FileOfEntity(VariantEntity ent) {
  const auto VariantEntityVisitor = Overload{
    [] (const Decl &entity) { return File::containing(entity); },
    [] (const Stmt &entity) { return File::containing(entity); },
    [] (const Type &entity) { return File::containing(entity); },
    [] (const Token &entity) { return File::containing(entity); },
    [] (const Macro &entity) { return File::containing(entity); },
    [] (const Designator &entity) { return File::containing(entity); },
    [] (const CXXBaseSpecifier &entity) { return File::containing(entity); },
    [] (const TemplateArgument &entity) { return File::containing(entity); },
    [] (const TemplateParameterList &entity) { return File::containing(entity); },
    [] (const Fragment &entity) { return File::containing(entity); },
    [] (const File &entity) { return entity; },
    [](auto) -> std::optional<File> { return std::nullopt; }
  };
  return std::visit<std::optional<File>>(VariantEntityVisitor, ent);
}

//! Get the first file token associated with an entity.
//!
//! NOTE(pag): We prefer `TokenRange::file_tokens` as that walks up macros.
Token FirstFileToken(const VariantEntity &ent) {
  const auto VariantEntityVisitor = Overload{
    [] (const Decl &entity) { return entity.tokens().file_tokens().front(); },
    [] (const Stmt &entity) { return entity.tokens().file_tokens().front(); },
    [] (const Type &) { return Token(); },

    // Find the containing file usage of this, not necessarily the derived filed
    // token.
    [] (const Token &entity) {
      return TokenRange(entity).file_tokens().front();
    },

    [] (const Macro &entity) {
      for (Token tok : entity.use_tokens()) {
        return tok.file_token();
      }
      return Token();
    },
    [] (const Designator &entity) {
      return entity.tokens().file_tokens().front();
    },
    [] (const CXXBaseSpecifier &entity) {
      return entity.tokens().file_tokens().front();
    },
    [] (const TemplateArgument &) {
      return Token();
    },
    [] (const TemplateParameterList &entity) {
      return entity.tokens().file_tokens().front();
    },

    // NOTE(pag): We don't do `entity.parsed_tokens().file_tokens()` because
    //            if it's a pure macro fragment, then it might not have any
    //            parsed tokens.
    [] (const Fragment &entity) {
      return entity.file_tokens().front();
    },
    [] (const File &entity) { return entity.tokens().front(); },
    [](auto) { return Token(); }
  };
  return std::visit<Token>(VariantEntityVisitor, ent);
}

//! Return the name of an entity.
std::optional<QString> NameOfEntity(
    const VariantEntity &ent,
    const std::unordered_map<PackedFileId, QString> &file_paths) {

  const auto VariantEntityVisitor = Overload{
    [] (const Decl &decl) -> std::optional<QString> {
      if (auto named = NamedDecl::from(decl)) {
        auto name = named->name();
        return QString::fromUtf8(
            name.data(), static_cast<qsizetype>(name.size()));
      }
      return std::nullopt;
    },

    [] (const Macro &macro) -> std::optional<QString> {
      if (auto named = DefineMacroDirective::from(macro)) {
        auto name = named->name().data();
        return QString::fromUtf8(
            name.data(), static_cast<qsizetype>(name.size()));
      }
      return std::nullopt;
    },
    [&file_paths] (const File &file) -> std::optional<QString> {
      if (auto it = file_paths.find(file.id()); it != file_paths.end()) {
        return it->second;
      }
      return std::nullopt;
    },

    [] (auto) -> std::optional<QString> { return std::nullopt; }
  };
  return std::visit<std::optional<QString>>(VariantEntityVisitor, ent);
}

}  // namespace mx::gui
