/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "FileTreeModel.h"

namespace mx::gui {

namespace {

bool SplitComponentAt(FileTreeModel::NodeMap &node_map,
                      const std::uint64_t node_id,
                      std::size_t component_index) {

  // Get the folder data structure
  auto &node = node_map.at(node_id);
  if (!std::holds_alternative<FileTreeModel::Node::FolderData>(node.data)) {
    return false;
  }

  auto &folder_node_data = std::get<FileTreeModel::Node::FolderData>(node.data);

  // Validate the component index parameter
  auto component_it = std::next(folder_node_data.component_list.begin(),
                                static_cast<int>(component_index));

  if (component_it >= folder_node_data.component_list.end()) {
    return false;
  }

  // Create the new folder node that will contains the components
  // at the right of the given index
  FileTreeModel::Node::FolderData new_folder_node_data;

  new_folder_node_data.component_list.insert(
      new_folder_node_data.component_list.end(),
      std::make_move_iterator(component_it),
      std::make_move_iterator(folder_node_data.component_list.end()));

  folder_node_data.component_list.erase(component_it,
                                        folder_node_data.component_list.end());

  // Move the child nodes
  new_folder_node_data.child_set = std::move(folder_node_data.child_set);
  folder_node_data.child_set.clear();

  auto new_folder_node_id = static_cast<std::uint64_t>(node_map.size());
  folder_node_data.child_set.insert(new_folder_node_id);

  // Insert the new folder node
  FileTreeModel::Node new_child_node;
  new_child_node.data = std::move(new_folder_node_data);

  node_map.insert({new_folder_node_id, std::move(new_child_node)});

  // Insert the new folder node
  folder_node_data.child_set.insert(new_folder_node_id);

  return true;
}

bool ImportPathHelper(FileTreeModel::NodeMap &node_map,
                      std::uint64_t parent_node_id,
                      std::vector<std::string> path,
                      const std::optional<mx::PackedFileId> &opt_file_id) {
  // Get the folder data structure of the parent node
  auto &parent_node = node_map.at(parent_node_id);
  if (!std::holds_alternative<FileTreeModel::Node::FolderData>(
          parent_node.data)) {
    return false;
  }

  auto &parent_node_data =
      std::get<FileTreeModel::Node::FolderData>(parent_node.data);

  // Iterate through the components of the current parent
  // node
  auto parent_path_component_it = parent_node_data.component_list.begin();
  auto incoming_path_component_it = path.begin();

  for (;;) {
    if (parent_path_component_it == parent_node_data.component_list.end() ||
        incoming_path_component_it == path.end()) {

      break;
    }

    const auto &parent_path_component = *parent_path_component_it;
    const auto &incoming_path_component = *incoming_path_component_it;

    if (parent_path_component != incoming_path_component) {
      break;
    }

    ++parent_path_component_it;
    ++incoming_path_component_it;
  }

  // If we consumed the last component in the incoming path, then it means
  // that there is a folder that has the same name as the file we want
  // to create
  if (incoming_path_component_it == path.end()) {
    return false;
  }

  // If we are not at the end of the parent component list, then it means
  // that we have to split this parent node. The worst case is splitting
  // right after the `/` component, since all paths are required to be
  // absolute
  if (parent_path_component_it != parent_node_data.component_list.end()) {
    auto component_index = static_cast<std::size_t>(std::distance(
        parent_node_data.component_list.begin(), parent_path_component_it));

    if (!SplitComponentAt(node_map, parent_node_id, component_index)) {
      return false;
    }

    return ImportPathHelper(node_map, parent_node_id, path, opt_file_id);
  }

  // Four cases to cover:
  // 1. inserting a file
  // 2. inserting a folder (node)
  // 3. inserting a folder (additional component)
  // 4. traversing a folder
  const auto &path_component = *incoming_path_component_it;
  std::optional<std::uint64_t> opt_next_child_node_id;

  for (const auto &child : parent_node_data.child_set) {
    const auto &child_node = node_map[child];

    auto contains_component{false};
    if (std::holds_alternative<FileTreeModel::Node::FileData>(
            child_node.data)) {
      const auto &child_node_data =
          std::get<FileTreeModel::Node::FileData>(child_node.data);
      contains_component = (child_node_data.file_name == path_component);

    } else {
      const auto &child_node_data =
          std::get<FileTreeModel::Node::FolderData>(child_node.data);
      contains_component =
          (child_node_data.component_list.front() == path_component);
    }

    if (contains_component) {
      opt_next_child_node_id = child;
      break;
    }
  }

  auto inserting_file = std::next(incoming_path_component_it, 1) == path.end();
  if (inserting_file) {
    if (opt_next_child_node_id.has_value()) {
      return false;
    }

    FileTreeModel::Node::FileData child_node_data;
    child_node_data.file_name = path_component;
    child_node_data.opt_file_id = opt_file_id;

    FileTreeModel::Node child_node;
    child_node.data = std::move(child_node_data);

    auto child_node_id = node_map.size();
    node_map.insert({child_node_id, std::move(child_node)});

    parent_node_data.child_set.insert(child_node_id);
    return true;
  }

  if (!opt_next_child_node_id.has_value()) {
    if (parent_node_data.child_set.empty()) {
      parent_node_data.component_list.push_back(path_component);
      return ImportPathHelper(node_map, parent_node_id, path, opt_file_id);
    }

    FileTreeModel::Node::FolderData child_node_data;
    child_node_data.component_list.push_back(path_component);

    FileTreeModel::Node child_node;
    child_node.data = std::move(child_node_data);

    auto child_node_id = node_map.size();
    node_map.insert({child_node_id, std::move(child_node)});

    parent_node_data.child_set.insert(child_node_id);
    opt_next_child_node_id = child_node_id;
  }

  const auto &next_child_node_id = opt_next_child_node_id.value();

  std::vector<std::string> next_path;
  while (incoming_path_component_it != path.end()) {
    const auto &incoming_path_component = *incoming_path_component_it;
    next_path.push_back(incoming_path_component);

    ++incoming_path_component_it;
  }

  return ImportPathHelper(node_map, next_child_node_id, next_path, opt_file_id);
}

}  // namespace

struct FileTreeModel::PrivateData final {
  mx::Index index;
  NodeMap node_map;
};

FileTreeModel::~FileTreeModel() {}

void FileTreeModel::Update() {
  emit beginResetModel();

  auto succeeded = ImportPathList(d->node_map, d->index.file_paths());
  static_cast<void>(succeeded);

  emit endResetModel();
}

std::optional<PackedFileId>
FileTreeModel::GetFileIdentifier(const QModelIndex &index) const {
  if (!index.isValid()) {
    return std::nullopt;
  }

  auto node_id = static_cast<std::uint64_t>(index.internalId());

  auto node_it = d->node_map.find(node_id);
  if (node_it == d->node_map.end()) {
    return std::nullopt;
  }

  const auto &node = node_it->second;
  if (!std::holds_alternative<Node::FileData>(node.data)) {
    return std::nullopt;
  }

  const auto &file_data = std::get<Node::FileData>(node.data);
  return file_data.opt_file_id;
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

  if (!std::holds_alternative<Node::FolderData>(parent_node.data)) {
    return QModelIndex();
  }

  const auto &folder_data = std::get<Node::FolderData>(parent_node.data);

  if (row >= static_cast<int>(folder_data.child_set.size())) {
    return QModelIndex();
  }

  auto child_set_it = std::next(folder_data.child_set.begin(), row);
  const auto &child_node_id = *child_set_it;

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
  // the grandparent's child_set. If we can't find it, then it is safe to return
  // a blank QModelIndex
  auto grandparent_node_id = parent_node.parent;

  auto grandparent_node_it = d->node_map.find(grandparent_node_id);
  if (grandparent_node_it == d->node_map.end()) {
    return QModelIndex();
  }

  const auto &grandparent_node = grandparent_node_it->second;

  if (!std::holds_alternative<Node::FolderData>(grandparent_node.data)) {
    return QModelIndex();
  }

  const auto &grandparent_node_folder_data =
      std::get<Node::FolderData>(grandparent_node.data);

  auto child_set_it =
      std::find(grandparent_node_folder_data.child_set.begin(),
                grandparent_node_folder_data.child_set.end(), parent_node_id);

  if (child_set_it == grandparent_node_folder_data.child_set.end()) {
    return QModelIndex();
  }

  auto parent_row = static_cast<int>(std::distance(
      grandparent_node_folder_data.child_set.begin(), child_set_it));

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

  if (!std::holds_alternative<Node::FolderData>(parent_node.data)) {
    return 0;
  }

  const auto &folder_data = std::get<Node::FolderData>(parent_node.data);
  return static_cast<int>(folder_data.child_set.size());
}

int FileTreeModel::columnCount(const QModelIndex &) const {
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

  if (std::holds_alternative<Node::FolderData>(node.data)) {
    const auto &folder_data = std::get<Node::FolderData>(node.data);

    std::string folder_name;
    for (auto component_it = folder_data.component_list.begin();
         component_it != folder_data.component_list.end(); ++component_it) {

      const auto &component = *component_it;

      folder_name += component;
      if (component != "/" &&
          std::next(component_it, 1) != folder_data.component_list.end()) {
        folder_name += "/";
      }
    }

    return QString::fromStdString(folder_name);

  } else {
    const auto &file_data = std::get<Node::FileData>(node.data);
    return QString::fromStdString(file_data.file_name);
  }
}

FileTreeModel::FileTreeModel(mx::Index index, QObject *parent)
    : IFileTreeModel(parent),
      d(new PrivateData()) {
  d->index = index;
  Update();
}

bool FileTreeModel::ImportPathList(
    FileTreeModel::NodeMap &node_map,
    const std::map<std::filesystem::path, mx::PackedFileId> &path_list) {

  // clang-format off
  node_map = {
    {
      // Key (node id)
      0,

      // Value (Node)
      {
        // Node::data
        Node::FolderData {
            // Node::FolderData::component_list
            { "ROOT" },

            // Node::FolderData::child_set
            { 1 },
        },

        // parent
        0,
      }
    },

    {
      // Key (node id)
      1,

      // Value (Node)
      {
        // Node::data
        Node::FolderData {
            // Node::FolderData::component_list
            { "/" },

            // Node::FolderData::child_set
            { },
        },

        // parent
        0,
      }
    },
  };
  // clang-format on

  auto succeeded{true};
  for (const auto &file_path_p : path_list) {
    const auto &path = file_path_p.first;
    const auto &file_id = file_path_p.second;

    if (!ImportPath(node_map, path, file_id)) {
      succeeded = false;
    }
  }

  PopulateParents(node_map);

  return succeeded;
}

bool FileTreeModel::ImportPath(
    FileTreeModel::NodeMap &node_map, const std::filesystem::path &path,
    const std::optional<mx::PackedFileId> &opt_file_id) {

  std::vector<std::string> component_list;
  for (const auto &comp : path) {
    component_list.push_back(comp.string());
  }

  return ImportPathHelper(node_map, 1, std::move(component_list), opt_file_id);
}

void PopulateParentsHelper(FileTreeModel::NodeMap &node_map,
                           const std::uint64_t &current_node_id,
                           const std::uint64_t &parent_node_id) {

  auto &current_node = node_map[current_node_id];
  current_node.parent = parent_node_id;

  if (!std::holds_alternative<FileTreeModel::Node::FolderData>(
          current_node.data)) {
    return;
  }

  const auto &current_node_folder_data =
      std::get<FileTreeModel::Node::FolderData>(current_node.data);

  for (const auto &child_node_id : current_node_folder_data.child_set) {
    PopulateParentsHelper(node_map, child_node_id, current_node_id);
  }
}

void FileTreeModel::PopulateParents(NodeMap &node_map) {
  PopulateParentsHelper(node_map, 1, 0);
}

}  // namespace mx::gui
