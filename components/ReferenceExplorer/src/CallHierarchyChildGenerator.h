/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "INodeGenerator.h"

namespace mx::gui {

class CallHierarchyChildGenerator final : public INodeGenerator {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~CallHierarchyChildGenerator(void) override;

  CallHierarchyChildGenerator(const Index &index_,
                              const FileLocationCache &file_cache_,
                              RawEntityId entity_id_,
                              const QModelIndex &parent_);

  gap::generator<Node> GenerateNodes(void) final;
};

}  // namespace mx::gui
