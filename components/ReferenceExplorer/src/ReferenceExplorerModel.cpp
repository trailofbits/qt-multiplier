/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerModel.h"

#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>
#include <multiplier/ui/Assert.h>

#include <QMimeData>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

namespace mx::gui {

namespace {

const QString kMimeTypeName{"application/mx-reference-explorer"};

}

struct ReferenceExplorerModel::PrivateData final {
  IndexData index_data;
  NodeTree node_tree;
};

bool ReferenceExplorerModel::AppendEntityObject(
    RawEntityId entity_id, EntityObjectType type, const QModelIndex &parent,
    std::optional<std::size_t> opt_ttl) {

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

  } else if (role == IReferenceExplorerModel::InternalIdentifierRole) {
    value.setValue(node_id);
  }

  return value;
}

QMimeData *
ReferenceExplorerModel::mimeData(const QModelIndexList &indexes) const {
  if (indexes.size() != 1) {
    return nullptr;
  }

  std::vector<std::uint64_t> next_node_id_queue;

  for (const auto &index : indexes) {
    if (!index.isValid()) {
      continue;
    }

    auto node_id_var =
        index.data(IReferenceExplorerModel::InternalIdentifierRole);
    if (!node_id_var.isValid()) {
      continue;
    }

    auto node_id = static_cast<std::uint64_t>(node_id_var.toULongLong());

    next_node_id_queue.push_back(node_id);
  }

  std::unordered_set<std::uint64_t> visited_entity_id_set;

  QByteArray encoded_data;
  QDataStream encoded_data_stream(&encoded_data, QIODevice::WriteOnly);

  while (!next_node_id_queue.empty()) {
    auto node_id_queue = std::move(next_node_id_queue);
    next_node_id_queue.clear();

    for (const auto &node_id : node_id_queue) {
      auto insert_status = visited_entity_id_set.insert(node_id);

      auto node_id_was_inserted = insert_status.second;
      if (!node_id_was_inserted) {
        continue;
      }

      auto node_it = d->node_tree.node_map.find(node_id);
      Assert(node_it != d->node_tree.node_map.end(), "Invalid node identifier");

      const auto &node = node_it->second;
      SerializeNode(encoded_data_stream, node);

      next_node_id_queue.insert(next_node_id_queue.end(),
                                node.child_node_id_list.begin(),
                                node.child_node_id_list.end());
    }
  }

  auto mime_data = new QMimeData();
  mime_data->setData(kMimeTypeName, encoded_data);
  return mime_data;
}

bool ReferenceExplorerModel::dropMimeData(const QMimeData *data,
                                          Qt::DropAction action, int, int,
                                          const QModelIndex &) {
  if (action == Qt::IgnoreAction) {
    return true;
  }

  if (!data->hasFormat(kMimeTypeName)) {
    return false;
  }

  auto encoded_data = data->data(kMimeTypeName);
  QDataStream encoded_data_stream(&encoded_data, QIODevice::ReadOnly);

  std::vector<NodeTree::Node> incoming_node_list;

  while (!encoded_data_stream.atEnd()) {
    auto opt_node = DeserializeNode(encoded_data_stream);
    Assert(opt_node.has_value(), "Invalid mime data format");

    auto &node = opt_node.value();
    incoming_node_list.push_back(std::move(node));
  }

  if (incoming_node_list.empty()) {
    return false;
  }

  std::unordered_map<std::uint64_t, std::uint64_t> node_id_mapping;

  auto original_parent_node_id = incoming_node_list[0].parent_node_id;
  node_id_mapping.insert({original_parent_node_id, 0});

  auto base_node_id = d->node_tree.node_map.size();
  for (const auto &node : incoming_node_list) {
    node_id_mapping[node.node_id] = base_node_id;
    ++base_node_id;
  }

  emit beginResetModel();

  auto first_node{true};

  for (auto &node : incoming_node_list) {

    Assert(node_id_mapping.count(node.node_id) == 1,
           "The generated node id is not valid");

    Assert(node_id_mapping.count(node.parent_node_id) == 1,
           "The parent node id is not valid");

    node.parent_node_id = node_id_mapping[node.parent_node_id];
    node.node_id = node_id_mapping[node.node_id];

    if (first_node) {
      auto &parent_node = d->node_tree.node_map[node.parent_node_id];
      parent_node.child_node_id_list.push_back(node.node_id);

      first_node = false;
    }

    for (auto &child_node_id : node.child_node_id_list) {
      child_node_id = node_id_mapping[child_node_id];
    }

    d->node_tree.node_map.insert({node.node_id, node});
  }

  emit endResetModel();
  return true;
}

Qt::ItemFlags ReferenceExplorerModel::flags(const QModelIndex &index) const {
  return QAbstractItemModel::flags(index) | Qt::ItemIsDragEnabled |
         Qt::ItemIsDropEnabled;
}

QStringList ReferenceExplorerModel::mimeTypes() const {
  return {kMimeTypeName};
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

  InitializeNodeTree(d->node_tree);
}

