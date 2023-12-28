// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/ITreeGenerator.h>

#include <multiplier/Frontend/File.h>
#include <multiplier/Index.h>

namespace mx {

class Index;
class FileLocationCache;

namespace gui {

class CallHierarchyGenerator final : public ITreeGenerator {
  const Index index;
  const FileLocationCache file_location_cache;
  const VariantEntity root_entity;

 public:
  virtual ~CallHierarchyGenerator(void) = default;

  inline CallHierarchyGenerator(Index index_,
                                FileLocationCache file_location_cache_,
                                VariantEntity root_entity_)
      : index(std::move(index_)),
        file_location_cache(std::move(file_location_cache_)),
        root_entity(std::move(root_entity_)) {}

  inline static std::shared_ptr<ITreeGenerator>
  Create(const Index &index, const FileLocationCache &cache,
         VariantEntity entity) {
    return std::make_shared<CallHierarchyGenerator>(
        index, cache, std::move(entity));
  }

  int NumColumns(void) const Q_DECL_FINAL;

  QString ColumnTitle(int) const Q_DECL_FINAL;

  QString
  TreeName(const std::shared_ptr<ITreeGenerator> &self) const Q_DECL_FINAL;

  gap::generator<std::shared_ptr<ITreeItem>>
  Roots(const std::shared_ptr<ITreeGenerator> &self) Q_DECL_FINAL;

  gap::generator<std::shared_ptr<ITreeItem>>
  Children(const std::shared_ptr<ITreeGenerator> &self,
           RawEntityId parent_entity) Q_DECL_FINAL;
};

}  // namespace gui
}  // namespace mx
