// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>
#include <QRunnable>

#include <memory>
#include <multiplier/Entity.h>
#include <multiplier/ui/IReferenceExplorerModel.h>

#include "Types.h"

namespace mx {
class Index;
}  // namespace mx
namespace mx::gui {

//! Implements the IEntityNameResolver interface
class CallHierarchyRootsResolver final : public QObject, public QRunnable {
  Q_OBJECT

 public:

  //! Constructor
  CallHierarchyRootsResolver(
      const Index &index, const FileLocationCache &file_cache_,
      const RawEntityId &entity_id, const QModelIndex &location);

  //! Destructor
  virtual ~CallHierarchyRootsResolver() override;

  //! \copybrief QRunnable::run
  virtual void run() override;

 signals:
  void Finished(std::vector<Node> node, const QModelIndex &parent, int row);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;
};

}  // namespace mx::gui
