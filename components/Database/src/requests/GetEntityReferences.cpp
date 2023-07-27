/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetEntityName.h"

#include <multiplier/ui/Util.h>
#include <multiplier/ui/Assert.h>

#include <multiplier/Compilation.h>
#include <multiplier/Entities/Token.h>
#include <multiplier/Entities/TokenKind.h>

#include <functional>

namespace mx::gui {

namespace {

const std::size_t kBatchSize{1024};

struct EntityQueueEntry final {
  std::uint64_t parent_node_id{};
  VariantEntity entity;
};

using EntityQueue = std::vector<EntityQueueEntry>;

VariantEntity NamedEntityContaining(const VariantEntity &entity,
                                    const VariantEntity &containing) {
  if (std::holds_alternative<Decl>(entity)) {

    if (auto contained_decl = std::get_if<Decl>(&containing);
        contained_decl && TypeDecl::from(*contained_decl)) {

      if (auto nd = NamedDecl::from(std::get<Decl>(entity))) {
        return nd->canonical_declaration();
      }
    }

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

    // TODO(pag): Do token-based lookup?

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
    Macro macro = std::move(std::get<Macro>(entity)).root();

    for (Token tok : macro.generate_expansion_tokens()) {
      if (Token pt = tok.parsed_token()) {
        //        std::cerr << "PT " << pt.id() << ' ' << pt.data() << '\n';
        if (auto nd = NamedDeclContaining(pt);
            !std::holds_alternative<NotAnEntity>(nd)) {
          return nd;
        }
      } else {
        //        std::cerr << "ET " << tok.id() << ' ' << tok.data() << '\n';
      }
    }

    // If the macro wasn't used inside of a decl/statement, then go try to
    // find the macro definition containing this macro.
    if (auto dd = DefineMacroDirective::from(macro)) {
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
      if (auto ne = NamedEntityContaining(std::move(m), containing);
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

// Generate references to the entity with `entity`. The references
// pairs of named entities and the referenced entity. Sometimes the
// referenced entity will match the named entity, other times the named
// entity will contain the reference (e.g. a function containing a call).
void EnumerateEntityReferences(
    QPromise<bool> &result_promise, const VariantEntity &entity,
    const std::function<bool(const VariantEntity &, const VariantEntity &)>
        &callback) {

  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;

#define REFERENCES_TO_CATEGORY(type_name, lower_name, enum_name, category) \
  } \
  else if (std::holds_alternative<type_name>(entity)) { \
    if (result_promise.isCanceled()) { \
      return; \
    } \
\
    for (Reference ref : std::get<type_name>(entity).references()) { \
      if (result_promise.isCanceled()) { \
        return; \
      } \
      VariantEntity rd = ref.as_variant(); \
      VariantEntity nd = NamedEntityContaining(rd, entity); \
      if (!std::holds_alternative<NotAnEntity>(nd)) { \
        if (!callback(nd, rd)) { \
          return; \
        } \
      } \
    }

    MX_FOR_EACH_ENTITY_CATEGORY(REFERENCES_TO_CATEGORY, REFERENCES_TO_CATEGORY,
                                REFERENCES_TO_CATEGORY,
                                MX_IGNORE_ENTITY_CATEGORY,
                                REFERENCES_TO_CATEGORY, REFERENCES_TO_CATEGORY,
                                MX_IGNORE_ENTITY_CATEGORY)

  } else {
    return;
  }
}

std::optional<IDatabase::QueryEntityReferencesResult::Node::Location>
GetEntityLocation(const FileLocationCache &file_cache,
                  const VariantEntity &entity) {

  Token file_tok = FirstFileToken(entity).file_token();
  if (!file_tok) {
    return std::nullopt;
  }

  std::optional<File> file = File::containing(file_tok);
  if (!file.has_value()) {
    Assert(file.has_value(), "Token::file_token returned non-file token?");
    return std::nullopt;
  }

  IDatabase::QueryEntityReferencesResult::Node::Location location;
  location.file_id = file->id().Pack();

  for (std::filesystem::path path : file->paths()) {
    location.path = QString::fromStdString(path.generic_string());
  }

  Assert(!location.path.isEmpty(), "Empty file paths aren't allowed");

  if (auto line_col = file_tok.location(file_cache)) {
    location.line = line_col->first;
    location.column = line_col->second;
  }

  return location;
}

std::size_t GetBatchSize(const IDatabase::QueryEntityReferencesResult &result) {
  return result.node_list.size();
}

void SendBatch(IDatabase::QueryEntityReferencesReceiver &receiver,
               IDatabase::QueryEntityReferencesResult &result,
               bool force = false) {

  if (!force && GetBatchSize(result) < kBatchSize) {
    return;
  }

  IDatabase::QueryEntityReferencesReceiver::DataBatch data_batch{result};
  receiver.OnDataBatch(data_batch);

  result.node_list.clear();
}

void GetEntityCallReferences(QPromise<bool> &result_promise,
                             const FileLocationCache &file_location_cache,
                             IDatabase::QueryEntityReferencesReceiver *receiver,
                             IDatabase::QueryEntityReferencesResult result,
                             EntityQueue next_entity_queue, std::size_t depth) {

  std::uint64_t node_id_generator{1};

  while (!next_entity_queue.empty()) {
    if (depth == 0) {
      break;
    }

    --depth;

    auto entity_queue = std::move(next_entity_queue);
    next_entity_queue.clear();

    for (const auto &entity_queue_entry : entity_queue) {
      if (result_promise.isCanceled()) {
        break;
      }

      auto current_parent_node_id = entity_queue_entry.parent_node_id;

      // Get the references, and append them under the parent node we just created
      EnumerateEntityReferences(
          result_promise, entity_queue_entry.entity,

          [&](const VariantEntity &entity,
              const VariantEntity &referenced_entity) -> bool {
            if (result_promise.isCanceled()) {
              return false;
            }

            auto entity_id = IdOfEntity(entity);

            IDatabase::QueryEntityReferencesResult::Node node;
            node.entity_id = entity_id;
            node.referenced_entity_id = IdOfEntity(referenced_entity);
            node.opt_name = NameOfEntityAsString(entity);

            node.opt_location =
                GetEntityLocation(file_location_cache, referenced_entity);

            if (!node.opt_location.has_value()) {
              node.opt_location =
                  GetEntityLocation(file_location_cache, entity);
            }

            node.opt_breadcrumbs = EntityBreadCrumbs(referenced_entity);

            node.mapping_info.parent_node_id = current_parent_node_id;
            node.mapping_info.node_id = ++node_id_generator;

            EntityQueueEntry entity_queue_entry;
            entity_queue_entry.entity = entity;
            entity_queue_entry.parent_node_id = node.mapping_info.node_id;
            next_entity_queue.push_back(std::move(entity_queue_entry));

            result.node_list.push_back(std::move(node));

            SendBatch(*receiver, result);
            return true;
          });
    }
  }

  SendBatch(*receiver, result, true);
  result_promise.addResult(true);
}

}  // namespace

void GetEntityReferences(QPromise<bool> &result_promise, const Index &index,
                         const FileLocationCache &file_location_cache,
                         IDatabase::QueryEntityReferencesReceiver *receiver,
                         RawEntityId entity_id,
                         IDatabase::ReferenceType reference_type,
                         bool include_redeclarations, bool emit_root_node,
                         std::size_t depth) {

  VariantEntity entity = index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    result_promise.addResult(false);
    return;
  }

  // Generate the root node
  IDatabase::QueryEntityReferencesResult result;
  std::uint64_t parent_root_node_id{};

  if (emit_root_node) {
    IDatabase::QueryEntityReferencesResult::Node entity_root_node;
    entity_root_node.opt_name = NameOfEntityAsString(entity);
    entity_root_node.entity_id = entity_root_node.referenced_entity_id =
        IdOfEntity(entity);
    entity_root_node.opt_location =
        GetEntityLocation(file_location_cache, entity);

    if (!entity_root_node.opt_location.has_value()) {
      entity_root_node.opt_location =
          GetEntityLocation(file_location_cache, entity);
    }

    entity_root_node.opt_breadcrumbs = EntityBreadCrumbs(entity);
    entity_root_node.mapping_info.node_id = 1;
    entity_root_node.mapping_info.parent_node_id = 0;

    result.node_list.push_back(std::move(entity_root_node));

    parent_root_node_id = 1;
  }

  // Build the entity queue
  EntityQueue entity_queue;
  if (include_redeclarations && std::holds_alternative<Decl>(entity)) {
    for (Decl decl : std::get<Decl>(entity).redeclarations()) {
      EntityQueueEntry entity_queue_entry;
      entity_queue_entry.parent_node_id = parent_root_node_id;
      entity_queue_entry.entity = VariantEntity(std::move(decl));

      entity_queue.push_back(std::move(entity_queue_entry));
    }

  } else {
    EntityQueueEntry entity_queue_entry;
    entity_queue_entry.parent_node_id = parent_root_node_id;
    entity_queue_entry.entity = entity;

    entity_queue.push_back(std::move(entity_queue_entry));
  }

  if (reference_type == IDatabase::ReferenceType::Callers) {
    GetEntityCallReferences(result_promise, file_location_cache, receiver,
                            std::move(result), entity_queue, depth);
  }
}

}  // namespace mx::gui
