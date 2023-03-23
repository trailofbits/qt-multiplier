/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerModel.h"
#include "INodeGenerator.h"

#include <multiplier/ui/Assert.h>
#include <multiplier/ui/Util.h>

#include <multiplier/File.h>
#include <multiplier/Index.h>

#include <QByteArray>
#include <QIODevice>
#include <QMimeData>
#include <QString>
#include <QThreadPool>

#include <filesystem>

namespace mx::gui {

namespace {

const QString kNodeInfoMimeType = "application/mx-reference-explorer-node-info";

const QString kInstanceInfoMimeType =
    "application/mx-reference-explorer-instance-info";

}  // namespace

struct ReferenceExplorerModel::PrivateData final {
  PrivateData(const mx::Index &index_,
              const mx::FileLocationCache &file_location_cache_)
      : index(index_),
        file_location_cache(file_location_cache_) {

    for (auto [path, id] : index.file_paths()) {
      file_path_map.emplace(id.Pack(),
                            QString::fromStdString(path.generic_string()));
    }
  }

  mx::Index index;

  //! Caches file/line/column mappings for open files.
  mx::FileLocationCache file_location_cache;

  //! The path map from mx::Index, keyed by id
  std::unordered_map<RawEntityId, QString> file_path_map;

  //! Node tree for this model.
  NodeTree node_tree;

  //! Active drag and drop mode
  DragAndDropMode drag_and_drop_mode{DragAndDropMode::CopySubTree};
};

//! Adds a new entity object under the given parent
void ReferenceExplorerModel::AppendEntityById(RawEntityId entity_id,
                                              ExpansionMode expansion_mode,
                                              const QModelIndex &parent) {

  INodeGenerator *generator = INodeGenerator::CreateRootGenerator(
      d->index, d->file_location_cache, entity_id, parent, expansion_mode);

  if (!generator) {
    return;
  }

  generator->setAutoDelete(true);

  connect(generator, &INodeGenerator::NodesAvailable, this,
          &ReferenceExplorerModel::InsertNodes);

  connect(generator, &INodeGenerator::Finished, this,
          &ReferenceExplorerModel::InsertNodes);

  QThreadPool::globalInstance()->start(generator);
}

void ReferenceExplorerModel::ExpandEntity(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  auto node_id = static_cast<std::uint64_t>(index.internalId());
  auto node_it = d->node_tree.node_map.find(node_id);
  if (node_it == d->node_tree.node_map.end()) {
    return;
  }

  auto &node = node_it->second;
  if (node.expanded) {
    return;
  }

  node.expanded = true;

  INodeGenerator *generator = INodeGenerator::CreateChildGenerator(
      d->index, d->file_location_cache, node_it->second.entity_id, index,
      node.expansion_mode);

  generator->setAutoDelete(true);

  connect(generator, &INodeGenerator::NodesAvailable, this,
          &ReferenceExplorerModel::InsertNodes);

  connect(generator, &INodeGenerator::Finished, this,
          &ReferenceExplorerModel::InsertNodes);

  QThreadPool::globalInstance()->start(generator);
}

void ReferenceExplorerModel::RemoveEntity(const QModelIndex &index) {
  if (!index.isValid()) {
    return;
  }

  std::uint64_t node_id = static_cast<std::uint64_t>(index.internalId());
  auto &node_map = d->node_tree.node_map;
  auto node_it = node_map.find(node_id);
  if (node_it == node_map.end()) {
    return;
  }

  Assert(node_id == node_it->second.node_id, "Out-of-sync node ids.");

  std::uint64_t parent_node_id = node_it->second.parent_node_id;

  auto parent_it = node_map.find(parent_node_id);

  // Check if we're removing the alternate root.
  if (node_id == d->node_tree.curr_root_node_id) {
    Assert(d->node_tree.curr_root_node_id != d->node_tree.root_node_id,
           "Can't remove the true root node.");
    d->node_tree.curr_root_node_id = d->node_tree.root_node_id;
  }

  QModelIndex parent_index = QModelIndex();
  int parent_offset = 0;
  if (parent_it == node_map.end()) {
    Assert(false, "Missing parent node, or removing true root node");

    // We're removing something inside of our parent.
  } else {
    Assert(parent_node_id == parent_it->second.node_id, "Out-of-sync node ids");

    auto found = false;
    for (auto sibling_id : parent_it->second.child_node_id_list) {
      if (sibling_id == node_id) {
        found = true;
        break;
      }
      ++parent_offset;
    }
    if (!found) {
      Assert(false, "Didn't find node to be deleted in parent's child list.");
    }

    if (parent_node_id != d->node_tree.curr_root_node_id) {
      parent_index = createIndex(parent_offset, 0, parent_node_id);
    }
  }

  emit beginResetModel();

  std::vector<std::uint64_t> wl;
  wl.push_back(node_id);

  // Recursively delete the child nodes.
  while (!wl.empty()) {
    std::uint64_t next_node_id = wl.back();
    Assert(next_node_id != parent_node_id, "Tree is actually a graph.");
    wl.pop_back();

    node_it = node_map.find(next_node_id);
    if (node_it == node_map.end()) {
      continue;
    }

    for (std::uint64_t child_node_id : node_it->second.child_node_id_list) {
      wl.push_back(child_node_id);
    }

    node_map.erase(node_it);
  }

  // Remove the node in its parent's list of child ids.
  if (parent_it != node_map.end()) {
    auto it = std::remove(parent_it->second.child_node_id_list.begin(),
                          parent_it->second.child_node_id_list.end(), node_id);
    parent_it->second.child_node_id_list.erase(
        it, parent_it->second.child_node_id_list.end());
  }

  emit endResetModel();
}

bool ReferenceExplorerModel::HasAlternativeRoot() const {
  return d->node_tree.root_node_id != d->node_tree.curr_root_node_id;
}

void ReferenceExplorerModel::SetRoot(const QModelIndex &index) {
  std::uint64_t root_node_id = d->node_tree.root_node_id;
  if (index.isValid()) {
    auto node_id_var =
        index.data(IReferenceExplorerModel::InternalIdentifierRole);

    if (node_id_var.isValid()) {
      root_node_id = node_id_var.toULongLong();
    }
  }

  emit beginResetModel();

  d->node_tree.curr_root_node_id = root_node_id;

  emit endResetModel();
}

void ReferenceExplorerModel::SetDefaultRoot() {
  SetRoot(QModelIndex());
}

ReferenceExplorerModel::~ReferenceExplorerModel() {}

QModelIndex ReferenceExplorerModel::index(int row, int column,
                                          const QModelIndex &parent) const {
  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  std::uint64_t parent_node_id{d->node_tree.curr_root_node_id};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  auto parent_node_it = d->node_tree.node_map.find(parent_node_id);
  if (parent_node_it == d->node_tree.node_map.end()) {
    return QModelIndex();
  }

  const Node &parent_node = parent_node_it->second;

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

  std::uint64_t parent_node_id{d->node_tree.curr_root_node_id};
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

  } else if (role == IReferenceExplorerModel::ExpansionModeRole) {
    value.setValue(node.expansion_mode);

  } else if (role == IReferenceExplorerModel::ExpansionStatusRole) {
    value.setValue(node.expanded);
  }

