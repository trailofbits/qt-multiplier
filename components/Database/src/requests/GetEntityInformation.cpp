/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetEntityInformation.h"

#include <multiplier/ui/Assert.h>
#include <multiplier/ui/Util.h>

#include <multiplier/Entities/ArraySubscriptExpr.h>
#include <multiplier/Entities/BinaryOperator.h>
#include <multiplier/Entities/CallExpr.h>
#include <multiplier/Entities/CastExpr.h>
#include <multiplier/Entities/ConditionalOperator.h>
#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/DoStmt.h>
#include <multiplier/Entities/EnumConstantDecl.h>
#include <multiplier/Entities/EnumDecl.h>
#include <multiplier/Entities/File.h>
#include <multiplier/Entities/ForStmt.h>
#include <multiplier/Entities/Fragment.h>
#include <multiplier/Entities/FunctionDecl.h>
#include <multiplier/Entities/IfStmt.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>
#include <multiplier/Entities/MacroExpansion.h>
#include <multiplier/Entities/MacroParameter.h>
#include <multiplier/Entities/MemberExpr.h>
#include <multiplier/Entities/NamedDecl.h>
#include <multiplier/Entities/RecordDecl.h>
#include <multiplier/Entities/SwitchStmt.h>
#include <multiplier/Entities/TypeTraitExpr.h>
#include <multiplier/Entities/UnaryOperator.h>
#include <multiplier/Entities/UnaryExprOrTypeTraitExpr.h>
#include <multiplier/Entities/WhileStmt.h>

#include <filesystem>

namespace mx::gui {
namespace {

const std::size_t kBatchSize{512};

std::size_t GetBatchSize(const EntityInformation &info) {
  return info.declarations.size() + info.definitions.size() +
         info.macros_used.size() + info.callees.size() + info.callers.size() +
         info.pointers.size() + info.address_ofs.size() +
         info.dereferences.size() + info.arguments.size() +
         info.assigned_tos.size() + info.assignments.size() +
         info.tests.size() + info.uses.size() + info.includes.size() +
         info.include_bys.size() + info.enumerators.size() +
         info.parameters.size() + info.variables.size() + info.members.size() +
         info.type_casts.size() + info.top_level_entities.size();
}

void SendBatch(IDatabase::RequestEntityInformationReceiver &receiver,
               EntityInformation &info, bool force = false) {
  if (!force && GetBatchSize(info) < kBatchSize) {
    return;
  }

  IDatabase::RequestEntityInformationReceiver::DataBatch data_batch{info};

  receiver.OnDataBatch(data_batch);

  info.declarations.clear();
  info.definitions.clear();
  info.macros_used.clear();
  info.callees.clear();
  info.callers.clear();
  info.pointers.clear();
  info.address_ofs.clear();
  info.dereferences.clear();
  info.arguments.clear();
  info.assigned_tos.clear();
  info.assignments.clear();
  info.tests.clear();
  info.uses.clear();
  info.includes.clear();
  info.include_bys.clear();
  info.enumerators.clear();
  info.parameters.clear();
  info.variables.clear();
  info.members.clear();
  info.type_casts.clear();
  info.top_level_entities.clear();
}

struct EntityInformationScopedSender final {
  EntityInformation &info;
  IDatabase::RequestEntityInformationReceiver &receiver;

  EntityInformationScopedSender(
      EntityInformation &info_,
      IDatabase::RequestEntityInformationReceiver &receiver_)
      : info(info_),
        receiver(receiver_) {}

