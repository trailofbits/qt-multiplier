/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "FileTreeModel.h"

#include <doctest/doctest.h>

namespace mx::gui {

namespace {

bool IsFolderNode(const FileTreeModel::Node &node) {
  return std::holds_alternative<FileTreeModel::Node::FolderData>(node.data);
}

const FileTreeModel::Node::FolderData &
GetFolderNodeData(const FileTreeModel::Node &node) {
  return std::get<FileTreeModel::Node::FolderData>(node.data);
}

bool IsFileNode(const FileTreeModel::Node &node) {
  return std::holds_alternative<FileTreeModel::Node::FileData>(node.data);
}

const FileTreeModel::Node::FileData &
GetFileNodeData(const FileTreeModel::Node &node) {
  return std::get<FileTreeModel::Node::FileData>(node.data);
}

bool FolderNodeEquals(const FileTreeModel::Node &node,
                      const std::vector<std::string> &component_list) {
  if (!IsFolderNode(node)) {
    return false;
  }

  const auto &folder_data = GetFolderNodeData(node);
  if (folder_data.component_list.size() != component_list.size()) {
    return false;
  }

  for (std::size_t i = 0; i < component_list.size(); ++i) {
    if (folder_data.component_list[i] != component_list[i]) {
      return false;
    }
  }

  return true;
}

bool VerifyFolderNode(const FileTreeModel::NodeMap &node_map,
                      const std::uint64_t &node_id,
                      const std::vector<std::string> &component_list,
                      const std::uint64_t &parent_node_id) {

  if (node_map.count(node_id) == 0 || node_map.count(parent_node_id) == 0) {
    return false;
  }

  const auto &node = node_map.at(node_id);
  if (!IsFolderNode(node)) {
    return false;
  }

  const auto &parent_node = node_map.at(parent_node_id);
  if (!IsFolderNode(parent_node)) {
    return false;
  }

  if (node.parent != parent_node_id) {
    return false;
  }

  if (!FolderNodeEquals(node, component_list)) {
    return false;
  }

  return true;
}

bool VerifyFileNode(const FileTreeModel::NodeMap &node_map,
                    const std::uint64_t &node_id, const std::string &file_name,
                    const std::uint64_t &parent_node_id) {

  if (node_map.count(node_id) == 0 || node_map.count(parent_node_id) == 0) {
    return false;
  }

  const auto &node = node_map.at(node_id);
  if (!IsFileNode(node)) {
    return false;
  }

  const auto &parent_node = node_map.at(parent_node_id);
  if (!IsFolderNode(parent_node)) {
    return false;
  }

  if (node.parent != parent_node_id) {
    return false;
  }

  const auto &file_node_data = GetFileNodeData(node);
  return file_node_data.file_name == file_name;
}

bool VerifyRootNodes(const FileTreeModel::NodeMap &node_map) {
  if (!VerifyFolderNode(node_map, 0, {"ROOT"}, 0)) {
    return false;
  }

  if (!VerifyFolderNode(node_map, 1, {"/"}, 0)) {
    return false;
  }

  return true;
}

}  // namespace

TEST_CASE("FileTreeModel::ImportPath") {
  FileTreeModel::NodeMap node_map;
  REQUIRE(FileTreeModel::ImportPathList(node_map, {}));
  REQUIRE(VerifyRootNodes(node_map));
  REQUIRE(node_map.size() == 2);

  auto L_ImportPath = [&node_map](const std::filesystem::path &path) {
    REQUIRE(FileTreeModel::ImportPath(node_map, path));
    FileTreeModel::PopulateParents(node_map);
  };

  L_ImportPath("/folder1/folder2/file1.h");
  REQUIRE(node_map.size() == 3);
  REQUIRE(VerifyFolderNode(node_map, 1, {"/", "folder1", "folder2"}, 0));
  REQUIRE(VerifyFileNode(node_map, 2, "file1.h", 1));

  L_ImportPath("/folder1/file2.h");
  REQUIRE(node_map.size() == 5);
  REQUIRE(VerifyFolderNode(node_map, 1, {"/", "folder1"}, 0));
  REQUIRE(VerifyFileNode(node_map, 2, "file1.h", 3));
  REQUIRE(VerifyFolderNode(node_map, 3, {"folder2"}, 1));
  REQUIRE(VerifyFileNode(node_map, 4, "file2.h", 1));

  L_ImportPath("/file3.h");
  REQUIRE(node_map.size() == 7);
  REQUIRE(VerifyFolderNode(node_map, 1, {"/"}, 0));
  REQUIRE(VerifyFileNode(node_map, 2, "file1.h", 3));
  REQUIRE(VerifyFolderNode(node_map, 3, {"folder2"}, 5));
  REQUIRE(VerifyFileNode(node_map, 4, "file2.h", 5));
  REQUIRE(VerifyFolderNode(node_map, 5, {"folder1"}, 1));
  REQUIRE(VerifyFileNode(node_map, 6, "file3.h", 1));

  L_ImportPath("/folder3/folder4/folder5/folder6/file4.h");
  REQUIRE(node_map.size() == 9);
  REQUIRE(VerifyFolderNode(node_map, 1, {"/"}, 0));
  REQUIRE(VerifyFileNode(node_map, 2, "file1.h", 3));
  REQUIRE(VerifyFolderNode(node_map, 3, {"folder2"}, 5));
  REQUIRE(VerifyFileNode(node_map, 4, "file2.h", 5));
  REQUIRE(VerifyFolderNode(node_map, 5, {"folder1"}, 1));
  REQUIRE(VerifyFileNode(node_map, 6, "file3.h", 1));
  REQUIRE(VerifyFolderNode(node_map, 7,
                           {"folder3", "folder4", "folder5", "folder6"}, 1));
  REQUIRE(VerifyFileNode(node_map, 8, "file4.h", 7));

  L_ImportPath("/folder3/folder4/folder5/folder6/file5.h");
  REQUIRE(node_map.size() == 10);
  REQUIRE(VerifyFolderNode(node_map, 1, {"/"}, 0));
  REQUIRE(VerifyFileNode(node_map, 2, "file1.h", 3));
  REQUIRE(VerifyFolderNode(node_map, 3, {"folder2"}, 5));
  REQUIRE(VerifyFileNode(node_map, 4, "file2.h", 5));
  REQUIRE(VerifyFolderNode(node_map, 5, {"folder1"}, 1));
  REQUIRE(VerifyFileNode(node_map, 6, "file3.h", 1));
  REQUIRE(VerifyFolderNode(node_map, 7,
                           {"folder3", "folder4", "folder5", "folder6"}, 1));
  REQUIRE(VerifyFileNode(node_map, 8, "file4.h", 7));
  REQUIRE(VerifyFileNode(node_map, 9, "file5.h", 7));

  L_ImportPath("/folder3/folder4/folder7/file6.h");
  REQUIRE(node_map.size() == 13);
  REQUIRE(VerifyFolderNode(node_map, 1, {"/"}, 0));
  REQUIRE(VerifyFileNode(node_map, 2, "file1.h", 3));
  REQUIRE(VerifyFolderNode(node_map, 3, {"folder2"}, 5));
  REQUIRE(VerifyFileNode(node_map, 4, "file2.h", 5));
  REQUIRE(VerifyFolderNode(node_map, 5, {"folder1"}, 1));
  REQUIRE(VerifyFileNode(node_map, 6, "file3.h", 1));
  REQUIRE(VerifyFolderNode(node_map, 7, {"folder3", "folder4"}, 1));
  REQUIRE(VerifyFileNode(node_map, 8, "file4.h", 10));
  REQUIRE(VerifyFileNode(node_map, 9, "file5.h", 10));
  REQUIRE(VerifyFolderNode(node_map, 10, {"folder5", "folder6"}, 7));
  REQUIRE(VerifyFolderNode(node_map, 11, {"folder7"}, 7));
  REQUIRE(VerifyFileNode(node_map, 12, "file6.h", 11));

  auto file6_path =
      FileTreeModel::GetNodeAbsolutePath(node_map, 12).toStdString();
  REQUIRE(file6_path == "/folder3/folder4/folder7/file6.h");
}

}  // namespace mx::gui
