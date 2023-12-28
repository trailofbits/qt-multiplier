/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeModel.h"

#include <multiplier/Frontend/MacroKind.h>
#include <multiplier/GUI/ICodeModel.h>

#include <QModelIndex>
#include <QVariant>

namespace mx::gui {

ICodeModel *ICodeModel::Create(const FileLocationCache &file_location_cache,
                               const Index &index,
                               const bool &remap_related_entity_id_role,
                               QObject *parent) {
  return new CodeModel(file_location_cache, index, remap_related_entity_id_role,
                       parent);
}

std::optional<std::pair<RawEntityId, RawEntityId>>
ICodeModel::MacroExpansionPoint(const QModelIndex &index) {
  QVariant related_entity_id_var = index.data(RealRelatedEntityIdRole);

  if (!related_entity_id_var.isValid()) {
    return std::nullopt;
  }

  RawEntityId macro_eid = qvariant_cast<RawEntityId>(related_entity_id_var);
  VariantId macro_vid = mx::EntityId(macro_eid).Unpack();
  if (!std::holds_alternative<MacroId>(macro_vid)) {
    return std::nullopt;
  }

  MacroId mid = std::get<MacroId>(macro_vid);
  if (mid.kind != MacroKind::DEFINE_DIRECTIVE &&
      mid.kind != MacroKind::SUBSTITUTION) {
    return std::nullopt;
  }

  QVariant token_id_var = index.data(ICodeModel::TokenIdRole);
  if (!token_id_var.isValid()) {
    return std::nullopt;
  }

  RawEntityId token_eid = qvariant_cast<RawEntityId>(token_id_var);
  VariantId token_vid = mx::EntityId(token_eid).Unpack();
  if (!std::holds_alternative<MacroTokenId>(token_vid)) {
    return std::nullopt;
  }

  return std::pair<RawEntityId, RawEntityId>(macro_eid, token_eid);
}

//! Tells this code view to use the `TokenTreeVisitor` to expand some
//! macros.
void ICodeModel::OnExpandMacros(const TokenTreeVisitor *) {}

}  // namespace mx::gui
