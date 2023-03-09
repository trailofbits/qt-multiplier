// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Entity.h>
#include <optional>
#include <QString>
#include <unordered_map>
#include <utility>

namespace mx {
class Decl;
class File;
class Index;
class Token;
namespace gui {

//! Return the entity ID associated with `ent`.
RawEntityId IdOfEntity(const VariantEntity &ent);

//! Return the file containing an entity.
std::optional<File> FileOfEntity(const VariantEntity &ent);

//! Get the first file token associated with an entity.
//!
//! NOTE(pag): We prefer `TokenRange::file_tokens` as that walks up macros.
Token FirstFileToken(const VariantEntity &ent);

//! Return the name of an entity.
std::optional<QString> NameOfEntity(const VariantEntity &ent);

//// Try to determine the declarations associated with this token.
//std::optional<Decl> DeclForToken(const Token &token);
//
//using EntityBaseOffsetPair = std::pair<RawEntityId, uint32_t>;
//
//// Returns a pair of `(fragment_id, offset)` or `(kInvalidEntityId, 0)` for a
//// given raw entity id.
//EntityBaseOffsetPair GetFragmentOffset(RawEntityId id);
//
//// Returns a pair of `(file_id, offset)` or `(kInvalidEntityId, 0)` for a
//// given raw entity id.
//EntityBaseOffsetPair GetFileOffset(RawEntityId id);
//
//// Return the "canonical" ID of a declaration. This tries to get us the
//// definition when possible.
//PackedDeclId CanonicalId(const Decl &decl);
//
//// Return the "canonical" version of a declaration. This tries to get us the
//// definition when possible.
//Decl CanonicalDecl(const Decl &decl);
//
//// Return some kind of name for a declaration.
//QString DeclName(const Decl &decl);
//
//// Return the file location of an entity.
//RawEntityId EntityFileLocation(const Index &index, RawEntityId eid);
//
//// Return the optional nearest fragment token associated with this declaration.
//std::optional<Token> DeclFragmentToken(const Decl &decl);
//
//// Return the optional nearest file token associated with this declaration.
//std::optional<Token> DeclFileToken(const Decl &decl);
//
//// Return the entity ID of the nearest file token associated with this
//// declaration.
//RawEntityId DeclFileLocation(const Decl &decl);
//
//// Try to get the nearest declaration for `id`. Ideally, `id` is a declaration
//// ID. Otherwise, it will find the nearest enclosing declaration, and return
//// that.
//std::optional<Decl> NearestDeclFor(const Index &index, RawEntityId id);
//
//// Create a breadcrumbs string of the token contexts.
//QString TokenBreadCrumbs(const Token &ent, bool run_length_encode = true);

}  // namespace gui
}  // namespace mx
