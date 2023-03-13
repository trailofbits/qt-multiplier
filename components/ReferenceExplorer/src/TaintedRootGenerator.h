/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "TaintedChildGenerator.h"

namespace mx::gui {

class TaintedRootGenerator final : public TaintedChildGenerator {
  Q_OBJECT

 public:
  virtual ~TaintedRootGenerator(void) override;

  inline TaintedRootGenerator(const ::mx::Index &index_,
                              const FileLocationCache &file_cache_,
                              RawEntityId entity_id_,
                              const QModelIndex &parent_)
      : TaintedChildGenerator(index_, file_cache_, entity_id_, parent_) {}

  virtual gap::generator<Node> GenerateNodes(void) override;
};

}  // namespace mx::gui
