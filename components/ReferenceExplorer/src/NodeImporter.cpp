/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "NodeImporter.h"

#include <multiplier/Entities/CXXBaseSpecifier.h>
#include <multiplier/Entities/TemplateArgument.h>
#include <multiplier/Entities/TemplateParameterList.h>
#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>
#include <multiplier/Entities/NamedDecl.h>
#include <multiplier/File.h>

#include <multiplier/ui/Assert.h>

#include "Utils.h"

namespace mx::gui {

struct NodeImporter::PrivateData {
  FileLocationCache file_cache;
  VariantEntity entity;
  VariantEntity referenced_entity;
  IReferenceExplorerModel::ExpansionMode import_mode{};
  QModelIndex parent;

  PrivateData(const FileLocationCache &file_cache_, VariantEntity entity_,
              VariantEntity referenced_entity_)
      : file_cache(file_cache_),
        entity(std::move(entity_)),
        referenced_entity(std::move(referenced_entity_)) {}
};

NodeImporter::~NodeImporter(void) {}

NodeImporter::NodeImporter(
    const FileLocationCache &file_cache, VariantEntity entity,
    VariantEntity referenced_entity,
    IReferenceExplorerModel::ExpansionMode import_mode,
    const QModelIndex &parent)
    : d(new PrivateData(file_cache, std::move(entity),
                        std::move(referenced_entity))) {
  d->import_mode = import_mode;
  d->parent = parent;
}

Node NodeImporter::CreateNode(
    const FileLocationCache &file_cache, VariantEntity entity,
    VariantEntity referenced_entity,
    IReferenceExplorerModel::ExpansionMode import_mode) {

  Node node;
  node.AssignUniqueId();
  node.expansion_mode = import_mode;
  node.entity_id = IdOfEntity(entity);
  node.opt_name = NameOfEntity(entity);

  // Root nodes reference themselves.
  node.referenced_entity_id = IdOfEntity(referenced_entity);

  Token file_tok = FirstFileToken(referenced_entity).file_token();
  if (!file_tok) {
    file_tok = FirstFileToken(entity).file_token();
  }

  if (file_tok) {
    auto file = File::containing(file_tok);
    Assert(file.has_value(), "Token::file_token returned non-file token?");

    IReferenceExplorerModel::Location location;
    location.file_id = file->id().Pack();

    for (std::filesystem::path path : file->paths()) {
      location.path = QString::fromStdString(path.generic_string());
    }

    Assert(!location.path.isEmpty(), "Empty file paths aren't allowed");

    if (auto line_col = file_tok.location(file_cache)) {
      location.line = line_col->first;
      location.column = line_col->second;
    }

    node.opt_location = std::move(location);
  }

  return node;
}

void NodeImporter::run() {
  emit Finished(
      CreateNode(d->file_cache, std::move(d->entity),
                 std::move(d->referenced_entity), d->import_mode),
      d->parent);
}

//
//struct NodeImporter::PrivateData {
//  PrivateData(mx::Index index, mx::FileLocationCache file_location_cache,
//              NodeTree &node_tree_)
//      : node_tree(node_tree_) {
//
//    // Reset the node importer, but prevent the practical re-use of node IDs.
//    node_tree.node_map.clear();
//    node_tree.visited_entity_id_set.clear();
//    node_tree.root_node_id = node_tree.next_node_id++;
//    node_tree.curr_root_node_id = node_tree.root_node_id;
//    node_tree.node_map.insert({node_tree.root_node_id, Node{}});
//
//    index_data.index = index;
//    index_data.file_location_cache = file_location_cache;
//
//    for (auto [path, id] : index.file_paths()) {
//      index_data.file_path_map.emplace(
//          id.Pack(), QString::fromStdString(path.generic_string()));
//    }
//  }
//
//  IndexData index_data;
//  NodeTree &node_tree;
//};
//
//NodeImporter::NodeImporter(mx::Index index,
//                           mx::FileLocationCache file_location_cache,
//                           NodeTree &node_tree)
//    : d(new PrivateData(index, file_location_cache, node_tree)) {}
//
//NodeImporter::~NodeImporter() {}
//
//void NodeImporter::ExpandNode(std::uint64_t node_id, unsigned max_depth) {
//  ExpandNode(d->node_tree, d->index_data, node_id, max_depth);
//}
//
//void NodeImporter::ExpandNode(NodeTree &node_tree, const IndexData &index_data,
//                              std::uint64_t node_id, unsigned max_depth) {
//
//  if (!max_depth) {
//    return;
//  }
//
//  auto node_it = node_tree.node_map.find(node_id);
//  if (node_it == node_tree.node_map.end()) {
//    return;
//  }
//
//  const Node &node = node_it->second;
//  for (const auto &p : References(index_data.index.entity(node.entity_id))) {
//    RawEntityId entity_id = p.first;
//    RawEntityId referenced_entity_id = p.second.referenced_entity_id().Pack();
//    if (entity_id == kInvalidEntityId ||
//        referenced_entity_id == kInvalidEntityId) {
//      continue;
//    }
//
//    auto child_node_id_it = std::find_if(
//        node.child_node_id_list.begin(), node.child_node_id_list.end(),
//
//        [=, &node_tree](const std::uint64_t &child_node_id) -> bool {
//          const Node &child_node =
//              node_tree.node_map.at(child_node_id);
//          return (child_node.referenced_entity_id == referenced_entity_id);
//        });
//
//    if (child_node_id_it != node.child_node_id_list.end()) {
//      continue;
//    }
//
//    ImportEntity(node_tree, index_data, node_id, entity_id,
//                 referenced_entity_id, max_depth - 1u);
//  }
//}
//
//bool NodeImporter::ImportEntity(RawEntityId entity_id,
//                                RawEntityId referenced_entity_id,
//                                Node::ExpansionMode import_mode,
//                                std::optional<std::uint64_t> opt_parent_node_id,
//                                unsigned max_depth) {
//
//  Assert(import_mode == Node::ExpansionMode::CallHierarchy,
//         "Invalid import mode");
//
//  auto prev_node_tree_size = d->node_tree.node_map.size();
//
//  ImportEntity(d->node_tree, d->index_data, opt_parent_node_id.value_or(0),
//               entity_id, referenced_entity_id, max_depth);
//
//  return prev_node_tree_size != d->node_tree.node_map.size();
//}
//
//void NodeImporter::ImportEntity(NodeTree &node_tree,
//                                const IndexData &index_data,
//                                std::uint64_t parent_node_id,
//                                RawEntityId entity_id,
//                                RawEntityId referenced_entity_id,
//                                unsigned max_depth) {
//
//  // TODO(alessandro): Some kind of thing to show that we have a repeat? This
//  //                   should maybe be a custom row that points to the old
//  //                   entry with a button of some kind.
//
//  // The user has right-clicked and selected "Expand" on a parent
//  // item even though it was already expanded.
//  //
//  // Ignore the command on this node for now. We could also just substract
//  // one from opt_max_depth and forward the request by just inserting a call
//  // to ExpandNode before the return
//  if (!node_tree.visited_entity_id_set.insert(referenced_entity_id).second) {
//    return;
//  }
//
//  VariantEntity entity = index_data.index.entity(entity_id);
//  VariantEntity referenced_entity =
//      index_data.index.entity(referenced_entity_id);
//
//  if (std::holds_alternative<NotAnEntity>(entity) ||
//      std::holds_alternative<NotAnEntity>(referenced_entity)) {
//    return;
//  }
//
//  // Always get a fresh node id, so that we don't have repetitions.
//  auto current_node_id = node_tree.next_node_id++;
//
//  Node cn;
//  cn.node_id = current_node_id;
//  cn.parent_node_id = parent_node_id;
//  cn.entity_id = entity_id;
//  cn.referenced_entity_id = referenced_entity_id;
//  cn.opt_name = NameOfEntity(entity, index_data.file_path_map);
//
//  // Get us the location of the referenced entity.
//  auto ref_tok = FirstFileToken(referenced_entity);
//  if (!ref_tok) {
//    ref_tok = FirstFileToken(entity);
//  }
//  if (Token file_tok = ref_tok.file_token()) {
//    auto file = File::containing(file_tok);
//    Assert(file.has_value(), "Token::file_token returned non-file token?");
//
//    IReferenceExplorerModel::Location location;
//    location.file_id = file->id().Pack();
//
//    auto file_path_map_it = index_data.file_path_map.find(location.file_id);
//    Assert(file_path_map_it != index_data.file_path_map.end(),
//           "Invalid path id");
//
//    location.path = file_path_map_it->second;
//    if (auto line_col = ref_tok.location(index_data.file_location_cache)) {
//      location.line = line_col->first;
//      location.column = line_col->second;
//    }
//
//    cn.opt_location = std::move(location);
//  }
//
//  Node &parent_node = node_tree.node_map.at(parent_node_id);
//  parent_node.child_node_id_list.push_back(current_node_id);
//
//  node_tree.node_map.insert({current_node_id, std::move(cn)});
//
//  ExpandNode(node_tree, index_data, current_node_id, max_depth);
//}

}  // namespace mx::gui
