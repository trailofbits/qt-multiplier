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

#include <multiplier/AST/Attr.h>
#include <multiplier/AST/CXXBaseSpecifier.h>
#include <multiplier/AST/Designator.h>
#include <multiplier/AST/FieldDecl.h>
#include <multiplier/AST/FunctionDecl.h>
#include <multiplier/AST/Stmt.h>
#include <multiplier/AST/TemplateArgument.h>
#include <multiplier/AST/TemplateParameterList.h>
#include <multiplier/AST/Type.h>
#include <multiplier/AST/TypeDecl.h>
#include <multiplier/AST/VarDecl.h>
#include <multiplier/Fragment.h>
#include <multiplier/Frontend/Compilation.h>
#include <multiplier/Frontend/DefineMacroDirective.h>
#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/IncludeLikeMacroDirective.h>
#include <multiplier/IR/Operation.h>

#include <QColor>
#include <QModelIndex>
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

VariantEntity NamedEntityContaining(const VariantEntity &entity);

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

//! Create a new token range, derived from `toks`, that introduces/injects
//! fake whitespace between `toks`. This is nifty when you want to render out
//! a parsed token range for human consumption.
TokenRange InjectWhitespace(const TokenRange &toks);

//! Return the name of an entity.
TokenRange NameOfEntity(const VariantEntity &ent, bool qualified=true,
                        bool scan_redecls=true);

//! Return the name of an entity as a `QString`.
std::optional<QString> NameOfEntityAsString(const VariantEntity &ent,
                                            bool qualified=true);

QString LocationOfEntity(const FileLocationCache &file_location_cache,
                         const VariantEntity &entity);

//! Return the tokens of `ent` as a string.
QString TokensToString(const VariantEntity &ent);

//! Create a breadcrumbs string of the token contexts.
QString TokenBreadCrumbs(const Token &ent, bool run_length_encode = true);

//! Create a breadcrumbs string of the token contexts.
QString EntityBreadCrumbs(const VariantEntity &ent,
                          bool run_length_encode = true);

// Convert `variant` to a string.
std::optional<QString> TryConvertToString(const QVariant &variant);

}  // namespace gui
}  // namespace mx
