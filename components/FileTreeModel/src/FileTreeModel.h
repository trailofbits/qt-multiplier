/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <unordered_map>

#include <multiplier/ui/IFileTreeModel.h>

namespace mx::gui {

class FileTreeModel final : public IFileTreeModel {
  Q_OBJECT

 public:
  virtual ~FileTreeModel() override;

  virtual void Update() override;

  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const override;

  virtual QModelIndex parent(const QModelIndex &child) const override;
  virtual int rowCount(const QModelIndex &parent) const override;
  virtual int columnCount(const QModelIndex &parent) const override;
  virtual QVariant data(const QModelIndex &index, int role) const override;

  struct Node final {
    std::string file_name;
    std::optional<mx::PackedFileId> opt_file_id;

    std::uint64_t parent{};
    std::unordered_map<std::string, std::uint64_t> child_map;
  };

  using NodeMap = std::unordered_map<std::uint64_t, Node>;

  static NodeMap ParsePathList(
      const std::map<std::filesystem::path, mx::PackedFileId> &path_list);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  FileTreeModel(mx::Index index, QObject *parent);

  friend class IFileTreeModel;
};

}  // namespace mx::gui
