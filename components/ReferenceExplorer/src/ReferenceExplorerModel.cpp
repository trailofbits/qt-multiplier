/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerModel.h"
#include "NodeImporter.h"

#include <multiplier/Entities/DefineMacroDirective.h>
#include <multiplier/Entities/FieldDecl.h>
#include <multiplier/Entities/FunctionDecl.h>
#include <multiplier/Entities/IncludeLikeMacroDirective.h>
#include <multiplier/Entities/VarDecl.h>
#include <multiplier/ui/Assert.h>

#include <QMimeData>
#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

namespace mx::gui {
namespace {

const QString kMimeTypeName{"application/mx-reference-explorer"};

}  // namespace

struct ReferenceExplorerModel::PrivateData final {
  PrivateData(mx::Index index_, mx::FileLocationCache file_location_cache_)
      : index(index_),
        file_location_cache(file_location_cache_),
        node_importer(index, file_location_cache, node_tree) {}

  mx::Index index;
  mx::FileLocationCache file_location_cache;
  NodeTree node_tree;
  NodeImporter node_importer;
};

bool ReferenceExplorerModel::AppendEntityObject(RawEntityId entity_id,
                                                const QModelIndex &parent) {

  std::uint64_t parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  // TODO(pag): Switch to something that can be made into a QFuture.
  VariantEntity entity = d->index.entity(entity_id);
  if (std::holds_alternative<Decl>(entity)) {
    entity_id = std::get<Decl>(entity).canonical_declaration().id().Pack();
  }

  // TODO: Switch to beginInsertRows/endInsertRows in order to preserve
  // the view state
  auto succeeded = d->node_importer.ImportEntity(
      entity_id, entity_id, NodeTree::Node::ImportMode::CallHierarchy,
      parent_node_id, 5);

  if (succeeded) {
    emit beginResetModel();
    emit endResetModel();
  }

  return true;
}

void ReferenceExplorerModel::ExpandEntity(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  const auto &node_id = static_cast<std::uint64_t>(index.internalId());

  // TODO: Switch to beginInsertRows/endInsertRows in order to preserve
  // the view state
  d->node_importer.ExpandNode(node_id, 2);

  emit beginResetModel();
  emit endResetModel();
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
      value = node.opt_name.value();

    } else {
      value = tr("Unnamed: ") + QString::number(node.entity_id);
    }

  } else if (role == Qt::ToolTipRole) {
    auto buffer = tr("Entity ID: ") + QString::number(node.entity_id) + "\n";

    if (std::optional<FragmentId> frag_id =
            FragmentId::from(node.referenced_entity_id)) {

      buffer += tr("Fragment ID: ") +
                QString::number(EntityId(frag_id.value()).Pack());
    }

    if (node.opt_location.has_value()) {
      buffer += "\n";
      buffer += tr("File ID: ") + QString::number(node.opt_location->file_id);
    }

    if (node.opt_location.has_value()) {
      const auto &location = node.opt_location.value();

      buffer += "\n";
      buffer += tr("Path: ") + location.path;
    }

    value = std::move(buffer);

  } else if (role == IReferenceExplorerModel::EntityIdRole) {
    value = node.entity_id;

  } else if (role == IReferenceExplorerModel::ReferencedEntityIdRole) {
    value = node.referenced_entity_id;

  } else if (role == IReferenceExplorerModel::FragmentIdRole) {
    if (std::optional<FragmentId> frag_id =
            FragmentId::from(node.referenced_entity_id)) {
      value.setValue(EntityId(frag_id.value()).Pack());
    }

  } else if (role == IReferenceExplorerModel::FileIdRole) {
    if (node.opt_location.has_value()) {
      value.setValue(node.opt_location->file_id);
    }

  } else if (role == IReferenceExplorerModel::LineNumberRole) {
    if (node.opt_location.has_value() && 0u < node.opt_location->line) {
      value.setValue(node.opt_location->line);
    }

  } else if (role == IReferenceExplorerModel::ColumnNumberRole) {
    if (node.opt_location.has_value() && 0u < node.opt_location->column) {
      value.setValue(node.opt_location->column);
    }

  } else if (role == IReferenceExplorerModel::LocationRole) {
    if (node.opt_location.has_value()) {
      value.setValue(node.opt_location.value());
    }

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

  const auto &root_index = indexes[0];

  QModelIndexList index_queue;
  if (root_index.isValid()) {
    index_queue << root_index;

  } else {
    auto row_count = rowCount(QModelIndex());

    for (int i = 0; i < row_count; ++i) {
      auto child_index = index(i, 0, QModelIndex());
      index_queue << child_index;
    }
  }

  std::vector<std::uint64_t> next_node_id_queue;

  for (const auto &index : index_queue) {
    if (!index.isValid()) {
      continue;
    }

    auto node_id_var =
        index.data(IReferenceExplorerModel::InternalIdentifierRole);

    Assert(node_id_var.isValid(), "Invalid InternalIdentifierRole value");

    auto node_id = qvariant_cast<std::uint64_t>(node_id_var);
    next_node_id_queue.push_back(node_id);
  }

  if (next_node_id_queue.empty()) {
    return nullptr;
  }

  std::unordered_set<std::uint64_t> visited_entity_id_set;

  QByteArray encoded_data;
  QDataStream encoded_data_stream(&encoded_data, QIODevice::WriteOnly);

  std::uint64_t instance_identifier{};

  {
    auto this_pointer = this;
    std::memcpy(&instance_identifier, &this_pointer,
                sizeof(instance_identifier));
  }

  encoded_data_stream << instance_identifier;

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

  std::uint64_t instance_identifier{};

  {
    auto this_pointer = this;
    std::memcpy(&instance_identifier, &this_pointer,
                sizeof(instance_identifier));
  }

  std::uint64_t incoming_instance_identifier{};
  encoded_data_stream >> incoming_instance_identifier;

  if (instance_identifier == incoming_instance_identifier) {
    return false;
  }

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
  auto item_flags{QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled};

  if (index.isValid()) {
    item_flags |= Qt::ItemIsDragEnabled;
  }

  return item_flags;
}

