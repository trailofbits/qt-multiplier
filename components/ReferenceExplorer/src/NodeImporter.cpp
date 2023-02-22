/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "NodeImporter.h"
#include "Utils.h"

#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>

#include <multiplier/ui/Assert.h>

namespace mx::gui {

namespace {

template <typename EntityType>
mx::Decl DeclEntityFromEntityTokens(EntityType entity) {
  static_assert(std::is_same<decltype(entity), Designator>::value ||
                    std::is_same<decltype(entity), Attr>::value,

                "EntityType must be either a Designator or an Attr type");

  std::optional<mx::Decl> opt_decl;

  for (Token tok : entity.tokens()) {
    auto declaration_list = mx::Decl::containing(tok);

    auto declaration_list_it = declaration_list.begin();
    if (declaration_list_it == declaration_list.end()) {
      continue;
    }

    opt_decl = *declaration_list_it;
    break;
  }

  Assert(opt_decl.has_value(), "Failed to import the attribute entity");
  return opt_decl.value();
}

std::optional<mx::Decl> GetDeclEntity(VariantEntity entity_var) {
  std::optional<mx::Decl> opt_decl;

  const auto VariantEntityVisitor = Overload{
      [&](Decl decl) { opt_decl = decl; },

      [&](Stmt stmt) {
        auto declaration_list = mx::Decl::containing(stmt);

        auto declaration_list_it = declaration_list.begin();
        Assert(declaration_list_it != declaration_list.end(),
               "Failed to import the statement entity");

        opt_decl = *declaration_list_it;
      },

      [&](Attr attr) { opt_decl = DeclEntityFromEntityTokens(attr); },

      [&](Designator designator) {
        opt_decl = DeclEntityFromEntityTokens(designator);
      },

      [&](Token token) {
        auto statement_list = mx::Stmt::containing(token);
        auto statement_list_it = statement_list.begin();

        auto declaration_list = mx::Decl::containing(token);
        auto declaration_list_it = declaration_list.begin();

        if (statement_list_it != statement_list.end()) {
          opt_decl = GetDeclEntity(*statement_list_it);

        } else if (declaration_list_it != declaration_list.end()) {
          opt_decl = GetDeclEntity(*declaration_list_it);
        }
      },

      [](auto) {},
  };

  std::visit(VariantEntityVisitor, entity_var);
  return opt_decl;
}

std::optional<mx::Decl> GetDeclEntity(const NodeImporter::IndexData &index_data,
                                      const RawEntityId &entity_id) {

  auto entity_var = index_data.index.entity(entity_id);
  return GetDeclEntity(entity_var);
}

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
      index_data.file_path_map.emplace(id, std::move(path));
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
    RawEntityId entity_id, const NodeTree::Node::ImportMode &import_mode,
    const std::optional<std::uint64_t> opt_parent_node_id,
    std::optional<std::size_t> opt_max_depth) {

  Assert(import_mode == NodeTree::Node::ImportMode::CallHierarchy,
         "Invalid import mode");

  auto prev_node_tree_size = d->node_tree.node_map.size();

  ImportEntity(d->node_tree, d->index_data, opt_parent_node_id.value_or(0),
               entity_id, opt_max_depth);

  return prev_node_tree_size != d->node_tree.node_map.size();
}

void NodeImporter::ExpandNode(const std::uint64_t &node_id,
                              std::optional<std::size_t> opt_max_depth) {
  ExpandNode(d->node_tree, d->index_data, node_id, opt_max_depth);
}

void NodeImporter::ImportEntity(NodeTree &node_tree,
                                const IndexData &index_data,
                                const std::uint64_t &parent_node_id,
                                const RawEntityId &entity_id,
                                std::optional<std::size_t> opt_max_depth) {

  auto opt_decl = GetDeclEntity(index_data, entity_id);
  if (!opt_decl.has_value()) {
    return;
  }

  return ImportEntity(node_tree, index_data, parent_node_id, opt_decl.value(),
                      opt_max_depth);
}

void NodeImporter::ImportEntity(NodeTree &node_tree,
                                const IndexData &index_data,
                                const std::uint64_t &parent_node_id,
                                mx::Decl decl,
                                std::optional<std::size_t> opt_max_depth) {

  auto decl_entity_id = decl.id().Pack();

  auto insert_status = node_tree.visited_entity_id_set.insert(decl_entity_id);
  if (!insert_status.second) {
    return;
  }

  auto current_node_id = node_tree.node_map.size();

  NodeTree::Node current_node;
  current_node.node_id = current_node_id;
  current_node.parent_node_id = parent_node_id;

  if (auto named = mx::NamedDecl::from(decl)) {
    current_node.opt_name = named->name();
  }

  current_node.entity_id = decl_entity_id;

  if (auto file = mx::File::containing(decl)) {
    IReferenceExplorerModel::Location location;
    location.file_id = file->id().Pack();

    auto file_path_map_it = index_data.file_path_map.find(file->id());
    Assert(file_path_map_it != index_data.file_path_map.end(),
           "Invalid path id");

    location.path = file_path_map_it->second;
    for (Token tok : decl.tokens().file_tokens()) {
      if (auto line_col = tok.location(index_data.file_location_cache)) {
        location.line = line_col->first;
        location.column = line_col->second;
        break;
      }
    }

    current_node.opt_location = std::move(location);
  }

  auto &parent_node = node_tree.node_map.at(parent_node_id);
  parent_node.child_node_id_list.push_back(current_node_id);

  node_tree.node_map.insert({current_node_id, std::move(current_node)});

  ExpandNode(node_tree, index_data, current_node_id, opt_max_depth);
}

void NodeImporter::ExpandNode(NodeTree &node_tree, const IndexData &index_data,
                              const std::uint64_t &node_id,
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

  const auto &node = node_it->second;
  auto entity_var = index_data.index.entity(node.entity_id);

  const auto NamedEntityVisitor =
      Overload{[](NamedDecl named_decl) { return named_decl.id().Pack(); },
               [](DefineMacroDirective macro) { return macro.id().Pack(); },
               [](mx::File file) { return file.id().Pack(); },
               [](auto) { return kInvalidEntityId; }};

  for (const auto &p : References(entity_var)) {
    const auto &opt_named_entity = p.first;
    if (!opt_named_entity.has_value()) {
      continue;
    }

    const auto &named_entity = opt_named_entity.value();

    auto entity_id = std::visit(NamedEntityVisitor, named_entity);
    if (entity_id == kInvalidEntityId) {
      continue;
    }

    auto child_node_id_it = std::find_if(
        node.child_node_id_list.begin(), node.child_node_id_list.end(),

        [&node_tree, &entity_id](const std::uint64_t &child_node_id) -> bool {
          const auto &child_node = node_tree.node_map.at(child_node_id);
          return (child_node.entity_id == entity_id);
        });

    if (child_node_id_it != node.child_node_id_list.end()) {
      continue;
    }

    ImportEntity(node_tree, index_data, node_id, entity_id, opt_max_depth);
  }
}

}  // namespace mx::gui
