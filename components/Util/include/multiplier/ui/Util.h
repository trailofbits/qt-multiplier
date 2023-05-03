// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

// The Entity.h is missing this header when building on linux

// clang-format off
#include <memory>
#include <multiplier/Entity.h>
// clang-format on

#include <multiplier/Entities/Attr.h>
#include <multiplier/Entities/CXXBaseSpecifier.h>
#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/Designator.h>
#include <multiplier/Entities/FieldDecl.h>
#include <multiplier/Entities/File.h>
#include <multiplier/Entities/Fragment.h>
#include <multiplier/Entities/FunctionDecl.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>
#include <multiplier/Entities/Stmt.h>
#include <multiplier/Entities/TemplateArgument.h>
#include <multiplier/Entities/TemplateParameterList.h>
#include <multiplier/Entities/Type.h>
#include <multiplier/Entities/TypeDecl.h>
#include <multiplier/Entities/VarDecl.h>

#include <QString>

#include <optional>
#include <unordered_map>
#include <utility>

namespace mx {
class Decl;
class File;
class Index;
class Token;
class TokenRange;
namespace gui {

template <typename T>
static VariantEntity NamedDeclContaining(const T &thing) requires(
    !std::is_same_v<T, VariantEntity>) {
  for (FunctionDecl func : FunctionDecl::containing(thing)) {
    return func;
  }

  for (FieldDecl field : FieldDecl::containing(thing)) {
    return field;
  }

  for (VarDecl var : VarDecl::containing(thing)) {
    if (var.is_local_variable_declaration_or_parm()) {
      if (VariantEntity ent = NamedDeclContaining<Decl>(var);
          !std::holds_alternative<NotAnEntity>(ent)) {
        return ent;
      }

    } else {
      return var;
    }
  }

  for (NamedDecl nd : NamedDecl::containing(thing)) {
    return nd;
  }

  return NotAnEntity{};
}

//! Return the named declaration containing `thing`, or `NotAnEntity`.
VariantEntity NamedDeclContaining(const VariantEntity &thing);

//! Return the entity ID associated with `ent`.
RawEntityId IdOfEntity(const VariantEntity &ent);

//! Return the file containing an entity.
std::optional<File> FileOfEntity(const VariantEntity &ent);

//! Get the token range associated with an entity.
TokenRange Tokens(const VariantEntity &ent);

//! Get the file token range associated with an entity.
TokenRange FileTokens(const VariantEntity &ent);

//! Get the first file token associated with an entity.
Token FirstFileToken(const VariantEntity &ent);

//! Return the name of an entity.
std::optional<QString> NameOfEntity(const VariantEntity &ent);

//! Return the tokens of `ent` as a string.
QString TokensToString(const VariantEntity &ent);

//! Create a breadcrumbs string of the token contexts.
QString TokenBreadCrumbs(const Token &ent, bool run_length_encode = true);

//! Create a breadcrumbs string of the token contexts.
std::optional<QString> EntityBreadCrumbs(const VariantEntity &ent,
                                         bool run_length_encode = true);

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

}  // namespace gui
}  // namespace mx
