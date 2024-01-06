/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetEntityInformation.h"

#include <multiplier/GUI/Assert.h>
#include <multiplier/GUI/Util.h>

#include <multiplier/AST/ArraySubscriptExpr.h>
#include <multiplier/AST/BinaryOperator.h>
#include <multiplier/AST/CallExpr.h>
#include <multiplier/AST/CastExpr.h>
#include <multiplier/AST/ConditionalOperator.h>
#include <multiplier/AST/DoStmt.h>
#include <multiplier/AST/EnumConstantDecl.h>
#include <multiplier/AST/EnumDecl.h>
#include <multiplier/AST/ForStmt.h>
#include <multiplier/AST/FunctionDecl.h>
#include <multiplier/AST/IfStmt.h>
#include <multiplier/AST/MemberExpr.h>
#include <multiplier/AST/NamedDecl.h>
#include <multiplier/AST/RecordDecl.h>
#include <multiplier/AST/SwitchStmt.h>
#include <multiplier/AST/StorageDuration.h>
#include <multiplier/AST/ThreadStorageClassSpecifier.h>
#include <multiplier/AST/TypeTraitExpr.h>
#include <multiplier/AST/UnaryOperator.h>
#include <multiplier/AST/UnaryExprOrTypeTraitExpr.h>
#include <multiplier/AST/WhileStmt.h>

#include <multiplier/Frontend/DefineMacroDirective.h>
#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/IncludeLikeMacroDirective.h>
#include <multiplier/Frontend/MacroExpansion.h>
#include <multiplier/Frontend/MacroParameter.h>

#include <multiplier/Fragment.h>

#include <filesystem>
#include <iomanip>
#include <sstream>

namespace mx::gui {
namespace {

using DataBatch = IDatabase::RequestEntityInformationReceiver::DataBatch;

const std::size_t kBatchSize{256};

void SendBatch(IDatabase::RequestEntityInformationReceiver &receiver,
               DataBatch &batch, bool force = false) {
  if (!force && batch.size() < kBatchSize) {
    return;
  }

  DataBatch batch_to_send;
  batch_to_send.swap(batch);

  receiver.OnDataBatch(std::move(batch_to_send));
}

struct EntityInformationScopedSender final {
  DataBatch &batch;
  IDatabase::RequestEntityInformationReceiver &receiver;

  EntityInformationScopedSender(
      DataBatch &batch_,
      IDatabase::RequestEntityInformationReceiver &receiver_)
      : batch(batch_),
        receiver(receiver_) {}