  return value;
}

QMimeData *
ReferenceExplorerModel::mimeData(const QModelIndexList &indexes) const {
  // Only allow dragging of one thing at a time. Dragging one thing implies
  // bringing along all of its children.
  if (indexes.size() != 1) {
    return nullptr;
  }

  const QModelIndex &root_index = indexes[0];

  auto mime_data = new QMimeData();

  // Serialize the whole subtree
  {
    std::vector<std::uint64_t> node_id_stack;

    // Append the internal node ID associated with `index` to `node_id_stack`.
    auto append_to_node_id_stack = [&](const QModelIndex &index) {
      if (!index.isValid()) {
        return false;
      }

      auto node_id_var =
          index.data(IReferenceExplorerModel::InternalIdentifierRole);

      Assert(node_id_var.isValid(), "Invalid InternalIdentifierRole value");

      auto node_id = qvariant_cast<std::uint64_t>(node_id_var);
      node_id_stack.push_back(node_id);
      return true;
    };

    // If we don't get a node ID for the root index, then go look for all of the
    // top-level rows.
    if (!append_to_node_id_stack(root_index)) {
      int row_count = rowCount(QModelIndex());
      for (int i = 0; i < row_count; ++i) {
        append_to_node_id_stack(index(i, 0, QModelIndex()));
      }
    }

    if (node_id_stack.empty()) {
      return nullptr;
    }

    QByteArray encoded_data;
    QDataStream encoded_data_stream(&encoded_data, QIODevice::WriteOnly);

    // NOTE(pag): This serializes nodes in the order that they appear in the
    //            tree, that way deserialization also preserves the same order,
    //            and all parent nodes are deserialized before child nodes.
    std::reverse(node_id_stack.begin(), node_id_stack.end());
    while (!node_id_stack.empty()) {
      std::uint64_t node_id = node_id_stack.back();
      node_id_stack.pop_back();

      auto node_it = d->node_tree.node_map.find(node_id);
      Assert(node_it != d->node_tree.node_map.end(), "Invalid node identifier");

      const Node &node = node_it->second;
      encoded_data_stream << node;

      node_id_stack.insert(node_id_stack.end(),
                           node.child_node_id_list.rbegin(),
                           node.child_node_id_list.rend());

      mime_data->setData(Node::kMimeTypeName, encoded_data);
    }
  }

  // Add the instance identifier mime data to prevent us from dragging
  // and dropping onto ourselves.
  {
    QByteArray encoded_data;
    QDataStream encoded_data_stream(&encoded_data, QIODevice::WriteOnly);

    std::uint64_t instance_identifier{};
    auto this_ptr{this};
    std::memcpy(&instance_identifier, &this_ptr, sizeof(instance_identifier));

    encoded_data_stream << instance_identifier;
    mime_data->setData(kInstanceInfoMimeType, encoded_data);
  }

  // Add the raw entity id information
  auto entity_id_var = root_index.data(IReferenceExplorerModel::EntityIdRole);
  if (entity_id_var.isValid()) {
    QByteArray encoded_data;
    QDataStream encoded_data_stream(&encoded_data, QIODevice::WriteOnly);

    auto entity_id = qvariant_cast<RawEntityId>(entity_id_var);
    encoded_data_stream << entity_id;

    mime_data->setData(kNodeInfoMimeType, encoded_data);
  }

  return mime_data;
}

