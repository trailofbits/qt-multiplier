// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/ITreeGenerator.h>
#include <multiplier/File.h>
#include <multiplier/Index.h>

namespace mx {

class Index;
class FileLocationCache;

namespace gui {

class CallHierarchyGenerator final : public ITreeGenerator {
  Q_OBJECT

  const Index index;
  const FileLocationCache file_location_cache;
  const RawEntityId root_entity_id;

 public:
  virtual ~CallHierarchyGenerator(void) = default;

  inline CallHierarchyGenerator(Index index_,
                                FileLocationCache file_location_cache_,
                                RawEntityId root_entity_id_)
      : index(std::move(index_)),
        file_location_cache(std::move(file_location_cache_)),
        root_entity_id(root_entity_id_) {}

  inline static std::shared_ptr<ITreeGenerator> Create(
      const Index &index, const FileLocationCache &cache,
      RawEntityId entity_id) {
    return std::make_shared<CallHierarchyGenerator>(index, cache, entity_id);
  }

  int NumColumns(void) const Q_DECL_FINAL;

  QVariant ColumnTitle(int) const Q_DECL_FINAL;

  QString TreeName(
      const std::shared_ptr<ITreeGenerator> &self) const Q_DECL_FINAL;

  gap::generator<std::shared_ptr<ITreeItem>> Roots(
      const std::shared_ptr<ITreeGenerator> &self) Q_DECL_FINAL;

  gap::generator<std::shared_ptr<ITreeItem>> Children(
      const std::shared_ptr<ITreeGenerator> &self,
      RawEntityId parent_entity) Q_DECL_FINAL;
};

}  // namespace gui
}  // namespace mx
