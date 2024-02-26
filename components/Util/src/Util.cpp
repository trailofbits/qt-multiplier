// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "Util.h"

#include <cassert>
#include <iostream>

#include <QDebug>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>

#include <multiplier/GUI/Interfaces/IModel.h>

#include <multiplier/AST.h>
#include <multiplier/Fragment.h>
#include <multiplier/Frontend.h>
#include <multiplier/Index.h>
#include <multiplier/Iterator.h>
#include <multiplier/IR/Block.h>

namespace mx::gui {

namespace {

static const QString kGeneratedCopyMenuSignature{"GeneratedCopyMenu"};

}

void GenerateCopySubMenu(QMenu *menu, const QModelIndex &index) {
  // BUG: As the context menu is not always handled in the same
  // level in the component hierarchy, we can end up with a
  // duplicated menu
  //
  // An example of this is the InformationExplorer, which will
  // eventually populate the context menu of external widgets
  // by mistake, due to how plugins are handled
  //
  // Submenus are typically really small: 4-5 entries, with at most
  // a single submenu (typically, a Copy menu we already generated).
  //
  // This still provides a complete scan for the sake of correctness
  auto next_action_queue{menu->actions()};

  while (!next_action_queue.empty()) {
    auto action_queue = std::move(next_action_queue);
    next_action_queue.clear();

    for (const auto &action : action_queue) {
      if (action->isSeparator()) {
        continue;

      } else if (action->menu() != nullptr) {
        for (const auto &sub_action : action->menu()->actions()) {
          next_action_queue.push_back(sub_action);
        }

      } else if (action->data().toString() == kGeneratedCopyMenuSignature) {
        return;
      }
    }
  }

  auto copyable_role_map_var = index.data(IModel::CopyableRoleMapIdRole);

  if (!copyable_role_map_var.isValid() ||
      !copyable_role_map_var.canConvert<CopyableRoleMap>()) {

    return;
  }

  auto copyable_role_map = qvariant_cast<CopyableRoleMap>(copyable_role_map_var);

  std::vector<QAction *> action_list;
  for (auto [key, value] : copyable_role_map.asKeyValueRange()) {
    // Make a copy for the lambda
    auto role_name = key;
    auto role_id = value;

    auto action = new QAction(role_name);
    action->setData(kGeneratedCopyMenuSignature);
    QObject::connect(action, &QAction::triggered,
            [=](void) {
              QString text;

              auto var = index.data(role_id);
              if (var.canConvert<QString>()) {
                text = var.toString();
              } else if (var.canConvert<unsigned long long int>()) {
                text = QString::number(var.toULongLong());
              }

              if (text.isEmpty()) {
                QMessageBox::critical(menu, QObject::tr("Copy error"),
                                      QObject::tr("The selected property is empty!"));

              } else {
                qApp->clipboard()->setText(text);
              }
            });

    action_list.push_back(action);
  }

  if (action_list.size() == 1) {
    auto action = action_list[0];
    action->setParent(menu);

    action->setText(QObject::tr("Copy") + "`" + action->text() + "`");
    menu->addAction(action);

  } else if (action_list.size() > 1) {
    auto copy_menu = new QMenu(QObject::tr("Copy..."), menu);
    for (auto action : action_list) {
      action->setParent(copy_menu);
      copy_menu->addAction(action);
    }

    menu->addMenu(copy_menu);
  }
}

VariantEntity NamedEntityContaining(const VariantEntity &entity) {
  if (std::holds_alternative<Decl>(entity)) {

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

    // // TODO(pag): Do token-based lookup? It might end up being a statement
    // //            in an attribute on a decl.
    // qDebug() << "Statement" << stmt.id().Pack() << "doesn't have a named containing decl";
    // for (auto ancestor = stmt.parent_declaration(); ancestor.has_value();
    //      ancestor = ancestor->parent_declaration()) {
    //   qDebug() << "Ancestor id:" << ancestor->id().Pack();
    // }

    // for (auto ancestor = stmt.parent_statement(); ancestor.has_value();
    //      ancestor = ancestor->parent_statement()) {
    //   qDebug() << "Ancestor statement id:" << ancestor->id().Pack();
    //   if (auto gp = ancestor->parent_declaration()) {
    //     qDebug() << "Gradnparent declaration id:" << gp->id().Pack();
    //   }
    // }

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
    auto macro = std::get<Macro>(entity).root();

    for (Token tok : macro.generate_expansion_tokens()) {
      if (Token ptok = tok.parsed_token()) {
        auto res = NamedDeclContaining(ptok);
        if (!std::holds_alternative<NotAnEntity>(res)) {
          return res;
        }
      }
    }

    // TODO(pag): Handle the case where a macro expands to nothing.

    // If the macro wasn't used inside of a decl/statement, then go try to
    // find the macro definition containing this macro.
    if (auto dd = DefineMacroDirective::from(std::get<Macro>(entity))) {
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
      if (auto ne = NamedEntityContaining(std::move(m));
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

//! Return the optional nearest fragment token associated with this declaration.
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

//! Return the optional nearest file token associated with this declaration.
Token DeclFileToken(const Decl &decl) {
  if (auto frag_tok = DeclFragmentToken(decl)) {
    return TokenRange(frag_tok.value()).file_tokens().front();
  } else {
    return Token();
  }
}

//! Get the token range associated with an entity.
TokenRange Tokens(const VariantEntity &ent) {
  const auto VariantEntityVisitor = Overload{
      [](const Decl &entity) { return entity.tokens(); },
      [](const Stmt &entity) { return entity.tokens(); },
      [](const Type &entity) { return entity.tokens(); },
      [](const Token &entity) { return TokenRange(entity); },
      [](const Macro &entity) { return entity.use_tokens(); },
      [](const Designator &entity) { return entity.tokens(); },
      [](const CXXBaseSpecifier &entity) { return entity.tokens(); },
      [](const TemplateArgument &) { return TokenRange(); },
      [](const TemplateParameterList &entity) { return entity.tokens(); },

      // NOTE(pag): We don't do `entity.parsed_tokens().file_tokens()` because
      //            if it's a pure macro fragment, then it might not have any
      //            parsed tokens.
      [](const Fragment &entity) { return entity.parsed_tokens(); },
      [](const File &entity) { return entity.tokens(); },
      [](auto) { return TokenRange(); }};
  return std::visit<TokenRange>(VariantEntityVisitor, ent);
}

//! Get the file token range associated with an entity.
TokenRange FileTokens(const VariantEntity &ent) {
  return Tokens(ent).file_tokens();
}

//! Get the first file token associated with an entity.
Token FirstFileToken(const VariantEntity &ent) {
  if (std::holds_alternative<Decl>(ent)) {
    if (auto ftok = DeclFileToken(std::get<Decl>(ent))) {
      return ftok;
    }
  }
  return FileTokens(ent).front();
}

namespace {

static bool IsKeyword(TokenKind tk) {
  switch (tk) {
    case TokenKind::KEYWORD_AUTO:
    case TokenKind::KEYWORD_BREAK:
    case TokenKind::KEYWORD_CASE:
    case TokenKind::KEYWORD_CHARACTER:
    case TokenKind::KEYWORD_CONST:
    case TokenKind::KEYWORD_CONTINUE:
    case TokenKind::KEYWORD_DEFAULT:
    case TokenKind::KEYWORD_DO:
    case TokenKind::KEYWORD_DOUBLE:
    case TokenKind::KEYWORD_ELSE:
    case TokenKind::KEYWORD_ENUM:
    case TokenKind::KEYWORD_EXTERN:
    case TokenKind::KEYWORD_FLOAT:
    case TokenKind::KEYWORD_FOR:
    case TokenKind::KEYWORD_GOTO:
    case TokenKind::KEYWORD_IF:
    case TokenKind::KEYWORD_INT:
    case TokenKind::KEYWORD__EXT_INT:
    case TokenKind::KEYWORD__BIT_INT:
    case TokenKind::KEYWORD_LONG:
    case TokenKind::KEYWORD_REGISTER:
    case TokenKind::KEYWORD_RETURN:
    case TokenKind::KEYWORD_SHORT:
    case TokenKind::KEYWORD_SIGNED:
    case TokenKind::KEYWORD_SIZEOF:
    case TokenKind::KEYWORD_STATIC:
    case TokenKind::KEYWORD_STRUCT:
    case TokenKind::KEYWORD_SWITCH:
    case TokenKind::KEYWORD_TYPEDEF:
    case TokenKind::KEYWORD_UNION:
    case TokenKind::KEYWORD_UNSIGNED:
    case TokenKind::KEYWORD_VOID:
    case TokenKind::KEYWORD_VOLATILE:
    case TokenKind::KEYWORD_WHILE:
    case TokenKind::KEYWORD__ALIGNAS:
    case TokenKind::KEYWORD__ALIGNOF:
    case TokenKind::KEYWORD__ATOMIC:
    case TokenKind::KEYWORD__BOOLEAN:
    case TokenKind::KEYWORD__COMPLEX:
    case TokenKind::KEYWORD__GENERIC:
    case TokenKind::KEYWORD__IMAGINARY:
    case TokenKind::KEYWORD__NORETURN:
    case TokenKind::KEYWORD__STATIC_ASSERT:
    case TokenKind::KEYWORD__THREAD_LOCAL:
    case TokenKind::KEYWORD___FUNC__:
    case TokenKind::KEYWORD___OBJC_YES:
    case TokenKind::KEYWORD___OBJC_NO:
    case TokenKind::KEYWORD_ASSEMBLY:
    case TokenKind::KEYWORD_BOOLEAN:
    case TokenKind::KEYWORD_CATCH:
    case TokenKind::KEYWORD_CLASS:
    case TokenKind::KEYWORD_CONST_CAST:
    case TokenKind::KEYWORD_DELETE:
    case TokenKind::KEYWORD_DYNAMIC_CAST:
    case TokenKind::KEYWORD_EXPLICIT:
    case TokenKind::KEYWORD_EXPORT:
    case TokenKind::KEYWORD_FALSE:
    case TokenKind::KEYWORD_FRIEND:
    case TokenKind::KEYWORD_MUTABLE:
    case TokenKind::KEYWORD_NAMESPACE:
    case TokenKind::KEYWORD_NEW:
    case TokenKind::KEYWORD_OPERATOR:
    case TokenKind::KEYWORD_PRIVATE:
    case TokenKind::KEYWORD_PROTECTED:
    case TokenKind::KEYWORD_PUBLIC:
    case TokenKind::KEYWORD_REINTERPRET_CAST:
    case TokenKind::KEYWORD_STATIC_CAST:
    case TokenKind::KEYWORD_TEMPLATE:
    case TokenKind::KEYWORD_THIS:
    case TokenKind::KEYWORD_THROW:
    case TokenKind::KEYWORD_TRUE:
    case TokenKind::KEYWORD_TRY:
    case TokenKind::KEYWORD_TYPENAME:
    case TokenKind::KEYWORD_TYPEID:
    case TokenKind::KEYWORD_USING:
    case TokenKind::KEYWORD_VIRTUAL:
    case TokenKind::KEYWORD_WCHAR_T:
    case TokenKind::KEYWORD_RESTRICT:
    case TokenKind::KEYWORD_INLINE:
    case TokenKind::KEYWORD_ALIGNAS:
    case TokenKind::KEYWORD_ALIGNOF:
    case TokenKind::KEYWORD_CHAR16_T:
    case TokenKind::KEYWORD_CHAR32_T:
    case TokenKind::KEYWORD_CONSTEXPR:
    case TokenKind::KEYWORD_DECLTYPE:
    case TokenKind::KEYWORD_NOEXCEPT:
    case TokenKind::KEYWORD_NULLPTR:
    case TokenKind::KEYWORD_STATIC_ASSERT:
    case TokenKind::KEYWORD_THREAD_LOCAL:
    case TokenKind::KEYWORD_CO_AWAIT:
    case TokenKind::KEYWORD_CO_RETURN:
    case TokenKind::KEYWORD_CO_YIELD:
    case TokenKind::KEYWORD_MODULE:
    case TokenKind::KEYWORD_IMPORT:
    case TokenKind::KEYWORD_CONSTEVAL:
    case TokenKind::KEYWORD_CONSTINIT:
    case TokenKind::KEYWORD_CONCEPT:
    case TokenKind::KEYWORD_REQUIRES:
    case TokenKind::KEYWORD_CHAR8_T:
    case TokenKind::KEYWORD__FLOAT16:
    case TokenKind::KEYWORD_TYPEOF:
    case TokenKind::KEYWORD_TYPEOF_UNQUALIFIED:
    case TokenKind::KEYWORD__ACCUM:
    case TokenKind::KEYWORD__FRACT:
    case TokenKind::KEYWORD__SAT:
    case TokenKind::KEYWORD__DECIMAL32:
    case TokenKind::KEYWORD__DECIMAL64:
    case TokenKind::KEYWORD__DECIMAL128:
    case TokenKind::KEYWORD___NULL:
    case TokenKind::KEYWORD___ALIGNOF:
    case TokenKind::KEYWORD___ATTRIBUTE:
    case TokenKind::KEYWORD___BUILTIN_CHOOSE_EXPRESSION:
    case TokenKind::KEYWORD___BUILTIN_OFFSETOF:
    case TokenKind::KEYWORD___BUILTIN_FILE:
    case TokenKind::KEYWORD___BUILTIN_FUNCTION:
    case TokenKind::KEYWORD___BUILTIN_LINE:
    case TokenKind::KEYWORD___BUILTIN_COLUMN:
    case TokenKind::KEYWORD___BUILTIN_SOURCE_TOKEN:
    case TokenKind::KEYWORD___BUILTIN_TYPES_COMPATIBLE_P:
    case TokenKind::KEYWORD___BUILTIN_VA_ARGUMENT:
    case TokenKind::KEYWORD___EXTENSION__:
    case TokenKind::KEYWORD___FLOAT128:
    case TokenKind::KEYWORD___IBM128:
    case TokenKind::KEYWORD___IMAG:
    case TokenKind::KEYWORD___INT128:
    case TokenKind::KEYWORD___LABEL__:
    case TokenKind::KEYWORD___REAL:
    case TokenKind::KEYWORD___THREAD:
    case TokenKind::KEYWORD___FUNCTION__:
    case TokenKind::KEYWORD___PRETTYFUNCTION__:
    case TokenKind::KEYWORD___AUTO_TYPE:
    case TokenKind::KEYWORD___FUNCDNAME__:
    case TokenKind::KEYWORD___FUNCSIG__:
    case TokenKind::KEYWORD_LFUNCTION__:
    case TokenKind::KEYWORD_LFUNCSIG__:
    case TokenKind::KEYWORD___IS_INTERFACE_CLASS:
    case TokenKind::KEYWORD___IS_SEALED:
    case TokenKind::KEYWORD___IS_DESTRUCTIBLE:
    case TokenKind::KEYWORD___IS_TRIVIALLY_DESTRUCTIBLE:
    case TokenKind::KEYWORD___IS_NOTHROW_DESTRUCTIBLE:
    case TokenKind::KEYWORD___IS_NOTHROW_ASSIGNABLE:
    case TokenKind::KEYWORD___IS_CONSTRUCTIBLE:
    case TokenKind::KEYWORD___IS_NOTHROW_CONSTRUCTIBLE:
    case TokenKind::KEYWORD___IS_ASSIGNABLE:
    case TokenKind::KEYWORD___HAS_NOTHROW_MOVE_ASSIGN:
    case TokenKind::KEYWORD___HAS_TRIVIAL_MOVE_ASSIGN:
    case TokenKind::KEYWORD___HAS_TRIVIAL_MOVE_CONSTRUCTOR:
    case TokenKind::KEYWORD___HAS_NOTHROW_ASSIGN:
    case TokenKind::KEYWORD___HAS_NOTHROW_COPY:
    case TokenKind::KEYWORD___HAS_NOTHROW_CONSTRUCTOR:
    case TokenKind::KEYWORD___HAS_TRIVIAL_ASSIGN:
    case TokenKind::KEYWORD___HAS_TRIVIAL_COPY:
    case TokenKind::KEYWORD___HAS_TRIVIAL_CONSTRUCTOR:
    case TokenKind::KEYWORD___HAS_TRIVIAL_DESTRUCTOR:
    case TokenKind::KEYWORD___HAS_VIRTUAL_DESTRUCTOR:
    case TokenKind::KEYWORD___IS_ABSTRACT:
    case TokenKind::KEYWORD___IS_AGGREGATE:
    case TokenKind::KEYWORD___IS_BASE_OF:
    case TokenKind::KEYWORD___IS_CLASS:
    case TokenKind::KEYWORD___IS_CONVERTIBLE_TO:
    case TokenKind::KEYWORD___IS_EMPTY:
    case TokenKind::KEYWORD___IS_ENUM:
    case TokenKind::KEYWORD___IS_FINAL:
    case TokenKind::KEYWORD___IS_LITERAL:
    case TokenKind::KEYWORD___IS_POD:
    case TokenKind::KEYWORD___IS_POLYMORPHIC:
    case TokenKind::KEYWORD___IS_STANDARD_LAYOUT:
    case TokenKind::KEYWORD___IS_TRIVIAL:
    case TokenKind::KEYWORD___IS_TRIVIALLY_ASSIGNABLE:
    case TokenKind::KEYWORD___IS_TRIVIALLY_CONSTRUCTIBLE:
    case TokenKind::KEYWORD___IS_TRIVIALLY_COPYABLE:
    case TokenKind::KEYWORD___IS_UNION:
    case TokenKind::KEYWORD___HAS_UNIQUE_OBJECT_REPRESENTATIONS:
    case TokenKind::KEYWORD___ADD_LVALUE_REFERENCE:
    case TokenKind::KEYWORD___ADD_POINTER:
    case TokenKind::KEYWORD___ADD_RVALUE_REFERENCE:
    case TokenKind::KEYWORD___DECAY:
    case TokenKind::KEYWORD___MAKE_SIGNED:
    case TokenKind::KEYWORD___MAKE_UNSIGNED:
    case TokenKind::KEYWORD___REMOVE_ALL_EXTENTS:
    case TokenKind::KEYWORD___REMOVE_CONST:
    case TokenKind::KEYWORD___REMOVE_CV:
    case TokenKind::KEYWORD___REMOVE_CVREF:
    case TokenKind::KEYWORD___REMOVE_EXTENT:
    case TokenKind::KEYWORD___REMOVE_POINTER:
    case TokenKind::KEYWORD___REMOVE_REFERENCE_T:
    case TokenKind::KEYWORD___REMOVE_RESTRICT:
    case TokenKind::KEYWORD___REMOVE_VOLATILE:
    case TokenKind::KEYWORD___UNDERLYING_TYPE:
    case TokenKind::KEYWORD___IS_TRIVIALLY_RELOCATABLE:
    case TokenKind::KEYWORD___IS_BOUNDED_ARRAY:
    case TokenKind::KEYWORD___IS_UNBOUNDED_ARRAY:
    case TokenKind::KEYWORD___IS_NULLPTR:
    case TokenKind::KEYWORD___IS_SCOPED_ENUM:
    case TokenKind::KEYWORD___IS_REFERENCEABLE:
    case TokenKind::KEYWORD___REFERENCE_BINDS_TO_TEMPORARY:
    case TokenKind::KEYWORD___IS_LVALUE_EXPRESSION:
    case TokenKind::KEYWORD___IS_RVALUE_EXPRESSION:
    case TokenKind::KEYWORD___IS_ARITHMETIC:
    case TokenKind::KEYWORD___IS_FLOATING_POINT:
    case TokenKind::KEYWORD___IS_INTEGRAL:
    case TokenKind::KEYWORD___IS_COMPLETE_TYPE:
    case TokenKind::KEYWORD___IS_VOID:
    case TokenKind::KEYWORD___IS_ARRAY:
    case TokenKind::KEYWORD___IS_FUNCTION:
    case TokenKind::KEYWORD___IS_REFERENCE:
    case TokenKind::KEYWORD___IS_LVALUE_REFERENCE:
    case TokenKind::KEYWORD___IS_RVALUE_REFERENCE:
    case TokenKind::KEYWORD___IS_FUNDAMENTAL:
    case TokenKind::KEYWORD___IS_OBJECT:
    case TokenKind::KEYWORD___IS_SCALAR:
    case TokenKind::KEYWORD___IS_COMPOUND:
    case TokenKind::KEYWORD___IS_POINTER:
    case TokenKind::KEYWORD___IS_MEMBER_OBJECT_POINTER:
    case TokenKind::KEYWORD___IS_MEMBER_FUNCTION_POINTER:
    case TokenKind::KEYWORD___IS_MEMBER_POINTER:
    case TokenKind::KEYWORD___IS_CONST:
    case TokenKind::KEYWORD___IS_VOLATILE:
    case TokenKind::KEYWORD___IS_SIGNED:
    case TokenKind::KEYWORD___IS_UNSIGNED:
    case TokenKind::KEYWORD___IS_SAME:
    case TokenKind::KEYWORD___IS_CONVERTIBLE:
    case TokenKind::KEYWORD___ARRAY_RANK:
    case TokenKind::KEYWORD___ARRAY_EXTENT:
    case TokenKind::KEYWORD___PRIVATE_EXTERN__:
    case TokenKind::KEYWORD___MODULE_PRIVATE__:
    case TokenKind::KEYWORD___BUILTIN_PTRAUTH_TYPE_DISCRIMINATOR:
    case TokenKind::KEYWORD___BUILTIN_XNU_TYPE_SIGNATURE:
    case TokenKind::KEYWORD___BUILTIN_XNU_TYPE_SUMMARY:
    case TokenKind::KEYWORD___BUILTIN_TMO_TYPE_METADATA:
    case TokenKind::KEYWORD___BUILTIN_XNU_TYPES_COMPATIBLE:
    case TokenKind::KEYWORD___DECLSPEC:
    case TokenKind::KEYWORD___CDECL:
    case TokenKind::KEYWORD___STDCALL:
    case TokenKind::KEYWORD___FASTCALL:
    case TokenKind::KEYWORD___THISCALL:
    case TokenKind::KEYWORD___REGCALL:
    case TokenKind::KEYWORD___VECTORCALL:
    case TokenKind::KEYWORD___FORCEINLINE:
    case TokenKind::KEYWORD___UNALIGNED:
    case TokenKind::KEYWORD___SUPER:
    case TokenKind::KEYWORD___GLOBAL:
    case TokenKind::KEYWORD___LOCAL:
    case TokenKind::KEYWORD___CONSTANT:
    case TokenKind::KEYWORD___PRIVATE:
    case TokenKind::KEYWORD___GENERIC:
    case TokenKind::KEYWORD___KERNEL:
    case TokenKind::KEYWORD___READ_ONLY:
    case TokenKind::KEYWORD___WRITE_ONLY:
    case TokenKind::KEYWORD___READ_WRITE:
    case TokenKind::KEYWORD___BUILTIN_ASTYPE:
    case TokenKind::KEYWORD_VEC_STEP:
    case TokenKind::KEYWORD_IMAGE_1D_T:
    case TokenKind::KEYWORD_IMAGE_1D_ARRAY_T:
    case TokenKind::KEYWORD_IMAGE_1D_BUFFER_T:
    case TokenKind::KEYWORD_IMAGE_2D_T:
    case TokenKind::KEYWORD_IMAGE_2D_ARRAY_T:
    case TokenKind::KEYWORD_IMAGE_2D_DEPTH_T:
    case TokenKind::KEYWORD_IMAGE_2D_ARRAY_DEPTH_T:
    case TokenKind::KEYWORD_IMAGE_2D_MSAA_T:
    case TokenKind::KEYWORD_IMAGE_2D_ARRAY_MSAA_T:
    case TokenKind::KEYWORD_IMAGE_2D_MSAA_DEPTH_T:
    case TokenKind::KEYWORD_IMAGE_2D_ARRAY_MSAA_DEPTH_T:
    case TokenKind::KEYWORD_IMAGE_3D_T:
    case TokenKind::KEYWORD_PIPE:
    case TokenKind::KEYWORD_ADDRSPACE_CAST:
    case TokenKind::KEYWORD___NOINLINE__:
    case TokenKind::KEYWORD_CBUFFER:
    case TokenKind::KEYWORD_TBUFFER:
    case TokenKind::KEYWORD_GROUPSHARED:
    case TokenKind::KEYWORD___BUILTIN_OMP_REQUIRED_SIMD_ALIGN:
    case TokenKind::KEYWORD___PASCAL:
    case TokenKind::KEYWORD___VECTOR:
    case TokenKind::KEYWORD___PIXEL:
    case TokenKind::KEYWORD___BOOLEAN:
    case TokenKind::KEYWORD___BF16:
    case TokenKind::KEYWORD_HALF:
    case TokenKind::KEYWORD___BRIDGE:
    case TokenKind::KEYWORD___BRIDGE_TRANSFER:
    case TokenKind::KEYWORD___BRIDGE_RETAINED:
    case TokenKind::KEYWORD___BRIDGE_RETAIN:
    case TokenKind::KEYWORD___COVARIANT:
    case TokenKind::KEYWORD___CONTRAVARIANT:
    case TokenKind::KEYWORD___KINDOF:
    case TokenKind::KEYWORD__NONNULL:
    case TokenKind::KEYWORD__NULLABLE:
    case TokenKind::KEYWORD__NULLABLE_RESULT:
    case TokenKind::KEYWORD__NULL_UNSPECIFIED:
    case TokenKind::KEYWORD___PTR64:
    case TokenKind::KEYWORD___PTR32:
    case TokenKind::KEYWORD___SPTR:
    case TokenKind::KEYWORD___UPTR:
    case TokenKind::KEYWORD___W64:
    case TokenKind::KEYWORD___UUIDOF:
    case TokenKind::KEYWORD___TRY:
    case TokenKind::KEYWORD___FINALLY:
    case TokenKind::KEYWORD___LEAVE:
    case TokenKind::KEYWORD___INT64:
    case TokenKind::KEYWORD___IF_EXISTS:
    case TokenKind::KEYWORD___IF_NOT_EXISTS:
    case TokenKind::KEYWORD___SINGLE_INHERITANCE:
    case TokenKind::KEYWORD___MULTIPLE_INHERITANCE:
    case TokenKind::KEYWORD___VIRTUAL_INHERITANCE:
    case TokenKind::KEYWORD___INTERFACE:
    case TokenKind::KEYWORD___BUILTIN_CONVERTVECTOR:
    case TokenKind::KEYWORD___BUILTIN_BIT_CAST:
    case TokenKind::KEYWORD___BUILTIN_AVAILABLE:
    case TokenKind::KEYWORD___BUILTIN_SYCL_UNIQUE_STABLE_NAME:
    case TokenKind::KEYWORD___UNKNOWN_ANYTYPE:
    case TokenKind::PP_IF:
    case TokenKind::PP_IFDEF:
    case TokenKind::PP_IFNDEF:
    case TokenKind::PP_ELIF:
    case TokenKind::PP_ELIFDEF:
    case TokenKind::PP_ELIFNDEF:
    case TokenKind::PP_ELSE:
    case TokenKind::PP_ENDIF:
    case TokenKind::PP_DEFINED:
    case TokenKind::PP_INCLUDE:
    case TokenKind::PP___INCLUDE_MACROS:
    case TokenKind::PP_DEFINE:
    case TokenKind::PP_UNDEF:
    case TokenKind::PP_LINE:
    case TokenKind::PP_ERROR:
    case TokenKind::PP_PRAGMA:
    case TokenKind::PP_IMPORT:
    case TokenKind::PP_INCLUDE_NEXT:
    case TokenKind::PP_WARNING:
    case TokenKind::PP_IDENTIFIER:
    case TokenKind::PP_SCCS:
    case TokenKind::PP_ASSERT:
    case TokenKind::PP_UNASSERT:
    case TokenKind::PP___PUBLIC_MACRO:
    case TokenKind::PP___PRIVATE_MACRO:
      return true;
    default:
      return false;
  }
}

static bool AddLeadingWhitespace(TokenKind tk) {
  switch (tk) {
//    case TokenKind::NUMERIC_CONSTANT:
//    case TokenKind::CHARACTER_CONSTANT:
//    case TokenKind::WIDE_CHARACTER_CONSTANT:
//    case TokenKind::UTF8_CHARACTER_CONSTANT:
//    case TokenKind::UTF16_CHARACTER_CONSTANT:
//    case TokenKind::UTF32_CHARACTER_CONSTANT:
//    case TokenKind::STRING_LITERAL:
//    case TokenKind::WIDE_STRING_LITERAL:
//    case TokenKind::HEADER_NAME:
//    case TokenKind::UTF8_STRING_LITERAL:
//    case TokenKind::UTF16_STRING_LITERAL:
//    case TokenKind::UTF32_STRING_LITERAL:

    case TokenKind::AMP:
    case TokenKind::AMP_AMP:
    case TokenKind::AMP_EQUAL:
    case TokenKind::STAR:
    case TokenKind::STAR_EQUAL:
    case TokenKind::PLUS:
//    case TokenKind::PLUS_PLUS:
    case TokenKind::PLUS_EQUAL:
    case TokenKind::MINUS:
//    case TokenKind::ARROW:
//    case TokenKind::MINUS_MINUS:
    case TokenKind::MINUS_EQUAL:
    case TokenKind::TILDE:
    case TokenKind::EXCLAIM:
    case TokenKind::EXCLAIM_EQUAL:
    case TokenKind::SLASH:
    case TokenKind::SLASH_EQUAL:
    case TokenKind::PERCENT:
    case TokenKind::PERCENT_EQUAL:
    case TokenKind::LESS:  // TODO(pag): Templates.
    case TokenKind::LESS_LESS:  // TODO(pag): Templates.
    case TokenKind::LESS_EQUAL:
    case TokenKind::LESS_LESS_EQUAL:
    case TokenKind::SPACESHIP:
    case TokenKind::GREATER:
    case TokenKind::GREATER_GREATER:
    case TokenKind::GREATER_EQUAL:
    case TokenKind::GREATER_GREATER_EQUAL:
    case TokenKind::CARET:
    case TokenKind::CARET_EQUAL:
    case TokenKind::PIPE:
    case TokenKind::PIPE_PIPE:
    case TokenKind::PIPE_EQUAL:
    case TokenKind::QUESTION:
//    case TokenKind::COLON:
//    case TokenKind::SEMI:
    case TokenKind::EQUAL:
    case TokenKind::EQUAL_EQUAL:
//    case TokenKind::COMMA:
    case TokenKind::LESS_LESS_LESS:
    case TokenKind::GREATER_GREATER_GREATER:
    case TokenKind::L_BRACE:
    case TokenKind::R_BRACE:
      return true;
    default:
      return IsKeyword(tk);
  }
}

static bool IsFirst(TokenKind tk) {
  switch (tk) {
    case TokenKind::L_PARENTHESIS:
    case TokenKind::L_SQUARE:
    case TokenKind::L_BRACE:
    case TokenKind::R_BRACE:
    case TokenKind::SEMI:
    case TokenKind::COMMA:
      return true;
    default:
      return false;
  }
}

static bool AddTrailingWhitespace(TokenKind tk) {
  switch (tk) {
    case TokenKind::AMP:
    case TokenKind::AMP_AMP:
    case TokenKind::AMP_EQUAL:
    case TokenKind::STAR:
    case TokenKind::STAR_EQUAL:
    case TokenKind::PLUS:
//    case TokenKind::PLUS_PLUS:
    case TokenKind::PLUS_EQUAL:
    case TokenKind::MINUS:
//    case TokenKind::ARROW:
//    case TokenKind::MINUS_MINUS:
    case TokenKind::MINUS_EQUAL:
//    case TokenKind::TILDE:
//    case TokenKind::EXCLAIM:
    case TokenKind::EXCLAIM_EQUAL:
    case TokenKind::SLASH:
    case TokenKind::SLASH_EQUAL:
    case TokenKind::PERCENT:
    case TokenKind::PERCENT_EQUAL:
    case TokenKind::LESS:  // TODO(pag): Templates.
    case TokenKind::LESS_LESS:  // TODO(pag): Templates.
    case TokenKind::LESS_EQUAL:
    case TokenKind::LESS_LESS_EQUAL:
    case TokenKind::SPACESHIP:
    case TokenKind::GREATER:
    case TokenKind::GREATER_GREATER:
    case TokenKind::GREATER_EQUAL:
    case TokenKind::GREATER_GREATER_EQUAL:
    case TokenKind::CARET:
    case TokenKind::CARET_EQUAL:
    case TokenKind::PIPE:
    case TokenKind::PIPE_PIPE:
    case TokenKind::PIPE_EQUAL:
    case TokenKind::QUESTION:
    case TokenKind::COLON:
    case TokenKind::SEMI:
    case TokenKind::EQUAL:
    case TokenKind::EQUAL_EQUAL:
    case TokenKind::COMMA:
    case TokenKind::LESS_LESS_LESS:
    case TokenKind::GREATER_GREATER_GREATER:
    case TokenKind::R_BRACE:
      return true;
    default:
      return IsKeyword(tk);
  }
}

static bool AddTrailingWhitespaceAsFirst(TokenKind tk) {
  switch (tk) {
    case TokenKind::STAR:
    case TokenKind::AMP:
    case TokenKind::PLUS:
    case TokenKind::MINUS:
      return false;
    default:
      return AddTrailingWhitespace(tk);
  }
}

static bool SuppressLeadingWhitespace(TokenKind tk) {
  switch (tk) {
    case TokenKind::COMMA:
    case TokenKind::R_PARENTHESIS:
    case TokenKind::R_SQUARE:
      return true;
    default:
      return false;
  }
}

static bool ForceLeadingWhitespace(bool prev_is_first, TokenKind prev, TokenKind curr) {
  auto prev_is_ident_kw = prev == TokenKind::IDENTIFIER || IsKeyword(prev);
  auto curr_is_ident_kw = curr == TokenKind::IDENTIFIER || IsKeyword(curr);
  if (prev_is_ident_kw && curr_is_ident_kw) {
    return true;
  }
  (void) prev_is_first;
  return prev == TokenKind::COMMA || prev == TokenKind::SEMI;
}

}  // namespace

//! Create a new token range, derived from `toks`, that introduces/injects
//! fake whitespace between `toks`. This is nifty when you want to render out
//! a parsed token range for human consumption.
TokenRange InjectWhitespace(const TokenRange &toks) {
  std::vector<CustomToken> tokens;
  bool add_leading_ws = false;
  bool is_first = true;
  TokenKind last_tk = TokenKind::UNKNOWN;

  for (Token tok : toks) {
    TokenKind tk = tok.kind();

    if (!add_leading_ws) {
      add_leading_ws = ForceLeadingWhitespace(is_first, last_tk, tk);
    }

    if (add_leading_ws || (!is_first && AddLeadingWhitespace(tk))) {
      if (!SuppressLeadingWhitespace(tk)) {
        UserToken st;
        st.kind = TokenKind::WHITESPACE;
        st.category = TokenCategory::WHITESPACE;
        st.data = " ";
        tokens.emplace_back(std::move(st));
      }
    }

    tokens.emplace_back(std::move(tok));
    last_tk = tk;
    if (is_first) {
      add_leading_ws = AddTrailingWhitespaceAsFirst(tk);
    } else {
      add_leading_ws = AddTrailingWhitespace(tk);
    }
    is_first = IsFirst(tk);
  }

  return TokenRange::create(std::move(tokens));
}

static VariantEntity NamedDeclContainingOperation(const ir::Operation &op) {
  if (auto decl = Decl::from(op)) {
    return NamedDeclContaining(decl.value());
  } else if (auto stmt = Stmt::from(op)) {
    return NamedDeclContaining(stmt.value());
  } else if (auto block = ir::Block::containing(op)) {
    return NamedDeclContainingOperation(
        ir::Operation::containing(block.value()));
  } else {
    return NotAnEntity{};
  }
}

//! Return the named declaration containing `thing`, or `NotAnEntity`.
VariantEntity NamedDeclContaining(const VariantEntity &ent) {
  const auto VariantEntityVisitor = Overload{
      [](const Decl &entity) { return NamedDeclContaining(entity); },
      [](const Stmt &entity) { return NamedDeclContaining(entity); },
      [](const Token &entity) { return NamedDeclContaining(entity); },
      [](const Macro &entity) -> VariantEntity {
        for (Token tok : entity.root().generate_use_tokens()) {
          if (auto cont = NamedDeclContaining(tok);
              !std::holds_alternative<NotAnEntity>(cont)) {
            return cont;
          }
        }
        return NotAnEntity{};
      },
      [](const ir::Operation &op) {
        return NamedDeclContainingOperation(op);
      },
      [](const auto &entity) -> VariantEntity {
        for (Token tok : entity.tokens()) {
          if (auto cont = NamedDeclContaining(tok);
              !std::holds_alternative<NotAnEntity>(cont)) {
            return cont;
          }
        }
        return NotAnEntity{};
      },
      [](const Fragment &) -> VariantEntity { return NotAnEntity{}; },
      [](const File &) -> VariantEntity { return NotAnEntity{}; },
      [](const Type &) -> VariantEntity { return NotAnEntity{}; },
      [](const TemplateArgument &) -> VariantEntity { return NotAnEntity{}; },
      [](const Compilation &) -> VariantEntity { return NotAnEntity{}; },
      [](const NotAnEntity &) -> VariantEntity { return NotAnEntity{}; }};
  return std::visit<VariantEntity>(VariantEntityVisitor, ent);
}

//! Return the entity ID associated with `ent`.
RawEntityId IdOfEntity(const VariantEntity &ent) {
  const auto VariantEntityVisitor =
      Overload{[](const auto &entity) { return entity.id().Pack(); },
               [](const NotAnEntity &) { return kInvalidEntityId; }};
  return std::visit<RawEntityId>(VariantEntityVisitor, ent);
}

//! Return the file containing an entity.
std::optional<File> FileOfEntity(const VariantEntity &ent) {
  const auto VariantEntityVisitor = Overload{
      [](const Decl &entity) { return File::containing(entity); },
      [](const Stmt &entity) { return File::containing(entity); },
      [](const Type &entity) { return File::containing(entity); },
      [](const Token &entity) { return File::containing(entity); },
      [](const Macro &entity) { return File::containing(entity); },
      [](const Designator &entity) { return File::containing(entity); },
      [](const CXXBaseSpecifier &entity) { return File::containing(entity); },
      [](const TemplateArgument &entity) { return File::containing(entity); },
      [](const TemplateParameterList &entity) {
        return File::containing(entity);
      },
      [](const Fragment &entity) { return File::containing(entity); },
      [](const File &entity) { return entity; },
      [](const Compilation &entity) -> std::optional<File> { return entity.main_source_file(); },
      [](auto) -> std::optional<File> { return std::nullopt; }};
  return std::visit<std::optional<File>>(VariantEntityVisitor, ent);
}

//! Return the name of an entity.
TokenRange NameOfEntity(const VariantEntity &ent,
                        bool qualified, bool scan_redecls) {

  const auto VariantEntityVisitor = Overload{
      [qualified, scan_redecls](const Decl &decl) -> TokenRange {
        Token name_tok;

        if (auto named = NamedDecl::from(decl)) {
          std::string_view orig_name = named->name();
          std::string_view name = orig_name;
          auto maybe_name_tok = named->token();
          if (!name.empty() && name == maybe_name_tok.data()) {
            name_tok = std::move(maybe_name_tok);
          
          } else if (scan_redecls) {
            for (NamedDecl redecl : named->redeclarations()) {
              name = redecl.name();
              maybe_name_tok = redecl.token();
              if (!name.empty() && name == maybe_name_tok.data()) {
                name_tok = std::move(maybe_name_tok);
                break;
              }
            }
          }

          if (!orig_name.empty() && !name_tok) {
            Q_ASSERT(false);
            std::vector<CustomToken> toks;
            UserToken tok;
            tok.related_entity = decl;
            tok.category = Token::categorize(tok.related_entity);
            tok.kind = TokenKind::IDENTIFIER;
            tok.data.insert(tok.data.end(), orig_name.begin(), orig_name.end());
            name_tok = TokenRange::create(std::move(toks)).front();
          }
        }

        if (!name_tok) {
          std::vector<CustomToken> toks;
          UserToken tok;
          tok.related_entity = decl;
          tok.category = Token::categorize(tok.related_entity);
          tok.kind = TokenKind::IDENTIFIER;

          switch (tok.category) {
            case TokenCategory::ENUM:
              tok.data = QObject::tr("(anonymous enum)").toStdString();
              break;

            case TokenCategory::CLASS:
              tok.data = QObject::tr("(anonymous class)").toStdString();
              break;

            case TokenCategory::STRUCT:
              tok.data = QObject::tr("(anonymous struct)").toStdString();
              break;

            case TokenCategory::UNION:
              tok.data = QObject::tr("(anonymous union)").toStdString();
              break;

            case TokenCategory::INSTANCE_MEMBER:
              tok.data = QObject::tr("(anonymous field)").toStdString();
              break;

            case TokenCategory::PARAMETER_VARIABLE:
              tok.data = QObject::tr("(anonymous parameter)").toStdString();
              break;

            case TokenCategory::NAMESPACE:
              tok.data = QObject::tr("(anonymous namespace)").toStdString();
              break;

            default:
              tok.data = QObject::tr("(anonymous)").toStdString();
              break;
          }

          toks.emplace_back(std::move(tok));
          name_tok = TokenRange::create(std::move(toks)).front();
        }

        auto pd = decl.parent_declaration();
        if (!pd || !qualified) {
          return TokenRange(name_tok);
        }

        std::vector<CustomToken> toks;

        for (Token tok : NameOfEntity(pd.value(), qualified, scan_redecls)) {
          toks.emplace_back(std::move(tok));
        }

        if (toks.empty()) {
          return TokenRange(name_tok);
        }

        UserToken tok;
        tok.category = TokenCategory::PUNCTUATION;
        tok.kind = TokenKind::COLON_COLON;
        tok.data = "::";
        toks.emplace_back(std::move(tok));

        toks.emplace_back(std::move(name_tok));

        return TokenRange::create(std::move(toks));
      },

      [](const Stmt &stmt) -> TokenRange {
        if (auto dre = DeclRefExpr::from(stmt)) {
          return dre->expression_token();

        } else if (auto me = MemberExpr::from(stmt)) {
          return me->member_token();

        } else if (auto ale = AddrLabelExpr::from(stmt)) {
          return ale->label_token();
        
        } else if (auto ls = LabelStmt::from(stmt)) {
          return ls->identifier_token();
        }
        return TokenRange();
      },

      [](const Macro &macro) -> TokenRange {
        if (auto named = DefineMacroDirective::from(macro)) {
          return named->name();

        } else if (auto param = MacroParameter::from(macro)) {
          return param->name();

        } else {
          return TokenRange();
        }
      },

      [](const File &file) -> TokenRange {
        for (std::filesystem::path path : file.paths()) {
          UserToken tk;
          tk.data = path.generic_string();
          tk.category = TokenCategory::FILE_NAME;
          tk.kind = TokenKind::HEADER_NAME;
          tk.related_entity = file;
          std::vector<CustomToken> tokens;
          tokens.emplace_back(std::move(tk));
          return TokenRange::create(std::move(tokens));
        }
        return TokenRange();
      },

      [](const Designator &designator) -> TokenRange {
        return designator.field_token();
      },

      [](const Token &token) -> TokenRange {
        return token;
      },

      [](auto) -> TokenRange { return TokenRange(); }};

  return std::visit<TokenRange>(VariantEntityVisitor, ent);
}

//! Return the name of an entity as a `QString`.
std::optional<QString> NameOfEntityAsString(const VariantEntity &ent,
                                            bool qualified) {
  if (auto name = NameOfEntity(ent, qualified)) {
    std::string_view data = name.data();
    if (!data.empty()) {
      return QString::fromUtf8(data.data(),
                               static_cast<qsizetype>(data.size()));
    }
  }
  return std::nullopt;
}

namespace {

static std::filesystem::path FilePath(const File &file) {
  for (auto path : file.paths()) {
    return path;
  }
  return {};
}

}  // namespace

QString LocationOfEntity(const FileLocationCache &file_location_cache,
                         const VariantEntity &entity) {

  if (std::optional<EntityLocation> opt_loc =
      LocationOfEntityEx(file_location_cache, entity)) {

    const auto &location = opt_loc.value();

    return QString("%1:%2:%3")
          .arg(QString::fromStdString(location.path.generic_string()))
          .arg(location.line)
          .arg(location.column);
  }

  return {};
}

std::optional<EntityLocation>
LocationOfEntityEx(const FileLocationCache &file_location_cache,
                   const VariantEntity &entity) {

  std::filesystem::path location;
  for (Token tok : FileTokens(entity)) {
    auto file = File::containing(tok);
    if (!file) {
      continue;
    }

    location = FilePath(file.value());

    if (auto line_col = tok.location(file_location_cache)) {
      return EntityLocation{location,
                            line_col->first,
                            line_col->second};
    }
  }

  if (location.empty()) {
    return std::nullopt;
  }

  return EntityLocation{location, 0, 0};
}

//! Return the tokens of `tokens` as a string.
QString TokensToString(const TokenRange &tokens) {
  QString data;
  auto sep = "";
  for (Token tok : tokens) {
    if (tok.kind() == TokenKind::WHITESPACE) {
      data += sep;
      sep = "";
    } else {
      data += QString::fromUtf8(tok.data().data(),
                                static_cast<qsizetype>(tok.data().size()));

      // Only materialize whitespace into spaces if there is a non-whitespace
      // token in-between.
      sep = " ";
    }
  }
  return data;
}

//! Return the tokens of `ent` as a string.
QString TokensToString(const VariantEntity &ent) {
  return TokensToString(FileTokens(ent));
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
          crumbs.AddEnum(expr.keyword_kind());
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

QString EntityBreadCrumbs(const VariantEntity &ent, bool run_length_encode) {

  if (std::holds_alternative<Decl>(ent)) {
    const Decl &decl = std::get<Decl>(ent);
    auto tok = decl.token();
    if (auto nd = NamedDecl::from(decl)) {
      if (nd->name().empty()) {
        tok = decl.tokens().front();
      }
    }
    return TokenBreadCrumbs(tok, run_length_encode);

  } else if (std::holds_alternative<Stmt>(ent)) {
    const Stmt &stmt = std::get<Stmt>(ent);
    if (std::optional<Expr> expr = Expr::from(stmt)) {
      if (Token tok = expr->expression_token()) {
        return TokenBreadCrumbs(tok, run_length_encode);
      }
    }
    for (Token tok : stmt.tokens()) {
      return TokenBreadCrumbs(tok, run_length_encode);
    }

  } else if (std::holds_alternative<Macro>(ent)) {
    std::optional<Macro> m;
    m.emplace(std::get<Macro>(ent));
    for (; m; m = m->parent()) {
      for (Token tok : m->generate_expansion_tokens()) {
        if (Token ptok = tok.parsed_token()) {
          return TokenBreadCrumbs(ptok, run_length_encode);
        }
      }
    }

  } else if (std::holds_alternative<Designator>(ent)) {
    const Designator &designator = std::get<Designator>(ent);
    if (Token tok = designator.field_token()) {
      return TokenBreadCrumbs(tok, run_length_encode);
    }
    for (Token tok : designator.tokens()) {
      return TokenBreadCrumbs(tok, run_length_encode);
    }
  }

  return {};
}

// Convert `variant` to a string.
std::optional<QString> TryConvertToString(const QVariant &data) {
  if (data.canConvert<QString>()) {
    return data.toString();

  } else if (data.canConvert<TokenRange>()) {
    auto tok_range = data.value<TokenRange>();
    auto tok_data = tok_range.data();
    if (!tok_data.empty()) {
      return QString::fromUtf8(
          tok_data.data(), static_cast<qsizetype>(tok_data.size()));
    }
  }

  return QString();
}

}  // namespace mx::gui
