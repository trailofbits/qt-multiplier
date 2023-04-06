/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetEntityInformation.h"

#include <filesystem>
#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/File.h>
#include <multiplier/Entities/NamedDecl.h>
#include <multiplier/ui/Util.h>

namespace mx::gui {
namespace {

template <typename EntType>
static EntityInformation::Selection CreateSelection(
    const FileLocationCache &file_location_cache, const EntType &entity) {

  EntityInformation::Selection sel;
  sel.entity = entity;
  sel.tokens = FileTokens(entity);

  if (sel.tokens) {
    if (Token tok = sel.tokens.front(); auto file = File::containing(tok)) {
      if (auto line_col = tok.location(file_location_cache)) {
        sel.location.emplace(file.value(), line_col->first, line_col->second);
      }
    }
  }

  return sel;
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

  for (NamedDecl redecl : entity.redeclarations()) {
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
