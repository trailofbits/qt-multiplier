/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetEntityInformation.h"

#include <filesystem>
#include <multiplier/Entities/CallExpr.h>
#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/File.h>
#include <multiplier/Entities/Fragment.h>
#include <multiplier/Entities/FunctionDecl.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>
#include <multiplier/Entities/MacroExpansion.h>
#include <multiplier/Entities/NamedDecl.h>
#include <multiplier/ui/Assert.h>
#include <multiplier/ui/Util.h>

#include <iostream>

namespace mx::gui {
namespace {

static std::optional<EntityInformation::Location> GetLocation(
    const TokenRange &toks, const FileLocationCache &file_location_cache) {
  for (Token tok : toks.file_tokens()) {
    auto file = File::containing(tok);
    if (!file) {
      continue;
    }

    if (auto line_col = tok.location(file_location_cache)) {
      return EntityInformation::Location(file.value(), line_col->first,
                                         line_col->second);
    }
  }

  return std::nullopt;
}

// Fill in the macros used within a given token range.
static void FillUsedMacros(const FileLocationCache &file_location_cache,
                           EntityInformation &info, const TokenRange &tokens) {
  std::vector<PackedMacroId> seen;

  for (Token tok : tokens) {
    for (Macro macro : Macro::containing(tok)) {
      macro = macro.root();
      if (macro.kind() != MacroKind::EXPANSION) {
        break;
      }

      PackedMacroId macro_id = macro.id();
      if (std::find(seen.begin(), seen.end(), macro_id) != seen.end()) {
        break;
      }

      seen.push_back(macro_id);
      std::optional<MacroExpansion> exp = MacroExpansion::from(macro);
      if (!exp) {
        break;
      }

      auto def = exp->definition();
      if (!def) {
        break;
      }

      TokenRange use_tokens = exp->use_tokens();
      for (Token use_tok : use_tokens) {
        if (use_tok.category() == TokenCategory::MACRO_NAME) {
          EntityInformation::Selection &sel = info.macros_used.emplace_back();
          sel.display_role.setValue(use_tok);
          sel.entity_role = std::move(exp.value());
          sel.location = GetLocation(use_tokens, file_location_cache);
          break;
        }
      }
      break;
    }
  }
}

//! Fill `info` with information about the declaration `entity`.
static EntityInformation GetDeclInformation(
    const FileLocationCache &file_location_cache, NamedDecl entity) {

  std::string_view name = entity.name();

  EntityInformation info;
  info.id = entity.id().Pack();
  info.entity.entity_role = entity;
  info.entity.location = GetLocation(entity.tokens(), file_location_cache);

  if (!name.empty()) {
    info.entity.display_role.setValue(entity.token());
  }

  // Fill all redeclarations.
  for (NamedDecl redecl : entity.redeclarations()) {

    // Collect all macros used by all redeclarations.
    FillUsedMacros(file_location_cache, info, redecl.tokens());

    EntityInformation::Selection &sel = info.redeclarations.emplace_back();
    sel.display_role.setValue(redecl.token());
    sel.entity_role = redecl;
    sel.location = GetLocation(redecl.tokens(), file_location_cache);

    // Try to get a name from elsewhere if we're missing it.
    if (!info.entity.display_role.isValid()) {
      name = redecl.name();
      if (!name.empty()) {
        info.entity.display_role.setValue(redecl.token());
      }
    }
  }

  // If this is a function, then look at who it calls, and who calls it.
  if (auto func = FunctionDecl::from(entity)) {
    for (CallExpr call : func->callers()) {
      for (FunctionDecl caller_func : FunctionDecl::containing(call)) {
        EntityInformation::Selection &sel = info.callers.emplace_back();
        sel.display_role.setValue(caller_func.token());
        sel.entity_role = call;
        sel.location = GetLocation(call.tokens(), file_location_cache);
        break;
      }
    }

    // Find the callees. Slightly annoying as we kind of have to invent a join.
    //
    // TODO(pag): Make `::in(entity)` work for all entities, not just files
    //            and fragments.
    Fragment frag = Fragment::containing(entity);
    for (CallExpr call : CallExpr::in(frag)) {
      std::optional<FunctionDecl> callee = call.direct_callee();
      if (!callee) {
        continue;  // TODO(pag): Look at how SciTools renders indirect callees.
      }

      // Make sure the callee is nested inside of `entity` (i.e. `func`).
      for (FunctionDecl callee_func : FunctionDecl::containing(call)) {
        if (callee_func != func.value()) {
          continue;
        }

        EntityInformation::Selection &sel = info.callees.emplace_back();
        sel.display_role.setValue(callee_func.token());
        sel.entity_role = call;
        sel.location = GetLocation(call.tokens(), file_location_cache);
        break;
      }
    }
  }

  return info;
}

//! Fill `info` with information about the macro `entity`.
static EntityInformation GetMacroInformation(
    const FileLocationCache &file_location_cache,
    const DefineMacroDirective &entity) {

  EntityInformation info;
  info.id = entity.id().Pack();
  info.entity.display_role.setValue(entity.name());
  info.entity.location = GetLocation(entity.use_tokens(), file_location_cache);
  info.entity.entity_role = entity;

  return info;
}

//! Fill `info` with information about the file `entity`.
static EntityInformation GetFileInformation(
    const FileLocationCache &file_location_cache,
    const File &entity) {

  EntityInformation info;
  info.id = entity.id().Pack();
  info.entity.entity_role = entity;

  for (std::filesystem::path path : entity.paths()) {
    info.entity.display_role.setValue(
        QString::fromStdString(path.generic_string()));
    break;
  }

  for (IncludeLikeMacroDirective inc : IncludeLikeMacroDirective::in(entity)) {
    if (std::optional<File> file = inc.included_file()) {
      EntityInformation::Selection &sel = info.includes.emplace_back();
      TokenRange tokens = inc.use_tokens();
      sel.entity_role = std::move(inc);
      sel.location = GetLocation(tokens, file_location_cache);
      sel.display_role.setValue(tokens.strip_whitespace());
    }
  }

  for (Reference ref : entity.references()) {
    if (auto inc = IncludeLikeMacroDirective::from(ref.as_macro())) {
      if (auto file = File::containing(inc.value())) {
        TokenRange tokens = inc->use_tokens();
        EntityInformation::Selection &sel = info.include_bys.emplace_back();
        sel.location = GetLocation(tokens, file_location_cache);
        sel.entity_role = inc.value();

        for (std::filesystem::path path : file->paths()) {
          sel.display_role.setValue(
              QString::fromStdString(path.generic_string()));
          break;
        }
      }
    }
  }

  for (Fragment frag : entity.fragments()) {
    for (DefineMacroDirective def : DefineMacroDirective::in(frag)) {
      EntityInformation::Selection &sel = info.top_level_entities.emplace_back();
      sel.display_role.setValue(def.name());
      sel.entity_role = def;
      sel.location = GetLocation(def.use_tokens(), file_location_cache);
    }

    for (Decl decl : frag.top_level_declarations()) {
      if (auto nd = NamedDecl::from(decl); nd && !nd->name().empty()) {
        EntityInformation::Selection &sel =
            info.top_level_entities.emplace_back();
        sel.display_role.setValue(nd->token());
        sel.location = GetLocation(nd->tokens(), file_location_cache);
        sel.entity_role = nd.value();
      }
    }
  }

  return info;
}

}  // namespace

void GetEntityInformation(
    QPromise<IDatabase::EntityInformationResult> &result_promise,
    const Index &index, const FileLocationCache &file_location_cache,
    RawEntityId entity_id) {

  VariantEntity entity = index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    result_promise.addResult(RPCErrorCode::InvalidEntityID);
    return;
  }

  std::optional<EntityInformation> info;

  if (std::holds_alternative<Token>(entity)) {
    entity = std::get<Token>(entity).related_entity();
  }

  if (std::holds_alternative<Decl>(entity)) {
    if (auto named = NamedDecl::from(std::get<Decl>(entity))) {
      info.emplace(GetDeclInformation(file_location_cache,
                                      named->canonical_declaration()));
    }

  } else if (std::holds_alternative<Macro>(entity)) {
    if (auto def = DefineMacroDirective::from(std::get<Macro>(entity))) {
      info.emplace(GetMacroInformation(file_location_cache,
                                       std::move(def.value())));
    }

  } else if (std::holds_alternative<File>(entity)) {
    info.emplace(GetFileInformation(file_location_cache,
                                    std::move(std::get<File>(entity))));
  }

  if (!info.has_value()) {
    result_promise.addResult(RPCErrorCode::InvalidInformationRequestType);
    return;
  }

  info->requested_id = entity_id;
  result_promise.addResult(std::move(info.value()));
}

}  // namespace mx::gui
