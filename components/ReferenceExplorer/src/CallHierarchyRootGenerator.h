// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <memory>
#include <multiplier/Entity.h>
#include <multiplier/ui/INodeGenerator.h>

namespace mx {
class Index;
}  // namespace mx
namespace mx::gui {

//! Implements the INodeGenerator interface
class CallHierarchyRootGenerator final : public INodeGenerator {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:

  //! Constructor
  CallHierarchyRootGenerator(
      const Index &index, const FileLocationCache &file_cache_,
      RawEntityId entity_id, const QModelIndex &location);

  //! Destructor
  virtual ~CallHierarchyRootGenerator() override;

  //! \copybrief QRunnable::run
  virtual void run() override;
};

}  // namespace mx::gui
