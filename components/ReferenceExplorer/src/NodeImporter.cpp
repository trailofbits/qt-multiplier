/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "NodeImporter.h"

#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>

#include <multiplier/ui/Assert.h>

#include <iostream>

namespace mx::gui {

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
    std::optional<std::size_t> opt_ttl) {

  Assert(import_mode == NodeTree::Node::ImportMode::CallHierarchy,
         "Invalid import mode");

  auto prev_node_tree_size = d->node_tree.node_map.size();

  ImportEntityById(d->node_tree, d->index_data, opt_parent_node_id.value_or(0),
                   entity_id, opt_ttl);

  return prev_node_tree_size != d->node_tree.node_map.size();
}

bool NodeImporter::ExpandEntity(const std::uint64_t &node_id,
                                std::optional<std::size_t> opt_ttl) {

  auto node_it = d->node_tree.node_map.find(node_id);
  if (node_it == d->node_tree.node_map.end()) {
    return false;
  }

  auto &node = node_it->second;

  ImportEntityById(d->node_tree, d->index_data, node.node_id, node.entity_id,
                   opt_ttl);

  return true;
}

void NodeImporter::ImportEntityById(NodeTree &node_tree,
                                    const IndexData &index_data,
                                    const std::uint64_t &parent_node_id,
                                    const RawEntityId &entity_id,
                                    std::optional<std::size_t> opt_ttl) {

  auto entity_var = index_data.index.entity(entity_id);
  bool succeeded{false};

  if (std::holds_alternative<Decl>(entity_var)) {
    const auto &entity = std::get<Decl>(entity_var);
    ImportDeclEntity(node_tree, index_data, parent_node_id, entity, opt_ttl);

    succeeded = true;

  } else if (std::holds_alternative<Stmt>(entity_var)) {
    const auto &entity = std::get<Stmt>(entity_var);
    ImportStmtEntity(node_tree, index_data, parent_node_id, entity, opt_ttl);

    succeeded = true;

  } else if (std::holds_alternative<Macro>(entity_var)) {
    const auto &entity = std::get<Macro>(entity_var);
    ImportMacroEntity(node_tree, index_data, parent_node_id, entity, opt_ttl);

    succeeded = true;

  } else if (std::holds_alternative<Attr>(entity_var)) {
    const auto &entity = std::get<Attr>(entity_var);
    ImportAttrEntity(node_tree, index_data, parent_node_id, entity, opt_ttl);

    succeeded = true;

  } else if (std::holds_alternative<Designator>(entity_var)) {
    const auto &entity = std::get<Designator>(entity_var);
    ImportDesignatorEntity(node_tree, index_data, parent_node_id, entity,
                           opt_ttl);

    succeeded = true;

  } else if (std::holds_alternative<File>(entity_var)) {
    const auto &entity = std::get<File>(entity_var);
    ImportFileEntity(node_tree, index_data, parent_node_id, entity, opt_ttl);

    succeeded = true;

    // TODO(pag): Type, CXXBaseSpecifier, CXXTemplateArgument,
    //            CXXTemplateParameterList.

  } else if (std::holds_alternative<Token>(entity_var)) {
    const auto &entity = std::get<Token>(entity_var);

    auto statement_list = mx::Stmt::containing(entity);
    auto statement_list_it = statement_list.begin();

    auto declaration_list = mx::Decl::containing(entity);
    auto declaration_list_it = declaration_list.begin();

    if (statement_list_it != statement_list.end()) {
      ImportStmtEntity(node_tree, index_data, parent_node_id,
                       *statement_list_it, opt_ttl);
      succeeded = true;

    } else if (declaration_list_it != declaration_list.end()) {
      ImportDeclEntity(node_tree, index_data, parent_node_id,
                       *declaration_list_it, opt_ttl);
      succeeded = true;
    }
  }

  Assert(succeeded, "Failed to import the token entity");
}