QStringList ReferenceExplorerModel::mimeTypes() const {
  return {kMimeTypeName};
}

ReferenceExplorerModel::ReferenceExplorerModel(
    mx::Index index, mx::FileLocationCache file_location_cache, QObject *parent)
    : IReferenceExplorerModel(parent),
      d(new PrivateData(index, file_location_cache)) {}

void ReferenceExplorerModel::SerializeNode(QDataStream &stream,
                                           const NodeTree::Node &node) {
  stream << node.node_id;
  stream << node.parent_node_id;
  stream << node.entity_id;
  stream << node.referenced_entity_id;

  if (node.opt_name.has_value()) {
    stream << true;
    stream << node.opt_name.value();
  } else {
    stream << false;
  }

  if (node.opt_location.has_value()) {
    stream << true;

    const auto &location = node.opt_location.value();
    stream << location.path;
    stream << location.file_id;
    stream << location.line;
    stream << location.column;

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

std::optional<NodeTree::Node>
ReferenceExplorerModel::DeserializeNode(QDataStream &stream) {
  try {
    NodeTree::Node node;

    stream >> node.node_id;
    stream >> node.parent_node_id;
    stream >> node.entity_id;
    stream >> node.referenced_entity_id;

    bool has_optional_field = false;

    // Read the name.
    stream >> has_optional_field;
    QString string_value;
    if (has_optional_field) {
      stream >> string_value;
      node.opt_name = string_value;
    }

    // Read the location.
    stream >> has_optional_field;
    if (has_optional_field) {
      Location location;
      stream >> location.path;
      stream >> location.file_id;
      stream >> location.line;
      stream >> location.column;
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
