/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerModel.h"

#include <multiplier/ui/Assert.h>

#include <iostream>

namespace mx::gui {

struct ReferenceExplorerModel::PrivateData final {
  IndexData index_data;
  NodeTree node_tree;
};

bool ReferenceExplorerModel::AppendEntityObject(
    RawEntityId entity_id, EntityObjectType type,
    const QModelIndex &parent, std::optional<std::size_t> opt_ttl) {

  static_cast<void>(type);

  std::uint64_t parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  emit beginResetModel();

  ImportEntityById(d->node_tree, d->index_data, parent_node_id, entity_id,
                   opt_ttl);

  emit endResetModel();
  return true;
}

ReferenceExplorerModel::~ReferenceExplorerModel() {}

QModelIndex ReferenceExplorerModel::index(int row, int column,
                                          const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  std::uint64_t parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  auto parent_node_it = d->node_tree.node_map.find(parent_node_id);
  if (parent_node_it == d->node_tree.node_map.end()) {
    return QModelIndex();
  }

  const auto &parent_node = parent_node_it->second;

  auto unsigned_row = static_cast<std::size_t>(row);
  if (unsigned_row >= parent_node.child_node_id_list.size()) {
    return QModelIndex();
  }

  auto child_node_id = parent_node.child_node_id_list[unsigned_row];
  if (d->node_tree.node_map.count(child_node_id) == 0) {
    return QModelIndex();
  }

  return createIndex(row, column, static_cast<quintptr>(child_node_id));
}

QModelIndex ReferenceExplorerModel::parent(const QModelIndex &child) const {
  if (!child.isValid()) {
    return QModelIndex();
  }

  auto child_node_id = static_cast<std::uint64_t>(child.internalId());

  auto child_node_it = d->node_tree.node_map.find(child_node_id);
  if (child_node_it == d->node_tree.node_map.end()) {
    return QModelIndex();
  }

  const auto &child_node = child_node_it->second;
  if (child_node.parent_node_id == 0) {
    return QModelIndex();
  }

  auto parent_node_it = d->node_tree.node_map.find(child_node.parent_node_id);
  if (parent_node_it == d->node_tree.node_map.end()) {
    return QModelIndex();
  }

  const auto &parent_node = parent_node_it->second;

  auto grandparent_node_id = parent_node.parent_node_id;

  auto grandparent_node_it = d->node_tree.node_map.find(grandparent_node_id);
  if (grandparent_node_it == d->node_tree.node_map.end()) {
    return QModelIndex();
  }

  const auto &grandparent_node = grandparent_node_it->second;

  auto child_node_id_it = std::find(grandparent_node.child_node_id_list.begin(),
                                    grandparent_node.child_node_id_list.end(),
                                    child_node.parent_node_id);

  if (child_node_id_it == grandparent_node.child_node_id_list.end()) {
    return QModelIndex();
  }

  auto parent_row = static_cast<int>(std::distance(
      grandparent_node.child_node_id_list.begin(), child_node_id_it));

  return createIndex(parent_row, 0, child_node.parent_node_id);
}

int ReferenceExplorerModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 1) {
    return 0;
  }

  std::uint64_t parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  auto parent_node_it = d->node_tree.node_map.find(parent_node_id);
  if (parent_node_it == d->node_tree.node_map.end()) {
    return 0;
  }

  const auto &parent_node = parent_node_it->second;
  return static_cast<int>(parent_node.child_node_id_list.size());
}

int ReferenceExplorerModel::columnCount(const QModelIndex &) const {
  if (d->node_tree.node_map.empty()) {
    return 0;
  }

  return 1;
}

QVariant ReferenceExplorerModel::data(const QModelIndex &index,
                                      int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  auto node_id = static_cast<std::uint64_t>(index.internalId());

  auto node_it = d->node_tree.node_map.find(node_id);
  if (node_it == d->node_tree.node_map.end()) {
    return QVariant();
  }

  const auto &node = node_it->second;

  QVariant value;
  if (role == Qt::DisplayRole) {
    if (node.opt_name.has_value()) {
      value = QString::fromStdString(node.opt_name.value());

    } else {
      value = tr("Unnamed: ") + QString::number(node.identifiers.entity_id);
    }
  } else if (role == IReferenceExplorerModel::LocationRole &&
             node.opt_location.has_value()) {
    value.setValue(node.opt_location.value());
  }

  return value;
}