void ReferenceExplorerModel::InsertNodes(QVector<Node> nodes, int row,
                                         const QModelIndex &drop_target) {

  // Figure out the drop target. This is the internal node id of the parent
  // node that will contain our dropped nodes.
  std::uint64_t drop_target_node_id = d->node_tree.curr_root_node_id;
  if (drop_target.isValid()) {
    auto drop_target_var =
        drop_target.data(IReferenceExplorerModel::InternalIdentifierRole);
    Assert(drop_target_var.isValid(), "Invalid InternalIdentifierRole value");
    drop_target_node_id = qvariant_cast<std::uint64_t>(drop_target_var);
  }

  if (!d->node_tree.node_map.contains(drop_target_node_id)) {
    return;
  }

  // Figure out where to drop the item in within the `drop_target_node_id`.
  int begin_row{};
  if (row != -1) {
    begin_row = row;
  } else if (drop_target.isValid()) {
    begin_row = drop_target.row();
  } else {
    begin_row = rowCount(QModelIndex());
  }

  // Link the new `root_nodes_dropped` into the parent node.
  Node &parent_node = d->node_tree.node_map[drop_target_node_id];
  Assert(parent_node.node_id == drop_target_node_id, "Invalid drop target");

  if (static_cast<size_t>(begin_row) > parent_node.child_node_id_list.size()) {
    return;
  }

  std::vector<std::uint64_t> root_nodes_dropped;

  // Create an old-to-new node ID mapping.
  std::unordered_map<std::uint64_t, std::uint64_t> id_mapping;
  for (Node &node : nodes) {
    const std::uint64_t old_id = node.node_id;
    Assert(old_id != 0u, "Invalid node id");
    node.AssignUniqueId();  // NOTE(pag): Replaces `Node::node_id`.
    Assert(node.node_id != 0u, "Invlaid unique node id");
    auto [it, added] = id_mapping.emplace(old_id, node.node_id);
    Assert(added, "Repeat node id found");
  }

  // Remap a node id as well as its parent node id. If the parent node id isn't
  // contained in our current tree, then this must be a node from the root of
  // what we were dragging. Rewrite the node's parent id to be the drop target
  // id.
  for (Node &node : nodes) {
    std::uint64_t parent_node_id = node.parent_node_id;
    auto parent_it = id_mapping.find(parent_node_id);
    if (parent_it == id_mapping.end()) {
      root_nodes_dropped.push_back(node.node_id);
      node.parent_node_id = drop_target_node_id;
    } else {
      node.parent_node_id = parent_it->second;
    }
  }

  // The `expanded` property of this node has changed, so tell the view
  // about it. This will disable the expand button (regardless of whether
  // we did get new nodes or not).
  emit dataChanged(drop_target, drop_target);

  // We did nothing, or we did nothing visible.
  auto num_dropped_nodes = static_cast<int>(root_nodes_dropped.size());
  if (!num_dropped_nodes) {
    return;
  }

  auto end_row = begin_row + static_cast<int>(root_nodes_dropped.size()) - 1;
  emit beginInsertRows(drop_target, begin_row, end_row);

  // Add the nodes into our tree.
  for (Node &node : nodes) {
    const std::uint64_t node_id = node.node_id;

    // Remap the child node ids.
    for (std::uint64_t &child_node_id : node.child_node_id_list) {
      child_node_id = id_mapping[child_node_id];
      Assert(child_node_id != 0u, "Missing child node");
    }

    auto [it, added] = d->node_tree.node_map.emplace(node_id, std::move(node));
    Assert(added, "Repeat node id found");
  }

  parent_node.child_node_id_list.insert(
      parent_node.child_node_id_list.begin() + static_cast<unsigned>(begin_row),
      root_nodes_dropped.begin(), root_nodes_dropped.end());

  emit endInsertRows();
}