void ReferenceExplorerModel::InitializeNodeTree(NodeTree &node_tree) {
  node_tree.node_map.insert({0, NodeTree::Node{}});
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

void ReferenceExplorerModel::ImportAttrEntity(
    NodeTree &node_tree, const IndexData &index_data,
    const std::uint64_t &parent_node_id, mx::Attr entity,
    std::optional<std::size_t> opt_ttl) {

  if (opt_ttl.has_value()) {
    auto ttl = opt_ttl.value();
    if (ttl == 0) {
      return;
    }

    opt_ttl = ttl - 1;
  }

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

void ReferenceExplorerModel::ImportDesignatorEntity(
    NodeTree &node_tree, const IndexData &index_data,
    const std::uint64_t &parent_node_id, mx::Designator entity,
    std::optional<std::size_t> opt_ttl) {

  if (opt_ttl.has_value()) {
    auto ttl = opt_ttl.value();
    if (ttl == 0) {
      return;
    }

    opt_ttl = ttl - 1;
  }

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

void ReferenceExplorerModel::ImportMacroEntity(
    NodeTree &node_tree, const IndexData &, const std::uint64_t &parent_node_id,
    mx::Macro entity, std::optional<std::size_t> opt_ttl) {

  if (node_tree.node_map.count(0) == 0) {
    node_tree.node_map.insert({0, NodeTree::Node{}});
  }

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

  auto fragment = mx::Fragment::containing(entity);
  current_node.identifiers.fragment_id = fragment.id().Pack();
  current_node.identifiers.entity_id = entity_id;

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

void ReferenceExplorerModel::ImportFileEntity(
    NodeTree &node_tree, const IndexData &index_data,
    const std::uint64_t &parent_node_id, mx::File entity,
    std::optional<std::size_t> opt_ttl) {
  // TODO(pag): Show all includes that include this file.
  (void) node_tree;
  (void) index_data;
  (void) parent_node_id;
  (void) entity;
  (void) opt_ttl;
}

void ReferenceExplorerModel::SerializeNode(QDataStream &stream,
                                           const NodeTree::Node &node) {
  stream << node.node_id;
  stream << node.parent_node_id;

  if (node.identifiers.opt_file_id.has_value()) {
    stream << true;
    stream << node.identifiers.opt_file_id.value();
  } else {
    stream << false;
  }

  stream << static_cast<std::uint64_t>(node.identifiers.fragment_id);
  stream << static_cast<std::uint64_t>(node.identifiers.entity_id);

  if (node.opt_name.has_value()) {
    stream << true;
    stream << QString::fromStdString(node.opt_name.value());
  } else {
    stream << false;
  }

  if (node.opt_location.has_value()) {
    stream << true;

    const auto &location = node.opt_location.value();
    stream << QString::fromStdString(location.path);

    if (location.opt_line.has_value()) {
      stream << true;
      stream << location.opt_line.value();

    } else {
      stream << false;
    }

    if (location.opt_column.has_value()) {
      stream << true;
      stream << location.opt_column.value();

    } else {
      stream << false;
    }

  } else {
    stream << false;
  }

  auto child_node_id_list_size =
      static_cast<std::uint64_t>(node.child_node_id_list.size());

  stream << child_node_id_list_size;

  for (const auto &child_node_id : node.child_node_id_list) {
    stream << child_node_id;
  }
}

std::optional<ReferenceExplorerModel::NodeTree::Node>
ReferenceExplorerModel::DeserializeNode(QDataStream &stream) {
  try {
    NodeTree::Node node;

    stream >> node.node_id;
    stream >> node.parent_node_id;

    bool has_optional_field{};
    stream >> has_optional_field;

    std::uint64_t raw_entity_id_value{};
    if (has_optional_field) {
      stream >> raw_entity_id_value;

      node.identifiers.opt_file_id =
          static_cast<RawEntityId>(raw_entity_id_value);
    }

    stream >> raw_entity_id_value;
    node.identifiers.fragment_id =
        static_cast<RawEntityId>(raw_entity_id_value);

    stream >> raw_entity_id_value;
    node.identifiers.entity_id = static_cast<RawEntityId>(raw_entity_id_value);

    stream >> has_optional_field;

    QString string_value;
    if (has_optional_field) {
      stream >> string_value;
      node.opt_name = string_value.toStdString();
    }

    stream >> has_optional_field;

    if (has_optional_field) {
      stream >> string_value;

      Location location;
      location.path = string_value.toStdString();

      stream >> has_optional_field;
      if (has_optional_field) {
        std::uint64_t line_number{};
        stream >> line_number;

        location.opt_line = line_number;
      }

      stream >> has_optional_field;
      if (has_optional_field) {
        std::uint64_t column_number{};
        stream >> column_number;

        location.opt_column = column_number;
      }

      node.opt_location = std::move(location);
    }

    std::uint64_t child_node_id_list_size{};
    stream >> child_node_id_list_size;

    for (std::uint64_t i = 0; i < child_node_id_list_size; ++i) {
      std::uint64_t child_node_id{};
      stream >> child_node_id;

      node.child_node_id_list.push_back(child_node_id);
    }

    return node;

  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace mx::gui