  ~EntityInformationScopedSender() {
    SendBatch(receiver, batch, true);
  }
};

static std::optional<EntityLocation>
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
      return EntityLocation(file.value(), line_col->first,
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
               DataBatch &batch, const TokenRange &tokens) {
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
          EntityInformation &sel = batch.emplace_back();
          sel.category = QObject::tr("Macros used");
          sel.display_role.setValue(use_tok);
          sel.entity_role = std::move(exp.value());
          sel.location =
              GetLocation(result_promise, use_tok, file_location_cache);
          break;
        }
      }
      break;
    }

    SendBatch(receiver, batch);
  }
}

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
                    DataBatch &batch, const TypeDecl &entity,
                    Reference ref) {

  // TODO(pag): Do better with these.
  (void) entity;

  if (auto du = ref.as_declaration()) {
    EntityInformation *sel = &(batch.emplace_back());
    sel->category = QObject::tr("Declaration uses");
    sel->display_role.setValue(
        InjectWhitespace(du->tokens().strip_whitespace()));
    sel->entity_role = du.value();
    sel->location =
        GetLocation(result_promise, du->tokens(), file_location_cache);
    SendBatch(receiver, batch);

  } else if (auto su = ref.as_statement()) {
    Stmt orig_su = su.value();
    for (; su; su = su->parent_statement()) {
      if (result_promise.isCanceled()) {
        return;
      }


      if (CastExpr::from(su.value())) {
        EntityInformation *sel = &(batch.emplace_back());
        sel->category = QObject::tr("Type casts");
        sel->display_role.setValue(
            InjectWhitespace(su->tokens().strip_whitespace()));
        sel->entity_role = orig_su;
        sel->location =
            GetLocation(result_promise, su->tokens(), file_location_cache);
        SendBatch(receiver, batch);
        return;

      } else if (TypeTraitExpr::from(su.value()) ||
                 UnaryExprOrTypeTraitExpr::from(su.value())) {
        EntityInformation *sel = &(batch.emplace_back());
        sel->category = QObject::tr("Trait uses");
        sel->display_role.setValue(
            InjectWhitespace(su->tokens().strip_whitespace()));
        sel->entity_role = orig_su;
        sel->location =
            GetLocation(result_promise, su->tokens(), file_location_cache);
        SendBatch(receiver, batch);
        return;
      }
    }

    EntityInformation *sel = &(batch.emplace_back());
    sel->category = QObject::tr("Statement uses");
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
                    DataBatch &batch, TypeDecl entity) {

  for (Reference ref : Reference::to(entity)) {
    if (result_promise.isCanceled()) {
      return;
    }

    FillTypeInformation(result_promise, receiver, file_location_cache, batch,
                        entity, std::move(ref));
  }
}

//! Fill `info` with information about the variable `var` as used by the
//! statement `stmt`.
static void FillVariableUsedByStatementInformation(
    QPromise<bool> &result_promise,
    IDatabase::RequestEntityInformationReceiver &receiver,
    const FileLocationCache &file_location_cache, DataBatch &batch,
    Stmt stmt) {

  bool is_field = stmt.kind() == StmtKind::MEMBER_EXPR;
  Assert(is_field || stmt.kind() == StmtKind::DECL_REF_EXPR,
         "Unexpected user statement");

  EntityInformation *sel = nullptr;
  Stmt child = stmt;
  std::optional<Stmt> parent = child.parent_statement();
  for (; parent; child = parent.value(), parent = parent->parent_statement()) {
    if (result_promise.isCanceled()) {
      return;
    }

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
  sel = &(batch.emplace_back());
  sel->category = QObject::tr("Uses");
  sel->display_role.setValue(
      InjectWhitespace(child.tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  goto done;

argument_use:
  sel = &(batch.emplace_back());
  sel->category = QObject::tr("Call arguments");
  sel->display_role.setValue(
      InjectWhitespace(parent->tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  goto done;

assigned_to_use:
  sel = &(batch.emplace_back());
  sel->category = QObject::tr("Assigned tos");
  sel->display_role.setValue(
      InjectWhitespace(parent->tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  goto done;

assigned_from_use:
  sel = &(batch.emplace_back());
  sel->category = QObject::tr("Assignments");
  sel->display_role.setValue(
      InjectWhitespace(parent->tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  goto done;

address_of_use:
  sel = &(batch.emplace_back());
  sel->category = QObject::tr("Address ofs");
  sel->display_role.setValue(
      InjectWhitespace(parent->tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  goto done;

dereference_use:
  sel = &(batch.emplace_back());
  sel->category = QObject::tr("Dereferences");
  sel->display_role.setValue(
      InjectWhitespace(parent->tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  goto done;

conditional_use:
  sel = &(batch.emplace_back());
  sel->category = QObject::tr("Influencing condition");
  sel->display_role.setValue(
      InjectWhitespace(child.tokens().strip_whitespace()));
  sel->entity_role = stmt;
  sel->location =
      GetLocation(result_promise, stmt.tokens(), file_location_cache);
  goto done;

done:
  SendBatch(receiver, batch);
}

//! Fill `info` with information about the variable `var`.
static void
FillVariableInformation(QPromise<bool> &result_promise,
                        IDatabase::RequestEntityInformationReceiver &receiver,
                        const FileLocationCache &file_location_cache,
                        DataBatch &batch, const ValueDecl &var) {

  for (Reference ref : Reference::to(var)) {
    if (std::optional<Stmt> stmt = ref.as_statement()) {
      FillVariableUsedByStatementInformation(result_promise, receiver,
                                             file_location_cache, batch,
                                             std::move(stmt.value()));
    }
  }
}

//! Fill `info` with information about the function `func`.
static void
FillFunctionInformation(QPromise<bool> &result_promise,
                        IDatabase::RequestEntityInformationReceiver &receiver,
                        const FileLocationCache &file_location_cache,
                        DataBatch &batch, FunctionDecl func) {

  for (Reference ref : Reference::to(func)) {
    if (result_promise.isCanceled()) {
      return;
    }

    if (auto designator = ref.as_designator()) {
      TokenRange tokens = designator->tokens();
      EntityInformation &sel = batch.emplace_back();
      sel.category = QObject::tr("Address ofs");
      sel.display_role.setValue(InjectWhitespace(tokens.strip_whitespace()));
      sel.entity_role = designator.value();
      sel.location = GetLocation(result_promise, tokens, file_location_cache);
      SendBatch(receiver, batch);
      continue;
    }

    if (std::optional<Stmt> stmt = ref.as_statement()) {

      // Look for direct calls. This is replicating `FunctionDecl::callers`.
      for (CallExpr call : CallExpr::containing(stmt.value())) {
        if (result_promise.isCanceled()) {
          return;
        }

        if (std::optional<FunctionDecl> callee = call.direct_callee()) {
          if (callee.value() != func) {
            continue;
          }

          EntityInformation &sel = batch.emplace_back();
          sel.category = QObject::tr("Callers");
          sel.entity_role = call;
          sel.location =
              GetLocation(result_promise, call.tokens(), file_location_cache);
          for (FunctionDecl caller : FunctionDecl::containing(call)) {
            sel.display_role.setValue(caller.token());
            break;
          }

          SendBatch(receiver, batch);
          goto found;
        }
      }

      if (result_promise.isCanceled()) {
        return;
      }

      // If we didn't find a caller, then it's probably an `address_ofs`.
      {
        EntityInformation &sel = batch.emplace_back();
        sel.category = QObject::tr("Address ofs");
        sel.display_role.setValue(FindLine(result_promise, stmt.value()));
        sel.entity_role = stmt.value();
        sel.location =
            GetLocation(result_promise, stmt->tokens(), file_location_cache);
        SendBatch(receiver, batch);
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

      EntityInformation &sel = batch.emplace_back();
      sel.category = QObject::tr("Callees");
      sel.display_role.setValue(callee->token());
      sel.entity_role = call;
      sel.location =
          GetLocation(result_promise, call.tokens(), file_location_cache);
      SendBatch(receiver, batch);
      break;
    }
  }

  // Find the local variables.
  for (const mx::Decl &var : func.declarations_in_context()) {
    if (result_promise.isCanceled()) {
      return;
    }

    if (std::optional<VarDecl> vd = mx::VarDecl::from(var)) {
      EntityInformation &sel = batch.emplace_back();
      if (vd->kind() == DeclKind::PARM_VAR) {
        sel.category = QObject::tr("Parameters");
      } else {
        if (vd->tsc_spec() != ThreadStorageClassSpecifier::UNSPECIFIED) {
          sel.category = QObject::tr("Thread local variables");

        } else if (vd->storage_duration() == StorageDuration::STATIC) {
          sel.category = QObject::tr("Static local variables");

        } else {
          sel.category = QObject::tr("Local variables");
        }
      }
      sel.display_role.setValue(vd->token());
      sel.entity_role = vd.value();
      sel.location =
          GetLocation(result_promise, vd->tokens(), file_location_cache);
      SendBatch(receiver, batch);
    }
  }
}

//! Fill `info` with information about the variable `var`.
static void
FillEnumInformation(QPromise<bool> &result_promise,
                    IDatabase::RequestEntityInformationReceiver &receiver,
                    const FileLocationCache &file_location_cache,
                    DataBatch &batch, EnumDecl entity) {

  for (mx::EnumConstantDecl ec : entity.enumerators()) {
    if (result_promise.isCanceled()) {
      return;
    }

    EntityInformation &sel = batch.emplace_back();
    sel.category = QObject::tr("Enumerators");
    sel.display_role.setValue(ec.token());
    sel.entity_role = ec;
    sel.location =
        GetLocation(result_promise, ec.tokens(), file_location_cache);

    SendBatch(receiver, batch);
  }
}

static void FillTypeDeclInformation(DataBatch &batch, TypeDecl entity) {
  if (auto type = entity.type_for_declaration()) {
    if (auto size = type->size_in_bits()) {
      EntityInformation &sel = batch.emplace_back();
      sel.category = "Size";
      if (!(size.value() % 8u)) {
        sel.display_role = QObject::tr("Size %1 (bytes)").arg(size.value() / 8u);
      } else {
        sel.display_role = QObject::tr("Size %1 (bits)").arg(size.value());
      }
    }

    if (auto align = type->alignment()) {
      EntityInformation &sel = batch.emplace_back();
      sel.category = "Size";
      sel.display_role = QObject::tr("Alignment %1 (bytes)").arg(align.value());
    }
  }
}



//! Fill `info` with information about the declaration `entity`.
static void
GetDeclInformation(QPromise<bool> &result_promise,
                   IDatabase::RequestEntityInformationReceiver &receiver,
                   const FileLocationCache &file_location_cache,
                   NamedDecl entity) {

  DataBatch batch;
  EntityInformationScopedSender info_auto_sender{batch, receiver};

  entity = entity.canonical_declaration();

  // Fill all redeclarations.
  for (NamedDecl redecl : entity.redeclarations()) {
    if (result_promise.isCanceled()) {
      return;
    }

    EntityInformation &sel = batch.emplace_back();
    if (redecl.is_definition()) {
      sel.category = QObject::tr("Definitions");
    } else {
      sel.category = QObject::tr("Declarations");
    }

    sel.display_role.setValue(redecl.token());
    sel.entity_role = redecl;
    sel.location =
        GetLocation(result_promise, redecl.tokens(), file_location_cache);

    SendBatch(receiver, batch);

    // Collect all macros used by all redeclarations.
    FillUsedMacros(result_promise, receiver, file_location_cache, batch,
                   redecl.tokens());
  }

  if (auto type = TypeDecl::from(entity)) {
    FillTypeDeclInformation(batch, std::move(type.value()));
  }

  // If this is a function, then look at who it calls, and who calls it.
  if (auto func = FunctionDecl::from(entity)) {
    FillFunctionInformation(result_promise, receiver, file_location_cache,
                            batch, std::move(func.value()));

  } else if (auto var = VarDecl::from(entity)) {
    FillVariableInformation(result_promise, receiver, file_location_cache,
                            batch, std::move(var.value()));

  } else if (auto field = FieldDecl::from(entity)) {
    FillVariableInformation(result_promise, receiver, file_location_cache,
                            batch, field.value());

  } else if (auto enumerator = EnumConstantDecl::from(entity)) {
    FillVariableInformation(result_promise, receiver, file_location_cache,
                            batch, enumerator.value());

  } else if (auto enum_ = EnumDecl::from(entity)) {

    FillEnumInformation(result_promise, receiver, file_location_cache,
                        batch, std::move(enum_.value()));

  } else if (auto tag = RecordDecl::from(entity)) {
    FillRecordInformation(result_promise, receiver, file_location_cache,
                          batch, std::move(tag.value()));
  }

  if (auto type = TypeDecl::from(entity)) {
    FillTypeInformation(result_promise, receiver, file_location_cache, batch,
                        std::move(type.value()));
  }

  SendBatch(receiver, batch);
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
      GetDeclInformation(result_promise, *receiver, file_location_cache,
                         named->canonical_declaration());
      succeeded = true;
    }

  } else if (std::holds_alternative<Macro>(entity)) {
    if (auto def = DefineMacroDirective::from(std::get<Macro>(entity))) {
      GetMacroInformation(result_promise, *receiver, file_location_cache,
                          std::move(def.value()));
      succeeded = true;
    }

  } else if (std::holds_alternative<File>(entity)) {
    GetFileInformation(result_promise, *receiver, file_location_cache,
                       std::move(std::get<File>(entity)));
    succeeded = true;
  }

  result_promise.addResult(succeeded);
}

}  // namespace mx::gui
