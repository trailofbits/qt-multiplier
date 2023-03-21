/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TaintedChildGenerator.h"
#include "Utils.h"

#include <multiplier/File.h>
#include <multiplier/Index.h>
#include <multiplier/Analysis/IRAnalysis.h>
#include <multiplier/Analysis/Taint.h>
#include <multiplier/Entities/StmtKind.h>
#include <multiplier/Fragment.h>
#include <multiplier/SourceIR.h>

namespace mx::gui {

namespace {

// Compressed version of the taint tracker that tries to eliminate implicit
// things.
static gap::generator<TaintTrackingResult> Taint(TaintTracker &tt,
                                                 VariantEntity entity) {
  for (TaintTrackingResult res : tt.add_source(entity)) {
    if (std::holds_alternative<TaintTrackingStep>(res)) {
      if (auto stmt = std::get<TaintTrackingStep>(res).as_statement()) {
        switch (stmt->kind()) {
          case StmtKind::IMPLICIT_CAST_EXPR:
          case StmtKind::IMPLICIT_VALUE_INIT_EXPR:
            for (auto sub_res : Taint(tt, std::move(stmt.value()))) {
              co_yield sub_res;
            }
            break;
          default: co_yield res; break;
        }
      } else {
        co_yield res;
      }
    } else {
      co_yield res;
    }
  }
}

// Compressed version of the taint tracker that tries to eliminate implicit
// things.
static gap::generator<Node> Taint(const FileLocationCache &file_cache,
                                  const SourceIR &ir, DependencyAnalysis &tt,
                                  MLIROperationPtr op) {
  for (DependencyTrackingResult res : tt.dependents(op)) {

    if (std::holds_alternative<DependencyTrackingStep>(res)) {
      DependencyTrackingStep &step = std::get<DependencyTrackingStep>(res);
      VariantEntity ent = step.as_variant(ir);
      if (std::holds_alternative<NotAnEntity>(ent)) {
        for (auto node : Taint(file_cache, ir, tt, step.as_operation())) {
          co_yield node;
        }
      }

      if (std::holds_alternative<Stmt>(ent)) {
        switch (std::get<Stmt>(ent).kind()) {
          case StmtKind::IMPLICIT_CAST_EXPR:
          case StmtKind::IMPLICIT_VALUE_INIT_EXPR:
            for (auto node : Taint(file_cache, ir, tt, step.as_operation())) {
              co_yield node;
            }
            continue;
          default: break;
        }
      }

      Node node;
      node.expansion_mode = IReferenceExplorerModel::TaintMode;
      node.referenced_entity_id = EntityId(ent).Pack();
      node.entity_id = node.referenced_entity_id;
      node.opt_location = Location::Create(file_cache, ent);

      if (QString tokens = TokensToString(ent); !tokens.isEmpty()) {
        node.opt_name = tokens;
      }
      co_yield node;

    } else if (std::holds_alternative<DependencyTrackingSink>(res)) {
      DependencyTrackingSink &sink = std::get<DependencyTrackingSink>(res);
      VariantEntity ent = sink.as_variant(ir);
      if (!std::holds_alternative<NotAnEntity>(ent)) {
        ent = ir.entity_for(op);
      }
      if (!std::holds_alternative<NotAnEntity>(ent)) {
        continue;
      }

      Node node;
      node.expansion_mode = IReferenceExplorerModel::TaintMode;
      node.expanded = true;
      node.opt_name = QString::fromStdString(sink.message());
      node.referenced_entity_id = EntityId(ent).Pack();
      node.opt_location = Location::Create(file_cache, ent);
      co_yield node;
    }
  }
}

}  // namespace

struct TaintedChildGenerator::PrivateData {
  Index index;
  FileLocationCache file_cache;
  const RawEntityId entity_id;

  inline PrivateData(const Index &index_, const FileLocationCache &file_cache_,
                     RawEntityId entity_id_)
      : index(index_),
        file_cache(file_cache_),
        entity_id(entity_id_) {}
};

TaintedChildGenerator::~TaintedChildGenerator(void) {}

TaintedChildGenerator::TaintedChildGenerator(
    const Index &index_, const FileLocationCache &file_cache_,
    RawEntityId entity_id_, const QModelIndex &parent_)
    : INodeGenerator(parent_),
      d(new PrivateData(index_, file_cache_, entity_id_)) {}

VariantEntity TaintedChildGenerator::Entity(void) const {
  return d->index.entity(d->entity_id);
}

const FileLocationCache &TaintedChildGenerator::FileCache(void) const {
  return d->file_cache;
}

namespace {
static gap::generator<Node> NoTaints(void) {
  co_return;
}
}  // namespace

gap::generator<Node>
TaintedChildGenerator::GenerateNodesAST(VariantEntity entity) {

  TaintTracker tt(d->index);

  for (const TaintTrackingResult &res : Taint(tt, std::move(entity))) {
    Node node;

    if (std::holds_alternative<TaintTrackingSink>(res)) {
      const TaintTrackingSink &sink = std::get<TaintTrackingSink>(res);
      node.expansion_mode = IReferenceExplorerModel::TaintMode;
      node.expanded = true;
      node.opt_name = QString::fromStdString(sink.message());
      node.referenced_entity_id = d->entity_id;
      node.opt_location = Location::Create(d->file_cache, sink.as_variant());

    } else if (std::holds_alternative<TaintTrackingStep>(res)) {
      const TaintTrackingStep &step = std::get<TaintTrackingStep>(res);
      node.expansion_mode = IReferenceExplorerModel::TaintMode;
      node.referenced_entity_id = step.id().Pack();
      node.entity_id = node.referenced_entity_id;
      node.opt_location = Location::Create(d->file_cache, step.as_variant());

      if (QString tokens = TokensToString(step.as_variant());
          !tokens.isEmpty()) {
        node.opt_name = tokens;
      }

    } else {
      continue;
    }

    node.AssignUniqueId();
    co_yield node;
  }
}

gap::generator<Node> TaintedChildGenerator::GenerateNodes(void) {
  VariantEntity entity = Entity();
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return NoTaints();
  }

  if (std::optional<Fragment> frag = Fragment::containing(entity)) {
    if (std::optional<SourceIR> ir = frag->ir()) {
      if (OperationRange op = ir->for_entity(entity)) {
        DependencyAnalysis tt(d->index);
        return Taint(d->file_cache, ir.value(), tt, op.value());
      }
    }
  }

  return GenerateNodesAST(std::move(entity));
}

}  // namespace mx::gui