void NodeImporter::ImportDeclEntity(NodeTree &node_tree,
                                    const IndexData &index_data,
                                    const std::uint64_t &parent_node_id,
                                    mx::Decl decl_entity,
                                    std::optional<std::size_t> opt_ttl) {

  auto decl_entity_id = decl_entity.id().Pack();
  if (node_tree.visited_entity_id_set.count(decl_entity_id) > 0) {
    return;
  }

  node_tree.visited_entity_id_set.insert(decl_entity_id);

  if (opt_ttl.has_value()) {
    auto ttl = opt_ttl.value();
    if (ttl == 0) {
      return;
    }

    opt_ttl = ttl - 1;
  }

  auto current_node_id = node_tree.node_map.size();

  NodeTree::Node current_node;
  current_node.node_id = current_node_id;
  current_node.parent_node_id = parent_node_id;

  if (auto named = mx::NamedDecl::from(decl_entity)) {
    current_node.opt_name = named->name();
  }

  current_node.entity_id = decl_entity_id;

  if (auto file = mx::File::containing(decl_entity)) {
    IReferenceExplorerModel::Location location;
    location.file_id = file->id().Pack();

    auto file_path_map_it = index_data.file_path_map.find(file->id());
    Assert(file_path_map_it != index_data.file_path_map.end(),
           "Invalid path id");

    location.path = file_path_map_it->second;
    for (Token tok : decl_entity.tokens().file_tokens()) {
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

  auto declaration_list = mx::Decl::containing(decl_entity);
  auto declaration_list_it = declaration_list.begin();
  if (declaration_list_it != declaration_list.end()) {
    ImportDeclEntity(node_tree, index_data, current_node_id,
                     *declaration_list_it, opt_ttl);

  } else {
    for (mx::Reference ref : decl_entity.references()) {
      if (auto ref_stmt = ref.as_statement()) {
        ImportStmtEntity(node_tree, index_data, current_node_id, *ref_stmt,
                         opt_ttl);

      } else if (auto ref_decl = ref.as_declaration()) {
        ImportDeclEntity(node_tree, index_data, current_node_id, *ref_decl,
                         opt_ttl);
      }
    }
  }
}

void NodeImporter::ImportStmtEntity(NodeTree &node_tree,
                                    const IndexData &index_data,
                                    const std::uint64_t &parent_node_id,
                                    mx::Stmt stmt_entity,
                                    std::optional<std::size_t> opt_ttl) {

  auto declaration_list = mx::Decl::containing(stmt_entity);

  auto declaration_list_it = declaration_list.begin();
  Assert(declaration_list_it != declaration_list.end(),
         "Failed to import the statement entity");

  ImportDeclEntity(node_tree, index_data, parent_node_id, *declaration_list_it,
                   opt_ttl);
}

void NodeImporter::ImportAttrEntity(NodeTree &node_tree,
                                    const IndexData &index_data,
                                    const std::uint64_t &parent_node_id,
                                    mx::Attr entity,
                                    std::optional<std::size_t> opt_ttl) {

  for (Token tok : entity.tokens()) {
    auto declaration_list = mx::Decl::containing(tok);
    auto declaration_list_it = declaration_list.begin();
    if (declaration_list_it == declaration_list.end()) {
      continue;
    }

    ImportDeclEntity(node_tree, index_data, parent_node_id,
                     *declaration_list_it, opt_ttl);
    return;
  }

  Assert(false, "Failed to import the attribute entity");
}

void NodeImporter::ImportDesignatorEntity(NodeTree &node_tree,
                                          const IndexData &index_data,
                                          const std::uint64_t &parent_node_id,
                                          mx::Designator entity,
                                          std::optional<std::size_t> opt_ttl) {

  for (Token tok : entity.tokens()) {
    auto declaration_list = mx::Decl::containing(tok);
    auto declaration_list_it = declaration_list.begin();
    if (declaration_list_it == declaration_list.end()) {
      continue;
    }

    ImportDeclEntity(node_tree, index_data, parent_node_id,
                     *declaration_list_it, opt_ttl);
    return;
  }

  Assert(false, "Failed to import the designator entity");
}

void NodeImporter::ImportMacroEntity(NodeTree &node_tree,
                                     const IndexData &index_data,
                                     const std::uint64_t &parent_node_id,
                                     mx::Macro entity,
                                     std::optional<std::size_t> opt_ttl) {

  auto entity_id = entity.id().Pack();
  if (node_tree.visited_entity_id_set.count(entity_id) > 0) {
    return;
  }

  node_tree.visited_entity_id_set.insert(entity_id);

  if (opt_ttl.has_value()) {
    auto ttl = opt_ttl.value();
    if (ttl == 0) {
      return;
    }

    opt_ttl = ttl - 1;
  }

  auto current_node_id = node_tree.node_map.size();

  NodeTree::Node current_node;
  current_node.node_id = current_node_id;
  current_node.parent_node_id = parent_node_id;
  current_node.entity_id = entity_id;

  for (Token tok : entity.tokens_covering_use()) {
    auto file_tok = tok.file_token();
    if (!file_tok) {
      continue;
    }

    auto file = mx::File::containing(entity);
    if (!file) {
      continue;
    }

    auto line_col = file_tok.location(index_data.file_location_cache);
    if (!line_col) {
      continue;
    }

    IReferenceExplorerModel::Location location;
    location.file_id = file->id().Pack();
    location.line = line_col->first;
    location.column = line_col->second;

    auto file_path_map_it = index_data.file_path_map.find(file->id());
    Assert(file_path_map_it != index_data.file_path_map.end(),
           "Invalid path id");

    location.path = file_path_map_it->second;
    current_node.opt_location = std::move(location);
    break;
  }

  if (auto named = mx::DefineMacroDirective::from(entity)) {
    current_node.opt_name = named->name().data();

    // Find uses of this macro.

  } else if (auto inc = mx::IncludeLikeMacroDirective::from(entity)) {
    current_node.opt_name = "<invalid file>";
    if (auto file = inc->included_file()) {
      // TODO(pag,alessandro): File path.
    }

    // Show the included file.

  } else {
    return;
  }
}

void NodeImporter::ImportFileEntity(NodeTree &node_tree,
                                    const IndexData &index_data,
                                    const std::uint64_t &parent_node_id,
                                    mx::File entity,
                                    std::optional<std::size_t> opt_ttl) {
  // TODO(pag): Show all includes that include this file.
  (void) node_tree;
  (void) index_data;
  (void) parent_node_id;
  (void) entity;
  (void) opt_ttl;
}


}  // namespace mx::gui
