/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <unordered_map>
#include <unordered_set>

#include <multiplier/ui/IFileTreeModel.h>

namespace mx::gui {

//! Implements the IFileTreeModel interface
class FileTreeModel final : public IFileTreeModel {
  Q_OBJECT

 public:
  virtual ~FileTreeModel() override;

  //! \copybrief IFileTreeModel::Update
  virtual void Update() override;

  //! Creates a new Qt model index
  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const override;

  //! Returns the parent of the given model index
  virtual QModelIndex parent(const QModelIndex &child) const override;

  //! Returns the amount or rows in the model
  //! Since this is a tree model, rows are intended as child items
  virtual int rowCount(const QModelIndex &parent) const override;

  //! Returns the amount of columns in the model
  //! \return Zero if the parent index is not valid. One otherwise (file name)
  virtual int columnCount(const QModelIndex &parent) const override;

  //! Returns the index data for the specified role
  virtual QVariant data(const QModelIndex &index, int role) const override;

  //! A single node in the internal tree
  struct Node final {
    //! Data for directory nodes
    struct FolderData final {
      //! Components inside this folder node
      std::vector<std::string> component_list;

      //! Child set
      std::unordered_set<std::uint64_t> child_set;
    };

    //! Data for file nodes
    struct FileData final {
      //! The PackedFileId for this node
      std::optional<mx::PackedFileId> opt_file_id;

      //! The file name for this node
      std::string file_name;
    };

    //! A variant that either holds file data or folder data
    using NodeData = std::variant<FolderData, FileData>;

    //! Node data
    NodeData data{FolderData{}};

    //! Parent node
    std::uint64_t parent{};
  };

  //! A tree-like structure implemented using a map
  using NodeMap = std::unordered_map<std::uint64_t, Node>;

  //! Imports a path list into the given NodeMap object
  static bool ImportPathList(
      NodeMap &node_map,
      const std::map<std::filesystem::path, mx::PackedFileId> &path_list);

  //! Imports the specified path into the NodeMap object
  //! \todo file_id is an optional, since we can't easily fake it for tests
  static bool
  ImportPath(NodeMap &node_map, const std::filesystem::path &path,
             const std::optional<mx::PackedFileId> &opt_file_id = std::nullopt);

  //! Visits the node map populating the parent values (required by Qt)
  static void PopulateParents(NodeMap &node_map);

  //! Returns the absolute path for the given node
  static QString GetNodeAbsolutePath(const NodeMap &node_map,
                                     const std::uint64_t &node_id);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  FileTreeModel(mx::Index index, QObject *parent);

  friend class IFileTreeModel;
};

}  // namespace mx::gui