  ~EntityInformationScopedSender() {
    SendBatch(receiver, info, true);
  }
};

static std::optional<EntityInformation::Location>
GetLocation(QPromise<bool> &result_promise, const TokenRange &toks,
            const FileLocationCache &file_location_cache) {
  for (Token tok : toks.file_tokens()) {
    if (result_promise.isCanceled()) {
      return std::nullopt;
    }

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
static void
FillUsedMacros(QPromise<bool> &result_promise,
               IDatabase::RequestEntityInformationReceiver &receiver,
               const FileLocationCache &file_location_cache,
               EntityInformation &info, const TokenRange &tokens) {
  std::vector<PackedMacroId> seen;

  for (Token tok : tokens) {
    if (result_promise.isCanceled()) {
      return;
    }

    for (Macro macro : Macro::containing(tok)) {
      if (result_promise.isCanceled()) {
        return;
      }

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
        if (result_promise.isCanceled()) {
          return;
        }

        if (use_tok.category() == TokenCategory::MACRO_NAME) {
          EntityInformation::Selection &sel = info.macros_used.emplace_back();
          sel.display_role.setValue(use_tok);
          sel.entity_role = std::move(exp.value());
          sel.location =
              GetLocation(result_promise, use_tok, file_location_cache);
          break;
        }
      }
      break;
    }

    SendBatch(receiver, info);
  }
}

//// Find a good token for the use of `entity` in `designator`.
//static Token FindDesignatorUse(const Designator &designator,
//                               const NamedDecl &entity) {
//  Token backup = designator.field_token();
//  for (Token tok : designator.tokens()) {
//    if (!backup) {
//      backup = tok;
//    }
//    if (tok.related_entity_id().Pack() == entity.id().Pack()) {
//      return tok;
//    }
//    VariantEntity ent = tok.related_entity();
//    if (std::holds_alternative<Decl>(ent)) {
//      if (std::get<Decl>(ent) == entity) {
//        return tok;
//      }
//    }
//  }
//
//  return backup;
//}

//! Find the statement that value represents the current line of code.
static mx::TokenRange FindLine(QPromise<bool> &result_promise,
                               mx::Stmt prev_stmt) {
  // Find implicit casts with the return value
  for (mx::Stmt stmt : mx::Stmt::containing(prev_stmt)) {
    if (result_promise.isCanceled()) {
      return {};
    }

    switch (stmt.kind()) {

      // Don't ascend too far up the statement parentage.
      case StmtKind::CASE_STMT:
      case StmtKind::DEFAULT_STMT:
      case StmtKind::LABEL_STMT:
      case StmtKind::COMPOUND_STMT:
      case StmtKind::SWITCH_STMT:
      case StmtKind::DO_STMT:
      case StmtKind::WHILE_STMT:
      case StmtKind::FOR_STMT:
      case StmtKind::IF_STMT:
      case StmtKind::CXX_TRY_STMT:
      case StmtKind::CXX_FOR_RANGE_STMT:
      case StmtKind::CXX_CATCH_STMT:
      case StmtKind::COROUTINE_BODY_STMT:  // Not sure what this is.
        goto done;

      case StmtKind::DECL_STMT: prev_stmt = std::move(stmt); goto done;

      default: prev_stmt = std::move(stmt); break;
    }
  }

done:
  return InjectWhitespace(prev_stmt.tokens().strip_whitespace());
}

//! Fill `info` with information about the variable `var`.
static void
FillTypeInformation(QPromise<bool> &result_promise,
                    IDatabase::RequestEntityInformationReceiver &receiver,
                    const FileLocationCache &file_location_cache,
                    EntityInformation &info, const TypeDecl &entity,
                    Reference ref) {

  // TODO(pag): Do better with these.
  (void) entity;

  if (auto du = ref.as_declaration()) {
    EntityInformation::Selection *sel = &(info.uses.emplace_back());
    sel->display_role.setValue(
        InjectWhitespace(du->tokens().strip_whitespace()));
    sel->entity_role = du.value();
    sel->location =
        GetLocation(result_promise, du->tokens(), file_location_cache);

  } else if (auto su = ref.as_statement()) {
    Stmt orig_su = su.value();
    for (; su; su = su->parent_statement()) {
      if (result_promise.isCanceled()) {
        return;
      }

      SendBatch(receiver, info);

      if (CastExpr::from(su.value())) {
        EntityInformation::Selection *sel = &(info.type_casts.emplace_back());
        sel->display_role.setValue(
            InjectWhitespace(su->tokens().strip_whitespace()));
        sel->entity_role = orig_su;
        sel->location =
            GetLocation(result_promise, su->tokens(), file_location_cache);
        return;

      } else if (TypeTraitExpr::from(su.value()) ||
                 UnaryExprOrTypeTraitExpr::from(su.value())) {
        EntityInformation::Selection *sel = &(info.uses.emplace_back());
        sel->display_role.setValue(
            InjectWhitespace(su->tokens().strip_whitespace()));
        sel->entity_role = orig_su;
        sel->location =
            GetLocation(result_promise, su->tokens(), file_location_cache);
        return;
      }
    }

    EntityInformation::Selection *sel = &(info.uses.emplace_back());
    sel->display_role.setValue(
        InjectWhitespace(orig_su.tokens().strip_whitespace()));
    sel->entity_role = orig_su;
    sel->location =
        GetLocation(result_promise, orig_su.tokens(), file_location_cache);
    return;
  }
}

//! Fill `info` with information about the variable `var`.
static void
FillTypeInformation(QPromise<bool> &result_promise,
                    IDatabase::RequestEntityInformationReceiver &receiver,
                    const FileLocationCache &file_location_cache,
                    EntityInformation &info, TypeDecl entity) {

  for (Reference ref : entity.references()) {
    if (result_promise.isCanceled()) {
      return;
    }

    FillTypeInformation(result_promise, receiver, file_location_cache, info,
                        entity, std::move(ref));
  }
}

//! Fill `info` with information about the variable `var` as used by the
//! statement `stmt`.
static void FillVariableUsedByStatementInformation(
    QPromise<bool> &result_promise,
    IDatabase::RequestEntityInformationReceiver &receiver,
    const FileLocationCache &file_location_cache, EntityInformation &info,
    Stmt stmt) {

  bool is_field = stmt.kind() == StmtKind::MEMBER_EXPR;
  Assert(is_field || stmt.kind() == StmtKind::DECL_REF_EXPR,
         "Unexpected user statement");

  EntityInformation::Selection *sel = nullptr;
  Stmt child = stmt;
  std::optional<Stmt> parent = child.parent_statement();
  for (; parent; child = parent.value(), parent = parent->parent_statement()) {
    if (result_promise.isCanceled()) {
      return;
    }

    SendBatch(receiver, info);

    switch (parent->kind()) {
      case StmtKind::SWITCH_STMT:
        if (auto switch_ = SwitchStmt::from(parent);
            switch_ && switch_->condition() == child) {
          goto conditional_use;
        } else {
          goto generic_use;
        }
      case StmtKind::DO_STMT:
        if (auto do_ = DoStmt::from(parent); do_ && do_->condition() == child) {
          goto conditional_use;
        } else {
          goto generic_use;
        }
      case StmtKind::WHILE_STMT:
        if (auto while_ = WhileStmt::from(parent);
            while_ && while_->condition() == child) {
          goto conditional_use;
        } else {
          goto generic_use;
        }
      case StmtKind::FOR_STMT:
        if (auto for_ = ForStmt::from(parent);
            for_ && for_->condition() == child) {
          goto conditional_use;
        } else {
          goto generic_use;
        }
      case StmtKind::IF_STMT:
        if (auto if_ = IfStmt::from(parent); if_ && if_->condition() == child) {
          goto conditional_use;
        } else {
          goto generic_use;
        }

      // This is going to be something like: `var;`. It's a useless use.
      case StmtKind::CASE_STMT:
      case StmtKind::DEFAULT_STMT:
      case StmtKind::LABEL_STMT:
      case StmtKind::COMPOUND_STMT:
      case StmtKind::CXX_TRY_STMT:
      case StmtKind::CXX_FOR_RANGE_STMT:
      case StmtKind::CXX_CATCH_STMT:
      case StmtKind::COROUTINE_BODY_STMT: goto generic_use;

      case StmtKind::UNARY_OPERATOR:
        if (auto uop = UnaryOperator::from(parent)) {
          switch (uop->opcode()) {
            case UnaryOperatorKind::ADDRESS_OF: goto address_of_use;
            case UnaryOperatorKind::DEREF: goto dereference_use;
            default: break;
          }
        }
        continue;
      case StmtKind::COMPOUND_ASSIGN_OPERATOR:
      case StmtKind::BINARY_OPERATOR:
        if (auto bin = BinaryOperator::from(parent)) {
          switch (bin->opcode()) {
            case mx::BinaryOperatorKind::ASSIGN:
            case mx::BinaryOperatorKind::MUL_ASSIGN:
            case mx::BinaryOperatorKind::DIV_ASSIGN:
            case mx::BinaryOperatorKind::REM_ASSIGN:
            case mx::BinaryOperatorKind::ADD_ASSIGN:
            case mx::BinaryOperatorKind::SUB_ASSIGN:
            case mx::BinaryOperatorKind::SHL_ASSIGN:
            case mx::BinaryOperatorKind::SHR_ASSIGN:
            case mx::BinaryOperatorKind::AND_ASSIGN:
            case mx::BinaryOperatorKind::XOR_ASSIGN:
            case mx::BinaryOperatorKind::OR_ASSIGN:
              if (bin->lhs() == child) {
                goto assigned_to_use;
              } else if (bin->rhs() == child) {
                goto assigned_from_use;
              }
              break;
            case mx::BinaryOperatorKind::COMMA:
              if (bin->lhs() == child) {
                goto generic_use;
              }
            default: break;
          }
        }
        continue;
      case StmtKind::CONDITIONAL_OPERATOR:
        if (auto cond = ConditionalOperator::from(parent);
            cond && cond->condition() == child) {
          goto conditional_use;
        } else {
          continue;
        }
      case StmtKind::MEMBER_EXPR:
        if (auto member = MemberExpr::from(parent);
            member && member->base() == child && member->is_arrow()) {
          goto dereference_use;
        }
        if (is_field) {
          goto generic_use;
        }
        continue;
      case StmtKind::ARRAY_SUBSCRIPT_EXPR:
        if (auto arr = ArraySubscriptExpr::from(parent)) {
          goto dereference_use;
        }
        continue;
      case StmtKind::CALL_EXPR:
        if (auto call = CallExpr::from(parent)) {
          if (call->callee() == child) {
            goto dereference_use;
          } else {
            if (auto dc = call->direct_callee();
                !dc || !dc->name().starts_with("__builtin_")) {
              goto argument_use;
            }
          }
        }
        continue;
      case StmtKind::DECL_STMT:
      case StmtKind::DESIGNATED_INIT_EXPR:
      case StmtKind::DESIGNATED_INIT_UPDATE_EXPR:
        for (auto init : parent->children()) {
          if (init == child) {
            goto assigned_from_use;
          }
        }
        goto assigned_to_use;
      default: break;
    }
  }

  // Fall-through case.
generic_use:
  sel = &(info.uses.emplace_back());
  sel->display_role.setValue(
      InjectWhitespace(child.tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  return;

argument_use:
  sel = &(info.arguments.emplace_back());
  sel->display_role.setValue(
      InjectWhitespace(parent->tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  return;

assigned_to_use:
  sel = &(info.assigned_tos.emplace_back());
  sel->display_role.setValue(
      InjectWhitespace(parent->tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  return;

assigned_from_use:
  sel = &(info.assignments.emplace_back());
  sel->display_role.setValue(
      InjectWhitespace(parent->tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  return;

address_of_use:
  sel = &(info.address_ofs.emplace_back());
  sel->display_role.setValue(
      InjectWhitespace(parent->tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  return;

dereference_use:
  sel = &(info.dereferences.emplace_back());
  sel->display_role.setValue(
      InjectWhitespace(parent->tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  return;

conditional_use:
  sel = &(info.tests.emplace_back());
  sel->display_role.setValue(
      InjectWhitespace(child.tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  return;
}

//! Fill `info` with information about the variable `var`.
static void
FillVariableInformation(QPromise<bool> &result_promise,
                        IDatabase::RequestEntityInformationReceiver &receiver,
                        const FileLocationCache &file_location_cache,
                        EntityInformation &info, const ValueDecl &var) {

  for (Reference ref : var.references()) {
    if (std::optional<Stmt> stmt = ref.as_statement()) {
      FillVariableUsedByStatementInformation(result_promise, receiver,
                                             file_location_cache, info,
                                             std::move(stmt.value()));
    }
  }
}

//! Fill `info` with information about the function `func`.
static void
FillFunctionInformation(QPromise<bool> &result_promise,
                        IDatabase::RequestEntityInformationReceiver &receiver,
                        const FileLocationCache &file_location_cache,
                        EntityInformation &info, FunctionDecl func) {

  for (Reference ref : func.references()) {
    if (result_promise.isCanceled()) {
      return;
    }

    if (auto designator = ref.as_designator()) {
      TokenRange tokens = designator->tokens();
      EntityInformation::Selection &sel = info.pointers.emplace_back();
      sel.display_role.setValue(InjectWhitespace(tokens.strip_whitespace()));
      sel.entity_role = designator.value();
      sel.location = GetLocation(result_promise, tokens, file_location_cache);
      continue;
    }

    SendBatch(receiver, info);

    if (std::optional<Stmt> stmt = ref.as_statement()) {

      // Look for direct calls. This is replicating `FunctionDecl::callers`.
      for (CallExpr call : CallExpr::containing(stmt.value())) {
        if (result_promise.isCanceled()) {
          return;
        }

        SendBatch(receiver, info);

        if (std::optional<FunctionDecl> callee = call.direct_callee()) {
          if (callee.value() != func) {
            continue;
          }

          EntityInformation::Selection &sel = info.callers.emplace_back();
          sel.entity_role = call;
          sel.location =
              GetLocation(result_promise, call.tokens(), file_location_cache);
          for (FunctionDecl caller : FunctionDecl::containing(call)) {
            sel.display_role.setValue(caller.token());
            break;
          }

          goto found;
        }
      }

      // If we didn't find a caller, then it's probably an `address_ofs`.
      {
        EntityInformation::Selection &sel = info.pointers.emplace_back();
        sel.display_role.setValue(FindLine(result_promise, stmt.value()));
        sel.entity_role = stmt.value();
        sel.location =
            GetLocation(result_promise, stmt->tokens(), file_location_cache);

        if (result_promise.isCanceled()) {
          return;
        }
      }
    }

  found:
    continue;
  }

  // Find the callees. Slightly annoying as we kind of have to invent a join.
  //
  // TODO(pag): Make `::in(entity)` work for all entities, not just files
  //            and fragments.
  Fragment frag = Fragment::containing(func);
  for (CallExpr call : CallExpr::in(frag)) {
    SendBatch(receiver, info);

    if (result_promise.isCanceled()) {
      return;
    }

    std::optional<FunctionDecl> callee = call.direct_callee();
    if (!callee) {
      continue;  // TODO(pag): Look at how SciTools renders indirect callees.
    }

    // Make sure the callee is nested inside of `entity` (i.e. `func`).
    for (FunctionDecl callee_func : FunctionDecl::containing(call)) {
      if (result_promise.isCanceled()) {
        return;
      }

      if (callee_func != func) {
        continue;
      }

      EntityInformation::Selection &sel = info.callees.emplace_back();
      sel.display_role.setValue(callee->token());
      sel.entity_role = call;
      sel.location =
          GetLocation(result_promise, call.tokens(), file_location_cache);
      break;
    }
  }

  // Find the local variables.
  for (const mx::Decl &var : func.declarations_in_context()) {
    if (result_promise.isCanceled()) {
      return;
    }

    if (auto vd = mx::VarDecl::from(var)) {
      EntityInformation::Selection *sel =
          vd->kind() == DeclKind::PARM_VAR ? &(info.parameters.emplace_back())
                                           : &(info.variables.emplace_back());
      sel->display_role.setValue(vd->token());
      sel->entity_role = vd.value();
      sel->location =
          GetLocation(result_promise, vd->tokens(), file_location_cache);
    }

    SendBatch(receiver, info);
  }
}

//! Fill `info` with information about the variable `var`.
static void
FillEnumInformation(QPromise<bool> &result_promise,
                    IDatabase::RequestEntityInformationReceiver &receiver,
                    const FileLocationCache &file_location_cache,
                    EntityInformation &info, EnumDecl entity) {

  for (mx::EnumConstantDecl ec : entity.enumerators()) {
    if (result_promise.isCanceled()) {
      return;
    }

    EntityInformation::Selection &sel = info.enumerators.emplace_back();
    sel.display_role.setValue(ec.token());
    sel.entity_role = ec;
    sel.location =
        GetLocation(result_promise, ec.tokens(), file_location_cache);

    SendBatch(receiver, info);
  }
}

//! Fill `info` with information about the variable `var`.
static void
FillRecordInformation(QPromise<bool> &result_promise,
                      IDatabase::RequestEntityInformationReceiver &receiver,
                      const FileLocationCache &file_location_cache,
                      EntityInformation &info, RecordDecl entity) {

  // Find the local variables.
  for (const mx::Decl &decl : entity.declarations_in_context()) {
    if (result_promise.isCanceled()) {
      return;
    }

    if (auto vd = mx::VarDecl::from(decl)) {
      EntityInformation::Selection &sel = info.variables.emplace_back();
      sel.display_role.setValue(vd->token());
      sel.entity_role = vd.value();
      sel.location =
          GetLocation(result_promise, vd->tokens(), file_location_cache);

    } else if (auto fd = mx::FieldDecl::from(decl)) {
      EntityInformation::Selection &sel = info.members.emplace_back();
      sel.display_role.setValue(fd->token());
      sel.entity_role = fd.value();
      sel.location =
          GetLocation(result_promise, fd->tokens(), file_location_cache);
    }

    SendBatch(receiver, info);

    // TODO(pag): FunctionDecl, CXXMethodDecl, etc.
  }
}

//! Fill `info` with information about the file `entity`.
static void
GetFileInformation(QPromise<bool> &result_promise,
                   IDatabase::RequestEntityInformationReceiver &receiver,
                   const RawEntityId &requested_id,
                   const FileLocationCache &file_location_cache,
                   const File &entity) {

  EntityInformation info;
  info.requested_id = requested_id;
  info.id = entity.id().Pack();
  info.entity.entity_role = entity;

  EntityInformationScopedSender info_auto_sender{info, receiver};

  for (std::filesystem::path path : entity.paths()) {
    info.entity.display_role.setValue(
        QString::fromStdString(path.generic_string()));
    break;
  }

  for (IncludeLikeMacroDirective inc : IncludeLikeMacroDirective::in(entity)) {
    if (result_promise.isCanceled()) {
      return;
    }

    if (std::optional<File> file = inc.included_file()) {
      EntityInformation::Selection &sel = info.includes.emplace_back();
      TokenRange tokens = inc.use_tokens();
      sel.entity_role = std::move(inc);
      sel.location = GetLocation(result_promise, tokens, file_location_cache);
      sel.display_role.setValue(tokens.strip_whitespace());
    }

    SendBatch(receiver, info);
  }

  for (Reference ref : entity.references()) {
    if (result_promise.isCanceled()) {
      return;
    }

    if (auto inc = IncludeLikeMacroDirective::from(ref.as_macro())) {
      if (auto file = File::containing(inc.value())) {
        TokenRange tokens = inc->use_tokens();
        EntityInformation::Selection &sel = info.include_bys.emplace_back();
        sel.location = GetLocation(result_promise, tokens, file_location_cache);
        sel.entity_role = inc.value();

        for (std::filesystem::path path : file->paths()) {
          sel.display_role.setValue(
              QString::fromStdString(path.generic_string()));
          break;
        }

        SendBatch(receiver, info);
      }
    }
  }

  for (Fragment frag : entity.fragments()) {
    for (DefineMacroDirective def : DefineMacroDirective::in(frag)) {
      if (result_promise.isCanceled()) {
        return;
      }

      EntityInformation::Selection &sel =
          info.top_level_entities.emplace_back();
      sel.display_role.setValue(def.name());
      sel.entity_role = def;
      sel.location =
          GetLocation(result_promise, def.use_tokens(), file_location_cache);

      SendBatch(receiver, info);
    }

    for (Decl decl : frag.top_level_declarations()) {
      if (result_promise.isCanceled()) {
        return;
      }

      if (auto nd = NamedDecl::from(decl); nd && !nd->name().empty()) {
        EntityInformation::Selection &sel =
            info.top_level_entities.emplace_back();
        sel.display_role.setValue(nd->token());
        sel.location =
            GetLocation(result_promise, nd->tokens(), file_location_cache);
        sel.entity_role = nd.value();
      }

      SendBatch(receiver, info);
    }
  }
}

//! Fill `info` with information about the macro `entity`.
static void
GetMacroInformation(QPromise<bool> &result_promise,
                    IDatabase::RequestEntityInformationReceiver &receiver,
                    const RawEntityId &requested_id,
                    const FileLocationCache &file_location_cache,
                    const DefineMacroDirective &entity) {

  EntityInformation info;
  info.requested_id = requested_id;
  info.id = entity.id().Pack();
  info.entity.display_role.setValue(entity.name());
  info.entity.location =
      GetLocation(result_promise, entity.use_tokens(), file_location_cache);
  info.entity.entity_role = entity;

  info.definitions.emplace_back(info.entity);

  EntityInformationScopedSender info_auto_sender{info, receiver};

  // Find the local variables.
  for (const MacroOrToken &mt : entity.parameters()) {
    if (result_promise.isCanceled()) {
      return;
    }

    SendBatch(receiver, info);

    if (!std::holds_alternative<Macro>(mt)) {
      continue;
    }

    const Macro &macro = std::get<Macro>(mt);
    std::optional<MacroParameter> mp = MacroParameter::from(macro);
    if (!mp) {
      continue;
    }

    EntityInformation::Selection *sel = &(info.parameters.emplace_back());
    TokenRange tokens = mp->use_tokens();
    if (Token name_tok = mp->name()) {
      if (entity.is_variadic()) {
        sel->display_role.setValue(tokens);
      } else {
        sel->display_role.setValue(name_tok);
      }

    } else if (entity.is_variadic()) {
      sel->display_role.setValue(QString("__VA_ARGS__"));
    }

    sel->entity_role = mp.value();
    sel->location = GetLocation(result_promise, tokens, file_location_cache);
  }

  for (Reference ref : entity.references()) {
    if (result_promise.isCanceled()) {
      return;
    }

    auto exp = MacroExpansion::from(ref.as_macro());
    if (!exp) {
      continue;
    }

    TokenRange tokens = exp->use_tokens();
    EntityInformation::Selection *sel = &(info.uses.emplace_back());
    sel->display_role.setValue(InjectWhitespace(tokens.strip_whitespace()));
    sel->entity_role = exp.value();
    sel->location = GetLocation(result_promise, tokens, file_location_cache);

    SendBatch(receiver, info);
  }
}

//! Fill `info` with information about the declaration `entity`.
static void
GetDeclInformation(QPromise<bool> &result_promise,
                   IDatabase::RequestEntityInformationReceiver &receiver,
                   const RawEntityId &requested_id,
                   const FileLocationCache &file_location_cache,
                   NamedDecl entity) {

  std::string_view name = entity.name();

  EntityInformation info;
  info.requested_id = requested_id;
  info.id = entity.id().Pack();
  info.entity.entity_role = entity;
  info.entity.location =
      GetLocation(result_promise, entity.tokens(), file_location_cache);

  EntityInformationScopedSender info_auto_sender{info, receiver};

  if (!name.empty()) {
    info.entity.display_role.setValue(entity.token());
  }

  // Fill all redeclarations.
  for (NamedDecl redecl : entity.redeclarations()) {
    if (result_promise.isCanceled()) {
      return;
    }

    EntityInformation::Selection *sel = nullptr;
    if (redecl.is_definition()) {
      sel = &(info.definitions.emplace_back());
    } else {
      sel = &(info.declarations.emplace_back());
    }
    // Collect all macros used by all redeclarations.
    FillUsedMacros(result_promise, receiver, file_location_cache, info,
                   redecl.tokens());

    sel->display_role.setValue(redecl.token());
    sel->entity_role = redecl;
    sel->location =
        GetLocation(result_promise, redecl.tokens(), file_location_cache);

    // Try to get a name from elsewhere if we're missing it.
    if (!info.entity.display_role.isValid()) {
      name = redecl.name();
      if (!name.empty()) {
        info.entity.display_role.setValue(redecl.token());
      }
    }

    SendBatch(receiver, info);
  }

  // If this is a function, then look at who it calls, and who calls it.
  if (auto func = FunctionDecl::from(entity)) {
    FillFunctionInformation(result_promise, receiver, file_location_cache, info,
                            std::move(func.value()));

  } else if (auto var = VarDecl::from(entity)) {
    FillVariableInformation(result_promise, receiver, file_location_cache, info,
                            std::move(var.value()));

  } else if (auto field = FieldDecl::from(entity)) {
    FillVariableInformation(result_promise, receiver, file_location_cache, info,
                            field.value());

  } else if (auto enumerator = EnumConstantDecl::from(entity)) {
    FillVariableInformation(result_promise, receiver, file_location_cache, info,
                            enumerator.value());

  } else if (auto enum_ = EnumDecl::from(entity)) {

    FillEnumInformation(result_promise, receiver, file_location_cache, info,
                        std::move(enum_.value()));

  } else if (auto tag = RecordDecl::from(entity)) {
    FillRecordInformation(result_promise, receiver, file_location_cache, info,
                          std::move(tag.value()));
  }

  if (auto type = TypeDecl::from(entity)) {
    FillTypeInformation(result_promise, receiver, file_location_cache, info,
                        std::move(type.value()));
  }

  SendBatch(receiver, info);
}

}  // namespace

void GetEntityInformation(QPromise<bool> &result_promise, const Index &index,
                          const FileLocationCache &file_location_cache,
                          IDatabase::RequestEntityInformationReceiver *receiver,
                          RawEntityId entity_id) {

  VariantEntity entity = index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    result_promise.addResult(false);
    return;
  }

  if (std::holds_alternative<Token>(entity)) {
    entity = std::get<Token>(entity).related_entity();
  }

  auto succeeded{false};
  if (std::holds_alternative<Decl>(entity)) {
    if (auto named = NamedDecl::from(std::get<Decl>(entity))) {
      GetDeclInformation(result_promise, *receiver, entity_id,
                         file_location_cache, named->canonical_declaration());
      succeeded = true;
    }

  } else if (std::holds_alternative<Macro>(entity)) {
    if (auto def = DefineMacroDirective::from(std::get<Macro>(entity))) {
      GetMacroInformation(result_promise, *receiver, entity_id,
                          file_location_cache, std::move(def.value()));
      succeeded = true;
    }

  } else if (std::holds_alternative<File>(entity)) {
    GetFileInformation(result_promise, *receiver, entity_id,
                       file_location_cache, std::move(std::get<File>(entity)));
    succeeded = true;
  }

  result_promise.addResult(succeeded);
}

}  // namespace mx::gui
