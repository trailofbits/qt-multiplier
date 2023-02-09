// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "Util.h"

#include <cassert>
#include <multiplier/AST.h>
#include <multiplier/Fragment.h>
#include <multiplier/Index.h>
#include <multiplier/Iterator.h>
#include <multiplier/Token.h>

#include <iostream>

namespace mx::gui {

// Returns a pair of `(fragment_id, offset)` or `(kInvalidEntityId, 0)` for a
// given raw entity id.
EntityBaseOffsetPair GetFragmentOffset(RawEntityId id) {
  VariantId vid = EntityId(id).Unpack();
  if (std::holds_alternative<DeclId>(vid)) {
    auto eid = std::get<DeclId>(vid);
    return {EntityId(FragmentId(eid.fragment_id)).Pack(), eid.offset};

  } else if (std::holds_alternative<StmtId>(vid)) {
    auto eid = std::get<StmtId>(vid);
    return {EntityId(FragmentId(eid.fragment_id)).Pack(), eid.offset};

  } else if (std::holds_alternative<TypeId>(vid)) {
    auto eid = std::get<TypeId>(vid);
    return {EntityId(FragmentId(eid.fragment_id)).Pack(), eid.offset};

  } else if (std::holds_alternative<ParsedTokenId>(vid)) {
    auto eid = std::get<ParsedTokenId>(vid);
    return {EntityId(FragmentId(eid.fragment_id)).Pack(), eid.offset};

  } else if (std::holds_alternative<MacroId>(vid)) {
    auto eid = std::get<MacroId>(vid);
    return {EntityId(FragmentId(eid.fragment_id)).Pack(), eid.offset};

  } else if (std::holds_alternative<AttrId>(vid)) {
    auto eid = std::get<AttrId>(vid);
    return {EntityId(FragmentId(eid.fragment_id)).Pack(), eid.offset};

  } else if (std::holds_alternative<ParsedTokenId>(vid)) {
    auto eid = std::get<ParsedTokenId>(vid);
    return {EntityId(FragmentId(eid.fragment_id)).Pack(), eid.offset};

  } else if (std::holds_alternative<MacroTokenId>(vid)) {
    auto eid = std::get<MacroTokenId>(vid);
    return {EntityId(FragmentId(eid.fragment_id)).Pack(), eid.offset};

  } else {
    return {kInvalidEntityId, 0u};
  }
}

// Returns a pair of `(file_id, offset)` or `(kInvalidEntityId, 0)` for a
// given raw entity id.
EntityBaseOffsetPair GetFileOffset(RawEntityId id) {
  VariantId vid = EntityId(id).Unpack();
  if (std::holds_alternative<FileTokenId>(vid)) {
    auto eid = std::get<FileTokenId>(vid);
    return {EntityId(FileId(eid.file_id)).Pack(), eid.offset};

  } else {
    return {kInvalidEntityId, 0u};
  }
}

// Return the "canonical" version of a declaration. This tries to get us the
// definition when possible.
Decl CanonicalDecl(const Decl &decl) {
  for (const Decl &redecl : decl.redeclarations()) {
    return redecl;
  }
  return decl;
}

// Return the "canonical" ID of a declaration. This tries to get us the
// definition when possible.
PackedDeclId CanonicalId(const Decl &decl) {
  return CanonicalDecl(decl).id();
}

// Return some kind of name for a declaration.
QString DeclName(const Decl &decl) {
  if (auto nd = NamedDecl::from(decl)) {
    if (auto name_data = nd->name(); !name_data.empty()) {
      return QString::fromUtf8(name_data.data(),
                               static_cast<int>(name_data.size()));
    }
  }
  return QString("%1(%2)")
      .arg(EnumeratorName(decl.category()))
      .arg(decl.id().Pack());
}

// Return the file location of an entity.
RawEntityId EntityFileLocation(const Index &index, RawEntityId eid) {
  VariantEntity entity = index.entity(eid);
  if (std::holds_alternative<Decl>(entity)) {
    return DeclFileLocation(std::get<Decl>(entity));

    // Statement, walk to the first fragent token, or the beginning of the
    // fragment.
  }
  if (std::holds_alternative<Stmt>(entity)) {
    Stmt stmt = std::get<Stmt>(entity);
    for (Token token : stmt.tokens()) {
      if (auto nearest_file_loc = token.nearest_file_token()) {
        return nearest_file_loc.id().Pack();
      }
    }

    if (auto file_toks = Fragment::containing(stmt).file_tokens()) {
      return file_toks.begin()->id().Pack();
    }

    // Type; walk to the containing fragment.
  } else if (std::holds_alternative<Type>(entity)) {
    Type type = std::get<Type>(entity);
    if (auto file_toks = Fragment::containing(type).file_tokens()) {
      return file_toks.begin()->id().Pack();
    }

    // Token substitution; walk up to the file location.
  } else if (std::holds_alternative<Macro>(entity)) {
    Macro macro = std::get<Macro>(entity);
    for (auto parent = macro.parent(); parent;
         macro = std::move(parent.value())) {
    }

    for (auto made_progress = true; made_progress;) {
      made_progress = false;
      for (MacroOrToken node : macro.children()) {
        if (std::holds_alternative<Macro>(node)) {
          macro = std::move(std::get<Macro>(node));
          made_progress = true;
          break;
        } else if (std::holds_alternative<Token>(node)) {
          if (auto file_tok = std::get<Token>(node).nearest_file_token()) {
            return file_tok.id().Pack();
          }
        }
      }
    }

  } else if (std::holds_alternative<Token>(entity)) {
    if (auto file_tok = std::get<Token>(entity).nearest_file_token()) {
      return file_tok.id().Pack();
    }
  }
  return kInvalidEntityId;
}

// Return the optional nearest fragment token associated with this declaration.
std::optional<Token> DeclFragmentToken(const Decl &decl) {

  // Structs and enums and such can often be defined inside of a typedef so we
  // want to go to the beginning of them.
  if (TypeDecl::from(decl)) {
    goto skip_name_match;
  }

  if (auto nd = NamedDecl::from(decl)) {
    if (auto tok = nd->token()) {
      if (tok.data() == nd->name()) {
        return tok;
      }
    }
  }

skip_name_match:
  for (Token decl_tok : decl.tokens()) {
    return decl_tok;
  }

  for (Token parsed_tok : Fragment::containing(decl).parsed_tokens()) {
    return parsed_tok;
  }

  return std::nullopt;
}

// Return the optional nearest file token associated with this declaration.
std::optional<Token> DeclFileToken(const Decl &decl) {
  if (auto frag_tok = DeclFragmentToken(decl)) {
    return frag_tok->nearest_file_token();
  } else {
    return std::nullopt;
  }
}

// Return the entity ID of the nearest file token associated with this
// declaration.
RawEntityId DeclFileLocation(const Decl &decl) {
  if (auto tok = DeclFileToken(decl)) {
    return tok->id().Pack();
  } else {
    return kInvalidEntityId;
  }
}

// Try to get the nearest declaration for `id`. Ideally, `id` is a declaration
// ID. Otherwise, it will find the nearest enclosing declaration, and return
// that.
std::optional<Decl> NearestDeclFor(const Index &index, RawEntityId id) {
  auto entity = index.entity(id);
  if (std::holds_alternative<Decl>(entity)) {
    return std::get<Decl>(entity);
  } else if (std::holds_alternative<Stmt>(entity)) {
    for (Decl decl : Decl::containing(std::get<Stmt>(entity))) {
      return decl;
    }
  } else if (std::holds_alternative<Token>(entity)) {
    for (Decl decl : Decl::containing(std::get<Token>(entity))) {
      return decl;
    }
  }
  return std::nullopt;
}

namespace {

static const QString kBreadCrumb("%1%2%3");
static const QString kReps[] = {"", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
static const QString kTooManyReps("⁺");
static const QString kNextSep(" → ");

struct BreadCrumbs {

  QString sep;
  QString breadcrumbs;

  int repetitions{0};
  const bool run_length_encode;

  BreadCrumbs(bool run_length_encode_)
      : run_length_encode(run_length_encode_) {}

  QString Release(void) {
    repetitions = 0;
    return std::move(breadcrumbs);
  }

  template <typename T>
  void AddEnum(T enumerator) {
    std::string_view ret = EnumeratorName(enumerator);
    if (ret.ends_with("_EXPR") || ret.ends_with("_STMT") ||
        ret.ends_with("_DECL") || ret.ends_with("_TYPE")) {
      AddStringView(ret.substr(0u, ret.size() - 5u));
    } else if (ret.ends_with("_OPERATOR")) {
      AddStringView(ret.substr(0u, ret.size() - 9u));
    } else if (ret.ends_with("_DIRECTIVE")) {
      AddStringView(ret.substr(0u, ret.size() - 10u));
    } else {
      AddStringView(ret);
    }
  }

  void AddStringView(std::string_view ret) {
    AddString(QString::fromUtf8(ret.data(), static_cast<int>(ret.size())));
  }

  void AddString(QString name) {
    if (run_length_encode && breadcrumbs.endsWith(name)) {
      ++repetitions;

    } else if (repetitions < 9) {
      breadcrumbs.append(
          kBreadCrumb.arg(kReps[repetitions]).arg(sep).arg(name));
      repetitions = 0;

    } else {
      breadcrumbs.append(kBreadCrumb.arg(kTooManyReps).arg(sep).arg(name));
      repetitions = 0;
    }

    sep = kNextSep;
  }
};

}  // namespace

// Create a breadcrumbs string of the token contexts.
QString TokenBreadCrumbs(const Token &ent, bool run_length_encode) {
  auto i = -1;

  BreadCrumbs crumbs(run_length_encode);

  for (auto context = ent.context(); context; context = context->parent()) {
    ++i;

    if (auto cdecl = context->as_declaration()) {
      crumbs.AddEnum(cdecl->kind());

    } else if (auto ctype = context->as_type()) {
      crumbs.AddEnum(ctype->kind());

    } else if (auto cstmt = context->as_statement()) {
      switch (cstmt->kind()) {
        case StmtKind::DECL_REF_EXPR:
        case StmtKind::COMPOUND_STMT:
        case StmtKind::PAREN_EXPR: break;
        case StmtKind::UNARY_EXPR_OR_TYPE_TRAIT_EXPR: {
          auto &expr =
              reinterpret_cast<const UnaryExprOrTypeTraitExpr &>(cstmt.value());
          crumbs.AddEnum(expr.expression_or_trait_kind());
          continue;
        }

        case StmtKind::IMPLICIT_CAST_EXPR: {
          auto &cast =
              reinterpret_cast<const ImplicitCastExpr &>(cstmt.value());
          auto ck = cast.cast_kind();
          if (ck != CastKind::L_VALUE_TO_R_VALUE && ck != CastKind::BIT_CAST &&
              ck != CastKind::FUNCTION_TO_POINTER_DECAY &&
              ck != CastKind::ARRAY_TO_POINTER_DECAY) {
            crumbs.AddEnum(ck);
          }
          break;
        }

        case StmtKind::UNARY_OPERATOR: {
          auto &op = reinterpret_cast<const UnaryOperator &>(cstmt.value());
          crumbs.AddEnum(op.opcode());
          continue;
        }

        case StmtKind::BINARY_OPERATOR: {
          auto &op = reinterpret_cast<const BinaryOperator &>(cstmt.value());
          crumbs.AddEnum(op.opcode());
          continue;
        }

        case StmtKind::MEMBER_EXPR:
          // If we're asking for the use of a field, then every use will start
          // with `MEMBER_EXPR`.
          if (!i) {
            continue;
          }
          [[clang::fallthrough]];

        default: crumbs.AddEnum(cstmt->kind()); break;
      }
    }
  }
  return crumbs.Release();
}

}  // namespace mx::gui
