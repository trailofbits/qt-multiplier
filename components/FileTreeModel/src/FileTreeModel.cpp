/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "FileTreeModel.h"

namespace mx::gui {

struct FileTreeModel::PrivateData final {
  mx::Index index;
  NodeMap node_map;
};

FileTreeModel::~FileTreeModel() {}

void FileTreeModel::Update() {
  emit beginResetModel();

  d->node_map = ParsePathList(d->index.file_paths());

  emit endResetModel();
}

QModelIndex FileTreeModel::index(int row, int column,
                                 const QModelIndex &parent) const {

  if (!hasIndex(row, column, parent)) {
    return QModelIndex();
  }

  std::uint64_t parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  auto parent_node_it = d->node_map.find(parent_node_id);
  if (parent_node_it == d->node_map.end()) {
    return QModelIndex();
  }

  const auto &parent_node = parent_node_it->second;

  if (row >= parent_node.child_map.size()) {
    return QModelIndex();
  }

  auto child_map_it = std::next(parent_node.child_map.begin(), row);
  const auto &child_node_id = child_map_it->second;

  auto child_node_it = d->node_map.find(child_node_id);
  if (child_node_it == d->node_map.end()) {
    return QModelIndex();
  }

  return createIndex(row, column, static_cast<quintptr>(child_node_id));
}

QModelIndex FileTreeModel::parent(const QModelIndex &child) const {
  if (!child.isValid()) {
    return QModelIndex();
  }

  // Get the child node
  auto child_node_id = static_cast<std::uint64_t>(child.internalId());

  auto child_node_it = d->node_map.find(child_node_id);
  if (child_node_it == d->node_map.end()) {
    return QModelIndex();
  }

  const auto &child_node = child_node_it->second;

  // Get the parent node
  if (child_node.parent == 0) {
    return QModelIndex();
  }

  auto parent_node_id = child_node.parent;
  auto parent_node_it = d->node_map.find(parent_node_id);
  if (parent_node_it == d->node_map.end()) {
    return QModelIndex();
  }

  const auto &parent_node = parent_node_it->second;

  // Now we need to find the row value we need. Look for the parent inside
  // the grandparent's child_map. If we can't find it, then it is safe to return
  // a blank QModelIndex
  auto grandparent_node_id = parent_node.parent;

  auto grandparent_node_it = d->node_map.find(grandparent_node_id);
  if (grandparent_node_it == d->node_map.end()) {
    return QModelIndex();
  }

  const auto &grandparent_node = grandparent_node_it->second;

  auto child_map_it = std::find_if(
      grandparent_node.child_map.begin(), grandparent_node.child_map.end(),

      [&](const std::pair<std::string, std::uint64_t> &p) -> bool {
        const auto &id = p.second;
        return id == parent_node_id;
      });

  if (child_map_it == grandparent_node.child_map.end()) {
    return QModelIndex();
  }

  auto parent_row = static_cast<int>(
      std::distance(grandparent_node.child_map.begin(), child_map_it));

  return createIndex(parent_row, 0, parent_node_id);
}

int FileTreeModel::rowCount(const QModelIndex &parent) const {
  if (parent.column() > 1) {
    return 0;
  }

  std::uint64_t parent_node_id{};
  if (parent.isValid()) {
    parent_node_id = static_cast<std::uint64_t>(parent.internalId());
  }

  auto parent_node_it = d->node_map.find(parent_node_id);
  if (parent_node_it == d->node_map.end()) {
    return 0;
  }

  const auto &parent_node = parent_node_it->second;
  return static_cast<int>(parent_node.child_map.size());
}

int FileTreeModel::columnCount(const QModelIndex &parent) const {
  if (d->node_map.empty()) {
    return 0;
  }

  return 1;
}

QVariant FileTreeModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || role != Qt::DisplayRole) {
    return QVariant();
  }

  auto node_id = static_cast<std::uint64_t>(index.internalId());

  auto node_it = d->node_map.find(node_id);
  if (node_it == d->node_map.end()) {
    return QVariant();
  }

  const auto &node = node_it->second;
  return QString::fromStdString(node.file_name);
}

FileTreeModel::FileTreeModel(mx::Index index, QObject *parent)
    : IFileTreeModel(parent),
      d(new PrivateData()) {
  d->index = index;
  Update();
}

FileTreeModel::NodeMap FileTreeModel::ParsePathList(
    const std::map<std::filesystem::path, mx::PackedFileId> &path_list) {

  std::uint64_t node_id_generator{0};

  NodeMap node_map = {{0, {"ROOT", std::nullopt}}};

  for (const auto &file_path_p : path_list) {
    const auto &path = file_path_p.first;
    const auto &path_id = file_path_p.second;

    auto current_node = &node_map.at(0);
    std::uint64_t current_parent{0};

    for (auto component_it = path.begin(); component_it != path.end();
         ++component_it) {

      const auto &component = *component_it;
      bool is_leaf_node = std::next(component_it, 1) == path.end();

      auto node_it = current_node->child_map.find(component);
      if (node_it == current_node->child_map.end()) {
        Node node;
        node.file_name = component.string();

        if (is_leaf_node) {
          node.opt_file_id = path_id;
        }

        node.parent = current_parent;

        auto node_id = ++node_id_generator;
        node_map.insert({node_id, std::move(node)});

        auto insert_status =
            current_node->child_map.insert({component.string(), node_id});

        node_it = insert_status.first;
      }

      auto &next_node_id = node_it->second;
      current_parent = next_node_id;

      current_node = &node_map.at(next_node_id);
    }
  }

  return node_map;
}

}  // namespace mx::gui
