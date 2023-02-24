/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "NodeImporter.h"
#include "Utils.h"

#include <multiplier/Entities/CXXBaseSpecifier.h>
#include <multiplier/Entities/TemplateArgument.h>
#include <multiplier/Entities/TemplateParameterList.h>
#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>
#include <multiplier/Entities/NamedDecl.h>
#include <multiplier/File.h>

#include <multiplier/ui/Assert.h>

namespace mx::gui {

namespace {

//template <typename EntityType>
//mx::Decl DeclEntityFromEntityTokens(EntityType entity) {
//  static_assert(std::is_same<decltype(entity), Designator>::value ||
//                std::is_same<decltype(entity), Attr>::value,
//
//                "EntityType must be either a Designator or an Attr type");
//
//  std::optional<mx::Decl> opt_decl;
//
//  for (Token tok : entity.tokens()) {
//    auto declaration_list = mx::Decl::containing(tok);
//
//    auto declaration_list_it = declaration_list.begin();
//    if (declaration_list_it == declaration_list.end()) {
//      continue;
//    }
//
//    opt_decl = *declaration_list_it;
//    break;
//  }
//
//  Assert(opt_decl.has_value(), "Failed to import the attribute entity");
//  return opt_decl.value();
//}

//std::optional<mx::Decl> GetDeclEntity(VariantEntity entity_var) {
//  std::optional<mx::Decl> opt_decl;
//
//  const auto VariantEntityVisitor = Overload{
//      [&](Decl decl) { opt_decl = decl; },
//
//      [&](Stmt stmt) {
//        auto declaration_list = mx::Decl::containing(stmt);
//
//        auto declaration_list_it = declaration_list.begin();
//        Assert(declaration_list_it != declaration_list.end(),
//               "Failed to import the statement entity");
//
//        opt_decl = *declaration_list_it;
//      },
//
//      [&](Attr attr) { opt_decl = DeclEntityFromEntityTokens(attr); },
//
//      [&](Designator designator) {
//        opt_decl = DeclEntityFromEntityTokens(designator);
//      },
//
//      [&](Token token) {
//        auto statement_list = mx::Stmt::containing(token);
//        auto statement_list_it = statement_list.begin();
//
//        auto declaration_list = mx::Decl::containing(token);
//        auto declaration_list_it = declaration_list.begin();
//
//        if (statement_list_it != statement_list.end()) {
//          opt_decl = GetDeclEntity(*statement_list_it);
//
//        } else if (declaration_list_it != declaration_list.end()) {
//          opt_decl = GetDeclEntity(*declaration_list_it);
//        }
//      },
//
//      [](auto) {},
//  };
//
//  std::visit(VariantEntityVisitor, entity_var);
//  return opt_decl;
//}
//
//std::optional<mx::Decl> GetDeclEntity(const NodeImporter::IndexData &index_data,
//                                      const RawEntityId &entity_id) {
//
//  auto entity_var = index_data.index.entity(entity_id);
//  return GetDeclEntity(entity_var);
//}

}  // namespace

struct NodeImporter::PrivateData {
  PrivateData(mx::Index index, mx::FileLocationCache file_location_cache,
              NodeTree &node_tree_)
      : node_tree(node_tree_) {

    node_tree = {};
    node_tree.node_map.insert({0, NodeTree::Node{}});

    index_data.index = index;
    index_data.file_location_cache = file_location_cache;

    for (auto [path, id] : index.file_paths()) {
      index_data.file_path_map.emplace(
          id, QString::fromStdString(path.generic_string()));
    }
  }