ReferenceExplorerModel::ReferenceExplorerModel(
    mx::Index index, mx::FileLocationCache file_location_cache, QObject *parent)
    : IReferenceExplorerModel(parent),
      d(new PrivateData) {

  d->index_data.index = index;
  d->index_data.file_location_cache = file_location_cache;

  for (auto [path, id] : index.file_paths()) {
    d->index_data.file_path_map.emplace(id, std::move(path));
  }
}

void ReferenceExplorerModel::ImportEntityById(
    NodeTree &node_tree, const IndexData &index_data,
    const std::uint64_t &parent_node_id, const RawEntityId &entity_id,
    std::optional<std::size_t> opt_ttl) {

  if (opt_ttl.has_value()) {
    auto ttl = opt_ttl.value();
    if (ttl == 0) {
      return;
    }

    opt_ttl = ttl - 1;
  }

  std::cout << __FILE__ << "@" << __LINE__ << " Processing: " << entity_id
            << "\n";

  auto entity_var = index_data.index.entity(entity_id);
  bool succeeded{false};

  if (std::holds_alternative<mx::Decl>(entity_var)) {
    const auto &entity = std::get<mx::Decl>(entity_var);
    ImportDeclEntity(node_tree, index_data, parent_node_id, entity, opt_ttl);

    succeeded = true;

  } else if (std::holds_alternative<mx::Stmt>(entity_var)) {
    const auto &entity = std::get<mx::Stmt>(entity_var);
    ImportStmtEntity(node_tree, index_data, parent_node_id, entity, opt_ttl);

    succeeded = true;

  } else if (std::holds_alternative<mx::Token>(entity_var)) {
    const auto &entity = std::get<mx::Token>(entity_var);

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

void ReferenceExplorerModel::ImportDeclEntity(
    NodeTree &node_tree, const IndexData &index_data,
    const std::uint64_t &parent_node_id, mx::Decl decl_entity,
    std::optional<std::size_t> opt_ttl) {

  if (node_tree.node_map.count(0) == 0) {
    node_tree.node_map.insert({0, NodeTree::Node{}});
  }

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

  auto fragment = mx::Fragment::containing(decl_entity);
  current_node.identifiers.fragment_id = fragment.id().Pack();
  current_node.identifiers.entity_id = decl_entity_id;
  current_node.decl_kind = decl_entity.kind();

  const auto file = mx::File::containing(fragment);
  if (file) {
    current_node.identifiers.opt_file_id = file->id().Pack();

    auto file_path_map_it = index_data.file_path_map.find(file->id());
    Assert(file_path_map_it != index_data.file_path_map.end(),
           "Invalid path id");

    const auto &file_path = file_path_map_it->second;

    Location location;
    location.path = file_path.generic_string();

    if (auto tok = decl_entity.token()) {
      if (auto line_col = tok.location(index_data.file_location_cache)) {
        location.opt_line = static_cast<std::size_t>(line_col->first);
        location.opt_column = static_cast<std::size_t>(line_col->second);
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

void ReferenceExplorerModel::ImportStmtEntity(
    NodeTree &node_tree, const IndexData &index_data,
    const std::uint64_t &parent_node_id, mx::Stmt stmt_entity,
    std::optional<std::size_t> opt_ttl) {

  if (opt_ttl.has_value()) {
    auto ttl = opt_ttl.value();
    if (ttl == 0) {
      return;
    }

    opt_ttl = ttl - 1;
  }

  auto declaration_list = mx::Decl::containing(stmt_entity);

  auto declaration_list_it = declaration_list.begin();
  Assert(declaration_list_it != declaration_list.end(),
         "Failed to import the statement entity");

  ImportDeclEntity(node_tree, index_data, parent_node_id, *declaration_list_it,
                   opt_ttl);
}

}  // namespace mx::gui