bool ReferenceExplorerModel::dropMimeData(const QMimeData *data,
                                          Qt::DropAction action, int row, int,
                                          const QModelIndex &drop_target) {
  if (action == Qt::IgnoreAction) {
    return true;
  }

  if (!data->hasFormat(Node::kMimeTypeName) &&
      !data->hasFormat(kNodeInfoMimeType)) {

    return false;
  }

  // Prevent dragging and dropping nodes when the source and destination
  // trees match, i.e. from ourself to ourself.
  {
    auto encoded_data = data->data(kInstanceInfoMimeType);
    QDataStream encoded_data_stream(&encoded_data, QIODevice::ReadOnly);

    std::uint64_t instance_identifier{};
    auto this_ptr{this};
    std::memcpy(&instance_identifier, &this_ptr, sizeof(instance_identifier));

    std::uint64_t incoming_instance_identifier{};
    encoded_data_stream >> incoming_instance_identifier;
    if (instance_identifier == incoming_instance_identifier) {
      return false;
    }
  }

  auto succeeded{false};

  // Deserialize the correct stream and transfer all the nodes as-is
  // when we are asked to perform a subtree copy
  if (d->drag_and_drop_mode == DragAndDropMode::CopySubTree) {
    auto encoded_data = data->data(Node::kMimeTypeName);
    QDataStream encoded_data_stream(&encoded_data, QIODevice::ReadOnly);

    // Deserialize the nodes
    QVector<Node> decoded_nodes;
    while (!encoded_data_stream.atEnd()) {
      encoded_data_stream >> decoded_nodes.emplaceBack();
    }

    if (decoded_nodes.isEmpty()) {
      return false;
    }

    auto old_num_nodes = d->node_tree.node_map.size();
    InsertNodes(std::move(decoded_nodes), row, drop_target);

    succeeded = d->node_tree.node_map.size() > old_num_nodes;

  } else {
    auto encoded_data = data->data(kNodeInfoMimeType);
    QDataStream encoded_data_stream(&encoded_data, QIODevice::ReadOnly);

    RawEntityId entity_id{};
    encoded_data_stream >> entity_id;

    ExpansionMode expansion_mode{};
    if (d->drag_and_drop_mode == DragAndDropMode::AddRootAndTaint) {
      expansion_mode = ExpansionMode::TaintMode;

    } else if (d->drag_and_drop_mode == DragAndDropMode::AddRootAndShowRefs) {
      expansion_mode = ExpansionMode::CallHierarchyMode;

    } else {
      Assert(false, "Invalid drag and drop state");
    }

    AppendEntityById(entity_id, expansion_mode, drop_target);
    succeeded = true;
  }

  return succeeded;
}

Qt::ItemFlags ReferenceExplorerModel::flags(const QModelIndex &index) const {
  auto item_flags{QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled};

  if (index.isValid()) {
    item_flags |= Qt::ItemIsDragEnabled;
  }

  return item_flags;
}

QStringList ReferenceExplorerModel::mimeTypes() const {
  return {Node::kMimeTypeName};
}

void ReferenceExplorerModel::SetDragAndDropMode(const DragAndDropMode &mode) {
  d->drag_and_drop_mode = mode;
}

ReferenceExplorerModel::ReferenceExplorerModel(
    const Index &index, const FileLocationCache &file_location_cache,
    QObject *parent)
    : IReferenceExplorerModel(parent),
      d(new PrivateData(index, file_location_cache)) {}

}  // namespace mx::gui
