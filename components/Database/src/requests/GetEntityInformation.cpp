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
#include <multiplier/Entities/NamedDecl.h>
#include <multiplier/ui/Util.h>

namespace mx::gui {
namespace {

static void FillSelectionLocation(
    EntityInformation::Selection &sel,
    const FileLocationCache &file_location_cache) {
  if (!sel.tokens) {
    return;
  }
  Token tok = sel.tokens.file_tokens().front();
  if (!tok) {
    return;
  }

  auto file = File::containing(tok);
  if (!file) {
    return;
  }

  if (auto line_col = tok.location(file_location_cache)) {
    sel.location.emplace(file.value(), line_col->first, line_col->second);
  }
}

template <typename EntType>
static EntityInformation::Selection CreateSelection(
    const FileLocationCache &file_location_cache, const EntType &entity) {

  EntityInformation::Selection sel;
  sel.entity = entity;
  sel.tokens = FileTokens(entity);
  FillSelectionLocation(sel, file_location_cache);

  return sel;
}

// Fill in the macros used within a given token range.
static void FillUsedMacros(const FileLocationCache &file_location_cache,
                           EntityInformation &info, const TokenRange &tokens) {
  std::vector<PackedMacroId> seen;

  for (Token tok : tokens) {
    for (Macro macro : Macro::containing(tok)) {
      if (macro.kind() != MacroKind::EXPANSION) {
        continue;
      }

      PackedMacroId macro_id = macro.id();
      if (std::find(seen.begin(), seen.end(), macro_id) != seen.end()) {
        continue;
      }

      seen.push_back(macro_id);
      auto def = MacroExpansion::from(macro)->definition();
      if (!def) {
        continue;
      }

      EntityInformation::Selection &sel = info.macros_used.emplace_back();
      sel.entity = std::move(def.value());
      sel.tokens = macro.use_tokens();

      FillSelectionLocation(sel, file_location_cache);
    }
  }
}

//! Fill `info` with information about the declaration `entity`.
static EntityInformation GetDeclInformation(
    const FileLocationCache &file_location_cache, NamedDecl entity) {

  EntityInformation info;
  info.entity = CreateSelection(file_location_cache, entity);
  info.id = entity.id().Pack();

  std::string_view name = entity.name();
  info.name = QString::fromUtf8(name.data(),
                                static_cast<qsizetype>(name.size()));

  // Fill all redeclarations.
  for (NamedDecl redecl : entity.redeclarations()) {

    // Collect all macros used by all redeclarations.
    FillUsedMacros(file_location_cache, info, redecl.tokens());

    if (redecl != entity) {
      info.redeclarations.emplace_back(
          CreateSelection<NamedDecl>(file_location_cache, redecl));

      // Try to get a name from elsewhere if we're missing it.
      if (info.name.isEmpty()) {
        name = redecl.name();
        info.name = QString::fromUtf8(name.data(),
                                      static_cast<qsizetype>(name.size()));
      }
    }
  }

  // If this is a function, then look at who it calls, and who calls it.
  if (auto func = FunctionDecl::from(entity)) {
    for (CallExpr call : func->callers()) {
      for (FunctionDecl caller_func : FunctionDecl::containing(call)) {
        auto &sel = info.callers.emplace_back(
            CreateSelection(file_location_cache, call));
        sel.entity = std::move(caller_func);
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

      for (FunctionDecl caller_of_call : FunctionDecl::containing(call)) {
        if (caller_of_call == func.value()) {
          auto &sel = info.callees.emplace_back(
              CreateSelection(file_location_cache, call));
          sel.entity = std::move(callee.value());
          break;
        }
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
  info.entity = CreateSelection(file_location_cache, entity);
  info.id = entity.id().Pack();

  std::string_view name = entity.name().data();
  info.name = QString::fromUtf8(name.data(),
                                static_cast<qsizetype>(name.size()));

  return info;
}

//! Fill `info` with information about the file `entity`.
static EntityInformation GetFileInformation(
    const FileLocationCache &file_location_cache,
    const File &entity) {

  EntityInformation info;
  info.entity = CreateSelection(file_location_cache, entity);
  info.id = entity.id().Pack();

  for (std::filesystem::path path : entity.paths()) {
    info.name = QString::fromStdString(path.generic_string());
    break;
  }

  for (IncludeLikeMacroDirective inc : IncludeLikeMacroDirective::in(entity)) {
    info.includes.emplace_back(CreateSelection(file_location_cache, inc));
  }

  for (Reference ref : entity.references()) {
    if (auto inc = IncludeLikeMacroDirective::from(ref.as_macro())) {
      info.include_bys.emplace_back(CreateSelection(file_location_cache,
                                                    inc.value()));
    }
  }

  for (Fragment frag : entity.fragments()) {
    for (DefineMacroDirective def : DefineMacroDirective::in(frag)) {
      info.top_level_entities.emplace_back(CreateSelection(file_location_cache,
                                                           def));
    }
    for (Decl decl : frag.top_level_declarations()) {
      if (auto nd = NamedDecl::from(decl); nd && !nd->name().empty()) {
        info.top_level_entities.emplace_back(CreateSelection(file_location_cache,
                                                             nd.value()));
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