  IndexData index_data;
  NodeTree &node_tree;
};

NodeImporter::NodeImporter(mx::Index index,
                           mx::FileLocationCache file_location_cache,
                           NodeTree &node_tree)
    : d(new PrivateData(index, file_location_cache, node_tree)) {}

NodeImporter::~NodeImporter() {}

bool NodeImporter::ImportEntity(
    RawEntityId entity_id, RawEntityId referenced_entity_id,
    NodeTree::Node::ImportMode import_mode,
    std::optional<std::uint64_t> opt_parent_node_id,
    std::optional<std::size_t> opt_max_depth) {

  Assert(import_mode == NodeTree::Node::ImportMode::CallHierarchy,
         "Invalid import mode");

  auto prev_node_tree_size = d->node_tree.node_map.size();

  ImportEntity(d->node_tree, d->index_data, opt_parent_node_id.value_or(0),
               entity_id, referenced_entity_id, opt_max_depth);

  return prev_node_tree_size != d->node_tree.node_map.size();
}

void NodeImporter::ExpandNode(std::uint64_t node_id,
                              std::optional<std::size_t> opt_max_depth) {
  ExpandNode(d->node_tree, d->index_data, node_id, opt_max_depth);
}

// Return the name of an entity.
static std::optional<QString> NameOfEntity(
    const VariantEntity &ent,
    const std::unordered_map<PackedFileId, QString> &file_paths) {

  const auto VariantEntityVisitor = Overload{
    [] (const Decl &decl) -> std::optional<QString> {
      if (auto named = NamedDecl::from(decl)) {
        auto name = named->name();
        return QString::fromUtf8(
            name.data(), static_cast<qsizetype>(name.size()));
      }
      return std::nullopt;
    },

    [] (const Macro &macro) -> std::optional<QString> {
      if (auto named = DefineMacroDirective::from(macro)) {
        auto name = named->name().data();
        return QString::fromUtf8(
            name.data(), static_cast<qsizetype>(name.size()));
      }
      return std::nullopt;
    },
    [&file_paths] (const File &file) -> std::optional<QString> {
      if (auto it = file_paths.find(file.id()); it != file_paths.end()) {
        return it->second;
      }
      return std::nullopt;
    },

    [] (auto) -> std::optional<QString> { return std::nullopt; }
  };
  return std::visit<std::optional<QString>>(VariantEntityVisitor, ent);
}

// Get the file token associated with an entity.
//
// NOTE(pag): We prefer `TokenRange::file_tokens` as that walks up macros.
static Token FirstFileToken(const VariantEntity &ent) {
  const auto VariantEntityVisitor = Overload{
    [] (const Decl &entity) { return entity.tokens().file_tokens().front(); },
    [] (const Stmt &entity) { return entity.tokens().file_tokens().front(); },
    [] (const Type &) { return Token(); },

    // Find the containing file usage of this, not necessarily the derived filed
    // token.
    [] (const Token &entity) {
      return TokenRange(entity).file_tokens().front();
    },

    [] (const Macro &entity) {
      for (Token tok : entity.tokens_covering_use()) {
        return tok.file_token();
      }
      return Token();
    },
    [] (const Designator &entity) {
      return entity.tokens().file_tokens().front();
    },
    [] (const CXXBaseSpecifier &entity) {
      return entity.tokens().file_tokens().front();
    },
    [] (const TemplateArgument &) {
      return Token();
    },
    [] (const TemplateParameterList &entity) {
      return entity.tokens().file_tokens().front();
    },

    // NOTE(pag): We don't do `entity.parsed_tokens().file_tokens()` because
    //            if it's a pure macro fragment, then it might not have any
    //            parsed tokens.
    [] (const Fragment &entity) {
      return entity.file_tokens().front();
    },
    [] (const File &entity) { return entity.tokens().front(); },
    [](auto) { return Token(); }
  };
  return std::visit<Token>(VariantEntityVisitor, ent);
}

//static std::optional<File> FileOfEntity(VariantEntity ent) {
//  const auto VariantEntityVisitor = Overload{
//    [] (const Decl &entity) { return File::containing(entity); },
//    [] (const Stmt &entity) { return File::containing(entity); },
//    [] (const Type &entity) { return File::containing(entity); },
//    [] (const Token &entity) { return File::containing(entity); },
//    [] (const Macro &entity) { return File::containing(entity); },
//    [] (const Designator &entity) { return File::containing(entity); },
//    [] (const CXXBaseSpecifier &entity) { return File::containing(entity); },
//    [] (const TemplateArgument &entity) { return File::containing(entity); },
//    [] (const TemplateParameterList &entity) { return File::containing(entity); },
//    [] (const Fragment &entity) { return File::containing(entity); },
//    [] (const File &entity) { return entity; },
//    [](auto) { return std::nullopt; }
//  };
//  return std::visit(VariantEntityVisitor, ent);
//}

void NodeImporter::ImportEntity(NodeTree &node_tree,
                                const IndexData &index_data,
                                std::uint64_t parent_node_id,
                                RawEntityId entity_id,
                                RawEntityId referenced_entity_id,
                                std::optional<std::size_t> opt_max_depth) {


  // TODO(alessandro): Some kind of thing to show that we have a repeat? This
  //                   should maybe be a custom row that points to the old
  //                   entry with a button of some kind.
  if (!node_tree.visited_entity_id_set.insert(referenced_entity_id).second) {
    return;
  }

  VariantEntity entity = index_data.index.entity(entity_id);
  VariantEntity referenced_entity =
      index_data.index.entity(referenced_entity_id);

  if (std::holds_alternative<NotAnEntity>(entity) ||
      std::holds_alternative<NotAnEntity>(referenced_entity)) {
    return;
  }

  auto current_node_id = node_tree.node_map.size();

  NodeTree::Node cn;
  cn.node_id = current_node_id;
  cn.parent_node_id = parent_node_id;
  cn.entity_id = entity_id;
  cn.referenced_entity_id = referenced_entity_id;
  cn.opt_name = NameOfEntity(entity, index_data.file_path_map);

  // Get us the location of the referenced entity.
  auto ref_tok = FirstFileToken(referenced_entity);
  if (!ref_tok) {
    ref_tok = FirstFileToken(entity);
  }
  if (Token file_tok = ref_tok.file_token()) {
    auto file = File::containing(file_tok);
    Assert(file.has_value(), "Token::file_token returned non-file token?");

    IReferenceExplorerModel::Location location;
    location.file_id = file->id().Pack();

    auto file_path_map_it = index_data.file_path_map.find(file->id());
    Assert(file_path_map_it != index_data.file_path_map.end(),
           "Invalid path id");

    location.path = file_path_map_it->second;
    if (auto line_col = ref_tok.location(index_data.file_location_cache)) {
      location.line = line_col->first;
      location.column = line_col->second;
    }

    cn.opt_location = std::move(location);
  }

  NodeTree::Node &parent_node = node_tree.node_map.at(parent_node_id);
  parent_node.child_node_id_list.push_back(current_node_id);

  node_tree.node_map.insert({current_node_id, std::move(cn)});

  ExpandNode(node_tree, index_data, current_node_id, opt_max_depth);
}

void NodeImporter::ExpandNode(NodeTree &node_tree, const IndexData &index_data,
                              std::uint64_t node_id,
                              std::optional<std::size_t> opt_max_depth) {

  if (opt_max_depth.has_value()) {
    auto &max_depth = opt_max_depth.value();
    if (max_depth == 0) {
      return;
    }

    --max_depth;
  }

  auto node_it = node_tree.node_map.find(node_id);
  if (node_it == node_tree.node_map.end()) {
    return;
  }

  const NodeTree::Node &node = node_it->second;
  for (const auto &p : References(index_data.index.entity(node.entity_id))) {
    RawEntityId entity_id = p.first;
    RawEntityId referenced_entity_id = p.second.referenced_entity_id().Pack();
    if (entity_id == kInvalidEntityId ||
        referenced_entity_id == kInvalidEntityId) {
      continue;
    }

    auto child_node_id_it = std::find_if(
        node.child_node_id_list.begin(), node.child_node_id_list.end(),

        [=, &node_tree](const std::uint64_t &child_node_id) -> bool {
          const NodeTree::Node &child_node =
              node_tree.node_map.at(child_node_id);
          return (child_node.referenced_entity_id == referenced_entity_id);
        });

    if (child_node_id_it != node.child_node_id_list.end()) {
      continue;
    }

    ImportEntity(node_tree, index_data, node_id, entity_id,
                 referenced_entity_id, opt_max_depth);
  }
}

}  // namespace mx::gui
