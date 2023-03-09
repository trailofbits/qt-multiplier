/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>
#include <multiplier/ui/IReferenceExplorerModel.h>

#include <QAtomicInt>
#include <QObject>
#include <QRunnable>
#include <vector>

#include "Types.h"

namespace mx {
class FileLocationCache;
class Index;
namespace gui {

class NodeExpander : public QObject, public QRunnable {
  Q_OBJECT

 private:
  QAtomicInt cancel_requested;

 protected:
  NodeExpander(void) = default;

 public:
  virtual ~NodeExpander(void);

  void RequestCancel(void);
  bool CancelRequested(void);

  static NodeExpander *CreateNodeExpander(
      const Index &index,
      const FileLocationCache &file_cache_,
      RawEntityId entity_id,
      const QModelIndex &parent,
      IReferenceExplorerModel::ExpansionMode expansion_mode);

 signals:
  void NodesAvailable(std::vector<Node> node, const QModelIndex &parent,
                      int num_produced);
  void Finished(const QModelIndex &parent, int num_produced);
};


class CallHierarchyNodeExpander final : public NodeExpander {
  Q_OBJECT

  Index index;
  FileLocationCache file_cache;
  const RawEntityId entity_id;
  const QModelIndex parent;

 public:
  virtual ~CallHierarchyNodeExpander(void) = default;

  inline CallHierarchyNodeExpander(
      const Index &index_,
      const FileLocationCache &file_cache_,
      RawEntityId entity_id_,
      const QModelIndex &parent_)
      : index(index_),
        file_cache(file_cache_),
        entity_id(entity_id_),
        parent(parent_) {}

  virtual void run() final;
};

}  // namespace gui
}  // namespace mx
