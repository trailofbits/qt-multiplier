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
